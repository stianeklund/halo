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

/* hud_draw_element (0xd3fe0)
 * Draw a HUD element with bitmap lookup, texture caching, and stack canary. */
void FUN_000d3fe0(int param_1, short *param_2, int param_3,
                  unsigned int param_4, int param_5)
{
  int iVar1;
  int iVar2;
  int *puVar3;
  int uVar4;
  char cVar5;
  short sVar6;
  int local_24c[128];
  unsigned char local_4c[16];
  int local_3c;
  int local_38;
  int local_34;
  float local_30;
  int local_2c;
  float local_28;
  unsigned int local_24;
  int local_20;
  int local_1c;
  unsigned char local_18[4];
  int local_14;
  int local_10;
  int local_c;
  short *local_8;

  local_20 = FUN_000d1540();
  csmemset(local_24c, 0x62, 0x200);
  iVar1 = verify_tag_reference((int *)(param_3 + 0x24));
  local_8 = (short *)tag_get(0x6269746d, iVar1);
  local_c = (int)FUN_00077040(*(int *)(param_3 + 0x30),
                              *(short *)(param_3 + 0x54), 0);
  iVar2 = (int)xbox_texture_cache_get_hardware_format((void *)local_c, 0, 1);
  if (iVar2 != 0) {
    verify_tag_reference((int *)(param_3 + 0x24));
    puVar3 = FUN_000d1580();
    if ((param_4 & 2) == 0) {
      if ((param_4 & 1) == 0) {
        uVar4 = *(int *)(param_3 + 0x34);
      } else {
        uVar4 = FUN_000d2320(param_3 + 0x34, param_5);
      }
    } else {
      uVar4 = *(int *)(param_3 + 0x4c);
    }
    sVar6 = *local_8;
    local_14 = (sVar6 == 4);
    FUN_000d3080((int)puVar3, (int)param_3, local_c, 0, param_2, 1.0f, 0,
                 uVar4, (param_4 >> 2) & 0xffffff01, (char)local_14, 0);
    local_8 = (short *)0;
    if (0 < *(int *)(param_3 + 0x58)) {
      iVar2 = 0;
      local_24 = param_4 & 4;
      local_34 = 0;
      local_2c = 0;
      cVar5 = (char)(sVar6 == 4);
      while (1) {
        local_1c = (int)tag_block_get_element((void *)(param_3 + 0x58), iVar2,
                                              0x1e0);
        local_30 = 1.0f;
        local_28 = 1.0f;
        if (cVar5 != '\0') {
          local_10 = (int)*(short *)(local_c + 6);
          local_30 = (float)(int)*(short *)(local_c + 4);
          local_28 = (float)local_10;
        }
        if (puVar3 == (int *)0) {
          puVar3 = &local_34;
        }
        local_3c = *(int *)(param_3 + 4);
        local_38 = *(int *)(param_3 + 8);
        if (((short)local_24 == 0) ||
            (uVar4 = 1, (*(unsigned char *)(param_3 + 0xc) & 1) != 0)) {
          uVar4 = 0;
        }
        FUN_000d1f40((short)*(int *)0x506548, param_2, (int)param_3, 0, uVar4,
                     0, local_18);
        FUN_000d1890(local_c, *param_2);
        FUN_000d27a0(param_1, local_18, puVar3, local_4c, 0, uVar4);
        iVar2 = iVar2 + 1;
        iVar2 = (int)(short)iVar2;
        if (*(int *)(param_3 + 0x58) <= iVar2) break;
        cVar5 = (char)local_14;
      }
    }
  }
  (void)local_3c; (void)local_38; (void)local_30; (void)local_2c;
  (void)local_28; (void)local_1c;
  sVar6 = 0x7f;
  do {
    if (local_24c[(int)sVar6] != 0x62626262) goto LAB_000d41e7;
    sVar6 = sVar6 - 1;
  } while (-1 < sVar6);
  sVar6 = -1;
LAB_000d41e7:
  iVar2 = FUN_000d1540();
  if (local_20 != iVar2) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x2ad, 1);
    system_exit(-1);
  }
  if (sVar6 != -1) {
    display_assert(
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)sVar6),
      "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x2ad, 1);
    system_exit(-1);
  }
}

/* hud_draw_text_element (0xd4470)
 * Draw a text element on the HUD, with optional icon rendering.
 * ABI: @esi=src_rect, @edi=dst_rect, @ebx=text, stack: param_1=use_icons */
