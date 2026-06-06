/* hud_draw_element_wrapper (0xd3fa0)
 * Wraps FUN_000d3080 with crosshair_overlay=0, passing through remaining
 * params. */
void FUN_000d3fa0(int param_1, short *param_2, int param_3, int param_4,
                  int param_5, int param_6, int param_7, int param_8,
                  int param_9, int param_10)
{
  FUN_000d3080(param_4, param_3, param_1, 0, param_2, *(float *)&param_5,
               param_6, param_7, param_8, param_9, param_10);
}
/* hud_draw_overlay_elements (0xd4260)
 * Draw overlay bitmap elements for a HUD widget. Iterates over the overlay
 * element tag block, performs bitmap lookup with optional animation cycling,
 * optional color interpolation, and renders each visible element via
 * FUN_000d3080. Protected by a stack canary (0x200 bytes of 0x62). */
void FUN_000d4260(int param_1, int param_2, int param_3,
                  unsigned int param_4, int param_5, unsigned char param_6,
                  int param_7)
{
  int element;
  int bitmap_seq;
  int color;
  int frame_idx;
  int iVar1;
  short sVar5;
  int local_214[128];
  int local_14;
  int out_sprite;
  int out_bitmap;
  int local_4;

  local_14 = FUN_000d1540();
  csmemset(local_214, 0x62, 0x200);
  local_4 = 0;
  if (0 < *(int *)(param_3 + 0x10)) {
    do {
      element = (int)tag_block_get_element((void *)(param_3 + 0x10), local_4,
                                           0x88);
      if ((*(unsigned char *)(element + 0x4c) & 2) == 0 &&
          (param_4 & (int)*(short *)(element + 0x4a)) != 0) {
        bitmap_seq = (int)tag_block_get_element(
          (void *)((int)tag_get(0x6269746d, *(int *)(param_3 + 0xc)) + 0x54),
          (int)*(short *)(element + 0x48), 0x40);
        if ((*(unsigned char *)(element + 0x4c) & 1) == 0 ||
            (param_6 & 1) == 0) {
          color = *(int *)(element + 0x24);
        } else {
          color = FUN_000d2320(element + 0x24, param_5);
        }
        if ((*(unsigned char *)(element + 0x4c) & 1) == 0 ||
            (param_6 & 1) == 0 || *(short *)(element + 0x44) < 1) {
          frame_idx = 0;
        } else {
          frame_idx =
            ((game_time_get() - param_5) / (int)*(short *)(element + 0x44)) /
              30 %
            *(int *)(bitmap_seq + 0x34);
        }
        out_bitmap = 0;
        out_sprite = 0;
        FUN_000d16a0(*(int *)(param_3 + 0xc),
                     *(unsigned short *)(element + 0x48), frame_idx,
                     &out_bitmap, &out_sprite);
        if (out_bitmap != 0 &&
            (int)xbox_texture_cache_get_hardware_format((void *)out_bitmap, 0,
                                                        1) != 0) {
          FUN_000d3080(out_sprite, element, out_bitmap, 0, (short *)param_2,
                       1.0f, 0, color, param_7, 0, 0);
        }
      }
      local_4 = local_4 + 1;
    } while (local_4 < *(int *)(param_3 + 0x10));
  }
  sVar5 = 0x7f;
  do {
    if (local_214[(int)sVar5] != 0x62626262) goto LAB_000d43f5;
    sVar5 = sVar5 - 1;
  } while (-1 < sVar5);
  sVar5 = -1;
LAB_000d43f5:
  iVar1 = FUN_000d1540();
  if (local_14 != iVar1) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x2ec, 1);
    system_exit(-1);
  }
  if (sVar5 != -1) {
    display_assert(
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)sVar5),
      "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x2ec, 1);
    system_exit(-1);
  }
}
/* HUD message display system. */

/* hud_messaging_initialize (0xd4680)
 * Allocates the hud messaging globals buffer via game_state_malloc. */
void hud_messaging_initialize(void)
{
  *(void **)0x46bd18 = game_state_malloc("hud messaging", 0, 0x11a8);
}

/* FUN_000d46a0 (0xd46a0)
 * Sets the player globals pointer and zeroes the hud messaging buffer. */
void FUN_000d46a0(void)
{
  void *buf;
  int val;
  buf = *(void **)0x46bd18;
  val = *(int *)0x46bd0c;
  *(int *)0x5aa68c = val;
  csmemset(buf, 0, 0x11a8);
}

/* FUN_000d46d0 (0xd46d0)
 * Shared RET stub, tail-called from hud_dispose_from_old_map. Empty body. */
