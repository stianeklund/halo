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
 *   0x19b5d0  draw_string_set_indents
 *   0x19b640  draw_string_set_color
 *   0x19b790  draw_string_get_color
 *   0x19b800  draw_string_set_style_justify_flags
 *   0x19b8b0  draw_string_set_font
 *   0x19c1b0  FUN_0019c1b0 (glyph clip-and-emit loop)
 */

/* Telnet console globals accessed here (base 0x46eee0). */
#define tc_initialized (*(char *)0x46ef68)
#define tc_client0_ep (*(int **)0x46eee4)
#define tc_client0_buf ((char *)0x46eee8)

/* Maximum tab stops allowed (asserted in draw_string_set_tab_stops). */
#define MAXIMUM_NUMBER_OF_TAB_STOPS 16

/* Word offsets into a 4 x int16_t clip rectangle {top, left, bottom, right}. */
#define RECT2D_TOP 0
#define RECT2D_LEFT 1
#define RECT2D_BOTTOM 2
#define RECT2D_RIGHT 3

/*
 * The per-glyph emit callback type (draw_string_emit_proc) lives in
 * src/types.h -- the kb.json declaration generator cannot parse an inline
 * function-pointer parameter, so it needs a plain type name. FUN_0019b3c0
 * and FUN_0019b430 below are two of the concrete implementations passed in.
 */

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
 * draw_string_set_indents — set the initial and paragraph indents.
 *
 * Both indents must be non-negative.  Stored as words at 0x4d9b4e
 * (initial) and 0x4d9b50 (paragraph).
 *
 * Confirmed: both params read as words (66 8b 75 08 = MOV SI,word ptr
 *            [EBP+8]; 66 8b 7d 0c = MOV DI,word ptr [EBP+0xc]) and tested
 *            with TEST/JGE, so both are signed 16-bit — hence the short
 *            parameter types, which is what makes VC71 emit the word load.
 * Confirmed: assert strings "initial_indent>=0" (line 0x16e) and
 *            "paragraph_indent>=0" (line 0x16f); both tails call
 *            0x8e2f0 (system_exit) with -1, not halt_and_catch_fire.
 * Confirmed: store order is [0x4d9b50] (paragraph) before [0x4d9b4e]
 *            (initial).
 */
