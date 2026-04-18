void game_engine_dispose(void)
{
  if (current_game_engine) {
    void (**vtable)(void) = (void (**)(void))current_game_engine;
    if (vtable[2])
      vtable[2]();
    current_game_engine = 0;
  }
}

void game_engine_dispose_from_old_map(void)
{
  if (current_game_engine) {
    void (**vtable)(void) = (void (**)(void))current_game_engine;
    if (vtable[4])
      vtable[4]();
  }
}

int game_engine_game_starting(void)
{
  if (current_game_engine) {
    void (**vtable)(void) = (void (**)(void))current_game_engine;
    if (vtable[7])
      vtable[7]();
    game_engine_evaluate_game_complexity();
  }
  return 0;
}

bool game_engine_running(void)
{
  return current_game_engine != 0;
}

bool game_engine_force_single_screen(void)
{
  return current_game_engine && game_engine_variant_index > 1 &&
         game_engine_variant_index < 4;
}

/* game_engine_update (0xaf370)
 *
 * The game-engine state block at 0x5aa720..0x5aa730 overlaps the KB's
 * 16-bit game_engine_variant_index declaration at 0x5aa730, so this lift
 * keeps the 32-bit phase fields on raw addresses until the wider layout is
 * established across the whole subsystem.
 */
void game_engine_update(void)
{
  data_iter_t iter;
  char *player;
  int player_handle;
  char *player_datum;
  int unit_handle;
  object_data_t *unit;
  int object_iter[4];
  void (**vtable)(void);

  if (!current_game_engine)
    return;

  ((void (*)(void))0xb2540)();
  ((void (*)(void))0xa8680)();
  ((void (*)(void))0xa8820)();
  ((void (*)(void))0xacbb0)();

  data_iterator_new(&iter, player_data);
  while ((player = (char *)data_iterator_next(&iter)) != NULL) {
    player_handle = iter.datum_handle;

    if (current_game_engine && player_handle != NONE &&
        ((*(uint32_t *)0x456b18 & 8) != 0)) {
      player_datum = (char *)datum_get(player_data, player_handle);
      unit_handle = *(int *)(player_datum + 0x34);
      if (unit_handle != NONE) {
        unit = (object_data_t *)object_get_and_verify_type(unit_handle, 3);
        unit->unk_148 = 0.0f;
        unit->unk_140 = 0.0f;
      }
    }

    if (current_game_engine) {
      vtable = (void (**)(void))current_game_engine;
      if (((*(uint32_t *)0x456b18 & 0x10) != 0 ||
           (vtable[32] &&
            ((char (*)(int, int))vtable[32])(player_handle, 1))) &&
          *(int *)((char *)datum_get(player_data, player_handle) + 0x34) !=
            NONE) {
        ((void (*)(int, int, int))0xbc410)(player_handle, 0, 0xf);
      }
    }

    ((void (*)(int))0xad600)(player_handle);

    vtable = (void (**)(void))current_game_engine;
    if (vtable[13])
      ((void (*)(int))vtable[13])(player_handle);
  }

  vtable = (void (**)(void))current_game_engine;
  if (vtable[17])
    ((void (*)(void))vtable[17])();

  switch (*(int *)0x5aa730) {
  case 0:
    if (((char (*)(void))0xabc80)()) {
      ((void (*)(void))0xa8b00)();
      return;
    }
    break;

  case 1:
    if (*(float *)0x5aa728 <= 2.0f && (*(uint32_t *)0x5aa720 & 0x10) == 0) {
      ((void (*)(const char *, float, int))0x1c8c80)((const char *)0x25386f,
                                                     0.0f, 0x1e);
      ((void (*)(const char *, float, int))0x1c8c80)("ambient_nature", 0.2f,
                                                     0x1e);
      ((void (*)(const char *, float, int))0x1c8c80)("ambient_machinery", 0.2f,
                                                     0x1e);
      ((void (*)(const char *, float, int))0x1c8c80)("ambient_computers", 0.2f,
                                                     0x1e);
      *(uint32_t *)0x5aa720 |= 0x10;
    }

    *(float *)0x5aa728 -= 1.0f / 30.0f;
    if (*(float *)0x5aa728 <= 0.0f) {
      *(float *)0x5aa72c = 0.0f;
      *(int *)0x5aa730 = 2;
      *(float *)0x5aa728 = 5.0f;

      data_iterator_new(&iter, player_data);
      while ((player = (char *)data_iterator_next(&iter)) != NULL) {
        unit_handle = *(int *)(player + 0x34);
        if (unit_handle != NONE)
          ((void (*)(int))0x1a7f80)(unit_handle);
        if (*(int16_t *)(player + 2) != NONE)
          scenario_switch_structure_bsp(*(int16_t *)(player + 2));
      }

      ((void (*)(void *, int, int))0x13d6f0)(object_iter, 2, 0);
      while (((void *(*)(void *))0x13d730)(object_iter) != NULL)
        object_delete(object_iter[2]);

      if (((void *(*)(void))0x12a1d0)() != NULL) {
        ((void (*)(int))0x12c370)((int)((void *(*)(void))0x12a1d0)());
        return;
      }
    }
    break;

  case 2:
  case 3:
    break;

  default:
    display_assert("!\"unreachable\"", "c:\\halo\\SOURCE\\game\\game_engine.c",
                   0x985, true);
    system_exit(-1);
  }
}
