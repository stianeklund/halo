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

/* Return a pointer to the sound class entry at class_index.
 * Each entry is 0xc bytes in the sound_class_data array.
 * class_index passed in SI (register arg). */
void *sound_class_get(int class_index /* @<si> */)
{
  int16_t idx = (int16_t)class_index;

  if (idx < 0 || idx >= 0x33) {
    display_assert("index>=0 && index<NUMBER_OF_SOUND_CLASSES",
                   "c:\\halo\\SOURCE\\sound\\sound_classes.c", 0x120, 1);
    system_exit(-1);
  }
  if (*(int *)0x50548c == 0) {
    display_assert("sound_class_data",
                   "c:\\halo\\SOURCE\\sound\\sound_classes.c", 0x121, 1);
    system_exit(-1);
  }
  return (void *)(*(int *)0x50548c + (int)idx * 0xc);
}

/* Return the runtime gain for a sound class.
 * Looks up the sound class entry by index and reads the float at offset +4. */
float sound_class_get_gain(int class_index)
{
  float *entry = (float *)sound_class_get(class_index);
  return entry[1];
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
