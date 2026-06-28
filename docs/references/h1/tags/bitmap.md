# Halo 1 Tag: bitmap

## Summary

To store textures and images in maps we use bitmap tags. Bitmap tags on their simplest are compiled from a `.tif` file in your data directory.

## Tag Information

Tag group: `bitmap`.

Defined fields and enum values mentioned:

- alpha bias
- bitmap data
- bitmap data base address
- bitmap data bitmap class
- bitmap data bitmap class actor
- bitmap data bitmap class actor variant
- bitmap data bitmap class antenna
- bitmap data bitmap class biped
- bitmap data bitmap class bitmap
- bitmap data bitmap class camera track
- bitmap data bitmap class color table
- bitmap data bitmap class continuous damage effect
- bitmap data bitmap class contrail
- bitmap data bitmap class damage effect
- bitmap data bitmap class decal
- bitmap data bitmap class detail object collection
- bitmap data bitmap class device
- bitmap data bitmap class device control
- bitmap data bitmap class device light fixture
- bitmap data bitmap class device machine
- bitmap data bitmap class dialogue
- bitmap data bitmap class effect
- bitmap data bitmap class equipment
- bitmap data bitmap class flag
- bitmap data bitmap class fog
- bitmap data bitmap class font
- bitmap data bitmap class garbage
- bitmap data bitmap class gbxmodel
- bitmap data bitmap class globals
- bitmap data bitmap class glow
- bitmap data bitmap class grenade hud interface
- bitmap data bitmap class hud globals
- bitmap data bitmap class hud message text
- bitmap data bitmap class hud number
- bitmap data bitmap class input device defaults
- bitmap data bitmap class invader bitmap
- bitmap data bitmap class invader font
- bitmap data bitmap class invader scenario
- bitmap data bitmap class invader sound
- bitmap data bitmap class invader ui widget definition
- bitmap data bitmap class invader unit hud interface
- bitmap data bitmap class invader weapon hud interface
- bitmap data bitmap class item
- bitmap data bitmap class item collection
- bitmap data bitmap class lens flare
- bitmap data bitmap class light
- bitmap data bitmap class light volume
- bitmap data bitmap class lightning
- bitmap data bitmap class material effects
- bitmap data bitmap class meter
- bitmap data bitmap class model
- bitmap data bitmap class model animations
- bitmap data bitmap class model collision geometry
- bitmap data bitmap class multiplayer scenario description
- bitmap data bitmap class none
- bitmap data bitmap class null
- bitmap data bitmap class object
- bitmap data bitmap class particle
- bitmap data bitmap class particle system
- bitmap data bitmap class physics
- bitmap data bitmap class placeholder
- bitmap data bitmap class point physics
- bitmap data bitmap class preferences network game
- bitmap data bitmap class projectile
- bitmap data bitmap class scenario
- bitmap data bitmap class scenario structure bsp
- bitmap data bitmap class scenery
- bitmap data bitmap class shader
- bitmap data bitmap class shader environment
- bitmap data bitmap class shader model
- bitmap data bitmap class shader transparent chicago
- bitmap data bitmap class shader transparent chicago extended
- bitmap data bitmap class shader transparent generic
- bitmap data bitmap class shader transparent glass
- bitmap data bitmap class shader transparent glsl
- bitmap data bitmap class shader transparent meter
- bitmap data bitmap class shader transparent plasma
- bitmap data bitmap class shader transparent water
- bitmap data bitmap class sky
- bitmap data bitmap class sound

## Details

**Bitmaps** tags store textures used for the environment, objects, cube maps, sprites, effects, menus, and more. Don't be confused by the name; this is not a `.bmp` file and it cannot be directly opened in image editing software. The purpose of the tag is to store metadata about a texture and its image data, possibly compressed in a format like DXT1, ready for the engine and GPU to use. It can also store multiple images in a single tag.

### Basics

To store textures and images in maps we use bitmap tags. Bitmap tags on their simplest are compiled from a `.tif` file in your data directory.

### Tool Commands