void FUN_000d46d0(void)
{
}

/* FUN_000d46e0 (0xd46e0)
 * Shared RET stub, tail-called from hud_dispose. Empty body. */
void FUN_000d46e0(void)
{
}

/* scripted_hud_set_state_message (0xd46f0)
 * Sets the scripted HUD message from the scenario's HMT tag. */
void scripted_hud_set_state_message(short param_1)
{
  int scenario;
  int hmt;

  scenario = (int)global_scenario_get();
  if (*(char *)(*(int *)0x46bd10 + 1) != '\0' &&
      *(int *)(scenario + 0x5a0) != -1) {
    hmt = (int)tag_get(0x686d7420, *(int *)(scenario + 0x5a0));
    *(int *)(*(int *)0x46bd18 + 0x118c) =
      (int)tag_block_get_element((void *)(hmt + 0x20), (int)param_1, 0x40);
  }
}

/* scripted_hud_set_flashing_state (0xd4740)
 * Sets the flashing state flag and records the game tick. */
void scripted_hud_set_flashing_state(char param_1)
{
  int base;

  if (param_1 != '\0' && *(char *)(*(int *)0x46bd18 + 0x1184) == '\0') {
    *(int *)(*(int *)0x46bd18 + 0x1180) = game_time_get();
    base = *(int *)0x46bd18;
    *(char *)(base + 0x1184) = param_1;
    return;
  }
  *(char *)(*(int *)0x46bd18 + 0x1184) = param_1;
}

/* scripted_hud_restart_flashing (0xd4780)
 * Resets the flashing timer if flashing is enabled. */
void scripted_hud_restart_flashing(void)
{
  if (*(char *)(*(int *)0x46bd18 + 0x1184) != '\0') {
    *(int *)(*(int *)0x46bd18 + 0x1180) = game_time_get();
    return;
  }
  error(2, "trying to restart help text flashing when flashing is disabled");
}

/* scripted_hud_set_objective (0xd47c0)
 * Sets the objective text from the HMT tag if it's text-only. */
void scripted_hud_set_objective(short param_1)
{
  int scenario;
  int hmt;
  int element;
  char *pcVar;
  int base;
  int globals;

  scenario = (int)global_scenario_get();
  if (*(int *)(scenario + 0x5a0) != -1) {
    hmt = (int)tag_get(0x686d7420, *(int *)(scenario + 0x5a0));
    element =
      (int)tag_block_get_element((void *)(hmt + 0x20), (int)param_1, 0x40);
    pcVar = (char *)tag_block_get_element(
      (void *)(hmt + 0x14), *(unsigned short *)(element + 0x22), 2);
    base = *(int *)0x46bd18;
    globals = *(int *)0x46bd0c;
    if (*(char *)(element + 0x24) == 1 && *pcVar == '\0') {
      *(int *)(*(int *)0x46bd18 + 0x1190) = element;
      *(short *)(base + 0x1194) =
        *(short *)(globals + 0x11e) + *(short *)(globals + 0x11c);
      return;
    }
    error(2, "objective text MUST only be text, no icons");
  }
}
/* scripted_hud_set_timer_warning_cutoff (0xd48e0)
 * Sets the warning cutoff time in ticks. */
void scripted_hud_set_timer_warning_cutoff(short param_1, short param_2)
{
  *(short *)(*(int *)0x46bd18 + 0x119e) = (param_1 * 60 + param_2) * 30;
}

/* scripted_hud_set_timer_position (0xd4900)
 * Sets the timer position on screen. */
void scripted_hud_set_timer_position(short param_1, short param_2,
                                     short param_3)
{
  int base;

  base = *(int *)0x46bd18;
  *(short *)(*(int *)0x46bd18 + 0x11a0) = param_1;
  *(short *)(base + 0x11a2) = param_2;
  if (param_3 < 0) {
    *(short *)(base + 0x11a4) = 0;
    return;
  }
  if (param_3 > 4) {
    *(short *)(base + 0x11a4) = 4;
    return;
  }
  *(short *)(base + 0x11a4) = param_3;
}
/* scripted_hud_pause_timer (0xd4980)
 * Pauses or unpauses the HUD timer, adjusting remaining ticks. */
