/* HUD message display system. */

/* Clear all scripted HUD message slots across all 4 players x 4 slots. */
_BYTE *scripted_hud_messages_clear(void)
{
  char *base = *(char **)0x46bd18 + 0x82;
  int player, slot;

  for (player = 0; player < 4; player++) {
    char *p = base;
    for (slot = 0; slot < 4; slot++) {
      *p = 0;
      p += 0x8c;
    }
    base += 0x460;
  }
  return 0;
}

/* Display a message on a player's HUD. Finds an empty message slot,
 * copies the wide string, and initializes the display timer. */
void hud_print_message(__int16 player, wchar_t *message)
{
  int base;
  char *slot;

  if (player == -1)
    return;

  base = (int)player * 0x460 + *(int *)0x46bd18;
  /* 0xd5070 takes ESI as a register arg (vehicle tag handle to match).
   * For plain text messages, ESI = -1 means "don't match any vehicle". */
  {
    int _eax;
    int args[2];
    args[0] = base;
    args[1] = 0;
    __asm__ __volatile__("pushl 4(%[a])\n\t"
                         "pushl 0(%[a])\n\t"
                         "call *%[fn]\n\t"
                         "addl $8, %%esp"
                         : "=a"(_eax)
                         : [a] "r"(args), [fn] "r"((void *)0xd5070),
                           "S"(-1)
                         : "ecx", "edx", "memory", "cc");
    slot = (char *)_eax;
  }
  ((void (*)(void *, void *, int))0x19dc90)(slot + 4, message, 0x3f);
  *(int *)(slot + 0x84) = -1;
  *(int *)slot = ((int (*)(void))0xb5aa0)();
  *(uint8_t *)(slot + 0x82) = 1;
  *(uint8_t *)(slot + 0x83) = *(uint8_t *)(*(char **)0x46bd18 + 0x1185);
  *(uint8_t *)(*(char **)0x46bd18 + 0x1185) += 1;
  *(uint8_t *)(base + 0x45e) = 0;
}

/* Set a vehicle notification on a player's HUD. Called when a player
 * enters a vehicle or changes seat. Finds a message slot via 0xd5070,
 * stores the vehicle tag handle and seat info, and initializes the
 * display timer. param_3 accumulates into the slot's counter at +0x88
 * if the slot was already active; otherwise the counter is reset first. */
void hud_messaging_set_vehicle_notification(int16_t local_player_index,
                                            int vehicle_tag_handle,
                                            int16_t param_3, int param_4)
{
  int base;
  char *slot;

  if (local_player_index == -1)
    return;

  base = (int)local_player_index * 0x460 + *(int *)0x46bd18;
  /* 0xd5070 takes ESI = vehicle_tag_handle as a register arg, plus
   * two stack args (base, param_4). Use inline asm to set ESI. */
  {
    int _eax;
    int args[2];
    args[0] = base;
    args[1] = param_4;
    __asm__ __volatile__("pushl 4(%[a])\n\t"
                         "pushl 0(%[a])\n\t"
                         "call *%[fn]\n\t"
                         "addl $8, %%esp"
                         : "=a"(_eax)
                         : [a] "r"(args), [fn] "r"((void *)0xd5070),
                           "S"(vehicle_tag_handle)
                         : "ecx", "edx", "memory", "cc");
    slot = (char *)_eax;
  }
  if (*(uint8_t *)(slot + 0x82) == 0) {
    *(int16_t *)(slot + 0x88) = 0;
  }
  *(int16_t *)(slot + 0x88) += param_3;
  *(int *)(slot + 0x84) = vehicle_tag_handle;
  *(uint8_t *)(slot + 0x8a) = (uint8_t)param_4;
  *(int *)slot = game_time_get();
  *(uint8_t *)(slot + 0x82) = 1;
  *(uint8_t *)(slot + 0x83) = *(uint8_t *)(*(char **)0x46bd18 + 0x1185);
  *(uint8_t *)(*(char **)0x46bd18 + 0x1185) += 1;
  *(uint8_t *)(base + 0x45e) = 0;
}
