/*
 * text/draw_string.c — string rendering state setup and telnet tab output
 * XBE source: c:\halo\SOURCE\text\draw_string.c
 *
 * Manages a small block of draw-string globals at 0x4d9b0c:
 *   [+0x00] int    font tag index      (0x4d9b0c)
 *   [+0x04] int    flags               (0x4d9b10)
 *   [+0x08] short  style               (0x4d9b14)
 *   [+0x0a] short  justify             (0x4d9b16)
 *   [+0x0c] float  color.alpha         (0x4d9b18)
 *   [+0x10] float  color.red           (0x4d9b1c)
 *   [+0x14] float  color.green         (0x4d9b20)
 *   [+0x18] float  color.blue          (0x4d9b24)
 *   [+0x1c] short  tab_stop_count      (0x4d9b28)
 *   [+0x1e] short  tab_stops[16]       (0x4d9b2a)
 *
 * terminal_string_process_tabs (0x130ab0) lives here even though it
 * touches telnet globals — it forwards the rendered text out over the
 * telnet debug console.  The telnet console globals base is 0x46eee0:
 *   [+0x04] int*  client endpoint      (0x46eee4)
 *   [+0x08] char  client input buffer  (0x46eee8)
 *   [+0x88] char  initialized flag     (0x46ef68)
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0x130ab0  terminal_string_process_tabs
 *   0x19b560  draw_string_set_tab_stops
 *   0x19b640  draw_string_set_color
 *   0x19b800  draw_string_set_style_justify_flags
 *   0x19b8b0  draw_string_set_font
 */

/* Telnet console globals accessed here (base 0x46eee0). */
#define tc_initialized (*(char *)0x46ef68)
#define tc_client0_ep (*(int **)0x46eee4)
#define tc_client0_buf ((char *)0x46eee8)

/* Maximum tab stops allowed (asserted in draw_string_set_tab_stops). */
#define MAXIMUM_NUMBER_OF_TAB_STOPS 16

/*
 * terminal_string_process_tabs — forward rendered text to telnet console.
 *
 * If the telnet subsystem is initialized, a client is connected, and the
 * text is non-empty, sends "\r\n" followed by the text over the TCP
 * connection.  If the client's trailing input buffer (tc_client0_buf) is
 * also non-empty it appends that too.  On any send failure the connection
 * is logged as lost and the endpoint is closed and cleared.
 *
 * Confirmed: checks 0x46ef68 (tc_initialized), 0x46eee4 (client ep),
 *            param_1 non-null and non-empty.
 * Confirmed: csstrlen (0x8df60), send_endpoint (0x82f50),
 *            error (0x8f390), destroy_endpoint (0x848c0).
 * Confirmed: CRLF prefix string at 0x261f2c = "\r\n".
 * Confirmed: error string at 0x29a87c = "connection lost to telnet client".
 */
void terminal_string_process_tabs(const char *text)
{
  int len;
  int sent;

  if (!tc_initialized)
    return;
  if (text == NULL)
    return;
  if (*text == '\0')
    return;
  if (tc_client0_ep == NULL)
    return;

  /* Send CRLF prefix. */
  sent = send_endpoint(tc_client0_ep, (const char *)0x261f2c, 2);
  if (sent <= 0)
    goto lost;

  /* Send the text itself. */
  len = csstrlen(text);
  sent = send_endpoint(tc_client0_ep, text, len);
  if (sent <= 0)
    goto lost;

  /* If the client input buffer has a pending line, echo it back. */
  if (tc_client0_buf[0] != '\0') {
    len = csstrlen(tc_client0_buf);
    sent = send_endpoint(tc_client0_ep, tc_client0_buf, len);
  }
  if (sent > 0)
    return;

lost:
  error(2, "connection lost to telnet client");
  destroy_endpoint(tc_client0_ep);
  tc_client0_ep = NULL;
}

/*
 * FUN_0019b3c0 — update text-bounds tracking globals.
 *
 * Records the min/max extents of a rendered text element for layout tracking.
 * param_5/param_6 = left/top corner (shorts); param_9/param_10 = width/height.
 * param_2 (int) is stored as an associated handle at 0x4d9b04.
 *
 * Globals (in a tightly packed block at 0x4d9af8):
 *   0x4d9afc (short) min_y   0x4d9afe (short) min_x
 *   0x4d9b00 (short) max_y   0x4d9b02 (short) max_x
 *   0x4d9b04 (int)   tag/handle
 *
 * 0x19b3c0 / draw_string.obj
 */
