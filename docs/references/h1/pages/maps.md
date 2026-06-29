# Halo 1 Reference: Editing and porting maps

## Source Page

- Source: `https://c20.reclaimers.net/h1/maps/`
- Local path: `maps/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

A **map**, also known as a **cache file**, is a bundle of processed tags which can be loaded and used by Halo. With the exception of _resource maps_, each map represents a playable campaign, multiplayer level, or main menu.

Maps are found in Halo's `maps` directory and have the ".map" extension. Maps in subdirectories are not loaded by the game. H1CE mods like Chimera and HAC2 store downloaded maps in a separate location and allow the game to load them.

Although maps work mainly the same way in each release of H1, there are a number of differences listed on this page which prevent maps from being reused across them as-is. For example, an H1CE map file cannot be used in H1PC Demo without recompiling it from tags.

Tool's build-cache-file verb is the official way to build a map for MCC, or for Custom Edition with the HEK, from a scenario tag. The resulting map contains all tags needed by the scenario. The unofficial invader-build tool can also build maps from tags, including targeting all platforms like Xbox, Retail, and Demo, while also validating your tags for invalid data that could cause crashes or undefined behaviour at runtime.

When tags are built into a map, their data is prepared for how it will be used at runtime. Tag path references are replaced with pre-calculated indices or pointers, child scenarios are merged, extra fields are calculated, and the metadata for bitmaps and sounds is separated from their raw data. Tool even makes some hard-coded tag edits.

Tag extraction is the process of reconstructing source tag files (also called _loose tags_) from a built map so that they can be easily modified and rebuilt into new maps. H1 is currently the only game where tag extraction is possible due to its well understood tag stuctures and processing, and mature community tools. Source tags can be mostly reconstructed, but some information like child scenarios and bitmap color plate data is lost during map building.

Tag extractors must take care to carefully reverse all tag processing performed when the map was built, and invader-extract is the best tool for this. Do not use outdated tools like HEK+.

Tag extraction can be hindered if the map has been protected.

### Editing and porting maps

The recommended approach to porting or modifying maps is to obtain their source tags and recompile the map using Tool or invader-build. This ensures the greatest flexibility and tags will be processed correctly when the new map is built. It is also possible to directly edit ("poke") the tags within a map using tools like Assembly, but this can be error prone or more limiting than working with source tags. Adding new assets ("injecting") is harder than using the intended asset pipeline.

### Map types

The type of a map is determined by the scenario type field when the scenario is compiled:

*   **Multiplayer**: In H1CE, multiplayer maps can be loaded through the in-game menu or with the command`sv_map <map> <gametype>`. Loading a multiplayer map using `map_name <map>` will trap the player in the level without spawning unless there is a singleplayer spawn present in the scenario (a spawn with no game modes set). In Standalone you need to use the combination of `game_variant <gametype>` followed by `map_name <scenario tag path>`.
*   **Singleplayer**: To load a singleplayer map in H1CE, you can either use a modded `ui.map` which includes menu options to launch it, or load it directly using the `map_name <map>` console command. In Standalone you just need to use `map_name <scenario tag path>`. When Tool compiles this map type, it strips multiplayer information from globals and applies some balancing tag patches. These patches are applied at runtime in H1X.
*   **UI**: The special `ui.map` contains resources for the game's main menu, including bitmaps for its UI elements like the server browser and the Halo ring background. When Tool compiles a UI map, it strips multiplayer info and fall damage blocks from globals. Custom UI maps for Custom Edition which intend to add a campaign menu must include a dummy first menu item since the game is hardcoded to remove it.

### Resource maps

Resource maps provide a way for certain tags to be stored _external_ to a playable map rather than its tags being totally self-contained. These maps themselves are not playable and have a different header structure, but instead contain shared tags referenced by normal map files. This feature was introduced with H1PC with `bitmaps.map` and `sounds.map` to store bitmap and sound tags respectively, and `loc.map` was added in H1CE to store font and unicode_ string_ list. MCC H1A no longer uses `loc.map` except for backwards compatibility with maps compiled for Custom Edition. H1X does not use resource maps.

Tool's build-cache-file will check resource maps for matching tag paths, and the behaviour depends on the version and arguments. HEK tool will exclude any tag data from your map that it finds in a resource map, instead referencing it as external data. You can opt out of this behaviour by temporarily moving the resource maps away. H1A Tool includes a new _resource map usage_ argument which lets you either ignore (default), reference from, or add to resource maps. HEK users can instead use invader-resource and OpenSauce to create custom resource maps.

Using incompatible resource maps will result in glitched textures, sounds, and text.

Storing bitmaps and sounds in common files had the benefit of reducing the disk space needed for H1PC's maps because multiple maps are able to reference the same data rather than duplicating it. The other benefit is that the resource map can be swapped out with another to alter tag content without affecting the dependent maps. In the case of `loc.map`, the file itself contains common UI messages and prompts but varies by language of the H1CE installation. This means a custom map can be compiled once but still have localized messages when used in another language of the game, as opposed to compiling a version of the map for each language.

### Compressed maps

H1X maps use zlib compression for all data following their header. Maps are decompressed into one of multiple disk caches depending on the header's scenario type.

This compression scheme is not supported natively in other releases of the game, but it is supported by the H1CE mod Chimera. Maps downloaded from HaloNet by this mod may be compressed this way. Although Chimera does not download maps to Halo's main `maps` directory, take care not to mix these maps with stock ones since they are not compatible with the base game and are unsupported by other mods at this time. Compressed maps can be identified using invader-info.

### OpenSauce .yelo maps

Maps with the extension `.yelo` can only be played using the OpenSauce mod for Custom Edition, since they can contain non-standard tag groups and rely on extended game features offered by OS. These maps are created using OS_Tool. Refinery supports extracting OpenSauce tags from these maps.

### Protected maps

A _protected map_ is a map which has been intentionally corrupted by legacy tools in a way which still allows it to be loaded and played in-game, but hinders attempts to extract tags or edit it directly by removing or scrambling data like tag paths. Map protection was often used during the 2000s era of Custom Edition modding by some modders and groups to prevent others from using their content, with approximately 1/3 of legacy maps being protected in some way. This practice has in the long run negatively impacted the community because the resulting maps are crash-prone, cannot be easily be ported to newer engines like H1A, and beginners cannot extract their tags cleanly for study. H1A even explicitly checks for and refuses to load protected maps.

Both the MEK's Refinery and Deathstar can "deprotect" maps in preparation for tag extraction but the results will likely still require cleanup, such as giving tags meaningful names and organization. It's recommended to use the invader toolset to work with tags extracted from protected maps because they may include corrupted data that invader can bludgeon and detect on rebuild into a new map.

### Map file size limit

The maximum allowable file sizes for playable maps varies by version. Halo will reject maps if their header has a file size that exceeds this limit.

*   H1X:
    *   SP: 278 MiB
    *   MP: 47 MiB
    *   UI: 35 MiB

*   H1CE: 384 MiB (Tool enforces 128 MiB for MP maps)
*   H1A: 2 GiB

invader-build can be used to build cache files which exceeds the stock limits, but this may require the user to use a mod to play the map.

### Tag space

The game's buffer for tag data is limited:

*   H1X: 22 MiB
*   H1CE and H1PC: 23 MiB
*   H1A: 64 MiB

Total tag size is comprised of all non-raw tag data (ie. no bitmap or sound raw data) plus the _largest_BSP size, since the BSP is loaded within the tag space and there will only be a single BSP loaded at a time. Additionally, a maximum of 65535 tags can be in a map.

Tool will enforce this limit when compiling a map. Keep an eye on its console output:

`total tag size is 8.43M (14.57M free)`
Care should be taken not to get too close to the tag limit, because even though you may compile a map with a certain set of resource maps (e.g. the English version of the game), Halo players with different languages may actually have _larger_ resource map tag data which now exceeds the limit and prevents your map from loading.

You can toubleshoot which tags are using the most memory by generating the `baggage.txt` report using the Sapien hotkey: Control + Shift + B.

### H1A changes

The H1A engine makes some adjustments to the map format:

*   BSP vertices are stored outside of the BSP tag and BSP data is loaded at address `0x41448000` instead of within the tag data space.
*   The tag data address has been adjusted from `0x40440000` to `0x40448000`.
*   The maps (and other files) are compressed using a variant of zlib compression.
*   Bitmaps, and sounds have been relocated from their respective bitmaps.map/sounds.map locations. The sounds are now in FMOD sound banks, and the bitmaps are stored inside ipaks.

### Map loading

Within a map, _tag definitions_ (sometimes called _metadata_) are stored separately any _raw data_ used by the tag, such as sounds and bitmaps. BSP data for all BSPs is also stored in its own location. The game is able to find these locations using special headers and indexes in the map file.

Each section is loaded in a different way:

*   Tag metadata is copied directly into memory at a fixed address. The game has a limited amount of tag space available for the currently loaded map. The size depends on the edition:

    *   H1X: 22 MiB
    *   H1PC and H1CE: 23 MiB
    *   H1A: 64 MiB

Tag metadata is loaded into the _start_ of this region. Because this data has been preprocessed by Tool, it requires no further processing and thus is very fast to load. The address where tag data is loaded is also dependent on the edition:

    *   H1X: `0x803A6000`
    *   H1PC Demo: `0x4BF10000`
    *   H1PC and H1CE: `0x40440000`
    *   H1A: Dynamic or `0x40448000`?

*   The active BSP is loaded into the _end_ of the tag space. When a BSP switch occurs, the new BSP data is read from the map file using information stored in the scenario tag and replaces the previous data in-memory. In H1A, it is loaded at a separate dedicated location instead.

*   Raw data is streamed from the map file as needed and dynamically allocated in the sound cache and texture cache. The texture cache is cleared during maps loads and BSP switches. Some tags like BSPs and scenarios contain a "predicted resources" block which hints to the game which data should be loaded into these caches.

Tags from resource maps are also loaded into the tag space as needed.

### File structure

Generally map files consist of a map header section followed by BSP, model, raw data, and indexed tag definitions. The header structure and/or values vary by game. Not all maps may look like this exactly, due to map protection or differences in map compiler. All data is little-endian.

This page does not give a full accounting of how BSP and model data are stored and loaded. For further information, see this page's acknowledgments section for source material.

Normal playable (non-resource) cache files begin with a header which is always 2048 bytes long.

| Field | Offset (relative) | Type | Comments |
| --- | --- | --- | --- |
| head magic | `0x0` | `char[4]` | Always takes the value `head`, or `1751474532` as a uint32. This identifies the start of the header. |
| cache version | `0x4` | `CacheVersion: enum32` | Identifies the cache file version, which differs by release of the game. This must match the game's cache file version for the map be loadable. Later games use additional versions, such as H2V's version `8`, which are omitted here. |
| | Option | Value | Comments | | --- | --- | --- | | xbox | `0x5` | The original classic Xbox version of Halo 1. This version also means the data _after_ the cache header is zlib compressed. _Stubbs_ maps also use this version. | | demo | `0x6` | The PC demo version. This special version is one of the reasons why PC retail maps will not work in the demo. | | pc | `0x7` | The PC retail version of the game, ported by Gearbox, as well as its earlier derivatives like H1A for Xbox 360. | | h1a mcc | `0xD` | The version for stock and custom maps for H1A in MCC. | | custom edition | `0x261` | For Halo Custom Edition maps. | |
| file size | `0x8` | `uint32` | Length of the map in bytes when uncompressed. Halo checks this to ensure the map is within the size limit. |
| padding length | `0xC` | `uint32` | * H1X only Length of the padding after the map in bytes. Only used on Xbox. |
| tag data offset | `0x10` | `uint32` | Offset into the file to the tag data header. |
| tag data size | `0x14` | `uint32` | Length of the tag data in bytes. |
|  | `0x18` | `pad(8)` |  |
| scenario name | `0x20` | `char[32]` | The name of the scenario tag which this map was compiled from, e.g. `bloodgulch`. Must match the filename, except in H1A. Null-terminated. |
| build version | `0x40` | `char[32]` | Must match the engine version on Xbox, `01.10.12.2276`. Otherwise this version can represent the version of the game build the cache file is for or the version of the tool which compiled it (as is the case with `invader-build`). |
| scenario type | `0x60` | `ScenarioType: enum16` | On Xbox, this tells the game which disk cache to decompress the map into. The singleplayer is 278 MiB, multiplayer is 47 MiB, and UI is 35 MiB. For example, setting this to `0` would always load the map to the singleplayer cache even if it's a multiplayer map by scenario type. |
| | Option | Value | Comments | | --- | --- | --- | | singleplayer | `0x0` | The map is meant to be used for singleplayer and loaded with the `map_name` command. Cannot be played in multiplayer. | | multiplayer | `0x1` | The map is meant for multiplayer and can be loaded with `sv_map`. | | user interface | `0x2` | The map is meant to serve as ui.map for the game's main menu. | |
|  | `0x62` | `pad(2)` |  |
| checksum | `0x64` | `uint32` | CRC32 checksum which verifies the integrity of BSP, model, and tag data. |
| h1a flags | `0x68` | `H1AFlags: bitfield32` | * H1A only A bitfield which customizes how the cache file is treated by H1A in MCC. These are set by set by the `classic/remastered` option of H1A Tool build-cache-file. |
| | Flag | Mask | Comments | | --- | --- | --- | | use sounds map | `0x1` | Use the sounds.map resource map rather than Saber's resource system. | | use bitmaps map | `0x2` | * Unused Has no effect at this time. | | no remastered sync | `0x4` | Has the effect of disabling the remastered graphics mode. | |
|  | `0x6C` | `pad(1936)` |  |
| foot magic | `0x7FC` | `char[4]` | Always takes the value `foot`, or `1718579060` as a uint32. This identifies the end of the header. |

Demo versions of H1PC use a different cache file header structure with reordered fields and extra padding between them as a means to make it harder to port retail cache files to the demo. The header is still 2048 bytes long.

| Field | Offset (relative) | Type | Comments |
| --- | --- | --- | --- |
|  | `0x0` | `pad(2)` | Filled with garbage values |
| scenario type | `0x2` | `ScenarioType: enum16` | On Xbox, this tells the game which disk cache to decompress the map into. The singleplayer is 278 MiB, multiplayer is 47 MiB, and UI is 35 MiB. For example, setting this to `0` would always load the map to the singleplayer cache even if it's a multiplayer map by scenario type. |
| | Option | Value | Comments | | --- | --- | --- | | singleplayer | `0x0` | The map is meant to be used for singleplayer and loaded with the `map_name` command. Cannot be played in multiplayer. | | multiplayer | `0x1` | The map is meant for multiplayer and can be loaded with `sv_map`. | | user interface | `0x2` | The map is meant to serve as ui.map for the game's main menu. | |
|  | `0x4` | `pad(700)` | Filled with garbage values |
| head magic | `0x2C0` | `char[4]` | Always takes the value `Ehad`, or `1164469604` as a uint32. |
| tag data size | `0x2C4` | `uint32` | Length of the tag data in bytes. |
| build version | `0x2C8` | `char[32]` | Must match the engine version on Xbox, `01.10.12.2276`. Otherwise this version can represent the version of the game build the cache file is for or the version of the tool which compiled it (as is the case with `invader-build`). |
|  | `0x2E8` | `pad(672)` | Filled with garbage values |
| cache version | `0x588` | `CacheVersion: enum32` | Identifies the cache file version, which differs by release of the game. This must match the demo cache file version (`6`) for the map be loadable. |
| scenario name | `0x58C` | `char[32]` | The name of the scenario tag which this map was compiled from, e.g. `bloodgulch`. Must match the filename. Null-terminated. |
|  | `0x5AC` | `pad(4)` | Filled with garbage values |
| checksum | `0x5B0` | `uint32` | CRC32 checksum which verifies the integrity of BSP, model, and tag data. |
|  | `0x5B4` | `pad(52)` | Filled with garbage values |
| file size | `0x5E8` | `uint32` | Length of the map in bytes when uncompressed. |
| tag data offset | `0x5EC` | `uint32` | Offset into the file to the tag data. |
| foot magic | `0x5F0` | `char[4]` | Always takes the value `Gfot`, or `1197895540` as a uint32. This identifies the end of the header. |
|  | `0x5F4` | `pad(524)` | Filled with garbage values |

| Field | Offset (relative) | Type | Comments |
| --- | --- | --- | --- |
| type | `0x0` | `ResourceMapType: enum32` | The type of data found in this resource map. |
| | Option | Value | Comments | | --- | --- | --- | | bitmaps | `0x1` | The file contains bitmap tags. | | sounds | `0x2` | The file contains sound tags. | | loc | `0x3` | The file contains font and unicode_ string_ list tags. | |
| paths offset | `0x4` | `uint32` | Offset to tag paths. |
| resources offset | `0x8` | `uint32` | Offset to resource indices. |
| resource count | `0xC` | `uint32` | The number of resources contained in this file. |

The tag index/tag data header is the start of where tag data and definitions are loaded directly into memory at runtime at the game's tag address. For most versions of the game, it looks like this:

| Field | Offset (relative) | Type | Comments |
| --- | --- | --- | --- |
| tag array pointer | `0x0` | `ptr32` | A pointer to the tag array, which lists all tags in the map. |
| scenario tag id | `0x4` | `uint32` | Unique ID of the scenario tag in the tag array. |
| checksum | `0x8` | `uint32` | A CRC32 checksum of the tag files used in the map. |
| tag count | `0xC` | `uint32` | The number of tags in the tag array. |
| model part count | `0x10` | `uint32` | Number of model parts in the map. |
| model data file offset | `0x14` | `uint32` | File offset to the vertex data. |
| model part count pc | `0x18` | `uint32` | This is a repeat of the model part count field. |
| vertex data size | `0x1C` | `uint32` | Size of the vertex data in bytes. |
| model data size | `0x20` | `uint32` | Total size of the model data in bytes. |
| magic | `0x24` | `char[4]` | Typically equal to "tags", or `1952540531` as a `uint32`. |

The H1X tag index header is slightly different since model data is also in the tag data, so it uses pointers instead of file offsets.

| Field | Offset (relative) | Type | Comments |
| --- | --- | --- | --- |
| tag array pointer | `0x0` | `ptr32` | A pointer to the tag array, which lists all tags in the map. |
| scenario tag id | `0x4` | `uint32` | Unique ID of the scenario tag in the tag array. |
| checksum | `0x8` | `uint32` | A CRC32 checksum of the tag files used in the map. |
| tag count | `0xC` | `uint32` | The number of tags in the tag array. |
| model part count | `0x10` | `uint32` | Number of model parts in the map. |
| vertex data pointer | `0x14` | `ptr32` | This is a pointer to the vertex data. |
| model part count xbox | `0x18` | `uint32` | This is a repeat of the model part count field. |
| triangle data pointer | `0x1C` | `ptr32` | This is a pointer to the triangle data. |
| magic | `0x20` | `char[4]` | Typically equal to "tags", or `1952540531` as a `uint32`. |

### Tag array entry

Each 32-byte element in the tag array contains information about a tag in the map, including a pointer or resource index to the actual tag definition itself.

| Field | Offset (relative) | Type | Comments |
| --- | --- | --- | --- |
| primary group id | `0x0` | `char[4]` | The group ID of the tag's primary tag class. |
| secondary group id | `0x4` | `char[4]` | The group ID of the tag's secondary (parent) tag class. For example, this would be object") for device_ machine tags. |
| tertiary group id | `0x8` | `char[4]` | The group ID of the tag's tertiary (grandparent) tag class. For example, this would be object") for device_ machine tags. |
| tag id | `0xC` | `uint32` | Unique ID of the tag. |
| tag path pointer | `0x10` | `ptr32<char>` | Pointer to the tag path string. This data may unusable in protected maps and is technically not needed at runtime except for debugging purposes. |
| tag data ref | `0x14` | `ptr32` | This field can be interpreted as either a `ptr32` to the tag's data within this cache file when loaded by the game, or as a `uint32`_resource index_ if the tag data is external (found in a resource map). H1X maps will never use a resource index since that feature was not implemented yet. Resource indices are replaced by a pointer to a loaded tag at runtime except in the case of sound tags. |
| is external | `0x18` | `uint32` | If the value is `1`, the tag data is external. This field is padding in H1X. |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   gbMichelle _(How maps are loaded into memory)_
*   hellux _(Testing and documenting tag space on Xbox)_
*   Jakey _(Tag data limit info)_
*   Masterz1337 _(Context on OpenSauce capabilities)_
*   SnowyMouse _(Documenting the map file structure, base addresses, and Invader code for reference)_
*   WaeV _(Diagrams and overview of the map file structure.)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/maps/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
