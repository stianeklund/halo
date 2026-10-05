/* 0x19c5d0 — draw_string: lay the 8-bit `text` out line by line inside the
 * rectangle at `screen_pos` and hand each laid-out run to FUN_0019c1b0 for
 * clipping and per-glyph emission through `callback`.
 *
 * Confirmed from the pristine-XBE disassembly of [0x19c5d0, 0x19c939):
 *   - SUB ESP,0x54: the 0x1c-byte tokenizer state block (FUN_0019bd30 /
 *     parse_string layout) at EBP-0x54; the 8-byte rectangle copied from
 *     *screen_pos (two dword moves) at EBP-0x38; the int16 pen pair at
 *     EBP-0x10. State fields read: +4 font table, +0xc position, +0x10
 *     justification, +0x12 character, +0x14 token type, +0x18 color word.
 *   - Asserts "bounds" (0x27f) and "string" (0x280) guard screen_pos and
 *     text; tab-stop index assert is line 0x2a0; switch default is
 *     display_assert(NULL, ..., 0x328). Each tail calls system_exit(-1).
 *   - FUN_0019bd30: style 0x4d9b14 in AX, &state in EBX, pushes text,
 *     [0x4d9b0c], zero-extended [0x4d9b16], 0x4d9b18.
 *   - The tab-stop count 0x4d9b28 is re-read from memory for the
 *     `tab < count` test after the stop value was loaded into AX.
 *   - FUN_0019c1b0: EAX = &rect, pushes callback, &pen, clip_bounds,
 *     state+0x18, text, first, last (cdecl, ADD ESP,0x1c).
 *   - On exit stores 0 to 0x4d9b4c then 0x4d9b4a and, if the third argument
 *     is non-NULL, stores the final pen dword through it.
 *
 * Same body shape as the wide-string twin FUN_0019c960 below. The kb
 * parameter names `color` and `flags` are unproven: the third argument is
 * written as an int16 pen pair (draw_string_compute_bounds passes
 * &screen_pos there) and `flags` is added to the line pitch.
 */
void draw_string(void *draw_character, void *bounds, const void *cursor_reference,
                 void *clip, int height_adjust, char *string)
{
  /* names: PAL 2342 source/text/draw_string.c:981 */
  char state[0x1c];
  int16_t line_bounds[4];
  int16_t cursor[2];
  void *character;
  short segment_start_index;
  short justification;
  short break_line_width;
  short previous_result;
  short break_string_index;
  short maximum_wrapped_line_index;
  short segment_end_index;
  short paragraph_line_index;
  short tab_stop_index;
  short wrapped_line_index;
  short line_width;
  short y;
  char done;
  char wrapped;

  tab_stop_index = 0;
  paragraph_line_index = 0;
  wrapped_line_index = 0;
  maximum_wrapped_line_index = 0;
  if (bounds == 0) {
    display_assert("bounds", "c:\\halo\\SOURCE\\text\\draw_string.c", 0x27f, 1);
    system_exit(-1);
  }
  if (string == 0) {
    display_assert("string", "c:\\halo\\SOURCE\\text\\draw_string.c", 0x280, 1);
    system_exit(-1);
  }
  FUN_0019bd30(*(short *)0x4d9b14, state, (int *)string, *(int *)0x4d9b0c,
               *(short *)0x4d9b16, (float *)0x4d9b18);

  do {
    short tab_stop_count;

    segment_start_index = *(int16_t *)(state + 0xc);
    justification = *(int16_t *)(state + 0x10);
    previous_result = -1;
    ((int *)line_bounds)[0] = ((int *)bounds)[0];
    ((int *)line_bounds)[1] = ((int *)bounds)[1];
    line_width = 0;
    break_string_index = 0;
    done = 0;
    tab_stop_count = *(short *)0x4d9b28;
    if (tab_stop_count > 0) {
      if (tab_stop_index < 0 || tab_stop_index > tab_stop_count) {
        display_assert("tab_stop_index>=0 && "
                       "tab_stop_index<=font_drawing_globals.tab_stop_count",
                       "c:\\halo\\SOURCE\\text\\draw_string.c", 0x2a0, 1);
        system_exit(-1);
      }
      if (tab_stop_index != 0)
        line_bounds[1] = ((short *)0x4d9b28)[(int)tab_stop_index];
      else if (paragraph_line_index != 0)
        line_bounds[1] = (int16_t)(line_bounds[1] + *(short *)0x4d9b50);
      else
        line_bounds[1] = (int16_t)(line_bounds[1] + *(short *)0x4d9b4e);
      if (tab_stop_index < *(short *)0x4d9b28)
        line_bounds[3] = ((short *)0x4d9b2a)[(int)tab_stop_index];
    } else if (paragraph_line_index != 0) {
      line_bounds[1] = (int16_t)(line_bounds[1] + *(short *)0x4d9b50);
    } else {
      line_bounds[1] = (int16_t)(line_bounds[1] + *(short *)0x4d9b4e);
    }

    cursor[0] = (int16_t)(*(int16_t *)(*(char **)(state + 4) + 0xa) + line_bounds[1]);
    y = (short)((*(uint16_t *)(*(char **)(state + 4) + 8) +
                 *(uint16_t *)(*(char **)(state + 4) + 6) +
                 *(uint16_t *)(*(char **)(state + 4) + 4) + height_adjust) *
                  (wrapped_line_index + paragraph_line_index) +
                *(uint16_t *)(*(char **)(state + 4) + 4) + line_bounds[0]);
    cursor[1] = y;

    do {
      wrapped = 0;
      parse_string(state);
      if (*(int16_t *)(state + 0x14) == 2 || *(int16_t *)(state + 0x14) == 6) {
        character =
          FUN_0019cff0(*(void **)(state + 4), *(uint16_t *)(state + 0x12));
        if (character != 0) {
          if (*(int16_t *)(state + 0x14) != 2 && previous_result == 2) {
            break_string_index = segment_end_index;
            break_line_width = line_width;
          }
          if (*(int16_t *)((char *)character + 4) + cursor[0] + line_width < line_bounds[3]) {
            line_width = (short)(line_width + *(int16_t *)((char *)character + 2));
          } else if (*(int *)0x4d9b10 & 1) {
            if (break_string_index > 0) {
              segment_end_index = break_string_index;
              line_width = break_line_width;
              wrapped = 1;
            }
            done = 1;
          }
        }
      } else {
        done = 1;
      }
      if (!wrapped)
        segment_end_index = *(int16_t *)(state + 0xc);
      previous_result = *(int16_t *)(state + 0x14);
    } while (!done);

    switch (justification) {
    case 1:
      cursor[0] = (int16_t)(rect2d_width(line_bounds) +
                         (int16_t)(line_bounds[1] -
                                   *(int16_t *)(*(char **)(state + 4) + 0xa)) -
                         line_width);
      break;
    case 2:
      cursor[0] = (int16_t)((((short)rect2d_width(line_bounds) - line_width) >> 1) + line_bounds[1]);
      break;
    }

    if ((*(int *)0x4d9b10 & 2) || y < line_bounds[2])
      FUN_0019c1b0((const uint16_t *)line_bounds, (draw_string_emit_proc)draw_character, cursor,
                   (const uint16_t *)clip, *(int *)(state + 0x18),
                   (int *)string, segment_start_index, segment_end_index);

    *(int16_t *)(state + 0xc) = segment_end_index;
    switch (*(int16_t *)(state + 0x14)) {
    case 0:
    case 5:
      break;
    case 2:
    case 6:
      wrapped_line_index++;
      if (wrapped_line_index > maximum_wrapped_line_index)
        maximum_wrapped_line_index = wrapped_line_index;
      break;
    case 3:
      if (tab_stop_index < *(short *)0x4d9b28) {
        tab_stop_index++;
        wrapped_line_index = 0;
      }
      break;
    case 4:
      wrapped_line_index = 0;
      break;
    case 1:
      tab_stop_index = 0;
      wrapped_line_index = 0;
      paragraph_line_index = (short)(paragraph_line_index + maximum_wrapped_line_index + 1);
      break;
    default:
      display_assert(0, "c:\\halo\\SOURCE\\text\\draw_string.c", 0x328, 1);
      system_exit(-1);
    }
  } while (*(int16_t *)(state + 0x14) != 0);

  *(short *)0x4d9b4c = 0;
  *(short *)0x4d9b4a = 0;
  if (cursor_reference != 0)
    *(int *)cursor_reference = *(int *)cursor;
}

