# Halo 1 Tag: wind

## Summary

You can think of wind as being built up with layers:

## Tag Information

Tag group: `wind`.

Defined fields and enum values mentioned:

- damping
- flags uses damped wind
- flags uses simple wind
- local variation rate
- local variation weight
- variation area
- variation area pitch
- variation area yaw
- velocity
- velocity max
- velocity min
- weather palette wind direction
- weather palette wind magnitude

## Details

**Wind** tags describe the base speed and directional variability of wind in a BSP cluster as a part of its assigned weather palette. Wind affects the movement of point_ physics such as flags, weather_ particle_ systems, contrails, and more. The base wind direction in your level is not specified by this tag, but rather in your weather palette entries in Sapien which you must assign to clusters.

You can think of wind as being built up with layers:

* The general wind direction and strength is defined in your palette entry.
* Variability in the speed and angle are defined in this tag.
* Local variation (wiggle) may be ratio-blended _or_ added after.
* For weather particle systems, there is added motion unrelated to wind.

All elements of variability and random weather motion are framerate-dependent so your custom wind and weather should be tested at typical playing framerates to ensure they look realistic. It can be easier to see the effects of wind settings by setting `framerate_throttle 1` in the console.

### Variation area

You can vary the overall wind direction over time within an angular range defined by the _variation area_, relative to the weather palette entry's direction. This is a global variation that applies to all particles.

Unfortunately, the rate of change is hard-coded and _framerate-dependent_ rather than tied to the 30 Hz simulation rate. This means the wind direction will change faster the higher your framerate is, affecting Gearbox and later editions of the game. Regardless, using this variation feature allows your wind to look more natural even at high FPS. Values around 30-60 and 10-20 degrees are recommended for yaw and pitch respectively.

Variation area only, at uncapped FPS and yaw range 180 degrees.

### Local variation

Local variation applies smaller-scale changes to particle directions variation. This can be imagined like a vibrating wiggly field that all the particles are embedded in, stretching and squeezing locally to give the impression of turbulence. Like variation area, this is framerate-dependent so higher FPS means faster local variation.

Local variation also has a roughly +x -y +z directionality bias which is hard-coded. The bias is similar to the _weather\_ particle\_ system_added motion but does not depend on particle count. A higher local variation weight causes particles to blow in this upward angled direction, especially at high FPS, which may be undesirable and needs to be counteracted by adjusting the weather palette wind direction. Test your weather at expected framerates.

Local variation only, at 30 FPS with rate 1 and velocity 2.

Weakly weighted local variation is generally not very noticeable on small groups of particles or short lived particles. If you want to have a strong local variation weight while also maintaining overall directionality, increase your wind palette entry's wind magnitude. However, this also results in scaling local variation magnitude for particles using _simple wind_!

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| velocity | `Bounds` | * Unit: world units Upper and lower bounds for base wind speed and local variation speed. Technically, this is not a true "velocity" because it is not a vector; wind direction is set in the BSP's weather palettes using Sapien. This wind speed may also be scaled by the palette entry's wind magnitude and slowed by the _damping_ factor below for particles using _damped wind_, however this affects just the base wind speed and not how this range scales local variation. |
Field values:
- min (`float`)
- max (`float`)
| variation area | `Euler2D` | * Unit: degrees Sets the horizontal (y) and vertical (p) angle ranges that wind will vary within. For example, a yaw of `90` means the wind direction can vary up to 45 degrees to the left or right of its base direction. Set these to `0` if you don't want any variation. Note that _weather\_ particle\_ system_ particles always have a small amount of built-in random velocity which is not caused by wind variation. These can be 360 degrees to allow the wind to come from any direction. |
Field values:
- yaw (`float`): Rotation to the left or right around the Z (vertical) axis.
- pitch (`float`): Rotation up or down.
| local variation weight | `float` | Sets the strength of local variation. The behaviour of this field depends on whether particles use _simple wind_. By default, this acts as a blending ratio between full base wind (`0`) and full local variation (`1`). However, particles using simple wind will have a scaled local variation added atop unmodified base wind. |
| local variation rate | `float` | Determines the frequency of local variation changes. Lower values in the range of `0` to `1` tend to look most realistic, with higher values causing too much vibration. This rate is **framerate-dependent** so local variation will appear to vibrate more on higher FPS machines. |
| damping | `float` | Scale wind influence on _point\_ physics_ using the _damped wind_ flag. A value of `0` means don't slow damped particles at all, while `1` means completely stop them. This allows certain types of _point\_ physics_ entities to react more weakly to the wind. Keep this in the 0-1 range; beyond that damped weather particles start to separate into bands or travel backwards. |

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `wind`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
