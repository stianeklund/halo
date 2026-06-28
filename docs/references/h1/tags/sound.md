# Halo 1 Tag: sound

## Summary

1.   Related HaloScript 2.   Structure and fields     1.   pitch ranges

## Tag Information

Tag group: `sound`.

HaloScript entries:

- `debug_looping_sound [boolean]`
- `debug_sound [boolean]`
- `debug_sound_cache [boolean]`
- `debug_sound_cache_graph [boolean]`
- `debug_sound_channels [boolean]`
- `debug_sound_channels_detail [boolean]`
- `debug_sound_environment [boolean]`
- `multiplayer_hit_sound_volume [real]`
- `sound_cache_dump_to_file`
- `sound_gain_under_dialog [real]`
- `sound_obstruction_ratio [real]`

Defined fields and enum values mentioned:

- channel count
- channel count mono
- channel count stereo
- cumulative promotion length
- flags
- flags fit to adpcm blocksize
- flags split long sound into permutations
- format
- format 16 bit pcm
- format flac
- format ima adpcm
- format ogg vorbis
- format xbox adpcm
- inner cone angle
- last promotion time
- longest permutation length
- maximum bend per second
- maximum distance
- minimum distance
- one gain modifier
- one pitch modifier
- one skip fraction modifier
- outer cone angle
- outer cone gain
- permutations
- pitch ranges
- pitch ranges actual permutation count
- pitch ranges bend bounds
- pitch ranges last permutation index
- pitch ranges name
- pitch ranges natural pitch
- pitch ranges next permutation index
- pitch ranges permutations
- pitch ranges permutations buffer size
- pitch ranges permutations format
- pitch ranges permutations gain
- pitch ranges permutations mouth data
- pitch ranges permutations name
- pitch ranges permutations next permutation index
- pitch ranges permutations samples
- pitch ranges permutations samples external
- pitch ranges permutations samples file offset
- pitch ranges permutations samples pointer
- pitch ranges permutations samples size
- pitch ranges permutations skip fraction
- pitch ranges permutations subtitle data
- pitch ranges permutations tag id 0
- pitch ranges permutations tag id 1
- pitch ranges playback rate
- pitch ranges used permutations
- promotion count
- promotion sound
- random gain modifier
- random pitch bounds
- random pitch bounds max
- random pitch bounds min
- sample rate
- sample rate 22050 hz
- sample rate 44100 hz
- scripted sound index
- scripted sound remaining time
- skip fraction
- sound class
- sound class ambient computers
- sound class ambient machinery
- sound class ambient nature
- sound class device computers
- sound class device door
- sound class device force field
- sound class device machinery
- sound class device nature
- sound class first person damage
- sound class game event
- sound class music
- sound class object impacts
- sound class particle impacts
- sound class projectile detonation
- sound class projectile impact
- sound class scripted dialog force unspatialized
- sound class scripted dialog other

## Details

1. Related HaloScript
 1. pitch ranges

