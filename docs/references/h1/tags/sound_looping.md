# Halo 1 Tag: sound_looping

## Summary

1.   Structure and fields     1.   tracks     2.   detail sounds

## Tag Information

Tag group: `sound_looping`.

Defined fields and enum values mentioned:

- background sound palette
- continuous damage effect
- detail sounds
- detail sounds distance bounds
- detail sounds flags
- detail sounds flags dont play with alternate
- detail sounds flags dont play without alternate
- detail sounds gain
- detail sounds pitch bounds
- detail sounds random period bounds
- detail sounds random period bounds max
- detail sounds random period bounds min
- detail sounds sound
- detail sounds yaw bounds
- detail sounds yaw bounds max
- detail sounds yaw bounds min
- flags
- flags deafening to ais
- flags not a loop
- flags stops music
- maximum distance
- one detail sound period
- one detail unknown floats
- runtime scripting sound
- sound class
- tracks
- tracks alternate end
- tracks alternate loop
- tracks end
- tracks fade in duration
- tracks fade out duration
- tracks flags
- tracks flags fade in alternate
- tracks flags fade in at start
- tracks flags fade out at stop
- tracks gain
- tracks loop
- tracks start
- zero detail sound period
- zero detail unknown floats

## Details

2. detail sounds

...

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| flags | `bitfield` |  |
Flag values:
- deafening to ais (`0x1`)
- not a loop (`0x2`): Prevents the sound from looping. When played with `sound_looping_start`, it will play only once.
- stops music (`0x4`)
| zero detail sound period | `float` |  |
| zero detail unknown floats | `float[2]` |  |
| one detail sound period | `float` |  |
| one detail unknown floats | `float[2]` |  |
| runtime scripting sound | `uint32` | * Cache only |
| maximum distance | `float` | * Cache only |
| continuous damage effect | `TagDependency`: continuous_damage_effect |  |
| tracks | `Block` | * HEK max count: 4 Tracks can be considered the "layers" of the _sound\_ looping_. Although many tracks can be added, the engine is not be capable of playing more than 4 of each sound class simultaneously when on high sound quality setting (2 on low or in Sapien). Extra tracks past these limits will not be played. |
Flag values:
- flags (`bitfield`)
- fade in at start (`0x1`)
- fade out at stop (`0x2`)
- fade in alternate (`0x4`)
- gain (`float`): * Min: 0 * Max: 1 * Default: 1
- fade in duration (`float`): * Unit: seconds How many seconds it takes for this track to fade in, such as when entering a cluster with a background sound.
- fade out duration (`float`): * Unit: seconds How many seconds it takes for this track to fade out.
- start (`TagDependency`: sound): The sound that will play for this track when this _sound\_ looping_ begins playing. This field is optional.
- loop (`TagDependency`: sound): A sound to loop indefinitely while the _sound\_ looping_ is playing.
- end (`TagDependency`: sound): The sound that will play for this track when fading out.
- alternate loop (`TagDependency`: sound)
- alternate end (`TagDependency`: sound)
| detail sounds | `Block` | * HEK max count: 32 |
Field values:
- sound (`TagDependency`: sound)
- random period bounds (`Bounds`): * Unit: seconds
- min (`float`)
- max (`float`)
- gain (`float`): * Min: 0 * Max: 1 * Default: 1
- flags (`bitfield`)
- don't play with alternate (`0x1`)
- don't play without alternate (`0x2`)
- yaw bounds (`Bounds`): * Default: -3.14159265359,3.14159265359
- min (`float`)
- max (`float`)
- pitch bounds (`Bounds`?): * Default: -1.57079632679,1.57079632679
- distance bounds (`Bounds`?): * Unit: world units

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `sound_looping`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