`tool bitmap <source-file>`
This command is to import a single `.tif` file from your data folder and convert it into a bitmap the respective tags folder.

Example: You have a file named `bar.tif` in your data folder under `data\foo\bitmaps`

The tool command you would use would be:

`tool bitmap "foo\bar\bitmaps\bar"`
That would output the file `tags\foo\bitmaps\bar.bitmap`

`tool bitmaps <source-folder>`
This command is to import all `.tif` files in a data folder into the respective tags folder.

Example: You have `foo.tif` and `bar.tif` under `data\example\bitmaps`

The tool command you would use would be:

`tool bitmaps "example\bitmaps"`
That would output `.bitmap` versions of these files in the respective tags folder under `example\bitmaps`

### Type

The bitmap type drop down box is used to tell tool how to process the image and what rules to apply.

* **2D Textures**: A flat 2D texture. Mainly used for environments, objects. Also used for huds and effects.
* **3D Textures**: A set of texture slices that when combined forms a texture cube. Used in the shader_ transparent_ plasma tag type.
* **Cube maps**: A set of 6 textures that combines into a cube. Used for simulating reflections in shaders.
* **Sprites**: A set of textures that have multiple sprites fitted on them. Used for effects and huds.
* **Interface bitmaps**: Textures that don't have to follow the rule of needing to be power-of-two. It should only be used for menus.

### Format

The format of the bitmap determines what way it will be stored, how big the bitmap file ends up being in a map file and how it ends up looking.

### Compressed with color-key transparency

The smallest format the game supports, it's about 12.5% of the size in memory as a 32-bit bitmap. It uses DXT1 as an internal compression format. This means that for every 4x4 grid of pixels it picks two colors to store and at runtime these two colors are interpolated with two more colors. This works well for most textures, except normal maps which can end up making shaders look blocky because of the delicacy of the data stored in them.

This format does not support alphas and transparency very well. The alpha for each pixel can only be either 100% (white) or 0% (black). It also makes the colors on the texture black when the alpha is 0%.

### Compressed with explicit alpha

At 25% the size of an equally sized 32-bit bitmap this format is mostly the same as color-key, with the only difference being that the image can now store an alpha. The internal format is DXT3 which means that the alpha uses 4-bit color, allowing for 16 different shades of brightness (Compared to the 256 levels for 32-bit).

The fact that this format stores the alpha in explicit values means it is good for noisy alphas with greatly differing brightness values for each pixel.

### Compressed with interpolated alpha

Also at 25% of the size of a 32-bit bitmap this format provides an alternative way to store the alpha from explicit. The internal format is DXT5, which means the alpha is stored in a similar way to how the colors are stored. When compiling the bitmap tool finds the highest and lowest values in a 4x4 pixel grid, stores those and then at runtime the game interpolates it with two more values.

The alpha can have great color depth in terms of shades, which is really useful for situations where alpha brightness matters, but in situations where there is great varying brightness between pixels block artifacts can show up.

### 16-bit color

This format is 50% the size of a 32-bit bitmap. This format has a few different internal formats which can greatly affect the outcome of how the texture looks. The internal formats are r5g6g5, a1b5g5b5, and a4b4g4b4. Each of these formats is named after how many bits per pixel is allocated for each channel. For the depth of the channel you can do shades=2^bits.

### 32-bit color

As the biggest size for bitmap formats 32-bit (Also referred to as true color) bitmaps store their colors at the same settings as most consumer monitors. The internal formats are a8r8g8b8 and x8r8g8b8, the only difference between these is that in x8r8g8b8 the 8 bits that would store the alpha in a8r8g8b8 are ignored.

### Monochrome

Using 25% of the size of a 32-bit bitmap this format was mainly used for huds in the xbox version of the game. This format does not function properly anymore on the PC version and Custom Edition of the game, but does work again in MCC.

### High-quality compression

This is a new format (BC7) added to H1A MCC in 2023 which provides both high quality and good compression. You can read more about BC7 here.

### Notes

