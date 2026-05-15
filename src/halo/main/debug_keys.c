typedef struct debug_key_definition {
  int16_t key_code;
  int16_t modifier_index;
  void (*callback)(uint32_t);
  uint8_t state_mode;
  uint8_t pad_9[3];
  char *state;
  const void *termination;
} debug_key_definition_t;

void debug_keys_initialize(void)
{
  debug_key_definition_t *definition;
  int count;
  int allocation_size;

  count = 0;
  if (*(void **)0x31f9c8 != NULL) {
    definition = (debug_key_definition_t *)0x31f9cc;
    do {
      if (definition->state != NULL) {
        *definition->state = 0;
      }
      count++;
    } while ((definition++)->termination != NULL);
  }

  allocation_size = ((count + 0x1f) >> 5) << 2;
  *(uint32_t **)0x46d928 = (uint32_t *)debug_malloc(
    allocation_size, false, "c:\\halo\\SOURCE\\main\\debug_keys.c", 0x58);
  if (*(uint32_t **)0x46d928 == NULL) {
    display_assert("global_debug_key_down",
                   "c:\\halo\\SOURCE\\main\\debug_keys.c", 0x59, true);
    system_exit(-1);
  }

  csmemset(*(uint32_t **)0x46d928, 0, allocation_size);
}

void debug_keys_dispose(void)
{
  if (*(void **)0x46d928 != NULL) {
    debug_free(*(void **)0x46d928, "c:\\halo\\SOURCE\\main\\debug_keys.c", 100);
    *(uint32_t **)0x46d928 = NULL;
  }
}

void debug_keys_update(void)
{
  debug_key_definition_t *definition;
  uint32_t *global_debug_key_down;
  uint32_t bit;
  uint32_t down_mask;
  uint8_t modifiers[4];
  bool key_69_down;
  bool key_6a_down;
  bool key_down;
  int index;

  key_69_down = input_key_is_down(0x69);
  key_6a_down = input_key_is_down(0x6a);
  modifiers[0] = !key_69_down && !key_6a_down;
  modifiers[1] = key_69_down && !key_6a_down;
  modifiers[2] = !key_69_down && key_6a_down;
  modifiers[3] = key_69_down && key_6a_down;

  index = 0;
  if (*(void **)0x31f9c8 != NULL) {
    definition = (debug_key_definition_t *)0x31f9cc;
    do {
      key_down = input_key_is_down(definition->key_code) &&
                 modifiers[(uint16_t)definition->modifier_index] != 0;

      if (definition->state_mode == 0 && definition->state != NULL) {
        *definition->state = key_down;
      }

      bit = 1 << (index & 0x1f);
      global_debug_key_down = &(*(uint32_t **)0x46d928)[index >> 5];
      down_mask = *global_debug_key_down;
      if ((down_mask & bit) == 0) {
        if (key_down) {
          *global_debug_key_down = down_mask | bit;
          if (definition->callback != NULL) {
            definition->callback(1);
          }
        }
      } else if (!key_down) {
        *global_debug_key_down = down_mask & ~bit;
        if (definition->state_mode != 0 && definition->state != NULL) {
          *definition->state = *definition->state == 0;
        }
        if (definition->callback != NULL) {
          definition->callback(0);
        }
      }

      index++;
    } while ((definition++)->termination != NULL);
  }
}

/* Set the AI debug display flag to 1 when called with a non-zero argument. */
void debug_key_erase_all_actors(char param_1)
{
  if (param_1 != '\0') {
    *(char *)0x5ac9c1 = 1;
  }
}

/* Trigger AI debug encounter display when called with a non-zero argument. */
void FUN_000ffdf0(char param_1)
{
  if (param_1 != '\0') {
    ai_debug_change_selected_encounter(1);
  }
}
