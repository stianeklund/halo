/* hud_draw.c — shared HUD drawing primitives (0xd3080-0xd3a60)
 *
 * Lifted from Halo CE Xbox (cachebeta.xbe).  Low-level HUD element drawing:
 * bitmap/overlay blit (FUN_000d3080/FUN_000d3200), animated meter drawing
 * (FUN_000d3340), and numeric readout drawing (FUN_000d3860).  Each renderer
 * is guarded by the debug stack canary (0x200 bytes of 0x62 plus a return-
 * address check via FUN_000d1540).  Original source: hud_draw.c.
 */

#include "x87_math.h"

/* FUN_000d3080 (0xd3080) — draw a HUD bitmap element with a crosshair overlay
 * rect.  EAX = crosshair_overlay (a 4-float uv rect; defaults to the unit rect
 * when NULL), ECX = hud_globals (screen scale at +4/+8, mirror flag at +0xc),
 * EDX = bitmap_data (pixel dims at +4/+6).  Resolves the placement via
 * FUN_000d1f40, the corner geometry via FUN_000d1890, and blits via
 * FUN_000d2580.  Debug stack canary guards the frame.  hud_draw.c line 0x32e. */
void FUN_000d3080(int crosshair_overlay /* @<eax> */, int hud_globals /* @<ecx> */,
                  int bitmap_data /* @<edx> */, int bitmap_handle, short *placement,
                  float scale, int angle, int color, int screen_pos,
                  char use_bitmap_size, char param_11)
{
  int canary;
  int guard[128];
  float out_corners[4];
  float scale_scaled[2];
  float rect[4];
  int mirror;
  short sVar3;

  (void)param_11;
  canary = FUN_000d1540();
  csmemset(guard, 0x62, 0x200);
  rect[0] = 0.0f;
  rect[1] = 1.0f;
  rect[2] = 0.0f;
  rect[3] = 1.0f;
  if (use_bitmap_size != 0) {
    rect[1] = (float)(int)*(short *)(bitmap_data + 4);
    rect[3] = (float)(int)*(short *)(bitmap_data + 6);
  }
  if (crosshair_overlay == 0) {
    crosshair_overlay = (int)rect;
  }
  scale_scaled[0] = scale * *(float *)(hud_globals + 4);
  scale_scaled[1] = scale * *(float *)(hud_globals + 8);
  mirror = 1;
  if ((char)screen_pos == 0 || (*(unsigned char *)(hud_globals + 0xc) & 1) != 0) {
    mirror = 0;
  }
  FUN_000d1f40(*(short *)0x506548, (unsigned short *)placement, (short *)hud_globals,
               0, (char)mirror, 0.0f, (short *)&screen_pos);
  FUN_000d1890(out_corners, (float *)crosshair_overlay, use_bitmap_size,
               (short *)bitmap_data, *placement);
  FUN_000d2580(scale_scaled, (short *)&screen_pos, bitmap_handle, bitmap_data,
               (int *)crosshair_overlay, out_corners, (float)angle, color);

  sVar3 = 0x7f;
  do {
    if (guard[(int)sVar3] != 0x62626262) goto LAB_000d3189;
    sVar3 = sVar3 - 1;
  } while (-1 < sVar3);
  sVar3 = -1;
LAB_000d3189:
  if (canary != FUN_000d1540()) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x32e, 1);
    system_exit(-1);
  }
  if (sVar3 != -1) {
    display_assert(csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)sVar3),
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x32e, 1);
    system_exit(-1);
  }
}

/* FUN_000d3200 (0xd3200) — draw a single HUD bitmap element using an explicit
 * scale value (scale_value, replicated into a 2-float scale vector) instead of
 * a crosshair overlay.  Computes the corner geometry with FUN_000d1890 and
 * blits with FUN_000d2580.  When use_bitmap_size is set the unit rect is scaled
 * to the bitmap's pixel dimensions (+4 width, +6 height); when uv_rect is NULL
 * it defaults to that rect.  Debug stack canary guards the frame.  hud_draw.c
 * source line 0x358. */
