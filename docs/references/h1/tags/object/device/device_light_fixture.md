# Halo 1 Reference: device_ light_ fixture

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/object/device/device_light_fixture/`
- Local path: `tags/object/device/device_light_fixture/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

1.   H1 Editing Kit
    2.   Community tools
    3.   Halo Custom Edition
    4.   Guides
    5.   Source data
    6.   Tags
        1.   actor
        2.   actor_variant
        3.   antenna
        4.   bitmap
        5.   camera_track
        6.   color_table
        7.   continuous_damage_effect
        8.   contrail
        9.   damage_effect
        10.   decal
        11.   detail_object_collection
        12.   dialogue
        13.   effect
        14.   flag
        15.   fog
        16.   font
        17.   gbxmodel
        18.   globals
        19.   glow
        20.   grenade_hud_interface
        21.   hud_globals
        22.   hud_message_text
        23.   hud_number
        24.   input_device_defaults
        25.   item_collection
        26.   lens_flare
        27.   light
        28.   light_volume
        29.   lightning
        30.   material_effects
        31.   meter
        32.   model
        33.   model_animations
        34.   model_collision_geometry
        35.   multiplayer_scenario_description
        36.   object (abstract tag)
            1.   device (abstract tag)
                1.   device_control
                2.   device_light_fixture

                3.   device_machine

            2.   item (abstract tag)
            3.   projectile
            4.   scenery
            5.   unit (abstract tag)

        37.   particle
        38.   particle_system
        39.   physics
        40.   placeholder
        41.   point_physics
        42.   preferences_network_game
        43.   scenario
        44.   scenario_structure_bsp
        45.   shader (abstract tag)
        46.   sky
        47.   sound
        48.   sound_environment
        49.   sound_looping
        50.   sound_scenery
        51.   string_list
        52.   tag_collection
        53.   ui_widget_collection
        54.   ui_widget_definition
        55.   unicode_string_list
        56.   unit_hud_interface
        57.   virtual_keyboard
        58.   weapon_hud_interface
        59.   weather_particle_system
        60.   wind

    7.   Scripting
    8.   Maps
    9.   Engine systems

* * *

1.   The Reclaimers Library
3.   Tags
4.   object (abstract tag)
5.   device (abstract tag)

1.   Collisions
2.   Structure and fields

### device_ light_ fixture

**device_ light_ fixture (`lifi`?)**

Light fixtures used decoratively in _Assault on the Control Room_

Parent tag: device

device references (2)
*   sound
*   effect

object references (16)
*   gbxmodel
*   model
*   model_ animations
*   model_ collision_ geometry
*   physics
*   shader
*   effect
*   light
*   light_ volume
*   contrail
*   particle_ system
*   sound_ looping
*   antenna
*   glow
*   lightning
*   flag

Referenced by: scenario

*   H1-Guerilla
*   invader-edit
*   invader-edit-qt
*   OS_ Guerilla
*   Mozzarilla
*   reclaimer-python

This page is incomplete! You can contribute information using GitHub issues or pull requests.

**Light fixtures** are a type of static object whose dynamic light can be enabled or disabled dynamically (e.g. by script or by device_ control). They can also just be used to decorate and illuminate lightmaps, although their effect on baked lighting cannot be toggled dynamically. Their intensity and falloff/cutoff angles can be set per-object when placed in Sapien.

### Collisions

A feature of light fixtures is that projectiles like bullets will collide with them, but units") like the player will not. This makes them ideal for small decorative lighting objects which might obstruct player movement otherwise. The sound_ scenery object has the same collision rules.

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_
*   zatarita _(Per-object settings tip)_

This text is available under the CC BY-SA 3.0 license • c20 on GitHub • Go to top

### Page contents

1.   Collisions
2.   Structure and fields

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/object/device/device_light_fixture/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
