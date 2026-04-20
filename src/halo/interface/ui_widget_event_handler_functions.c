typedef bool (*ui_widget_event_handler_fn)(void *widget, void *event_data,
                                           bool *widget_deleted);

bool ui_widget_event_handler_dispatch(void *widget, int unknown,
                                      uint16_t handler_func_index,
                                      bool *widget_deleted)
{
  int16_t index = (int16_t)handler_func_index;
  ui_widget_event_handler_fn handler;
  const char *handler_name;
  bool ok;

  if (widget == NULL || widget_deleted == NULL) {
    display_assert(
      "(widget != NULL) && (widget_deleted != NULL)",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x1de,
      true);
    system_exit(-1);
  }

  if (index >= 0 && index < 0x66) {
    handler = ((ui_widget_event_handler_fn *)0x31e158)[index];
    ok = handler(widget, (void *)unknown, widget_deleted);
    if (!ok) {
      handler_name = ((const char **)0x31e2f0)[index];
      console_warning("event handler '%s' failed", handler_name);
    }
    return ok;
  }

  error(2, "invalid event_handler_function");
  return false;
}
