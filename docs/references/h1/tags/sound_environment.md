# Halo 1 Tag: sound_environment

## Summary

49.   sound_looping          50.   sound_scenery          51.   string_list          52.   tag_collection          53.   ui_widget_collection          54.   ui_widget_definition          55.   unicode_string_list          56.   unit_hud_interface          57.   virtual_keyboard          58.   weapon_hud_interface          59.   weather_particle_system          60.   wind

## Tag Information

Tag group: `sound_environment`.

Defined fields and enum values mentioned:

- decay hf ratio
- decay time
- density
- diffusion
- hf reference
- priority
- reflections delay
- reflections intensity
- reverb delay
- reverb intensity
- room intensity
- room intensity hf
- room rolloff
- unknown

## Details

1. The Reclaimers Library

## sound_ environment

**sound_ environment (`snde`?)**

Referenced by (2)
* fog
* scenario_ structure_ bsp

* H1-Guerilla
* invader-edit
* invader-edit-qt
* OS_ Guerilla
* Mozzarilla
* reclaimer-python

A sound environment determines the accoustic qualities of a cluster like reverberation. This is what makes spaces sound open, confined, or echoey.

## Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| unknown | `int32` | * Cache only |
| priority | `int16` |  |
| room intensity | `float` |  |
| room intensity hf | `float` |  |
| room rolloff | `float` |  |
| decay time | `float` | * Unit: seconds |
| decay hf ratio | `float` |  |
| reflections intensity | `float` |  |
| reflections delay | `float` | * Unit: seconds |
| reverb intensity | `float` |  |
| reverb delay | `float` | * Unit: seconds |
| diffusion | `float` |  |
| density | `float` |  |
| hf reference | `float` | * Unit: Hz |

## Acknowledgements

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `sound_environment`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
