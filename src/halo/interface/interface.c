/* UI/HUD interface subsystem init/dispose. */

void interface_initialize(void)
{
  ((void (*)(void))0xe33a0)();
  ((void (*)(void))0xd02f0)();
  ((void (*)(void))0x19b320)();
  ((void (*)(void))0xdc750)();
}

void interface_dispose_from_old_map(void)
{
  ((void (*)(void))0x19b3a0)();
  ((void (*)(void))0xd03e0)();
}

void interface_dispose(void)
{
  ((void (*)(void))0x19b3b0)();
  ((void (*)(void))0xe33e0)();
  ((void (*)(void))0xd0340)();
}

#define NUMBER_OF_INTERFACE_TAGS 16

/* Look up a tag_index from the interface globals tag block.
 * The interface globals element (size 0x130) contains an array of 16
 * tag_reference entries (each 0x10 bytes) starting at offset 0x0.
 * The tag_index field is at offset 0xc within each tag_reference.
 * Returns the tag_index for the given interface_tag_index slot. */
int interface_get_tag_index(int interface_tag_index)
{
  char *globals;
  char *element;

  assert_halt(interface_tag_index >= 0 &&
              interface_tag_index < NUMBER_OF_INTERFACE_TAGS);

  globals = (char *)game_globals_get();
  if (*(int *)(globals + 0x140) != 0) {
    globals = (char *)game_globals_get();
    element = (char *)tag_block_get_element(globals + 0x140, 0, 0x130);
    return *(int *)(element + interface_tag_index * 0x10 + 0xc);
  }

  return *(int *)((char *)0 + interface_tag_index * 0x10 + 0xc);
}

/* Look up an ARGB float color from a color_table ('colo') tag.
 * Uses interface_get_tag_index to resolve the color tag, then
 * indexes into the color table block (element size 0x30).
 * Color data is 4 floats (ARGB) at offset 0x20 within each entry.
 * The color_index is taken modulo the block count (as a short).
 * Returns out_color. */
void *interface_get_color(int interface_tag_index, short color_index,
                          void *out_color)
{
  int tag_idx;
  int *color_tag;
  int count;
  char *entry;
  float *out = (float *)out_color;

  tag_idx = interface_get_tag_index(interface_tag_index);

  out[3] = 1.0f;
  out[2] = 1.0f;
  out[1] = 1.0f;
  out[0] = 1.0f;

  if (tag_idx != -1) {
    color_tag = (int *)tag_get(0x636f6c6f, tag_idx);
    count = *color_tag;
    if (count != 0) {
      entry = (char *)tag_block_get_element(color_tag,
                                            (short)(color_index % count), 0x30);
      out[0] = *(float *)(entry + 0x20);
      out[1] = *(float *)(entry + 0x24);
      out[2] = *(float *)(entry + 0x28);
      out[3] = *(float *)(entry + 0x2c);
    }
  }

  return out_color;
}

/* Set up font, color, and style for drawing interface text.
 * Resolves font_index to a tag_index, looks up an ARGB color from the
 * interface color table (indexed by color_tag_index / color_index), then
 * configures the draw_string subsystem with those parameters. */
void interface_draw_text(int font_index, int style, int justify, int flags,
                         int color_tag_index, short color_index)
{
  float color[4];
  int tag_index;

  tag_index = interface_get_tag_index(font_index);
  interface_get_color(color_tag_index, color_index, color);
  draw_string_set_font(tag_index, style, justify, flags, color);
}

/* Draw black divider bars between split-screen viewports.
 * Called every frame when 2-4 local players are active.
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * 2 players: horizontal bar at y=239-241 across full width (640px).
 * 3 players: horizontal bar at y=239-241 top half; vertical bar at x=319-321
 *            bottom half (y=319-480).
 * 4 players: same as 3-player plus vertical bar top half (y=0-480). */
void interface_draw_splitscreen_dividers(void)
{
  bool forced_single;
  bool cinematic;
  __int16 player_count;
  int16_t rect[4]; /* {top, left, bottom, right} */

  forced_single = game_engine_force_single_screen();
  if (forced_single) {
    return;
  }
  cinematic = cinematic_in_progress();
  if (cinematic) {
    return;
  }
  player_count = local_player_count();
  if (player_count <= 1) {
    return;
  }

  /* 2-player: thin horizontal bar at y=239-241 across full width */
  rect[0] = 0xef; /* top    = 239 */
  rect[1] = 0; /* left   = 0   */
  rect[2] = 0xf1; /* bottom = 241 */
  rect[3] = 0x280; /* right  = 640 */
  draw_quad(rect, (int)0xff000000);

  if (player_count <= 2) {
    return;
  }

  /* 3/4-player: vertical right side of divider (bottom half) */
  rect[3] = 0x141; /* right  = 321 */
  rect[2] = 0x1e0; /* bottom = 480 */
  rect[1] = 0x13f; /* left   = 319 */

  if (player_count == 3) {
    /* 3-player: vertical bar at x=319-321, y=240-480 */
    rect[0] = 0xf0; /* top = 240 */
    draw_quad(rect, (int)0xff000000);
    return;
  }

  /* 4-player: vertical bar at x=319-321, y=0-480 */
  rect[0] = 0; /* top = 0 */
  if (player_count != 4) {
    display_assert("window_count==4",
                   "c:\\halo\\SOURCE\\interface\\interface.c", 0x374, 1);
    system_exit(-1);
  }
  draw_quad(rect, (int)0xff000000);
}

/* Initialize interface for a new map: set up HUD elements and load the
 * first interface globals tag block entry for widget rendering. */
void interface_initialize_for_new_map(void)
{
  char *globals;
  char *element;

  ((void (*)(void))0xd0360)();
  ((void (*)(void))0x19b330)();
  ((void (*)(void))0xdc7a0)();

  globals = (char *)game_globals_get();
  if (*(int *)(globals + 0x140) == 0) {
    element = 0;
  } else {
    globals = (char *)game_globals_get();
    element =
      (char *)((int (*)(void *, int, int))0x19b210)(globals + 0x140, 0, 0x130);
  }

  ((void (*)(int, int, int, int, void *))0x19b8b0)(*(int *)(element + 0x1c), -1,
                                                   0, 0, *(void **)0x2ee6c4);
}
