# Halo 1 Engine Systems

## Summary

Halo 1 uses Bungie's Blam engine, written in C and C++, with portal-based visibility, baked lightmaps, dynamic objects, custom physics, AI and pathfinding, a scripting VM, sound and texture caches, map-cache loading, and multiplayer synchronization. This page keeps only details explicitly relevant to Halo CE for Xbox and to reverse-engineering the Xbox runtime.

## Runtime Behavior

| System        | Xbox-relevant behavior                                                                                                                                                                                                                                                       |
|---------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Map loading   | Xbox maps use cache version `0x5`; data after the 2048-byte header is zlib-compressed. Tag metadata is loaded at Xbox address `0x803A6000`. Xbox has no PC-style resource maps; bitmap/sound raw data is not externalized through `bitmaps.map`, `sounds.map`, or `loc.map`. |
| Tag space     | Xbox tag space is documented as 22 MiB. The active BSP is loaded into the end of the tag-space region and replaced on BSP switch. A map can contain at most 65535 tags.                                                                                                      |
| Map size      | Xbox playable map limits are documented as 278 MiB for singleplayer, 47 MiB for multiplayer, and 35 MiB for UI. The Xbox map header includes `padding length` and `scenario type`; scenario type selects the disk cache used for decompression.                              |
| Renderer      | The original Xbox renderer is the baseline for later PC/MCC regressions. It uses portal-based occlusion, baked lightmaps for BSPs, dynamic lights, and 128x128 dynamic shadow maps for moving objects.                                                                       |
| Texture cache | Textures are loaded into an in-memory cache. Missing cache entries can cause first-use effects, such as projectile decals, not to render immediately. Predicted resources in tags help pre-cache textures and sounds.                                                        |
| Sound system  | Sounds are loaded into an in-memory sound cache with a maximum of 512 entries or 64 MiB. Sound obstruction is based on collision ray tests between source and camera. Channel limits are 26 mono 3D, 4 mono, 4 stereo, and 4 44 kHz stereo channels.                         |
| Physics       | Halo 1 uses a custom in-house physics engine, not Havok. Vehicles use the `physics` tag for driving and vehicle-vs-vehicle/BSP/scenery behavior, but projectile/biped/particle collision uses `model_collision_geometry`.                                                    |
| AI            | AI behavior is driven by actors, encounters, squads, firing positions, pathfinding data, and scripts. AI have incomplete individual knowledge, track props, and select among 14 action states.                                                                               |
| Game state    | Runtime state is stored in datum arrays for objects, players, actors, effects, particles, script globals, script threads, object lists, and many other systems. Game state is updated at 30 simulation ticks per second and can be saved through savegames or core saves.    |
| Netcode       | Xbox uses the System Link Protocol. Gearbox PC netcode and later client-side hit-detection behavior are not Xbox runtime behavior.                                                                                                                                           |

## Limits and Constants

| Area                   | Limit or constant                                                                                                                                                   |
|------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Simulation             | 30 ticks per second.                                                                                                                                                |
| Renderer particles     | `particle_system` particles: 256; non-particle-system `particle` instances: 512.                                                                                    |
| Renderer objects       | Rendered object limit documented as 256 for the unmodified game.                                                                                                    |
| Renderer BSP           | Maximum dynamic BSP triangles: 16k. Surfaces per point light: 4096. Surfaces per dynamic object shadow: 4096. Dynamic lights: 128.                                  |
| World distance         | Camera is hard-coded to stay inside a 10000-world-unit cube centered on origin, 5000 units along any axis. Precision issues become visible around 1000 world units. |
| Vehicle vertical floor | Vehicles cannot move below about `-4950` world units on the Z axis.                                                                                                 |
| AI pathfinding         | Maximum pathfinding distance is 3276.7 world units. Halo 1 does not support pathfinding in movable reference frames.                                                |
| Network objects        | At most 511 network objects can be synchronized at once; index 0 means not networked.                                                                               |
| Grenade sync           | Grenade count is encoded as a four-bit signed integer; values above 7 underflow in synchronized multiplayer state.                                                  |

## Important Datum Arrays

| Table                       | Legacy limit | Relevance                                                                    |
|-----------------------------|--------------|------------------------------------------------------------------------------|
| objects                     | 2048         | Main live object table.                                                      |
| players                     | 16           | Player runtime state.                                                        |
| teams                       | 16           | Multiplayer and allegiance state.                                            |
| cached object render states | 256          | Renderer object-state cache.                                                 |
| decals                      | 2048         | Dynamic decal state.                                                         |
| particle systems            | 64           | Active particle-system datums.                                               |
| particle system particles   | 512          | Active particle-system particle datums.                                      |
| actors                      | 256          | Active AI actor state.                                                       |
| encounters                  | 128          | Scenario AI encounter state.                                                 |
| props                       | 768          | AI relationship/knowledge tracking.                                          |
| AI knowledge                | 768          | AI perception/knowledge state.                                               |
| object list headers         | 48           | Script-created object-list headers, reference-counted and garbage-collected. |
| list object references      | 128          | Linked object references belonging to object lists.                          |
| hs threads                  | 256          | HaloScript thread state.                                                     |
| hs globals                  | 1024         | Script globals and external globals.                                         |
| device groups               | 1024         | Scenario device-group state.                                                 |
| lights                      | 896          | Runtime light state.                                                         |
| light volumes               | 256          | Runtime light-volume state.                                                  |
| lightnings                  | 256          | Runtime lightning state.                                                     |
| effect                      | 256          | Active effect datums.                                                        |
| effect locations            | 512          | Effect location datums.                                                      |
| contrails                   | 512          | Active contrail state.                                                       |
| contrail points             | 1024         | Contrail point state.                                                        |
| mounted weapon units        | 8            | Built-in gunner/mounted weapon state.                                        |

