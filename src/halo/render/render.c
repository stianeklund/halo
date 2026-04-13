void render_initialize(void)
{
  cached_object_render_states =
    game_state_data_new("cached object render states", 0x100, 0x100);
  assert_halt(cached_object_render_states);
}

void render_initialize_for_new_map(void)
{
  data_delete_all(cached_object_render_states);
}

void render_dispose(void)
{
  cached_object_render_states = 0;
}

void render_frame_pregame(pregame_render_info_t *pregame_info,
                          void *main_globals_movie)
{
  pregame_render_info_t *pregame_info2; // ebx
  window_parameters_t window_parameters; // [esp+Ch] [ebp-260h] BYREF
  float v4[2]; // [esp+264h] [ebp-8h] BYREF
  float v5; // [esp+274h] [ebp+8h] FORCED BYREF

  ++render;
  rasterizer_frame_begin(v4);
  rasterizer_windows_begin();
  profile_render_window_start(0);
  csmemset(&window_parameters, 0, 0x258u);
  pregame_info2 = pregame_info;
  qmemcpy(&unknown_global_camera, &pregame_info->cam0,
          sizeof(unknown_global_camera));
  render_camera_build_frustum(&unknown_global_camera, 0, global_frustum, 1);
  qmemcpy(&window_parameters.camera, &pregame_info2->cam1,
          sizeof(window_parameters.camera));
  render_camera_build_frustum(&window_parameters.camera, 0,
                              window_parameters.frustum, 1);
  window_parameters.unk_0[0] = 0;
  rasterizer_window_begin(&window_parameters);
  render_ui_widgets(0, &pregame_info2->cam1.viewport_bounds);
  bink_playback_render();
  if (game_map_loading_in_progress(&v5)) {
    progress_bar_display(v5);
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

void render_frame(void *a2, __int16 a3, _WORD *a4, _WORD *a5, void *a6,
                  float a7)
{
  int16_t i;
  float elapsed[2];
  int16_t *win;
  int tick;
  int offset;

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
    if (((char)win[1] == '\0') && (win[0] != -1)) {
      if (a5 != NULL && a4 != NULL) {
        offset =
          (int32_t)(*(int16_t *)a4 * *(int16_t *)0x31fa98 + *(int16_t *)a5) |
          ((int32_t)(((int16_t *)a4)[1] * *(int16_t *)0x31fa98 +
                     ((int16_t *)a5)[1])
           << 16);
      }
      render_window(win, a5 != NULL ? (void *)&offset : NULL);
    } else {
      render_window_pregame(win);
    }
    win += 0x56;
  }
  ((void (*)(void))0xe28e0)();
  rasterizer_windows_end();
  rasterizer_frame_end();
}
