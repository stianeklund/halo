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

/* Search sound class names for pattern and set their enabled flag.
 * enable == 0 means enable (sets the disabled field to false).
 * 0x1c8a40 / sound_classes.obj
 */
void debug_sound_classes_enable(char *pattern, char enable)
{
    char **pp;
    int i;
    void *def;

    i = 0;
    pp = (char **)0x32f5d0;
    do {
        if (**pp != '\0') {
            if (crt_strstr(*pp, pattern) != NULL) {
                def = sound_class_get_definition((short)i);
                *(bool *)((char *)def + 0x28) = (enable == '\0');
            }
        }
        i++;
        pp++;
    } while ((short)i < 0x33);
}

/* Search sound class names for pattern and set their min/max distances.
 * 0x1c8a90 / sound_classes.obj
 */
void debug_sound_classes_set_distances(char *pattern, float dist1, float dist2)
{
    char **pp;
    int i;
    void *def;

    i = 0;
    pp = (char **)0x32f5d0;
    do {
        if (**pp != '\0') {
            if (crt_strstr(*pp, pattern) != NULL) {
                def = sound_class_get_definition((short)i);
                *(float *)((char *)def + 0x18) = dist1;
                def = sound_class_get_definition((short)i);
                *(float *)((char *)def + 0x1c) = dist2;
            }
        }
        i++;
        pp++;
    } while ((short)i < 0x33);
}

/* Set wet-mix gain on all sound classes matching pattern.
 * Stores (1.0 - wet), clamped to [0, 1], at class+0x10.
 * 0x1c8ae0 / sound_classes.obj
 */
void debug_sound_classes_set_wet(char *pattern, float wet)
{
    char **pp;
    int i;
    void *def;
    float val;

    i = 0;
    pp = (char **)0x32f5d0;
    do {
        if (**pp != '\0') {
            if (crt_strstr(*pp, pattern) != NULL) {
                val = 1.0f - wet;
                if (val < 0.0f)
                    val = 0.0f;
                else if (val > 1.0f)
                    val = 1.0f;
                def = sound_class_get_definition((short)i);
                *(float *)((char *)def + 0x10) = val;
            }
        }
        i++;
        pp++;
    } while ((short)i < 0x33);
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

/* Advance sound class gain interpolation by delta_ticks. */
void sound_classes_update(int delta_ticks)
{
  short i;

  if (delta_ticks > 0) {
    i = 0;
    do {
      float *entry;
      short remaining;
      int remaining_int;

      entry = (float *)sound_class_get(i);
      remaining = *(short *)(entry + 2);
      remaining_int = (int)remaining;
      if (remaining_int > delta_ticks) {
        *(short *)(entry + 2) = remaining - (short)delta_ticks;
        entry[1] =
          (entry[0] - entry[1]) * ((float)delta_ticks / (float)remaining_int) +
          entry[1];
      } else {
        entry[1] = entry[0];
        *(short *)(entry + 2) = 0;
      }
      i++;
    } while (i < 0x33);
  }
}

/* Return the runtime gain for a sound class.
 * Looks up the sound class entry by index and reads the float at offset +4. */
float sound_class_get_gain(int class_index)
{
  float *entry = (float *)sound_class_get(class_index);
  return entry[1];
}