void scripted_hud_pause_timer(char param_1)
{
  int base;
  short *timer_start;
  short *timer_remaining;
  short now;

  base = *(int *)0x46bd18;
  timer_start = (short *)(*(int *)0x46bd18 + 0x1198);
  timer_remaining = (short *)(*(int *)0x46bd18 + 0x119c);
  *(char *)(*(int *)0x46bd18 + 0x11a6) = param_1;
  if (*timer_remaining > 0) {
    if (param_1 != '\0') {
      now = (short)game_time_get();
      timer_remaining = (short *)(base + 0x119c);
      *timer_remaining = *timer_remaining + (*timer_start - now);
      return;
    }
    now = (short)game_time_get();
    timer_remaining = (short *)(base + 0x119c);
    *timer_remaining = *timer_remaining + (now - *timer_start);
  }
}
/* FUN_000d4a20 (0xd4a20)
 * Start or stop the loading screen timer. */
void scripted_hud_time_code_show(char param_1)
{
  if (param_1 != '\0') {
    *(int *)0x2f66e4 = game_time_get();
    *(int *)0x2f66e8 = *(int *)0x2f66e4;
    return;
  }
  *(int *)0x2f66e4 = -1;
}

/* FUN_000d4a50 (0xd4a50)
 * Pause or unpause the loading screen timer. */
void scripted_hud_time_code_start(char param_1)
{
  int now;

  if (param_1 != '\0') {
    now = game_time_get();
    *(int *)0x2f66e4 = *(int *)0x2f66e4 + (now - *(int *)0x2f66e8);
    *(int *)0x2f66e8 = -1;
    return;
  }
  *(int *)0x2f66e8 = game_time_get();
}

/* FUN_000d4a90 (0xd4a90)
 * Reset the loading timer start to current tick. */
void scripted_hud_time_code_reset(void)
{
  *(int *)0x2f66e4 = game_time_get();
  if (*(int *)0x2f66e8 != -1) {
    *(int *)0x2f66e8 = *(int *)0x2f66e4;
  }
}
/* FUN_000d4f00 (0xd4f00)
 * Toggle help text display state for a player. */
void FUN_000d4f00(short param_1, char param_2)
{
  int iVar1;
  int iVar2;

  iVar1 = param_1 * 0x460;
  iVar2 = iVar1 + *(int *)0x46bd18;
  *(unsigned char *)(iVar2 + 0x45e) =
    *(unsigned char *)(iVar1 + 0x45e + *(int *)0x46bd18) |
    (*(char *)(iVar1 + 0x458 + *(int *)0x46bd18) != param_2);
  *(char *)(iVar2 + 0x458) = param_2;
  *(int *)(iVar2 + 0x454) = 0;
  if (param_2 != '\0') {
    *(int *)(iVar2 + 0x454) = 0;
    ustrncpy((wchar_t *)(iVar2 + 0x230), (wchar_t *)0x26cdf0, 0xff);
  }
  *(char *)(iVar2 + 0x45f) = param_2;
}

/* FUN_000d4f70 (0xd4f70)
 * Set help text string for a player. */
void FUN_000d4f70(short param_1, wchar_t *param_2)
{
  int iVar1;

  iVar1 = param_1 * 0x460 + *(int *)0x46bd18;
  ustrncpy((wchar_t *)(iVar1 + 0x230), param_2, 0xff);
  *(short *)(iVar1 + 0x42e) = 0;
}

/* FUN_000d4fb0 (0xd4fb0)
 * Get the objective text string from the HMT tag. */
int FUN_000d4fb0(void)
{
  int result;
  int scenario;
  int hmt;
  int msg;
  char *element;

  result = 0;
  if (*(int *)(*(int *)0x46bd18 + 0x1190) != 0) {
    scenario = (int)global_scenario_get();
    hmt = (int)tag_get(0x686d7420, *(int *)(scenario + 0x5a0));
    msg = *(int *)(*(int *)0x46bd18 + 0x1190);
    element = (char *)tag_block_get_element((void *)(hmt + 0x14),
                                            *(unsigned short *)(msg + 0x22), 2);
    if (*(char *)(msg + 0x24) != 1) {
      display_assert("message->element_count==1",
                     "c:\\halo\\SOURCE\\interface\\hud_messaging.c", 0x2a1, 1);
      system_exit(-1);
    }
    if (*element != '\0') {
      display_assert("element->type==_hud_message_type_text",
                     "c:\\halo\\SOURCE\\interface\\hud_messaging.c", 0x2a2, 1);
      system_exit(-1);
    }
    result = (int)tag_data_get_pointer(
      (void *)hmt, (int)((unsigned int)*(unsigned short *)(msg + 0x20) << 1),
      (int)((unsigned int)(unsigned char)element[1] << 1));
  }
  return result;
}

/* FUN_000d4d90 (0xd4d90)
 * Set a HUD message element reference for a player. */