/* 0x19c960 — Wide-string (unsigned short text) draw_string: lay `text` out
 * line by line inside the rectangle at `screen_pos` and hand each laid-out
 * run to FUN_0019c3c0 for clipping and per-glyph emission.
 *
 * Confirmed from the pristine-XBE disassembly of [0x19c960, 0x19ccd1):
 *   - SUB ESP,0x58: the 0x1c-byte tokenizer state block (FUN_0019bd30 /
 *     FUN_0019c0a0 layout, see draw_string.c) sits at EBP-0x58; the
 *     8-byte rectangle copied from *screen_pos (two dword moves) at
 *     EBP-0x3c; the int16 pen pair at EBP-0x10.
 *   - Asserts "bounds" (0x347) and "string" (0x348) guard screen_pos and
 *     text; the tab-stop index assert is line 0x368; the switch default is
 *     display_assert(NULL, ..., 0x3f0). Each tail calls system_exit(-1).
 *   - FUN_0019bd30 is called with the same global operands as the two
 *     draw_string.c callers (style 0x4d9b14 in AX, &state in EBX).
 *   - Tab stops: 0x4d9b28 is the count and the stops follow at 0x4d9b2a, so
 *     `[tab*2+0x4d9b28]` (tab >= 1) is stop tab-1 and `[tab*2+0x4d9b2a]` is
 *     stop tab. Indents: 0x4d9b4e (line 0) / 0x4d9b50 (later lines).
 *   - Glyph fields read from FUN_0019cff0's result: +2 advance, +4 width.
 *     Font-table fields: +4, +6, +8 summed with `flags` form the line pitch
 *     multiplied by (wrap + line); +4 and +0xa are baseline offsets.
 *   - Justification (state+0x10) 1 = right, 2 = centre, via rect2d_width.
 *   - 0x4d9b10 bit 0 enables word-wrap at the last space-to-word boundary;
 *     bit 1 forces emission even when the pen is below rect[2].
 *   - On exit clears the highlight range words 0x4d9b4c/0x4d9b4a and, if
 *     the third argument is non-NULL, stores the final pen dword through it.
 *
 * The kb parameter names `color` and `flags` are unproven: the third
 * argument is written as an int16 pen pair (see FUN_0019cdb0, which passes
 * &out_rect there), and `flags` is added to the line pitch. Token type 2 is
 * never produced by FUN_0019c0a0 as lifted; its handling is kept verbatim.
 */
void FUN_0019c960(void *callback, void *screen_pos, const void *color,
                  void *clip_bounds, int flags, unsigned short *text)
{
  char state[0x1c];
  int16_t rect[4];
  int16_t pen[2];
  void *glyph;
  short first;
  short justification;
  short saved_width;
  short prev_token;
  short saved_pos;
  short max_wrap;
  short last;
  short line;
  short tab;
  short wrap;
  short width;
  short tab_count;
  short y;
  char done;
  char rewound;

  tab = 0;
  line = 0;
  wrap = 0;
  max_wrap = 0;
  if (screen_pos == 0) {
    display_assert("bounds", "c:\\halo\\SOURCE\\text\\draw_string.c", 0x347, 1);
    system_exit(-1);
  }
  if (text == 0) {
    display_assert("string", "c:\\halo\\SOURCE\\text\\draw_string.c", 0x348, 1);
    system_exit(-1);
  }
  FUN_0019bd30(*(short *)0x4d9b14, state, (int *)text, *(int *)0x4d9b0c,
               *(short *)0x4d9b16, (float *)0x4d9b18);

  do {
    first = *(int16_t *)(state + 0xc);
    justification = *(int16_t *)(state + 0x10);
    width = 0;
    saved_pos = 0;
    prev_token = -1;
    done = 0;
    ((int *)rect)[0] = ((int *)screen_pos)[0];
    ((int *)rect)[1] = ((int *)screen_pos)[1];
    tab_count = *(short *)0x4d9b28;
    if (tab_count > 0) {
      if (tab < 0 || tab > tab_count) {
        display_assert("tab_stop_index>=0 && "
                       "tab_stop_index<=font_drawing_globals.tab_stop_count",
                       "c:\\halo\\SOURCE\\text\\draw_string.c", 0x368, 1);
        system_exit(-1);
      }
      if (tab != 0)
        rect[1] = ((short *)0x4d9b28)[tab];
      else if (line != 0)
        rect[1] = (int16_t)(rect[1] + *(short *)0x4d9b50);
      else
        rect[1] = (int16_t)(rect[1] + *(short *)0x4d9b4e);
      if (tab < tab_count)
        rect[3] = ((short *)0x4d9b2a)[tab];
    } else if (line != 0) {
      rect[1] = (int16_t)(rect[1] + *(short *)0x4d9b50);
    } else {
      rect[1] = (int16_t)(rect[1] + *(short *)0x4d9b4e);
    }

    pen[0] = (int16_t)(*(int16_t *)(*(char **)(state + 4) + 0xa) + rect[1]);
    y = (short)((*(uint16_t *)(*(char **)(state + 4) + 8) +
                 *(uint16_t *)(*(char **)(state + 4) + 6) +
                 *(uint16_t *)(*(char **)(state + 4) + 4) + flags) *
                  (wrap + line) +
                *(uint16_t *)(*(char **)(state + 4) + 4) + rect[0]);
    pen[1] = y;

    do {
      rewound = 0;
      FUN_0019c0a0(state);
      if (*(int16_t *)(state + 0x14) == 2 || *(int16_t *)(state + 0x14) == 6) {
        glyph =
          FUN_0019cff0(*(void **)(state + 4), *(uint16_t *)(state + 0x12));
        if (glyph != 0) {
          if (*(int16_t *)(state + 0x14) != 2 && prev_token == 2) {
            saved_pos = last;
            saved_width = width;
          }
          if (*(int16_t *)((char *)glyph + 4) + pen[0] + width < rect[3]) {
            width = (short)(width + *(int16_t *)((char *)glyph + 2));
          } else if (*(int *)0x4d9b10 & 1) {
            if (saved_pos > 0) {
              last = saved_pos;
              width = saved_width;
              rewound = 1;
            }
            done = 1;
          }
        }
      } else {
        done = 1;
      }
      if (!rewound)
        last = *(int16_t *)(state + 0xc);
      prev_token = *(int16_t *)(state + 0x14);
    } while (!done);

    switch (justification) {
    case 1:
      pen[0] = (int16_t)(rect2d_width(rect) +
                         (int16_t)(rect[1] -
                                   *(int16_t *)(*(char **)(state + 4) + 0xa)) -
                         width);
      break;
    case 2:
      pen[0] = (int16_t)((((short)rect2d_width(rect) - width) >> 1) + rect[1]);
      break;
    }

    if ((*(int *)0x4d9b10 & 2) || y < rect[2])
      FUN_0019c3c0((const uint16_t *)rect, (draw_string_emit_proc)callback, pen,
                   (const uint16_t *)clip_bounds, *(int *)(state + 0x18),
                   (int *)text, first, last);

    *(int16_t *)(state + 0xc) = last;
    switch (*(int16_t *)(state + 0x14)) {
    case 0:
    case 5:
      break;
    case 2:
    case 6:
      wrap++;
      if (wrap > max_wrap)
        max_wrap = wrap;
      break;
    case 3:
      if (tab < *(short *)0x4d9b28) {
        tab++;
        wrap = 0;
      }
      break;
    case 4:
      wrap = 0;
      break;
    case 1:
      tab = 0;
      wrap = 0;
      line = (short)(line + max_wrap + 1);
      break;
    default:
      display_assert(0, "c:\\halo\\SOURCE\\text\\draw_string.c", 0x3f0, 1);
      system_exit(-1);
    }
  } while (*(int16_t *)(state + 0x14) != 0);

  *(short *)0x4d9b4c = 0;
  *(short *)0x4d9b4a = 0;
  if (color != 0)
    *(int *)color = *(int *)pen;
}