void draw_string_set_indents(short initial_indent, short paragraph_indent)
{
  if (initial_indent < 0) {
    display_assert("initial_indent>=0", "c:\\halo\\SOURCE\\text\\draw_string.c",
                   0x16e, 1);
    system_exit(-1);
  }
  if (paragraph_indent < 0) {
    display_assert("paragraph_indent>=0",
                   "c:\\halo\\SOURCE\\text\\draw_string.c", 0x16f, 1);
    system_exit(-1);
  }
  *(short *)0x4d9b50 = paragraph_indent;
  *(short *)0x4d9b4e = initial_indent;
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
 * draw_string_get_color — read the draw-string ARGB color state into *color.
 *
 * Exact inverse of draw_string_set_color: copies the four color dwords out of
 * the globals block at 0x4d9b18..0x4d9b24 into the caller's 16-byte buffer.
 *
 * Confirmed: single cdecl stack param at [EBP+0x8], loaded into ESI.
 * Confirmed: NULL guard is TEST ESI,ESI / JNZ — assert reason "color",
 *            line 0x188, halt=1, then system_exit(-1) (noreturn, no cleanup).
 * Confirmed: copy is four plain dword MOVs (global -> [ESI+0/4/8/c]) with the
 *            EAX/ECX/EDX/EAX register rotation being MSVC scheduling only; no
 *            FPU ops appear anywhere in the function.
 * Confirmed: field order in ESI: [+0]=alpha, [+4]=red, [+8]=green, [+c]=blue.
 */
void draw_string_get_color(void *color)
{
  int *out = (int *)color;

  if (out == NULL) {
    display_assert("color", "c:\\halo\\SOURCE\\text\\draw_string.c", 0x188, 1);
    system_exit(-1);
  }
  /* Raw dword copies preserve the bit-exact float representation. */
  out[0] = *(const int *)0x4d9b18; /* alpha */
  out[1] = *(const int *)0x4d9b1c; /* red   */
  out[2] = *(const int *)0x4d9b20; /* green */
  out[3] = *(const int *)0x4d9b24; /* blue  */
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
 * draw_string_set_highlight — store the two-word highlight range.
 *
 * Confirmed: both params are read as words (MOV AX,word ptr [EBP+0x8];
 *            MOV CX,word ptr [EBP+0xc]), so both are 16-bit — hence the
 *            short parameter types, which is what makes VC71 emit the
 *            word load instead of a 32-bit one.
 * Confirmed: stores are word-sized to 0x4d9b4a then 0x4d9b4c, in that
 *            order (the reverse of the sibling draw_string_set_indents).
 * Confirmed: 8 instructions total, no asserts, no range checks, no
 *            callees, plain RET => cdecl.
 * Uncertain: parameter semantics.  There are no asserts or strings
 *            naming these values, so the names stay mechanical; the
 *            pair is presumably a start/end character range, but that
 *            is not proven by the binary.
 *
 * 0x19b8f0 / draw_string.obj
 */
void draw_string_set_highlight(short param_1, short param_2)
{
  *(short *)0x4d9b4a = param_1;
  *(short *)0x4d9b4c = param_2;
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

/*
 * FUN_0019bd30 — initialise a draw-string tokenizer/render state block.
 *
 * Validates style and justification, fills the state block, packs the four
 * float colour components into one 8-bit-per-channel word, and resolves the
 * font table for (style, font_index).
 *
 * State block layout (offsets corroborated by FUN_0019c0a0 below, which
 * consumes the same struct):
 *   +0x00 int      font_index
 *   +0x04 void *   font table, from FUN_0019bcc0(style, font_index)
 *   +0x08 int *    buffer      (the wide-char string; +8 per FUN_0019c0a0)
 *   +0x0c int16_t  pos = 0     (tokenizer cursor; +0xc per FUN_0019c0a0)
 *   +0x0e int16_t  style
 *   +0x10 int16_t  justification
 *   +0x18 int      packed colour
 * +0x12 (current_char) and +0x14 (token_type) are deliberately left alone --
 * FUN_0019c0a0 writes them as it tokenizes.
 *
 * ABI: two register params. `style` arrives in AX (@<ax>) -- MOV ESI,EAX
 * @0019bd34 then CMP SI,-1, so it is read before any write and used 16-bit.
 * `state` arrives in EBX (@<ebx>) -- MOV [EBX],ECX @0019bda4 writes THROUGH it
 * with no prior write to EBX. Four cdecl stack params follow. Ghidra typed the
 * function void(void).
 *
 * Assert bounds come from the message strings themselves, so the magic numbers
 * are evidenced rather than guessed: _text_style_plain is -1 and
 * NUMBER_OF_TEXT_STYLES is 4 (CMP SI,0x4 @0019bd45);
 * NUMBER_OF_TEXT_JUSTIFICATIONS is 3 (CMP AX,0x3 @0019bd74). Both assert tails
 * call system_exit(-1), not halt_and_catch_fire.
 *
 * Colour packing is a progressive shift-or, matching the original's
 * interleaved SHL/OR chain: channel 0 ends up in the high byte, channel 3 in
 * the low byte. 255.0f is the multiplier at 0x2602c8.
 *
 * CALL 0x1d9068 at four sites is the _ftol2 intrinsic, written here as a plain
 * (int) cast -- never as a call (non-standard ABI, see the intrinsics table).
 *
 * 0x19bd30 / draw_string.obj
 */
void FUN_0019bd30(int16_t style, void *state, int *buffer, int font_index,
                  int16_t justification, float *color)
{
  int packed;

  if (style != -1 && (style < 0 || style >= 4)) {
    display_assert(
      "style==_text_style_plain || (style>=0 && style<NUMBER_OF_TEXT_STYLES)",
      "c:\\halo\\SOURCE\\text\\draw_string.c", 0x415, 1);
    system_exit(-1);
  }
  if (justification < 0 || justification >= 3) {
    display_assert(
      "justification>=0 && justification<NUMBER_OF_TEXT_JUSTIFICATIONS",
      "c:\\halo\\SOURCE\\text\\draw_string.c", 0x416, 1);
    system_exit(-1);
  }
  *(int *)state = font_index;
  *(int **)((char *)state + 8) = buffer;
  *(int16_t *)((char *)state + 0x10) = justification;
  *(int16_t *)((char *)state + 0xc) = 0;
  *(int16_t *)((char *)state + 0xe) = style;
  packed = (int)(color[0] * 255.0f);
  packed = (packed << 8) | (int)(color[1] * 255.0f);
  packed = (packed << 8) | (int)(color[2] * 255.0f);
  packed = (packed << 8) | (int)(color[3] * 255.0f);
  *(int *)((char *)state + 0x18) = packed;
  *(void **)((char *)state + 4) = FUN_0019bcc0(style, font_index);
}

/*
 * FUN_0019c0a0 — advance a wide-char string tokenizer by one character.
 *
 * Reads the next wide character (int16_t) from state->buffer[pos], stores it
 * in state->current_char (+0x12), increments state->pos (+0xc), then
 * classifies and stores the token type at state->token_type (+0x14):
 *   '\0' (0)    → type 0 (end of string)
 *   '\t' (9)    → type 3 (tab)
 *   '\r' (13)   → type 1 (newline)
 *   '|n' (7c 6e) → type 1, char = '\r' (escape sequence for newline)
 *   other        → type 6 (printable/other)
 * Returns the token type.
 *
 * state@<eax>: pointer to { ...; int *buffer (+8); short pos (+0xc);
 *              short current_char (+0x12); short token_type (+0x14); ... }
 *
 * 0x19c0a0 / draw_string.obj
 */
int16_t FUN_0019c0a0(void *state)
{
  char *s = (char *)state;
  short pos;
  int16_t c;
  int16_t c2;

  pos = *(short *)(s + 0xc);
  c = *(int16_t *)(*(int *)(s + 0x8) + (int)pos * 2);
  *(int16_t *)(s + 0x12) = c;
  pos = (short)(pos + 1);
  *(short *)(s + 0xc) = pos;

  switch ((unsigned short)c) {
  case 0:
    *(int16_t *)(s + 0x14) = 0;
    return *(volatile int16_t *)(s + 0x14);
  case 9:
    *(int16_t *)(s + 0x14) = 3;
    return *(volatile int16_t *)(s + 0x14);
  case 0xd:
    *(int16_t *)(s + 0x14) = 1;
    return *(volatile int16_t *)(s + 0x14);
  case 0x7c:
    c2 = *(int16_t *)(*(int *)(s + 0x8) + (int)pos * 2);
    pos = (short)(pos + 1);
    *(short *)(s + 0xc) = pos;
    if (c2 == 0x6e) {
      *(int16_t *)(s + 0x12) = 0xd;
      *(int16_t *)(s + 0x14) = 1;
      return *(volatile int16_t *)(s + 0x14);
    }
    /* fall through */
  default:
    *(int16_t *)(s + 0x14) = 6;
    break;
  }
  return *(volatile int16_t *)(s + 0x14);
}

/*
 * FUN_0019c1b0 — clip a character range of a string and emit one glyph at a
 * time through a caller-supplied blitter.
 *
 * Intersects up to two clip rectangles, seeds a tokenizer state block via
 * FUN_0019bd30, then walks characters [first, last) of the string. For each
 * character parse_string advances the tokenizer, FUN_0019cff0 resolves the
 * glyph, and the glyph's destination rectangle is clipped against the
 * intersection before the blitter is called. Fully clipped-away glyphs still
 * advance the pen.
 *
 * ABI: one register param plus seven cdecl stack params (ADD ESP,0x1C at the
 * single call site @0019c88c). `clip_a` arrives in EAX (@<eax>): TEST EAX,EAX
 * @0019c1b6 reads it before any write, MOV CX,[EAX+2] @0019c1d9 dereferences
 * it, and the caller loads it with LEA EAX,[EBP-0x38] one instruction before
 * the CALL. Ghidra typed the function void(void).
 *
 * State block is the same 0x1c-byte struct FUN_0019bd30 fills; the frame is
 * SUB ESP,0x38 = 0x1c state + 0x1c of separate locals. Ghidra rendered three
 * state fields as independent locals (buffer-alias confusion, lift-learnings
 * §5): local_38 = EBP-0x34 = state+0x04 (font table), local_30 = EBP-0x2c =
 * state+0x0c (tokenizer pos), and the EBP-0x26 read = state+0x12
 * (current character). Reading them from `state` is what makes the loop
 * terminate: nothing here increments the counter -- parse_string advances
 * state+0x0c, exactly as FUN_0019c0a0 does.
 *
 * Confirmed: the clip pairing is (+2 left, +6 right) and (+0 top, +4 bottom) --
 *            CMP SI,DX @0019c25f and CMP DI,BX @0019c268 test those two pairs
 *            for an empty rectangle, so +2/+6 and +0/+4 are the opposing edges.
 * Confirmed: FUN_0019cff0(state->font_table, state->current_char) returns a
 *            glyph descriptor or NULL; fields read are +2 (pen advance),
 *            +4 (width), +6 (height), +8 (origin x), +0xa (origin y).
 * Confirmed: 0x4d9b4a/0x4d9b4c are a half-open *character index* range and the
 *            effect is a highlight -- CMP AX,[0x4d9b4a] / CMP AX,[0x4d9b4c]
 *            @0019c2b0 test the tokenizer position and select an XOR 0xFFFFFF
 *            of the colour. This upgrades the "Uncertain: parameter semantics"
 *            note on draw_string_set_highlight above to confirmed.
 * Uncertain: the original reuses the `clip_b` parameter slot as a scratch
 *            int16_t for the glyph height (MOV [EBP+0x10],DI @0019c311) after
 *            the rectangle intersection has finished with it, and one later
 *            read is a full dword (MOV EDI,[EBP+0x10] @0019c369) whose high
 *            half is stale pointer bits. A separate local is used here: every
 *            consumer truncates to 16 bits, so this is equivalent, but it
 *            costs a stack slot the original did not spend.
 *
 * 0x19c1b0 / draw_string.obj
 */
void FUN_0019c1b0(const uint16_t *clip_a, draw_string_emit_proc emit,
                  int16_t *pen, const uint16_t *clip_b, int color, int *buffer,
                  int16_t first, int16_t last)
{
  char state[0x1c];
  int clip_left;
  int clip_top;
  int clip_bottom;
  int clip_right;
  int edge;
  int effective_color;
  int src_x;
  int src_y;
  void *glyph;
  short glyph_height;
  short pen_x;
  short dest_x;
  short dest_y;
  short width;
  short height;

  clip_left = -0x8000;
  clip_top = -0x8000;
  clip_right = 0x7fff;
  clip_bottom = 0x7fff;

  if (clip_a != 0) {
    edge = clip_a[RECT2D_LEFT];
    if ((short)edge > (short)clip_left)
      clip_left = edge;
    edge = clip_a[RECT2D_RIGHT];
    if ((short)edge < (short)clip_right)
      clip_right = edge;
    edge = clip_a[RECT2D_TOP];
    if ((short)edge > (short)clip_top)
      clip_top = edge;
    edge = clip_a[RECT2D_BOTTOM];
    if ((short)edge < (short)clip_bottom)
      clip_bottom = edge;
  }
  if (clip_b != 0) {
    edge = clip_b[RECT2D_LEFT];
    if ((short)edge > (short)clip_left)
      clip_left = edge;
    edge = clip_b[RECT2D_RIGHT];
    if ((short)edge < (short)clip_right)
      clip_right = edge;
    edge = clip_b[RECT2D_TOP];
    if ((short)edge > (short)clip_top)
      clip_top = edge;
    edge = clip_b[RECT2D_BOTTOM];
    if ((short)edge < (short)clip_bottom)
      clip_bottom = edge;
  }
  if ((short)clip_left >= (short)clip_right ||
      (short)clip_top >= (short)clip_bottom)
    return;

  FUN_0019bd30(*(short *)0x4d9b14, state, buffer, *(int *)0x4d9b0c,
               *(short *)0x4d9b16, (float *)0x4d9b18);

  *(int16_t *)(state + 0xc) = first;
  while (*(int16_t *)(state + 0xc) < last) {
    if (*(int16_t *)(state + 0xc) >= *(short *)0x4d9b4a &&
        *(int16_t *)(state + 0xc) < *(short *)0x4d9b4c)
      effective_color = color ^ 0xffffff;
    else
      effective_color = color;

    parse_string(state);
    glyph = FUN_0019cff0(*(void **)(state + 4),
                         (uint16_t) * (int16_t *)(state + 0x12));
    if (glyph == 0)
      continue;

    pen_x = pen[0];
    dest_y = (short)(pen[1] - *(int16_t *)((char *)glyph + 0xa));
    width = *(int16_t *)((char *)glyph + 4);
    src_x = 0;
    src_y = 0;
    glyph_height = *(int16_t *)((char *)glyph + 6);
    dest_x = (short)(pen_x - *(int16_t *)((char *)glyph + 8));
    pen[0] = (int16_t)(*(int16_t *)((char *)glyph + 2) + pen_x);

    if (dest_x + width > (short)clip_right)
      width = clip_right - dest_x;
    if (dest_x < (short)clip_left) {
      src_x = clip_left - dest_x;
      dest_x = (short)clip_left;
      width = width - src_x;
    }

    if (dest_y + glyph_height > (short)clip_bottom)
      height = clip_bottom - dest_y;
    else
      height = glyph_height;
    if (dest_y < (short)clip_top) {
      src_y = clip_top - dest_y;
      dest_y = (short)clip_top;
      height = height - src_y;
    }

    if ((short)width > 0 && (short)height > 0)
      emit(state, *(void **)(state + 4), glyph, effective_color, dest_x, dest_y,
           src_x, src_y, (short)width, (short)height);
  }
}
