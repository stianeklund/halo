# Halo 1 Tag: detail_object_collection

## Summary

You can paint detail objects into a BSP using Sapien. Some tips to keep in mind are:

## Tag Information

Tag group: `detail_object_collection`.

HaloScript entries:

- `debug_detail_objects [boolean]`

Defined fields and enum values mentioned:

- base map
- collection type
- collection type screen facing
- collection type viewer facing
- detail objects
- detail objects instances color
- global z offset
- sprite plate
- types
- types ambient color
- types ambient color alpha
- types ambient color blue
- types ambient color green
- types ambient color red
- types color override factor
- types far fade distance
- types first sprite index
- types flags
- types flags interpolate color in hsv
- types flags more colors
- types flags unused a
- types flags unused b
- types maximum color
- types minimum color
- types name
- types near fade distance
- types sequence index
- types size
- types sprite count

## Details

**Detail objects** are 2D sprites which can be painted onto the BSP using Sapien. They are used to add grass and other small details which fade in/out by distance. They can also take on the colour tint of the shader_ environment base map where painted.

### Placement

You can paint detail objects into a BSP using Sapien. Some tips to keep in mind are:

* Avoid painting detail objects in BSP overlap areas since they may noticeably change during BSP transitions.
* Use a low density; detail objects are not depth-sorted for rendering so dense clusters of them will appear to render out of order from certain viewing angles.
* Changes to the detail object tag require reloading Sapien to take effect. Some changes require repainting.

Detail objects are stored in the BSP tag rather than the scenario, and it's easy to accidentally clear them. Activities which modify the BSP tag, like changing cluster properties (weather, background sounds), **will result in all detail objects being cleared**. You should usually leave painting detail objects until the end of level creation, once the BSP is finalized.

Both CE and H1A Sapien have a bug where loaded detail objects are incorrectly saved back to the BSP tag with their _global z offset_ added. This means opening the scenario and causing the BSP to be resaved (e.g. by painting more detail objects) will cause all previous detail objects to be shifted vertically by their Z offset. Changes to _just_ the scenario or resaving the BSP tag in Guerilla do not cause this. Again, leave painting detail objects to the end of level creation.

### Colour and lighting

Detail objects can pick up colour and shading from the local environment. Their final colour results from a combination of:

* Sampled shader base maps where painted,
* Sampled lightmap,
* The _color override factor_ (factor of base map tint),
* Random variation from _minimum and maximum color_,
* The _ambient color_.

This final colour is "baked" into the BSP tag for each detail object instance. Changing any of the above inputs, including the level's lightmaps, requires relighting detail objects to calculate their new colour. You can do this in Sapien using Shift + Control + L while _Detail objects_ is active in the _Hierarchy window_.

### Cells

Detail objects are grouped into _cells_, which are 8x8 world unit axis-aligned boxes that exist only where detail objects have been painted. Cell boundaries always exist on multiples of 8 world units. The game will render details objects for any cells within 8 world units of the camera, so setting _far fade distance_ higher than 8 will result in the detail objects abruptly disappearing when their cell is out of range.

You can visualize cell activity in Standaone with `debug_detail_objects`. Sapien will also display a blue bounding box around all cells which would be active from the point in the level where the cursor points.

Empty cells are removed when the BSP tag is saved in Sapien.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(debug_detail_objects [boolean])` When enabled, active detail object cells will be outlined in blue and individual detail objects are highlighted with red markers. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| collection type | `enum` | Sets how the detail objects are oriented. |
Option values:
- screen facing (`0x0`): The detail objects always face the viewer directly, even when standing above them. **This type does not render in H1A**.
- viewer facing (`0x1`): The detail objects stand vertically but always spin to face toward the viewer. This type is ideal for grass.
| global z offset | `float` | * Unit: world units Sets a Z offset added to loaded detail objects to either raise them above the ground or lower them into it, e.g. with a small negative value to compensate for an empty border in the texture. This can be changed without having to repaint. You do not need to set a Z offset to raise sprites so their bottom edge touches the ground; the game already does this. The Z offset is just for fine-tuning height. Note that Sapien does not account for this modified Z height when resaving the BSP tag, causing detail objects to be **shifted vertically**. If you wish to avoid this, ensure your sprites are painted to the bottom of the texture and set this value to `0`. Historial tag extractors like the MEK have also not accounted for this offset when extracting the BSP tag, resulting in incorrect detail object locations. Ensure you're using an up-to-date extractor like Invader. |
| sprite plate | `TagDependency`: bitmap | * Non-null A reference to a bitmap which contains sprites for the detail object. This bitmap **must have _sprites_ type** or the game will crash. The bitmap can contain multiple sequences, indexed by each detail object type's sequence index. The alpha channel of the sprite determines its transparency. |
| types | `Block` | * HEK max count: 16 |
Flag values:
- name (`TagString`): A name which makes this type easy to identify when painting in Sapien.
- sequence index (`uint8`): Sets the sequence from the sprite plate used for this type, since bitmaps can contain multiple _sequences_ each with multiple _sprites_. Sprite plates with a single sequence should just use index `0`.
- flags (`bitfield`): Flags altering the appearance of the detail object.
- unused a (`0x1`): * Unused
- unused b (`0x2`): * Unused
- interpolate color in hsv (`0x4`): Determines if random colour variation is interpolated in HSV space instead of RGB. This results in colours transitioning through intermediate hues like a rainbow.
- more colors (`0x8`): If set, HSV interpolation goes the "long way" around.
- first sprite index (`uint8`): * Cache only
- sprite count (`uint8`): * Cache only
- color override factor (`float`): * Min: 0 * Max: 1 Fraction of detail object colour to use instead of the base map colour in the environment. A value of `0` means the detail objects colours are fully multiplied by the sampled colour of the base map they are painted over, while a value of `1` means they will be unaffected by the environment and look like the sprite bitmap. This setting doesn't affect sampling of lightmaps. Changing this setting requires relighting detail objects.
- near fade distance (`float`): * Unit: world units Sets the distance from the camera's view plane where the detail objects start to fade out. Detail objects closer than this will be opaque.
- far fade distance (`float`): * Unit: world units Sets the distance from the camera's view plane where the detail objects fade out completely. This should usually be no higher than 8, since only detail objects belong to cells within 8 units are rendered.
- size (`float`): * Unit: world units per pixel Sets the size of a pixel of the detail object. For example, a sprite that is 16 pixels tall at a scale of `0.004` will be `16 * 0.004 = 0.064` world units tall ingame.
- minimum color (`ColorRGB`): Sets the start of the colour variation range. Set to white for no colour variation. Halo will sample a random value from this range to multiply against the detail object texture. The interpolation can be done in either RGB or HSV space depending on the flags.
- maximum color (`ColorRGB`): Sets the end of the colour variation range. Set to white for no colour variation.
- ambient color (`ColorARGBInt`): Sets the ambient light colour of the detail objects, which is multiplied against the texure when rendering. This takes precedence over the sampled lightmap colour depending on the alpha value, with `0` being lightmap only and `255` being this ambient colour only.
- RGB Color with alpha, with 8-bit color depth per channel (0-255) (Field): Type
- Comments (): ---
- alpha (`uint8`)
- red (`uint8`)
- green (`uint8`)
- blue (`uint8`)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `detail_object_collection`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
