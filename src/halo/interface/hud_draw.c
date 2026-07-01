/* hud_draw.c — shared HUD drawing primitives (0xd3080-0xd3a60)
 *
 * Lifted from Halo CE Xbox (cachebeta.xbe).  Low-level HUD element drawing:
 * bitmap/overlay blit (FUN_000d3080/FUN_000d3200), animated meter drawing
 * (FUN_000d3340), and numeric readout drawing (FUN_000d3860).  Each renderer
 * is guarded by the debug stack canary (0x200 bytes of 0x62 plus a return-
 * address check via FUN_000d1540).  Original source: hud_draw.c.
 */

#include "x87_math.h"
