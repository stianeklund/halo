# Halo 1 Reference: shader_ transparent_ meter

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/shader/shader_transparent_meter/`
- Local path: `tags/shader/shader_transparent_meter/readme.md`
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
        37.   particle
        38.   particle_system
        39.   physics
        40.   placeholder
        41.   point_physics
        42.   preferences_network_game
        43.   scenario
        44.   scenario_structure_bsp
        45.   shader (abstract tag)
            1.   shader_environment
            2.   shader_model
            3.   shader_transparent_chicago
            4.   shader_transparent_chicago_extended
            5.   shader_transparent_generic
            6.   shader_transparent_glass
            7.   shader_transparent_meter

            8.   shader_transparent_plasma
            9.   shader_transparent_water

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
4.   shader (abstract tag)

1.   Structure and fields

### shader_ transparent_ meter

**shader_ transparent_ meter (`smet`?)**

The plasma pistol overheat meter uses shader_ transparent_ meter.

Parent tag: shader

Direct references: bitmap

*   H1-Guerilla
*   invader-edit
*   invader-edit-qt
*   OS_ Guerilla
*   Mozzarilla
*   reclaimer-python

This page is incomplete! You can contribute information using GitHub issues or pull requests.

...

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| meter flags | `bitfield` |  |
| | Flag | Mask | Comments | | --- | --- | --- | | decal | `0x1` | | | two sided | `0x2` | | | flash color is negative | `0x4` | | | tint mode 2 | `0x8` | | | unfiltered | `0x10` | | |
| map | `TagDependency`: bitmap |  |
| gradient min color | `ColorRGB` |  |
| gradient max color | `ColorRGB` |  |
| background color | `ColorRGB` |  |
| flash color | `ColorRGB` |  |
| meter tint color | `ColorRGB` |  |
| meter transparency | `float` | * Min: 0 * Max: 1 |
| background transparency | `float` | * Min: 0 * Max: 1 |
| meter brightness source | `enum` |  |
| | Option | Value | Comments | | --- | --- | --- | | none | `0x0` | | | a out | `0x1` | | | b out | `0x2` | | | c out | `0x3` | | | d out | `0x4` | | |
| flash brightness source | `enum`? |  |
| value source | `enum`? |  |
| gradient source | `enum`? |  |
| flash extension source | `enum`? |  |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   MosesOfEgypt _(Tag structure research)_
*   SnowyMouse _(Invader tag definitions)_

This text is available under the CC BY-SA 3.0 license • c20 on GitHub • Go to top

### Page contents

1.   Structure and fields

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/shader/shader_transparent_meter/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