/* 0x19ccf0 — Lay out `text` without drawing it and report the resulting
 * bounds plus the rectangle of the final pen position.
 *
 * Confirmed from the pristine-XBE disassembly of [0x19ccf0, 0x19cda9):
 *   - Resets the draw_string min/max accumulator block that
 *     FUN_0019b3c0 updates per element (same globals documented in
 *     draw_string.c): 0x4d9afc/0x4d9afe (min_y/min_x) <- 0x7fff,
 *     0x4d9b00/0x4d9b02 (max_y/max_x) <- 0x8000.
 *   - `mov esi,[0x4d9b14]` / `mov edi,[0x4d9b0c]` feed the @<si>/@<edi>
 *     register args of FUN_0019bcc0(style, font_index); its EAX result is
 *     stored to 0x4d9b04 and later read for two int16 fields at +4 and +6
 *     (vertical extents above/below the baseline — meaning unproven).
 *   - `push 0x19b3c0 / push esi / push ecx(=&screen_pos) / push 0 / push 0 /
 *      push eax(=text)` then `call 0x19c5d0` + `add esp,0x18`: cdecl
 *     draw_string(FUN_0019b3c0, screen_pos, &screen_pos, 0, 0, text).
 *     The third argument is the address of this function's own first
 *     parameter slot; after the call [ebp+8]/[ebp+0xa] are re-read as two
 *     int16s (the pen position the callee wrote back), while the ORIGINAL
 *     pointer value is still live in ESI and is dereferenced once for
 *     out_bounds[0]. Both reads are reproduced here.
 *   - Store order is [eax+2], [eax+6], [eax], [eax+4] then [ecx+2], [ecx],
 *     [ecx+6], [ecx+4]; 0x4d9b04 is re-loaded between the two eax stores,
 *     so the global is re-read rather than cached.
 *
 * Inferred: the +0x14 rectangle is a one-pixel-wide vertical rect at the
 * final pen position (x = pen_x .. pen_x + 1), i.e. a text cursor/caret;
 * the name below records the shape, not proven intent.
 */
void draw_string_compute_bounds(void *screen_pos, char *text,
                                int16_t *out_bounds, int16_t *out_cursor)
{
  void *start_pos;
  const int16_t *pen;

  start_pos = screen_pos;
  pen = (const int16_t *)&screen_pos;

  *(int16_t *)0x4d9afc = 0x7fff;
  *(int16_t *)0x4d9afe = 0x7fff;
  *(int16_t *)0x4d9b00 = (int16_t)0x8000;
  *(int16_t *)0x4d9b02 = (int16_t)0x8000;

  *(void **)0x4d9b04 = FUN_0019bcc0(*(int16_t *)0x4d9b14, *(int *)0x4d9b0c);

  draw_string(FUN_0019b3c0, screen_pos, &screen_pos, 0, 0, text);

  out_cursor[1] = pen[0];
  out_cursor[3] = (int16_t)(pen[0] + 1);
  out_cursor[0] = (int16_t)(pen[1] - (*(const int16_t **)0x4d9b04)[2]);
  out_cursor[2] = (int16_t)((*(const int16_t **)0x4d9b04)[3] + pen[1]);

  out_bounds[1] = *(int16_t *)0x4d9afe;
  out_bounds[0] = *(const int16_t *)start_pos;
  out_bounds[3] = *(int16_t *)0x4d9b02;
  out_bounds[2] = out_cursor[2];
}

/* 0x19cdb0 — Wide-string (unsigned short text) twin of
 * draw_string_compute_bounds above.
 *
 * Confirmed from the pristine-XBE disassembly of [0x19cdb0, 0x19ce69):
 * same body instruction-for-instruction as 0x19ccf0 except the layout call
 * is `call 0x19c960` (FUN_0019c960, the unsigned-short-text draw_string
 * variant) with pushes 0x19b3c0 / esi(=out_rect) / ecx(=&out_rect) / 0 / 0 /
 * eax(=text), then `add esp,0x18`. After the call [ebp+8]/[ebp+0xa] (the
 * first parameter slot) are re-read as two int16s written back by the
 * callee, while the original pointer in ESI is dereferenced for
 * out_bounds[0]. Store order: [in_rect+2], [+6], [+0], [+4], then
 * [out_bounds+2], [+0], [+6], [+4]; 0x4d9b04 is re-loaded between stores.
 *
 * The kb parameter names (out_rect, in_rect) are unproven: out_rect is
 * the start screen position and in_rect is written, not read.
 */
void FUN_0019cdb0(short *out_rect, void *text, short *out_bounds,
                  short *in_rect)
{
  short *start_pos;
  const int16_t *pen;

  start_pos = out_rect;
  pen = (const int16_t *)&out_rect;

  *(int16_t *)0x4d9afc = 0x7fff;
  *(int16_t *)0x4d9afe = 0x7fff;
  *(int16_t *)0x4d9b00 = (int16_t)0x8000;
  *(int16_t *)0x4d9b02 = (int16_t)0x8000;

  *(void **)0x4d9b04 = FUN_0019bcc0(*(int16_t *)0x4d9b14, *(int *)0x4d9b0c);

  FUN_0019c960(FUN_0019b3c0, out_rect, &out_rect, 0, 0, (unsigned short *)text);

  in_rect[1] = pen[0];
  in_rect[3] = (int16_t)(pen[0] + 1);
  in_rect[0] = (int16_t)(pen[1] - (*(const int16_t **)0x4d9b04)[2]);
  in_rect[2] = (int16_t)((*(const int16_t **)0x4d9b04)[3] + pen[1]);

  out_bounds[1] = *(int16_t *)0x4d9afe;
  out_bounds[0] = *start_pos;
  out_bounds[3] = *(int16_t *)0x4d9b02;
  out_bounds[2] = in_rect[2];
}

