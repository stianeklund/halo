# Halo 1 Reference: Configuration

## Source Page

- Source: `https://c20.reclaimers.net/h1/h1-ek/h1-sapien/`
- Local path: `h1-ek/h1-sapien/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Sapien** is an interactive scenario and BSP editor used for populating levels with objects and AI encounters, configuring BSP cluster data like wind and sound environments, running radiosity, compiling scripts, and more. It includes a scripting console and runs many of the same systems as Halo like AI and scripting, allowing you to preview encounters and inspect the game world with debug settings.

Unlike later games, H1 Sapien does not include a full player simulation mode and there is only a basic recorded animations mode, but you can use Standalone to test a level as a player instead.

This page covers both H1A and HEK versions of Sapien, which generally work the same but have some differences.

### Configuration

Similar to how Custom Edition and Standalone automatically run console commands at startup from `init.txt`, you can also create `editor_init.txt` for Sapien. Include console commands, one per line, in this file and Sapien will run them at startup. You can comment-out lines with a semicolon.

For example, enabling `debug_objects 1` in H1A Sapien has some different defaults than HEK Sapien. You may wish to change these defaults for each startup:

```
;restore default debug_objects appearance from HEK Sapien for H1A Sapien
debug_objects_collision_models 1
debug_objects_bounding_spheres 1
debug_objects_root_node 0
```

Or if you prefer not to see the green lines on the BSP when the camera goes outside it, you can disable it:

`debug_structure_automatic 0`
These lines will also be present in your console's history so you can use this to preload commonly used toggles.

H1A Sapien also supports several command line arguments. These features are experimental and might not work as expected:

*   `-multipleinstance`: Allow multiple instances of Sapien to be launched at once.
*   `-data_dir` and `-tags_dir`: Used to set custom content paths.

### Compile scripts

With the classic HEK tools, it's necessary to _compile scripts_ into your scenario tag using Sapien before they will take effect. Open your scenario in Sapien, then select _File > Compile Scripts_.

This takes the script sources from your level's `scripts` folder and compiles them into script node data embedded in your scenario tag, which is what the game actually uses at runtime. For example, if your scenario is `tags\levels\test\tutorial\tutorial.scenario`, you would place your script file in `data\levels\test\tutorial\scripts\`. Your script file can be named anything, but must have a `.hsc` file extension. If there are any compilation errors, Sapien will display them in the _Game window_.

This step is **not necessary with the H1A tools** because they automatically compile scripts from source data whenever a scenario tag is loaded by Sapien or Standalone, or built into a map using Tool.

### Switch BSP

It's common for singleplayer scenarios to include multiple BSPs. Use the _Edit > Switch BSP_ menu to change between them, but **never use**`switch_bsp` in the console. The menu option maintains proper editor state, whereas `switch_bsp` is intended for level scripts.

### Windows

### Game window

The game window is the main interface when interacting with objects in the level. It is also where you can run commands by pressing the ~ (tilde) key. The resolution and aspect ratio cannot be adjusted.

Movement of the camera is done in the same way as the in-game debug camera; **hold the middle mouse button** (unless you lock the mouse) plus:

*   Use the mouse to aim
*   Move with W, A, S, and D
*   Go up with R and down with F
*   Increase camera speed by scrolling down or pressing Shift
*   Decrease camera speed by scrolling up
*   Temporarily boost camera speed by holding Ctrl (new in H1A Sapien)

Camera rotation with the G key is only supported in-game and not in Sapien. If you have accidentally opened the singleplayer pause menu, it can be closed again with Middle mouse + Escape.

New in H1A Sapien, you can also use a gamepad to control the camera:

*   Right stick click: toggle gamepad control
*   Right stick: aiming
*   Left stick: horizontal movement
*   Right trigger: move up
*   Left trigger: move down
*   Left stick click: speed boost
*   D-pad up/down: speed increase/decrease (make sure to enable `framerate_throttle 1` first)

Known issues with gamepad camera control include low aiming sensitivity, lack of stick deadzones, slowness of vertical movement, and speed control framerate dependence.

### Hierarchy view

The Hierarchy view displays all the objects currently placed in the game and organizes them by type. The left pane of the window shows the Hierarchy tree and currently selected type, and the right pane shows the objects of this selected group or type that are currently placed in the level.

### Tool window

This window contains settings for the currently active tool mode, such as object placement, detail object painting, or cluster properties application. The currently active tool depends on the selected hierarchy view item.

The most commonly used settings, or options that are modified the most, are the options under the _Active marker handles_ section and the _Don't draw center marker_ option.

### Properties palette

The Properties palette window displays the properties for the currently selected hierarchy item. The type of object can be changed or chosen in this display as well as various other properties such as the position and rotation of the object, and spawn flags that set various attributes for the object. When applying cluster properties, the camera location in the game window determines the active cluster shown in this window.

If this window doesn't let you interact with it and plays a Windows alert sound when you try, you have an invalid field. Look for which field is being highlighted and correct it (e.g. change `-0` rotation to just `0`).

### Output window

This window is only relevant when recording animations and can be ignored or minimized otherwise.

### Keyboard shortcuts/hotkeys

Some of these shortcuts are only used in certain windows or editor modes.

### General

*   ~: Opens the HaloScript console, pressing it again or pressing enter on an empty console will close it.
*   Space: clones the selected object to the camera's location and orientation. If multiple objects are selected, uses the first.
*   Pause/Break: Pauses your Sapien instance. Press "OK" in the opened window to resume Sapien.
*   Control + B: Open the BSP switch dialog window.
*   Control + Shift + B: Creates the file `baggage.txt`. If you end up getting a maximum tag slots error or are running low on tag space, this file shows the memory usage of tags in the editor.
*   Shift + Click: Select a group of objects or keep previously placed objects selected. You can also use it to select the first and last object in the hierarchy list to select everything in-between at once. Useful for deleting multiple objects or moving them all at once.
*   Control + Click: Select a group of objects or keep previously placed objects selected. This will only select the object you specifically click in the hierarchy list. Useful for deleting multiple objects or moving them all at once.
*   Hold Tab: Using this key combo while having an object selected will set the rotation gizmo to sync with the local rotation of the object. Only really useful if "Local Axes" is not enabled.
*   In the hierarchy view, pressing a key will cycle through all folders that start with that character. For example, pressing A while having the "Missions" folder expanded will immediately take you to the "AI" folder.
*   N: This hotkey will snap a selected object to the normal of the ground below it. **This hotkey is broken in HEK Sapien and can cause it to crash when restarted**, also causing editor icons and name overlays to disappear for the session. This problem was fixed in H1A Sapien.

### Encounters and AI

*   Middle mouse + F1: Selects the spawned actor in the center of the game view.
*   Middle mouse + F2 Select next encounter. You can also use the console command `ai_select <encounter>`.
*   Middle mouse + F3: Select previous encounter.
*   Middle mouse + F4: When an encounter is selected, selects the next actor.
*   Middle mouse + Shift + F4: Selects the previous actor.
*   Middle mouse + F5: Cycles through render modes for actor sprays:
    *   Actions
    *   Activation status
    *   None

*   Middle mouse + F6: Erase all _spawned_ actors, e.g. those created with `ai_place`.
*   M: Toggles group labels on firing positions, shows the default actor for move positions used by a squad instance, and highlights editor gizmos/placeholders, making them easier to see.

### Recorded animations

These hotkeys apply in scripted camera mode.

*   A: Toggle "Attach camera to unit" option.
*   E: Toggle "Edit camera point" option.
*   C: Toggle "Scripted camera control".
*   Space: Creates a new camera point at the game view camera's location if "Edit camera point" is disabled. If "Edit camera point" is enabled then it instead moves the "Active camera point" to the camera's location.
*   Shift + V: Using this key combo while in scripted camera mode will take over (posess) the selected unit.
*   Backspace: Cycles through camera types for the posessed unit:
    *   First person
    *   Third person
    *   Flycam

*   Caps lock: Start/stop animation recording.
*   Shift + Q: Exits a posessed unit while in scripted camera mode.

See main page: recorded-animations

### Detail objects painting

With detail objects added to their palette, you can use the _Tool window_ to adjust painting settings and paint within the _Game window_.

*   Left Click: Paints detail objects around the cursor.
*   Right Click: Erases detail objects around the cursor.
*   Shift + Right Click: Erases all detail objects in the red-highlighted cell.
*   Shift + Control + Right Click: As above, but also deleted the cell itself.
*   Shift + Control + L: Relight detail objects (useful after updating lightmaps).

### Radiosity

Both Tool and Sapien can be used to generate lightmaps, though using H1A tool with asserts disabled is **strongly recommended** for high quality lightmaps since it is easier to control the stop parameter (when to save), is much faster, and doesn't require the window to be focused.

Sapien is suitable for draft lighting on basic maps. Enter these console commands in order:

```
;0 for draft quality, 1 for final
radiosity_quality 0
;begins radiosity. numbers will start to count down
radiosity_start
;wait for the numbers to count down to 0 or near 0, then:
radiosity_save
```

Set `radiosity_step_count 1` for more frequent progress feedback.

### Limits

Sapien has 5x higher object limits than the game itself (memory pool size and object count at 2048*5). This is because many objects may simultaneously exist in Sapien which are actually scripted to appear at certain times only during gameplay. Despite this, you may run into other engine limits when dealing with extracted scenarios originally merged from child scenarios which wouldn't have individually hit limits.

HEK Sapien is limited to 2 GB of virtual memory even on modern 64-bit Windows systems, which is common for older 32-bit applications. H1A Sapien is already "large address aware" (LAA). While this memory limit is usually not an issue, an abundance of large textures or other tags in a map may cause HEK Sapien to crash. To work around this, `sapien.exe` can be marked LAA to tell the OS it supports 4 GB of virtual memory:

*   Applying value `0x2F` at offset `0x136` in the executable if you're comfortable using a hex editor like ImHex).
*   Or, install and run NTCore 4GB Patch. Select the Sapien executable.

### Compatibility

*   H1A Sapien requires DX11 support, whereas HEK Sapien requires DX9.
*   HEK Sapien users have reported problems saving tags due to the Windows VirtualStore. Ensure you have the right permissions.
*   On Linux, HEK Sapien can be run successfully using Wine but may not yet be compatible with DXVK. If not, use built-in or standard native DirectX libraries instead.

### Troubleshooting

### Interface

| Issue | Solution |
| --- | --- |
| Child windows are not visible or stuck outside the main window. | Open the following registry key using regedit and delete all entries ending with "rect": * H1A: `HKEY_CURRENT_USER\Software\i343\halo1a_sapien` * HEK: `HKEY_USERS\S-1-5-21-0-0-0-1000\Software\Microsoft\Microsoft Games\Halo HEK\sapien` (user ID may vary) |
| Clicking the _Edit Types_ button doesn't open a window. | Delete `HKEY_CURRENT_USER\Software\i343\halo1a_sapien` in regedit. |
| Can't change the open scenario | This is a known issue, simply close Sapien and open it again; this will allow you to open the scenario. |
| Mouse is locked to game view | Issue/feature with some keyboard layouts, press the middle mouse button to unlock it. |
| The game window is completely black and does not display the console when ~ (tilde) is pressed. | HEK Sapien, like Custom Edition, does not support MSAA. Disable anti-aliasing for Sapien in your graphics control panel. Fixed in H1A Sapien. |
| The _Edit Types_ window does not allow tags to be added. | Check the debug.txt log for errors. Otherwise, with HEK Sapien, try running without a compatibility mode if you've set one. |
| Child windows are not visible or stuck outside the main window. | Open the registry key using regedit and delete all entries ending with "rect". |
| Debug wireframe colors and bounding radii change at angles and turn black, making it hard to identify their types. | None known for HEK Sapien, fixed in H1A Sapien |

### Crashes

When Sapien crashes, check `debug.txt` for hints. Most problems are due to invalid tags. HEK Sapien logs `Couldn't read map file './sapienbeta.map'` but you can ignore this.