void FUN_0019b3c0(int param_1, int param_2, int param_3, int param_4,
                  short param_5, short param_6, int param_7, int param_8,
                  short param_9, short param_10)
{
  if (param_5 < *(short *)0x4d9afe)
    *(short *)0x4d9afe = param_5;
  if (param_6 < *(short *)0x4d9afc)
    *(short *)0x4d9afc = param_6;
  if (*(short *)0x4d9b02 < (short)(param_5 + param_9))
    *(short *)0x4d9b02 = (short)(param_5 + param_9);
  if (*(short *)0x4d9b00 < (short)(param_10 + param_6)) {
    *(short *)0x4d9b00 = (short)(param_10 + param_6);
    *(int *)0x4d9b04 = param_2;
    return;
  }
  *(int *)0x4d9b04 = param_2;
}

/*
 * FUN_0019b430 — cursor hit-test callback for text layout.
 *
 * Computes the Chebyshev (L∞) distance from the reference point at globals
 * 0x4d9af0 (ref_x, short) / 0x4d9af2 (ref_y, short) to the four edges of
 * the text element bounding box [param_5..param_5+param_9] ×
 * [param_6..param_6+param_10]. If this distance beats the current best
 * (0x4d9af6), updates it and sets the cursor position markers at 0x4d9af4 and
 * 0x4d9af8 based on which half of the element the reference point falls in.
 * Always updates 0x4d9af8.
 *
 * param_1 = pointer to text element; *(short*)(param_1+0xc) = cursor position
 * value.
 *
 * 0x19b430 / draw_string.obj
 */
void FUN_0019b430(int param_1, int param_2, int param_3, int param_4,
                  short param_5, short param_6, int param_7, int param_8,
                  short param_9, short param_10)
{
  short ref_x = *(short *)0x4d9af0;
  short dx_left = (short)param_5 - ref_x;
  short dx_right = (short)(param_5 + param_9) - ref_x;
  short dy_top = (short)param_6 - *(short *)0x4d9af2;
  short dy_bottom = (short)param_10 + dy_top;
  short max_dist;
  int edx;
  int ecx;

  if (dx_left < 0)
    dx_left = -dx_left;
  if (dx_right < 0)
    dx_right = -dx_right;
  if (dy_top < 0)
    dy_top = -dy_top;
  if (dy_bottom < 0)
    dy_bottom = -dy_bottom;

  max_dist = dx_left;
  if (max_dist <= dx_right)
    max_dist = dx_right;
  if (max_dist <= dy_top)
    max_dist = dy_top;
  if (max_dist <= dy_bottom)
    max_dist = dy_bottom;

  if (max_dist < *(short *)0x4d9af6) {
    *(short *)0x4d9af6 = max_dist;
    edx = (int)ref_x - (int)(short)param_5;
    ecx = ((int)(short)(param_5 + param_9) - (int)(short)param_5) >> 1;
    if (edx < ecx) {
      *(short *)0x4d9af4 = *(short *)0x4d9af8;
      *(short *)0x4d9af8 = *(short *)(param_1 + 0xc);
      return;
    }
    *(short *)0x4d9af4 = *(short *)(param_1 + 0xc);
    *(short *)0x4d9af8 = *(short *)(param_1 + 0xc);
    return;
  }
  *(short *)0x4d9af8 = *(short *)(param_1 + 0xc);
}

/*
 * draw_string_set_tab_stops — set the tab stop array for subsequent draws.
 *
 * Validates count is in [0, MAXIMUM_NUMBER_OF_TAB_STOPS).  Stores the
 * count at 0x4d9b28 and copies count shorts from stops to 0x4d9b2a.
 *
 * Confirmed: assert string "count>=0 && count<MAXIMUM_NUMBER_OF_TAB_STOPS"
 *            in draw_string.c line 0x15e.
 * Confirmed: cap at 0x10 (16) after assert path.
 * Confirmed: SHL EAX,1 before csmemcpy — copies count*2 bytes (shorts).
 * Confirmed: tab count stored as word at 0x4d9b28; array at 0x4d9b2a.
 */