void FUN_000d3200(int bitmap_data, short screen_index, short *screen_pos,
                  int uv_rect, float scale_value, float rotation, int color,
                  char use_bitmap_size)
{
  int canary;
  int guard[128];
  float out_corners[4];
  float scale[2];
  float rect[4];
  short sVar3;

  canary = FUN_000d1540();
  csmemset(guard, 0x62, 0x200);
  rect[0] = 0.0f;
  rect[1] = 1.0f;
  rect[2] = 0.0f;
  rect[3] = 1.0f;
  if (use_bitmap_size != 0) {
    rect[1] = (float)(int)*(short *)(bitmap_data + 4);
    rect[3] = (float)(int)*(short *)(bitmap_data + 6);
  }
  if (uv_rect == 0) {
    uv_rect = (int)rect;
  }
  scale[0] = scale_value;
  scale[1] = scale_value;
  FUN_000d1890(out_corners, (float *)uv_rect, use_bitmap_size,
               (short *)bitmap_data, screen_index);
  FUN_000d2580(scale, screen_pos, 0, bitmap_data, (int *)uv_rect, out_corners,
               rotation, color);

  sVar3 = 0x7f;
  do {
    if (guard[(int)sVar3] != 0x62626262) goto LAB_000d32c9;
    sVar3 = sVar3 - 1;
  } while (-1 < sVar3);
  sVar3 = -1;
LAB_000d32c9:
  if (canary != FUN_000d1540()) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x358, 1);
    system_exit(-1);
  }
  if (sVar3 != -1) {
    display_assert(csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)sVar3),
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x358, 1);
    system_exit(-1);
  }
}

/* Contiguous meter-parameters block that FUN_000d3340 builds on its stack and
 * passes (as &meter_params) down through FUN_000d3080 -> FUN_000d2580, which
 * stores it into render_desc[0].  The unported rasterizer FUN_0015f8e0
 * (rasterizer_xbox_dynavobgeom.c) dereferences render_desc[0] as this struct and
 * reads fields at +0x10/+0x11 (tint modes) and +0x18 (gradient), which lie past
 * the four-dword color array.  These MUST be laid out contiguously with the
 * colors: the original builds them all at EBP-0x2c.  A prior lift declared the
 * trailing fields as separate C locals and passed only the color array, so C
 * placed the tint/gradient fields wherever it liked
 * wherever and the (void)-casts let clang dead-store-eliminate the writes ->
 * the rasterizer read stack garbage at +0x11 (tint_mode_2, usually 0 -> assert)
 * and +0x18 (gradient). */
struct hud_meter_parameters {
  unsigned int  color[4];     /* +0x00..+0x0c */
  unsigned char tint_mode_1;  /* +0x10 */
  unsigned char tint_mode_2;  /* +0x11  (rasterizer asserts != 0) */
  unsigned char pad[2];       /* +0x12..+0x13 (never read) */
  int           intensity;    /* +0x14 */
  float         gradient;     /* +0x18  (rasterizer asserts == 1.0f) */
};

/* FUN_000d3340 (0xd3340) — draw an animated HUD "meter" element (e.g. a health
 * or shield bar) from a hud meter definition (meter_def at param_3).  Resolves
 * the source bitmap (bitm) via the meter's interface bitmap reference (+0x24),
 * computes the two blend colors from the meter's empty/full color words
 * (+0x34/+0x38, argb pixel32) with a saturated alpha derived from the meter's
 * gradient bytes (+0x48 scale, +0x49 bias) and the caller's two input scalars
 * (param_4, param_5), then blits through FUN_000d3080.  The +0x40 background
 * color has its alpha inverted; +0x44 holds per-meter flags, +0x45 the alpha
 * floor, +0x46 the sequence index, +0x4c/+0x50 the flash timing.  Debug stack
 * canary guards the frame.  hud_draw.c source line 0x1ac. */
