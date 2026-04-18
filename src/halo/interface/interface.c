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