void draw_string_set_tab_stops(void *stops, short count)
{
  if (count < 0 || count >= MAXIMUM_NUMBER_OF_TAB_STOPS) {
    display_assert("count>=0 && count<MAXIMUM_NUMBER_OF_TAB_STOPS",
                   "c:\\halo\\SOURCE\\text\\draw_string.c", 0x15e, 1);
    system_exit(-1);
    /* After assert: cap at 16 and continue. */
    if (count > 0x10) {
      *(short *)0x4d9b28 = 0x10;
      goto copy;
    }
  }
  *(short *)0x4d9b28 = count;
  if (count < 1)
    return;
copy:
  csmemcpy((void *)0x4d9b2a, stops, (int)*(short *)0x4d9b28 << 1);
}

/*
 * draw_string_set_color — set the draw-string ARGB color state.
 *
 * Validates that color is non-NULL and each of the four float components
 * (alpha, red, green, blue) is in [0.0, 1.0].  Stores the four floats
 * at 0x4d9b18..0x4d9b24 via raw dword moves (preserving bit pattern).
 *
 * Confirmed: assert "color" line 0x17a; assert per-channel lines 0x17b-0x17e.
 * Confirmed: float comparisons use x87 FCOMP against [0x2533c0]=0.0f and
 *            [0x2533c8]=1.0f.
 * Confirmed: final stores via MOV EAX,[ESI]; MOV [0x4d9b18],EAX etc.
 * Confirmed: field order in ESI: [+0]=alpha, [+4]=red, [+8]=green, [+c]=blue.
 */
void draw_string_set_color(const void *color)
{
  const float *c = (const float *)color;

  if (c == NULL) {
    display_assert("color", "c:\\halo\\SOURCE\\text\\draw_string.c", 0x17a, 1);
    system_exit(-1);
  }
  if (!(c[0] >= 0.0f && c[0] <= 1.0f)) {
    display_assert("(color->alpha >= 0.f) && (color->alpha <= 1.f)",
                   "c:\\halo\\SOURCE\\text\\draw_string.c", 0x17b, 1);
    system_exit(-1);
  }
  if (!(c[1] >= 0.0f && c[1] <= 1.0f)) {
    display_assert("(color->red >= 0.f) && (color->red <= 1.f)",
                   "c:\\halo\\SOURCE\\text\\draw_string.c", 0x17c, 1);
    system_exit(-1);
  }
  if (!(c[2] >= 0.0f && c[2] <= 1.0f)) {
    display_assert("(color->green >= 0.f) && (color->green <= 1.f)",
                   "c:\\halo\\SOURCE\\text\\draw_string.c", 0x17d, 1);
    system_exit(-1);
  }
  if (!(c[3] >= 0.0f && c[3] <= 1.0f)) {
    display_assert("(color->blue >= 0.f) && (color->blue <= 1.f)",
                   "c:\\halo\\SOURCE\\text\\draw_string.c", 0x17e, 1);
    system_exit(-1);
  }
  /* Store via raw dword copies to preserve bit-exact float representation. */
  *(int *)0x4d9b18 = *(const int *)&c[0]; /* alpha */
  *(int *)0x4d9b1c = *(const int *)&c[1]; /* red   */
  *(int *)0x4d9b20 = *(const int *)&c[2]; /* green */
  *(int *)0x4d9b24 = *(const int *)&c[3]; /* blue  */
}

/*
 * draw_string_set_style_justify_flags — set text style, justification, flags.
 *
 * Validates:
 *   flags  — bits above 3 must be clear (VALID_FLAGS, NUMBER_OF_TEXT_FLAGS=4)
 *   style  — must be -1 (plain) or in [0, NUMBER_OF_TEXT_STYLES) i.e. [0,3)
 *   justify — must be in [0, NUMBER_OF_TEXT_JUSTIFICATIONS) i.e. [0,2]
 *
 * Confirmed: TEST EBX,0xfffffff0 for flags check (line 0x19a).
 * Confirmed: style compared as word: CMP DI,-0x1; JZ ok; TEST DI,DI;
 *            JL bad; CMP DI,0x4; JL ok (line 0x19b).
 * Confirmed: justify compared as word: TEST SI,SI; JL bad; CMP SI,0x3;
 *            JL ok (line 0x19c).
 * Confirmed: stores — MOV word[0x4d9b14],DI; MOV word[0x4d9b16],SI;
 *            MOV dword[0x4d9b10],EBX.
 */
