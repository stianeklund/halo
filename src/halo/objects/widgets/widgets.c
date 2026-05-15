/* Object widgets — antenna, flag, light, etc. attached to game objects. */

/* widget_type_definition table at 0x323528, stride 0x28 (40) bytes. Fields:
 *   +0x04  uint8_t  needs_lighting
 *   +0x20  void (*update_proc)(float)            // update function
 *   +0x24  void (*render_proc)(int object_handle,
 *                              int  widget_definition_handle,
 *                              void *lighting,
 *                              void *parent_model_effect)
 * NUMBER_OF_WIDGET_TYPES = 5. */
#define WIDGET_TYPE_TABLE_BASE 0x00323528
#define WIDGET_TYPE_STRIDE 0x28
#define NUMBER_OF_WIDGET_TYPES 5

/* Pointer to the widget data array (data_t**). */
#define WIDGET_DATA_PTR 0x005a90c4

/* widgets_render_object_widgets — walks the widget list attached to an object
 * (head at object+0x11c, next pointer at +8) and invokes each widget type's
 * render handler.  Mis-named "widgets_initialize" in kb.json for historical
 * reasons; this is the render path called from render_objects.c:0x186. */
void widgets_render_object_widgets(int object_handle, int lighting,
                                   void *parent_model_effect)
{
  void *object;
  int widget_handle;
  void *widget;
  int16_t type;
  uint8_t *type_def;
  void (*render_proc)(int, int, int, void *);

  object = object_get_and_verify_type(object_handle, 0xffffffff);
  widget_handle = *(int *)((char *)object + 0x11c);
  while (widget_handle != -1) {
    widget = datum_get(*(data_t **)WIDGET_DATA_PTR, widget_handle);
    type = *(int16_t *)((char *)widget + 2);
    if (type < 0 || type >= NUMBER_OF_WIDGET_TYPES) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }
    type_def =
      (uint8_t *)(WIDGET_TYPE_TABLE_BASE + (int)type * WIDGET_TYPE_STRIDE);
    render_proc = *(void (**)(int, int, int, void *))(type_def + 0x24);
    if (render_proc != 0) {
      if (type_def[4] != 0 && lighting == 0) {
        display_assert("!type_definition->needs_lighting || lighting",
                       "c:\\halo\\SOURCE\\objects\\widgets\\widgets.c", 0xf1,
                       1);
        system_exit(-1);
      }
      render_proc(object_handle, *(int *)((char *)widget + 4), lighting,
                  parent_model_effect);
    }
    widget_handle = *(int *)((char *)widget + 8);
  }
}

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

/* widgets_initialize_for_new_map — empty in the shipped build; reserved hook
 * called from objects_initialize_for_new_map. Verified: single RET at 0x136580.
 */
void widgets_initialize_for_new_map(void)
{
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
