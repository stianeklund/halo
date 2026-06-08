---
name: hud-draw-element-callees
description: Confirmed register-arg ABI for hud_draw FUN_000d27a0 and FUN_000d1890 (callees of FUN_000d3fe0)
metadata:
  type: reference
---

Confirmed from cachebeta.xbe disasm (hud_draw.c, hud.obj). FUN_000d3fe0 (hud_draw_element)
is the sole caller of both in src.

**FUN_000d1890** (icon corner-offset computer): `@<eax>`+`@<edi>`+`@<bl>` + 2 stack.
`void FUN_000d1890(float *out_corners @<eax>, float *rect @<edi>, char align_flag @<bl>, int bitmap_handle, short screen_index);`
- @<eax> out: writes out_corners[0..3] (4 floats), switch on screen_index (cases 0-4).
- @<edi> rect: reads rect[0..3] (icon extents); caller falls back to a contiguous float[4] {0,1,0,1} when the d1580 ptr is null — must be a real array, not separate locals.
- @<bl>: 0 => use bitmap dims from param_1+4/+6; nonzero => use 1,1.

**FUN_000d27a0** (hud element renderer): `__thiscall` `@<ecx>`+`@<eax>` + 6 stack.
`void FUN_000d27a0(int element @<ecx>, float *scale @<eax>, int local_player_index, void *cursor, float *icon_rect, float *corners, void *param_6, int color);`
- @<ecx> element: tag_block element ptr (reads +0x4c..+0x60, +0x70/0x80/0x90, +0x154).
- @<eax> scale: float[2] = {*(float*)(struct+4), *(float*)(struct+8)}; reads *scale, scale[1].
- 6th stack arg `color` is a raw int color bitpattern stored verbatim (`pfVar6[3]=param_7`) — type int, NOT float; numeric float cast corrupts it.

**Lift hazard caught here:** in d3fe0 the color (`uVar4`, set once pre-loop from the param_4
branch = [EBP+0x10]) was being collapsed with the per-iteration d1f40 draw_flag. They are
distinct: color lives in memory and is read by both d3080 and d27a0; draw_flag is transient
in EAX, only for d1f40. Decouple them (separate `draw_flag` local). See [[feedback-check-disasm]].

Both forward thunks (cdecl->reg) generate correctly in build/generated/thunks.c, including
the non-scratch @<edi> (movl) and byte @<bl> (movb). Fix raised d3fe0 VC71 81.1%->83.2%.
