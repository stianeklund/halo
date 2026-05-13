void game_statistics_start(void)
{
  *(uint8_t *)0x457060 = 1;
}

/* Update game statistics for all items in the data table.
 * Stores the current game time (in seconds) at +0xb8, marks entry active at
 * +0x8e, and increments the match counter at +0x90 for entries matching
 * param_1. */
void FUN_000b5650(short param_1)
{
  data_iter_t local_14;
  char *item;
  int time;

  data_iterator_new(&local_14, *(void **)0x5aa6d4);
  item = (char *)data_iterator_next(&local_14);
  if (item != NULL) {
    do {
      time = game_time_get();
      *(int *)(item + 0xb8) = time / 0x1e;
      *(short *)(item + 0x8e) = 1;
      if (*(int *)(item + 0x20) == (int)param_1) {
        *(short *)(item + 0x90) += 1;
      }
      item = (char *)data_iterator_next(&local_14);
    } while (item != NULL);
  }
  *(char *)0x457060 = 0;
}