## Debug Globals and Script Hooks

| Area      | Entries                                                                                                                                                                                                                                                                                                                      |
|-----------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Collision | `collision_debug`, `collision_debug_spray`, `collision_debug_repeat`, `collision_debug_length`, `collision_debug_width`, `collision_debug_height`, `collision_debug_point_*`, `collision_debug_vector_*`, and many `collision_debug_flag_*` globals control ray tests and collision-feature visualization.                   |
| Physics   | `physics_constants_reset`, `physics_get_gravity`, `physics_set_gravity`, and `vehicle_hover` expose selected physics controls. `physics_set_gravity` is not network synchronized.                                                                                                                                            |
| Sound     | `debug_sound`, `debug_looping_sound`, `debug_sound_cache`, `debug_sound_cache_graph`, `debug_sound_channels`, `debug_sound_environment`, `sound_cache_dump_to_file`, `sound_gain_under_dialog`, `sound_obstruction_ratio`, and `multiplayer_hit_sound_volume` expose cache, obstruction, channel, and dialogue-mix behavior. |
| AI        | `ai_render_props` is referenced by the AI props notes; `ai_show_swarms` displays swarm and swarm-component datum counts. Script functions such as `ai_place`, `ai_migrate`, `ai_follow_*`, `ai_conversation`, and `ai_set_*` bridge scripts into AI runtime state.                                                           |
| Renderer  | `rasterizer_wireframe 1` demonstrates portal-based occlusion culling. Texture-cache and predicted-resource behavior affects first-use rendering.                                                                                                                                                                             |

## Xbox-Specific Notes

- Xbox applies some in-memory tag edits for balance. The maps page notes that Tool applies some tag patches when building PC/CE maps, while the engine page states Xbox performs some balancing tag edits at runtime.
- Xbox renderer behavior is the target baseline. Gearbox-era DirectX 9, H1A DirectX 11, Chimera, MCC, and Custom Edition regressions are useful contrast but should not be treated as Xbox behavior.
- Xbox map headers use cache version `0x5`, build version `01.10.12.2276`, compression after the header, and Xbox-specific `scenario type` disk-cache selection.
- Xbox tag-index headers store vertex and triangle data as pointers rather than PC-style file offsets.
- Xbox does not use PC resource maps; resource-map behavior is relevant only when contrasting ports.
- Xbox multiplayer uses System Link Protocol, not Gearbox server-authoritative PC netcode.
- The engine can trigger main-menu attract videos on Xbox through `attract_mode_start`.

## Reverse-Engineering Relevance

- Map loading constants provide strong anchors: Xbox tag metadata base `0x803A6000`, cache version `0x5`, and Xbox build string `01.10.12.2276`.
- Datum-array limits are useful for identifying allocation tables, loop bounds, and asserts in the Xbox binary.
- Sound and texture cache behavior explains transient first-use misses; predicted-resource references should be traced from scenario/effect/object tags into cache loaders.
- Physics collision behavior differs by object class. Vehicle physics spheres, model collision BSPs, and scenario BSP collision are distinct code paths and should not be conflated in lifts.
- AI props, knowledge, encounters, firing positions, and pathfinding data are separate systems. Script functions often manipulate encounter/squad state rather than individual actor internals directly.
- Netcode notes imply that AI, BSP transitions, device-machine state, vehicle health, and some damage effects are not synchronized through normal multiplayer state.

## Open Questions

- The exact Xbox structures for most datum arrays remain reverse-engineering targets; the Reclaimers page documents limits and some fields only for glow and object-list header datums.
- The runtime implementation and addresses of Xbox in-memory balance tag edits should be validated in the binary before naming specific patch functions.
- Collision debug globals and flags need binary-backed mapping to global addresses before use as reliable symbols.
- The precise behavior of several collision debug flags is documented as unknown or unclear and should remain unknown until verified.

## Provenance

Adapted from the Reclaimers Library Halo 1 engine, renderer, physics engine, AI system, sound system, game state, netcode, and maps pages. Retrieved 2026-06-25. Non-Xbox-only PC, Custom Edition, H1A, OpenSauce, and MCC details were omitted except where needed to distinguish them from Halo CE for Xbox behavior. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