/* 0x19ce70 — Seed the shared draw-string cursor hit-test search and resolve
 * the text cursor position nearest a screen point.
 *
 * Writes the reference point (*ref_point, a packed pair of shorts) and a
 * "no match yet" sentinel into the small globals block at 0x4d9af0 that
 * FUN_0019b430 (draw_string.c) reads/updates per candidate text element:
 *   0x4d9af0 (short ref_x) / 0x4d9af2 (short ref_y) <- *ref_point
 *   0x4d9af4 (short) cursor marker A -> reset to 0, returned
 *   0x4d9af6 (short) best distance   -> reset to 0x7fff (sentinel)
 *   0x4d9af8 (short) cursor marker B -> reset to 0
 * Then walks the text layout via draw_string (international_strings.obj,
 * same TU) with FUN_0019b430 as the per-element hit-test callback — same
 * calling idiom as the draw_string call in rasterizer_text.c. Returns the
 * resulting cursor marker.
 */
int16_t FUN_0019ce70(void *screen_pos, char *text, const void *ref_point)
{
  *(int *)0x4d9af0 = *(const int *)ref_point;
  *(int16_t *)0x4d9af4 = 0;
  *(int16_t *)0x4d9af6 = 0x7fff;
  *(int16_t *)0x4d9af8 = 0;

  draw_string(FUN_0019b430, screen_pos, 0, 0, 0, text);

  return *(int16_t *)0x4d9af4;
}

/* 0x19cec0 — Draw `text` into a bitmap through the software glyph
 * blitter (bitmap_draw_character, 0x19b910) instead of the rasterizer.
 *
 * Confirmed from the pristine-XBE disassembly of [0x19cec0, 0x19cfd1):
 *   - `mov [0x4d9ae8],esi` stores the bitmap pointer into the software
 *     glyph global block documented in draw_string.c (+0x00 bitmap_data*)
 *     unconditionally, before the format test.
 *   - `movsx eax,[esi+0xc]` / `cmp eax,0xb` / `ja end` then an 12-byte
 *     index table at 0x19cfdc feeding a 2-entry jump table at 0x19cfd4:
 *     only the signed int16 at bitmap+0xc in {0,1,2,6,11} reaches the
 *     body; every other value (negative ones via the unsigned JA) returns
 *     with no side effect beyond the 0x4d9ae8 store. Field meaning is
 *     unproven here; the case set matches a pixel-format selector.
 *   - Both clamp blocks are the same shape: two `cmp bm,r / jg` minima
 *     against bitmap+0x4 and bitmap+0x6, two `test/jge` clamps of r[0]
 *     and r[1] to >= 0, then cdecl set_rectangle2d(&local, max(0,r[1]),
 *     max(0,r[0]), min(bm[2],r[3]), min(bm[3],r[2])) with `add esp,0x14`.
 *     The frame is `sub esp,0x10` holding exactly the two 8-byte rects at
 *     [ebp-8] (screen) and [ebp-0x10] (clip).
 *   - The screen_bounds block runs on the NULL path (`test ebx,ebx` /
 *     `jnz`), and the original dereferences that same NULL pointer there:
 *     three of the four loads are assembled as absolute `[0x2]`, `[0x4]`,
 *     `[0x6]` (MSVC propagated the proven-zero base) and the fourth as
 *     `[ebx]`. Reproduced as written rather than "corrected" — the reads
 *     are on the parameter, so a NULL screen_bounds faults in the
 *     original exactly as it does here.
 *   - Tail call `push ecx(=text) / push 0 / push eax / push 0 / push ebx /
 *     push 0x19b910` + `add esp,0x18`: cdecl
 *     draw_string(bitmap_draw_character, screen_bounds, 0, clip, 0, text),
 *     where clip is 0 when clip_bounds was NULL.
 */
void bitmap_draw_string(void *bitmap, int16_t *screen_bounds,
                        int16_t *clip_bounds, char *text)
{
  const int16_t *bm;
  int16_t screen_rect[4];
  int16_t clip_rect[4];
  void *clip_arg;

  bm = (const int16_t *)bitmap;
  *(void **)0x4d9ae8 = bitmap;

  switch (bm[6]) {
  case 0:
  case 1:
  case 2:
  case 6:
  case 11:
    if (screen_bounds == 0) {
      set_rectangle2d(screen_rect, screen_bounds[1] < 0 ? 0 : screen_bounds[1],
                      screen_bounds[0] < 0 ? 0 : screen_bounds[0],
                      bm[2] > screen_bounds[3] ? screen_bounds[3] : bm[2],
                      bm[3] > screen_bounds[2] ? screen_bounds[2] : bm[3]);
      screen_bounds = screen_rect;
    }

    clip_arg = 0;
    if (clip_bounds != 0) {
      set_rectangle2d(clip_rect, clip_bounds[1] < 0 ? 0 : clip_bounds[1],
                      clip_bounds[0] < 0 ? 0 : clip_bounds[0],
                      bm[2] > clip_bounds[3] ? clip_bounds[3] : bm[2],
                      bm[3] > clip_bounds[2] ? clip_bounds[2] : bm[3]);
      clip_arg = clip_rect;
    }

    draw_string(bitmap_draw_character, screen_bounds, 0, clip_arg, 0, text);
    break;
  }
}

/* 0x19cff0 — Two-level character lookup in a font-like tag.
 *
 * Confirmed from the disassembly of [0x19cff0, 0x19d05e):
 *   - tag_block_get_element(font_table + 0x30, character >> 8, 0xc):
 *     a 12-byte element whose first dword is a count.
 *   - count <= 0 -> return NULL (EBX = 0).
 *   - count == 0x100 -> tag_block_get_element(element, character & 0xff, 2);
 *     any other positive count leaves the pointer NULL, and the original
 *     then reads the int16 at address 0 (`xor eax,eax / mov ax,[eax]`).
 *     That read is reproduced as written, not guarded.
 *   - int16 index == -1 -> return NULL; otherwise return
 *     tag_block_get_element(font_table + 0x7c, (int)index, 0x14) (EAX
 *     of the call is the return value).
 * Meaning of the blocks at +0x30 / +0x7c is unproven.
 */
__declspec(noinline) void *FUN_0019cff0(void *font_table, uint16_t character)
{
  int *page;
  short *index;

  page = (int *)tag_block_get_element((char *)font_table + 0x30, character >> 8,
                                      0xc);
  if (*page > 0) {
    if (*page == 0x100) {
      index = (short *)tag_block_get_element(page, character & 0xff, 2);
    } else {
      index = NULL;
    }
    if (*index != -1) {
      return tag_block_get_element((char *)font_table + 0x7c, (int)*index,
                                   0x14);
    }
  }
  return NULL;
}

/* 0x19d060 — Set the language/encoding selector read by
 * unicode_is_multibyte below. Values outside 0..5 (the encodings that
 * switch recognizes: 1=Shift-JIS, 2=Big5, 3=GBK, 4=Johab-like,
 * 5=Thai-like, 0=none) are clamped to 0. Stores into the encoding
 * selector global at 0x4d9be0. */
void set_language_code(short code)
{
  if (code < 0)
    code = 0;
  else if (code >= 6)
    code = 0;

  *(int16_t *)0x4d9be0 = code;
}

