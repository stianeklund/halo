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
