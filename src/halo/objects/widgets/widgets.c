/* Object widgets — antenna, flag, light, etc. attached to game objects. */

/* Update all widget types. Iterates over 5 widget type handlers from
 * the function pointer table at 0x323548 (stride 40 bytes). */
void widgets_update(float delta_time)
{
  int16_t type;

  for (type = 0; type < 5; type++) {
    if (type < 0 || type > 4) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }
    {
      void (*handler)(float) =
        (void (*)(float)) * (void **)(0x323548 + (int)type * 40);
      if (handler != 0)
        handler(delta_time);
    }
  }
}

/* widgets_dispose — clears the per-widget debug handle slot, called from
 * objects_dispose.  Verified: single store of 0xffffffff to 0x0046f070. */
void widgets_dispose(void)
{
  *(int *)0x0046f070 = -1;
}

/* widgets_dispose_from_old_map — empty in the shipped build; reserved hook
 * called from objects_dispose_from_old_map. Verified: single RET at 0x1365b0.
 */
void widgets_dispose_from_old_map(void)
{
}
