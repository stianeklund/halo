void ui_widgets_safe_to_load(bool a1)
{
}

/* display_error_when_main_menu_loaded — queues a single error message handle
 * to be shown the next time the main menu shell window is loaded. The
 * 16-bit slot at word_46CC48 holds the pending handle; -1 means "no
 * pending error". main_screen_shell_load consumes the slot by passing the
 * handle to the error-dialog routine and resetting it back to -1. Only
 * one queued message is supported at a time; subsequent calls while a
 * message is pending are dropped with a priority-2 warning. */
void display_error_when_main_menu_loaded(int16_t error_handle)
{
  if (word_46CC48 == -1) {
    word_46CC48 = error_handle;
    return;
  }
  error(2, "there is already an error message queued for display at the "
           "main menu; ignoring this one");
}

void ui_widgets_disable_pause_game(int duration_ticks)
{
  assert_halt(duration_ticks >= 0);
  dword_46CC44 = duration_ticks;
}