/* 0x19d080 — Return true if the two bytes starting at p form a valid
 * multibyte character under the current language encoding.
 *
 * The encoding selector lives at 0x4d9be0 (int16_t):
 *   1 = Shift-JIS  (lead: 0x81..0x9f or 0xe0..0xfe; trail: 0x40..0xfc, !=0x7f)
 *   2 = Big5       (lead: 0xa1..0xfe;                trail: 0xa1..0xfe)
 *   3 = GBK        (lead: 0x81..0xfe;                trail: 0x40..0x7e or
 * 0xa1..0xfe) 4 = Johab-like (lead: 0x81..0xfe;                trail:
 * 0x41..0x5a or 0x61..0x7a or 0x81..0xfe) 5 = Thai-like  (lead: 0x84..0xd3 or
 * 0xd8..0xde or 0xe0..0xf9; trail: 0x41..0x7e or 0x81..0xfe) Any other encoding
 * value returns false.
 *
 * A leading '|' byte (0x7c) followed by a byte in "ibukprlctn" is treated
 * as multibyte regardless of the encoding setting. */
bool unicode_is_multibyte(const uint8_t *p)
{
  uint8_t b0;
  uint8_t b1;
  bool result;

  b0 = p[0];
  b1 = p[1];
  result = 0;
  if (b0 != 0) {
    if (b0 == 0x7c && b1 != 0 &&
        crt_strchr("ibukprlctn", (int)b1) != (char *)0x0) {
      result = 1;
    } else {
      switch (*(int16_t *)0x4d9be0) {
      case 1:
        if ((b0 >= 0x81 && b0 <= 0x9f) || (b0 >= 0xe0 && b0 <= 0xfe)) {
          if (b1 >= 0x40 && b1 <= 0xfc && b1 != 0x7f)
            result = 1;
        }
        break;
      case 2:
        if (b0 >= 0xa1 && b0 <= 0xfe && b1 >= 0xa1 && b1 <= 0xfe)
          result = 1;
        break;
      case 3:
        if (b0 >= 0x81 && b0 <= 0xfe &&
            ((b1 >= 0x40 && b1 <= 0x7e) || (b1 >= 0xa1 && b1 <= 0xfe)))
          result = 1;
        break;
      case 4:
        if (b0 >= 0x81 && b0 <= 0xfe &&
            ((b1 >= 0x41 && b1 <= 0x5a) || (b1 >= 0x61 && b1 <= 0x7a) ||
             (b1 >= 0x81 && b1 <= 0xfe)))
          result = 1;
        break;
      case 5:
        if (((b0 >= 0x84 && b0 <= 0xd3) || (b0 >= 0xd8 && b0 <= 0xde) ||
             (b0 >= 0xe0 && b0 <= 0xf9)) &&
            ((b1 >= 0x41 && b1 <= 0x7e) || (b1 >= 0x81 && b1 <= 0xfe)))
          result = 1;
        break;
      }
    }
  }
  return result;
}

/* 0x19d1b0 — Read the character at *cursor and advance cursor forward.
 * If the byte is a multibyte lead byte (per unicode_is_multibyte), reads
 * two bytes big-endian and advances by 2; otherwise reads one byte and
 * advances by 1. Returns the character as uint16_t. */
uint16_t unicode_cursor_forward(const char *str, int16_t *cursor)
{
  if (*cursor < 0 || (size_t)*cursor > csstrlen(str)) {
    display_assert(csprintf((char *)0x5ab100,
                            "#%d is out of range in string @%p", (int)*cursor,
                            str),
                   "c:\\halo\\SOURCE\\text\\international_strings.c", 0x20, 1);
    system_exit(-1);
  }

  str += *cursor;
  if (unicode_is_multibyte((const uint8_t *)str)) {
    uint16_t ch = (uint16_t)(((uint8_t)str[0] << 8) | (uint8_t)str[1]);
    *cursor += 2;
    return ch;
  } else {
    uint16_t ch = (uint8_t)str[0];
    *cursor += 1;
    return ch;
  }
}

/* 0x19d240 — Move cursor backward by one character. Scans forward from
 * position 0 using unicode_cursor_forward, tracking the previous position.
 * Warns if *cursor falls between multibyte character bytes. Sets *cursor
 * to the start of the preceding character and returns it. */
uint16_t unicode_cursor_backward(const char *str, int16_t *cursor)
{
  int16_t pos;
  int16_t prev;
  uint16_t ch;

  if (*cursor <= 0 || (unsigned int)(int)*cursor > csstrlen(str)) {
    display_assert(csprintf((char *)0x5ab100,
                            "#%d is out of range in string @%p", (int)*cursor,
                            str),
                   "c:\\halo\\SOURCE\\text\\international_strings.c", 0x37, 1);
    system_exit(-1);
  }

  pos = 0;
  do {
    prev = pos;
    ch = unicode_cursor_forward(str, &pos);
  } while (pos < *cursor);

  if (pos != *cursor) {
    display_assert(csprintf((char *)0x5ab100,
                            "index #%d is inbetween characters in string %p",
                            (int)*cursor, str),
                   "c:\\halo\\SOURCE\\text\\international_strings.c", 0x43, 0);
  }

  *cursor = prev;
  return ch;
}

/* 0x19d300 — Snap cursor to a valid character boundary. Scans forward from
 * position 0 using unicode_cursor_forward until reaching or passing *cursor,
 * then writes the last valid position back to *cursor. */
void unicode_snap_cursor(const char *str, int16_t *cursor)
{
  int16_t pos;

  if (*cursor < 0 || (unsigned int)(int)*cursor > csstrlen(str)) {
    display_assert(csprintf((char *)0x5ab100,
                            "#%d is out of range in string @%p", (int)*cursor,
                            str),
                   "c:\\halo\\SOURCE\\text\\international_strings.c", 0x55, 1);
    system_exit(-1);
  }

  pos = 0;
  if (*cursor > 0) {
    do {
      unicode_cursor_forward(str, &pos);
    } while (pos < *cursor);
  }

  *cursor = pos;
}

/* 0x19d380 — Return true if character ch occurs anywhere in str. Walks str
 * with unicode_cursor_forward from position 0, comparing each decoded
 * character against ch, stopping at the terminating 0 (not found) or at the
 * first match (found). Callers (parse_string, draw_string.obj) use this to
 * test a decoded character against small delimiter-set strings. */
bool unicode_string_contains_char(uint16_t ch, const char *str)
{
  int16_t cursor;
  uint16_t c;

  cursor = 0;
  do {
    c = unicode_cursor_forward(str, &cursor);
    if (c == 0)
      return 0;
  } while (c != ch);

  return 1;
}

/* 0x19d3c0 — Fetch one string out of a 'str#' string-list tag by element
 * index, or return the placeholder text when anything about the request is
 * out of range.
 *
 * Confirmed from the pristine-XBE disassembly of [0x19d3c0, 0x19d413):
 *   - `cmp eax,-1 / jz` on the first parameter: NONE tag index takes the
 *     fallback path immediately, before any call.
 *   - `push eax / push 0x73747223 / call 0x1ba140 / add esp,8`: cdecl
 *     tag_get('str#', index).  0x73747223 is 's','t','r','#' little-endian.
 *   - `mov cx,[ebp+0xc] / test cx,cx / jl` then `mov edx,[eax] / movsx
 *     ecx,cx / cmp ecx,edx / jge`: the int16 element index is bounds-checked
 *     signed against the dword at offset 0 of the tag (the tag_block count).
 *     The int16 load happens after the call in the reference; the value is
 *     unchanged by it, only the schedule differs.
 *   - `push 0x14 / push ecx / push eax / call 0x19b210 / add esp,0xc`: cdecl
 *     tag_block_get_element(tag, index, 0x14) — a 20-byte element, the
 *     tag_data shape (dword size at +0x00, data pointer at +0x0c).
 *   - `mov ecx,[eax] / test ecx,ecx / jle`: the element's size dword is
 *     loaded BEFORE the store below and reused for it, so the length is not
 *     re-read after the data pointer is taken; reproduced with a local.
 *   - `mov eax,[eax+0xc] / mov byte [ecx+eax-1],0`: force-terminates the
 *     string data in place at data[size - 1], then returns that pointer.
 *   - Every rejecting branch falls to `mov eax,esi`, where ESI was loaded
 *     with 0x2b4560 in the prologue — the address of the "<missing string>"
 *     literal.
 *
 * Unknown: whether offsets +0x04/+0x08/+0x10 of the element are used
 * elsewhere; this function touches only +0x00 and +0x0c, so the element is
 * accessed as raw dwords rather than through an invented struct.
 */
