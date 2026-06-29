# Halo 1 Reference: Creation

## Source Page

- Source: `https://c20.reclaimers.net/h1/tags/scenario_structure_bsp/lightmaps/`
- Local path: `tags/scenario_structure_bsp/lightmaps/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Lightmaps** are a combination of data storing the static "baked" lighting of a BSP. Since the BSP does not move, and many shadow-casting objects like scenery have fixed locations, base global illumination can be precalculated. Dynamic objects will cast shadow-mapped shadows and be lit using lightmap data at runtime.

Lightmap data in the BSP is comprised of both:

*   A reference to a generated bitmap containing diffuse lighting as texture sheets.
*   Mesh data which stores local lighting information for dynamic objects, among other purposes.

### Creation

The HEK's built-in _radiosity)_ process can be run using Tool or Sapien. It splits the BSP's render mesh into many fragments depending on shader") parameters like _simple parameterization_ and _detail level_, and UV-maps them to texture sheets. A texture is rendered to apply levels of light to those surfaces.

If higher resolution or greater control of lighting is desired, Aether facilitates texture baking in standalone 3D software.

Sky lights, emissive environment shaders"), scenery with lights, and light fixtures can all be used as light sources to illuminate the BSP. If you change any of these inputs, or move any scenery, you must re-run radiosity.

### Lighting for dynamic objects

All objects") usually receive their lighting from the environment using data in the BSP tag generated during radiosity. See object shadows and lighting").

Only moving objects like units") cast real-time shadows; scenery cast shadows in the baked lightmap using the object's collision model rather than its render model, likely because the collision model is stored using a BSP structure which is more efficient to perform lighting calculations with. Dynamic shadows will only be rendered if the ground point lightmap sample is bright enough or when default lighting applies.

The following are related functions that you can use in your scenario scripts and/or debug globals that you can enter into the developer console for troubleshooting.

|  | Function/global                                                                                                                                                                                                                 | Type   |
|--|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------|
|  | `(debug_object_lights)` Shows the incoming light colour and vector for all objects which results from sampling lightmap data at the ground point beneath the object. This data is used to shade the object and cast its shadow. | Global |
|  | `(object_light_ambient_scale)` Scales ambient light from the lightmap. Defaults to `0.4`.                                                                                                                                       | Global |
|  | `(object_light_interpolate)` Toggles if object lighting transitions smoothly when the object moves between different ground point surfaces or default lighting.                                                                 | Global |
|  | `(rasterizer_environment_diffuse_textures [boolean])` Disables diffuse textures in the BSP, showing just the lightmap shading and specular components.                                                                          | Global |
|  | `(rasterizer_environment_lightmaps [boolean])` Toggles the rendering of structure BSP lightmaps. When disabled, the level will be completely invisible.                                                                         | Global |
|  | `(rasterizer_lightmaps_filtering [boolean])` Enables or disables texture filtering for lightmaps. When disabled, lightmaps will appear blocky and jagged. Has no effect in H1A.                                                 | Global |
|  | `(rasterizer_lightmaps_incident_radiosity [boolean])` Toggles directional environmental bump mapped lighting. Does not affect the sampling of stored incident radiosity for object shadows.                                     | Global |
|  | `(rasterizer_lightmap_mode [short])` Changes the rendering mode of lightmaps:                                                                                                                                                   | Mode   | Description | | --- | --- | | `0` | Normal (default). | | `1` | BSP specular reflections will not be multiplied by the lightmap. | | `2` | Fullbright mode. You can set the ambient light with `rasterizer_lightmap_ambient`. | | `3` | Colours BSP surfaces by what lightmap bitmap index they use (technically, with a random colour seeded by the lightmap address). It can help to disable `rasterizer_environment_diffuse_textures` and `rasterizer_environment_reflections` to see this more clearly. | | `4` | Fullbright without environmental bump map lighting. Ambient light is not configurable in this mode. | Any higher value will appear the same as `1`. Repeated changing of this value can cause degraded performance and eventually a crash. | Global |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Conscars _(Object lighting tests)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/tags/scenario_structure_bsp/lightmaps/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
