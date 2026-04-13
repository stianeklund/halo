void numeric_countdown_timer_update(void)
{
  int current_time;

  current_time = *(int *)0x4d8a80;
  if (*(char *)0x4d8a7c) {
    current_time = (game_time_get() * 1000) / 30;
    if (*(int *)0x4d8a80 <= current_time) {
      *(int *)0x4d8a78 += *(int *)0x4d8a80 - current_time;
      if (*(int *)0x4d8a78 < 0)
        *(int *)0x4d8a78 = 0;
    }
  }
  *(int *)0x4d8a80 = current_time;
}
