/* Test the per-group flag bit for a transparent geometry group (0x184570).
 * Returns 1 when the group's bit in the 384-bit flag array at 0x4d0cbc is
 * CLEAR (or when the group pointer does not resolve to a presorted index),
 * 0 when the bit is SET. The array is 0x30 bytes (0x180 groups, one bit per
 * group) and is cleared by rasterizer_transparent_geometry_begin.
 * Binary: MOVSX EDX,AX / SAR EDX,5 -> signed word index; NEG EAX / SBB AL,AL /
 * INC AL -> AL = (bit == 0). */
char FUN_00184570(void *group)
{
  short presorted_index;

  presorted_index = rasterizer_transparent_geometry_group_to_presorted_index(
    (unsigned int)group);
  if (presorted_index != -1) {
    return (char)(((1 << (presorted_index & 0x1f)) &
                   ((unsigned int *)0x4d0cbc)[presorted_index >> 5]) == 0);
  }
  return 1;
}

/* FUN_001845b0: set or clear this group's bit in the transparent-geometry-group
 * bit vector at 0x4d0cbc (0x30 bytes = 12 dwords = 384 bits, matching the
 * 0x180 group cap; zeroed by the csmemset above). A group pointer that does not
 * resolve to a presorted index (-1) is silently ignored.
 *
 * NOTE the branch polarity, which is the opposite of what a "set flag" reading
 * would suggest and must not be "normalized": only the LOW BYTE of clear_bit is
 * tested (MOV CL,[EBP+0xc]; TEST CL,CL), and
 *   low byte == 0  -> OR   mask (SET the bit)   [own POP EBP/RET at 0x1845e8]
 *   low byte != 0  -> ANDN mask (CLEAR the bit) [RET at 0x18460d]
 * The set path returns early (two distinct RET sites), so the early-return
 * shape is reproduced here rather than an if/else.
 *
 * The index math runs on the SIGN-EXTENDED short (MOVSX ECX,AX then SAR ECX,5),
 * so the shift must stay arithmetic on a signed int. The explicit `& 0x1f` on
 * the shift count is real, not a Ghidra artifact: the original emits
 * AND ECX,0x1f before SHL EDX,CL in both branches. The word index (SAR ECX,5)
 * is recomputed inside each branch rather than hoisted above the TEST, so the
 * expression is written out per branch here. (0x1845b0) */
void FUN_001845b0(void *group, int clear_bit)
{
  short group_presorted_index;
  int index;

  group_presorted_index =
    rasterizer_transparent_geometry_group_to_presorted_index(
      (unsigned int)group);
  if (group_presorted_index == -1) {
    return;
  }
  index = group_presorted_index;
  if ((char)clear_bit == 0) {
    *(unsigned int *)(0x4d0cbc + (index >> 5) * 4) =
      *(unsigned int *)(0x4d0cbc + (index >> 5) * 4) | (1 << (index & 0x1f));
    return;
  }
  *(unsigned int *)(0x4d0cbc + (index >> 5) * 4) =
    *(unsigned int *)(0x4d0cbc + (index >> 5) * 4) & ~(1 << (index & 0x1f));
}

/* FUN_00184610: resolve the first vertex index of a transparent geometry group
 * (0x184610). Two mutually exclusive sources on the group record:
 *   +0x58  pointer to an int16 vertex index (nullable) -- when set, the stored
 *          index is returned directly. The load is `MOV AX,word ptr [EAX]`, a
 *          WORD load, so this must stay a 16-bit read (Ghidra models the upper
 *          half of EAX as CONCAT22 garbage).
 *   +0x54  dynamic-vertex-buffer index, sentinel -1 -- when not -1 it is passed
 *          (PUSH ESI / CALL 0x17c9c0 / ADD ESP,4 -- one stack arg; Ghidra drops
 *          it and shows a 0-arg call) to 0x17c9c0, whose short result is the
 *          return value.
 * With neither source the group has no vertices: report through error() at
 * level 2 and return -1 (EDI is pre-seeded 0xffffffff at entry purely to feed
 * `MOV AX,DI` on this path, so it is not modelled as a variable here).
 * The null-group assert tail is CALL 0x8e2f0 = system_exit(-1), not
 * halt_and_catch_fire (Ghidra prints thunk_FUN_001029a0). Every return path is
 * `MOV AX,...`, hence the 16-bit return type. */
