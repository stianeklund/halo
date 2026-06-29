# Halo 1 Reference: Sound cache

## Source Page

- Source: `https://c20.reclaimers.net/h1/engine/sound-system/`
- Local path: `engine/sound-system/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

The **sound system** is responsible for playing effects and music sounds in-game and in tools like Sapien and Standalone.

### Sound cache

Like the renderer's texture cache, the sound system also holds sound data in an in-memory _sound cache_. When a sound must be played that is not in this cache, it will be loaded from a map cache file (possibly a shared resource map) or the tags directory depending on the build type of the engine. The cache can hold a maximum of 512 entries or 64 MB.

The predicted resources block seen in some tag classes are meant to give the engine a hint about what sounds (and textures) should be cached.

### Environmental audio

MCC and Custom Edition (when using EAX emulation with DSOAL) apply positional and environmental effects to sounds. For this reason, it is important that level artists ensure BSP clusters have an appropriate sound_ environment applied.

### Sound obstruction

Sounds which are playing behind an obstruction are muffled. An obstruction is anything which blocks a collision ray test between the sound source and the camera. This may be the BSP or an object with collision geometry.

### Channels

The engine has the following channel limits:

*   26 mono 3D channels
*   4 mono channels
*   4 stereo channels
*   4 44k stereo channels

The following are related functions that you can use in your scenario scripts and/or debug globals that you can enter into the developer console for troubleshooting.

|  | Function/global | Type |
| --- | --- | --- |
|  | `(debug_looping_sound [boolean])` Displays active sound_ looping with cyan spheres for their maxmimum distance and blue spheres for their minimum. | Global |
|  | `(debug_sound [boolean])` Sound sources will be labeled in 3D with their tag path and a their minimum and maximum distances shown as red and yellow spheres, respectively. | Global |
|  | `(debug_sound_cache [boolean])` If enabled, sound cache statistics will be shown in the top left corner of the screen, including how full it is. | Global |
|  | `(debug_sound_cache_graph [boolean])` Shows a graph of sound cache slots at the top of the screen, similar to `texture_cache_graph`. Can be used in Sapien/debug builds. | Global |
|  | `(debug_sound_channels [boolean])` Displays the utilization of sound channel limits in the top left corner of the screen. | Global |
|  | `(debug_sound_channels_detail [boolean])` Displays two columns of number pairs while sounds play with an unknown meaning. | Global |
|  | `(debug_sound_environment [boolean])` If enabled, shows the tag path of the cluster's current sound_ environment. | Global |
|  | `(multiplayer_hit_sound_volume [real])` Sets the gain of the hit sound heard when damage is dealt in multiplayer. Defaults to `0.2`. | Global |
|  | `(sound_cache_dump_to_file)` Writes the sound cache information from `debug_sound_cache` and a listing of currently cached sounds to `sound_cache_dump.txt`. | Global |
|  | `(sound_gain_under_dialog [real])` Controls how quiet non-dialog sounds are when scripted dialog is playing (sound class must be `scripted_dialog_other`, `scripted_dialog_force_player`, or `scripted_dialog_force_unspatialized`). Does not apply to involuntary AI dialog like death/pain lines. Defaults to `0.7`. The effect takes about half a second to fade in/out. | Global |
|  | `(sound_obstruction_ratio [real])` Controls how "muffled" sounds are when heard behind an obstruction like the BSP or an object with collision geometry. Defaults to `0.1`. A value of `0` means "not muffled", while a value above about `6` effectively mutes them. | Global |

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/engine/sound-system/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