When importing TIF file using tool with the format set to a compressed one tool might make the bitmap it outputs more noisy than it needs to be. It is speculated that this is because of a broken DXT toolkit being used in tool. Using other programs like Mozzarilla you can import DXT1,2,3 at higher qualities without increasing memory footprint.

### Usage

The usage fields are used to enable special processing onto the bitmap.

### Default

A default bitmap with normal mipmaps

### Alpha Blend

This makes pixels that have 0 alpha turn black in the smaller (mipmap) versions of the texture. This is to prevent color bleeding on transparent images.

### Height map

This will try to convert your source image into a normal map based on the brightness of the pixels. For this setting you need to set a height to get a proper output. This setting also converts the bitmap to a special 8-bit palletized format specifically designed for normal maps. Sadly this format does not render properly in the PC and Custom Edition versions of the game, only on Xbox. To be able to use this format you need to enable the flag "disable height map compression", which turns it into a normal 32-bit bitmap, but still with the lower color detail.

Using this is not recommended as generating your own normal map from a height map in a program like Photoshop will get you much better results.

### Detail map

When setting the usage to detail map, tool will fade the bitmap to grey in every mipmap, this is so that when you are further away things that use the detail map won't look as noisy. The alpha fades to white.

You can modify how quickly the mipmaps fade to grey by editing "detail fade factor" under "post-processing". 0 means that it will slowly fade to grey until the last mipmap, and 1 means that the first and every subsequent mipmap is grey.

### Light map

This is the setting used when tool or sapien generates a lightmap, you should not use this when importing normal bitmaps.

### Vector map

Used mostly for special effects this setting stores the rgb channels as directional XYZ vectors, the alpha is left unmodified

### 2D Textures

2D textures are flat textures used in 3D environments, on objects, huds and effects. Bitmaps can hold one or more 2D textures either as permutations or animated textures.

### Importing

2D textures can be imported from source images without extra borders around them, but when compiling multiple into one bitmap you will need to add them following the same rules as for sprites. 2D textures are required to be power-of-two.

### 3D Textures

3D textures are sets of textures (slices) that make up a solid 3D cube. They are only used in shader_ transparent_ plasma tags, which are used for the energy shield effect for instance.

### Importing

3D textures need to be imported the same way as you import a sprite sheet, all slices should be put next to each other horizontally. Preferably you want to have the same amount of slices as the height and width of each slice. So if each slice is 64x64, you want 64 slices to make it into a 64x64x64 cube.

### Cube maps

Cube maps are a combination of 6 textures that form one cube. It is used for reflections in shaders.

### Importing

Cube maps are imported either as folded out cubes or sprite sheets. All faces should have the same resolution, power-of-two and square. Just like sprites, cube map bitmaps can contain multiple sequences/cube maps for use with forced permutations.

_NOTE: Having multiple cube maps in one bitmap does not randomize them._

### Examples of acceptable source images for cube map import:

| Unfolded | Sprite Sequence | Multiple in one tag |
| --- | --- | --- |
|  |  |  |

### Sprites

The sprite type allows a bitmap to contain a non-power-of-two texture, with support for animations with multiple permutations (sequences). Sprites are typically used for particles.

### Color Plate

| Scaled 10x | Original Size |
| --- | --- |
|  |  |

The color plate is the first 3 top-left pixels of the source file. It tells tool which colors are being used for the sprite borders (background), sequence dividers, and dummy space, respectively. Any colors may be used, as long as they aren't used in the sprites themselves.

