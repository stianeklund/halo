#include "x87_math.h"

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
  int i1;
  int i2;
  float *pu3;
  int u4;
  char c5;
  short s6;
  int draw_flag;
  float scale[2];
  float icon_rect[4];
  int l_24c[128];
  unsigned char l_4c[16];
  unsigned int l_24;
  int l_20;
  int l_1c;
  unsigned char l_18[4];
  int l_14;
  int l_10;
  int l_c;
  short *l_8;

  l_20 = FUN_000d1540();
  csmemset(l_24c, 0x62, 0x200);
  i1 = verify_tag_reference((int *)(param_3 + 0x24));
  l_8 = (short *)tag_get(0x6269746d, i1);
  l_c =
    (int)FUN_00077040(*(int *)(param_3 + 0x30), *(short *)(param_3 + 0x54), 0);
  i2 = (int)xbox_texture_cache_get_hardware_format((void *)l_c, 0, 1);
  if (i2 != 0) {
    pu3 =
      (float *)FUN_000d1580(verify_tag_reference((int *)(param_3 + 0x24)),
                            *(short *)(param_3 + 0x54), 0);
    if ((param_4 & 2) == 0) {
      if ((param_4 & 1) == 0) {
        u4 = *(int *)(param_3 + 0x34);
      } else {
        u4 = FUN_000d2320((int *)(param_3 + 0x34), param_5);
      }
    } else {
      u4 = *(int *)(param_3 + 0x4c);
    }
    s6 = *l_8;
    l_14 = (s6 == 4);
    FUN_000d3080((int)pu3, (int)param_3, l_c, 0, param_2, 1.0f, 0, u4,
                 (param_4 >> 2) & 0xffffff01, (char)l_14, 0);
    l_8 = (short *)0;
    if (0 < *(int *)(param_3 + 0x58)) {
      i2 = 0;
      l_24 = param_4 & 4;
      icon_rect[0] = 0.0f;
      icon_rect[2] = 0.0f;
      c5 = (char)(s6 == 4);
      while (1) {
        l_1c =
          (int)tag_block_get_element((void *)(param_3 + 0x58), i2, 0x1e0);
        icon_rect[1] = 1.0f;
        icon_rect[3] = 1.0f;
        if (c5 != '\0') {
          l_10 = (int)*(short *)(l_c + 6);
          icon_rect[1] = (float)(int)*(short *)(l_c + 4);
          icon_rect[3] = (float)l_10;
        }
        if (pu3 == (float *)0) {
          pu3 = icon_rect;
        }
        scale[0] = *(float *)(param_3 + 4);
        scale[1] = *(float *)(param_3 + 8);
        if (((short)l_24 == 0) ||
            (draw_flag = 1, (*(unsigned char *)(param_3 + 0xc) & 1) != 0)) {
          draw_flag = 0;
        }
        FUN_000d1f40((short)*(int *)0x506548, (unsigned short *)param_2,
                     (short *)param_3, 0, draw_flag, 0, (short *)l_18);
        /* d1890: @<eax>=l_4c (out corners), @<edi>=pu3 (in rect),
         * @<bl>=c5 (align flag); 2 stack args: bitmap, screen index. */
        FUN_000d1890((float *)l_4c, pu3, c5, (short *)l_c,
                     *param_2);
        /* d27a0: @<ecx>=l_1c (element ptr), @<eax>=scale[2];
         * 6 stack args; 6th = u4 (color, raw int bitpattern). */
        FUN_000d27a0(l_1c, scale, param_1, l_18, pu3,
                     (float *)l_4c, 0, u4);
        i2 = i2 + 1;
        i2 = (int)(short)i2;
        if (*(int *)(param_3 + 0x58) <= i2)
          break;
        c5 = (char)l_14;
      }
    }
  }
  (void)scale;
  (void)l_1c;
  s6 = 0x7f;
  do {
    if (l_24c[(int)s6] != 0x62626262)
      goto LAB_000d41e7;
    s6 = s6 - 1;
  } while (-1 < s6);
  s6 = -1;
LAB_000d41e7:
  i2 = FUN_000d1540();
  if (l_20 != i2) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x2ad, 1);
    system_exit(-1);
  }
  if (s6 != -1) {
    display_assert(
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)s6),
      "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x2ad, 1);
    system_exit(-1);
  }
}

/* hud_draw_overlay_elements (0xd4260)
 * Draw overlay bitmap elements for a HUD widget. Iterates over the overlay
 * element tag block, performs bitmap lookup with optional animation cycling,
 * optional color interpolation, and renders each visible element via
 * FUN_000d3080. Protected by a stack canary (0x200 bytes of 0x62). */
void FUN_000d4260(int param_1, int param_2, int param_3, unsigned int param_4,
                  int param_5, unsigned char param_6, int param_7)
{
  int element;
  int bitmap_seq;
  int color;
  int frame_idx;
  int i1;
  short s5;
  int l_214[128];
  int l_14;
  int out_sprite;
  int out_bitmap;
  int l_4;

  l_14 = FUN_000d1540();
  csmemset(l_214, 0x62, 0x200);
  l_4 = 0;
  if (0 < *(int *)(param_3 + 0x10)) {
    do {
      element =
        (int)tag_block_get_element((void *)(param_3 + 0x10), l_4, 0x88);
      if ((*(unsigned char *)(element + 0x4c) & 2) == 0 &&
          (param_4 & (int)*(short *)(element + 0x4a)) != 0) {
        bitmap_seq = (int)tag_block_get_element(
          (void *)((int)tag_get(0x6269746d, *(int *)(param_3 + 0xc)) + 0x54),
          (int)*(short *)(element + 0x48), 0x40);
        if ((*(unsigned char *)(element + 0x4c) & 1) == 0 ||
            (param_6 & 1) == 0) {
          color = *(int *)(element + 0x24);
        } else {
          color = FUN_000d2320((int *)(element + 0x24), param_5);
        }
        if ((*(unsigned char *)(element + 0x4c) & 1) == 0 ||
            (param_6 & 1) == 0 || *(short *)(element + 0x44) < 1) {
          frame_idx = 0;
        } else {
          frame_idx =
            ((game_time_get() - param_5) / (int)*(short *)(element + 0x44)) /
            30 % *(int *)(bitmap_seq + 0x34);
        }
        out_bitmap = 0;
        out_sprite = 0;
        FUN_000d16a0(*(int *)(param_3 + 0xc),
                     *(unsigned short *)(element + 0x48), frame_idx,
                     &out_bitmap, &out_sprite);
        if (out_bitmap != 0 && (int)xbox_texture_cache_get_hardware_format(
                                 (void *)out_bitmap, 0, 1) != 0) {
          FUN_000d3080(out_sprite, element, out_bitmap, 0, (short *)param_2,
                       1.0f, 0, color, param_7, 0, 0);
        }
      }
      l_4 = l_4 + 1;
    } while (l_4 < *(int *)(param_3 + 0x10));
  }
  s5 = 0x7f;
  do {
    if (l_214[(int)s5] != 0x62626262)
      goto LAB_000d43f5;
    s5 = s5 - 1;
  } while (-1 < s5);
  s5 = -1;
LAB_000d43f5:
  i1 = FUN_000d1540();
  if (l_14 != i1) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x2ec, 1);
    system_exit(-1);
  }
  if (s5 != -1) {
    display_assert(
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)s5),
      "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x2ec, 1);
    system_exit(-1);
  }
}

/* hud_draw_text_element (0xd4470)
 * Draw a text element on the HUD, with optional icon rendering.
 * ABI: @esi=src_rect, @edi=dst_rect, @ebx=text, stack: param_1=use_icons */
void FUN_000d4470(char param_1, short *src_rect, short *dst_rect, void *text)
{
  short l_8[4];

  draw_string_set_indents(
    (int)(unsigned short)src_rect[1] - (int)(unsigned short)dst_rect[1], 0);
  FUN_0019cdb0(dst_rect, text, l_8, src_rect);
  src_rect[1] = src_rect[1] - 3;
  l_8[1] = dst_rect[1];
  if (param_1 != '\0') {
    if (game_engine_running() != '\0') {
      draw_string_and_hack_in_icons(l_8, 0, 0, 0, (wchar_t *)text, 1);
      *dst_rect = *src_rect;
      return;
    }
  }
  rasterizer_draw_string(l_8, 0, 0, 0, (unsigned short *)text);
  *dst_rect = *src_rect;
}

/* hud_draw_icon_sprite (0xd44f0)
 * Look up a bitmap element and draw it as a sprite.
 * ABI: @esi=element, @ebx=cursor */