void FUN_000d4470(char param_1, short *src_rect, short *dst_rect, void *text)
{
  short local_8[4];

  draw_string_set_indents(
    (int)(unsigned short)src_rect[1] - (int)(unsigned short)dst_rect[1], 0);
  FUN_0019cdb0(dst_rect, text, local_8, src_rect);
  src_rect[1] = src_rect[1] - 3;
  local_8[1] = dst_rect[1];
  if (param_1 != '\0') {
    if (game_engine_running() != '\0') {
      draw_string_and_hack_in_icons(local_8, 0, 0, 0, (wchar_t *)text, 1);
      *dst_rect = *src_rect;
      return;
    }
  }
  rasterizer_draw_string(local_8, 0, 0, 0, (unsigned short *)text);
  *dst_rect = *src_rect;
}

/* hud_draw_icon_sprite (0xd44f0)
 * Look up a bitmap element and draw it as a sprite.
 * ABI: @esi=element, @ebx=cursor */
void FUN_000d44f0(int cursor, short *element, int param_1, int param_2)
{
  int iVar4;
  short sVar2;
  int local_14;
  short local_10[2];
  int local_c;
  int local_8;

  iVar4 = 0;
  local_c = 0;
  local_14 = 0;
  if (*(char *)((int)element + 12) != '\0') {
    iVar4 = game_time_get();
    iVar4 = iVar4 / (int)*(char *)((int)element + 12);
  }
  FUN_000d16a0(*(int *)(*(int *)0x46bd0c + 0xb0), *element, iVar4, &local_c,
               &local_14);
  if (local_c != 0 &&
      (int)xbox_texture_cache_get_hardware_format((void *)local_c, 0, 1) != 0) {
    sVar2 = local_player_count();
    local_8 = 0x3f400000;
    if (sVar2 < 2) {
      local_8 = 0x3f800000;
    }
    local_10[0] = (short)*(int *)(cursor + 2);
    local_10[1] = (short)element[3];
    if ((*(unsigned char *)((int)element + 0xd) & 2) != 0) {
      param_2 = *(int *)(element + 4);
    }
    FUN_000d3200(local_c, 2, local_10, local_14, *(float *)&local_8, 0,
                 param_2, 0);
    if ((*(unsigned char *)((int)element + 0xd) & 4) != 0) {
      *(short *)(cursor + 2) = (short)local_10[0];
      return;
    }
    if (local_14 != 0) {
      *(short *)(cursor + 2) = (short)local_10[0];
      return;
    }
    *(short *)(cursor + 2) = (short)local_10[0];
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

/* scripted_hud_set_timer_time (0xd4860)
 * Sets the timer countdown value in ticks and records current tick. */
void scripted_hud_set_timer_time(short param_1, short param_2)
{
  int base;
  short sVar;

  base = *(int *)0x46bd18;
  *(short *)(base + 0x119c) = (param_1 * 0x3c + param_2) * 0x1e;
  *(unsigned char *)(base + 0x11a6) = 0;
  *(unsigned char *)(base + 0x11a7) = 1;
  *(int *)(base + 0x1198) = game_time_get();
  base = *(int *)0x46bd18;
  sVar = *(short *)(base + 0x11a4);
  if (sVar < 0) {
    *(short *)(base + 0x11a4) = 0;
    return;
  }
  if (4 < sVar) {
    *(short *)(base + 0x11a4) = 4;
    return;
  }
  *(short *)(base + 0x11a4) = sVar;
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

/* scripted_hud_show_timer (0xd4960)
 * Shows or hides the HUD timer. */
void scripted_hud_show_timer(unsigned char param_1)
{
  *(unsigned char *)(*(int *)0x46bd18 + 0x11a7) = param_1;
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

/* scripted_hud_get_timer_ticks (0xd49d0)
 * Returns remaining timer ticks, or 0 if hidden. */
short scripted_hud_get_timer_ticks(void)
{
  int base;
  short *timer_start;
  short result;

  base = *(int *)0x46bd18;
  timer_start = (short *)(*(int *)0x46bd18 + 0x1198);
  result = 0;
  if (*(char *)(*(int *)0x46bd18 + 0x11a7) != '\0') {
    result = *(short *)(*(int *)0x46bd18 + 0x119c);
    if (result == -1) {
      return -1;
    }
    if (*(char *)(*(int *)0x46bd18 + 0x11a6) == '\0') {
      result = (short)game_time_get();
      result = (*(short *)(base + 0x119c) + *timer_start) - result;
    }
  }
  return result;
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

/* FUN_000d4e90 (0xd4e90)
 * Set a tag reference value for a HUD message element. */
void hud_set_state_message_text(short param_1, short param_2, short param_3,
                                unsigned char param_4)
{
  int iVar1;

  iVar1 = param_1 * 0x460 + *(int *)0x46bd18;
  if (*(char *)(param_1 * 0x460 + 0x458 + *(int *)0x46bd18) != '\0' &&
      *(char *)(*(int *)0x46bd10 + 1) == '\0' && *(int *)(iVar1 + 0x454) != 0) {
    *(short *)(iVar1 + 0x434 + param_2 * 4) = param_3;
    *(unsigned char *)(iVar1 + 0x436 + param_2 * 4) = param_4;
    *(unsigned char *)(iVar1 + 0x459) = *(unsigned char *)(iVar1 + 0x459) |
                                        (1 << ((unsigned char)param_2 & 0x1f));
  }
}

/* Find a message slot in the 4-entry array at base (each 0x8c bytes).
 * Prefers: exact match (tag_handle + param2), then free slot, then oldest.
 * tag_handle passed in ESI (register arg). */
void *hud_find_message_slot(int base, int param2, int tag_handle /* @<esi> */)
{
  int16_t i;
  int16_t best_index;
  int best_time;
  void *result;
  char *entry;

  i = 0;
  best_index = 0;
  best_time = 0x7fffffff;
  result = (void *)0;

  do {
    entry = (char *)(base + (int)i * 0x8c);

    if ((tag_handle == -1 || tag_handle != *(int *)(entry + 0x84) ||
         (char)param2 != *(char *)(entry + 0x8a)) &&
        *(char *)(entry + 0x82) != 0) {
      int time_val = *(int *)entry;
      if (time_val < best_time) {
        best_time = time_val;
        best_index = i;
      }
    } else {
      result = (void *)entry;
      if (tag_handle == -1 || tag_handle == *(int *)(entry + 0x84))
        break;
    }

    i++;
  } while ((uint16_t)i < 4);

  if (result == (void *)0) {
    result = (void *)(base + (int)best_index * 0x8c);
  }
  return result;
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
  /* Find a message slot, matching vehicle tag handle via @esi. */
  slot = (char *)hud_find_message_slot(base, param_4, vehicle_tag_handle);
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

/* hud_find_nav_point_by_name (0xd5ec0)
 * Search the nav point definitions for a matching name, return its index. */
int hud_find_nav_point_by_name(const char *param_1)
{
  short found;
  short i;
  int element;
  int result;

  found = -1;
  if (*(int *)0x46bd0c != 0) {
    i = 0;
    if (*(int *)(*(int *)0x46bd0c + 0x160) > 0) {
      do {
        element = (int)tag_block_get_element((void *)(*(int *)0x46bd0c + 0x160),
                                             (int)i, 0x68);
        result = csstricmp(param_1, (const char *)element);
        if (result == 0) {
          found = i;
          if (i != -1)
            return (int)i;
          break;
        }
        i = i + 1;
      } while ((int)i < *(int *)(*(int *)0x46bd0c + 0x160));
    }
  }
  error(2, "could not find nav point");
  return (int)found;
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

/* nav_point_set: add or update a nav point entry for a player (0xd6030).
 * Searches for existing match or empty slot in the 4-entry array.
 * ABI: @eax=player_handle, stack: type_value, nav_type, object_handle, extra */
void FUN_000d6030(int player_handle, short type_value, short nav_type,
                  int object_handle, int extra)
{
  short local_player;
  int nav_data;
  short best;
  short i;
  short *entry;
  short cx;

  if (player_handle == -1)
    return;
  local_player =
    *(short *)((int)datum_get(*(data_t **)0x5aa6d4, player_handle) + 2);
  if (local_player < 0 || local_player >= 4 || object_handle == -1 ||
      type_value == -1)
    return;

  nav_data = hud_get_nav_point_data(local_player);
  best = -1;
  i = 0;
  do {
    entry = (short *)(nav_data + i * 0xc);
    cx = (short)(entry[1] << 12) >> 12;
    if (cx == nav_type && *(int *)(entry + 4) == object_handle) {
      *entry = type_value;
      *(int *)(entry + 2) = extra;
      return;
    }
    if (cx == -1) {
      best = i;
    }
    i = i + 1;
  } while (i < 4);

  if (best != -1) {
    entry = (short *)(nav_data + best * 0xc);
    *(int *)(entry + 4) = object_handle;
    *(int *)(entry + 2) = extra;
    entry[1] =
      entry[1] ^
      ((*(unsigned char *)(entry + 1) ^ (unsigned char)nav_type) & 0xf);
    *entry = type_value;
    return;
  }
  error(2, "Could not add another nav point");
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

/* FUN_000d61f0 (0xd61f0)
 * Set flag nav point for all players on a team. */
void FUN_000d61f0(int type_value, int team, short object_handle, int extra)
{
  FUN_000d6180(type_value, (short)team, extra, 2, 0, (int)object_handle);
}

/* FUN_000d6220 (0xd6220)
 * Set object nav point for all players on a team. */
void FUN_000d6220(int type_value, int team, short object_handle, int extra)
{
  FUN_000d6180(type_value, (short)team, extra, 0, 0, (int)object_handle);
}

/* FUN_000d6250 (0xd6250)
 * Set enemy nav point for all players on a team. */
void FUN_000d6250(int type_value, int team, int object_handle, int extra)
{
  FUN_000d6180(type_value, (short)team, extra, 1, 0, object_handle);
}

/* FUN_000d6280 (0xd6280)
 * Set nav points for all players.
 * ABI: @ebx=nav_type, @edi=object_handle, stack: type_value, extra */
void FUN_000d6280(int type_value, int extra, short nav_type, int object_handle)
{
  int iter[4];
  int datum;

  data_iterator_new((void *)iter, *(data_t **)0x5aa6d4);
  datum = (int)data_iterator_next((void *)iter);
  while (datum != 0) {
    if (*(short *)(datum + 2) != -1) {
      FUN_000d6030(iter[2], (short)type_value, nav_type,
                   (int)(short)object_handle, extra);
    }
    datum = (int)data_iterator_next((void *)iter);
  }
}

/* FUN_000d62f0 (0xd62f0)
 * Set flag nav points for all players. */
void FUN_000d62f0(int type_value, int object_handle, int extra)
{
  FUN_000d6280(type_value, extra, 2, object_handle);
}

/* nav_point_clear: remove a nav point entry for a player (0xd6320).
 * ABI: @eax=player_handle, @esi=nav_type, @edi=object_handle */
void FUN_000d6320(int player_handle, short nav_type, int object_handle)
{
  short local_player;
  int nav_data;
  short i;
  short *entry;
  short bx;

  if (player_handle == -1)
    return;
  local_player =
    *(short *)((int)datum_get(*(data_t **)0x5aa6d4, player_handle) + 2);
  if (local_player < 0 || local_player >= 4 || object_handle == -1)
    return;

  nav_data = hud_get_nav_point_data(local_player);
  i = 0;
  do {
    entry = (short *)(nav_data + i * 0xc);
    bx = (short)(entry[1] << 12) >> 12;
    if (bx == nav_type && *(int *)(entry + 4) == object_handle) {
      *(unsigned char *)(entry + 1) = *(unsigned char *)(entry + 1) | 0xf;
      *(int *)(entry + 4) = -1;
      *entry = -1;
      return;
    }
    i = i + 1;
  } while (i < 4);
}

/* nav_point_clear_flag wrapper (0xd6390).
 * Clears a flag nav point (type=2). */
void FUN_000d6390(int player_handle, short object_handle)
{
  FUN_000d6320(player_handle, 2, (int)object_handle);
}

/* nav_point_clear_object wrapper (0xd63b0).
 * Clears an object nav point (type=0). */
void FUN_000d63b0(int player_handle, short object_handle)
{
  FUN_000d6320(player_handle, 0, (int)object_handle);
}

/* nav_point_clear_enemy wrapper (0xd63d0).
 * Clears an enemy nav point (type=1). */
void FUN_000d63d0(int player_handle, int object_handle)
{
  FUN_000d6320(player_handle, 1, object_handle);
}

/* FUN_000d63f0 (0xd63f0)
 * Clear nav points for all players on a given team.
 * ABI: @eax=object_handle, @ecx=nav_type, @ebx=team_handle */
void FUN_000d63f0(int object_handle, short nav_type, int team_handle)
{
  int iter[4];
  int datum;

  data_iterator_new((void *)iter, *(data_t **)0x5aa6d4);
  datum = (int)data_iterator_next((void *)iter);
  while (datum != 0) {
    if (*(short *)(datum + 2) != -1 &&
        (int)(short)team_handle == *(int *)(datum + 0x20)) {
      FUN_000d6320(iter[2], nav_type, object_handle);
    }
    datum = (int)data_iterator_next((void *)iter);
  }
}

/* FUN_000d6450 (0xd6450) — clear object nav points by team. */
void FUN_000d6450(int team_handle, short object_handle)
{
  FUN_000d63f0((int)object_handle, 0, team_handle);
}

/* FUN_000d6470 (0xd6470) — clear enemy nav points by team. */
void FUN_000d6470(int team_handle, int object_handle)
{
  FUN_000d63f0(object_handle, 1, team_handle);
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

/* FUN_000d64f0 (0xd64f0) — clear object nav point for a unit's player. */
void FUN_000d64f0(int param_1, int param_2)
{
  int player_index;

  player_index = player_index_from_unit_index(param_1);
  if (player_index != -1) {
    FUN_000d6320(player_index, 0, param_2);
  }
}

/* FUN_000d6520 (0xd6520)
 * Clear enemy nav point for a unit's player. */
void FUN_000d6520(int param_1, int param_2)
{
  int player_index;

  player_index = player_index_from_unit_index(param_1);
  if (player_index != -1) {
    FUN_000d6320(player_index, 1, param_2);
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

/* nav_point_render (0xd6cc0)
 * Render nav points for a player. */
void FUN_000d6cc0(int param_1)
{
  int nav_data;
  int player;
  int unit_handle;
  unsigned short *puVar6;
  int iVar2;
  short sVar5;
  int loop_count;
  float position[3];

  if ((short)param_1 == -1)
    goto done;
  if (local_player_get_player_index((short)param_1) == -1)
    goto done;
  player = local_player_get_player_index((short)param_1);
  player = (int)datum_get(*(data_t **)0x5aa6d4, player);
  unit_handle = *(int *)(player + 0x34);
  if (unit_handle == -1)
    goto done;
  if (*(int *)(*(int *)0x46bd0c + 0x15c) == -1)
    goto done;

  nav_data = hud_get_nav_point_data((short)param_1);
  puVar6 = (unsigned short *)(nav_data + 8);
  loop_count = 4;
  do {
    if (puVar6[-4] == 0xffff || *(int *)puVar6 == -1 ||
        ((*((unsigned short *)puVar6 - 3) & 0xf) == 0xf)) {
      *(unsigned char *)((char *)puVar6 - 6) |= 0xf;
    } else {
      sVar5 = (short)(*((unsigned short *)puVar6 - 3) << 12) >> 12;
      if (sVar5 == 0) {
        iVar2 = (int)global_scenario_get();
        iVar2 = (int)tag_block_get_element((void *)(iVar2 + 0x4e4),
                                           *(int *)puVar6, 0x5c);
        position[0] = *(float *)(iVar2 + 0x24);
        position[1] = *(float *)(iVar2 + 0x28);
        position[2] = *(float *)(iVar2 + 0x2c);
      } else if (sVar5 == 1) {
        iVar2 = (int)object_try_and_get_and_verify_type(*(int *)puVar6, -1);
        if (iVar2 == 0)
          goto skip;
        FUN_0001aae0(*(int *)puVar6, position, (float *)&param_1);
      } else if (sVar5 == 2) {
        game_engine_get_goal_position((int *)position, (short)*(int *)puVar6);
      } else {
        display_assert("!\"unreachable\"",
                       "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x2d5,
                       1);
        system_exit(-1);
      }
      position[2] = position[2] + *(float *)((char *)puVar6 - 4);
      FUN_000d6660(
        param_1, position, (short)puVar6[-4],
        (short)((unsigned short)(*(unsigned char *)((char *)puVar6 - 6))
                << 8) >>
          12);
    }
  skip:
    puVar6 = puVar6 + 6;
    loop_count = loop_count - 1;
  } while (loop_count != 0);

done:
  game_engine_render_nav_points(param_1);
}

/* nav_point_update (0xd6e50)
 * Update nav point visibility flags for a player. */
void FUN_000d6e50(int param_1)
{
  int nav_data;
  int unit_handle;
  int player;
  int iVar2;
  unsigned short *puVar6;
  short sVar5;
  int loop_count;
  int local_234[128];
  float local_28[3];
  float local_1c[4];
  float target_pos[3];
  int local_18;
  short sVar;

  local_18 = FUN_000d1540();
  csmemset(local_234, 0x62, 0x200);
  nav_data = hud_get_nav_point_data((short)param_1);
  player = local_player_get_player_index((short)param_1);
  if (player == -1) {
    unit_handle = -1;
  } else {
    player = local_player_get_player_index((short)param_1);
    player = (int)datum_get(*(data_t **)0x5aa6d4, player);
    unit_handle = *(int *)(player + 0x34);
  }
  puVar6 = (unsigned short *)(nav_data + 2);
  loop_count = 4;
  do {
    int obj_handle = -1;
    if (puVar6[-1] == 0xffff || *(int *)(puVar6 + 3) == -1 ||
        (*puVar6 & 0xf) == 0xf) {
      *(unsigned char *)puVar6 = *(unsigned char *)puVar6 | 0xf;
    } else if (unit_handle != -1) {
      unit_get_head_position(unit_handle, local_28);
      sVar5 = (short)(*puVar6 << 12) >> 12;
      if (sVar5 == 0) {
        iVar2 = (int)global_scenario_get();
        iVar2 = (int)tag_block_get_element((void *)(iVar2 + 0x4e4),
                                           *(int *)(puVar6 + 3), 0x5c);
        target_pos[0] = *(float *)(iVar2 + 0x24);
        target_pos[1] = *(float *)(iVar2 + 0x28);
        target_pos[2] = *(float *)(iVar2 + 0x2c);
      } else if (sVar5 == 1) {
        iVar2 =
          (int)object_try_and_get_and_verify_type(*(int *)(puVar6 + 3), -1);
        obj_handle = *(int *)(puVar6 + 3);
        if (iVar2 == 0 || (*(unsigned char *)(iVar2 + 0xb6) & 4) != 0) {
          *(unsigned char *)puVar6 = *(unsigned char *)puVar6 | 0xf;
          puVar6[3] = 0xffff;
          puVar6[4] = 0xffff;
          puVar6[-1] = 0xffff;
          goto next;
        }
        FUN_0001aae0(obj_handle, target_pos, local_1c);
      } else if (sVar5 == 2) {
        game_engine_get_goal_position((int *)target_pos, (short)puVar6[3]);
        /* target_pos filled by callee */
      }
      target_pos[2] = target_pos[2] + *(float *)(puVar6 + 1);
      {
        char vis =
          (char)FUN_000d6550(param_1, local_28, target_pos, obj_handle);
        *puVar6 = *puVar6 ^
                  (((unsigned char)(vis << 4) ^ (unsigned char)*puVar6) & 0xf0);
      }
    }
  next:
    puVar6 = puVar6 + 6;
    loop_count = loop_count - 1;
  } while (loop_count != 0);
  sVar = 0x7f;
  do {
    if (local_234[(int)sVar] != 0x62626262)
      goto check;
    sVar = sVar - 1;
  } while (sVar >= 0);
  sVar = -1;
check:
  iVar2 = FUN_000d1540();
  if (local_18 != iVar2) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x1f0, 1);
    system_exit(-1);
  }
  if (sVar != -1) {
    display_assert(
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)sVar),
      "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x1f0, 1);
    system_exit(-1);
  }
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

/* hud_sounds_update (0xd70b0)
 * Update HUD sound effects based on state flags. */
void FUN_000d70b0(short param_1, unsigned int param_2, int *param_3,
                  int param_4, unsigned short *param_5)
{
  int iVar1;
  short sVar2;
  int *piVar3;
  int iVar6;
  unsigned char bVar5;

  iVar6 = 0;
  sVar2 = 0;
  if (0 < *param_3) {
    do {
      piVar3 = (int *)tag_block_get_element((void *)param_3, iVar6, 0x38);
      bVar5 = (unsigned char)iVar6;
      if ((param_2 & piVar3[4]) != 0) {
        if (*piVar3 == 0x6c736e64) {
          if (*(int *)(param_4 + iVar6 * 4) == -1) {
            *(int *)(param_4 + iVar6 * 4) =
              unattached_looping_sound_start(piVar3[3], -1, piVar3[5]);
          }
        } else if (*piVar3 == 0x736e6421) {
          iVar1 = *(int *)(param_4 + iVar6 * 4);
          if (iVar1 != -1) {
            if (((unsigned int)*param_5 & (1 << (bVar5 & 0x1f))) != 0)
              goto set_bit;
            if (iVar1 != -1) {
              sound_stop_impulse(iVar1);
            }
          }
          sound_impulse_start(piVar3[3], *(float *)(piVar3 + 5));
          *(int *)(param_4 + iVar6 * 4) = piVar3[3];
        } else {
          display_assert("!\"unreachable\"",
                         "c:\\halo\\SOURCE\\interface\\hud_sounds.c", 0x2f,
                         1);
          system_exit(-1);
          *param_5 = *param_5 | (unsigned short)(1 << (bVar5 & 0x1f));
          goto next;
        }
      set_bit:
        *param_5 = *param_5 | (unsigned short)(1 << (bVar5 & 0x1f));
      } else {
        iVar1 = *(int *)(param_4 + iVar6 * 4);
        if (iVar1 != -1) {
          if (*piVar3 == 0x6c736e64) {
            unattached_looping_sound_stop(iVar1);
          } else if (*piVar3 != 0x736e6421) {
            display_assert("!\"unreachable\"",
                           "c:\\halo\\SOURCE\\interface\\hud_sounds.c", 0x40,
                           1);
            system_exit(-1);
          }
          *(int *)(param_4 + iVar6 * 4) = -1;
          *param_5 = *param_5 & ~(unsigned short)(1 << (bVar5 & 0x1f));
        }
      }
    next:
      sVar2 = sVar2 + 1;
      iVar6 = (int)sVar2;
    } while (iVar6 < *param_3);
  }
}

/* unit_hud_slot_reset (0xd7240)
 * Reset a unit HUD slot to default values.
 * ABI: @esi=slot_pointer */
void FUN_000d7240(int slot)
{
  csmemset((void *)(slot + 0x22), 0xff, 2);
  *(int *)(slot + 4) = (int)0xbf800000;
  *(int *)(slot) = (int)0xbf800000;
  *(int *)(slot + 0x14) = -1;
  *(int *)(slot + 0x18) = -1;
  *(int *)(slot + 8) = (int)0xbf800000;
  *(int *)(slot + 0x1c) = -1;
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
  csmemcpy(dst, src, 0x58);
}

/* unit_hud_update_shield_health (0xd7800)
 * Track shield/health changes and manage regen timing.
 * ABI: @eax=player_handle */
void FUN_000d7800(int player_handle)
{
  char cVar1;
  int iVar2;
  int iVar6;
  short sVar7;
  float *pfVar4;
  float fVar5;
  int local_20c[128];
  int local_c;
  int local_8;

  local_c = FUN_000d1540();
  csmemset(local_20c, 0x62, 0x200);
  iVar2 = local_player_get_player_index(player_handle);
  if (iVar2 == -1) goto LAB_000d794f;
  iVar2 = local_player_get_player_index(player_handle);
  iVar2 = (int)datum_get(*(data_t **)0x5aa6d4, iVar2);
  if (*(int *)(iVar2 + 0x34) == -1) goto LAB_000d794f;
  iVar2 = (int)object_get_and_verify_type(*(int *)(iVar2 + 0x34), 3);
  pfVar4 = (float *)FUN_000d7280(*(short *)((int)player_handle + 2));
  if (pfVar4[1] == -1.0f) {
    pfVar4[1] = *(float *)(iVar2 + 0x90);
  }
  if (*pfVar4 == -1.0f) {
    *pfVar4 = *(float *)(iVar2 + 0x94);
  }
  fVar5 = *(float *)0x2533c0;
  if (*pfVar4 <= *(float *)(iVar2 + 0x94)) {
    if (*(float *)(iVar2 + 0x94) <= *pfVar4) {
      *pfVar4 = *(float *)(iVar2 + 0x94);
      if (fVar5 < pfVar4[2]) {
        local_8 = game_time_get();
        local_8 = local_8 - (int)pfVar4[3];
        goto LAB_000d7939;
      }
    } else {
      *pfVar4 = *(float *)(iVar2 + 0x94);
      pfVar4[2] = -1.0f;
    }
  } else {
    if (pfVar4[2] < *(float *)0x2533c0 ||
        *(float *)0x2533c8 < pfVar4[2]) {
      fVar5 = (float)game_time_get();
      pfVar4[3] = fVar5;
    }
    iVar6 = game_time_get();
    if (iVar6 - (int)pfVar4[3] < 0xf) {
      pfVar4[2] = 0.0f;
      goto LAB_000d794f;
    }
    *pfVar4 = *(float *)(iVar2 + 0x94);
    local_8 = game_time_get();
    local_8 = local_8 - (int)pfVar4[3];
  LAB_000d7939:
    pfVar4[2] = (float)local_8 * *(float *)0x2546a4 + pfVar4[2];
  }
  fVar5 = (float)game_time_get();
  pfVar4[3] = fVar5;
LAB_000d794f:
  cVar1 = cinematic_in_progress();
  if (cVar1 != '\0') {
    iVar2 = local_player_get_player_index(player_handle);
    if (iVar2 != -1) {
      iVar2 = (int)datum_get(*(data_t **)0x5aa6d4, iVar2);
      FUN_000d7560(iVar2, *(char *)0x46bd10);
    }
  }
  sVar7 = 0x7f;
  do {
    if (local_20c[(int)sVar7] != 0x62626262) goto LAB_000d79a8;
    sVar7 = sVar7 - 1;
  } while (-1 < sVar7);
  sVar7 = -1;
LAB_000d79a8:
  iVar2 = FUN_000d1540();
  if (local_c != iVar2) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x201, 1);
    system_exit(-1);
  }
  if (sVar7 != -1) {
    display_assert(
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)sVar7),
      "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x201, 1);
    system_exit(-1);
  }
}

/* hud_render_damage_indicators (0xd7a20)
 * Render motion sensor direction indicators for incoming damage. */
void FUN_000d7a20(int param_1)
{
  int iVar3;
  int iVar5;
  int iVar6;
  float fVar7;
  short sVar2;
  unsigned char local_18[4];
  int local_14;
  short local_10[2];
  int local_c;
  int local_8;

  if ((short)param_1 == -1) {
    return;
  }
  iVar3 = local_player_get_player_index(param_1);
  if (iVar3 == -1) {
    iVar3 = 0;
  } else {
    iVar3 = local_player_get_player_index(param_1);
    iVar3 = (int)datum_get(*(data_t **)0x5aa6d4, iVar3);
    iVar3 = *(int *)(iVar3 + 0x34);
  }
  iVar5 = (int)object_try_and_get_and_verify_type(iVar3, 3);
  if (iVar5 == 0) {
    player_effect_clear_damage_indicators(param_1);
    return;
  }
  iVar3 = *(int *)0x46bd0c;
  sVar2 = local_player_count();
  fVar7 = FUN_000d1690(1 < sVar2);
  player_effect_get_damage_indicators(param_1, local_18);
  local_14 = 4;
  iVar5 = 0;
  do {
    if (local_18[iVar5] != 0 && local_18[iVar5] < 0x1e) {
      switch (iVar5) {
      case 0:
        param_1 = 0x40490fdb;
        break;
      case 1:
        param_1 = 0x3fc90fdb;
        break;
      case 2:
        param_1 = 0;
        break;
      case 3:
        param_1 = 0x4096cbe4;
        break;
      default:
        display_assert("!\"unreachable\"",
                       "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x400, 1);
        system_exit(-1);
      }
      iVar6 = *(int *)(iVar3 + 0x344);
      sVar2 = local_player_count();
      if (sVar2 < 2) {
        sVar2 = *(short *)(iVar3 + 0x348);
      } else {
        sVar2 = *(short *)(iVar3 + 0x34a);
      }
      local_8 = 0;
      local_c = 0;
      FUN_000d16a0(iVar6, sVar2, 0, &local_8, &local_c);
      if (local_8 != 0 &&
          (int)xbox_texture_cache_get_hardware_format((void *)local_8, 0, 1) !=
              0) {
        local_10[0] = (short)fVar7;
        local_10[1] = (short)fVar7;
        FUN_000d3200(local_8, 4, local_10, local_c, fVar7,
                     *(float *)&param_1, *(int *)(iVar3 + 0x34c), 0);
      }
    }
    iVar5 = iVar5 + 1;
    local_14 = local_14 - 1;
  } while (local_14 != 0);
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
