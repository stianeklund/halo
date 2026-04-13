/* Iterate all item objects (type 0x1c) and return true if any have
 * a positive danger count, indicating a dangerous item is near a player. */
bool dangerous_items_near_player(void)
{
  char iter[16];
  void *item;

  ((void (*)(void *, int, int))0x13d6f0)(iter, 0x1c, 1);
  for (item = ((void *(*)(void *))0x13d730)(iter); item != NULL;
       item = ((void *(*)(void *))0x13d730)(iter)) {
    if (*(int16_t *)((char *)item + 0x1a8) > 0)
      return true;
  }
  return false;
}