| Error | Solution |
| --- | --- |
| ``` EXCEPTION halt in \...\sound\sound_dsound_pc.c,#1966: properties->gain>=0.f && properties->gain<=1.f EXCEPTION halt in \halopc\haloce\source\sound\sound_dsound_pc.c,#1940: properties->gain>=0.f && properties->gain<=1.f ``` | This is probably not an issue with your tags, but rather Sapien failing to initialize its sound system (perhaps after device changes or system sleep). Restart your system or put `sound_enable 0` into Sapien's `editor_init.txt` if you don't need to hear sounds in Sapien for now. Also ensure you don't have any scripts setting `sound_set_music_gain` over 1. |
| ``` EXCEPTION halt in \\...\rasterizer\dx9\rasterizer_dx9.c,#1461: global_window_parameters.fog.planar_maximum_depth>0.0f EXCEPTION halt in \halopc\haloce\source\rasterizer\dx9\rasterizer_dx9.c,#2014: global_window_parameters.fog.planar_maximum_depth>0.0f ``` | Try moving or resizing your fog plane(s). |
| ``` EXCEPTION halt in \\...\tag_files\tag_groups.c,#3395: group_tag==NONE || tag_group_get(group_tag) EXCEPTION halt in \halopc\haloce\source\tag_files\tag_groups.c,#2419: group_tag==NONE || tag_group_get(group_tag) ``` | Sapien has encountered an unrecognized tag class, such as an OpenSauce tag or vestigial tag. Remove references to this tag class. |
| `EXCEPTION halt in \\...\sound\sound_dsound_pc.c,#2083: play_cursor_position >= 0 && play_cursor_position < GetAvgBytesPerSecond(sound_samples_per_second(channel_type_sample_rate(channel->type_flags)), channel_get_num_channels(channel_index))` | A sound device was lost or disabled while Sapien was running. Make sure you don't unplug anything and considering using the `-nosound` argument if you have wireless headphones that automatically disconnect. |
| `EXCEPTION halt in \\...\sound\sound_dsound_pc.c,#2151: length <= channel->buffer_size` | Same as above. |
| `EXCEPTION halt in objects.c,#2419: got an object type we didn't expect (expected one of 0x00000001 but got #1).` | Attempted to take over a unit in recording mode while no unit was selected. Make sure to select a unit first. |
| `\halopc\haloce\source\rasterizer\dx9\rasterizer_dx9_hardware_bitmaps.c(148): E_OUTOFMEMORY in IDirect3DDevice9_CreateTexture(global_d3d_device, width, height, bitmap->mipmap_count+1-mip_levels_to_drop, 0, rasterizer_bitmap_format_table[bitmap->format], D3DPOOL_MANAGED, &(IDirect3DTexture9*)bitmap->hardware_format, NULL) (code=-2147024882, error=<can't get description>) 10.01.19 17:07:33 couldn't allocate #1398128 tag data for 'bitmap_pixel_data'` | You are running out of memory in HEK Sapien. Try freeing up more physical memory on your system, and/or using a large address aware Sapien or H1A Sapien instead. |
| `\halopc\haloce\source\rasterizer\dx9\rasterizer_dx9_hardware_bitmaps.c(148): E_OUTOFMEMORY in IDirect3DDevice9_CreateTexture(global_d3d_device, width, height, bitmap->mipmap_count+1-mip_levels_to_drop, 0, rasterizer_bitmap_format_table[bitmap->format], D3DPOOL_MANAGED, &(IDirect3DTexture9*)bitmap->hardware_format, NULL) (code=-2147024882, error=<can't get description>)` | You have a bitmap tag which is too large. Do not exceed dimensions of 2048 pixels because support is GPU-dependent; technically DirectX 9 did not allow sizes over this limit. This issue is fixed in H1A Sapien as it updated to DX11 which doesn't have this issue. |
| `EXCEPTION halt in .\\\\detail_object_tool_handler.cpp,#103: &diffuse_color: assert_valid_real_rgb_color(-9.395227, -3.398408, -2.530689)` | A detail object was painted outside the map. Be careful when painting around corners and small spaces, and save frequently. Presumed fixed in H1A Sapien. |
| `EXCEPTION halt in /halopc/haloce/source/cseries/profile.c,#442: parent_timesection->self_msec >= child_timesection->elapsed_msec` | This may be caused by a multi-core processor. Try running in Windows 98 compatibility mode, or setting the process affinity to a single core using Task Manager before opening the scenario. This issue is fixed in H1A Sapien. |
| `EXCEPTION halt in \halopc\haloce\source\rasterizer\rasterizer_transparent_geometry.c,#137: group->sorted_index>=0 && group->sorted_index<transparent_geometry_group_count` | An object") has _transparent self occlusion_ enabled while also referencing a transparent shader") with _extra layers_. This is not a problem in-game. This issue is fixed in H1A Sapien. |
| `EXCEPTION halt in .\hierarchy_items.cpp,#2030: can_add_manually` | You clicked the "New instance" button when it was meant to be disabled. Fixed in H1A. |
| `EXCEPTION halt in \halopc\haloce\source\tag_files\tag_groups.c,#3157: #0 is not a valid shader_transparent_chicago_map_block index in #0,#0)` | Your map uses a transparent shader with no maps defined. Add at least 1. |
| `EXCEPTION halt in \halopc\haloce\source\tag_files\tag_groups.c,#3142: offset>=0 && offset+size<=data->size` | You may have a corrupted [sound tag which was extracted with HEK+. Always use invader-extract instead. |
| `EXCEPTION halt in c:\mcc\main\h1\code\h1a2\sources\render\render.c,#249: !memcmp(&window->render_camera.viewport_bounds, &window->rasterizer_camera.viewport_bounds, sizeof(rectangle2d))` | You resized the game window while `debug_render_freeze` was enabled. Disable it first. |
| `EXCEPTION halt in e:\jenkins\workspace\mcch1code-release\mcc\release\h1\code\h1a2\sources\ai\actor_firing_position.c,#1431: (test_surface_index >= 0) && (test_surface_index < collision_bsp->surfaces.count)` | This error happens when firing positions cannot detect a BSP surface, possibly from being outside the BSP. This may have happened if the BSP has changed shape since they were placed. Switch through each of your BSPs and look for firing positions in the _Hierarchy view_ with a red X beside them. Remove then recreate these firing positions. |
| `EXCEPTION halt in c:\mcc\main\h1\code\h1a2\sources\tag_files\tag_groups.c,#4440: #2 is not a valid scenario_detail_object_collection_palette_block index in #0,#2)` | You've possibly painted detail objects into the BSP and saved the BSP tag using Sapien, but not yet saved the scenario tag. This will result in Standalone not being able to find the palette entry. |

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/h1-ek/h1-sapien/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