...

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

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- fit to adpcm blocksize (`0x1`)
- split long sound into permutations (`0x2`)
| sound class | `enum` | Determines the "type" of sound and how it is treated by the engine. For example, weapon sounds can wake sleeping Grunts and music volume can be adjusted independently. The engine is only capable of playing up to 4 sounds of a sound class simultaneously on high sound settings. |
Option values:
- projectile impact (`0x0`)
- projectile detonation (`0x1`)
- unused (`0x2`)
- unused1 (`0x3`)
- weapon fire (`0x4`)
- weapon ready (`0x5`)
- weapon reload (`0x6`)
- weapon empty (`0x7`)
- weapon charge (`0x8`)
- weapon overheat (`0x9`)
- weapon idle (`0xA`)
- unused2 (`0xB`)
- unused3 (`0xC`)
- object impacts (`0xD`)
- particle impacts (`0xE`)
- slow particle impacts (`0xF`)
- unused4 (`0x10`)
- unused5 (`0x11`)
- unit footsteps (`0x12`)
- unit dialog (`0x13`)
- unused6 (`0x14`)
- unused7 (`0x15`)
- vehicle collision (`0x16`)
- vehicle engine (`0x17`)
- unused8 (`0x18`)
- unused9 (`0x19`)
- device door (`0x1A`)
- device force field (`0x1B`)
- device machinery (`0x1C`)
- device nature (`0x1D`)
- device computers (`0x1E`)
- unused10 (`0x1F`)
- music (`0x20`)
- ambient nature (`0x21`)
- ambient machinery (`0x22`)
- ambient computers (`0x23`)
- unused11 (`0x24`)
- unused12 (`0x25`)
- unused13 (`0x26`)
- first person damage (`0x27`)
- unused14 (`0x28`)
- unused15 (`0x29`)
- unused16 (`0x2A`)
- unused17 (`0x2B`)
- scripted dialog player (`0x2C`): Causes non-dialog sounds to temporarily fade out while playing, controlled by `sound_gain_under_dialog`. This sound class will not play when the player is dead, regardless of how the sound was triggered.
- scripted effect (`0x2D`)
- scripted dialog other (`0x2E`): Causes non-dialog sounds to temporarily fade out while playing, controlled by `sound_gain_under_dialog`.
- scripted dialog force unspatialized (`0x2F`): Causes non-dialog sounds to temporarily fade out while playing, controlled by `sound_gain_under_dialog`.
- unused18 (`0x30`)
- unused19 (`0x31`)
- game event (`0x32`)
| sample rate | `enum` | * Read-only |
Option values:
- 22050 Hz (`0x0`)
- 44100 Hz (`0x1`)
| minimum distance | `float` | * Unit: world units |
| maximum distance | `float` | * Unit: world units |
| skip fraction | `float` |  |
| random pitch bounds | `Bounds` | * Default: 1,1 This is the base playback rate of the sound, affecting tempo and pitch. |
Field values:
- min (`float`)
- max (`float`)
| inner cone angle | `float` | * Default: 6.28318548202515 |
| outer cone angle | `float` | * Default: 6.28318548202515 |
| outer cone gain | `float` | * Default: 1 |
| random gain modifier | `float` | * Default: 1 * Max: 1 * Min: 0 Warning: HEK Guerilla allows you to set this value outside its valid range. Avoid crashes by staying in the valid range. H1A Guerilla doesn't have this issue. |
| maximum bend per second | `float` |  |
| zero skip fraction modifier | `float` |  |
| zero gain modifier | `float` | * Max: 1 * Min: 0 Warning: HEK Guerilla allows you to set this value outside its valid range. Avoid crashes by staying in the valid range. H1A Guerilla doesn't have this issue. |
| zero pitch modifier | `float` |  |
| one skip fraction modifier | `float` |  |
| one gain modifier | `float` | * Max: 1 * Min: 0 Warning: HEK Guerilla allows you to set this value outside its valid range. Avoid crashes by staying in the valid range. H1A Guerilla doesn't have this issue. |
| one pitch modifier | `float` |  |
| channel count | `enum` | * Read-only |
Option values:
- mono (`0x0`)
- stereo (`0x1`)
| format | `enum` | * Read-only |
Option values:
- 16-bit PCM (`0x0`)
- Xbox ADPCM (`0x1`)
- IMA ADPCM (`0x2`)
- Ogg Vorbis (`0x3`)
- FLAC (`0x4`)
| promotion sound | `TagDependency`: sound |  |
| promotion count | `uint16` |  |
| longest permutation length | `uint32` | * Cache only * Hidden natural pitch * seconds * 1100; not set if pitch modifier is set to anything besides 1; not accurate since increasing natural pitch decreases the length uses the 'buffer size' value for 16-bit PCM and Ogg Vorbis (divide by 2 * channel count to get sample count); uses entire size of samples for ADPCM (multiply by 130 / 36 * channel count to get sample count) |
| cumulative promotion length | `uint32` | * Cache only |
| last promotion time | `uint32` | * Cache only |
| scripted sound remaining time | `uint32` | * Cache only set to `0xFFFFFFFF` on cache build |
| scripted sound index | `uint32` | * Cache only set to null id on cache build |
| pitch ranges | `Block` | * HEK max count: 8 * Read-only |
Field values:
- name (`TagString`)
- natural pitch (`float`): * Default: 1 This is the base pitch for this pitch range. When the pitch bend is equal to this, then the audio is played at normal pitch and speed. Note that 0 defaults to 1.
- bend bounds (`Bounds`?): This is the minimum and maximum bend in which this pitch range will be used. If the lower bound is higher than natural pitch, then it will be set to natural pitch. Also, if the higher bound is lower than natural pitch, then it will be set to natural pitch.
- actual permutation count (`uint16`): * Read-only This is the number of actual permutations in the pitch range, not including chunks due to splitting.
- playback rate (`float`): * Cache only
- used permutations (`uint32`): * Cache only engine internal bitfield, set to `0xFFFFFFFF` on cache build
- last permutation index (`uint16`): * Cache only set to null index on cache build
- next permutation index (`uint16`): * Cache only set to null index on cache build
- permutations (`Block`): * HEK max count: 256 * Read-only
- * Processed during compile (Field): Type
- Comments (): ---
- name (`TagString`): * Read-only
- skip fraction (`float`): * Min: 0 * Max: 1
- gain (`float`): * Min: 0 * Max: 1 * Default: 1
- format (`enum`?): * Read-only
- next permutation index (`uint16`→): * Read-only
- samples pointer (`uint32`): * Cache only
- tag id 0 (`uint32`): * Cache only Set to the sound tag's tag ID
- buffer size (`uint32`): * Read-only this is the buffer size used to hold (and, for Vorbis, decompress) the 16-bit PCM data (unused in Xbox ADPCM)
- tag id 1 (`uint32`): * Cache only Set to the sound tag's tag ID
- samples (`TagDataOffset`): * Read-only
- size (`uint32`)
- external (`uint32`)
- file offset (`uint32`)
- pointer (`ptr64`)
- mouth data (`TagDataOffset`?): * Read-only
- subtitle data (`TagDataOffset`?): * Read-only

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `sound`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
