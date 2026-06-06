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

  (void)local_28;
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
  int tick;
  short sVar;

  base = *(int *)0x46bd18;
  *(short *)(base + 0x119c) = (param_1 * 0x3c + param_2) * 0x1e;
  *(unsigned char *)(base + 0x11a6) = 0;
  *(unsigned char *)(base + 0x11a7) = 1;
  tick = game_time_get();
  base = *(int *)0x46bd18;
  *(int *)(base + 0x1198) = tick;
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
  *(short *)(base + 0x11a0) = param_1;
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
  short now;

  base = *(int *)0x46bd18;
  *(char *)(base + 0x11a6) = param_1;
  if (*(short *)(base + 0x119c) > 0) {
    if (param_1 != '\0') {
      now = (short)game_time_get();
      *(short *)(base + 0x119c) += *(short *)(base + 0x1198) - now;
      return;
    }
    now = (short)game_time_get();
    *(short *)(base + 0x119c) += now - *(short *)(base + 0x1198);
  }
}

/* scripted_hud_get_timer_ticks (0xd49d0)
 * Returns remaining timer ticks, or 0 if hidden. */
short scripted_hud_get_timer_ticks(void)
{
  int base;
  short result;

  base = *(int *)0x46bd18;
  result = 0;
  if (*(char *)(base + 0x11a7) != '\0') {
    result = *(short *)(base + 0x119c);
    if (result == -1) {
      return -1;
    }
    if (*(char *)(base + 0x11a6) == '\0') {
      result = (short)game_time_get();
      result = (*(short *)(base + 0x119c) + *(short *)(base + 0x1198)) - result;
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
  int tick;

  tick = game_time_get();
  *(int *)0x2f66e4 = tick;
  if (*(int *)0x2f66e8 != -1) {
    *(int *)0x2f66e8 = tick;
  }
}


/* hud_render_timer (0xd4ab0) */
void hud_render_timer(void)
{
  int iVar10;
  int *piVar7;
  int iVar6;
  int iVar8;
  int *puVar9;
  int *puVar11;
  short uVar1;
  short sVar2;
  short local_90;
  int local_8e[8];
  short local_8e_tail;
  int local_68;
  float local_64;
  float local_60;
  int local_48[8];
  char local_28;
  char local_27;
  char local_26;
  char pad_21;
  double local_14;
  int local_10;
  int local_c;
  int local_8;
  int local_4;
  int loading_time;

  iVar10 = *(int *)0x46bd18;
  piVar7 = (int *)(iVar10 + 0x1198);
  if (*(char *)(iVar10 + 0x11a7) != '\0') {
    local_90 = *(short *)(iVar10 + 0x11a4);
    csmemset(local_8e, 0, 34);
    local_8 = game_time_get();
    iVar8 = 0;
    local_c = 0;
    local_10 = (int)scripted_hud_get_timer_ticks();
    local_68 = *(int *)(iVar10 + 0x11a0);
    *(volatile char *)&local_28 = 2;
    *(volatile char *)&local_26 = 4;
    *(volatile char *)&local_27 = 1;
    *(volatile float *)&local_64 = 1.0f;
    *(volatile float *)&local_60 = 1.0f;
    (void)local_8e_tail;
    (void)pad_21;
    iVar6 = interface_get_tag_index(0xb);
    if (iVar6 != -1) {
      iVar6 = (int)tag_get(0x68756423, iVar6);
      local_c = (int)*(char *)(iVar6 + 0x11);
      iVar8 = (int)((double)local_c * 2.0);
      local_c = iVar8;
    }
    switch (*(short *)(iVar10 + 0x11a4)) {
    case 0:
    case 2:
      break;
    case 1:
    case 3:
      *(short *)&local_68 = (short)(*(short *)&local_68 + (short)iVar8 * 5);
      local_c = -iVar8;
      break;
    case 4:
      *(short *)&local_68 = (short)(*(short *)&local_68 + (short)iVar8 * -3);
      break;
    default:
      display_assert("!\"unreachable\"",
                     "c:\\halo\\SOURCE\\interface\\hud_messaging.c", 0x1f8, 1);
      system_exit(-1);
    }
    iVar6 = 8;
    if ((short)local_10 > 0) {
      uVar1 = (short)local_10;
      puVar9 = (int *)(*(int *)0x46bd0c + 0x360);
      puVar11 = local_48;
      for (; iVar6 != 0; iVar6 = iVar6 - 1) {
        *puVar11 = *puVar9;
        puVar9 = puVar9 + 1;
        puVar11 = puVar11 + 1;
      }
      sVar2 = *(short *)(iVar10 + 0x119e);
      iVar6 = local_c;
      if ((short)uVar1 <= sVar2) {
        iVar6 = 1;
        if (sVar2 < *(short *)(iVar10 + 0x119c)) {
          *(short *)(iVar10 + 0x119c) = sVar2;
          *piVar7 = ((int)sVar2 - (int)(short)uVar1) + local_8;
        }
      }
    } else {
      iVar8 = *piVar7;
      *(short *)(iVar10 + 0x119c) = (short)-1;
      puVar9 = (int *)(*(int *)0x46bd0c + 0x380);
      puVar11 = local_48;
      for (; iVar6 != 0; iVar6 = iVar6 - 1) {
        *puVar11 = *puVar9;
        puVar9 = puVar9 + 1;
        puVar11 = puVar11 + 1;
      }
      local_c = 1;
      if (iVar8 == -1) {
        *piVar7 = game_time_get();
      }
      uVar1 = (short)local_10;
      iVar6 = local_c;
    }
    iVar10 = (int)(short)(uVar1 & ((short)uVar1 < 1) - 1);
    local_8 = iVar10;
    local_4 = iVar10;
    iVar8 = (int)((float)local_4 * *(float *)0x2546a4 * *(float *)0x25634c);
    FUN_000d3860((short)*(int *)0x506548, &local_90, &local_68,
                 iVar8, -1, iVar6, *piVar7, 2.0f);
    local_4 = (int)(short)local_c;
    local_14 = (double)local_4 * *(double *)0x281b40;
    local_4 = (int)*(short *)&local_68;
    *(short *)&local_68 = (short)(int)((double)local_4 + local_14);
    iVar8 = (iVar10 / 30) % 60;
    FUN_000d3860((short)*(int *)0x506548, &local_90, &local_68,
                 iVar8, -1, iVar6, *piVar7, 2.0f);
    local_4 = (int)*(short *)&local_68;
    *(short *)&local_68 = (short)(int)((double)local_4 + local_14);
    iVar8 = ((iVar10 % 1800) * 100) / 30;
    FUN_000d3860((short)*(int *)0x506548, &local_90, &local_68,
                 iVar8, -1, iVar6, *piVar7, 2.0f);
  }
  if (*(int *)0x2f66e4 != -1) {
    loading_time = *(int *)0x2f66e8;
    if (loading_time == -1) {
      loading_time = game_time_get();
    }
    crt_sprintf((char *)0x5ab100, (const char *)0x25acb8,
                loading_time - *(int *)0x2f66e4);
    FUN_00189c40(1, (const char *)0x5ab100);
  }
}

/* FUN_000d4f00 (0xd4f00)
 * Toggle help text display state for a player. */
void FUN_000d4f00(short param_1, char param_2)
{
  int base;

  base = param_1 * 0x460 + *(int *)0x46bd18;
  *(unsigned char *)(base + 0x45e) =
      *(unsigned char *)(base + 0x45e) | (*(char *)(base + 0x458) != param_2);
  *(char *)(base + 0x458) = param_2;
  *(int *)(base + 0x454) = 0;
  if (param_2 != '\0') {
    *(int *)(base + 0x454) = 0;
    ustrncpy((wchar_t *)(base + 0x230), (wchar_t *)0x26cdf0, 0xff);
  }
  *(char *)(base + 0x45f) = param_2;
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
  if (*(char *)(iVar1 + 0x458) != '\0' &&
      *(char *)(*(int *)0x46bd10 + 1) == '\0' && *(int *)(iVar1 + 0x454) != 0) {
    *(int *)(iVar1 + 0x434 + param_2 * 4) = param_3;
    *(unsigned char *)(iVar1 + 0x459) &= (unsigned char)~(1 << (unsigned char)param_2);
  }
}

/* FUN_000d4e90 (0xd4e90)
 * Set a tag reference value for a HUD message element. */
void hud_set_state_message_text(short param_1, short param_2, short param_3,
                                unsigned char param_4)
{
  int iVar1;

  iVar1 = param_1 * 0x460 + *(int *)0x46bd18;
  if (*(char *)(iVar1 + 0x458) != '\0' &&
      *(char *)(*(int *)0x46bd10 + 1) == '\0' && *(int *)(iVar1 + 0x454) != 0) {
    *(short *)(iVar1 + 0x434 + param_2 * 4) = param_3;
    *(unsigned char *)(iVar1 + 0x436 + param_2 * 4) = param_4;
    *(unsigned char *)(iVar1 + 0x459) |= (unsigned char)(1 << (unsigned char)param_2);
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

/* FUN_000d5350 (0xd5350)
 * Main HUD messaging renderer. Draws objectives, state messages, and custom
 * text elements for a single local player. Handles three message sources:
 * objective messages, state messages, and per-slot queued messages (vehicle
 * notifications, pickups, etc). */
void FUN_000d5350(int param_1)
{
  char cVar1;
  short sVar2;
  int uVar7;
  int font_tag_data;
  char *pcVar9;
  int *piVar10;
  int iVar12;
  short *psVar13;
  short *local_38_p;
  int iVar16;
  int slot_base;
  int hmt_tag;
  int message_ptr;
  int game_ticks;
  unsigned int font_height;
  int max_slots;
  int font_index;
  short sVar15;
  short local_64;
  int local_62_dw;
  float color[4];
  uint32_t packed_color;
  int local_30_dw;
  int local_2c;
  char show_objective;
  char show_state;
  char show_custom;
  char is_splitscreen;
  short rect_a[4];
  short rect_b[4];
  int msg_slot_idx;
  unsigned char prefs[24];
  short bounds_a[4];
  short bounds_b[4];
  short bounds_c[4];
  float icon_color[4];
  wchar_t format_buf[256];

  cVar1 = cinematic_in_progress();
  if (cVar1 != '\0') {
    return;
  }
  if ((short)param_1 == -1) {
    return;
  }
  uVar7 = local_player_get_player_index((short)param_1);
  cVar1 = FUN_000ab230(uVar7);
  if (cVar1 == '\0') {
    return;
  }

  sVar2 = local_player_count();
  if (sVar2 < 2 || (font_index = *(int *)(*(int *)0x5aa68c + 0x64), font_index == -1)) {
    font_index = *(int *)(*(int *)0x5aa68c + 0x54);
  }
  sVar2 = local_player_count();
  is_splitscreen = 1 < sVar2;

  {
    short sVar3;
    sVar3 = local_player_count();
    FUN_000d1f40((short)param_1, (short *)(*(int *)0x5aa68c + 0x24),
                 *(int *)0x5aa68c, 0, 1 < sVar3, 0, &local_64);
  }
  local_62_dw = *(int *)((char *)&local_64 + 2);

  font_tag_data = (int)tag_get(0x666f6e74, font_index);
  if (is_splitscreen) {
    font_height = (unsigned int)(unsigned short)(
        *(short *)(font_tag_data + 8) + *(short *)(font_tag_data + 4));
    local_62_dw = local_62_dw - *(int *)0x2f66ec;
  } else {
    font_height = (unsigned int)(unsigned short)(
        *(short *)(font_tag_data + 8) + *(short *)(font_tag_data + 6) +
        *(short *)(font_tag_data + 4));
  }
  sVar15 = (short)local_62_dw;

  slot_base = (int)(short)*(int *)0x506548 * 0x460 + *(int *)0x46bd18;
  {
    short sVar3;
    sVar3 = local_player_count();
    max_slots = 4 - (int)(1 < sVar3);
  }

  if (*(int *)(*(int *)0x46bd18 + 0x1190) == 0 ||
      (show_objective = '\x01', *(short *)(*(int *)0x46bd18 + 0x1194) == 0)) {
    show_objective = '\0';
  }
  if (*(char *)(*(int *)0x46bd10 + 1) == '\0' ||
      (show_state = '\x01', *(int *)(*(int *)0x46bd18 + 0x118c) == 0)) {
    show_state = '\0';
  }
  if (*(char *)(slot_base + 0x458) == '\0' ||
      (*(int *)(slot_base + 0x454) == 0 && *(short *)(slot_base + 0x230) == 0)) {
    show_custom = '\0';
  } else {
    show_custom = '\x01';
  }

  if (show_objective != '\0' || show_state != '\0' || show_custom != '\0') {
    local_38_p = (short *)(slot_base + 0x230);
    input_abstraction_get_local_player_preferences((short)param_1, prefs);
    cVar1 = show_objective;
    iVar16 = *(int *)0x46bd0c;

    if (show_objective == '\0') {
      if (show_state == '\0') {
        color[0] = *(float *)(*(int *)0x5aa68c + 0x70);
        color[1] = *(float *)(*(int *)0x5aa68c + 0x74);
        color[2] = *(float *)(*(int *)0x5aa68c + 0x78);
        color[3] = *(float *)(*(int *)0x5aa68c + 0x7c);
        packed_color = FUN_000d1c90(color);
      } else {
        if (*(char *)(*(int *)0x46bd18 + 0x1184) == '\0') {
          if ((*(unsigned char *)(*(int *)0x46bd0c + 0xe2) & 1) == 0) {
            packed_color = *(uint32_t *)(*(int *)0x46bd0c + 0xd0);
          } else {
            packed_color = *(uint32_t *)(*(int *)0x46bd0c + 0xd4);
          }
        } else {
          packed_color = (uint32_t)FUN_000d2320(*(int *)0x46bd0c + 0xd0,
              *(int *)(*(int *)0x46bd18 + 0x1180));
        }
        pixel32_to_real_argb_color(packed_color, color);
      }
    } else {
      iVar12 = *(int *)0x46bd0c + 0x100;
      iVar16 = game_time_get();
      packed_color = (uint32_t)FUN_000d2320(iVar12,
          iVar16 + (((int)*(short *)(*(int *)0x46bd18 + 0x1194) -
                     (int)*(short *)(iVar12 + 0x1c)) -
                    (int)*(short *)(iVar12 + 0x1e)));
      pixel32_to_real_argb_color(packed_color, color);
      local_2c = (int)*(short *)(iVar12 + 0x1e);
      {
        float fade;
        fade = (float)(int)*(short *)(*(int *)0x46bd18 + 0x1194) / (float)local_2c;
        if (1.0f < fade) {
          fade = 1.0f;
        }
        color[0] = fade * color[0];
      }
      packed_color = FUN_000d1c90(color);
    }

    rect_a[3] = *(short *)0x50658a - *(short *)0x50657e;
    rect_a[1] = local_64;
    rect_a[0] = sVar15;
    if (*(char *)(*(int *)0x46bd10 + 1) != '\0') {
      rect_a[0] = sVar15 + (short)font_height * 4;
    }
    rect_a[2] = (short)font_height + rect_a[0];
    *(int *)&rect_b[0] = *(int *)&rect_a[0];
    *(int *)&rect_b[2] = *(int *)&rect_a[2];

    draw_string_set_font(font_index, -1, 0, 0, color);

    if (cVar1 == '\0') {
      if (show_state != '\0') {
        iVar16 = (int)global_scenario_get();
        hmt_tag = (int)tag_get(0x686d7420, *(int *)(iVar16 + 0x5a0));
        message_ptr = *(int *)(*(int *)0x46bd18 + 0x118c);
        goto LAB_000d57ad;
      }
      if (*(int *)(slot_base + 0x454) != 0) {
        if (show_custom == '\0') {
          display_assert("show_state_message",
              "c:\\halo\\SOURCE\\interface\\hud_messaging.c", 0x42a, 1);
          system_exit(-1);
        }
        hmt_tag = (int)tag_get(0x686d7420, *(int *)(*(int *)0x46bd0c + 0xfc));
        message_ptr = *(int *)(slot_base + 0x454);
        goto LAB_000d57ad;
      }
      if (*local_38_p != 0) {
        FUN_000d4470(1, rect_b, rect_a, (void *)local_38_p);
      }
    } else {
      if (*(int *)(*(int *)0x46bd18 + 0x1190) == 0 ||
          *(short *)(*(int *)0x46bd18 + 0x1194) == 0) {
        display_assert(
            "hud_messaging_globals->objective.message && hud_messaging_globals->objective.uptime",
            "c:\\halo\\SOURCE\\interface\\hud_messaging.c", 0x41e, 1);
        system_exit(-1);
      }
      {
        short elapsed;
        int remaining;
        psVar13 = (short *)(*(int *)0x46bd18 + 0x1194);
        elapsed = game_time_get_elapsed();
        remaining = (int)*psVar13 - (int)elapsed;
        if (remaining <= 0) {
          elapsed = 0;
        } else {
          psVar13 = (short *)(*(int *)0x46bd18 + 0x1194);
          elapsed = game_time_get_elapsed();
          elapsed = *psVar13 - elapsed;
        }
        *psVar13 = elapsed;
      }
      iVar16 = (int)global_scenario_get();
      hmt_tag = (int)tag_get(0x686d7420, *(int *)(iVar16 + 0x5a0));
      message_ptr = *(int *)(*(int *)0x46bd18 + 0x1190);

LAB_000d57ad:
      local_30_dw = (int)(unsigned short)*(unsigned short *)(message_ptr + 0x20);
      local_2c = 0;
      if (*(unsigned char *)(message_ptr + 0x24) != 0) {
        int tag_block_base;
        iVar16 = 0;
        tag_block_base = hmt_tag + 0x14;
        psVar13 = local_38_p;
        do {
          pcVar9 = (char *)tag_block_get_element(
              (void *)tag_block_base,
              (int)(unsigned short)*(unsigned short *)(message_ptr + 0x22) + iVar16,
              2);
          if (*pcVar9 == '\0') {
            uVar7 = (int)tag_data_get_pointer((void *)hmt_tag,
                ((unsigned int)(unsigned short)local_30_dw) << 1,
                (unsigned int)(unsigned char)pcVar9[1] << 1);
            draw_string_set_indents(
                *(int *)((char *)rect_b + 2) - *(int *)((char *)rect_a + 2), 0);
            FUN_0019cdb0(rect_a, (void *)uVar7, bounds_a, rect_b);
            rect_b[3] = rect_b[3] - 3;
            bounds_a[1] = rect_a[1];
            rasterizer_draw_string(bounds_a, 0, 0, 0, (unsigned short *)uVar7);
            rect_a[0] = rect_b[0];
            local_30_dw = local_30_dw +
                (int)(unsigned short)(unsigned char)pcVar9[1];
          } else if (*pcVar9 == '\x01') {
            unsigned char bVar1;
            int icon_idx;
            bVar1 = (unsigned char)pcVar9[1];
            icon_idx = -1;
            if (bVar1 <= 0x11) {
              icon_idx = (int)(unsigned short)bVar1;
            } else if (bVar1 <= 0x1f) {
              if (bVar1 <= 0x1c) {
                icon_idx = (int)(unsigned short)
                    prefs[(int)(signed char)((char *)0x2f66c2)[bVar1]];
              } else {
                icon_idx = (int)(short)(signed char)((char *)0x2f66c2)[bVar1];
              }
            } else {
              if (*(char *)(*(int *)0x46bd10 + 1) == '\0') {
                short custom_si;
                int ci;
                custom_si = (short)((unsigned short)bVar1 - 0x20);
                if (custom_si >= 8) {
                  display_assert("custom_index<NUMBER_OF_HUD_CUSTOM_ICONS",
                      "c:\\halo\\SOURCE\\interface\\hud_messaging.c", 0x455, 1);
                  system_exit(-1);
                }
                ci = (int)custom_si;
                if (((unsigned char)(1 << ((unsigned char)custom_si & 0x1f)) &
                     *(unsigned char *)((char *)psVar13 + 0x229)) == 0) {
                  if (*(int *)((char *)psVar13 + ci * 4 + 0x204) == 0) {
                    error(2, "help message using old code. get latest code and tags.");
                  } else {
                    FUN_000d44f0((int)rect_b,
                        (short *)(*(int *)((char *)psVar13 + ci * 4 + 0x204)),
                        (int)rect_a, (int)packed_color);
                  }
                } else {
                  short icon_ref;
                  wchar_t *icon_text;
                  icon_ref = *(short *)((char *)psVar13 + ci * 4 + 0x204);
                  if (icon_ref == -1) {
                    icon_text = 0;
                  } else if (*(char *)((char *)psVar13 + ci * 4 + 0x206) == '\0') {
                    icon_text = 0;
                    FUN_0019d420(*(int *)(*(int *)0x46bd0c + 0xc0),
                        (int)(unsigned short)icon_ref);
                  } else {
                    int scenario;
                    scenario = (int)global_scenario_get();
                    if (*(int *)(scenario + 0x580) == -1) {
                      display_assert(
                          "global_scenario_get()->custom_object_names.index!=NONE",
                          "c:\\halo\\SOURCE\\interface\\hud_messaging.c", 0x45e, 1);
                      system_exit(-1);
                    }
                    icon_text = 0;
                    scenario = (int)global_scenario_get();
                    FUN_0019d420(*(int *)(scenario + 0x580),
                        (int)(unsigned short)icon_ref);
                  }
                  FUN_000d4470((char)(int)(void *)icon_text,
                      rect_b, rect_a, (void *)psVar13);
                  psVar13 = local_38_p;
                }
                goto LAB_000d5b78;
              }
              error(2, "help text cannot use custom icons");
            }
            if ((int)(short)icon_idx < *(int *)(*(int *)0x46bd0c + 0xc4)) {
              short remapped;
              int icon_element;
              remapped = remap_sticks_for_local_player((short)icon_idx, param_1);
              icon_element = (int)tag_block_get_element(
                  (void *)(*(int *)0x46bd0c + 0xc4), (int)remapped, 0x10);
              if ((*(unsigned char *)(icon_element + 0xd) & 1) == 0) {
                FUN_000d44f0((int)rect_b, (short *)icon_element,
                    (int)rect_a, (int)packed_color);
              } else {
                if ((*(unsigned char *)(icon_element + 0xd) & 2) != 0) {
                  pixel32_to_real_argb_color(packed_color, icon_color);
                  draw_string_set_font(font_index, -1, 0, 0, icon_color);
                }
                uVar7 = (int)FUN_0019d420(*(int *)(*(int *)0x46bd0c + 0xc0),
                    (int)*(unsigned short *)(icon_element + 0xe));
                draw_string_set_indents(
                    *(int *)((char *)rect_b + 2) - *(int *)((char *)rect_a + 2), 0);
                FUN_0019cdb0(rect_a, (void *)uVar7, bounds_b, rect_b);
                rect_b[3] = rect_b[3] - 3;
                bounds_b[1] = rect_a[1];
                rasterizer_draw_string(bounds_b, 0, 0, 0, (unsigned short *)uVar7);
                rect_a[0] = rect_b[0];
                draw_string_set_font(font_index, -1, 0, 0, color);
              }
            } else {
              draw_string_set_indents(
                  *(int *)((char *)rect_b + 2) - *(int *)((char *)rect_a + 2), 0);
              FUN_0019cdb0(rect_a, (void *)L"<no button icon>", bounds_c, rect_b);
              rect_b[3] = rect_b[3] - 3;
              bounds_c[1] = rect_a[1];
              rasterizer_draw_string(bounds_c, 0, 0, 0,
                  (unsigned short *)L"<no button icon>");
              rect_a[0] = rect_b[0];
            }
          } else {
            display_assert("!\"unreachable\"",
                "c:\\halo\\SOURCE\\interface\\hud_messaging.c", 0x4a5, 1);
            system_exit(-1);
          }
LAB_000d5b78:
          local_2c = local_2c + 1;
          iVar16 = (int)(short)local_2c;
        } while (iVar16 < (int)(unsigned int)*(unsigned char *)(message_ptr + 0x24));
      }
    }
    draw_string_set_indents(0, 0);
    iVar16 = *(int *)&rect_b[2];
    if (show_objective != '\0' || (cVar1 = is_splitscreen, show_state != '\0')) {
      goto LAB_000d5c20;
    }
  }

  if (*(char *)(slot_base + 0x458) != '\0' || *(char *)(slot_base + 0x45e) != '\0') {
    if (cVar1 != '\0') {
      local_62_dw = (int)(short)font_height;
    }
    {
      float ftmp;
      if (is_splitscreen) {
        int split_y;
        split_y = (int)(short)local_62_dw - *(int *)0x2f66ec;
        ftmp = (float)split_y +
               (float)(int)(short)font_height * *(float *)(*(int *)0x5aa68c + 0x90);
      } else {
        ftmp = (float)(int)(short)font_height * *(float *)(*(int *)0x5aa68c + 0x90) +
               (float)(int)(short)local_62_dw;
      }
      iVar16 = (int)ftmp;
    }
    max_slots = max_slots - 1;
  }

LAB_000d5c20:
  qsort((void *)slot_base, 4, 0x8c,
      (int (__cdecl *)(const void *, const void *))hud_messaging_slot_compare);
  msg_slot_idx = 0;
  if (0 < (short)max_slots) {
    do {
      int *piVar14;
      int slot_offset;
      float fade_alpha;
      float uptime_limit;
      piVar14 = (int *)((short)msg_slot_idx * 0x8c + slot_base);
      if (*(char *)((short)msg_slot_idx * 0x8c + 0x82 + slot_base) == '\0') {
        return;
      }
      game_ticks = game_time_get();
      color[0] = *(float *)(*(int *)0x5aa68c + 0x80);
      color[1] = *(float *)(*(int *)0x5aa68c + 0x84);
      color[2] = *(float *)(*(int *)0x5aa68c + 0x88);
      color[3] = *(float *)(*(int *)0x5aa68c + 0x8c);
      slot_offset = game_ticks - *piVar14;
      uptime_limit = *(float *)(*(int *)0x5aa68c + 0x68) * 30.0f;
      if (uptime_limit < (float)slot_offset) {
        fade_alpha = 1.0f -
            ((float)slot_offset - uptime_limit) /
            (*(float *)(*(int *)0x5aa68c + 0x6c) * 30.0f);
        if (fade_alpha < 0.0f) {
          fade_alpha = 0.0f;
        } else if (1.0f < fade_alpha) {
          fade_alpha = 1.0f;
        }
        color[0] = (float)pow((double)fade_alpha, 1.9) * color[0];
      }
      rect_b[3] = *(short *)0x50658a - *(short *)0x50657e;
      rect_b[1] = local_64;
      rect_b[0] = (short)iVar16;
      rect_b[2] = (short)font_height + rect_b[0];
      {
        float ftmp2;
        ftmp2 = (float)(int)(short)font_height *
                *(float *)(*(int *)0x5aa68c + 0x90) +
                (float)(int)(short)iVar16;
        iVar16 = (int)ftmp2;
      }
      draw_string_set_font(font_index, -1, 0, 0, color);
      if (piVar14[0x21] == -1) {
        piVar10 = piVar14 + 1;
        goto LAB_000d5e5d;
      } else {
        char item_variant;
        int item_tag;
        item_variant = *(char *)((int)piVar14 + 0x8a);
        if (item_variant == (char)-1) {
          item_variant = (char)(1 < (short)piVar14[0x22]);
        }
        item_tag = (int)tag_get(0x6974656d, piVar14[0x21]);
        piVar10 = (int *)hud_get_item_string(
            (int)(short)(*(short *)(item_tag + 0x180) + (short)item_variant));
        if ((*(char *)((int)piVar14 + 0x8a) != (char)-1 || item_variant == '\0') &&
            (short)piVar14[0x22] == 0) {
          goto LAB_000d5e5d;
        }
        {
          short max_count;
          max_count = *(short *)(item_tag + 0x188);
          if (max_count < 2) {
            max_count = 1;
          }
          usprintf(format_buf, (const wchar_t *)piVar10,
              (int)(short)piVar14[0x22] / (int)max_count);
          rasterizer_draw_string(rect_b, 0, 0, 0, (unsigned short *)format_buf);
        }
      }
      goto LAB_000d5e65_skip;
LAB_000d5e5d:
      rasterizer_draw_string(rect_b, 0, 0, 0, (unsigned short *)piVar10);
LAB_000d5e65_skip:
      slot_offset = game_ticks - *piVar14;
      {
        char alive;
        alive = (char)((float)slot_offset <
            (*(float *)(*(int *)0x5aa68c + 0x6c) +
             *(float *)(*(int *)0x5aa68c + 0x68)) * 30.0f);
        *(char *)((int)piVar14 + 0x82) = alive;
        if (!alive) {
          *piVar14 = -1;
        }
      }
      msg_slot_idx = msg_slot_idx + 1;
    } while ((short)msg_slot_idx < (short)max_slots);
  }
  return;
}
/* hud_find_nav_point_by_name (0xd5ec0)
 * Search the nav point definitions for a matching name, return its index. */
short hud_find_nav_point_by_name(const char *param_1)
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
            return i;
          break;
        }
        i = i + 1;
      } while ((int)i < *(int *)(*(int *)0x46bd0c + 0x160));
    }
  }
  error(2, "could not find nav point");
  return found;
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