void FUN_000d3340(int param_1, int param_2, int meter_def, int param_4,
                  int param_5, int flags, int param_7, float param_8)
{
  int canary;
  int guard[128];
  float rgb_lower[3];   /* local_58 */
  float rgb_upper[3];   /* local_4c */
  char color_use_bitmap; /* local_40 (byte: *bitmap == 4) */
  int bitmap_data;      /* local_3c */
  int tag_index;        /* local_38 */
  int meter;            /* local_34, = meter_def */
  struct hud_meter_parameters meter_params; /* colors + tint modes + gradient,
                             * contiguous at EBP-0x2c; passed to d3080 */
  float color[3];       /* local_14/local_10/local_c */
  float alpha;          /* local_8 */
  float fade;           /* param_7 reinterpreted as float (caller passes raw bits) */
  short *bitmap;
  int hardware_format;
  int resolved_tag;
  int amount;
  int meter_alpha;
  int meter_empty_alpha;
  short sVar6;

  (void)param_1;
  meter = meter_def;
  canary = FUN_000d1540();
  csmemset(guard, 0x62, 0x200);
  resolved_tag = verify_tag_reference((int *)(meter_def + 0x24));
  bitmap = (short *)tag_get(0x6269746d, resolved_tag);
  bitmap_data = (int)FUN_00077040(*(int *)(meter_def + 0x30),
                                  *(short *)(meter_def + 0x46), 0);
  hardware_format = (int)xbox_texture_cache_get_hardware_format((void *)bitmap_data, 0, 1);
  if (hardware_format != 0) {
    resolved_tag = verify_tag_reference((int *)(meter_def + 0x24));
    tag_index = (int)FUN_000d1580(resolved_tag, *(short *)(meter_def + 0x46), 0);
    color_use_bitmap = (char)(*bitmap == 4);

    /* saturate amount = (+0x48 * param_4 + +0x49) to [floor(+0x45), 0xff].
     * The product is recomputed inline (as the original does) at each site and
     * converted with a plain signed FILD. */
    meter_empty_alpha = *(unsigned char *)(meter_def + 0x45);
    amount = FUN_000d1c50((float)(*(unsigned char *)(meter_def + 0x48) * (unsigned char)param_4 +
                                  *(unsigned char *)(meter_def + 0x49)));
    if (amount < 0) {
      amount = 0;
    } else {
      amount = FUN_000d1c50((float)(*(unsigned char *)(meter_def + 0x48) * (unsigned char)param_4 +
                                    *(unsigned char *)(meter_def + 0x49)));
      if (amount < 0x100) {
        amount = FUN_000d1c50((float)(*(unsigned char *)(meter_def + 0x48) * (unsigned char)param_4 +
                                      *(unsigned char *)(meter_def + 0x49)));
      } else {
        amount = 0xff;
      }
    }
    meter_alpha = *(unsigned char *)(meter_def + 0x45);
    if (meter_empty_alpha <= amount) {
      amount = FUN_000d1c50((float)(*(unsigned char *)(meter_def + 0x48) * (unsigned char)param_4 +
                                    *(unsigned char *)(meter_def + 0x49)));
      if (amount < 0) {
        meter_alpha = 0;
      } else {
        amount = FUN_000d1c50((float)(*(unsigned char *)(meter_def + 0x48) * (unsigned char)param_4 +
                                      *(unsigned char *)(meter_def + 0x49)));
        if (amount < 0x100) {
          meter_alpha = FUN_000d1c50((float)(*(unsigned char *)(meter_def + 0x48) * (unsigned char)param_4 +
                                             *(unsigned char *)(meter_def + 0x49)));
        } else {
          meter_alpha = 0xff;
        }
      }
    }

    /* second saturate amount = (+0x48 * param_5 + +0x49) to [floor(+0x45), 0xff] */
    amount = FUN_000d1c50((float)(*(unsigned char *)(meter + 0x48) * (unsigned char)param_5 +
                                  *(unsigned char *)(meter + 0x49)));
    if (amount < 0) {
      amount = 0;
    } else {
      amount = FUN_000d1c50((float)(*(unsigned char *)(meter + 0x48) * (unsigned char)param_5 +
                                    *(unsigned char *)(meter + 0x49)));
      if (amount < 0x100) {
        amount = FUN_000d1c50((float)(*(unsigned char *)(meter + 0x48) * (unsigned char)param_5 +
                                      *(unsigned char *)(meter + 0x49)));
      } else {
        amount = 0xff;
      }
    }
    meter_empty_alpha = *(unsigned char *)(meter + 0x45);
    if (meter_empty_alpha <= amount) {
      amount = FUN_000d1c50((float)(*(unsigned char *)(meter + 0x48) * (unsigned char)param_5 +
                                    *(unsigned char *)(meter + 0x49)));
      if (amount < 0) {
        meter_empty_alpha = 0;
      } else {
        amount = FUN_000d1c50((float)(*(unsigned char *)(meter + 0x48) * (unsigned char)param_5 +
                                      *(unsigned char *)(meter + 0x49)));
        if (amount < 0x100) {
          meter_empty_alpha = FUN_000d1c50((float)(*(unsigned char *)(meter + 0x48) * (unsigned char)param_5 +
                                                   *(unsigned char *)(meter + 0x49)));
        } else {
          meter_empty_alpha = 0xff;
        }
      }
    }

    if ((flags & 2) == 0) {
      if ((*(unsigned char *)(meter + 0x44) & 1) == 0) {
        fade = *(float *)&param_7; /* param_7 carries IEEE-754 bits passed as int */
        if (*(float *)0x2533c0 <= fade) {
          alpha = *(float *)0x2533c8 - fade;
          if (*(float *)0x2533c0 <= alpha) {
            if (*(float *)0x2533c8 < alpha) {
              alpha = 1.0f;
            }
          } else {
            alpha = 0.0f;
          }
        } else {
          alpha = 0.0f;
        }
        pixel32_to_real_rgb_color(*(unsigned int *)(meter + 0x3c), color);
        color[0] = color[0] * alpha;
        color[1] = color[1] * alpha;
        meter_params.color[0] = (*(unsigned int *)(meter + 0x34) & 0xffffff) |
                    ((int)(short)meter_alpha << 0x18);
        meter_params.color[1] = *(unsigned int *)(meter + 0x38) & 0xffffff;
        color[2] = color[2] * alpha;
        meter_params.color[3] = (FUN_000d1dd0(color) & 0xffffff) |
                    ((int)(short)meter_empty_alpha << 0x18);
      } else if ((flags & 1) == 0) {
        meter_params.color[3] = (int)(short)meter_alpha << 0x18;
        meter_params.color[1] = *(unsigned int *)(meter + 0x34) & 0xffffff;
        meter_params.color[0] = meter_params.color[3] | meter_params.color[1];
      } else if ((*(unsigned char *)(meter + 0x44) & 2) == 0) {
        meter_params.color[1] = *(unsigned int *)(meter + 0x38) & 0xffffff;
        meter_params.color[3] = (int)(short)meter_alpha << 0x18;
        meter_params.color[0] = meter_params.color[1] | meter_params.color[3];
      } else {
        pixel32_to_real_rgb_color(*(unsigned int *)(meter + 0x34), rgb_lower);
        pixel32_to_real_rgb_color(*(unsigned int *)(meter + 0x38), rgb_upper);
        if ((*(unsigned char *)(meter + 0x44) & 0x10) == 0) {
          alpha = param_8;
        } else {
          alpha = *(float *)0x2533c8 - param_8;
        }
        FUN_0007c270(color, 0, rgb_lower, rgb_upper, alpha);
        meter_params.color[0] = FUN_000d1dd0(color);
        meter_params.color[0] = meter_params.color[0] | (int)(short)meter_alpha << 0x18;
        meter_params.color[1] = FUN_000d1dd0(color);
        meter_params.color[3] = (int)(short)meter_alpha << 0x18;
      }
    } else {
      meter_params.color[1] = 0;
      meter_params.color[0] = 0;
      meter_params.color[3] = 0;
    }
    meter_params.color[2] = ((-1 - (*(unsigned int *)(meter + 0x40) >> 0x18)) * 0x1000000) |
                (*(unsigned int *)(meter + 0x40) & 0xffffff);
    /* meter+0x14: packed ARGB flash/blend color the rasterizer copies into its
     * render-state (_DAT_001fb7c4).  Original stores FUN_000d1e90's EAX here;
     * zeroing it rendered the meter black (secondary a30 HUD regression). */
    meter_params.intensity   = (int)FUN_000d1e90(
        *(float *)(meter + 0x50), *(float *)0x2533c8 - *(float *)(meter + 0x4c));
    meter_params.gradient    = 1.0f;   /* +0x18, rasterizer asserts == 1.0f */
    meter_params.tint_mode_1 = 0;      /* +0x10 */
    meter_params.tint_mode_2 = 1;      /* +0x11, rasterizer asserts != 0 */
    FUN_000d3080(tag_index, meter_def, bitmap_data, (int)&meter_params, (short *)param_2,
                 1.0f, 0, -1, (flags >> 2) & 0xffffff01, (char)color_use_bitmap, 0);
  }

  sVar6 = 0x7f;
  do {
    if (guard[(int)sVar6] != 0x62626262) goto LAB_000d37e5;
    sVar6 = sVar6 - 1;
  } while (-1 < sVar6);
  sVar6 = -1;
LAB_000d37e5:
  if (canary != FUN_000d1540()) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x1ac, 1);
    system_exit(-1);
  }
  if (sVar6 != -1) {
    display_assert(csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)sVar6),
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x1ac, 1);
    system_exit(-1);
  }
}