short FUN_00184610(void *group)
{
  short *vertex_index;
  int dynamic_vertex_buffer_index;

  if (group == 0) {
    display_assert(
      "group",
      "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0xf4,
      1);
    system_exit(-1);
  }
  vertex_index = *(short **)((char *)group + 0x58);
  if (vertex_index != 0) {
    return *vertex_index;
  }
  dynamic_vertex_buffer_index = *(int *)((char *)group + 0x54);
  if (dynamic_vertex_buffer_index != -1) {
    return rasterizer_widget_draw_sprite2d(dynamic_vertex_buffer_index);
  }
  error(2, "### ERROR transparent geometry group has no vertices");
  return -1;
}

void render_initialize(void)
{
  cached_object_render_states = game_state_data_new(
    "cached object render states", 0x100, 0x100); /* dup-args-ok */
  assert_halt(cached_object_render_states);
}

void render_initialize_for_new_map(void)
{
  data_delete_all(cached_object_render_states);
}

/* Invalidate the cached render states data if it exists and is valid
 * (0x184ba0). Thunk through 0x18afe0. */
void j__render_dispose_from_old_map(void)
{
  int ptr = *(int *)0x50652c;
  if (ptr && *(char *)(ptr + 0x24) != 0) {
    data_make_invalid((data_t *)ptr);
  }
}

void render_dispose(void)
{
  cached_object_render_states = 0;
}

/* Render a window in pregame mode. window_type selects the render path:
 *   0 = full pregame UI (loading screen, menus, bink playback)
 *   1 = inactive window (no player assigned, simpler scene render)
 * Called from render_frame with window_type passed via EBX register. */
void render_window_pregame(int window_type, int16_t *win)
{
  window_parameters_t window_params;

  profile_render_window_start(0);
  csmemset(&window_params, 0, sizeof(window_parameters_t));

  qmemcpy(&unknown_global_camera, (char *)win + 4, sizeof(camera_t));
  render_camera_build_frustum(&unknown_global_camera, 0, global_frustum, 1);

  qmemcpy(&window_params.camera, (char *)win + 0x58, sizeof(camera_t));
  render_camera_build_frustum(&window_params.camera, 0, window_params.frustum,
                              1);

  /* set up scene parameters */
  window_params.unk_0[0] = 0;
  window_params.unk_0[1] = -1;
  *((uint8_t *)&window_params + 5) = (window_type == 0);

  rasterizer_window_begin(&window_params);

  switch (window_type) {
  case 0:
    ((void (*)(void))0x0dff70)();
    ((void (*)(void))0x17e190)();
    break;
  case 1:
    ((void (*)(void))0x0af9a0)();
    break;
  default:
    display_assert("!\"unreachable\"", "c:\\halo\\SOURCE\\render\\render.c",
                   0x11f, 1);
    system_exit(-1);
    break;
  }

  rasterizer_window_end();
  profile_render_window_end();
}

void render_frame_pregame(pregame_render_info_t *pregame_info,
                          void *main_globals_movie)
{
  window_parameters_t window_parameters;
  float elapsed[2];
  float progress;

  ++render;
  rasterizer_frame_begin(elapsed);
  rasterizer_windows_begin();
  profile_render_window_start(0);
  csmemset(&window_parameters, 0, 0x258u);

  qmemcpy(&unknown_global_camera, &pregame_info->cam0,
          sizeof(unknown_global_camera));
  render_camera_build_frustum(&unknown_global_camera, 0, global_frustum, 1);

  qmemcpy(&window_parameters.camera, &pregame_info->cam1,
          sizeof(window_parameters.camera));
  render_camera_build_frustum(&window_parameters.camera, 0,
                              window_parameters.frustum, 1);
  window_parameters.unk_0[0] = 0;
  rasterizer_window_begin(&window_parameters);
  render_ui_widgets(0, &pregame_info->cam1.viewport_bounds);
  bink_playback_render();
  if (game_map_loading_in_progress(&progress)) {
    progress_bar_display(progress);
  }
  rasterizer_window_end();
  profile_render_window_end();
  rasterizer_windows_end();
  rasterizer_frame_end();
}

void render_frame_present(_WORD *a1, void *a2)
{
  ((void (*)(_WORD *, void *))0x17c930)(a2, a1);
}