char *FUN_0019d3c0(int index, short param_2)
{
  int *tag;
  int *element;
  int size;
  char *data;

  if (index != NONE) {
    tag = (int *)tag_get(0x73747223, index);
    if (param_2 >= 0 && (int)param_2 < *tag) {
      element = (int *)tag_block_get_element(tag, (int)param_2, 0x14);
      size = *element;
      if (size > 0) {
        data = (char *)element[3];
        data[size - 1] = '\0';
        return data;
      }
    }
  }

  return "<missing string>";
}

/* 0x19d420 — UTF-16 string-list counterpart of FUN_0019d3c0.
 * PAL unicode_string_list_get_string and the 2276 disassembly agree on the
 * signed 16-bit index, 0x14-byte entry, and in-place UTF-16 terminator.
 * The low bit of the byte size is discarded by the original SHR before the
 * final word store.  The fallback is the original wide-string address.
 */
int FUN_0019d420(int tag_index, int16_t string_index)
{
  int *list;
  int *entry;
  int size;
  uint16_t *data;

  if (tag_index != NONE) {
    list = (int *)tag_get(0x75737472, tag_index);
    if (string_index >= 0 && (int)string_index < *list) {
      entry = (int *)tag_block_get_element(list, (int)string_index, 0x14);
      size = *entry;
      if (size > 0) {
        data = (uint16_t *)entry[3];
        data[(size >> 1) - 1] = 0;
        return (int)data;
      }
    }
  }

  return 0x2b4574;
}

/* Shared unsigned-compare bound for the u* buffer helpers below
 * (umemchr, umemcmp): both guard asserts name this identifier verbatim
 * ("count < MAXIMUM_MEMCMP_SIZE" / "(count >= 0) && (count <=
 * MAXIMUM_MEMCMP_SIZE)") and both compile to a single unsigned compare
 * against the literal 0x10000000 (CMP EDI,0x10000000 / JB resp. JBE) --
 * confirmed identical immediate at both call sites via direct XBE
 * disassembly. */
#define MAXIMUM_MEMCMP_SIZE 0x10000000

/* 0x19d480 — Validate buffer/count then forward to the CRT memchr.
 *
 * Confirmed: two guard asserts recovered verbatim from the binary, both
 *            attributed to c:\halo\SOURCE\text\unicode.c (a different
 *            original TU than the rest of this file's asserts, which is
 *            why the literal file string differs from
 *            international_strings.c below):
 *              line 0x54 "buffer"                      -> buffer != NULL
 *              line 0x55 "count < MAXIMUM_MEMCMP_SIZE"  -> unsigned compare
 *                (CMP EDI,0x10000000 / JB) against MAXIMUM_MEMCMP_SIZE.
 * Confirmed: no EAX fixup after the CALL — _memchr's return value (a
 *            pointer into buffer, or NULL) is this function's return value.
 * Confirmed: call args via disassembly PUSH order (PUSH count; PUSH c;
 *            PUSH buffer -> cdecl call _memchr(buffer, c, count)).
 */
void *umemchr(void *buffer, int c, size_t count)
{
  if (!(buffer)) {
    display_assert("buffer", "c:\\halo\\SOURCE\\text\\unicode.c", 0x54, 1);
    system_exit(-1);
  }
  if (!(count < MAXIMUM_MEMCMP_SIZE)) {
    display_assert("count < MAXIMUM_MEMCMP_SIZE",
                   "c:\\halo\\SOURCE\\text\\unicode.c", 0x55, 1);
    system_exit(-1);
  }

  return _memchr(buffer, c, count);
}

/* 0x19d4f0 — Validate dest/src/count/non-overlap then forward to csmemcpy.
 *
 * Confirmed via direct XBE disassembly (the fingerprinted Ghidra artifact
 * for this address carried no decompile/callees/call-site audit — every
 * field held a "Ghidra is not reachable" error — so the bytes below are
 * read straight from the pristine XBE over
 * tools/verify/function_bounds.json's [0x19d4f0, 0x19d584) span):
 *   0019d4f0 push ebp / mov ebp,esp / push ebx / mov ebx,[ebp+0xc]
 *            / push esi / mov esi,[ebp+8] / test esi,esi / push edi
 *            / je 0x19d504 ; test ebx,ebx / jne 0x19d521
 *              -> combined short-circuit: falls into the assert block
 *                 when dest==0 OR src==0.
 *   0019d504..0019d51e: display_assert("dest && src",
 *              "c:\\halo\\SOURCE\\text\\unicode.c", 0x60, 1);
 *              system_exit(-1);  (strings read at 0x2b4660 / 0x2b45b4)
 *   0019d521 mov edi,[ebp+0x10] / cmp edi,0x10000000 / jb 0x19d549
 *              -> single unsigned compare; note this site is JB (strict),
 *                 unlike umemmove's JBE at 0x19d631, and the assert text
 *                 at 0x2b4628 correspondingly reads "<" not "<=".
 *   0019d52c..0019d546: display_assert("(count >= 0) && (count < "
 *              "MAXIMUM_MEMCPY_MEMMOVE_SIZE)",
 *              "c:\\halo\\SOURCE\\text\\unicode.c", 0x61, 1);
 *              system_exit(-1);
 *   0019d549 lea eax,[ebx+edi] (src+count) / cmp eax,esi (dest)
 *            / jbe 0x19d574 ; lea ecx,[esi+edi] (dest+count)
 *            / cmp ecx,ebx (src) / jbe 0x19d574
 *              -> non-overlap check, src-side term evaluated first.
 *   0019d557..0019d571: display_assert(
 *              "(((char *)src+count) <= (char *)dest) || "
 *              "(((char *)dest+count) <= (char *)src)",
 *              "c:\\halo\\SOURCE\\text\\unicode.c", 0x62, 1);
 *              system_exit(-1);  (cond string read at 0x2b45d8)
 *   0019d574 push edi(count) / push ebx(src) / push esi(dest)
 *            / call 0x8e0b0 (csmemcpy, kb.json-confirmed cdecl
 *              void *csmemcpy(void *destination, void *source, size_t size))
 *            / add esp,0xc / pop edi,esi,ebx,ebp / ret.
 *
 * Unknown: the epilogue neither sets nor clears EAX, so whether the
 * original returned csmemcpy's pointer or was declared void is not
 * decidable from these bytes; declared void here to match the umemmove /
 * umemset siblings in this TU. Codegen is identical either way.
 */
