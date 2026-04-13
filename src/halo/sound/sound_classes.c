void sound_classes_initialize(void)
{
  *(void **)0x50548c = game_state_malloc("sound classes", 0, 0x264);
}

void sound_classes_dispose_from_old_map(void)
{
}

void sound_classes_dispose(void)
{
  *(int *)0x50548c = 0;
}

void sound_classes_initialize_for_new_map(void)
{
  int16_t i;
  int offset;
  int *entry;

  i = 0;
  offset = 0;
  do {
    assert_halt(i >= 0 && i < 0x33);
    assert_halt(*(int *)0x50548c);
    entry = (int *)(*(int *)0x50548c + offset);
    i++;
    offset += 0xc;
    entry[1] = 0x3f800000;
    entry[0] = 0x3f800000;
    *(int16_t *)(entry + 2) = 0;
  } while (i < 0x33);
}