/* Render a single game window. win is the window struct (passed via ESI in the
 * original binary). offset_or_null points to a packed (x_tile, y_tile) pair
 * for split-screen tile subdivision, or NULL for full-screen rendering.
 * Handles fog distance clamping, camera frustum setup, optional water/sky
 * reflection rendering, and the main scene render pass. */
void render_window(int16_t *win, void *offset_or_null)
{
  char *esi = (char *)win;
  char *render_cam = esi + 4;
  char *rasterizer_cam = esi + 0x58;
  float bounds[4];
  int rendered_reflection = 0;
  char reflection_info[28];
  camera_t reflection_cam;
  float render_frustum[99];
  float rasterizer_frustum[99];
  float reflection_frustum[99];

  /* initialize render pass for this camera */
  ((void (*)(void *))0x1965f0)(render_cam);

  /* update render globals from scene */
  *(int16_t *)0x506732 = 0;
  FUN_0018fbc0(*(int16_t *)esi, (int)(uint16_t) * (int16_t *)0x50678a,
               (const float *)render_cam, (char *)0x506730);
  ((void (*)(int, void *))0x198f10)((int)(uint16_t) * (int16_t *)0x506784,
                                    (void *)0x506730);

  /* track closest fog distance when BSP index is unknown */
  if (*(float *)0x506748 != *(float *)0x2533c0) {
    if (*(int16_t *)0x50678a == -1 && *(float *)0x506770 > *(float *)0x506748) {
      *(float *)0x506770 = *(float *)0x506748;
    }
  }

  /* clamp z_far to fog distance when fog type matches */
  if (*(float *)0x506740 == *(float *)0x2533c8 &&
      *(float *)0x506748 != *(float *)0x2533c0) {
    float fog = *(float *)0x506748;
    if (fog < *(float *)(esi + 0x44))
      *(float *)(esi + 0x44) = fog;
  }

  /* clamp z_far to closest fog in mode 2 */
  if (*(int16_t *)0x50674c == 2 && *(float *)0x506770 != *(float *)0x2533c0) {
    float fog = *(float *)0x506770;
    if (fog < *(float *)(esi + 0x44))
      *(float *)(esi + 0x44) = fog;
  }

  /* fog sanity: z_far must exceed z_near */
  if (*(float *)(esi + 0x44) <= *(float *)(esi + 0x40)) {
    if (!*(uint8_t *)0x4d0d02) {
      error(2, "### ERROR something is wrong with the fog in the "
               "sky tag or the fog tag");
      *(uint8_t *)0x4d0d02 = 1;
    }
    *(float *)(esi + 0x44) = *(float *)(esi + 0x40) + *(float *)0x25bb10;
  }

  /* assert viewport and window bounds match between cameras */
  if (((int (*)(void *, void *, int))0x8da40)(esi + 0x30, esi + 0x84, 8) != 0) {
    display_assert("!memcmp(&window->render_camera.viewport_bounds, "
                   "&window->rasterizer_camera.viewport_bounds, "
                   "sizeof(rectangle2d))",
                   "c:\\halo\\SOURCE\\render\\render.c", 0xbb, 1);
    system_exit(-1);
  }
  if (((int (*)(void *, void *, int))0x8da40)(esi + 0x38, esi + 0x8c, 8) != 0) {
    display_assert("!memcmp(&window->render_camera.window_bounds, "
                   "&window->rasterizer_camera.window_bounds, "
                   "sizeof(rectangle2d))",
                   "c:\\halo\\SOURCE\\render\\render.c", 0xbc, 1);
    system_exit(-1);
  }

  /* compute frustum bounds from the render camera */
  ((void (*)(void *, float *))0x185950)(render_cam, bounds);

  /* split-screen tile adjustment: narrow bounds to this tile */
  if (offset_or_null != NULL) {
    int16_t *tile = (int16_t *)offset_or_null;
    int total = (int)*(int16_t *)0x31fa98 * (int)*(int16_t *)0x46e008;
    if (total > 0) {
      float tw = (bounds[1] - bounds[0]) / (float)total;
      float th = (bounds[3] - bounds[2]) / (float)total;
      int y_idx = total - (int)tile[1] - 1;
      float x0 = (float)(int)tile[0] * tw + bounds[0];
      float y0 = (float)y_idx * th + bounds[2];
      bounds[0] = x0;
      bounds[1] = x0 + tw;
      bounds[2] = y0;
      bounds[3] = y0 + th;
    }
  }

  /* build projection frustums for both cameras */
  render_camera_build_frustum((camera_t *)render_cam, bounds, render_frustum,
                              1);
  render_camera_build_frustum((camera_t *)rasterizer_cam, bounds,
                              rasterizer_frustum, 1);

  /* reflection rendering (single player local only) */
  if (game_connection() == 1) {
    char has_refl = ((char (*)(void *, void *, void *))0x1975e0)(
      render_cam, render_frustum, reflection_info);
    if (has_refl) {
      int saved_bsp = *(int *)0x506784;

      /* reflection requires full-screen viewport */
      if (*(int16_t *)(esi + 0x32) != 0) {
        display_assert("window->render_camera.viewport_bounds.x0==0",
                       "c:\\halo\\SOURCE\\render\\render.c", 0xe1, 1);
        system_exit(-1);
      }
      if (*(int16_t *)(esi + 0x30) != 0) {
        display_assert("window->render_camera.viewport_bounds.y0==0",
                       "c:\\halo\\SOURCE\\render\\render.c", 0xe2, 1);
        system_exit(-1);
      }
      if (*(int16_t *)(esi + 0x36) != 0x280) {
        display_assert("window->render_camera.viewport_bounds.x1=="
                       "RASTERIZER_TARGET_RENDER_PRIMARY_WIDTH",
                       "c:\\halo\\SOURCE\\render\\render.c", 0xe3, 1);
        system_exit(-1);
      }
      if (*(int16_t *)(esi + 0x34) != 0x1e0) {
        display_assert("window->render_camera.viewport_bounds.y1=="
                       "RASTERIZER_TARGET_RENDER_PRIMARY_HEIGHT",
                       "c:\\halo\\SOURCE\\render\\render.c", 0xe4, 1);
        system_exit(-1);
      }

      /* build reflection camera and frustum */
      ((void (*)(void *, void *, void *))0x186ef0)(render_cam, reflection_info,
                                                   &reflection_cam);
      render_camera_build_frustum(&reflection_cam, bounds, reflection_frustum,
                                  1);

      /* render reflection to secondary target */
      ((void (*)(int))0x17c960)(0);
      *(int *)0x506784 = (int)*(int16_t *)(reflection_info + 0x18);

      render_scene(-1, &reflection_cam, reflection_frustum, &reflection_cam,
                   reflection_frustum, 1, 0);

      /* restore BSP and switch back to main render target */
      *(int *)0x506784 = saved_bsp;
      ((void (*)(int))0x17c960)(1);

      rendered_reflection = 1;
    }
  }

  render_scene(*(int16_t *)win, render_cam, render_frustum, rasterizer_cam,
               rasterizer_frustum, 0, (char)rendered_reflection);
}