void draw_string_set_style_justify_flags(short style, short justify, int flags)
{
  if (flags & 0xfffffff0) {
    display_assert("VALID_FLAGS(flags, NUMBER_OF_TEXT_FLAGS)",
                   "c:\\halo\\SOURCE\\text\\draw_string.c", 0x19a, 1);
    system_exit(-1);
  }
  if (style != -1 && (style < 0 || style >= 4)) {
    display_assert(
      "style==_text_style_plain || (style>=0 && style<NUMBER_OF_TEXT_STYLES)",
      "c:\\halo\\SOURCE\\text\\draw_string.c", 0x19b, 1);
    system_exit(-1);
  }
  if (justify < 0 || justify >= 3) {
    display_assert(
      "justification>=0 && justification<NUMBER_OF_TEXT_JUSTIFICATIONS",
      "c:\\halo\\SOURCE\\text\\draw_string.c", 0x19c, 1);
    system_exit(-1);
  }
  /* Note: stores happen after all assertions, matching original order. */
  *(short *)0x4d9b14 = style;
  *(short *)0x4d9b16 = justify;
  *(int *)0x4d9b10 = flags;
}

/*
 * draw_string_set_font — configure the draw-string font state.
 *
 * Verifies the tag_index names a valid 'font' tag via tag_get, stores the
 * tag index, then delegates color and style/justify/flags setup.
 *
 * Confirmed: PUSH ESI (tag_index); PUSH 0x666f6e74 ('font'); CALL tag_get.
 * Confirmed: MOV [0x4d9b0c],ESI after tag_get (stores tag_index, not result).
 * Confirmed: param_5 (color) pushed first to draw_string_set_color (EBP+0x18).
 * Confirmed: style=EBP+0xc, justify=EBP+0x10, flags=EBP+0x14 →
 *            PUSH ECX(flags); PUSH EDX(justify); PUSH EAX(style);
 *            CALL draw_string_set_style_justify_flags.
 * Confirmed: single ADD ESP,0x18 cleans both preceding calls (4+12 args).
 */
void draw_string_set_font(int tag_index, int style, int justify, int flags,
                          const void *color)
{
  tag_get(0x666f6e74, tag_index); /* validate 'font' tag; result unused */
  *(int *)0x4d9b0c = tag_index;
  draw_string_set_color(color);
  draw_string_set_style_justify_flags((short)style, (short)justify, flags);
}

/*
 * FUN_0019bcc0 — resolve the effective font tag for a given style.
 *
 * If style == -1 (plain): returns tag_get("font", font_index) directly.
 * Otherwise asserts style in [0, 3], gets the font tag for font_index,
 * looks up the per-style font override at [font_tag+0x48 + style*0x10],
 * and falls back to font_index if the style entry is -1.
 * Returns the final tag_get("font", resolved_index) pointer.
 *
 * Frameless function: style@<si>, font_index@<edi>.
 *
 * 0x19bcc0 / draw_string.obj
 */
void *FUN_0019bcc0(int16_t style, int font_index)
{
  int tag_handle;
  void *font_tag;

  if (style != (int16_t)-1) {
    if (style < 0 || style >= 4) {
      display_assert(
        "style==_text_style_plain || (style>=0 && style<NUMBER_OF_TEXT_STYLES)",
        "c:\\halo\\SOURCE\\text\\draw_string.c", 0x406, 1);
      system_exit(-1);
    }
    font_tag = tag_get(0x666f6e74, font_index);
    tag_handle = *(int *)((char *)font_tag + 0x48 + (int)style * 0x10);
    if (tag_handle == -1)
      tag_handle = font_index;
  } else {
    tag_handle = font_index;
  }
  return tag_get(0x666f6e74, tag_handle);
}
