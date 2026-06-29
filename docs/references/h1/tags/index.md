# Halo 1 Tag Documentation Index

This is a local QMD index for readable Halo 1 tag notes. The tag pages keep the useful field, runtime, and scripting text locally instead of requiring agents to follow external links.

Last generation attempt: 2026-06-24

The local goal is compact, search-friendly documentation for tag behavior, fields, runtime limits, and HaloScript controls.

## Coverage

- Expected H1 tag pages: 60
- Generated this run: 59
- Preserved existing local docs: 1
- Failed this run: 0

## Tag Pages

| Tag | Local doc | Status |
| --- | --- | --- |
| `actor` | [actor.md](actor.md) | generated |
| `actor_variant` | [actor_variant.md](actor_variant.md) | generated |
| `antenna` | [antenna.md](antenna.md) | generated |
| `bitmap` | [bitmap.md](bitmap.md) | generated |
| `camera_track` | [camera_track.md](camera_track.md) | generated |
| `color_table` | [color_table.md](color_table.md) | generated |
| `continuous_damage_effect` | [continuous_damage_effect.md](continuous_damage_effect.md) | generated |
| `contrail` | [contrail.md](contrail.md) | generated |
| `damage_effect` | [damage_effect.md](damage_effect.md) | generated |
| `decal` | [decal.md](decal.md) | preserved |
| `detail_object_collection` | [detail_object_collection.md](detail_object_collection.md) | generated |
| `dialogue` | [dialogue.md](dialogue.md) | generated |
| `effect` | [effect.md](effect.md) | generated |
| `flag` | [flag.md](flag.md) | generated |
| `fog` | [fog.md](fog.md) | generated |
| `font` | [font.md](font.md) | generated |
| `gbxmodel` | [gbxmodel.md](gbxmodel.md) | generated |
| `globals` | [globals.md](globals.md) | generated |
| `glow` | [glow.md](glow.md) | generated |
| `grenade_hud_interface` | [grenade_hud_interface.md](grenade_hud_interface.md) | generated |
| `hud_globals` | [hud_globals.md](hud_globals.md) | generated |
| `hud_message_text` | [hud_message_text.md](hud_message_text.md) | generated |
| `hud_number` | [hud_number.md](hud_number.md) | generated |
| `input_device_defaults` | [input_device_defaults.md](input_device_defaults.md) | generated |
| `item_collection` | [item_collection.md](item_collection.md) | generated |
| `lens_flare` | [lens_flare.md](lens_flare.md) | generated |
| `light` | [light.md](light.md) | generated |
| `light_volume` | [light_volume.md](light_volume.md) | generated |
| `lightning` | [lightning.md](lightning.md) | generated |
| `material_effects` | [material_effects.md](material_effects.md) | generated |
| `meter` | [meter.md](meter.md) | generated |
| `model` | [model.md](model.md) | generated |
| `model_animations` | [model_animations.md](model_animations.md) | generated |
| `model_collision_geometry` | [model_collision_geometry.md](model_collision_geometry.md) | generated |
| `multiplayer_scenario_description` | [multiplayer_scenario_description.md](multiplayer_scenario_description.md) | generated |
| `object` | [object.md](object.md) | generated |
| `particle` | [particle.md](particle.md) | generated |
| `particle_system` | [particle_system.md](particle_system.md) | generated |
| `physics` | [physics.md](physics.md) | generated |
| `placeholder` | [placeholder.md](placeholder.md) | generated |
| `point_physics` | [point_physics.md](point_physics.md) | generated |
| `preferences_network_game` | [preferences_network_game.md](preferences_network_game.md) | generated |
| `scenario` | [scenario.md](scenario.md) | generated |
| `scenario_structure_bsp` | [scenario_structure_bsp.md](scenario_structure_bsp.md) | generated |
| `shader` | [shader.md](shader.md) | generated |
| `sky` | [sky.md](sky.md) | generated |
| `sound` | [sound.md](sound.md) | generated |
| `sound_environment` | [sound_environment.md](sound_environment.md) | generated |
| `sound_looping` | [sound_looping.md](sound_looping.md) | generated |
| `sound_scenery` | [sound_scenery.md](sound_scenery.md) | generated |
| `string_list` | [string_list.md](string_list.md) | generated |
| `tag_collection` | [tag_collection.md](tag_collection.md) | generated |
| `ui_widget_collection` | [ui_widget_collection.md](ui_widget_collection.md) | generated |
| `ui_widget_definition` | [ui_widget_definition.md](ui_widget_definition.md) | generated |
| `unicode_string_list` | [unicode_string_list.md](unicode_string_list.md) | generated |
| `unit_hud_interface` | [unit_hud_interface.md](unit_hud_interface.md) | generated |
| `virtual_keyboard` | [virtual_keyboard.md](virtual_keyboard.md) | generated |
| `weapon_hud_interface` | [weapon_hud_interface.md](weapon_hud_interface.md) | generated |
| `weather_particle_system` | [weather_particle_system.md](weather_particle_system.md) | generated |
| `wind` | [wind.md](wind.md) | generated |

## Suggested Local Page Shape

Use this shape when hand-refining generated pages:

| Section | Purpose |
| --- | --- |
| Summary | One-paragraph purpose of the tag and tag group code. |
| Runtime behavior | Engine behavior, limits, debug commands, and special cases. |
| Reverse-engineering relevance | Useful globals, systems, source files, assertions, or likely function areas. |
| Fields | Compact field table with values, units, defaults, and side effects. |
| Script controls | Related HaloScript functions/globals and how they affect runtime state. |
| Open questions | Unknowns from the source page or things worth validating in the Xbox binary. |
| Provenance | Retrieval date and source attribution without link-heavy reference clutter. |