/* nav_point_draw_single (0xd6660)
 * Draw a single nav point indicator with distance/angle calculations. */
void FUN_000d6660(int param_1, float *param_2, short param_3, short param_4)
{
  float fVar1;
  float fVar2;
  float fVar3;
  float *pfVar4;
  char cVar5;
  short sVar7;
  int iVar8;
  int iVar9;
  unsigned int uVar10;
  int uVar11;
  unsigned char bVar12;
  unsigned int uVar13;
  int local_2ac[128];
  int local_34;
  float local_30;
  float local_2c;
  float local_28;
  float local_24 = 0;
  float local_20 = 0;
  float local_1c = 0;
  float local_18;
  float local_14;
  float local_10;
  float local_c = 0;
  float local_8;

  (void)local_28;
  local_34 = FUN_000d1540();
  csmemset(local_2ac, 0x62, 0x200);
  iVar8 = (int)tag_block_get_element(
    (void *)(*(int *)0x46bd0c + 0x160), (int)param_3, 0x68);
  pfVar4 = param_2;
  local_30 = *param_2;
  local_2c = param_2[1];
  local_28 = param_2[2];
  iVar9 = local_player_get_player_index(param_1);
  if (iVar9 == -1) {
    uVar11 = -1;
  } else {
    uVar11 = local_player_get_player_index(param_1);
    iVar9 = (int)datum_get(*(data_t **)0x5aa6d4, uVar11);
    uVar11 = *(int *)(iVar9 + 0x34);
  }
  unit_set_seat_state(uVar11, &local_24);
  local_18 = sqrtf((*pfVar4 - local_24) * (*pfVar4 - local_24) +
                  (pfVar4[1] - local_20) * (pfVar4[1] - local_20) +
                  (pfVar4[2] - local_1c) * (pfVar4[2] - local_1c));
  if (local_18 <= *(float *)0x254cc0) {
    local_14 = *(float *)0x2533c8;
  } else {
    local_14 = 0.5f;
  }
  matrix_transform_point((float *)0x5065b4, &local_30, &local_30);
  sVar7 = param_4;
  if (sVar7 == 1 ||
      (cVar5 = render_camera_view_to_screen((int *)0x506550, (int *)0x5065a4, &local_30,
                            &local_10),
       cVar5 == '\0')) {
    local_10 = local_30;
    local_c = -local_2c;
    sVar7 = 1;
  } else {
    local_10 = local_10 - (float)(((int)*(short *)0x506582 -
                                   (int)*(short *)0x50657e) /
                                      2 +
                                  (int)*(short *)0x50657e);
    local_c = local_c - (float)(((int)*(short *)0x506580 -
                                 (int)*(short *)0x50657c) /
                                    2 +
                                (int)*(short *)0x50657c);
  }
  local_8 = 0.0f;
  fVar1 =
      ((float)((int)*(short *)0x506588 + 2 - (int)*(short *)0x506584 + 2) -
       (*(float *)(*(int *)0x46bd0c + 300) +
        *(float *)(*(int *)0x46bd0c + 0x128))) *
      *(float *)0x253398;
  fVar3 = ((float)((int)*(short *)0x506588 - (int)*(short *)0x506584) -
           (*(float *)(*(int *)0x46bd0c + 0x124) +
            *(float *)(*(int *)0x46bd0c + 0x120))) *
          *(float *)0x253398;
  fVar2 = fVar3 * fVar1;
  fVar1 = fVar1 * local_c;
  if (sVar7 == 1 ||
      fVar2 * fVar2 <= fVar3 * local_10 * (fVar3 * local_10) + fVar1 * fVar1) {
    sVar7 = 1;
    fVar1 = sqrtf((fVar2 * fVar2) /
                 (fVar3 * local_10 * (fVar3 * local_10) + fVar1 * fVar1));
    local_10 = local_10 * fVar1;
    local_c = fVar1 * local_c;
    if ((*(unsigned char *)(iVar8 + 0x4c) & 1) == 0) {
      local_8 = -(float)atan2((double)local_10, (double)local_c);
    }
  }
  local_10 =
      (float)(((int)*(short *)0x506582 - (int)*(short *)0x50657e) / 2) +
      local_10;
  local_c = (float)(((int)*(short *)0x506580 - (int)*(short *)0x50657c) / 2) +
            local_c;
  if (sVar7 == -1) {
    display_assert(
        "waypoint_type!=NONE",
        "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x267, 1);
    system_exit(-1);
  }
  iVar9 = 0;
  uVar11 = 0;
  FUN_000d16a0(*(int *)(*(int *)0x46bd0c + 0x15c),
               *(short *)(iVar8 + 0x34 + sVar7 * 2), 0, &iVar9, &uVar11);
  if (iVar9 != 0 &&
      (int)xbox_texture_cache_get_hardware_format((void *)iVar9, 0, 1) != 0) {
    local_10 = (float)(int)local_10;
    local_c = (float)(int)local_c;
    bVar12 = (unsigned char)FUN_000d1c50(*(float *)(iVar8 + 0x2c) * 255.0f);
    pixel32_to_real_argb_color(*(unsigned int *)(iVar8 + 0x28), &local_24);
    fVar1 = *(float *)0x2533c8 - *(float *)(iVar8 + 0x30);
    fVar2 = *(float *)0x2533c0;
    if (*(float *)0x2533c0 <= fVar1) {
      fVar2 = fVar1;
      if (*(float *)0x2533c8 < fVar1) {
        fVar2 = *(float *)0x2533c8;
      }
    }
    local_24 = fVar2 * local_24;
    fVar2 = *(float *)0x2533c0;
    if (*(float *)0x2533c0 <= fVar1) {
      fVar2 = fVar1;
      if (*(float *)0x2533c8 < fVar1) {
        fVar2 = *(float *)0x2533c8;
      }
    }
    local_20 = fVar2 * local_20;
    fVar2 = *(float *)0x2533c0;
    if (*(float *)0x2533c0 <= fVar1) {
      fVar2 = fVar1;
      if (*(float *)0x2533c8 < fVar1) {
        fVar2 = *(float *)0x2533c8;
      }
    }
    local_1c = fVar2 * local_1c;
    uVar13 = (unsigned int)bVar12 << 0x18;
    uVar10 = FUN_000d1dd0(&local_24);
    FUN_000d3200(iVar9, 4, &local_10, uVar11, local_14, local_8,
                 uVar10 | uVar13, 0);
  }
  sVar7 = 0x7f;
  do {
    if (local_2ac[(int)sVar7] != 0x62626262) goto LAB_000d6c45;
    sVar7 = sVar7 - 1;
  } while (-1 < sVar7);
  sVar7 = -1;
LAB_000d6c45:
  iVar8 = FUN_000d1540();
  if (local_34 != iVar8) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x2a3, 1);
    system_exit(-1);
  }
  if (sVar7 != -1) {
    display_assert(
        csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)sVar7),
        "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x2a3, 1);
    system_exit(-1);
  }
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
  int obj_handle;
  char vis;

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
    obj_handle = -1;
    if (puVar6[-1] == 0xffff || *(int *)(puVar6 + 3) == -1 ||
        (*puVar6 & 0xf) == 0xf) {
      *(unsigned char *)puVar6 = *(unsigned char *)puVar6 | 0xf;
    } else if (unit_handle != -1) {
      unit_get_head_position(unit_handle, local_28);
      sVar5 = (short)(*puVar6 << 12) >> 12;
      switch (sVar5) {
      case 0:
        iVar2 = (int)global_scenario_get();
        iVar2 = (int)tag_block_get_element((void *)(iVar2 + 0x4e4),
                                           *(int *)(puVar6 + 3), 0x5c);
        target_pos[0] = *(float *)(iVar2 + 0x24);
        target_pos[1] = *(float *)(iVar2 + 0x28);
        target_pos[2] = *(float *)(iVar2 + 0x2c);
        break;
      case 1:
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
        break;
      case 2:
        game_engine_get_goal_position((int *)target_pos, (unsigned short)puVar6[3]);
        break;
      }
      target_pos[2] = target_pos[2] + *(float *)(puVar6 + 1);
      {
        vis = (char)FUN_000d6550(param_1, local_28, target_pos, obj_handle);
        {
          int shifted = (int)vis << 4;
          *puVar6 = *puVar6 ^ (unsigned short)(((unsigned char)shifted ^ (unsigned char)*puVar6) & 0xf0);
        }
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
  memcpy(dst, src, 0x58);
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

/* FUN_000d7d40 (0xd7d40)
 * Full HUD unit render for a single player. Draws shield meters,
 * health bars, damage indicators, motion tracker, and overlay widgets.
 * Uses a 0x200-byte stack canary (0x62 fill) with post-check.
 * Source: c:\halo\SOURCE\interface\hud_unit.c line 0x209. */
void FUN_000d7d40(int param_1)
{
  int canary_buf[128];
  int handle_slots[18];
  int tag_indices[18];
  float fraction_slots[4];
  unsigned int full_shield;
  unsigned int damage_active;
  unsigned int slot_count;
  unsigned int local_player_idx;
  int player_index;
  int canary_cookie;
  int *unit_ptr;
  int unit_data;
  int unhi_tag;
  int parent_handle;
  int health_meter_data[26];
  int widget_meter_data[26];
  short local_130[2];
  int overlay_colors[6];
  unsigned int flags;
  int iVar7, iVar8, iVar13;
  int flash_param_int;
  int clamp_a, clamp_b;
  short sVar4, sVar5;
  char cVar3;
  unsigned char bVar;
  float fVar1, fVar2, fVar14;
  float local_44;
  float local_34;
  float local_48;
  float meter_scale;
  short *psVar12;
  short *psVar22;
  int tag_ref_result;
  int *meter_src_ptr;
  int i;
  void *local_78_buf[1];
  float *pfVar6;
  unsigned char *unit_tag_data;

  canary_cookie = FUN_000d1540();
  csmemset(canary_buf, 0x62, 0x200);

  if (*(short *)(param_1 + 2) != *(short *)0x506548) {
    display_assert(
        "player->local_player_index==render.local_player_index",
        "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x209, 1);
    system_exit(-1);
  }

  if (*(short *)(param_1 + 2) != *(short *)0x506548)
    goto done_canary_check;
  if (*(int *)(param_1 + 0x34) == -1)
    goto done_canary_check;

  unit_ptr = (int *)object_get_and_verify_type(*(int *)(param_1 + 0x34), 3);
  unit_tag_data = (unsigned char *)tag_get(0x756e6974, *unit_ptr);
  local_player_idx = (unsigned int)(unsigned short)*(short *)(param_1 + 2);
  player_index = local_player_get_player_index(local_player_idx);
  pfVar6 = (float *)FUN_000d7280((short)local_player_idx);

  handle_slots[0] = *(int *)(param_1 + 0x34);
  for (i = 0; i < 17; i++) {
    handle_slots[1 + i] = 0;
  }

  sVar4 = local_player_count();
  tag_indices[0] = FUN_001a6820((int)unit_tag_data, 1 < sVar4);

  for (i = 0; i < 17; i++) {
    tag_indices[1 + i] = 0;
  }

  slot_count = 1;

  if (*(int *)((char *)pfVar6 + 0x1c) == (int)0xFFFFFFFF) {
    FUN_000d7280((short)local_player_idx);
    FUN_000d7240((short)local_player_idx);
  }

  *(int *)((char *)pfVar6 + 0x1c) = *(int *)(param_1 + 0x34);

  parent_handle = unit_ptr[0x33];
  fraction_slots[0] = *(float *)&parent_handle;

  if (parent_handle != -1 && *(short *)((char *)unit_ptr + 0x2a0) != -1) {
    int *vehicle_ptr;
    int vehicle_tag;
    int seat_element;

    vehicle_ptr = (int *)object_get_and_verify_type(parent_handle, 3);
    vehicle_tag = (int)tag_get(0x756e6974, *vehicle_ptr);
    seat_element = (int)tag_block_get_element(
        (void *)(vehicle_tag + 0x2e4),
        (int)*(short *)((char *)unit_ptr + 0x2a0), 0x11c);
    unit_tag_data = (unsigned char *)seat_element;

    FUN_000d7280((short)local_player_idx);
    sVar4 = local_player_count();
    iVar8 = FUN_001a6820(vehicle_tag, 1 < sVar4);

    if ((*unit_tag_data & 4) != 0) {
      if (iVar8 != -1) {
        handle_slots[1] = parent_handle;
        tag_indices[1] = iVar8;
        slot_count = 2;
      }

      iVar13 = vehicle_ptr[0x32];
      while (iVar13 != -1 && slot_count < 0x12) {
        unsigned char *next_obj_data;
        int next_unit;

        next_obj_data = (unsigned char *)object_get_and_verify_type(
            iVar13, (int)0xFFFFFFFF);
        next_unit =
            (int)object_try_and_get_and_verify_type(iVar13, 3);
        if (next_unit != 0 &&
            (float)*(int *)(next_unit + 0xcc) == fraction_slots[0] &&
            *(short *)(next_unit + 0x2a0) != -1) {
          handle_slots[slot_count] = iVar13;
          sVar4 = local_player_count();
          iVar13 = FUN_001a6870(
              vehicle_tag,
              *(unsigned short *)(next_unit + 0x2a0), 1 < sVar4);
          tag_indices[slot_count] = iVar13;
          slot_count = slot_count + 1;
        }
        iVar13 = *(int *)(next_obj_data + 0xc4);
      }
    }
  }

  {
    int shield_frac_raw = *(int *)((char *)unit_ptr + 0x2f0);
    full_shield = (unsigned int)(shield_frac_raw == 0x3f800000);

    if ((unit_ptr[0x6d] & 0x80000) != 0 ||
        *(float *)((char *)unit_ptr + 0x2f4) >= *(float *)0x2549d4) {
      damage_active = 0;
    } else if ((*(unsigned char *)((char *)unit_ptr + 0x1b8) & 0x10) != 0) {
      damage_active = 1;
    } else {
      damage_active = 0;
    }

    fraction_slots[0] = *(float *)((char *)unit_ptr + 0x2f4);
  }

  {
    unsigned int uVar16 = slot_count;
    unsigned int uVar18;

    while (uVar16 != 0) {
      uVar18 = uVar16 - 1;

      iVar13 = (int)object_try_and_get_and_verify_type(
          handle_slots[uVar16 - 1], 3);
      unit_data = iVar13;

      uVar16 = uVar18;
      if (iVar13 == 0 || tag_indices[uVar18] == -1)
        goto next_slot;

      iVar7 = (int)tag_get(0x756e6869, tag_indices[uVar18]);
      unhi_tag = iVar7;

      if (*(int *)(iVar7 + 0x54) != -1) {
        bVar = (unsigned char)((*(unsigned char *)(iVar13 + 0xb6)) >> 1) & 2;
        sVar4 = local_player_count();
        if (1 < sVar4) {
          bVar = bVar | 4;
        }
        FUN_000d3fe0(local_player_idx, (short *)iVar7, iVar7 + 0x24,
                     (unsigned int)bVar, (int)0xFFFFFFFF);
      }

      cVar3 = game_engine_has_shield(player_index);
      iVar8 = unit_data;
      {
        int iVar21 = unhi_tag;

        if (cVar3 != '\0' &&
            (iVar8 = iVar13, iVar21 = iVar7,
             (*(unsigned int *)(*(int *)0x46bd20 + 0x160) & 4) == 0)) {

          if (*(float *)(iVar13 + 0x94) < *(float *)0x25337c ||
              (*(unsigned int *)(*(int *)0x46bd20 + 0x160) & 8) != 0) {
            flags = 1;
          } else {
            flags = 0;
          }

          if ((*(unsigned char *)(unit_data + 0xb6) & 4) != 0) {
            flags = flags | 2;
          }

          sVar4 = local_player_count();
          if (1 < sVar4) {
            flags = flags | 4;
          }

          if (uVar18 == 0) {
            if ((flags & 1) == 0) {
              *(int *)((char *)pfVar6 + 0x10) = (int)0xFFFFFFFF;
            } else if (*(int *)((char *)pfVar6 + 0x10) == (int)0xFFFFFFFF) {
              *(int *)((char *)pfVar6 + 0x10) = game_time_get();
            }
          }

          if (*(int *)(iVar7 + 0x124) != -1) {
            int layer_idx;
            int saved_layer_idx;
            int meter_max;
            int *color_ptr;

            game_engine_running();

            if (uVar18 == 0) {
              local_44 = pfVar6[0];
            } else {
              local_44 = *(float *)(unit_data + 0x94);
            }

            meter_max =
                (int)(unsigned short)*(unsigned short *)(unhi_tag + 0x13e);
            if (meter_max == 0) {
              meter_max = 0xff;
            }

            meter_src_ptr = (int *)(unhi_tag + 0xf4);
            {
              int *src = (int *)(unhi_tag + 0xf4);
              int *dst = widget_meter_data;
              int cnt;
              for (cnt = 0x1a; cnt != 0; cnt--) {
                *dst = *src;
                src++;
                dst++;
              }
            }

            overlay_colors[0] = 0;
            overlay_colors[1] = 0xff0000;
            overlay_colors[2] = 0xff00;
            overlay_colors[3] = 0xffff00;
            overlay_colors[4] = 0x7f00ff;
            color_ptr = overlay_colors;

            layer_idx = 0;
            fVar14 = *(float *)&flags;

            if (0 <= *(int *)0x2f66f0) {
              do {
                saved_layer_idx = layer_idx;

                local_34 =
                    *(float *)(unit_data + 0x94) - (float)layer_idx;
                if (local_34 < *(float *)0x2533c0) {
                  local_34 = 0.0f;
                } else if (local_34 > *(float *)0x2533c8) {
                  local_34 = 1.0f;
                }

                fVar14 = local_44 - (float)layer_idx;
                fVar1 = *(float *)0x2533c0;
                if (*(float *)0x2533c0 <= fVar14) {
                  fVar1 = fVar14;
                  if (*(float *)0x2533c8 < fVar14) {
                    fVar1 = *(float *)0x2533c8;
                  }
                }

                fVar2 = local_34;
                if (local_34 < fVar1) {
                  fVar2 = fVar1;
                }

                if ((local_34 < *(float *)0x2533c0 !=
                     (local_34 == *(float *)0x2533c0)) &&
                    (fVar14 = *(float *)&flags,
                     fVar2 < *(float *)0x2533c0 !=
                         (fVar2 == *(float *)0x2533c0)))
                  break;

                if (fVar1 <= local_34) {
                  flash_param_int = (int)0xbf800000;
                } else {
                  flash_param_int = *(int *)((char *)pfVar6 + 0x08);
                }

                local_48 = (float)(short)meter_max * fVar2;
                clamp_a = (int)local_48;
                if (clamp_a < 0) {
                  clamp_a = 0;
                } else {
                  if ((int)local_48 > 0xff) {
                    clamp_a = 0xff;
                  } else {
                    clamp_a = (int)local_48;
                  }
                }

                {
                  float local_38_v =
                      (float)(short)meter_max * local_34;
                  clamp_b = (int)local_38_v;
                  if (clamp_b < 0) {
                    clamp_b = 0;
                  } else {
                    if ((int)local_38_v > 0xff) {
                      clamp_b = 0xff;
                    } else {
                      clamp_b = (int)local_38_v;
                    }
                  }
                }

                {
                  int *meter_data_ptr;
                  if (layer_idx == 0) {
                    meter_data_ptr = meter_src_ptr;
                  } else {
                    meter_data_ptr = widget_meter_data;
                  }

                  FUN_000d3340(local_player_idx, unhi_tag,
                               (int)meter_data_ptr, clamp_b, clamp_a,
                               flags, flash_param_int, local_34);
                }

                color_ptr = color_ptr + 1;
                layer_idx = saved_layer_idx + 1;
                fVar14 = *(float *)&flags;
              } while (saved_layer_idx + 1 <= *(int *)0x2f66f0);
            }
          }

          iVar8 = unit_data;
          iVar21 = unhi_tag;

          if (*(int *)(unhi_tag + 0xbc) != -1) {
            FUN_000d3fe0(local_player_idx, (short *)unhi_tag,
                         unhi_tag + 0x8c, (unsigned int)fVar14,
                         *(int *)((char *)pfVar6 + 0x10));
            iVar8 = unit_data;
            iVar21 = unhi_tag;
          }
        }

        if ((*(unsigned int *)(*(int *)0x46bd20 + 0x160) & 1) == 0) {
          unsigned short health_status_bits;

          health_status_bits = *(unsigned short *)(iVar8 + 0xb6);
          if ((health_status_bits & 8) == 0 &&
              (*(unsigned int *)(*(int *)0x46bd20 + 0x160) & 2) == 0) {
            flags = 0;
          } else {
            flags = 1;
          }
          if ((health_status_bits & 4) != 0) {
            flags = flags | 2;
          }
          sVar4 = local_player_count();
          if (1 < sVar4) {
            flags = flags | 4;
          }

          if (uVar18 == 0) {
            if ((flags & 1) == 0) {
              *(int *)((char *)pfVar6 + 0x14) = (int)0xFFFFFFFF;
            } else if (*(int *)((char *)pfVar6 + 0x14) ==
                       (int)0xFFFFFFFF) {
              *(int *)((char *)pfVar6 + 0x14) = game_time_get();
            }
          }

          iVar13 = unhi_tag;
          if (*(int *)(iVar21 + 0x214) != -1) {
            short health_max;
            int h_cnt;
            int health_alpha;
            int health_flash_alpha;

            health_max = *(short *)(iVar21 + 0x22e);
            if (health_max == 0) {
              health_max = 8;
            }

            {
              int *h_src = (int *)(unhi_tag + 0x1e4);
              int *h_dst = health_meter_data;
              for (iVar8 = unit_data, h_cnt = 0x1a; h_cnt != 0;
                   h_cnt--) {
                *h_dst = *h_src;
                h_src++;
                h_dst++;
              }
            }

            if (*(float *)(unit_data + 0x90) <
                *(float *)(iVar13 + 0x250)) {
              if (*(float *)(unit_data + 0x90) <
                      *(float *)(iVar13 + 0x254) ||
                  *(float *)(unit_data + 0x90) ==
                      *(float *)(iVar13 + 0x254)) {
                health_meter_data[15] = health_meter_data[14];
              } else {
                health_meter_data[15] = *(int *)(iVar13 + 0x24c);
              }
            }
            health_meter_data[14] = health_meter_data[15];

            meter_scale = (float)(int)health_max;

            iVar7 = FUN_000d1c50(
                meter_scale * *(float *)(unit_data + 0x90));
            if (iVar7 < 0) {
              health_alpha = 0;
            } else {
              iVar7 = FUN_000d1c50(
                  meter_scale * *(float *)(iVar8 + 0x90));
              if (iVar7 < 0x100) {
                health_alpha = FUN_000d1c50(
                    meter_scale * *(float *)(iVar8 + 0x90));
              } else {
                health_alpha = 0xff;
              }
            }

            iVar7 = FUN_000d1c50(
                meter_scale * *(float *)(iVar8 + 0x90));
            if (iVar7 < 0) {
              health_flash_alpha = 0;
            } else {
              iVar7 = FUN_000d1c50(
                  meter_scale * *(float *)(iVar8 + 0x90));
              if (iVar7 < 0x100) {
                health_flash_alpha = FUN_000d1c50(
                    meter_scale * *(float *)(iVar8 + 0x90));
              } else {
                health_flash_alpha = 0xff;
              }
            }

            FUN_000d3340(local_player_idx, iVar13,
                         (int)health_meter_data, health_flash_alpha,
                         health_alpha, flags, (int)0xbf800000,
                         *(float *)(iVar8 + 0x90));
            iVar21 = unhi_tag;
          }

          if (*(int *)(iVar21 + 0x1ac) != -1) {
            FUN_000d3fe0(local_player_idx, (short *)iVar21,
                         iVar21 + 0x17c, flags,
                         *(int *)((char *)pfVar6 + 0x14));
          }

          pfVar6[1] = *(float *)(unit_data + 0x90);
        }

        if (uVar18 == 0 &&
            (*(unsigned char *)(*(int *)0x46bd20 + 0x160) & 0x10) == 0 &&
            FUN_000a9f90(player_index) != '\0') {

          local_130[0] = 2;

          sVar4 = local_player_count();
          bVar = (unsigned char)((sVar4 < 2) - 1) & 4;
          if ((*(unsigned char *)(*(int *)0x46bd20 + 0x160) & 0x20) !=
              0) {
            bVar = bVar | 1;
          }

          if ((bVar & 1) == 0) {
            *(int *)((char *)pfVar6 + 0x18) = (int)0xFFFFFFFF;
          } else if (*(int *)((char *)pfVar6 + 0x18) ==
                     (int)0xFFFFFFFF) {
            *(int *)((char *)pfVar6 + 0x18) = game_time_get();
          }

          iVar13 = unhi_tag;
          if (*(int *)(unhi_tag + 0x29c) != -1) {
            FUN_000d3fe0(local_player_idx, local_130,
                         unhi_tag + 0x26c, (unsigned int)bVar,
                         (int)0xFFFFFFFF);
          }
          if (*(int *)(iVar13 + 0x304) != -1) {
            FUN_000d3fe0(local_player_idx, local_130,
                         iVar13 + 0x2d4, (unsigned int)bVar,
                         (int)0xFFFFFFFF);
          }

          sVar4 = local_player_count();
          FUN_000d1f40((short)local_player_idx, local_130,
                       iVar13 + 0x35c, 0, 1 < sVar4, 0,
                       local_78_buf);

          sVar4 = local_player_count();
          FUN_000dbfb0(local_player_idx, 1 < sVar4,
                       (int)local_78_buf);
        }

        {
          int widget_base;
          int *widget_block_ptr;
          int widget_flags_mask;

          iVar13 = unhi_tag;
          widget_base = unhi_tag + 0x380;
          cVar3 = FUN_000a95a0();
          sVar4 = local_player_count();
          widget_block_ptr = (int *)(iVar13 + 0x3a4);
          widget_flags_mask = (int)((sVar4 < 2) - 1) & 4;

          if (0 < *(int *)(iVar13 + 0x3a4)) {
            int widget_type_mask;
            int widget_idx_int;

            widget_type_mask = (unsigned int)(cVar3 != '\0');
            widget_idx_int = 0;

            do {
              int widget_element;

              widget_element = (int)tag_block_get_element(
                  (void *)widget_block_ptr, widget_idx_int, 0x84);

              if ((widget_type_mask &
                   (1 << (*(unsigned char *)(widget_element + 0x68) &
                          0x1f))) != 0) {
                if ((*(unsigned char *)(widget_element + 0x6a) & 1) !=
                    0) {
                  unsigned int packed_color;
                  packed_color = FUN_000d1dd0(
                      (float *)(unit_data + 0x138));
                  *(unsigned int *)(widget_element + 0x34) =
                      packed_color | 0xff000000;
                }

                FUN_000d3fe0(local_player_idx,
                             (short *)widget_base,
                             widget_element,
                             (unsigned int)widget_flags_mask,
                             (int)0xFFFFFFFF);
              }

              i = (int)(short)(widget_idx_int + 1);
              widget_idx_int = i;
            } while (i < *widget_block_ptr);
          }
        }

        {
          int overlay_loop_idx = 0;

          if (0 < *(int *)(unhi_tag + 0x3cc)) {
            int overlay_idx = 0;

            do {
              psVar12 = (short *)tag_block_get_element(
                  (void *)(unhi_tag + 0x3cc), overlay_idx, 0x144);

              {
                short overlay_type;
                unsigned int type_bit;
                unsigned int bitmask_20;

                overlay_type = *psVar12;
                type_bit =
                    1 << ((unsigned char)overlay_type & 0x1f);
                bitmask_20 = (unsigned int)*(unsigned short
                                                *)((char *)pfVar6 +
                                                   0x20);

                if ((type_bit & bitmask_20) != 0 &&
                    (full_shield & type_bit) == 0) {
                  *(short *)((char *)pfVar6 +
                             overlay_type * 2 + 0x22) =
                      (short)0xffff;
                }

                overlay_type = *psVar12;
                type_bit =
                    1 << ((unsigned char)overlay_type & 0x1f);

                if ((full_shield & type_bit) == 0) {
                  if ((damage_active & type_bit) == 0) {
                    psVar22 =
                        (short *)((char *)pfVar6 +
                                  overlay_type * 2 + 0x22);
                    if (*psVar22 != -1) {
                      psVar22 = (short *)((char *)pfVar6 +
                                          *psVar12 * 2 + 0x22);
                      iVar13 =
                          FUN_000d2300((int)(psVar12 + 0x24));
                      if (*psVar22 < iVar13)
                        goto overlay_active_no_shield;
                    }
                    *psVar22 = -1;
                  } else {
                  overlay_active_no_shield:
                    tag_ref_result = verify_tag_reference(
                        (int *)(psVar12 + 0x1c));
                    sVar4 = local_player_count();
                    psVar22 =
                        (short *)((char *)pfVar6 +
                                  *psVar12 * 2 + 0x22);
                    sVar5 = game_time_get_elapsed();
                    *psVar22 = *psVar22 + sVar5;

                    if (tag_ref_result != (int)0xFFFFFFFF) {
                      int time_val = game_time_get();
                      FUN_000d3fe0(
                          local_player_idx,
                          (short *)unhi_tag,
                          (int)(psVar12 + 10),
                          ((unsigned int)((sVar4 < 2) - 1) &
                           4) |
                              1,
                          time_val -
                              (int)*(short *)((char *)pfVar6 +
                                              *psVar12 * 2 +
                                              0x22));
                    }
                  }
                } else {
                  int overlay_ref2;

                  tag_ref_result = verify_tag_reference(
                      (int *)(psVar12 + 0x1c));
                  overlay_ref2 = verify_tag_reference(
                      (int *)(psVar12 + 0x50));
                  sVar4 = local_player_count();
                  flags =
                      (unsigned int)((sVar4 < 2) - 1) & 4;

                  if (fraction_slots[*psVar12] <
                          *(float *)(psVar12 + 0x72) &&
                      fraction_slots[*psVar12] !=
                          *(float *)(psVar12 + 0x72)) {
                    flags = flags | 1;
                  }

                  psVar22 =
                      (short *)((char *)pfVar6 +
                                *psVar12 * 2 + 0x22);
                  sVar5 = game_time_get_elapsed();
                  *psVar22 = *psVar22 + sVar5;

                  psVar22 =
                      (short *)((char *)pfVar6 +
                                *psVar12 * 2 + 0x22);
                  iVar13 =
                      FUN_000d2300((int)(psVar12 + 0x24));
                  *psVar22 =
                      (short)((int)*psVar22 % (iVar13 << 1));

                  if (tag_ref_result != (int)0xFFFFFFFF) {
                    int time_val2 = game_time_get();
                    FUN_000d3fe0(
                        local_player_idx,
                        (short *)unhi_tag,
                        (int)(psVar12 + 10), flags,
                        time_val2 -
                            (int)*(short *)((char *)pfVar6 +
                                            *psVar12 * 2 +
                                            0x22));
                  }

                  if (overlay_ref2 != (int)0xFFFFFFFF) {
                    float frac_value;
                    float computed_alpha;
                    int alpha_a2, alpha_b2;
                    short alpha_scale;

                    alpha_scale = psVar12[99];
                    frac_value = fraction_slots[*psVar12];
                    meter_scale = (float)(int)alpha_scale;
                    computed_alpha = meter_scale * frac_value;

                    clamp_a = (int)computed_alpha;
                    if (clamp_a < 0) {
                      alpha_a2 = 0;
                    } else {
                      clamp_a = (int)computed_alpha;
                      if (clamp_a > 0xff) {
                        alpha_a2 = 0xff;
                      } else {
                        alpha_a2 = (int)computed_alpha;
                      }
                    }

                    clamp_b = (int)computed_alpha;
                    if (clamp_b < 0) {
                      alpha_b2 = 0;
                    } else {
                      clamp_b = (int)computed_alpha;
                      if (clamp_b > 0xff) {
                        alpha_b2 = 0xff;
                      } else {
                        alpha_b2 = (int)computed_alpha;
                      }
                    }

                    FUN_000d3340(
                        local_player_idx, unhi_tag,
                        (int)(psVar12 + 0x3e), alpha_b2,
                        alpha_a2, flags, (int)0xbf800000,
                        frac_value);
                  }
                }
              }

              overlay_idx =
                  (int)(short)(overlay_loop_idx + 1);
              overlay_loop_idx = overlay_loop_idx + 1;
            } while (overlay_idx <
                     *(int *)(unhi_tag + 0x3cc));
          }
        }

        *(unsigned short *)((char *)pfVar6 + 0x20) =
            (unsigned short)full_shield;
      }

    next_slot:
      slot_count = uVar18;
    }
  }

done_canary_check:
  {
    short canary_idx = 0x7f;
    do {
      if (canary_buf[canary_idx] != 0x62626262)
        goto canary_found;
      canary_idx = canary_idx - 1;
    } while (canary_idx >= 0);
    canary_idx = -1;

  canary_found:
    {
      int cookie_check = FUN_000d1540();
      if (canary_cookie != cookie_check) {
        display_assert("corrupt return address!",
                       "c:\\halo\\SOURCE\\interface\\hud_unit.c",
                       0x3c9, 1);
        system_exit(-1);
      }
      if (canary_idx != -1) {
        char *msg =
            csprintf((char *)0x5ab100, "corrupt stack at %d!",
                     (int)canary_idx);
        display_assert(
            msg, "c:\\halo\\SOURCE\\interface\\hud_unit.c",
            0x3c9, 1);
        system_exit(-1);
      }
    }
  }
}