void render_frame(void *a2, __int16 a3, _WORD *a4, _WORD *a5, void *a6,
                  float a7)
{
  int16_t i;
  float elapsed[2];
  int16_t *win;
  int tick;
  int16_t offset[2];

  *(int32_t *)0x506540 += 1;
  *(float *)0x50654c = a7;
  csmemset(elapsed, 0, 8);
  tick = game_time_get();
  elapsed[0] = (float)tick * *(float *)0x2546a4;
  rasterizer_frame_begin(elapsed);
  rasterizer_windows_begin();
  win = (int16_t *)a2;
  for (i = 0; i < a3; i++) {
    *(int16_t *)0x50654a = i;
    if ((char)win[1] != '\0') {
      render_window_pregame(0, win);
    } else if (win[0] == -1) {
      render_window_pregame(1, win);
    } else {
      if (a5 != NULL && a4 != NULL) {
        offset[0] =
          (int16_t)(*(int16_t *)a4 * *(int16_t *)0x31fa98 + *(int16_t *)a5);
        offset[1] = (int16_t)(((int16_t *)a4)[1] * *(int16_t *)0x31fa98 +
                              ((int16_t *)a5)[1]);
      }
      render_window(win, a5 != NULL ? (void *)offset : NULL);
    }
    win += 0x56;
  }
  ((void (*)(void))0xe28e0)();
  rasterizer_windows_end();
  rasterizer_frame_end();
}