void hud_set_state_message(short param_1, short param_2)
{
  int iVar1;
  int iVar3;
  short sVar4;

  if (*(char *)(*(int *)0x46bd10 + 1) == '\0' &&
      *(int *)(*(int *)0x46bd0c + 0xfc) != -1) {
    iVar3 = (int)(short)param_1 * 0x460 + *(int *)0x46bd18;
    sVar4 = param_2;
    if (param_2 != -1) {
      iVar1 = (int)tag_get(0x686d7420, *(int *)(*(int *)0x46bd0c + 0xfc));
      if ((int)(short)param_2 < *(int *)(iVar1 + 0x20)) {
        *(int *)(iVar3 + 0x454) = (int)tag_block_get_element(
          (void *)(iVar1 + 0x20), (int)(short)param_2, 0x40);
        *(unsigned char *)(iVar3 + 0x459) = 0;
        *(unsigned char *)(iVar3 + 0x458) = (param_2 != -1);
        return;
      }
      sVar4 = -1;
    }
    *(unsigned char *)(iVar3 + 0x458) = (sVar4 != -1);
  }
}

/* FUN_000d4e30 (0xd4e30)
 * Set a numeric value for a HUD message element. */
void hud_set_state_message_icon(short param_1, short param_2, int param_3)
{
  int iVar1;

  iVar1 = param_1 * 0x460 + *(int *)0x46bd18;
  if (*(char *)(param_1 * 0x460 + 0x458 + *(int *)0x46bd18) != '\0' &&
      *(char *)(*(int *)0x46bd10 + 1) == '\0' && *(int *)(iVar1 + 0x454) != 0) {
    *(int *)(iVar1 + 0x434 + param_2 * 4) = param_3;
    *(unsigned char *)(iVar1 + 0x459) = *(unsigned char *)(iVar1 + 0x459) &
                                        ~(1 << ((unsigned char)param_2 & 0x1f));
  }
}
/* hud_messaging_slot_compare (0xd50f0)
 * qsort comparator for hud message slots. Sort order:
 * primary: display timer (int at +0), ascending (oldest first);
 * secondary: int field at +0x84;
 * tertiary: byte priority field at +0x83.
 * Confirmed: three-level comparison via Ghidra decompile. */
int hud_messaging_slot_compare(int *param_1, int *param_2)
{
  int diff;

  diff = *param_2 - *param_1;
  if (diff == 0) {
    diff = param_2[0x21] - param_1[0x21];
    if (diff == 0) {
      diff = (int)(unsigned char)((char *)param_2 + 0x83)[0] -
             (int)(unsigned char)((char *)param_1 + 0x83)[0];
    }
  }
  return diff;
}

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

/* hud_get_font_index (0xd5160)
 * Returns font index: multiplayer-specific if 2+ local players and valid,
 * otherwise falls back to the default font index. */
int hud_get_font_index(void)
{
  short count;
  int base;
  int result;

  count = local_player_count();
  base = *(int *)0x5aa68c;
  if (count <= 1 || (result = *(int *)(base + 0x64), result == -1)) {
    result = *(int *)(base + 0x54);
  }
  return result;
}

/* hud_get_text_color (0xd5180)
 * Copies 4 HUD text color words from the messaging globals into param_1[0..3].
 */
void hud_get_text_color(int *param_1)
{
  int *src;

  src = (int *)(*(int *)0x5aa68c + 0x70);
  param_1[0] = src[0];
  param_1[1] = src[1];
  param_1[2] = src[2];
  param_1[3] = src[3];
}

/* hud_messaging_globals_update (0xd51b0)
 * Resets the HUD message priority counter to 0. */
void hud_messaging_globals_update(void)
{
  *(uint8_t *)(*(char **)0x46bd18 + 0x1185) = 0;
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
  /* Find a message slot. ESI = -1 means "don't match any vehicle". */
  slot = (char *)hud_find_message_slot(base, 0, -1);
  ustrncpy((wchar_t *)(slot + 4), message, 0x3f);
  *(int *)(slot + 0x84) = -1;
  *(int *)slot = game_time_get();
  *(uint8_t *)(slot + 0x82) = 1;
  *(uint8_t *)(slot + 0x83) = *(uint8_t *)(*(char **)0x46bd18 + 0x1185);
  *(uint8_t *)(*(char **)0x46bd18 + 0x1185) += 1;
  *(uint8_t *)(base + 0x45e) = 0;
}
/* FUN_000d52e0 (0xd52e0)
 * Sends a scripted HUD message to all local players whose object is
 * on the same team as the given actor. Iterates all 4 local player
 * slots, looks up each player's object, compares team (offset 0x20)
 * with the actor's team, and calls hud_print_message for matches.
 * Only runs if a game engine is active (game_engine_running).
 *
 * Confirmed: game_engine_running at 0xa8e30; local_player_get_player_index
 * at 0xba3c0; datum_get(0x5aa6d4) for player objects; +0x20 = team field;
 * hud_print_message at 0xd51c0. */