If no color plate is defined, AKA: there is nothing in the left top. You have to use the default color for sprite borders (pure blue #0000FF), sequence dividers cannot be clearly defined, and you cannot use dummy space.

_NOTE: The plate is not allowed to touch the sprites, but is allowed to touch sequence dividers._

### Sprite borders

Sprites must be surrounded by a rectangular border of solid color. Any color may be used for the border as long as it isn't used in any of the sprites. This color should be the exact same as the first pixel of the color plate, or pure blue (#0000FF) if there is no color plate.

Any amount of padding may be used as long as the sprite is isolated by at least a one pixel border. Individual sprites also don't need to be perfectly lined up.

### Budget

Budget size determines how big each texture page is (and thus how many sprites will appear on each page). Budget count sets how many texture pages there will be. Both of these values should be set for sprites.

When compiling a bitmap, tool will output how many pages were generated and the percentage of space filled by the sprites. Budget size and count should be tweaked to get this percentage as high as possible, as unused space is wasted memory.

Budget sizes are limited to "square" dimensions like 256x256 and 512x512. Because of this, it is sometimes more memory efficient to split the sprites up across several pages. For example, putting a sprite sheet with seven 56x56 sprites on a single page would require a 512x512 budget size, because 512x512 is the smallest size that can fit all of the sprites. However, If that same sprite sheet was split up across two pages, each page would only need to be 256x256, cutting memory usage in half.

_NOTE: Tool automatically uses at least 4 pixels of padding between each sprite. (This is so that there will always be at least one pixel of space between different sprites in all mipmaps, as tool also uses a default mipmap count of 2 for these. 4 -> 2 -> 1.) This means four 32x32 sprites will for instance not fit on a 64x64 page. Make sure to take this into account when choosing a budget size!_

### Sequences

A sprite sheet with multiple sequences. Frames are lined up horizontally, and permutations are stacked vertically. Note the magenta line separating each sequence. A sequence is an animated sprite. Each sprite in a sequence represents a "frame" of the sprite's animation. A bitmap may contain multiple sequences, which allows for random and forced permutations. Each frame of a sequence is lined up left-to-right. Each sequence permutation is stacked vertically, top-to-bottom.

If the source file has more than one sequence, each sequence should also have a sequence divider above it. A sequence divider is a straight line of solid color (using the color plate's second pixel's color) at least one pixel wide that spans the entire width of the image. The divider must be a different color than the sprite border. The sequence divider may be left out if the file only has one sequence or if it has no color plate.

_TIP: Mozzarilla is a good tool for viewing sequences._

_NOTE: All sprites in a sequence must be the same size._

_NOTE: Tool will sometimes split a sequence across several pages. This does not affect functionality._

_QUIRK: Only the start of a sequence divider needs to be the color defined on the color plate. After that it can be any color, or could even just end prematurely._

### Dummy Space

Space usage on a sprite with dummy space vs without.

Dummy space is space that is counted toward the size and position of your sprite, but not included in the data of the sprite in the bitmap. This is useful for two reasons: It allows a sprite that's smaller than other sprites in a sequence, and it tells tool how to center the smaller sprite without bulking up the file size with padding. Dummy space must be a solid color that is different from the sprite border and sequence separator colors.

### Sprite Usage

Under "... more sprite processing" there is a drop down box that controls the background color of the texture page. This is used to avoid color bleeding in from outside the sprite boundaries at runtime and creating outline-like artifacts.

* **blend/add/subtract/max**: Makes the background black.
* **multiply/min**: Makes the background white.
* **double multiply**: Makes the background grey (50% grey).

### Interface bitmaps

Interface bitmaps are textures that do not need to follow the power-of-two rule. They are used for menus (Not huds) and should never be used for anything else. They also generate without mipmaps and need to be 32-bit.

### Mipmap count

Under "miscellaneous" there is a setting for how many mipmaps you want in your bitmap. This is useful if you wish to limit the mipmap levels, or remove them entirely.

0 defaults to all mipmaps, 1 is only the biggest mipmap, etc.

### Tool Errors and Warnings

Errors that are known and suggestions on how to fix them.

### --> !!WARNING!! failed to open TIFF: file does not exist <--

| Cause | Fix |
| --- | --- |
| The file is not where you told tool it is | Make sure the file is somewhere in data or a subfolder and check your spelling of the path. |
| The file is a TIFF file and not a TIF file | save as a .TIF file; legacy Tool doesn't understand .TIFF for some reason, but it does understand TIF. No longer true in MCC Tool. |

### Unknown data compression algoritm # (0x#). --> !!WARNING!! failed to open TIFF: not a TIFF file <--

* Cause: Your TIF file might be using a new algoritm that is not supported by tool
* Fix: try saving it with a different program or with different settings.
* Info: (Tool uses a version of libtiff from 2002/07/30, so it will not understand a lot of the new tiff specifications.

### skipping bitmap with non-power-of-two dimensions

* Cause: Your source tif does not follow the power of two rule.
* Fix: Make sure your source image has a resolution that can be divided by 2 without ending in a decimal. 2x2, 4x4, 8x8, 16x16, 32x32, 64x64, 128x128, 256x256, 512x512, 1024x1024, 2048x2048.
* Alternative fix: If you are intending to make a sprite sheet or an interface bitmap, ignore this and set the type to the correct value in the .bitmap file, save, and put in the tool command again.

| Cause | Fix |
| --- | --- |
| You didn't add the three color plate pixels in the top-left corner of the source file. | Add the color plate |
| You have bitmaps of different size in a sequence. | Use dummy space to pad the smaller sprites to match the bigger ones. |
| One of your sprites is touching the plate. | Move the sprite away from the plate by at least one pixel. |

### ### ERROR one or more sprites do not fit in the requested page size

| Cause | Fix |
| --- | --- |
| The sprite page is too small to contain the sprite sheet; not all bitmaps are processed if any. | Set a bigger page size in the bitmap tag. |
| A sequence divider has been interpreted as a sprite. | Make sure your plate pixel that indicates the color of the dividers has the exact same color as the dividers. |

_Reminder: Every sprite is padded with 4 pixels on each border, so while your sprites may fit in theory, they may not in practice._

### sprite budget met ( 0%)

* Cause: Tool is reading your source file, but not actually finding any textures.
* Fix: Verify your color plate is set up correctly, and each sprite is properly isolated with border color.

### ==> !!WARNING!! bitmap with greater than 1-bit alpha being compressed as DXT1 <==

* Cause: You're compiling a source file that has a detailed alpha map into a bitmap with color key transparency.
* Fix: You could ignore this, but chances are the alpha was important. Use interpolated or explicit alpha instead.

### ### WARNING no sprite budget set

* Cause: "sprite budget count" is not set in the bitmap tag
* Effect: Tool picks a size for you, and you're probably not going to like it.
* Fix: set the sprite budget count

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| type | `enum` |  |
Option values:
- 2d textures (`0x0`)
- 3d textures (`0x1`): Used for shader_ transparent_ plasma noise maps.
- cube maps (`0x2`)
- sprites (`0x3`)
- interface bitmaps (`0x4`)
| encoding format | `enum` | Format to use when generating the tag |
Option values:
- dxt1 (`0x0`)
- dxt3 (`0x1`)
- dxt5 (`0x2`)
- 16-bit (`0x3`)
- 32-bit (`0x4`)
- monochrome (`0x5`): * H1X only * H1A only This format is only supported on Xbox and MCC. Using this format can significantly reduce tag size for monochromatic bitmaps.
- high-quality compression (`0x6`): * H1A only BC7 is a new compression format supported in MCC. It allows each compressed block of pixels to be encoded with a different allocation of bits per channel, providing good compression and maintaining a high quality.
| usage | `enum` |  |
Option values:
- alpha blend (`0x0`)
- default (`0x1`)
- height map (`0x2`)
- detail map (`0x3`)
- light map (`0x4`)
- vector map (`0x5`)
| flags | `bitfield` |  |
Flag values:
- enable diffusion dithering (`0x1`)
- disable height map compression (`0x2`)
- uniform sprite sequences (`0x4`)
- filthy sprite bug fix (`0x8`)
- hud scale 0.5 (`0x10`)
- invert detail fade (`0x20`)
- use average color for detail fade (`0x40`): When bitmap usage is _detail map_, mipmaps will fade to the average colour of the bitmap when importing. Otherwise it will fade to gray. Prior to this flag being added in December 2022, the H1 MCC mod tools would fade to average colour by default like Halo 2.
| detail fade factor | `float` | * Min: 0 * Max: 1 |
| sharpen amount | `float` | * Min: 0 * Max: 1 |
| bump height | `float` | * Unit: repeats |
| sprite budget size | `enum` |  |
Option values:
- 32x32 (`0x0`)
- 64x64 (`0x1`)
- 128x128 (`0x2`)
- 256x256 (`0x3`)
- 512x512 (`0x4`)
- 1024x1024 (`0x5`)
- 2048x2048 (`0x6`)
| sprite budget count | `uint16` |  |
| color plate width | `uint16` | * Non-cached * Unit: pixels * Volatile * Read-only |
| color plate height | `uint16` | * Non-cached * Unit: pixels * Volatile * Read-only |
| compressed color plate data | `TagDataOffset` | * Non-cached * Volatile * Read-only Contains the original raw source data used to compile this bitmap tag, if available. Bitmap tags which have been extracted from cache files will not have this data. |
Field values:
- size (`uint32`)
- external (`uint32`)
- file offset (`uint32`)
- pointer (`ptr64`)
| processed pixel data | `TagDataOffset`? | * Non-cached * Read-only |
| blur filter size | `float` | * Unit: pixels * Min: 0 * Max: 10 |
| alpha bias | `float` | * Min: -1 * Max: 1 |
| mipmap count | `uint16` |  |
| sprite usage | `enum` |  |
Option values:
- blend add subtract max (`0x0`)
- multiply min (`0x1`)
- double multiply (`0x2`)
| sprite spacing | `uint16` | * Read-only |
| bitmap group sequence | `Block` | * HEK max count: 256 * Max: 65534 * Read-only |
Field values:
- name (`TagString`)
- first bitmap index (`uint16`→)
- bitmap count (`uint16`)
- sprites (`Block`): * HEK max count: 64 * Max: 65534
- * Read-only (Field): Type
- Comments (): ---
- bitmap index (`uint16`→)
- left (`float`)
- right (`float`)
- top (`float`)
- bottom (`float`)
- registration point (`Point2D`)
- x (`float`)
- y (`float`)
| bitmap data | `Block` | * HEK max count: 2048 * Max: 65534 * Read-only |
Field values:
- bitmap class (`enum`): * Hidden
- none (`0xFFFFFFFF`)
- null (`0x0`)
- actor (`0x61637472`)
- actor variant (`0x61637476`)
- antenna (`0x616E7421`)
- model animations (`0x616E7472`)
- biped (`0x62697064`)
- bitmap (`0x6269746D`)
- spheroid (`0x626F6F6D`)
- continuous damage effect (`0x63646D67`)
- model collision geometry (`0x636F6C6C`)
- color table (`0x636F6C6F`)
- contrail (`0x636F6E74`)
- device control (`0x6374726C`)
- decal (`0x64656361`)
- ui widget definition (`0x44654C61`)
- input device defaults (`0x64657663`)
- device (`0x64657669`)
- detail object collection (`0x646F6263`)
- effect (`0x65666665`)
- equipment (`0x65716970`)
- flag (`0x666C6167`)
- fog (`0x666F6720`)
- font (`0x666F6E74`)
- material effects (`0x666F6F74`)
- garbage (`0x67617262`)
- glow (`0x676C7721`)
- grenade hud interface (`0x67726869`)
- hud message text (`0x686D7420`)
- hud number (`0x68756423`)
- hud globals (`0x68756467`)
- item (`0x6974656D`)
- item collection (`0x69746D63`)
- damage effect (`0x6A707421`)
- lens flare (`0x6C656E73`)
- lightning (`0x656C6563`)
- device light fixture (`0x6C696669`)
- light (`0x6C696768`)
- sound looping (`0x6C736E64`)
- device machine (`0x6D616368`)
- globals (`0x6D617467`)
- meter (`0x6D657472`)
- light volume (`0x6D677332`)
- gbxmodel (`0x6D6F6432`)
- model (`0x6D6F6465`)
- multiplayer scenario description (`0x6D706C79`)
- preferences network game (`0x6E677072`)
- object (`0x6F626A65`)
- particle (`0x70617274`)
- particle system (`0x7063746C`)
- physics (`0x70687973`)
- placeholder (`0x706C6163`)
- point physics (`0x70706879`)
- projectile (`0x70726F6A`)
- weather particle system (`0x7261696E`)
- scenario structure bsp (`0x73627370`)
- scenery (`0x7363656E`)
- shader transparent chicago extended (`0x73636578`)
- shader transparent chicago (`0x73636869`)
- scenario (`0x73636E72`)
- shader environment (`0x73656E76`)
- shader transparent glass (`0x73676C61`)
- shader (`0x73686472`)
- sky (`0x736B7920`)
- shader transparent meter (`0x736D6574`)
- sound (`0x736E6421`)
- sound environment (`0x736E6465`)
- shader model (`0x736F736F`)
- shader transparent generic (`0x736F7472`)
- ui widget collection (`0x536F756C`)
- shader transparent plasma (`0x73706C61`)
- sound scenery (`0x73736365`)
- string list (`0x73747223`)
- shader transparent water (`0x73776174`)
- tag collection (`0x74616763`)
- camera track (`0x7472616B`)
- dialogue (`0x75646C67`)
- unit hud interface (`0x756E6869`)
- unit (`0x756E6974`)
- unicode string list (`0x75737472`)
- virtual keyboard (`0x76636B79`)
- vehicle (`0x76656869`)
- weapon (`0x77656170`)
- wind (`0x77696E64`)
- weapon hud interface (`0x77706869`)
- invader bitmap (`0x65626974`): * Non-standard
- invader scenario (`0x53636E72`): * Non-standard
- invader sound (`0x65736E64`): * Non-standard
- invader font (`0x6E666E74`): * Non-standard
- invader ui widget definition (`0x6E757764`): * Non-standard
- invader unit hud interface (`0x6E756869`): * Non-standard
- invader weapon hud interface (`0x6E776869`): * Non-standard
- shader transparent glsl (`0x7374676C`): * Non-standard
- width (`uint16`): * Unit: pixels
- height (`uint16`): * Unit: pixels
- depth (`uint16`): * Unit: pixels
- type (`enum`)
- 2d texture (`0x0`)
- 3d texture (`0x1`)
- cube map (`0x2`)
- white (`0x3`)
- format (`enum`)
- a8 (`0x0`)
- y8 (`0x1`)
- ay8 (`0x2`)
- a8y8 (`0x3`)
- unused1 (`0x4`)
- unused2 (`0x5`)
- r5g6b5 (`0x6`)
- unused3 (`0x7`)
- a1r5g5b5 (`0x8`)
- a4r4g4b4 (`0x9`)
- x8r8g8b8 (`0xA`)
- a8r8g8b8 (`0xB`)
- unused4 (`0xC`)
- unused5 (`0xD`)
- dxt1 (`0xE`)
- dxt3 (`0xF`)
- dxt5 (`0x10`)
- p8 bump (`0x11`): This format is only supported on Xbox and MCC.
- bc7 (`0x12`): This format is only supported on MCC.
- flags (`bitfield`)
- power of two dimensions (`0x1`)
- compressed (`0x2`)
- palettized (`0x4`)
- swizzled (`0x8`)
- linear (`0x10`)
- v16u16 (`0x20`)
- unused (`0x40`): * Cache only
- make it actually work (`0x80`): * Cache only
- external (`0x100`): * Cache only
- stubbed (`0x200`): * Cache only
- zstandard compressed (extended) (`0x400`)
- registration point (`Point2DInt`)
- x (`int16`)
- y (`int16`)
- mipmap count (`uint16`)
- pixel data offset (`uint32`)
- pixel data size (`uint32`): * Cache only
- bitmap tag id (`uint32`): * Cache only
- pointer (`ptr32`): * Cache only
- hardware format (`ptr32`): * Cache only Direct3D resource
- base address (`ptr32`): * Cache only this appears to be a pointer specific to the tool editing it; it gets changed whenever the tag is opened in any of the HEK tools (guerilla.exe, tool.exe, etc.)

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `bitmap`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
