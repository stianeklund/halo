# Halo 1 Tag: sky

## Summary

The **sky** tag, often called a **skybox**, models the environment outside the BSP. It contains radiosity parameters that affect lightmaps like ambient light and directional lights, defines general atmospheric fog (not to be confused with planar fog), and can have a 3D model that draws behind level geometry.

## Tag Information

Tag group: `sky`.

HaloScript entries:

- `rasterizer_environment_fog`
- `rasterizer_fog_atmosphere [boolean]`
- `rasterizer_ray_of_buddha [boolean]`

Defined fields and enum values mentioned:

- animation graph
- animations
- animations animation index
- animations period
- flags sun
- indoor ambient radiosity color
- indoor ambient radiosity power
- indoor fog color
- indoor fog maximum density
- indoor fog opaque distance
- indoor fog screen
- indoor fog start distance
- lights
- lights color
- lights diameter
- lights direction
- lights direction pitch
- lights direction yaw
- lights flags
- lights flags affects exteriors
- lights flags affects interiors
- lights lens flare
- lights lens flare marker name
- lights power
- lights test distance
- model
- occlusion radius
- outdoor ambient radiosity color
- outdoor ambient radiosity power
- outdoor fog color
- outdoor fog maximum density
- outdoor fog opaque distance
- outdoor fog start distance
- shader functions
- skies

## Details

The **sky** tag, often called a **skybox**, models the environment outside the BSP. It contains radiosity parameters that affect lightmaps like ambient light and directional lights, defines general atmospheric fog (not to be confused with planar fog), and can have a 3D model that draws behind level geometry.

The sky is not responsible for weather effects, which are instead assigned to clusters in Sapien.

### Indoor skies

Even fully indoor levels can use a sky. Sky tags contain both outdoor/exterior and indoor/interior options for ambient light, fog, and directional light. The first sky in the scenario's skies block (sky `0`) has special meaning and its indoor options are used for any indoor clusters the BSP has. A fully indoor level with no visible sky faces will only have indoor clusters.

However, it is not mandatory that indoor levels use a sky. Without any skies in the scenario the space outside the BSP will simply be black and there will be no interior fog or ambient light, as is the case with _Chiron TL34_. Indoor levels can still be lit using light-emitting shaders in the BSP and objects that affect lightmaps.

### Multiple skies

Scenarios can reference up to 8 skies. This is mostly useful for long singleplayer levels where the artist wants to portray a change in weather, time of day, or lighting in a different BSP. This is accomplished by using the `+sky0` and `+sky1`special material names, for example. Tool will not permit any cluster to have a mix of these special materials.

The scenario _should_ reference a sky in the skies block for each sky index used in special materials. For example, if you used `+sky0`, `+sky1`, and `+sky2` then the skies block should contain 3 sky references. The game and lightmapping process is tolerant if this is not the case, however. Any missing skies block entries or empty/null sky references will simply result in black being seen through those sky faces and those clusters receiving no sky lighting and fog.

When the game changes to a different sky the fog colour will smoothly transition over cumulative camera movement of approximately 20 world units. In other words, the fog doesn't change if the camera isn't moving and it needs to move about 20 units (even back and forth) to fully transition. The model itself does not transition in any way and changes instantly. Transitions can happen as the camera/player moves between clusters with different skies, between indoor and outdoor clusters, or during a BSP switch. An example of the latter can be seen in _Assault on the Control Room_ when the player reaches the first chasm and Cortana says "the weather patterns here seem natural"; look up and you should see the fog transition as you move.

### Animation

The model of a skybox can be rigged and animated with an overlay (JMO) model_ animations. Don't forget to set an animation period in the sky's _animations_ block. By animating the skybox you can have moving moons, ships, clouds, and even moving sun markers. This cannot be used to achieve dynamic time of day lighting for your map because lightmaps and dynamic object shadow directions are precomputed based on the sky's rest position and cannot be animated.

You can also animate the sky's shaders for things like sliding cloud textures or a twinkling star effect.

The following are related debug globals that you can enter into the developer console for troubleshooting.

