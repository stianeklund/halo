/* Object widgets — antenna, flag, light, etc. attached to game objects. */

#include "widget_types.h"

/* The static widget_type_definition table; layout and NUMBER_OF_WIDGET_TYPES
 * come from widget_types.h, which the binary proves is where they lived. */
#define WIDGET_TYPE_TABLE ((const widget_type_definition *)0x00323528)

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
  const widget_type_definition *type_def;
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
    type_def = &WIDGET_TYPE_TABLE[type];
    render_proc = type_def->render_proc;
    if (render_proc != 0) {
      if (type_def->needs_lighting != 0 && lighting == 0) {
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

/* Update all widget types. Walks the update_proc slot of each
 * widget_type_definition; the original starts the walk at 0x323548, which is
 * table base + offsetof(update_proc). */
void widgets_update(float delta_time)
{
  int16_t type;

  for (type = 0; type < NUMBER_OF_WIDGET_TYPES; type++) {
    if (type < 0 || type >= NUMBER_OF_WIDGET_TYPES) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }
    {
      void (*handler)(float) = WIDGET_TYPE_TABLE[type].update_proc;
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