void umemcpy(void *dest, const void *src, size_t count)
{
  if (!(dest && src)) {
    display_assert("dest && src", "c:\\halo\\SOURCE\\text\\unicode.c", 0x60, 1);
    system_exit(-1);
  }
  if (!(count < MAXIMUM_MEMCPY_MEMMOVE_SIZE)) {
    display_assert("(count >= 0) && (count < MAXIMUM_MEMCPY_MEMMOVE_SIZE)",
                   "c:\\halo\\SOURCE\\text\\unicode.c", 0x61, 1);
    system_exit(-1);
  }
  if (!((((const char *)src + count) <= (const char *)dest) ||
        (((char *)dest + count) <= (const char *)src))) {
    display_assert("(((char *)src+count) <= (char *)dest) || "
                   "(((char *)dest+count) <= (char *)src)",
                   "c:\\halo\\SOURCE\\text\\unicode.c", 0x62, 1);
    system_exit(-1);
  }

  csmemcpy(dest, (void *)src, count);
}

/* 0x19d590 — Validate buffer1/buffer2/count then forward to csmemcmp.
 *
 * Confirmed via direct XBE disassembly (Ghidra MCP unreachable this
 * session; artifact cache had no decompile/callees for this address, so
 * the disassembly below is read straight from the pristine XBE at
 * tools/verify/function_bounds.json's [0x19d590, 0x19d5f9) span):
 *   0019d590 push ebp / mov ebp,esp / push ebx / mov ebx,[ebp+0xc]
 *            / push esi / mov esi,[ebp+8] / test esi,esi / push edi
 *            / je 0x19d5a4 ; test ebx,ebx / jne 0x19d5c1
 *              -> combined short-circuit: falls into the assert block
 *                 when buffer1==0 OR buffer2==0.
 *   0019d5a4..0019d5be: display_assert("buffer1 && buffer2",
 *              "c:\\halo\\SOURCE\\text\\unicode.c", 0x6d, 1);
 *              system_exit(-1);  (string literals read at 0x2b469c /
 *              0x2b45b4, matching umemchr's file string above)
 *   0019d5c1 mov edi,[ebp+0x10] / cmp edi,0x10000000 / jbe 0x19d5e9
 *              -> single unsigned compare (the ">=0" half of the
 *                 assert text is a tautology for size_t and folds away)
 *   0019d5cc..0019d5e6: display_assert("(count >= 0) && (count <= "
 *              "MAXIMUM_MEMCMP_SIZE)", "c:\\halo\\SOURCE\\text\\unicode.c",
 *              0x6e, 1); system_exit(-1);  (cond string read at
 *              0x2b466c)
 *   0019d5e9 push edi(count) / push ebx(buffer2) / push esi(buffer1)
 *            / call 0x8da40 (csmemcmp, kb.json-confirmed cdecl
 *              int csmemcmp(const void *a, const void *b, int size))
 *            / add esp,0xc -> cdecl cleanup, no EAX fixup: csmemcmp's
 *              return value is this function's return value.
 */
int umemcmp(const void *buffer1, const void *buffer2, size_t count)
{
  if (!(buffer1 && buffer2)) {
    display_assert("buffer1 && buffer2", "c:\\halo\\SOURCE\\text\\unicode.c",
                   0x6d, 1);
    system_exit(-1);
  }
  if (!(count <= MAXIMUM_MEMCMP_SIZE)) {
    display_assert("(count >= 0) && (count <= MAXIMUM_MEMCMP_SIZE)",
                   "c:\\halo\\SOURCE\\text\\unicode.c", 0x6e, 1);
    system_exit(-1);
  }

  return csmemcmp(buffer1, buffer2, count);
}

/* 0x19d600 — Validate dest/src/count then forward to csmemmove.
 *
 * Confirmed via direct XBE disassembly (Ghidra MCP unreachable this
 * session; artifact cache had no decompile/callees for this address, so
 * the disassembly below is read straight from the pristine XBE at
 * tools/verify/function_bounds.json's [0x19d600, 0x19d669) span):
 *   0019d600 push ebp / mov ebp,esp / push ebx / mov ebx,[ebp+0xc]
 *            / push esi / mov esi,[ebp+8] / test esi,esi / push edi
 *            / je 0x19d614 ; test ebx,ebx / jne 0x19d631
 *              -> combined short-circuit: falls into the assert block
 *                 when dest==0 OR src==0.
 *   0019d614..0019d62e: display_assert("dest && src",
 *              "c:\\halo\\SOURCE\\text\\unicode.c", 0x79, 1);
 *              system_exit(-1);  (strings read at 0x2b4660 / 0x2b45b4)
 *   0019d631 mov edi,[ebp+0x10] / cmp edi,0x10000000 / jbe 0x19d659
 *              -> single unsigned compare against MAXIMUM_MEMCPY_MEMMOVE_SIZE
 *                 (the ">=0" half of the assert text is a tautology for
 *                 size_t and folds away)
 *   0019d63c..0019d656: display_assert("(count >= 0) && (count <= "
 *              "MAXIMUM_MEMCPY_MEMMOVE_SIZE)",
 *              "c:\\halo\\SOURCE\\text\\unicode.c", 0x7a, 1);
 *              system_exit(-1);  (cond string read at 0x2b46b0)
 *   0019d659 push edi(count) / push ebx(src) / push esi(dest)
 *            / call 0x8dae0 (csmemmove, kb.json-confirmed cdecl
 *              void csmemmove(void *dest, const void *src, unsigned int size))
 *            / add esp,0xc -> cdecl cleanup, void return (no EAX fixup).
 */
void umemmove(void *dest, const void *src, size_t count)
{
  if (!(dest && src)) {
    display_assert("dest && src", "c:\\halo\\SOURCE\\text\\unicode.c", 0x79, 1);
    system_exit(-1);
  }
  if (!(count <= MAXIMUM_MEMCPY_MEMMOVE_SIZE)) {
    display_assert("(count >= 0) && (count <= MAXIMUM_MEMCPY_MEMMOVE_SIZE)",
                   "c:\\halo\\SOURCE\\text\\unicode.c", 0x7a, 1);
    system_exit(-1);
  }

  csmemmove(dest, src, count);
}

/* 0x19d670 — Validate buffer/count then forward to csmemset.
 *
 * Confirmed via direct XBE disassembly (Ghidra MCP unreachable this
 * session; artifact cache had no decompile/callees for this address, so
 * the disassembly below is read straight from the pristine XBE at
 * tools/verify/function_bounds.json's [0x19d670, 0x19d6d9) span):
 *   0019d670 push ebp / mov ebp,esp / push esi / mov esi,[ebp+8]
 *            / test esi,esi / push edi / jne 0x19d69c
 *              -> falls into the assert block when buffer==0.
 *   0019d67c..0019d699: display_assert("buffer",
 *              "c:\\halo\\SOURCE\\text\\unicode.c", 0x85, 1);
 *              system_exit(-1);  (strings read at 0x267900 / 0x2b45b4)
 *   0019d69c mov edi,[ebp+0x10] / cmp edi,0x10000000 / jbe 0x19d6c7
 *              -> single unsigned compare against MAXIMUM_MEMSET_SIZE
 *                 (the ">=0" half of the assert text is a tautology for
 *                 size_t and folds away)
 *   0019d6a7..0019d6c4: display_assert("(count >= 0) && (count <= "
 *              "MAXIMUM_MEMSET_SIZE)", "c:\\halo\\SOURCE\\text\\unicode.c",
 *              0x86, 1); system_exit(-1);  (cond string read at 0x2b46e8
 *              -- a distinct macro name from umemcmp's MAXIMUM_MEMCMP_SIZE
 *              and umemmove's MAXIMUM_MEMCPY_MEMMOVE_SIZE, though all
 *              three share the same 0x10000000 literal)
 *   0019d6c7 mov eax,[ebp+0xc] / push edi(count) / push eax(c)
 *            / push esi(buffer) / call 0x8db80 (csmemset, kb.json-confirmed
 *              cdecl void *csmemset(void *buffer, int c, size_t size))
 *            / add esp,0xc -> cdecl cleanup, no EAX fixup: csmemset's
 *              return value (buffer) is this function's return value.
 */