|  | Global |
| --- | --- |
|  | `(rasterizer_environment_fog)` Toggles both environmental sky fog and fog plane colors. Does not affect _fog screen_. Use `rasterizer_fog_plane` or `rasterizer_fog_atmosphere` to individually toggle fog types. |
|  | `(rasterizer_fog_atmosphere [boolean])` Toggles atmospheric fog as defined in the active sky tag. |
|  | `(rasterizer_ray_of_buddha [boolean])` Toggles the lens flare "god rays" effect, present on sky lights or lens flares explicitly set to _sun_. |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| model | `TagDependency` * gbxmodel * model | An optional reference to a model which is rendered for this sky. Fully indoor maps where the sky isn't visible don't need to have a model since there would be no way to see it (and it doesn't render from indoor clusters anyway). |
| animation graph | `TagDependency`: model_animations | An animation which can move parts of the sky model, including markers. This must be an overlay (JMO) animation to work. |
| indoor ambient radiosity color | `ColorRGB` | Sets the colour used for ambient light in indoor clusters. This can be used to brighten indoor spaces to a minimum level. |
| indoor ambient radiosity power | `float` | * Min: 0 Sets the strength of indoor ambient light. This value must not be negative (causes a crash in pre-H1A versions). |
| outdoor ambient radiosity color | `ColorRGB` | Sets the colour used for ambient light in outdoor clusters. It is like a fill light that approximates light coming from all directions and will be most visible in shadowed areas that would not be lit otherwise. This is typically a similar colour to the sky's backdrop, e.g. blue for a blue sky and black in space. |
| outdoor ambient radiosity power | `float` | * Min: 0 Sets the strength of outdoor ambient light. Values around `0.2` are common. If this value is too high the level will appear flat and shapeless from a lack of strong shadows. This value must not be negative (causes a crash in pre-H1A versions). |
| outdoor fog color | `ColorRGB` | The colour of the atmospheric fog which draws over the BSP by distance while in outdoor clusters. |
| outdoor fog maximum density | `float` | * Min: 0 * Max: 1 * Default: 1 Limits the maximum density of outdoor fog. For example, a value of `0.75` would mean that no matter how far away parts of the level and objects are, the fog will only ever reach 75% opacity and you will still be able to see their textures and details faintly. If this value is `1` then distant areas beyond _outdoor\_ fog\_ opaque\_ distance_ will always be _outdoor\_ fog\_ color_. |
| outdoor fog start distance | `float` | * Unit: world units Within this distance there will be no outdoor fog drawn. This is the start distance where fog begins to fade to its maximum density. A small value like `0` or `1` is typical. |
| outdoor fog opaque distance | `float` | * Unit: world units The distance where fog reaches its maximum density. Note that the _outdoor\_ fog\_ maximum\_ density_ may limit how opaque it actually is at this distance. For comparison, Blood Gulch's sky uses `100` while The Maw's uses `20`. |
| indoor fog color | `ColorRGB` | The colour of the atmospheric fog which draws over the BSP by distance while in indoor clusters. |
| indoor fog maximum density | `float` | * Min: 0 * Max: 1 * Default: 1 Limits the maximum density of indoor fog. If this value is `1.0` then distant clusters beyond _indoor\_ fog\_ opaque\_ distance_ will be culled from the PVS. |
| indoor fog start distance | `float` | * Unit: world units Within this distance there will be no indoor fog drawn. This is the start distance where fog begins to fade to its maximum density. A small value like `0` or `1` is typical. |
| indoor fog opaque distance | `float` | * Unit: world units The distance where fog reaches its maximum opacity for indoor clusters. When _indoor fog maximum density_ is `1.0`, this distance helps cull visible clusters when Tool is generating the PVS. |
| indoor fog screen | `TagDependency`: fog | * Unused This field is seemingly unused but was probably intended to allow indoor clusters to have a fog screen effect like planar fog. Setting a reference here does not cause anything to render even in H1A where fog screen effects were fixed for planar fog. |
| shader functions | `Block` | * HEK max count: 8 * Unused Not used by the engine. Ignore this field. |
| animations | `Block` | * HEK max count: 8 Defines the repeating period (duration) of sky model animations. |
Field values:
- animation index (`int16`): An index into the model_ animations animations block.
- period (`float`): * Unit: seconds How long it takes the animation to complete 1 repeat.
| lights | `Block` | * HEK max count: 8 A list of directional light sources that cast light on the level. These are mainly used to represent incoming sunlight, but you can include multiple lights to model different light sources. For example, if you were making a night sky with two moons you could have two lights, or if your sky is cloudy you may wish to have a direct overhead light with a high _diameter_ and low _power_ to model soft lighting bounced down from the clouds. A sky doesn't _have to_ have any lights. Indoor skies like the one for _The Library_ have no need unless you have a special reason to force directional lights indoors with the _affects interiors_ flag. |
Field values:
- lens flare (`TagDependency`: lens_flare): If provided, this light will appear in the sky with a lens_ flare. If the flare's occlusion radius is set to exactly `50`, the flare will also render the _Ray of Buddha_ effect regardless of if the referenced lens flare's _sun_ flag is set. If this reference is left blank then the light source itself will be invisible (but still casts light for lightmaps).
- lens flare marker name (`TagString`): If provided, the lens flare will be rendered in the sky at the location of this marker in the model. If a marker name is provided but no such marker exists in the model then no lens flare will be rendered at all. Otherwise, when a marker name is not provided the game will render the lens flare according to the light's _direction_.
- flags (`bitfield`): Flags that control which cluster types this light affects.
- affects exteriors (`0x1`): If enabled, this light will affect outdoor clusters.
- affects interiors (`0x2`): If enabled, this light will affect indoor clusters. Remember to set this light's _test distance_ low enough or else this flag will seem to have no effect. You will not need to set this flag except in special circumstances.
- color (`ColorRGB`): The colour of the directional light. Overhead sunlight will be a yellowish white, while sunsets will be more orange.
- power (`float`): * Min: 0 The irradiance power of the sky light. This value must not be negative (causes a crash in pre-H1A versions).
- test distance (`float`): The maximum distance to search for a shadow-casting surface from a surface being lit towards the sky light. The radiosity process shoots rays toward sky lights to determine if they are directly lit, but they can be limited with this field. If you are finding that distant parts of the BSP are not casting shadows where you expect, try increasing this. Increasing the distance likely has a time cost for radiosity since more surfaces need to be considered.
- direction (`Euler2D`): The direction _toward_ the sky light from the origin. The _yaw_ is in a counterclockwise direction from the +X axis while the _pitch_ ranges from `-90` (straight below) to `90` (straight above), with `0` being at the horizon.
- yaw (`float`): Rotation to the left or right around the Z (vertical) axis.
- pitch (`float`): Rotation up or down.
- diameter (`float`): Angular diameter of the light source. It is entered in degrees in Guerilla but internally stored in radians. A wider angular diameter makes shadows softer. Increase this diameter if your light source is diffuse like as an overcast sky.

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `sky`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