/* FUN_000d3860 (0xd3860) — draw a numeric HUD readout (score/timer/ammo) from
 * the shared "digits" hud number widget (interface tag index 0xb).  Resolves
 * the digit bitmap (bitm) and its source bitmap-data (bitm), positions the run
 * through FUN_000d1f40, then draws digit glyphs right-to-left via repeated
 * FUN_000d3200 blits, advancing the running x-position (a short) by the digit
 * pitch (+0x11) times the scale each glyph.  Handles a leading "special" prefix
 * (widget +0x46 count), leading-zero suppression / minimum digit count (+0x44),
 * the '%'/'x' terminator glyph (+0x14), a negative-sign glyph (index 0xc), and
 * a "big number" mode (+0x45 bit 2) that divides by 1000.  Each glyph blit
 * asserts the source bitmap matches the number bitmap.  Debug stack canary
 * guards the frame.  hud_draw.c source line 0x25a. */
void FUN_000d3860(short local_player, void *element, void *position, int value,
                  int param_5, unsigned int flags, int timer_start, float scale)
{
  int canary;
  int guard[128];
  int div_scratch;      /* local_38 (loop quotient temp) */
  int bitmap_data;      /* local_28 (FUN_00077040 result) */
  short *source_bitmap; /* local_2c (bitm tag) */
  int color;            /* local_1c (forwarded to d3200 color; raw int) */
  float base_x;         /* local_18 (running float x before _ftol2) */
  float digit_offset;   /* local_8 (glyph vertical/offset float) */
  float scale_v;        /* local_14 (clamped scale) */
  char special_big;     /* local_19 (999 < value) */
  char is_negative;     /* local_21 (value < 0) */
  short out_pos[2];     /* local_24/local_22: x,y — both written contiguously by FUN_000d1f40 */
  short screen_pos[2];  /* local_10/local_e (x,y passed to d3200) */
  int out_bitmap;       /* local_c (d16a0 out_bitmap) */
  int out_sprite;       /* local_10-alt: d16a0 out_sprite */
  int hud;              /* local, iVar4 (hud number widget) */
  int gate;             /* local, iVar5 (texture cache guard) */
  short running_x;      /* SI (accumulated glyph x-position) */
  short digit_count;    /* AX in loops */
  int special_count;    /* iVar5 reused (leading prefix count) */
  int v;                /* param_4 working copy */
  short sVar8;

  canary = FUN_000d1540();
  csmemset(guard, 0x62, 0x200);
  hud = interface_get_tag_index(0xb);
  if (hud != -1) {
    hud = (int)tag_get(0x68756423, hud);
    source_bitmap = (short *)tag_get(0x6269746d, *(int *)(hud + 0xc));
    bitmap_data = (int)FUN_00077040(*(int *)(hud + 0xc), 0, 0);
    sVar8 = (short)value;
    special_big = (char)(999 < sVar8);
    gate = (int)xbox_texture_cache_get_hardware_format((void *)bitmap_data, 0, 1);
    if (gate != 0) {
      is_negative = (char)(sVar8 < 0);
      if ((*(char *)((int)position + 0x46) == 0) || ((short)param_5 == -1)) {
        special_count = 0;
      } else {
        special_count = 4;
        if (*(char *)((int)position + 0x46) < 5) {
          special_count = (int)*(char *)((int)position + 0x46);
        }
        special_count = special_count + 1;
      }
      base_x = (float)(*(char *)((int)position + 0x44) + special_count);
      if (*(char *)((int)position + 0x46) == 0) {
        digit_offset = 0.0f;
      } else {
        digit_offset = (float)(int)*(char *)(hud + 0x14);
      }
      if (scale <= *(float *)0x2533c0) {
        scale_v = 1.0f;
      } else {
        scale_v = scale;
      }
      if ((*(unsigned char *)((int)position + 0x45) & 4) != 0) {
        base_x = base_x + *(float *)0x2533c8;
        if (special_big != 0) {
          sVar8 = sVar8 / 1000;
          param_5 = value * 10;
        }
      }
      v = ((int)sVar8 ^ ((int)sVar8 >> 0x1f)) - ((int)sVar8 >> 0x1f); /* abs(sVar8) */

      FUN_000d1f40(local_player, (unsigned short *)element, (short *)position, 0,
                   (char)((flags >> 2) & 0xffffff01), 0.0f, out_pos);

      /* running_x = (short) of the base run position, per screen orientation */
      switch (*(short *)element) {
      case 0:
      case 2:
        running_x = (short)(((base_x - *(float *)0x253f40) *
                             (float)(int)*(char *)(hud + 0x11) + digit_offset) *
                            scale_v + (float)(int)out_pos[0]);
        break;
      case 1:
      case 3:
        running_x = out_pos[0];
        break;
      case 4:
        running_x = (short)(((base_x - *(float *)0x2533c8) *
                             (float)(int)*(char *)(hud + 0x11) + digit_offset) *
                            scale_v * *(float *)0x253398 + (float)(int)out_pos[0]);
        break;
      default:
        display_assert("!\"unreachable\"",
                       "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x1ed, 1);
        system_exit(-1);
      }

      if (bitmap_data != 0) {
        if ((flags & 2) == 0) {
          if ((flags & 1) == 0) {
            color = *(int *)((int)position + 0x24);
          } else {
            color = (int)FUN_000d2320((int *)((int)position + 0x24), timer_start);
          }
        } else {
          color = *(int *)((int)position + 0x3c);
        }

        /* leading "special" prefix glyph */
        if ((*(unsigned char *)((int)position + 0x45) & 4) != 0) {
          out_bitmap = 0;
          out_sprite = 0;
          screen_pos[0] = running_x;
          screen_pos[1] = out_pos[1];
          FUN_000d16a0(*(int *)(hud + 0xc), 0, (unsigned int)((is_negative != 0) + '\r'),
                       &out_bitmap, &out_sprite);
          if (bitmap_data != out_bitmap) {
            display_assert("source_bitmap==number_bitmap",
                           "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x203, 1);
            system_exit(-1);
          }
          FUN_000d3200(out_bitmap, *(short *)element, screen_pos, out_sprite,
                       scale_v, 0.0f, color, (char)(*source_bitmap == 4));
          running_x = (short)((float)(int)running_x -
                              (float)(int)*(char *)(hud + 0x11) * scale_v);
        }

        /* leading-zero suppression: draw the fractional/high digits */
        if ((*(char *)((int)position + 0x46) != 0) && (-1 < (short)param_5)) {
          if (*(char *)((int)position + 0x46) < 5) {
            special_count = (int)(short)*(char *)((int)position + 0x46);
          } else {
            special_count = 4;
          }
          if ((short)special_count < 4) {
            unsigned int drop;
            drop = (4U - (unsigned int)special_count) & 0xffff;
            do {
              param_5 = (int)(short)param_5 / 10;
              drop = drop - 1;
            } while (drop != 0);
          }
          if (0 < (short)special_count) {
            digit_count = (short)special_count;
            screen_pos[1] = out_pos[1];
            do {
              short d;
              out_bitmap = 0;
              out_sprite = 0;
              d = (short)param_5;
              param_5 = (int)d / 10;
              screen_pos[0] = running_x;
              FUN_000d16a0(*(int *)(hud + 0xc), 0, (unsigned int)((int)d % 10),
                           &out_bitmap, &out_sprite);
              if (bitmap_data != out_bitmap) {
                display_assert("source_bitmap==number_bitmap",
                               "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x21b, 1);
                system_exit(-1);
              }
              FUN_000d3200(out_bitmap, *(short *)element, screen_pos, out_sprite,
                           scale_v, 0.0f, color, (char)(*source_bitmap == 4));
              running_x = (short)((float)(int)running_x -
                                  (float)(int)*(char *)(hud + 0x11) * scale_v);
              digit_count = digit_count - 1;
            } while (digit_count != 0);
          }
          /* terminator glyph (index 10): advance by one pitch, back off the
           * glyph offset, each rounded to a short via _ftol2 */
          running_x = (short)((float)(int)running_x +
                              (float)(int)*(char *)(hud + 0x11) * scale_v);
          running_x = (short)((float)(int)running_x -
                              (float)(int)*(char *)(hud + 0x14) * scale_v);
          out_bitmap = 0;
          out_sprite = 0;
          screen_pos[0] = running_x;
          screen_pos[1] = out_pos[1];
          FUN_000d16a0(*(int *)(hud + 0xc), 0, 10, &out_bitmap, &out_sprite);
          if (bitmap_data != out_bitmap) {
            display_assert("source_bitmap==number_bitmap",
                           "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x22c, 1);
            system_exit(-1);
          }
          FUN_000d3200(out_bitmap, *(short *)element, screen_pos, out_sprite,
                       scale_v, 0.0f, color, (char)(*source_bitmap == 4));
          running_x = (short)((float)(int)running_x -
                              (float)(int)*(char *)(hud + 0x11) * scale_v);
        }

        /* integer digits, right-to-left */
        special_count = 0;
        if ('\0' < *(char *)((int)position + 0x44)) {
          do {
            short d;
            d = (short)v;
            div_scratch = (int)d / 10;
            if ((d == 0) && ((*(unsigned char *)((int)position + 0x45) & 1) == 0)) {
              break;
            }
            screen_pos[1] = out_pos[1];
            out_bitmap = 0;
            out_sprite = 0;
            screen_pos[0] = running_x;
            FUN_000d16a0(*(int *)(hud + 0xc), 0, (unsigned int)((int)d % 10),
                         &out_bitmap, &out_sprite);
            if (bitmap_data != out_bitmap) {
              display_assert("source_bitmap==number_bitmap",
                             "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x23f, 1);
              system_exit(-1);
            }
            FUN_000d3200(out_bitmap, *(short *)element, screen_pos, out_sprite,
                         scale_v, 0.0f, color, (char)(*source_bitmap == 4));
            running_x = (short)((float)(int)running_x -
                                (float)(int)*(char *)(hud + 0x11) * scale_v);
            v = div_scratch;
            special_count = special_count + 1;
          } while ((short)special_count < (short)*(char *)((int)position + 0x44));
        }

        /* negative-sign glyph (index 0xc) */
        if (is_negative != 0) {
          screen_pos[1] = out_pos[1];
          out_bitmap = 0;
          out_sprite = 0;
          screen_pos[0] = running_x;
          FUN_000d16a0(*(int *)(hud + 0xc), 0, 0xc, &out_bitmap, &out_sprite);
          if (bitmap_data != out_bitmap) {
            display_assert("source_bitmap==number_bitmap",
                           "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x253, 1);
            system_exit(-1);
          }
          FUN_000d3200(out_bitmap, *(short *)element, screen_pos, out_sprite,
                       scale_v, 0.0f, color, (char)(*source_bitmap == 4));
        }
      }
    }
  }

  sVar8 = 0x7f;
  do {
    if (guard[(int)sVar8] != 0x62626262) goto LAB_000d3f15;
    sVar8 = sVar8 - 1;
  } while (-1 < sVar8);
  sVar8 = -1;
LAB_000d3f15:
  if (canary != FUN_000d1540()) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x25a, 1);
    system_exit(-1);
  }
  if (sVar8 != -1) {
    display_assert(csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)sVar8),
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x25a, 1);
    system_exit(-1);
  }
}