#define MAXIMUM_MEMSET_SIZE 0x10000000

void *umemset(void *buffer, int c, size_t count)
{
  if (!(buffer)) {
    display_assert("buffer", "c:\\halo\\SOURCE\\text\\unicode.c", 0x85, 1);
    system_exit(-1);
  }
  if (!(count <= MAXIMUM_MEMSET_SIZE)) {
    display_assert("(count >= 0) && (count <= MAXIMUM_MEMSET_SIZE)",
                   "c:\\halo\\SOURCE\\text\\unicode.c", 0x86, 1);
    system_exit(-1);
  }

  return csmemset(buffer, c, count);
}

/* 0x19d6e0 — Validate source length and dest/src non-overlap, then forward to
 * the CRT wide-string copy.
 *
 * Confirmed via direct XBE disassembly (the fingerprinted Ghidra artifact for
 * this target carries only {"error": "Ghidra is not reachable ..."} in its
 * decompile/disassembly/callers/xrefs fields, so the evidence below is read
 * straight from the pristine XBE — md5 c7869590a1c64ad034e49a5ee0c02465 — over
 * tools/verify/function_bounds.json's [0x19d6e0, 0x19d75c) span):
 *   0019d6e0 push ebp / mov ebp,esp / push ebx / push esi / push edi
 *   0019d6e6 mov edi,[ebp+0xc] (src) / push edi / call 0x1db11e (_wcslen)
 *            / mov esi,eax (source_size) / add esp,4 -> cdecl 1-arg cleanup.
 *   0019d6f4 cmp esi,0x8000 / jb 0x19d71c
 *              -> single unsigned compare against MAXIMUM_STRING_SIZE; the
 *                 ">= 0" half of the assert text is a tautology for size_t
 *                 and folds away, exactly as in umemset above.
 *   0019d6fc..0019d719: display_assert("(source_size >= 0) && (source_size < "
 *              "MAXIMUM_STRING_SIZE)", "c:\\halo\\SOURCE\\text\\unicode.c",
 *              0x92, 1); system_exit(-1);  (cond string read at 0x2b4754,
 *              file string at 0x2b45b4)
 *   0019d71c mov ebx,[ebp+8] (dest) -- note dest is loaded only after the
 *              first assert block; that is scheduling, not a dependency.
 *   0019d71f lea eax,[edi+esi*2] (src + source_size, wchar_t arithmetic)
 *            / cmp eax,ebx / jb 0x19d74d      -> (src+source_size) < dest
 *   0019d726 lea ecx,[ebx+esi*2] (dest + source_size)
 *            / cmp ecx,edi / jb 0x19d74d      -> (dest+source_size) < src
 *              Both are JB (unsigned), and either one alone skips the assert,
 *              so the guard is the || form the assert string spells out.
 *   0019d72d..0019d74a: display_assert("((src+source_size) < dest) || ((dest +"
 *              " source_size) < src)", "c:\\halo\\SOURCE\\text\\unicode.c",
 *              0x93, 1); system_exit(-1);  (cond string read at 0x2b4718)
 *   0019d74d push edi(src) / push ebx(dest) / call 0x1db180 / add esp,8
 *              -> cdecl (dest, src). 0x1db180 disassembles as a plain
 *                 word-at-a-time copy loop returning [esp+4], i.e.
 *                 wchar_t *_wcscpy(wchar_t *dest, const wchar_t *src); its
 *                 EAX is discarded here (no fixup before the epilogue), so
 *                 this function returns void.
 */
/* UNRESOLVED: the assert text proves Bungie's macro here is spelled
 * MAXIMUM_STRING_SIZE, but the CMP at 0x19d6f4 proves its value in unicode.c
 * is 0x8000, while src/common.h's MAXIMUM_STRING_SIZE is 0x2000. Whether
 * unicode.c shadowed the common.h macro or the two are genuinely distinct
 * constants is not established by this function's bytes, so the local literal
 * is kept under a distinct name rather than silently redefining the shared
 * macro or bending this call site to 0x2000. */
#define UNICODE_C_MAXIMUM_STRING_SIZE 0x8000

void align_to_character(wchar_t *dest, const wchar_t *src)
{
  size_t source_size;

  source_size = _wcslen(src);
  if (!(source_size < UNICODE_C_MAXIMUM_STRING_SIZE)) {
    display_assert("(source_size >= 0) && (source_size < MAXIMUM_STRING_SIZE)",
                   "c:\\halo\\SOURCE\\text\\unicode.c", 0x92, 1);
    system_exit(-1);
  }
  if (!((src + source_size) < dest || (dest + source_size) < src)) {
    display_assert("((src+source_size) < dest) || ((dest + source_size) < src)",
                   "c:\\halo\\SOURCE\\text\\unicode.c", 0x93, 1);
    system_exit(-1);
  }

  _wcscpy(dest, src);
}

/* 0x19d760 — append `src` to `dest` after validating both strings.
 *
 * Confirmed from the pristine-XBE disassembly of [0x19d760, 0x19d801):
 *   - Both parameters are loaded into ESI ([ebp+8] = dest) and EDI
 *     ([ebp+0xc] = src) up front; the NULL test is `TEST ESI,ESI / JZ` then
 *     `TEST EDI,EDI / JNZ`, i.e. the short-circuit `dest && src`.
 *   - Each failing check calls display_assert(reason, file, line, 1)
 *     followed by `PUSH -1 / CALL 0x8e2f0` (system_exit), lines 0x9d/0x9e/0x9f
 *     of c:\halo\SOURCE\text\unicode.c.
 *   - Both length checks are `CALL 0x1db11e` (_wcslen) with a single cdecl
 *     push and `ADD ESP,4`, then `CMP EAX,0x8000 / JC` — an UNSIGNED
 *     compare, so the length is kept in a size_t. dest is measured first,
 *     src second; the same local is reused for both.
 *   - The tail is `PUSH EDI / PUSH ESI / CALL 0x1db156 / ADD ESP,8`, i.e.
 *     cdecl _wcscat(dest, src). Its EAX result is discarded with no fixup
 *     before the epilogue, so this function returns void.
 */
void ustrcat(wchar_t *dest, const wchar_t *src)
{
  size_t string_size;

  if (!dest || !src) {
    display_assert("dest && src", "c:\\halo\\SOURCE\\text\\unicode.c", 0x9d, 1);
    system_exit(-1);
  }

  string_size = _wcslen(dest);
  if (!(string_size < UNICODE_C_MAXIMUM_STRING_SIZE)) {
    display_assert("wcslen(dest) < MAXIMUM_STRING_SIZE",
                   "c:\\halo\\SOURCE\\text\\unicode.c", 0x9e, 1);
    system_exit(-1);
  }

  string_size = _wcslen(src);
  if (!(string_size < UNICODE_C_MAXIMUM_STRING_SIZE)) {
    display_assert("wcslen(src) < MAXIMUM_STRING_SIZE",
                   "c:\\halo\\SOURCE\\text\\unicode.c", 0x9f, 1);
    system_exit(-1);
  }

  _wcscat(dest, src);
}