void FUN_000d44f0(int cursor, short *element, int param_1, int param_2)
{
  int i4;
  short s2;
  int l_14;
  short l_10[2];
  int l_c;
  int l_8;
  float scale;

  i4 = 0;
  l_c = 0;
  l_14 = 0;
  if (*(char *)((int)element + 12) != '\0') {
    i4 = game_time_get();
    i4 = i4 / (int)*(char *)((int)element + 12);
  }
  FUN_000d16a0(*(int *)(*(int *)0x46bd0c + 0xb0), *element, i4, &l_c,
               &l_14);
  if (l_c != 0 &&
      (int)xbox_texture_cache_get_hardware_format((void *)l_c, 0, 1) != 0) {
    s2 = local_player_count();
    l_8 = 0x3f400000;
    if (s2 < 2) {
      l_8 = 0x3f800000;
    }
    scale = *(float *)&l_8;
    l_10[0] = (short)((float)(int)element[2] * scale +
                          (float)(int)*(short *)(cursor + 2));
    l_10[1] = (short)((float)(int)*(short *)(cursor + 4) -
                          (float)(int)element[3] * scale);
    if ((*(unsigned char *)((int)element + 0xd) & 2) != 0) {
      param_2 = *(int *)(element + 4);
    }
    FUN_000d3200(l_c, 2, l_10, l_14, scale, 0, param_2, 0);
    if ((*(unsigned char *)((int)element + 0xd) & 4) != 0) {
      *(short *)(cursor + 2) =
        (short)((float)(int)element[1] * scale + (float)(int)l_10[0]);
      return;
    }
    if (l_14 != 0) {
      int *rect = (int *)l_14;
      float rect_w = *(float *)(rect + 1) - *(float *)rect;
      *(short *)(cursor + 2) =
        (short)(((float)(int)*(short *)(l_c + 4) * rect_w +
                 (float)(int)element[1]) *
                  scale +
                (float)(int)l_10[0]);
      return;
    }
    *(short *)(cursor + 2) =
      (short)((float)((int)*(short *)(l_c + 4) + (int)element[1]) * scale +
              (float)(int)l_10[0]);
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
    if (*(char *)(element + 0x24) == 1 && *pcVar == '\0') {
      globals = *(int *)0x46bd0c;
      base = *(int *)0x46bd18;
      *(int *)(base + 0x1190) = element;
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
  short s;

  base = *(int *)0x46bd18;
  *(short *)(base + 0x119c) = (param_1 * 0x3c + param_2) * 0x1e;
  *(unsigned char *)(base + 0x11a6) = 0;
  *(unsigned char *)(base + 0x11a7) = 1;
  tick = game_time_get();
  base = *(int *)0x46bd18;
  *(int *)(base + 0x1198) = tick;
  s = *(short *)(base + 0x11a4);
  if (s < 0) {
    *(short *)(base + 0x11a4) = 0;
    return;
  }
  if (4 < s) {
    *(short *)(base + 0x11a4) = 4;
    return;
  }
  *(short *)(base + 0x11a4) = s;
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

  /* The original holds a pointer to the HUD-timer sub-struct at globals+0x1198
   * and addresses its members with small displacements (0x0, 0x4, 0xe); folding
   * 0x1198 into every displacement instead costs one `add esi, 0x1198`. */
  base = *(int *)0x46bd18 + 0x1198;
  *(char *)(base + 0xe) = param_1;
  if (*(short *)(base + 0x4) > 0) {
    if (param_1 != '\0') {
      now = (short)game_time_get();
      *(short *)(base + 0x4) += *(short *)base - now;
      return;
    }
    now = (short)game_time_get();
    *(short *)(base + 0x4) += now - *(short *)base;
  }
}

/* scripted_hud_get_timer_ticks (0xd49d0)
 * Returns remaining timer ticks, or 0 if hidden. */
short scripted_hud_get_timer_ticks(void)
{
  char *timer;
  char visible;
  short result;
  int now;
  unsigned int total;

  timer = *(char **)0x46bd18;
  visible = timer[0x11a7];
  timer += 0x1198;
  result = 0;
  if (visible != '\0') {
    if (*(uint16_t *)(timer + 0x4) == 0xffff) {
      return -1;
    }
    result = *(short *)(timer + 0x4);
    if (timer[0xe] == '\0') {
      now = game_time_get();
      total = (unsigned short)(*(unsigned short *)(timer + 0x4) + *(unsigned short *)timer);
      result = (short)(total - now);
    }
  }
  return result;
}

/* scripted_hud_time_code_show (0xd4a20)
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

/* scripted_hud_time_code_start (0xd4a50)
 * Pause or unpause the loading screen timer. */
void scripted_hud_time_code_start(char param_1)
{
  int now;
  int *timer_end;

  if (param_1 != '\0') {
    now = game_time_get();
    timer_end = (int *)0x2f66e8;
    *(int *)0x2f66e4 = *(int *)0x2f66e4 + (now - *timer_end);
    *timer_end = -1;
    return;
  }
  *(int *)0x2f66e8 = game_time_get();
}

/* scripted_hud_time_code_reset (0xd4a90)
 * Reset the loading timer start to current tick. */
void scripted_hud_time_code_reset(void)
{
  int tick;

  tick = game_time_get();
  if (*(int *)0x2f66e8 == -1) {
    *(int *)0x2f66e4 = tick;
  } else {
    *(int *)0x2f66e4 = tick;
    *(int *)0x2f66e8 = tick;
  }
}

/* hud_render_timer (0xd4ab0) */
void hud_render_timer(void)
{
  char *timer;
  int i6;
  int i8;
  short u1;
  short s2;
  short l_90;
  int l_8e[8];
  short l_8e_tail;
  int l_68;
  float l_64;
  float l_60;
  int l_48[8];
  char l_28;
  char l_27;
  char l_26;
  char pad_21;
  double l_14;
  int l_10;
  int l_c;
  int l_8;
  int l_4;
  int loading_time;
  float font_height;

  if (*(char *)(*(char **)0x46bd18 + 0x11a7) != '\0') {
    timer = *(char **)0x46bd18 + 0x1198;
    l_90 = *(short *)(timer + 0xc);
    csmemset(l_8e, 0, 34);
    l_8 = game_time_get();
    i8 = 0;
    l_c = 0;
    l_10 = (int)scripted_hud_get_timer_ticks();
    l_68 = *(int *)(timer + 0x8);
    *(volatile char *)&l_28 = 2;
    *(volatile char *)&l_26 = 4;
    *(volatile char *)&l_27 = 1;
    *(volatile float *)&l_64 = 1.0f;
    *(volatile float *)&l_60 = 1.0f;
    (void)l_8e_tail;
    (void)pad_21;
    i6 = interface_get_tag_index(0xb);
    if (i6 != -1) {
      i6 = (int)tag_get(0x68756423, i6);
      l_c = (int)*(char *)(i6 + 0x11);
      font_height = (float)l_c;
      i8 = (int)(font_height + font_height);
      l_c = i8;
    }
    switch (*(short *)(timer + 0xc)) {
    case 0:
    case 2:
      break;
    case 1:
    case 3:
      *(short *)&l_68 = (short)(*(short *)&l_68 + (short)i8 * 5);
      l_c = -i8;
      break;
    case 4:
      *(short *)&l_68 = (short)(*(short *)&l_68 + (short)i8 * -3);
      break;
    default:
      display_assert("!\"unreachable\"",
                     "c:\\halo\\SOURCE\\interface\\hud_messaging.c", 0x1f8, 1);
      system_exit(-1);
    }
    i6 = 8;
    if ((short)l_10 > 0) {
      u1 = (short)l_10;
      qmemcpy(l_48, (void *)(*(int *)0x46bd0c + 0x360), 32);
      s2 = *(short *)(timer + 0x6);
      i6 = l_c;
      if ((short)u1 <= s2) {
        i6 = 1;
        if (s2 < *(short *)(timer + 0x4)) {
          *(short *)(timer + 0x4) = s2;
          *(int *)timer = ((int)s2 - (int)(short)u1) + l_8;
        }
      }
    } else {
      i8 = *(int *)timer;
      *(short *)(timer + 0x4) = (short)-1;
      qmemcpy(l_48, (void *)(*(int *)0x46bd0c + 0x380), 32);
      l_c = 1;
      if (i8 == -1) {
        *(int *)timer = game_time_get();
      }
      u1 = (short)l_10;
      i6 = l_c;
    }
    i8 = (int)(short)(u1 & ((short)u1 < 1) - 1);
    l_8 = i8;
    l_4 = i8;
    i8 = (int)((float)l_4 * *(float *)0x2546a4 * *(float *)0x25634c);
    FUN_000d3860((short)*(int *)0x506548, &l_90, &l_68, i8, -1,
                 i6, *(int *)timer, 2.0f);
    l_4 = (int)(short)l_c;
    l_14 = (double)l_4 * *(double *)0x281b40;
    l_4 = (int)*(short *)&l_68;
    *(short *)&l_68 = (short)(int)((double)l_4 + l_14);
    i8 = (l_8 / 30) % 60;
    FUN_000d3860((short)*(int *)0x506548, &l_90, &l_68, i8, -1,
                 i6, *(int *)timer, 2.0f);
    l_4 = (int)*(short *)&l_68;
    *(short *)&l_68 = (short)(int)((double)l_4 + l_14);
    i8 = ((l_8 % 1800) * 100) / 30;
    FUN_000d3860((short)*(int *)0x506548, &l_90, &l_68, i8, -1,
                 i6, *(int *)timer, 2.0f);
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

/* hud_set_state_message (0xd4d90)
 * Set a HUD message element reference for a player. */
void hud_set_state_message(short param_1, short param_2)
{
  int i1;
  int i3;
  short s4;

  if (*(char *)(*(int *)0x46bd10 + 1) == '\0' &&
      *(int *)(*(int *)0x46bd0c + 0xfc) != -1) {
    i3 = (int)(short)param_1 * 0x460 + *(int *)0x46bd18;
    s4 = param_2;
    if (param_2 != -1) {
      i1 = (int)tag_get(0x686d7420, *(int *)(*(int *)0x46bd0c + 0xfc));
      if ((int)(short)param_2 < *(int *)(i1 + 0x20)) {
        *(int *)(i3 + 0x454) = (int)tag_block_get_element(
          (void *)(i1 + 0x20), (int)(short)param_2, 0x40);
        *(unsigned char *)(i3 + 0x459) = 0;
        *(unsigned char *)(i3 + 0x458) = (param_2 != -1);
        return;
      }
      s4 = -1;
    }
    *(unsigned char *)(i3 + 0x458) = (s4 != -1);
  }
}

/* hud_set_state_message_icon (0xd4e30)
 * Set a numeric value for a HUD message element. */
void hud_set_state_message_icon(short param_1, short param_2, int param_3)
{
  int i1;

  i1 = param_1 * 0x460 + *(int *)0x46bd18;
  if (*(char *)(i1 + 0x458) != '\0' &&
      *(char *)(*(int *)0x46bd10 + 1) == '\0' && *(int *)(i1 + 0x454) != 0) {
    *(int *)(i1 + 0x434 + param_2 * 4) = param_3;
    *(unsigned char *)(i1 + 0x459) &=
      (unsigned char)~(1 << (unsigned char)param_2);
  }
}

/* hud_set_state_message_text (0xd4e90)
 * Set a tag reference value for a HUD message element. */
void hud_set_state_message_text(short param_1, short param_2, short param_3,
                                unsigned char param_4)
{
  int i1;

  i1 = param_1 * 0x460 + *(int *)0x46bd18;
  if (*(char *)(i1 + 0x458) != '\0' &&
      *(char *)(*(int *)0x46bd10 + 1) == '\0' && *(int *)(i1 + 0x454) != 0) {
    *(short *)(i1 + 0x434 + param_2 * 4) = param_3;
    *(unsigned char *)(i1 + 0x436 + param_2 * 4) = param_4;
    *(unsigned char *)(i1 + 0x459) |=
      (unsigned char)(1 << (unsigned char)param_2);
  }
}

/* hud_enable_custom_state_message (0xd4f00)
 * Toggle help text display state for a player. */
void hud_enable_custom_state_message(short param_1, char param_2)
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

/* hud_set_state_text (0xd4f70)
 * Set help text string for a player. */
void hud_set_state_text(short param_1, wchar_t *param_2)
{
  int i1;

  i1 = param_1 * 0x460 + *(int *)0x46bd18;
  ustrncpy((wchar_t *)(i1 + 0x230), param_2, 0xff);
  *(short *)(i1 + 0x42e) = 0;
}

/* hud_messaging_get_objective (0xd4fb0)
 * Get the objective text string from the HMT tag. */
int hud_messaging_get_objective(void)
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

    if ((tag_handle != -1 && tag_handle == *(int *)(entry + 0x84) &&
         (char)param2 == *(char *)(entry + 0x8a)) ||
        *(char *)(entry + 0x82) == 0) {
      result = (void *)entry;
      if (tag_handle == -1 || tag_handle == *(int *)(entry + 0x84))
        break;
    } else {
      if (*(int *)entry < best_time) {
        best_time = *(int *)entry;
        best_index = i;
      }
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
int *hud_get_text_color(int *param_1)
{
  struct s_hud_text_color {
    int dwords[4];
  };

  *(struct s_hud_text_color *)param_1 = *(struct s_hud_text_color *)(*(char **)0x5aa68c + 0x70);
  return param_1;
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
  char *globals;

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
  globals = *(char **)0x46bd18;
  *(uint8_t *)(slot + 0x82) = 1;
  *(uint8_t *)(slot + 0x83) = *(uint8_t *)(globals + 0x1185);
  *(uint8_t *)(globals + 0x1185) += 1;
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
  char c1;
  int player_obj;
  int actor_obj;
  int player_index;
  int i;

  c1 = game_engine_running();
  if (c1 != '\0') {
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
  char c1;
  short s2;
  int u7;
  int font_tag_data;
  char *pc9;
  int *pi10;
  int i12;
  short *ps13;
  short *l_38_p;
  int i16;
  int slot_base;
  int hmt_tag;
  int message_ptr;
  int game_ticks;
  unsigned int font_height;
  int max_slots;
  int font_index;
  short s15;
  short position_out[2];
  short l_64;
  int y_raw;
  int y_cur;
  float color[4];
  uint32_t packed_color;
  int l_30_dw;
  int l_2c;
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

  c1 = cinematic_in_progress();
  if (c1 != '\0') {
    return;
  }
  if ((short)param_1 == -1) {
    return;
  }
  u7 = local_player_get_player_index((short)param_1);
  c1 = game_engine_hud_draw_messages(u7);
  if (c1 == '\0') {
    return;
  }

  s2 = local_player_count();
  if (s2 < 2 ||
      (font_index = *(int *)(*(int *)0x5aa68c + 0x64), font_index == -1)) {
    font_index = *(int *)(*(int *)0x5aa68c + 0x54);
  }
  s2 = local_player_count();
  is_splitscreen = 1 < s2;

  {
    short s3;
    s3 = local_player_count();
    FUN_000d1f40((short)param_1, (unsigned short *)*(int *)0x5aa68c,
                 (short *)(*(int *)0x5aa68c + 0x24), 0, 1 < s3, 0,
                 position_out);
  }
  l_64 = position_out[0];
  /* position_out[1] is read once into EDI (the running y cursor) and
   * afterwards only ever re-read as a *short* from its untouched stack slot:
   * the split-screen adjustment below applies to the register copy only.
   * (The original loads a dword at out+2 but never uses above bit 15.) */
  y_raw = (int)position_out[1];
  y_cur = y_raw;

  font_tag_data = (int)tag_get(0x666f6e74, font_index);
  if (is_splitscreen) {
    font_height = (unsigned int)(unsigned short)(*(short *)(font_tag_data + 8) +
                                                 *(short *)(font_tag_data + 4));
    y_cur = y_cur - *(int *)0x2f66ec;
  } else {
    font_height = (unsigned int)(unsigned short)(*(short *)(font_tag_data + 8) +
                                                 *(short *)(font_tag_data + 6) +
                                                 *(short *)(font_tag_data + 4));
  }
  s15 = (short)y_cur;

  slot_base = (int)(short)*(int *)0x506548 * 0x460 + *(int *)0x46bd18;
  {
    short s3;
    s3 = local_player_count();
    max_slots = 4 - (int)(1 < s3);
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
      (*(int *)(slot_base + 0x454) == 0 &&
       *(short *)(slot_base + 0x230) == 0)) {
    show_custom = '\0';
  } else {
    show_custom = '\x01';
  }

  if (show_objective != '\0' || show_state != '\0' || show_custom != '\0') {
    l_38_p = (short *)(slot_base + 0x230);
    input_abstraction_get_local_player_preferences((short)param_1, prefs);
    c1 = show_objective;
    i16 = *(int *)0x46bd0c;

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
          packed_color =
            (uint32_t)FUN_000d2320((int *)(*(int *)0x46bd0c + 0xd0),
                                   *(int *)(*(int *)0x46bd18 + 0x1180));
        }
        pixel32_to_real_argb_color(packed_color, color);
      }
    } else {
      i12 = *(int *)0x46bd0c + 0x100;
      i16 = game_time_get();
      packed_color = (uint32_t)FUN_000d2320(
        (int *)i12, i16 + (((int)*(short *)(*(int *)0x46bd18 + 0x1194) -
                                  (int)*(short *)(i12 + 0x1c)) -
                                 (int)*(short *)(i12 + 0x1e)));
      pixel32_to_real_argb_color(packed_color, color);
      l_2c = (int)*(short *)(i12 + 0x1e);
      {
        float fade;
        fade =
          (float)(int)*(short *)(*(int *)0x46bd18 + 0x1194) / (float)l_2c;
        if (1.0f < fade) {
          fade = 1.0f;
        }
        color[0] = fade * color[0];
      }
      packed_color = FUN_000d1c90(color);
    }

    rect_a[3] = *(short *)0x50658a - *(short *)0x50657e;
    rect_a[1] = l_64;
    rect_a[0] = s15;
    if (*(char *)(*(int *)0x46bd10 + 1) != '\0') {
      rect_a[2] = (short)font_height + (s15 + (short)font_height * 4);
    } else {
      rect_a[2] = (short)font_height + s15;
    }
    *(int *)&rect_b[0] = *(int *)&rect_a[0];
    *(int *)&rect_b[2] = *(int *)&rect_a[2];

    draw_string_set_font(font_index, -1, 0, 0, color);

    if (c1 == '\0') {
      if (show_state != '\0') {
        i16 = (int)global_scenario_get();
        hmt_tag = (int)tag_get(0x686d7420, *(int *)(i16 + 0x5a0));
        message_ptr = *(int *)(*(int *)0x46bd18 + 0x118c);
        goto LAB_000d57ad;
      }
      if (*(int *)(slot_base + 0x454) != 0) {
        if (show_custom == '\0') {
          display_assert("show_state_message",
                         "c:\\halo\\SOURCE\\interface\\hud_messaging.c", 0x42a,
                         1);
          system_exit(-1);
        }
        hmt_tag = (int)tag_get(0x686d7420, *(int *)(*(int *)0x46bd0c + 0xfc));
        message_ptr = *(int *)(slot_base + 0x454);
        goto LAB_000d57ad;
      }
      if (*l_38_p != 0) {
        FUN_000d4470(1, rect_b, rect_a, (void *)l_38_p);
      }
    } else {
      if (*(int *)(*(int *)0x46bd18 + 0x1190) == 0 ||
          *(short *)(*(int *)0x46bd18 + 0x1194) == 0) {
        display_assert("hud_messaging_globals->objective.message && "
                       "hud_messaging_globals->objective.uptime",
                       "c:\\halo\\SOURCE\\interface\\hud_messaging.c", 0x41e,
                       1);
        system_exit(-1);
      }
      {
        short elapsed;
        int remaining;
        ps13 = (short *)(*(int *)0x46bd18 + 0x1194);
        elapsed = game_time_get_elapsed();
        remaining = (int)*ps13 - (int)elapsed;
        if (remaining <= 0) {
          elapsed = 0;
        } else {
          ps13 = (short *)(*(int *)0x46bd18 + 0x1194);
          elapsed = game_time_get_elapsed();
          elapsed = *ps13 - elapsed;
        }
        *ps13 = elapsed;
      }
      i16 = (int)global_scenario_get();
      hmt_tag = (int)tag_get(0x686d7420, *(int *)(i16 + 0x5a0));
      message_ptr = *(int *)(*(int *)0x46bd18 + 0x1190);

    LAB_000d57ad:
      l_30_dw =
        (int)(unsigned short)*(unsigned short *)(message_ptr + 0x20);
      l_2c = 0;
      if (*(unsigned char *)(message_ptr + 0x24) != 0) {
        int tag_block_base;
        i16 = 0;
        tag_block_base = hmt_tag + 0x14;
        ps13 = l_38_p;
        do {
          pc9 = (char *)tag_block_get_element(
            (void *)tag_block_base,
            (int)(unsigned short)*(unsigned short *)(message_ptr + 0x22) +
              i16,
            2);
          switch (*pc9) {
          case '\0':
            u7 = (int)tag_data_get_pointer(
              (void *)hmt_tag, ((unsigned int)(unsigned short)l_30_dw) << 1,
              (unsigned int)(unsigned char)pc9[1] << 1);
            draw_string_set_indents(
              *(int *)((char *)rect_b + 2) - *(int *)((char *)rect_a + 2), 0);
            FUN_0019cdb0(rect_a, (void *)u7, bounds_a, rect_b);
            rect_b[1] = rect_b[1] - 3;
            bounds_a[1] = rect_a[1];
            rasterizer_draw_string(bounds_a, 0, 0, 0, (unsigned short *)u7);
            rect_a[0] = rect_b[0];
            l_30_dw =
              l_30_dw + (int)(unsigned short)(unsigned char)pc9[1];
            break;
          case '\x01': {
            unsigned char b1;
            int icon_idx;
            b1 = (unsigned char)pc9[1];
            icon_idx = -1;
            if (b1 <= 0x11) {
              icon_idx = (int)(unsigned short)b1;
            } else if (b1 <= 0x1f) {
              if (b1 <= 0x1c) {
                /* prefs buffer is filled at base+0; the original indexes it at
                 * base+8 (movzx bx,[ebp+edx-0x98] vs buffer base [ebp-0xa0]).
                 */
                icon_idx = (int)(unsigned short)
                  prefs[(int)(signed char)((char *)0x2f66c2)[b1] + 8];
              } else {
                icon_idx = (int)(short)(signed char)((char *)0x2f66c2)[b1];
              }
            } else {
              if (*(char *)(*(int *)0x46bd10 + 1) == '\0') {
                short custom_si;
                int ci;
                custom_si = (short)((unsigned short)b1 - 0x20);
                if (custom_si >= 8) {
                  display_assert("custom_index<NUMBER_OF_HUD_CUSTOM_ICONS",
                                 "c:\\halo\\SOURCE\\interface\\hud_messaging.c",
                                 0x455, 1);
                  system_exit(-1);
                }
                ci = (int)custom_si;
                if (((unsigned char)(1 << ((unsigned char)custom_si & 0x1f)) &
                     *(unsigned char *)((char *)ps13 + 0x229)) == 0) {
                  if (*(int *)((char *)ps13 + ci * 4 + 0x204) == 0) {
                    error(
                      2,
                      "help message using old code. get latest code and tags.");
                  } else {
                    FUN_000d44f0(
                      (int)rect_b,
                      (short *)(*(int *)((char *)ps13 + ci * 4 + 0x204)),
                      (int)rect_a, (int)packed_color);
                  }
                } else {
                  short icon_ref;
                  wchar_t *icon_text;
                  icon_ref = *(short *)((char *)ps13 + ci * 4 + 0x204);
                  if (icon_ref == -1) {
                    icon_text = L"<unknown>";
                  } else if (*(char *)((char *)ps13 + ci * 4 + 0x206) ==
                             '\0') {
                    icon_text =
                      (wchar_t *)FUN_0019d420(*(int *)(*(int *)0x46bd0c + 0xc0),
                                              (int)(unsigned short)icon_ref);
                  } else {
                    int scenario;
                    scenario = (int)global_scenario_get();
                    if (*(int *)(scenario + 0x580) == -1) {
                      display_assert(
                        "global_scenario_get()->custom_object_names.index!="
                        "NONE",
                        "c:\\halo\\SOURCE\\interface\\hud_messaging.c", 0x45e,
                        1);
                      system_exit(-1);
                    }
                    scenario = (int)global_scenario_get();
                    icon_text =
                      (wchar_t *)FUN_0019d420(*(int *)(scenario + 0x580),
                                              (int)(unsigned short)icon_ref);
                  }
                  FUN_000d4470(0, rect_b, rect_a, (void *)icon_text);
                  ps13 = l_38_p;
                }
                goto LAB_000d5b78;
              }
              error(2, "help text cannot use custom icons");
            }
            if ((int)(short)icon_idx < *(int *)(*(int *)0x46bd0c + 0xc4)) {
              short remapped;
              int icon_element;
              remapped =
                remap_sticks_for_local_player((short)icon_idx, param_1);
              icon_element = (int)tag_block_get_element(
                (void *)(*(int *)0x46bd0c + 0xc4), (int)remapped, 0x10);
              if ((*(unsigned char *)(icon_element + 0xd) & 1) == 0) {
                FUN_000d44f0((int)rect_b, (short *)icon_element, (int)rect_a,
                             (int)packed_color);
              } else {
                if ((*(unsigned char *)(icon_element + 0xd) & 2) != 0) {
                  pixel32_to_real_argb_color(packed_color, icon_color);
                  draw_string_set_font(font_index, -1, 0, 0, icon_color);
                }
                u7 = (int)FUN_0019d420(
                  *(int *)(*(int *)0x46bd0c + 0xc0),
                  (int)*(unsigned short *)(icon_element + 0xe));
                draw_string_set_indents(*(int *)((char *)rect_b + 2) -
                                          *(int *)((char *)rect_a + 2),
                                        0);
                FUN_0019cdb0(rect_a, (void *)u7, bounds_b, rect_b);
                rect_b[1] = rect_b[1] - 3;
                bounds_b[1] = rect_a[1];
                rasterizer_draw_string(bounds_b, 0, 0, 0,
                                       (unsigned short *)u7);
                rect_a[0] = rect_b[0];
                draw_string_set_font(font_index, -1, 0, 0, color);
              }
            } else {
              draw_string_set_indents(
                *(int *)((char *)rect_b + 2) - *(int *)((char *)rect_a + 2), 0);
              FUN_0019cdb0(rect_a, (void *)L"<no button icon>", bounds_c,
                           rect_b);
              rect_b[1] = rect_b[1] - 3;
              bounds_c[1] = rect_a[1];
              rasterizer_draw_string(bounds_c, 0, 0, 0,
                                     (unsigned short *)L"<no button icon>");
              rect_a[0] = rect_b[0];
            }
            break;
          }
          default:
            display_assert("!\"unreachable\"",
                           "c:\\halo\\SOURCE\\interface\\hud_messaging.c",
                           0x4a5, 1);
            system_exit(-1);
            break;
          }
        LAB_000d5b78:
          l_2c = l_2c + 1;
          i16 = (int)(short)l_2c;
        } while (i16 <
                 (int)(unsigned int)*(unsigned char *)(message_ptr + 0x24));
      }
    }
    draw_string_set_indents(0, 0);
    y_cur = *(int *)&rect_b[2];
    if (show_objective != '\0' || show_state != '\0') {
      goto LAB_000d5c20;
    }
  }

  if (*(char *)(slot_base + 0x458) != '\0' ||
      *(char *)(slot_base + 0x45e) != '\0') {
    float ftmp;
    if (is_splitscreen) {
      int split_y;
      split_y = (int)(short)y_raw - *(int *)0x2f66ec;
      ftmp = (float)split_y + (float)(int)(short)font_height *
                                *(float *)(*(int *)0x5aa68c + 0x90);
    } else {
      ftmp =
        (float)(int)(short)font_height * *(float *)(*(int *)0x5aa68c + 0x90) +
        (float)(int)(short)y_raw;
    }
    y_cur = (int)ftmp;
    max_slots = max_slots - 1;
  }

LAB_000d5c20:
  qsort((void *)slot_base, 4, 0x8c,
        (int(__cdecl *)(const void *, const void *))hud_messaging_slot_compare);
  msg_slot_idx = 0;
  if (0 < (short)max_slots) {
    do {
      int *pi14;
      int slot_offset;
      float fade_alpha;
      float uptime_limit;
      pi14 = (int *)((short)msg_slot_idx * 0x8c + slot_base);
      if (*(char *)((short)msg_slot_idx * 0x8c + 0x82 + slot_base) == '\0') {
        return;
      }
      game_ticks = game_time_get();
      color[0] = *(float *)(*(int *)0x5aa68c + 0x80);
      color[1] = *(float *)(*(int *)0x5aa68c + 0x84);
      color[2] = *(float *)(*(int *)0x5aa68c + 0x88);
      color[3] = *(float *)(*(int *)0x5aa68c + 0x8c);
      slot_offset = game_ticks - *pi14;
      uptime_limit = *(float *)(*(int *)0x5aa68c + 0x68) * 30.0f;
      if (uptime_limit < (float)slot_offset) {
        fade_alpha = 1.0f - ((float)slot_offset - uptime_limit) /
                              (*(float *)(*(int *)0x5aa68c + 0x6c) * 30.0f);
        if (fade_alpha < 0.0f) {
          fade_alpha = 0.0f;
        } else if (1.0f < fade_alpha) {
          fade_alpha = 1.0f;
        }
        color[0] = (float)pow((double)fade_alpha, 1.9) * color[0];
      }
      *(int *)&rect_b[0] = *(int *)0x506584;
      *(int *)&rect_b[2] = *(int *)0x506588;
      rect_b[3] = *(short *)0x50658a - *(short *)0x50657e;
      rect_b[1] = l_64;
      rect_b[0] = (short)y_cur;
      rect_b[2] = (short)font_height + rect_b[0];
      {
        float ftmp2;
        ftmp2 =
          (float)(int)(short)font_height * *(float *)(*(int *)0x5aa68c + 0x90) +
          (float)(int)(short)y_cur;
        y_cur = (int)ftmp2;
      }
      draw_string_set_font(font_index, -1, 0, 0, color);
      if (pi14[0x21] == -1) {
        pi10 = pi14 + 1;
        goto LAB_000d5e5d;
      } else {
        char item_variant;
        int item_tag;
        item_variant = *(char *)((int)pi14 + 0x8a);
        if (item_variant == (char)-1) {
          item_variant = (char)(1 < (short)pi14[0x22]);
        }
        item_tag = (int)tag_get(0x6974656d, pi14[0x21]);
        pi10 = (int *)hud_get_item_string(
          (int)(short)(*(short *)(item_tag + 0x180) + (short)item_variant));
        if ((*(char *)((int)pi14 + 0x8a) != (char)-1 ||
             item_variant == '\0') &&
            (short)pi14[0x22] == 0) {
          goto LAB_000d5e5d;
        }
        {
          short max_count;
          max_count = *(short *)(item_tag + 0x188);
          if (max_count < 2) {
            max_count = 1;
          }
          usprintf(format_buf, (const wchar_t *)pi10,
                   (int)(short)pi14[0x22] / (int)max_count);
          rasterizer_draw_string(rect_b, 0, 0, 0, (unsigned short *)format_buf);
        }
      }
      goto LAB_000d5e65_skip;
    LAB_000d5e5d:
      rasterizer_draw_string(rect_b, 0, 0, 0, (unsigned short *)pi10);
    LAB_000d5e65_skip:
      slot_offset = game_ticks - *pi14;
      {
        char alive;
        alive =
          (char)((float)slot_offset < (*(float *)(*(int *)0x5aa68c + 0x6c) +
                                       *(float *)(*(int *)0x5aa68c + 0x68)) *
                                        30.0f);
        *(char *)((int)pi14 + 0x82) = alive;
        if (!alive) {
          *pi14 = -1;
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

  found = -1;
  if (*(int *)0x46bd0c != 0) {
    for (i = 0; (int)i < *(int *)(*(int *)0x46bd0c + 0x160); i++) {
      element = (int)tag_block_get_element((void *)(*(int *)0x46bd0c + 0x160),
                                           (int)i, 0x68);
      if (csstricmp(param_1, (const char *)element) == 0) {
        found = i;
        break;
      }
    }
  }
  if (found == -1) {
    error(2, "could not find nav point");
  }
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
      /* Pass the FULL 32-bit object handle. The original pushes EDI whole
       * (000d61c7: PUSH EDI); a prior lift truncated it via (int)(short),
       * dropping the datum salt (e.g. 0xeaab000d -> 0x0000000d). For object/
       * enemy navs (nav_type 0/1) that corrupts the tracked handle, so
       * nav_point_visibility_test's hit-object compare (collision_result+0x38
       * == handle) never matches and the nav never hides when you board or
       * look at the tracked object. NOTE: the sibling FUN_000d6280 genuinely
       * truncates in the original (000d62ba: MOVSX EAX,DI), so its (short)
       * cast is faithful and is deliberately left untouched. */
      FUN_000d6030(iter[2], (short)type_value, nav_type, object_handle, extra);
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

/* FUN_000d6490 (0xd6490) — set object nav point for a unit's player.
 *
 * param_4 is a FLOAT, proven by the only XBE caller, the HaloScript handler
 * 0xc2cd0: it materializes the argument with `FLD dword [EAX+0xc]` and passes
 * it via the MSVC push-then-fstp idiom (`PUSH ECX ; FSTP dword [ESP]`), which
 * only happens when the callee's parameter is float.  It was previously
 * declared `int` here, which would have compiled that call site to an integer
 * PUSH and silently changed the argument's codegen.
 *
 * The value is forwarded to FUN_000d6030's `extra` parameter, which is still
 * declared `int` and stores it raw (`*(int *)(entry + 2) = extra`) into the
 * nav-point record — so `extra` is really a float too, but retyping it would
 * turn two bit-exact MOV stores in 0xd6030 into x87 FLD/FSTP pairs and touch
 * its three other callers.  Forwarding through `*(int *)&param_4` reads this
 * function's own incoming frame slot as a dword, which is bit-exact and emits
 * the same `PUSH [EBP+0x14]` as before, leaving 0xd6030 untouched. */
void FUN_000d6490(int param_1, int unit_handle, short param_3, float param_4)
{
  int player_index;

  player_index = player_index_from_unit_index(unit_handle);
  if (player_index != -1) {
    FUN_000d6030(player_index, (short)param_1, 0, (int)param_3,
                 *(int *)&param_4);
  }
}

/* FUN_000d64c0 (0xd64c0) — set enemy nav point for a unit's player.
 *
 * param_4 is a float for the same reason as its sibling 0xd6490 above: the
 * 0xc2d20 call site materializes it with the MSVC push-then-fstp idiom, which
 * only happens when the callee's parameter is float.  It is forwarded to
 * FUN_000d6030's still-`int` `extra` parameter through `*(int *)&param_4`,
 * reading this function's own incoming frame slot as a dword — bit-exact, and
 * it emits the same `PUSH [EBP+0x14]` as the previous `int` declaration. */
void FUN_000d64c0(int param_1, int unit_handle, int param_3, float param_4)
{
  int player_index;

  player_index = player_index_from_unit_index(unit_handle);
  if (player_index != -1) {
    FUN_000d6030(player_index, (short)param_1, 1, param_3, *(int *)&param_4);
  }
}

/* FUN_000d64f0 (0xd64f0) — clear object nav point for a unit's player.
 * param_2 is a 16-bit object handle: the original sign-extends it
 * (MOVSX EDI,word[EBP+0xc]) before the full 32-bit compare in FUN_000d6320,
 * so it must be a short here, not an int. */
void FUN_000d64f0(int param_1, short param_2)
{
  int player_index;

  player_index = player_index_from_unit_index(param_1);
  if (player_index != -1) {
    FUN_000d6320(player_index, 0, (int)param_2);
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
  char collision_result[80];
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

  if (FUN_0014df70(0xc2ad, param_2, direction, unit_handle,
                   (short *)collision_result) == '\0' ||
      (*(short *)collision_result == 3 &&
       *(int *)(collision_result + 0x38) == param_4)) {
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
  float f1;
  float f2;
  float f3;
  float *pf4;
  char c5;
  short s7;
  int i8;
  int i9;
  unsigned int u10;
  int u11;
  unsigned char b12;
  unsigned int u13;
  int l_2ac[128];
  char text_element[36];
  char text_pos_buf[84];
  int l_34;
  float l_30[3];
  float l_24[3] = { 0, 0, 0 };
  float distance;
  float l_14;
  float screen_pos[2] = { 0, 0 };
  float l_8;
  short screen_coords[2];
  l_34 = FUN_000d1540();
  csmemset(l_2ac, 0x62, 0x200);
  i8 = (int)tag_block_get_element((void *)(*(int *)0x46bd0c + 0x160),
                                     (int)param_3, 0x68);
  pf4 = param_2;
  l_30[0] = *param_2;
  l_30[1] = param_2[1];
  l_30[2] = param_2[2];
  i9 = local_player_get_player_index(param_1);
  if (i9 == -1) {
    u11 = -1;
  } else {
    u11 = local_player_get_player_index(param_1);
    i9 = (int)datum_get(*(data_t **)0x5aa6d4, u11);
    u11 = *(int *)(i9 + 0x34);
  }
  unit_set_seat_state(u11, l_24);
  distance = sqrtf((*pf4 - l_24[0]) * (*pf4 - l_24[0]) +
                   (pf4[1] - l_24[1]) * (pf4[1] - l_24[1]) +
                   (pf4[2] - l_24[2]) * (pf4[2] - l_24[2]));
  if (distance > *(float *)0x254cc0) {
    l_14 = 0.5f;
  } else {
    l_14 =
      (float)pow((double)(*(float *)0x2533c8 - distance * *(float *)0x253d48),
                 *(double *)0x281e18) +
      *(float *)0x253398;
  }
  matrix_transform_point((float *)0x5065b4, l_30, l_30);
  s7 = param_4;
  if (s7 == 1 || (c5 = render_camera_view_to_screen(
                       (int *)0x506550, (int *)0x5065a4, l_30, screen_pos),
                     c5 == '\0')) {
    screen_pos[0] = l_30[0];
    screen_pos[1] = -l_30[1];
    s7 = 1;
  } else {
    screen_pos[0] =
      screen_pos[0] -
      (float)(((int)*(short *)0x506582 - (int)*(short *)0x50657e) / 2 +
              (int)*(short *)0x50657e);
    screen_pos[1] =
      screen_pos[1] -
      (float)(((int)*(short *)0x506580 - (int)*(short *)0x50657c) / 2 +
              (int)*(short *)0x50657c);
  }
  l_8 = 0.0f;
  f1 = ((float)((int)*(short *)0x50658a - (int)*(short *)0x506586) -
           (*(float *)(*(int *)0x46bd0c + 300) +
            *(float *)(*(int *)0x46bd0c + 0x128))) *
          *(float *)0x253398;
  f3 = ((float)((int)*(short *)0x506588 - (int)*(short *)0x506584) -
           (*(float *)(*(int *)0x46bd0c + 0x124) +
            *(float *)(*(int *)0x46bd0c + 0x120))) *
          *(float *)0x253398;
  f2 = f3 * f1;
  f1 = f1 * screen_pos[1];
  if (s7 == 1 ||
      f2 * f2 <=
        f3 * screen_pos[0] * (f3 * screen_pos[0]) + f1 * f1) {
    s7 = 1;
    f1 =
      sqrtf((f2 * f2) /
            (f3 * screen_pos[0] * (f3 * screen_pos[0]) + f1 * f1));
    screen_pos[0] = screen_pos[0] * f1;
    screen_pos[1] = f1 * screen_pos[1];
    if ((*(unsigned char *)(i8 + 0x4c) & 1) == 0) {
      l_8 = -(float)atan2((double)screen_pos[0], (double)screen_pos[1]);
    }
  }
  screen_pos[0] =
    (float)(((int)*(short *)0x506582 - (int)*(short *)0x50657e) / 2) +
    screen_pos[0];
  screen_pos[1] =
    (float)(((int)*(short *)0x506580 - (int)*(short *)0x50657c) / 2) +
    screen_pos[1];
  if (s7 == -1) {
    display_assert("waypoint_type!=NONE",
                   "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x267, 1);
    system_exit(-1);
  }
  i9 = 0;
  u11 = 0;
  FUN_000d16a0(*(int *)(*(int *)0x46bd0c + 0x15c),
               *(short *)(i8 + 0x34 + s7 * 2), 0, &i9, &u11);
  if (i9 != 0 &&
      (int)xbox_texture_cache_get_hardware_format((void *)i9, 0, 1) != 0) {
    {
      int alpha_round = FUN_000d1c50(*(float *)(i8 + 0x2c));
      int alpha_scaled = alpha_round * 0xff;
      if (alpha_scaled < 0) {
        b12 = 0;
      } else if (alpha_scaled > 0xff) {
        b12 = 0xff;
      } else {
        b12 = (unsigned char)(-(char)FUN_000d1c50(*(float *)(i8 + 0x2c)));
      }
    }
    pixel32_to_real_rgb_color(*(unsigned int *)(i8 + 0x28), l_24);
    f1 = *(float *)0x2533c8 - *(float *)(i8 + 0x30);
    f2 = *(float *)0x2533c0;
    if (*(float *)0x2533c0 <= f1) {
      f2 = f1;
      if (*(float *)0x2533c8 < f1) {
        f2 = *(float *)0x2533c8;
      }
    }
    l_24[0] = f2 * l_24[0];
    f2 = *(float *)0x2533c0;
    if (*(float *)0x2533c0 <= f1) {
      f2 = f1;
      if (*(float *)0x2533c8 < f1) {
        f2 = *(float *)0x2533c8;
      }
    }
    l_24[1] = f2 * l_24[1];
    f2 = *(float *)0x2533c0;
    if (*(float *)0x2533c0 <= f1) {
      f2 = f1;
      if (*(float *)0x2533c8 < f1) {
        f2 = *(float *)0x2533c8;
      }
    }
    l_24[2] = f2 * l_24[2];
    u13 = (unsigned int)b12 << 0x18;
    u10 = FUN_000d1dd0(l_24);
    screen_coords[0] = (short)screen_pos[0];
    screen_coords[1] = (short)screen_pos[1];
    FUN_000d3200(i9, 4, screen_coords, u11, l_14, l_8,
                 u10 | u13, 0);

    if (s7 != 1) {
      short *text_pos_ptr;
      short text_x, text_y;
      int tmp_int;
      float pow_val;
      int text_value;

      /* Original 0xd6ac7: FLD [EBP-0x14] -- the sqrtf distance slot -- times
       * 0x281e00 (world units -> meters). The seat/color buffer l_24 ends
       * at [EBP-0x18]; the distance is a separate local, not l_24[3]. */
      distance = distance * *(float *)0x281e00;
      csmemset(text_element, 0, 0x24);
      csmemset(text_pos_buf, 0, 0x54);
      *(short *)text_element = 0;
      /* original writes the 5 color/style fields into text_pos_buf (arg3), NOT
       * text_element (arg2, whose only write is the first-word zero above).
       * Disasm: stores at
       * [EBP-0x60]/[EBP-0x5c]/[EBP-0x40]/[EBP-0x3f]/[EBP-0x3e] =
       * text_pos_buf+0x24/+0x28/+0x44/+0x45/+0x46 (base EBP-0x84). */
      *(unsigned int *)(text_pos_buf + 0x24) = FUN_000d1dd0(l_24) | u13;
      *(unsigned int *)(text_pos_buf + 0x28) = FUN_000d1dd0(l_24) | u13;
      text_pos_buf[0x44] = 3;
      text_pos_buf[0x46] = 1;
      text_pos_buf[0x45] = 5;

      {
        short scr_x = (short)screen_pos[0];
        short scr_y = (short)screen_pos[1];
        float bmp_w;
        float bmp_h;

        tmp_int = (int)*(short *)(i9 + 4);
        bmp_w = (*(float *)(u11 + 4) - *(float *)u11);
        text_x = (short)((float)(int)scr_x + bmp_w * (float)tmp_int *
                                               *(float *)0x253398 * l_14 *
                                               *(float *)0x281dfc);

        tmp_int = (int)*(short *)(i9 + 6);
        bmp_h = (*(float *)(u11 + 0xc) - *(float *)(u11 + 0x8));
        text_y = (short)((float)(int)scr_y + bmp_h * (float)tmp_int *
                                               *(float *)0x253398 * l_14 *
                                               *(float *)0x281df8);

        text_x = text_x + (*(short *)0x50657e - *(short *)0x506586);
        text_y = text_y + (*(short *)0x50657c - *(short *)0x506584);
      }

      text_pos_ptr = (short *)text_pos_buf;
      text_pos_ptr[0] = text_x;
      text_pos_ptr[1] = text_y;

      pow_val = (float)pow(*(double *)0x281df0, *(double *)0x281de8);
      {
        float fmod_input = (float)fabs((double)(pow_val * distance));
        /* clang -mno-sse compiles fmod() to FPREM1 (IEEE remainder, result
         * can be negative); the original 0xd6bdc uses FPREM (truncating).
         * A negative tenths digit prints a stray '-' and the displayed
         * digit count flickers. x87_fmod keeps the result in [0, pow_val). */
        float fmod_result = x87_fmod(fmod_input, (double)pow_val);
        text_value = (int)fmod_result;
      }

      {
        int rounded = FUN_000d1c50(distance);
        FUN_000d3860((short)param_1, text_element, text_pos_buf, rounded,
                     text_value, 0, 0, 0.0f);
      }
    }
  }
  s7 = 0x7f;
  do {
    if (l_2ac[(int)s7] != 0x62626262)
      goto LAB_000d6c45;
    s7 = s7 - 1;
  } while (-1 < s7);
  s7 = -1;
LAB_000d6c45:
  i8 = FUN_000d1540();
  if (l_34 != i8) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x2a3, 1);
    system_exit(-1);
  }
  if (s7 != -1) {
    display_assert(
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)s7),
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
  unsigned short *pu6;
  int i2;
  short s5;
  int loop_count;
  float position[3];
  /* Save the local_player_index up front (original: u1 = param_1 @0xd6cc6).
   * The case-1 (object waypoint) branch below reuses param_1's stack slot as a
   * throwaway scratch for FUN_0001aae0's radius out-param, so param_1 itself is
   * clobbered. The original keeps the real index in u1 and uses it for
   * FUN_000d6660 and game_engine_render_nav_points; the prior lift dropped that
   * save and reused the clobbered param_1, tripping the players.c#133
   * local_player_index range assert on object-tracking nav points. */
  int local_player_index = param_1;

  if ((short)param_1 == -1)
    goto done;
  if (local_player_get_player_index((short)param_1) == -1)
    goto done;
  player = local_player_get_player_index((short)local_player_index);
  player = (int)datum_get(*(data_t **)0x5aa6d4, player);
  unit_handle = *(int *)(player + 0x34);
  if (unit_handle == -1)
    goto done;
  if (*(int *)(*(int *)0x46bd0c + 0x15c) == -1)
    goto done;

  nav_data = hud_get_nav_point_data((short)local_player_index);
  pu6 = (unsigned short *)(nav_data + 8);
  loop_count = 4;
  do {
    if (pu6[-4] == 0xffff || *(int *)pu6 == -1 ||
        ((*((unsigned short *)pu6 - 3) & 0xf) == 0xf)) {
      *(unsigned char *)((char *)pu6 - 6) |= 0xf;
    } else {
      s5 = (short)(*((unsigned short *)pu6 - 3) << 12) >> 12;
      switch (s5) {
      case 0:
        i2 = (int)global_scenario_get();
        i2 = (int)tag_block_get_element((void *)(i2 + 0x4e4),
                                           *(int *)pu6, 0x5c);
        position[0] = *(float *)(i2 + 0x24);
        position[1] = *(float *)(i2 + 0x28);
        position[2] = *(float *)(i2 + 0x2c);
        break;
      case 1:
        i2 = (int)object_try_and_get_and_verify_type(*(int *)pu6, -1);
        if (i2 == 0)
          goto skip;
        FUN_0001aae0(*(int *)pu6, position, (float *)&param_1);
        break;
      case 2:
        game_engine_get_goal_position((int *)position, (short)*(int *)pu6);
        break;
      default:
        display_assert("!\"unreachable\"",
                       "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x2d5,
                       1);
        system_exit(-1);
        break;
      }
      position[2] = position[2] + *(float *)((char *)pu6 - 4);
      FUN_000d6660(
        local_player_index, position, (short)pu6[-4],
        (short)((unsigned short)(*(unsigned char *)((char *)pu6 - 6))
                << 8) >>
          12);
    }
  skip:
    pu6 = pu6 + 6;
    loop_count = loop_count - 1;
  } while (loop_count != 0);

done:
  game_engine_render_nav_points(local_player_index);
}

/* nav_point_update (0xd6e50)
 * Update nav point visibility flags for a player. */
void FUN_000d6e50(int param_1)
{
  int nav_data;
  int unit_handle;
  int player;
  int i2;
  unsigned short *pu6;
  short s5;
  int loop_count;
  int l_234[128];
  float l_28[3];
  float l_1c[4];
  float target_pos[3];
  int l_18;
  short s;
  int obj_handle;
  char vis;

  l_18 = FUN_000d1540();
  csmemset(l_234, 0x62, 0x200);
  nav_data = hud_get_nav_point_data((short)param_1);
  player = local_player_get_player_index((short)param_1);
  if (player == -1) {
    unit_handle = -1;
  } else {
    player = local_player_get_player_index((short)param_1);
    player = (int)datum_get(*(data_t **)0x5aa6d4, player);
    unit_handle = *(int *)(player + 0x34);
  }
  pu6 = (unsigned short *)(nav_data + 2);
  loop_count = 4;
  do {
    obj_handle = -1;
    if (pu6[-1] == 0xffff || *(int *)(pu6 + 3) == -1 ||
        (*pu6 & 0xf) == 0xf) {
      *(unsigned char *)pu6 = *(unsigned char *)pu6 | 0xf;
    } else if (unit_handle != -1) {
      unit_get_head_position(unit_handle, l_28);
      s5 = (short)(*pu6 << 12) >> 12;
      switch (s5) {
      case 0:
        i2 = (int)global_scenario_get();
        i2 = (int)tag_block_get_element((void *)(i2 + 0x4e4),
                                           *(int *)(pu6 + 3), 0x5c);
        target_pos[0] = *(float *)(i2 + 0x24);
        target_pos[1] = *(float *)(i2 + 0x28);
        target_pos[2] = *(float *)(i2 + 0x2c);
        break;
      case 1:
        i2 =
          (int)object_try_and_get_and_verify_type(*(int *)(pu6 + 3), -1);
        obj_handle = *(int *)(pu6 + 3);
        if (i2 == 0 || (*(unsigned char *)(i2 + 0xb6) & 4) != 0) {
          *(unsigned char *)pu6 = *(unsigned char *)pu6 | 0xf;
          pu6[3] = 0xffff;
          pu6[4] = 0xffff;
          pu6[-1] = 0xffff;
          goto next;
        }
        FUN_0001aae0(obj_handle, target_pos, l_1c);
        break;
      case 2:
        game_engine_get_goal_position((int *)target_pos,
                                      (unsigned short)pu6[3]);
        break;
      }
      target_pos[2] = target_pos[2] + *(float *)(pu6 + 1);
      {
        vis = (char)FUN_000d6550(param_1, l_28, target_pos, obj_handle);
        {
          int shifted = (int)vis << 4;
          *pu6 =
            *pu6 ^
            (unsigned short)(((unsigned char)shifted ^ (unsigned char)*pu6) &
                             0xf0);
        }
      }
    }
  next:
    pu6 = pu6 + 6;
    loop_count = loop_count - 1;
  } while (loop_count != 0);
  s = 0x7f;
  do {
    if (l_234[(int)s] != 0x62626262)
      goto check;
    s = s - 1;
  } while (s >= 0);
  s = -1;
check:
  i2 = FUN_000d1540();
  if (l_18 != i2) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x1f0, 1);
    system_exit(-1);
  }
  if (s != -1) {
    display_assert(
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)s),
      "c:\\halo\\SOURCE\\interface\\hud_nav_points.c", 0x1f0, 1);
    system_exit(-1);
  }
}

/* FUN_000d7080 (0xd7080)
 * Iterate all local players and update nav point rendering. */
void FUN_000d7080(void)
{
  int result;
  short s;

  result = (int)local_player_get_next(-1);
  s = (short)result;
  while (s != -1) {
    FUN_000d6e50(result);
    result = (int)local_player_get_next((short)result);
    s = (short)result;
  }
}

/* hud_sounds_update (0xd70b0)
 * Update HUD sound effects based on state flags. */
void FUN_000d70b0(short param_1, unsigned int param_2, int *param_3,
                  int param_4, unsigned short *param_5)
{
  int i1;
  short s2;
  int *pi3;
  int i6;

  i6 = 0;
  s2 = 0;
  if (0 < *param_3) {
    do {
      pi3 = (int *)tag_block_get_element((void *)param_3, i6, 0x38);
      if ((param_2 & pi3[4]) != 0) {
        switch (*pi3) {
        case 0x6c736e64:
          if (*(int *)(param_4 + i6 * 4) == -1) {
            i1 = unattached_looping_sound_start(pi3[3], -1, pi3[5]);
            goto store_sound;
          }
          break;
        case 0x736e6421:
          i1 = *(int *)(param_4 + i6 * 4);
          if (i1 == -1 || !((unsigned int)*param_5 & (1 << i6))) {
            if (i1 != -1) {
              sound_stop_impulse(i1);
            }
            i1 = sound_impulse_start(pi3[3], *(float *)(pi3 + 5));
          store_sound:
            *(int *)(param_4 + i6 * 4) = i1;
          }
          break;
        default:
          display_assert("!\"unreachable\"",
                         "c:\\halo\\SOURCE\\interface\\hud_sounds.c", 0x2f, 1);
          system_exit(-1);
          break;
        }
        *param_5 = *param_5 | (unsigned short)(1 << i6);
      } else {
        i1 = *(int *)(param_4 + i6 * 4);
        if (i1 != -1) {
          switch (*pi3) {
          case 0x6c736e64:
            unattached_looping_sound_stop(i1);
            break;
          case 0x736e6421:
            break;
          default:
            display_assert("!\"unreachable\"",
                           "c:\\halo\\SOURCE\\interface\\hud_sounds.c", 0x40,
                           1);
            system_exit(-1);
            break;
          }
          *(int *)(param_4 + i6 * 4) = -1;
          *param_5 = *param_5 & ~(unsigned short)(1 << i6);
        }
      }
      s2 = s2 + 1;
      i6 = (int)s2;
    } while (i6 < *param_3);
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
  char c1;
  short s2;
  int *pf3;
  int i4;
  int *pu5;
  int i7;
  unsigned int u8;

  pf3 = (int *)FUN_000d7280(*(short *)(param_1 + 2));
  i4 = *(int *)(param_1 + 0x34);
  if (i4 == -1) {
    i4 = pf3[7];
  }
  pu5 = (int *)object_try_and_get_and_verify_type(i4, 3);
  if (pu5 != (int *)0) {
    i7 = (int)tag_get(0x756e6974, *pu5);
    s2 = local_player_count();
    i7 = FUN_001a6820(i7, 1 < s2);
    if (i7 != -1) {
      i7 = (int)tag_get(0x756e6869, i7);
      u8 = 0;
      if ((*(unsigned char *)((int)pu5 + 4) & 4) != 0 ||
          !(*(float *)((int)pu5 + 0x90) > *(float *)0x2533c0)) {
        pf3[7] = -1;
      } else if (param_2 != '\0' &&
                 (c1 = cinematic_in_progress(), c1 == '\0')) {
        if (*(int *)pf3 != (int)0xbf800000) {
          i4 = local_player_get_player_index(*(short *)(param_1 + 2));
          c1 = game_engine_has_shield(i4);
          if (c1 != '\0' &&
              (*(unsigned int *)(*(int *)0x46bd20 + 0x160) & 4) == 0) {
            u8 = (*(unsigned short *)((int)pu5 + 0xb6) >> 0xc) & 1;
            if (*(float *)pf3 > *(float *)((int)pu5 + 0x94)) {
              u8 = u8 | 2;
            } else {
              u8 = u8 & ~2;
            }
            if (*(float *)((int)pu5 + 0x94) < *(float *)0x25337c &&
                *(float *)((int)pu5 + 0x94) > *(float *)0x2533c0) {
              u8 = u8 | 4;
            } else {
              u8 = u8 & ~4;
            }
            if (*(float *)((int)pu5 + 0x94) <= *(float *)0x2533c0) {
              u8 = u8 | 8;
            } else {
              u8 = u8 & ~8;
            }
          }
        }
        if ((*(unsigned int *)(*(int *)0x46bd20 + 0x160) & 1) == 0) {
          if (*(float *)((int)pu5 + 0x90) < *(float *)0x25337c) {
            u8 = u8 | 0x10;
          } else {
            u8 = u8 & ~0x10;
          }
          if ((*(unsigned char *)((int)pu5 + 0xb6) & 4) != 0) {
            u8 = u8 | 0x20;
          } else {
            u8 = u8 & ~0x20;
          }
          if (*(float *)(pf3 + 1) > *(float *)((int)pu5 + 0x90) &&
              *(float *)(pf3 + 1) - *(float *)((int)pu5 + 0x90) <
                *(float *)0x281e94) {
            u8 = u8 | 0x40;
          } else {
            u8 = u8 & ~0x40;
          }
          if (*(float *)(pf3 + 1) - *(float *)((int)pu5 + 0x90) >=
              *(float *)0x281e94) {
            u8 = u8 | 0x80;
          } else {
            u8 = u8 & ~0x80;
          }
        }
      }
      FUN_000d70b0(*(short *)(param_1 + 2), u8, (int *)(i7 + 0x3c0),
                   (int)(pf3 + 10), (unsigned short *)(pf3 + 9));
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
  char c1;
  int i2;
  short s7;
  float *pf4;
  int l_20c[128];
  int l_c;

  l_c = FUN_000d1540();
  csmemset(l_20c, 0x62, 0x200);
  i2 = local_player_get_player_index(player_handle);
  if (i2 == -1)
    goto LAB_000d794f;
  i2 = local_player_get_player_index(player_handle);
  i2 = (int)datum_get(*(data_t **)0x5aa6d4, i2);
  if (*(int *)(i2 + 0x34) == -1)
    goto LAB_000d794f;
  i2 = (int)object_get_and_verify_type(*(int *)(i2 + 0x34), 3);
  pf4 = (float *)FUN_000d7280((short)player_handle);
  if (pf4[1] == -1.0f) {
    pf4[1] = *(float *)(i2 + 0x90);
  }
  if (*pf4 == -1.0f) {
    *pf4 = *(float *)(i2 + 0x94);
  }
  if (*pf4 > *(float *)(i2 + 0x94)) {
    if (pf4[2] < 0.0f || pf4[2] > 1.0f) {
      pf4[3] = (float)game_time_get();
    }
    if (game_time_get() - (int)pf4[3] < 15) {
      pf4[2] = 0.0f;
    } else {
      *pf4 = *(float *)(i2 + 0x94);
      pf4[2] += (float)(game_time_get() - (int)pf4[3]) * *(float *)0x2546a4;
      pf4[3] = (float)game_time_get();
    }
  } else {
    if (*pf4 < *(float *)(i2 + 0x94)) {
      *pf4 = *(float *)(i2 + 0x94);
      pf4[2] = -1.0f;
    } else {
      *pf4 = *(float *)(i2 + 0x94);
      if (pf4[2] > 0.0f) {
        pf4[2] += (float)(game_time_get() - (int)pf4[3]) * *(float *)0x2546a4;
      }
    }
    pf4[3] = (float)game_time_get();
  }
LAB_000d794f:
  c1 = cinematic_in_progress();
  if (c1 != '\0') {
    i2 = local_player_get_player_index(player_handle);
    if (i2 != -1) {
      i2 = (int)datum_get(*(data_t **)0x5aa6d4, i2);
      FUN_000d7560(i2, **(char **)0x46bd10);
    }
  }
  s7 = 0x7f;
  do {
    if (l_20c[(int)s7] != 0x62626262)
      goto LAB_000d79a8;
    s7 = s7 - 1;
  } while (-1 < s7);
  s7 = -1;
LAB_000d79a8:
  i2 = FUN_000d1540();
  if (l_c != i2) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x201, 1);
    system_exit(-1);
  }
  if (s7 != -1) {
    display_assert(
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)s7),
      "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x201, 1);
    system_exit(-1);
  }
}

/* hud_render_damage_indicators (0xd7a20)
 * Render motion sensor direction indicators for incoming damage. */
void FUN_000d7a20(int param_1)
{
  int i3;
  int i5;
  int i6;
  float f7;
  short s2;
  unsigned char l_18[4];
  int l_14;
  short l_10[2];
  int l_c;
  int l_8;
  int tmp;
  float pos_x;
  float pos_y;
  short *pESI;

  if ((short)param_1 == -1) {
    return;
  }
  i3 = local_player_get_player_index(param_1);
  if (i3 == -1) {
    i3 = 0;
  } else {
    i3 = local_player_get_player_index(param_1);
    i3 = (int)datum_get(*(data_t **)0x5aa6d4, i3);
    i3 = *(int *)(i3 + 0x34);
  }
  i5 = (int)object_try_and_get_and_verify_type(i3, 3);
  if (i5 == 0) {
    player_effect_clear_damage_indicators(param_1);
    return;
  }
  i3 = *(int *)0x46bd0c;
  pESI = (short *)(i3 + 0x310);
  s2 = local_player_count();
  f7 = FUN_000d1690(1 < s2);
  player_effect_get_damage_indicators(param_1, l_18);
  l_14 = 4;
  i5 = 0;
  do {
    if (l_18[i5] != 0 && l_18[i5] < 0x1e) {
      switch (i5) {
      case 0:
        pos_x = (float)((int)pESI[0] + (int)*(short *)0x506584);
        tmp = ((int)*(short *)0x506582 + (int)*(short *)0x50657e) / 2;
        param_1 = 0x40490fdb;
        pos_y = (float)tmp;
        break;
      case 1:
        pos_x = (float)((int)pESI[2] + (int)*(short *)0x506586);
        tmp = ((int)*(short *)0x506580 + (int)*(short *)0x50657c) / 2;
        param_1 = 0x3fc90fdb;
        pos_y = (float)tmp;
        break;
      case 2:
        pos_x = (float)((int)*(short *)0x506588 - (int)pESI[1]);
        tmp = ((int)*(short *)0x506582 + (int)*(short *)0x50657e) / 2;
        param_1 = 0;
        pos_y = (float)tmp;
        break;
      case 3:
        pos_x = (float)((int)*(short *)0x50658a - (int)pESI[3]);
        tmp = ((int)*(short *)0x506580 + (int)*(short *)0x50657c) / 2;
        param_1 = 0x4096cbe4;
        pos_y = (float)tmp;
        break;
      default:
        display_assert("!\"unreachable\"",
                       "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x400, 1);
        system_exit(-1);
      }

      pos_x = pos_x - (float)(int)*(short *)0x50657e;
      pos_y = pos_y - (float)(int)*(short *)0x50657c;

      i6 = *(int *)((char *)pESI + 0x34);
      s2 = local_player_count();
      if (s2 < 2) {
        s2 = *(short *)((char *)pESI + 0x38);
      } else {
        s2 = *(short *)((char *)pESI + 0x3a);
      }
      l_8 = 0;
      l_c = 0;
      FUN_000d16a0(i6, s2, 0, &l_8, &l_c);
      if (l_8 != 0 && (int)xbox_texture_cache_get_hardware_format(
                            (void *)l_8, 0, 1) != 0) {
        l_10[0] = (short)pos_x;
        l_10[1] = (short)pos_y;
        FUN_000d3200(l_8, 4, l_10, l_c, f7, *(float *)&param_1,
                     *(int *)((char *)pESI + 0x3c), 0);
      }
    }
    i5 = i5 + 1;
    l_14 = l_14 - 1;
  } while (l_14 != 0);
}

/* FUN_000d7cd0 (0xd7cd0)
 * Subtract damage amount from a player's HUD damage indicator. */
void FUN_000d7cd0(int player_handle, float param_2)
{
  int player;
  float *pf2;

  player = (int)datum_get(*(data_t **)0x5aa6d4, player_handle);
  if (*(short *)(player + 2) != -1) {
    pf2 = (float *)FUN_000d7280(*(short *)(player + 2));
    *pf2 = *pf2 - param_2;
  }
}

/* FUN_000d7d10 (0xd7d10)
 * Iterate local players and update unit HUD for each. */
void FUN_000d7d10(void)
{
  int result;
  short s;

  result = (int)local_player_get_next(-1);
  s = (short)result;
  while (s != -1) {
    FUN_000d7800(s);
    result = (int)local_player_get_next(s);
    s = (short)result;
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
  /*
   * In the original MSVC frame this is &l_24 (base EBP-0x20); only
   * fraction_slots[0] (the shield fraction, unit_ptr+0x2f4) is ever stored.
   * The overlay loop reads fraction_slots[*ps12], where *ps12 is the
   * overlay_type. That read is reached only via the (full_shield & (1<<type))
   * branch, and full_shield is forced to {0,1} at 0x7f91/0x7f9a
   * (= unit_ptr+0x2f0 == 1.0f). A {0,1} value ANDed with (1<<type) is nonzero
   * only for type==0, so the indexed read is structurally always
   * fraction_slots[0]; slots [1..3] are dead on the read path and the latent
   * OOB for overlay_type>=4 cannot occur here. The discrete float[4] is
   * therefore faithful. (See FUN_000d7d40 disasm 0x87d2-0x87e1.)
   */
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
  short l_130[2];
  int overlay_colors[6];
  unsigned int flags;
  int i7, i8, i13;
  int flash_param_int;
  int clamp_a, clamp_b;
  short s4, s5;
  char c3;
  unsigned char b;
  float f1, f2, f14;
  float l_44;
  float l_34;
  float l_48;
  float meter_scale;
  short *ps12;
  short *ps22;
  int tag_ref_result;
  int *meter_src_ptr;
  int i;
  void *l_78_buf[1];
  float *pf6;
  unsigned char *unit_tag_data;

  canary_cookie = FUN_000d1540();
  csmemset(canary_buf, 0x62, 0x200);

  if (*(short *)(param_1 + 2) != *(short *)0x506548) {
    display_assert("player->local_player_index==render.local_player_index",
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
  pf6 = (float *)FUN_000d7280((short)local_player_idx);

  handle_slots[0] = *(int *)(param_1 + 0x34);
  csmemset(handle_slots + 1, 0, 17 * 4);

  s4 = local_player_count();
  tag_indices[0] = FUN_001a6820((int)unit_tag_data, 1 < s4);

  csmemset(tag_indices + 1, 0, 17 * 4);

  slot_count = 1;

  if (*(int *)((char *)pf6 + 0x1c) == (int)0xFFFFFFFF) {
    csmemset((char *)pf6 + 0x22, 0xFF, 2);
    *(float *)pf6 = -1.0f;
    *(float *)((char *)pf6 + 0x04) = -1.0f;
    *(float *)((char *)pf6 + 0x08) = -1.0f;
    *(int *)((char *)pf6 + 0x14) = -1;
    *(int *)((char *)pf6 + 0x18) = -1;
    *(int *)((char *)pf6 + 0x1c) = -1;
  }

  *(int *)((char *)pf6 + 0x1c) = *(int *)(param_1 + 0x34);

  parent_handle = unit_ptr[0x33];

  if (parent_handle != -1 && *(short *)((char *)unit_ptr + 0x2a0) != -1) {
    int *vehicle_ptr;
    int vehicle_tag;
    int seat_element;

    vehicle_ptr = (int *)object_get_and_verify_type(parent_handle, 3);
    vehicle_tag = (int)tag_get(0x756e6974, *vehicle_ptr);
    seat_element = (int)tag_block_get_element(
      (void *)(vehicle_tag + 0x2e4), (int)*(short *)((char *)unit_ptr + 0x2a0),
      0x11c);
    unit_tag_data = (unsigned char *)seat_element;

    FUN_000d7280((short)local_player_idx);
    s4 = local_player_count();
    i8 = FUN_001a6820(vehicle_tag, 1 < s4);

    if ((*unit_tag_data & 4) != 0) {
      if (i8 != -1) {
        handle_slots[1] = parent_handle;
        tag_indices[1] = i8;
        slot_count = 2;
      }

      i13 = vehicle_ptr[0x32];
      while (i13 != -1 && slot_count < 0x12) {
        unsigned char *next_obj_data;
        int next_unit;

        next_obj_data =
          (unsigned char *)object_get_and_verify_type(i13, (int)0xFFFFFFFF);
        next_unit = (int)object_try_and_get_and_verify_type(i13, 3);
        if (next_unit != 0 && *(int *)(next_unit + 0xcc) == parent_handle &&
            *(short *)(next_unit + 0x2a0) != -1) {
          handle_slots[slot_count] = i13;
          s4 = local_player_count();
          i13 = FUN_001a6870(
            vehicle_tag, *(unsigned short *)(next_unit + 0x2a0), 1 < s4);
          tag_indices[slot_count] = i13;
          slot_count = slot_count + 1;
        }
        i13 = *(int *)(next_obj_data + 0xc4);
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
    unsigned int u16 = slot_count;
    unsigned int u18;

    while (u16 != 0) {
      u18 = u16 - 1;

      i13 =
        (int)object_try_and_get_and_verify_type(handle_slots[u16 - 1], 3);
      unit_data = i13;

      u16 = u18;
      if (i13 == 0 || tag_indices[u18] == -1)
        goto next_slot;

      i7 = (int)tag_get(0x756e6869, tag_indices[u18]);
      unhi_tag = i7;

      if (*(int *)(i7 + 0x54) != -1) {
        b = (unsigned char)((*(unsigned char *)(i13 + 0xb6)) >> 1) & 2;
        s4 = local_player_count();
        if (1 < s4) {
          b = b | 4;
        }
        FUN_000d3fe0(local_player_idx, (short *)i7, i7 + 0x24,
                     (unsigned int)b, (int)0xFFFFFFFF);
      }

      c3 = game_engine_has_shield(player_index);
      i8 = unit_data;
      {
        int i21 = unhi_tag;

        if (c3 != '\0' &&
            (i8 = i13, i21 = i7,
             (*(unsigned int *)(*(int *)0x46bd20 + 0x160) & 4) == 0)) {
          if (*(float *)(i13 + 0x94) < *(float *)0x25337c ||
              (*(unsigned int *)(*(int *)0x46bd20 + 0x160) & 8) != 0) {
            flags = 1;
          } else {
            flags = 0;
          }

          if ((*(unsigned char *)(unit_data + 0xb6) & 4) != 0) {
            flags = flags | 2;
          }

          s4 = local_player_count();
          if (1 < s4) {
            flags = flags | 4;
          }

          if (u18 == 0) {
            if ((flags & 1) == 0) {
              *(int *)((char *)pf6 + 0x10) = (int)0xFFFFFFFF;
            } else if (*(int *)((char *)pf6 + 0x10) == (int)0xFFFFFFFF) {
              *(int *)((char *)pf6 + 0x10) = game_time_get();
            }
          }

          if (*(int *)(i7 + 0x124) != -1) {
            int layer_idx;
            int saved_layer_idx;
            int meter_max;
            int *color_ptr;

            game_engine_running();

            if (u18 == 0) {
              l_44 = pf6[0];
            } else {
              l_44 = *(float *)(unit_data + 0x94);
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

            if (0 <= *(int *)0x2f66f0) {
              do {
                saved_layer_idx = layer_idx;

                l_34 = *(float *)(unit_data + 0x94) - (float)layer_idx;
                if (l_34 < *(float *)0x2533c0) {
                  l_34 = 0.0f;
                } else if (l_34 > *(float *)0x2533c8) {
                  l_34 = 1.0f;
                }

                f14 = l_44 - (float)layer_idx;
                f1 = *(float *)0x2533c0;
                if (*(float *)0x2533c0 <= f14) {
                  f1 = f14;
                  if (*(float *)0x2533c8 < f14) {
                    f1 = *(float *)0x2533c8;
                  }
                }

                f2 = l_34;
                if (l_34 < f1) {
                  f2 = f1;
                }

                if ((l_34 < *(float *)0x2533c0 !=
                     (l_34 == *(float *)0x2533c0)) &&
                    (f2 < *(float *)0x2533c0 !=
                     (f2 == *(float *)0x2533c0)))
                  break;

                if (f1 <= l_34) {
                  flash_param_int = (int)0xbf800000;
                } else {
                  flash_param_int = *(int *)((char *)pf6 + 0x08);
                }

                l_48 = (float)(short)meter_max * f2;
                clamp_a = (int)l_48;
                if (clamp_a < 0) {
                  clamp_a = 0;
                } else {
                  if ((int)l_48 > 0xff) {
                    clamp_a = 0xff;
                  } else {
                    clamp_a = (int)l_48;
                  }
                }

                {
                  float l_38_v = (float)(short)meter_max * l_34;
                  clamp_b = (int)l_38_v;
                  if (clamp_b < 0) {
                    clamp_b = 0;
                  } else {
                    if ((int)l_38_v > 0xff) {
                      clamp_b = 0xff;
                    } else {
                      clamp_b = (int)l_38_v;
                    }
                  }
                }

                {
                  int *meter_data_ptr;
                  widget_meter_data[13] = *color_ptr;
                  widget_meter_data[14] = *color_ptr;
                  if (layer_idx == 0) {
                    meter_data_ptr = meter_src_ptr;
                  } else {
                    meter_data_ptr = widget_meter_data;
                  }

                  FUN_000d3340(local_player_idx, unhi_tag, (int)meter_data_ptr,
                               clamp_b, clamp_a, flags, flash_param_int,
                               l_34);
                }

                color_ptr = color_ptr + 1;
                layer_idx = saved_layer_idx + 1;
              } while (saved_layer_idx + 1 <= *(int *)0x2f66f0);
            }
          }

          i8 = unit_data;
          i21 = unhi_tag;

          if (*(int *)(unhi_tag + 0xbc) != -1) {
            FUN_000d3fe0(local_player_idx, (short *)unhi_tag, unhi_tag + 0x8c,
                         flags, *(int *)((char *)pf6 + 0x10));
            i8 = unit_data;
            i21 = unhi_tag;
          }
        }

        if ((*(unsigned int *)(*(int *)0x46bd20 + 0x160) & 1) == 0) {
          unsigned short health_status_bits;

          health_status_bits = *(unsigned short *)(i8 + 0xb6);
          if ((health_status_bits & 8) == 0 &&
              (*(unsigned int *)(*(int *)0x46bd20 + 0x160) & 2) == 0) {
            flags = 0;
          } else {
            flags = 1;
          }
          if ((health_status_bits & 4) != 0) {
            flags = flags | 2;
          }
          s4 = local_player_count();
          if (1 < s4) {
            flags = flags | 4;
          }

          if (u18 == 0) {
            if ((flags & 1) == 0) {
              *(int *)((char *)pf6 + 0x14) = (int)0xFFFFFFFF;
            } else if (*(int *)((char *)pf6 + 0x14) == (int)0xFFFFFFFF) {
              *(int *)((char *)pf6 + 0x14) = game_time_get();
            }
          }

          i13 = unhi_tag;
          if (*(int *)(i21 + 0x214) != -1) {
            short health_max;
            int health_alpha;
            int health_flash_alpha;

            health_max = *(short *)(i21 + 0x22e);
            if (health_max == 0) {
              health_max = 8;
            }

            {
              int *h_src = (int *)(unhi_tag + 0x1e4);
              int *h_dst = health_meter_data;
              int h_cnt;
              for (i8 = unit_data, h_cnt = 0x1a; h_cnt != 0; h_cnt--) {
                *h_dst = *h_src;
                h_src++;
                h_dst++;
              }
            }

            if (*(float *)(unit_data + 0x90) < *(float *)(i13 + 0x250)) {
              if (*(float *)(unit_data + 0x90) < *(float *)(i13 + 0x254) ||
                  *(float *)(unit_data + 0x90) == *(float *)(i13 + 0x254)) {
                /* override stores at base+0x38/+0x34 = indices 14/13, not 15/14
                 * (disasm: MOV [EBP-0x15c]/[EBP-0x160], base EBP-0x194). */
                health_meter_data[14] = health_meter_data[13];
              } else {
                health_meter_data[14] = *(int *)(i13 + 0x24c);
              }
            }
            health_meter_data[13] = health_meter_data[14];

            meter_scale = (float)(int)health_max;

            i7 = FUN_000d1c50(meter_scale * *(float *)(unit_data + 0x90));
            if (i7 < 0) {
              health_alpha = 0;
            } else {
              i7 = FUN_000d1c50(meter_scale * *(float *)(i8 + 0x90));
              if (i7 < 0x100) {
                health_alpha =
                  FUN_000d1c50(meter_scale * *(float *)(i8 + 0x90));
              } else {
                health_alpha = 0xff;
              }
            }

            i7 = FUN_000d1c50(meter_scale * *(float *)(i8 + 0x90));
            if (i7 < 0) {
              health_flash_alpha = 0;
            } else {
              i7 = FUN_000d1c50(meter_scale * *(float *)(i8 + 0x90));
              if (i7 < 0x100) {
                health_flash_alpha =
                  FUN_000d1c50(meter_scale * *(float *)(i8 + 0x90));
              } else {
                health_flash_alpha = 0xff;
              }
            }

            FUN_000d3340(local_player_idx, i13, (int)health_meter_data,
                         health_flash_alpha, health_alpha, flags,
                         (int)0xbf800000, *(float *)(i8 + 0x90));
            i21 = unhi_tag;
          }

          if (*(int *)(i21 + 0x1ac) != -1) {
            FUN_000d3fe0(local_player_idx, (short *)i21, i21 + 0x17c,
                         flags, *(int *)((char *)pf6 + 0x14));
          }

          pf6[1] = *(float *)(unit_data + 0x90);
        }

        if (u18 == 0 &&
            (*(unsigned char *)(*(int *)0x46bd20 + 0x160) & 0x10) == 0 &&
            game_engine_hud_draw_motion_sensor(player_index) != '\0') {
          l_130[0] = 2;

          s4 = local_player_count();
          b = (unsigned char)((s4 < 2) - 1) & 4;
          if ((*(unsigned char *)(*(int *)0x46bd20 + 0x160) & 0x20) != 0) {
            b = b | 1;
          }

          if ((b & 1) == 0) {
            *(int *)((char *)pf6 + 0x18) = (int)0xFFFFFFFF;
          } else if (*(int *)((char *)pf6 + 0x18) == (int)0xFFFFFFFF) {
            *(int *)((char *)pf6 + 0x18) = game_time_get();
          }

          i13 = unhi_tag;
          if (*(int *)(unhi_tag + 0x29c) != -1) {
            FUN_000d3fe0(local_player_idx, l_130, unhi_tag + 0x26c,
                         (unsigned int)b, (int)0xFFFFFFFF);
          }
          if (*(int *)(i13 + 0x304) != -1) {
            FUN_000d3fe0(local_player_idx, l_130, i13 + 0x2d4,
                         (unsigned int)b, (int)0xFFFFFFFF);
          }

          s4 = local_player_count();
          FUN_000d1f40((short)local_player_idx, (unsigned short *)l_130,
                       (short *)(i13 + 0x35c), 0, 1 < s4, 0,
                       (short *)l_78_buf);

          s4 = local_player_count();
          FUN_000dbfb0(local_player_idx, 1 < s4, (int)l_78_buf);
        }

        {
          int widget_base;
          int *widget_block_ptr;
          int widget_flags_mask;

          i13 = unhi_tag;
          widget_base = unhi_tag + 0x380;
          c3 = game_engine_has_teams();
          s4 = local_player_count();
          widget_block_ptr = (int *)(i13 + 0x3a4);
          widget_flags_mask = (int)((s4 < 2) - 1) & 4;

          if (0 < *(int *)(i13 + 0x3a4)) {
            int widget_type_mask;
            int widget_idx_int;

            widget_type_mask = (unsigned int)(c3 != '\0');
            widget_idx_int = 0;

            do {
              int widget_element;

              widget_element = (int)tag_block_get_element(
                (void *)widget_block_ptr, widget_idx_int, 0x84);

              if ((widget_type_mask &
                   (1 << (*(unsigned char *)(widget_element + 0x68) & 0x1f))) !=
                  0) {
                if ((*(unsigned char *)(widget_element + 0x6a) & 1) != 0) {
                  unsigned int packed_color;
                  packed_color = FUN_000d1dd0((float *)(unit_data + 0x138));
                  *(unsigned int *)(widget_element + 0x34) =
                    packed_color | 0xff000000;
                }

                FUN_000d3fe0(local_player_idx, (short *)widget_base,
                             widget_element, (unsigned int)widget_flags_mask,
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
              ps12 = (short *)tag_block_get_element(
                (void *)(unhi_tag + 0x3cc), overlay_idx, 0x144);

              {
                short overlay_type;
                unsigned int type_bit;
                unsigned int bitmask_20;

                overlay_type = *ps12;
                type_bit = 1 << ((unsigned char)overlay_type & 0x1f);
                bitmask_20 =
                  (unsigned int)*(unsigned short *)((char *)pf6 + 0x20);

                if ((type_bit & bitmask_20) != 0 &&
                    (full_shield & type_bit) == 0) {
                  *(short *)((char *)pf6 + overlay_type * 2 + 0x22) =
                    (short)0xffff;
                }

                overlay_type = *ps12;
                type_bit = 1 << ((unsigned char)overlay_type & 0x1f);

                if ((full_shield & type_bit) == 0) {
                  if ((damage_active & type_bit) == 0) {
                    ps22 =
                      (short *)((char *)pf6 + overlay_type * 2 + 0x22);
                    if (*ps22 != -1) {
                      ps22 = (short *)((char *)pf6 + *ps12 * 2 + 0x22);
                      i13 = FUN_000d2300((int)(ps12 + 0x24));
                      if (*ps22 < i13)
                        goto overlay_active_no_shield;
                    }
                    *ps22 = -1;
                  } else {
                  overlay_active_no_shield:
                    tag_ref_result =
                      verify_tag_reference((int *)(ps12 + 0x1c));
                    s4 = local_player_count();
                    ps22 = (short *)((char *)pf6 + *ps12 * 2 + 0x22);
                    s5 = game_time_get_elapsed();
                    *ps22 = *ps22 + s5;

                    if (tag_ref_result != (int)0xFFFFFFFF) {
                      int time_val = game_time_get();
                      FUN_000d3fe0(local_player_idx, (short *)unhi_tag,
                                   (int)(ps12 + 10),
                                   ((unsigned int)((s4 < 2) - 1) & 4) | 1,
                                   time_val -
                                     (int)*(short *)((char *)pf6 +
                                                     *ps12 * 2 + 0x22));
                    }
                  }
                } else {
                  int overlay_ref2;

                  tag_ref_result =
                    verify_tag_reference((int *)(ps12 + 0x1c));
                  overlay_ref2 = verify_tag_reference((int *)(ps12 + 0x50));
                  s4 = local_player_count();
                  flags = (unsigned int)((s4 < 2) - 1) & 4;

                  if (fraction_slots[*ps12] < *(float *)(ps12 + 0x72) &&
                      fraction_slots[*ps12] != *(float *)(ps12 + 0x72)) {
                    flags = flags | 1;
                  }

                  ps22 = (short *)((char *)pf6 + *ps12 * 2 + 0x22);
                  s5 = game_time_get_elapsed();
                  *ps22 = *ps22 + s5;

                  ps22 = (short *)((char *)pf6 + *ps12 * 2 + 0x22);
                  i13 = FUN_000d2300((int)(ps12 + 0x24));
                  *ps22 = (short)((int)*ps22 % (i13 << 1));

                  if (tag_ref_result != (int)0xFFFFFFFF) {
                    int time_val2 = game_time_get();
                    FUN_000d3fe0(
                      local_player_idx, (short *)unhi_tag, (int)(ps12 + 10),
                      flags,
                      time_val2 -
                        (int)*(short *)((char *)pf6 + *ps12 * 2 + 0x22));
                  }

                  if (overlay_ref2 != (int)0xFFFFFFFF) {
                    float frac_value;
                    float computed_alpha;
                    int alpha_a2, alpha_b2;
                    short alpha_scale;

                    alpha_scale = ps12[99];
                    frac_value = fraction_slots[*ps12];
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

                    FUN_000d3340(local_player_idx, unhi_tag,
                                 (int)(ps12 + 0x3e), alpha_b2, alpha_a2,
                                 flags, (int)0xbf800000, frac_value);
                  }
                }
              }

              overlay_idx = (int)(short)(overlay_loop_idx + 1);
              overlay_loop_idx = overlay_loop_idx + 1;
            } while (overlay_idx < *(int *)(unhi_tag + 0x3cc));
          }
        }

        *(unsigned short *)((char *)pf6 + 0x20) =
          (unsigned short)full_shield;
      }

    next_slot:
      slot_count = u18;
    }
  }

done_canary_check: {
  short canary_idx = 0x7f;
  do {
    if (canary_buf[canary_idx] != 0x62626262)
      goto canary_found;
    canary_idx = canary_idx - 1;
  } while (canary_idx >= 0);
  canary_idx = -1;

canary_found: {
  int cookie_check = FUN_000d1540();
  if (canary_cookie != cookie_check) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x3c9, 1);
    system_exit(-1);
  }
  if (canary_idx != -1) {
    char *msg =
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)canary_idx);
    display_assert(msg, "c:\\halo\\SOURCE\\interface\\hud_unit.c", 0x3c9, 1);
    system_exit(-1);
  }
}
}
}