void FUN_000d52e0(int actor_handle, wchar_t *message)
{
  char cVar1;
  int player_obj;
  int actor_obj;
  int player_index;
  int i;

  cVar1 = game_engine_running();
  if (cVar1 != '\0') {
    i = 0;
    do {
      player_index = local_player_get_player_index((int16_t)i);
      if (player_index != -1) {
        player_obj = (int)datum_get(*(data_t **)0x5aa6d4, player_index);
        actor_obj = (int)datum_get(*(data_t **)0x5aa6d4, actor_handle);
        if (*(int *)(player_obj + 0x20) == *(int *)(actor_obj + 0x20)) {
          hud_print_message((int16_t)i, message);
        }
      }
      i++;
    } while ((int16_t)i < 4);
  }
}
/* hud_get_nav_point_data (0xd5f40)
 * Returns pointer to a player's nav point data (0x30 bytes per player). */
int hud_get_nav_point_data(short param_1)
{
  if (param_1 < 0 || param_1 >= 4) {
    display_assert("local_player_index>=0&&local_player_index<MAXIMUM_NUMBER_"
                   "OF_LOCAL_PLAYERS",
                   "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x5f, 1);
    system_exit(-1);
  }
  if (*(int *)0x46bd1c == 0) {
    display_assert("nav_point_data",
                   "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x60, 1);
    system_exit(-1);
  }
  return param_1 * 0x30 + *(int *)0x46bd1c;
}

/* hud_nav_points_initialize (0xd5fb0)
 * Allocates nav point data via game_state_malloc. */
void hud_nav_points_initialize(void)
{
  *(int *)0x46bd1c = (int)game_state_malloc("hud nav points", 0, 0xc0);
  if (*(int *)0x46bd1c == 0) {
    display_assert("nav_point_data",
                   "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x6a, 1);
    system_exit(-1);
  }
}

/* hud_messaging_initialize_for_new_map: clear the messaging slot table.
 * Called from hud_initialize_for_new_map (0xd0360).
 * Fills *(void**)0x46bd1c with 0xff for 0xc0 bytes (all slots invalid). */
void hud_messaging_initialize_for_new_map(void)
{
  csmemset(*(void **)0x46bd1c, 0xff, 0xc0);
}

/* hud_messaging_dispose_from_old_map: no-op stub.
 * Called from hud_dispose_from_old_map (0xd03e0). */
void hud_messaging_dispose_from_old_map(void)
{
}

/* hud_messaging_dispose: no-op stub.
 * Called from hud_dispose (0xd0340). */
void hud_messaging_dispose(void)
{
}
/* nav_point_set_flag wrapper (0xd6120).
 * Calls FUN_000d6030 with nav_type=2 (flag). */
void FUN_000d6120(int param_1, int player_handle, short param_3, int param_4)
{
  FUN_000d6030(player_handle, (short)param_1, 2, (int)param_3, param_4);
}

/* nav_point_set_object wrapper (0xd6140).
 * Calls FUN_000d6030 with nav_type=0 (object). */
void FUN_000d6140(int param_1, int player_handle, short param_3, int param_4)
{
  FUN_000d6030(player_handle, (short)param_1, 0, (int)param_3, param_4);
}

/* nav_point_set_enemy wrapper (0xd6160).
 * Calls FUN_000d6030 with nav_type=1 (enemy). */
void FUN_000d6160(int param_1, int player_handle, int param_3, int param_4)
{
  FUN_000d6030(player_handle, (short)param_1, 1, param_3, param_4);
}

/* FUN_000d6180 (0xd6180)
 * Set nav point for all players on a matching team.
 * ABI: @ebx=nav_type, @esi=extra, @edi=object_handle, stack: type_value, team,
 * param_3 */
void FUN_000d6180(int type_value, short team, int param_3, short nav_type,
                  int extra, int object_handle)
{
  int iter[4];
  int datum;

  data_iterator_new((void *)iter, *(data_t **)0x5aa6d4);
  datum = (int)data_iterator_next((void *)iter);
  while (datum != 0) {
    if (*(short *)(datum + 2) != -1 && (int)team == *(int *)(datum + 0x20)) {
      FUN_000d6030(iter[2], (short)type_value, nav_type,
                   (int)(short)object_handle, extra);
    }
    datum = (int)data_iterator_next((void *)iter);
  }
}
/* FUN_000d6490 (0xd6490) — set object nav point for a unit's player. */
void FUN_000d6490(int param_1, int unit_handle, short param_3, int param_4)
{
  int player_index;

  player_index = player_index_from_unit_index(unit_handle);
  if (player_index != -1) {
    FUN_000d6030(player_index, (short)param_1, 0, (int)param_3, param_4);
  }
}

/* FUN_000d64c0 (0xd64c0) — set enemy nav point for a unit's player. */
void FUN_000d64c0(int param_1, int unit_handle, int param_3, int param_4)
{
  int player_index;

  player_index = player_index_from_unit_index(unit_handle);
  if (player_index != -1) {
    FUN_000d6030(player_index, (short)param_1, 1, param_3, param_4);
  }
}
/* nav_point_visibility_test (0xd6550)
 * Ray-cast from param_2 to param_3 to check if the nav point is visible. */
short FUN_000d6550(int param_1, float *param_2, float *param_3, int param_4)
{
  int player_handle;
  int player;
  int unit_handle;
  short result;
  short collision_result[28];
  float direction[3];

  if (global_current_collision_user_depth >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x1fe, 1);
    system_exit(-1);
  }
  collision_user_stack[global_current_collision_user_depth] = 0x14;
  global_current_collision_user_depth = global_current_collision_user_depth + 1;

  player_handle = local_player_get_player_index((short)param_1);
  if (player_handle == -1) {
    unit_handle = -1;
  } else {
    player_handle = local_player_get_player_index((short)param_1);
    player = (int)datum_get(*(data_t **)0x5aa6d4, player_handle);
    unit_handle = *(int *)(player + 0x34);
  }

  direction[0] = param_3[0] - param_2[0];
  direction[1] = param_3[1] - param_2[1];
  direction[2] = param_3[2] - param_2[2];

  if (FUN_0014df70(0xc2ad, param_2, direction, unit_handle, collision_result) ==
        '\0' ||
      (collision_result[0] == 3 &&
       *(int *)(collision_result + 0x14) == param_4)) {
    result = 0;
  } else {
    result = 2;
  }

  if (global_current_collision_user_depth <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x210, 1);
    system_exit(-1);
  }
  global_current_collision_user_depth = global_current_collision_user_depth - 1;
  return result;
}
/* FUN_000d7080 (0xd7080)
 * Iterate all local players and update nav point rendering. */
void FUN_000d7080(void)
{
  int result;
  short sVar;

  result = (int)local_player_get_next(-1);
  sVar = (short)result;
  while (sVar != -1) {
    FUN_000d6e50(result);
    result = (int)local_player_get_next((short)result);
    sVar = (short)result;
  }
}
/* unit_hud_get_slot (0xd7280)
 * Returns pointer to a player's unit HUD slot.
 * ABI: @esi=local_player_index */
int FUN_000d7280(short local_player_index)
{
  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert("local_player_index>=0 && "
                   "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                   "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x106, 1);
    system_exit(-1);
  }
  if (*(int *)0x46bd20 == 0) {
    display_assert("unit_hud_globals",
                   "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x107, 1);
    system_exit(-1);
  }
  return local_player_index * 0x58 + *(int *)0x46bd20;
}

/* unit_hud_initialize (0xd72f0)
 * Allocates the unit HUD interface globals buffer. */
void FUN_000d72f0(void)
{
  *(int *)0x46bd20 = (int)game_state_malloc("hud unit interface", 0, 0x164);
  if (*(int *)0x46bd20 == 0) {
    display_assert("unit_hud_globals",
                   "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x110, 1);
    system_exit(-1);
  }
}

/* FUN_000d7330 (0xd7330)
 * Initialize unit_hud_globals: clears the global buffer (0x164 bytes),
 * then for each of 4 local players sets float fields to -1.0f (0xbf800000),
 * marks int fields as -1, and fills remaining slot bytes with 0xff. */
void FUN_000d7330(void)
{
  int *slot;
  int i;
  int16_t j;

  if (*(void **)0x46bd20 == 0) {
    display_assert("unit_hud_globals",
                   "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x11b, 1);
    system_exit(-1);
  }
  csmemset(*(void **)0x46bd20, 0, 0x164);
  j = 0;
  i = 0;
  do {
    if ((j < 0) || (j >= 4)) {
      display_assert("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x106, 1);
      system_exit(-1);
    }
    if (*(void **)0x46bd20 == 0) {
      display_assert("unit_hud_globals",
                     "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x107, 1);
      system_exit(-1);
    }
    slot = (int *)(i + *(int *)0x46bd20);
    csmemset((char *)slot + 0x22, 0xff, 2);
    slot[1] = (int)0xbf800000;
    slot[0] = (int)0xbf800000;
    slot[5] = -1;
    slot[6] = -1;
    slot[2] = (int)0xbf800000;
    slot[7] = -1;
    *(int16_t *)((char *)slot + 0x24) = 0;
    csmemset((char *)slot + 0x28, 0xff, 0x30);
    j++;
    i += 0x58;
  } while (j < 4);
}

/* FUN_000d7420 (0xd7420)
 * Shared RET stub, tail-called from hud_dispose_from_old_map. Empty body. */
void FUN_000d7420(void)
{
}

/* FUN_000d7430 (0xd7430)
 * Shared RET stub, tail-called from hud_dispose. Empty body. */
void FUN_000d7430(void)
{
}

/* show_hud (0xd7440) — toggle HUD visibility flag bit 0. */
void FUN_000d7440(char param_1)
{
  if (param_1 == '\0') {
    *(unsigned int *)(*(int *)0x46bd20 + 0x160) =
      *(unsigned int *)(*(int *)0x46bd20 + 0x160) | 1;
    return;
  }
  *(unsigned int *)(*(int *)0x46bd20 + 0x160) =
    *(unsigned int *)(*(int *)0x46bd20 + 0x160) & 0xfffffffe;
}

/* show_hud_help_text (0xd7470) — toggle help text flag bit 1. */
void FUN_000d7470(char param_1)
{
  if (param_1 != '\0') {
    *(unsigned int *)(*(int *)0x46bd20 + 0x160) =
      *(unsigned int *)(*(int *)0x46bd20 + 0x160) | 2;
    return;
  }
  *(unsigned int *)(*(int *)0x46bd20 + 0x160) =
    *(unsigned int *)(*(int *)0x46bd20 + 0x160) & 0xfffffffd;
}

/* show_hud_health (0xd74a0) — toggle health display flag bit 2. */
void FUN_000d74a0(char param_1)
{
  if (param_1 == '\0') {
    *(unsigned int *)(*(int *)0x46bd20 + 0x160) =
      *(unsigned int *)(*(int *)0x46bd20 + 0x160) | 4;
    return;
  }
  *(unsigned int *)(*(int *)0x46bd20 + 0x160) =
    *(unsigned int *)(*(int *)0x46bd20 + 0x160) & 0xfffffffb;
}

/* show_hud_motion_sensor (0xd74d0) — toggle motion sensor flag bit 3. */
void FUN_000d74d0(char param_1)
{
  if (param_1 != '\0') {
    *(unsigned int *)(*(int *)0x46bd20 + 0x160) =
      *(unsigned int *)(*(int *)0x46bd20 + 0x160) | 8;
    return;
  }
  *(unsigned int *)(*(int *)0x46bd20 + 0x160) =
    *(unsigned int *)(*(int *)0x46bd20 + 0x160) & 0xfffffff7;
}

/* show_hud_crosshair (0xd7500) — toggle crosshair display flag bit 4. */
void FUN_000d7500(char param_1)
{
  if (param_1 == '\0') {
    *(unsigned int *)(*(int *)0x46bd20 + 0x160) =
      *(unsigned int *)(*(int *)0x46bd20 + 0x160) | 0x10;
    return;
  }
  *(unsigned int *)(*(int *)0x46bd20 + 0x160) =
    *(unsigned int *)(*(int *)0x46bd20 + 0x160) & 0xffffffef;
}

/* show_hud_ammo (0xd7530) — toggle ammo display flag bit 5. */
void FUN_000d7530(char param_1)
{
  if (param_1 != '\0') {
    *(unsigned int *)(*(int *)0x46bd20 + 0x160) =
      *(unsigned int *)(*(int *)0x46bd20 + 0x160) | 0x20;
    return;
  }
  *(unsigned int *)(*(int *)0x46bd20 + 0x160) =
    *(unsigned int *)(*(int *)0x46bd20 + 0x160) & 0xffffffdf;
}

/* unit_hud_update_sounds (0xd7560)
 * Build sound state flags from unit properties and update HUD sounds. */
void FUN_000d7560(int param_1, char param_2)
{
  char cVar1;
  short sVar2;
  int *pfVar3;
  int iVar4;
  int *puVar5;
  int iVar7;
  unsigned int uVar8;

  pfVar3 = (int *)FUN_000d7280(*(short *)(param_1 + 2));
  iVar4 = *(int *)(param_1 + 0x34);
  if (iVar4 == -1) {
    iVar4 = pfVar3[7];
  }
  puVar5 = (int *)object_try_and_get_and_verify_type(iVar4, 3);
  if (puVar5 != (int *)0) {
    iVar7 = (int)tag_get(0x756e6974, *puVar5);
    sVar2 = local_player_count();
    iVar7 = FUN_001a6820(iVar7, 1 < sVar2);
    if (iVar7 != -1) {
      iVar7 = (int)tag_get(0x756e6869, iVar7);
      uVar8 = 0;
      if ((*(unsigned char *)((int)puVar5 + 4) & 4) != 0 ||
          *(float *)((int)puVar5 + 0x90) <= *(float *)0x2533c0) {
        pfVar3[7] = -1;
      } else if (param_2 != '\0' &&
                 (cVar1 = cinematic_in_progress(), cVar1 == '\0')) {
        if (*(int *)pfVar3 != (int)0xbf800000) {
          iVar4 = local_player_get_player_index(*(short *)(param_1 + 2));
          cVar1 = game_engine_has_shield(iVar4);
          if (cVar1 != '\0' &&
              (*(unsigned int *)(*(int *)0x46bd20 + 0x160) & 4) == 0) {
            uVar8 =
                (*(unsigned short *)((int)puVar5 + 0xb6) & 0x1000) >> 0xc;
            if (*(float *)pfVar3 < *(float *)((int)puVar5 + 0x94)) {
              uVar8 = uVar8 | 2;
            }
            if (*(float *)((int)puVar5 + 0x94) < *(float *)0x25337c &&
                *(float *)0x2533c0 < *(float *)((int)puVar5 + 0x94)) {
              uVar8 = uVar8 | 4;
            }
            if (*(float *)((int)puVar5 + 0x94) == *(float *)0x2533c0) {
              uVar8 = uVar8 | 8;
            }
          }
        }
        if ((*(unsigned int *)(*(int *)0x46bd20 + 0x160) & 1) == 0) {
          if (*(float *)((int)puVar5 + 0x90) < *(float *)0x25337c) {
            uVar8 = uVar8 | 0x10;
          }
          if ((*(unsigned char *)((int)puVar5 + 0xb6) & 4) != 0) {
            uVar8 = uVar8 | 0x20;
          }
          if (*(float *)((int)puVar5 + 0x90) < *(float *)(pfVar3 + 1) &&
              *(float *)(pfVar3 + 1) - *(float *)((int)puVar5 + 0x90) <
                  *(float *)0x281e94) {
            uVar8 = uVar8 | 0x40;
          }
          if (*(float *)0x281e94 <=
              *(float *)(pfVar3 + 1) - *(float *)((int)puVar5 + 0x90)) {
            uVar8 = uVar8 | 0x80;
          }
        }
      }
      FUN_000d70b0(*(short *)(param_1 + 2), uVar8, (int *)(iVar7 + 0x3c0),
                   (int)(pfVar3 + 10), (unsigned short *)(pfVar3 + 9));
    }
  }
}

/* unit_hud_copy_slot (0xd7780)
 * Copy unit HUD data from old player to new player. */
void FUN_000d7780(short old_player, short new_player)
{
  int *src;
  int *dst;

  if (old_player == -1) {
    display_assert("old_local_player_index!=NONE",
                   "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x1ab, 1);
    system_exit(-1);
  }
  if (new_player == -1) {
    display_assert("new_local_player_index!=NONE",
                   "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x1ac, 1);
    system_exit(-1);
  }
  src = (int *)FUN_000d7280(old_player);
  dst = (int *)FUN_000d7280(new_player);
  memcpy(dst, src, 0x58);
}
/* FUN_000d7cd0 (0xd7cd0)
 * Subtract damage amount from a player's HUD damage indicator. */
void FUN_000d7cd0(int player_handle, float param_2)
{
  int player;
  float *pfVar2;

  player = (int)datum_get(*(data_t **)0x5aa6d4, player_handle);
  if (*(short *)(player + 2) != -1) {
    pfVar2 = (float *)FUN_000d7280(*(short *)(player + 2));
    *pfVar2 = *pfVar2 - param_2;
  }
}

/* FUN_000d7d10 (0xd7d10)
 * Iterate local players and update unit HUD for each. */
void FUN_000d7d10(void)
{
  int result;
  short sVar;

  result = (int)local_player_get_next(-1);
  sVar = (short)result;
  while (sVar != -1) {
    FUN_000d7800(sVar);
    result = (int)local_player_get_next(sVar);
    sVar = (short)result;
  }
}
