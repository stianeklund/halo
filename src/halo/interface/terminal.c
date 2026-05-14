/*
 * interface/terminal.c — in-game terminal overlay for console/debug output
 * XBE source: c:\halo\SOURCE\interface\terminal.c
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0xe3410  terminal_remove_line
 *   0xe34a0  terminal_show
 *   0xe34e0  terminal_open
 *   0xe3560  terminal_dispose
 *   0xe3580  terminal_process_input
 *   0xe3640  terminal_age_lines
 *   0xe3940  terminal_get_line
 *   0xe39e0  terminal_update
 *   0xe3a10  terminal_output
 *   0xe3690  terminal_draw
 */

/* terminal_remove_line — unlink and free a single terminal line datum.
 *
 * Removes the entry identified by line_handle (passed via EDI register arg)
 * from the doubly-linked list maintained by [0x46c40c] (head) and
 * [0x46c410] (tail), then calls datum_delete to release the slot.
 *
 * Line datum layout (relative to datum base pointer):
 *   +0x04 = prev handle (int; -1 if this is the head)
 *   +0x08 = next handle (int; -1 if this is the tail)
 *
 * Confirmed: CALL 0x119320 (datum_get) used to resolve both next and prev.
 * Confirmed: CALL 0x1196d0 (datum_delete) with (pool, EDI=handle).
 * Confirmed: MOV [0x46c410],EAX when next==-1 (update tail to prev).
 * Confirmed: MOV [0x46c40c],EAX when prev==-1 (update head to next).
 */
void terminal_remove_line(int line_handle)
{
  char *line;
  char *neighbor;
  int prev;
  int next;

  line = (char *)datum_get(*(void **)0x46c408, line_handle);
  next = *(int *)(line + 0x8);
  prev = *(int *)(line + 0x4);

  /* Patch the next entry's prev link, or update the tail if no next. */
  if (next != -1) {
    neighbor = (char *)datum_get(*(void **)0x46c408, next);
    *(int *)(neighbor + 0x4) = prev;
  } else {
    *(int *)0x46c410 = prev;
  }

  /* Patch the prev entry's next link, or update the head if no prev. */
  if (prev != -1) {
    neighbor = (char *)datum_get(*(void **)0x46c408, prev);
    *(int *)(neighbor + 0x8) = next;
  } else {
    *(int *)0x46c40c = next;
  }

  datum_delete(*(void **)0x46c408, line_handle);
}

/* terminal_show — force the terminal overlay visible.
 *
 * Sets both display timers to -1 (infinite) and activates the terminal
 * data pool.
 *
 * Confirmed: MOV [0x46c40c], -1; MOV [0x46c410], -1.
 * Confirmed: CALL 0x119720 (data_make_valid) with [0x46c408].
 */
void terminal_show(void)
{
  if (*(uint8_t *)0x46c404 != 0) {
    *(int *)0x46c40c = -1;
    *(int *)0x46c410 = -1;
    data_make_valid(*(void **)0x46c408);
  }
}

/*
 * terminal_open — initialize a terminal state structure.
 *
 * Registers the terminal state pointer if no terminal is currently open.
 * Sets up the edit text substructure at offset 0x1b4, sets max line
 * length to 255, and initializes the key count to 0.
 *
 * Confirmed: assert "state" at line 0xc3.
 * Confirmed: global terminal_state pointer at 0x46c414.
 * Confirmed: edit_text pointer at state+0x1b4 = state+0xb4 (buffer area).
 * Confirmed: max length 0xff at state+0x1b8.
 * Confirmed: CALL 0x97440 (edit_text_initialize) with state+0x1b4.
 * Confirmed: MOV word ptr [ESI], 0 — key count at state+0.
 */
bool terminal_open(void *terminal)
{
  char *state = (char *)terminal;

  if (state == NULL) {
    display_assert("state", "c:\\halo\\SOURCE\\interface\\terminal.c", 0xc3, 1);
    system_exit(-1);
  }

  if (*(void **)0x46c414 != NULL)
    return 0;

  *(void **)0x46c414 = state;
  *(void **)(state + 0x1b4) = state + 0xb4;
  *(int16_t *)(state + 0x1b8) = 0xff;
  edit_text_initialize(*(char **)(0x46c414) + 0x1b4);
  *(int16_t *)state = 0;
  return 1;
}

/* terminal_dispose — close the terminal if it matches the active one.
 *
 * Confirmed: compares param against [0x46c414], clears if equal.
 */
void terminal_dispose(void *terminal)
{
  if (terminal == *(void **)0x46c414)
    *(void **)0x46c414 = 0;
}

/* terminal_process_input — drain the keyboard buffer into the terminal state.
 *
 * Called each frame while the terminal is active. Reads all pending
 * buffered keystrokes from the input system, stores up to 32 of them in
 * the terminal state's key array (at state+2, 4 bytes per entry), and
 * forwards each one to the edit_text widget at state+0x1b4.
 *
 * Also tracks a "key activity" flag at [0x46c418] and the timestamp of
 * the last keystroke at [0x46c41c]. If no new keys arrived but 30+ ticks
 * have elapsed since the last one, the activity flag is cleared.
 *
 * Terminal state layout (relative to state pointer):
 *   +0x000 = int16 key_count  (reset to 0 at start, incremented per key)
 *   +0x002 = int32 key_array[32]  (raw keystroke dwords, up to 0x20 entries)
 *   +0x1b4 = edit_text substructure
 *
 * Globals:
 *   0x46c414 = terminal_state ptr
 *   0x46c418 = byte activity flag (set on keypress, cleared on timeout)
 *   0x46c41c = int last-key local_time_get() value
 *
 * Confirmed: XOR AL,AL / TEST ECX,ECX / JZ → returns false (AL=0) if no state.
 * Confirmed: MOV word ptr [EAX],0 — resets key_count.
 * Confirmed: CALL 0xcf620 (input_get_buffered_key) returns bool in AL.
 * Confirmed: CMP CX,0x20 — max 32 keys.
 * Confirmed: MOV dword ptr [EAX + EDX*4 + 2], ECX — key stored at state+2+i*4.
 * Confirmed: CALL 0x974a0 (edit_text_process_key) with (state+0x1b4, &key).
 * Confirmed: MOV byte ptr [0x46c418],1 ; MOV [0x46c41c],ESI.
 * Confirmed: ADD ECX,0x1e ; CMP ESI,ECX — 30-tick timeout check.
 * Confirmed: SETZ DL ; MOV [0x46c418],DL — clears flag if it was set.
 * Confirmed: MOV AL,1 ; RET — always returns true when state != NULL.
 */
bool terminal_process_input(void)
{
  char *state;
  int now;
  int key;
  int16_t key_count;

  state = *(char **)0x46c414;
  if (state == NULL)
    return 0;

  now = local_time_get();

  /* Reset the per-frame key accumulation counter. */
  *(int16_t *)state = 0;

  /* Drain the keyboard buffer: loop while keystrokes are available. */
  while (input_get_buffered_key(&key)) {
    key_count = *(int16_t *)state;

    /* Store in the key array if there is room (max 32 entries). */
    if (key_count < 0x20) {
      *(int *)(state + 2 + (int)key_count * 4) = key;
      *(int16_t *)state = key_count + 1;
    }

    /* Forward to the edit_text widget for cursor/character handling. */
    edit_text_process_key(state + 0x1b4, &key);

    /* Mark terminal as active and record the time of this keypress. */
    *(uint8_t *)0x46c418 = 1;
    *(int *)0x46c41c = now;
  }

  /* If no keys were received for 30+ ticks, clear the activity flag. */
  if (*(int *)0x46c41c + 0x1e < now) {
    *(uint8_t *)0x46c418 = (*(uint8_t *)0x46c418 == 0);
    *(int *)0x46c41c = now;
  }

  return 1;
}

/* terminal_age_lines — increment age counters and evict stale lines.
 *
 * Walks the terminal line list from head ([0x46c40c]) to tail following
 * the next pointer at line+8.  For each entry the age counter at
 * line+0x120 is incremented; if it exceeds 0x96 (150) the line is
 * removed via terminal_remove_line (called with the handle in EDI).
 *
 * Confirmed: MOV EDI,[0x0046c40c] — start from head.
 * Confirmed: CMP EDI,-1 / JZ exit — empty list check.
 * Confirmed: CALL 0x119320 (datum_get) to resolve current handle.
 * Confirmed: MOV EDX,[EAX+0x120] ; INC EDX ; MOV [EAX+0x120],EDX — age bump.
 * Confirmed: CMP ECX,0x96 / JLE — threshold is 150 (0x96).
 * Confirmed: CALL 0xe3410 (terminal_remove_line) — EDI holds the handle.
 * Confirmed: MOV ESI,[EAX+8] ; ... MOV EDI,ESI — advance to next.
 */
void terminal_age_lines(void)
{
  int handle;
  char *line;
  int next;
  int age;

  handle = *(int *)0x46c40c;
  if (handle == -1)
    return;

  do {
    line = (char *)datum_get(*(void **)0x46c408, handle);
    next = *(int *)(line + 0x8);
    age = *(int *)(line + 0x120) + 1;
    *(int *)(line + 0x120) = age;

    if (age > 0x96) {
      /* terminal_remove_line takes handle via @edi — called directly here.
       * EDI already holds the current handle from the loop variable. */
      terminal_remove_line(handle);
    }

    handle = next;
  } while (handle != -1);
}

/* terminal_get_line — allocate a new terminal line at the head of the list.
 *
 * If the pool is full (count == 32), the tail entry is removed first to
 * make room (via terminal_remove_line).  A new datum is then allocated via
 * data_new_at_index and linked at the head of the doubly-linked list
 * tracked by [0x46c40c] (head) and [0x46c410] (tail).
 *
 * Returns the datum handle of the new line, or asserts/exits if
 * data_new_at_index unexpectedly returns -1.
 *
 * Confirmed: CMP word ptr [EAX+0x2e], 0x20 — pool count check.
 * Confirmed: MOV EDI,[0x0046c410]; CALL 0xe3410 — remove tail via @edi.
 * Confirmed: CALL 0x119610 (data_new_at_index).
 * Confirmed: assert "new_line_index!=NONE" at line 0x7a.
 * Confirmed: MOV [EAX+0x4],-1; MOV [EAX+0x8],ECX — prev=-1, next=old_head.
 * Confirmed: MOV [0x46c40c],ESI — update head.
 * Confirmed: if old_head==-1 then MOV [0x46c410],ESI (tail=new), else
 *            MOV [old_head+0x4],ESI (old_head->prev = new).
 */
int terminal_get_line(void)
{
  int new_handle;
  char *new_line;
  char *old_head_line;
  int old_head;

  /* If the pool is full, evict the tail entry to make room. */
  if (*(int16_t *)(*(char **)0x46c408 + 0x2e) == 0x20) {
    terminal_remove_line(*(int *)0x46c410);
  }

  new_handle = data_new_at_index(*(void **)0x46c408);

  if (new_handle == -1) {
    display_assert("new_line_index!=NONE",
                   "c:\\halo\\SOURCE\\interface\\terminal.c", 0x7a, 1);
    system_exit(-1);
  }

  new_line = (char *)datum_get(*(void **)0x46c408, new_handle);

  /* Initialize doubly-linked list links: no prev, next = current head. */
  *(int *)(new_line + 0x4) = -1;
  old_head = *(int *)0x46c40c;
  *(int *)(new_line + 0x8) = old_head;

  /* Update head pointer to the new entry. */
  *(int *)0x46c40c = new_handle;

  if (old_head != -1) {
    /* The list was non-empty: patch old head's prev to point back to new. */
    old_head_line = (char *)datum_get(*(void **)0x46c408, old_head);
    *(int *)(old_head_line + 0x4) = new_handle;
  } else {
    /* List was empty: new entry is also the tail. */
    *(int *)0x46c410 = new_handle;
  }

  return new_handle;
}

/* terminal_update — per-frame terminal tick.
 *
 * Processes keyboard input for the terminal and ages displayed lines.
 * Skips aging when console_is_active() returns true (console is open).
 *
 * Confirmed: checks flag at 0x46c404, calls 0xe3580 (terminal_process_input)
 *            and 0xff4c0 (console_is_active), then 0xe3640
 * (terminal_age_lines). Confirmed: MOV BL,AL saves process_input result; MOV
 * AL,BL restores it. Confirmed: return type is void (caller in main_loop
 * ignores EAX).
 */
void terminal_update(void)
{
  if (*(uint8_t *)0x46c404 != 0) {
    terminal_process_input();
    if (!console_is_active()) {
      terminal_age_lines();
    }
  }
}

/*
 * terminal_output — write a formatted line to the terminal overlay.
 *
 * Allocates a terminal line from the data pool, copies color (or uses
 * default white), formats the text via vsnprintf, checks for "|t" tab
 * markers, and processes the string for display.
 *
 * Confirmed: default color {1.0f, 0.7f, 0.7f, 0.7f} on stack.
 * Confirmed: assert "format" at line 0x18d.
 * Confirmed: CALL 0xe3940 (terminal_get_line) returns datum handle.
 * Confirmed: CALL 0x119320 (datum_get) to get line data pointer.
 * Confirmed: 16-byte color copy at line+0x110.
 * Confirmed: vsnprintf(line+0xd, 0xfe, format, va_args) at 0x1db0c8.
 * Confirmed: assert if formatted length >= 0xff, line 0x19d.
 * Confirmed: strstr(text, "|t") sets flag at line+0xc.
 * Confirmed: CALL 0x130ab0 processes tab markers.
 */
void terminal_output(void *color, const char *format, const char *text)
{
  float default_color[4];
  int line_handle;
  char *line;
  char *text_buf;
  int16_t result;

  default_color[0] = 1.0f;
  default_color[1] = 0.7f;
  default_color[2] = 0.7f;
  default_color[3] = 0.7f;

  if (*(uint8_t *)0x46c404 == 0)
    return;

  line_handle = terminal_get_line();

  if (format == NULL) {
    display_assert("format", "c:\\halo\\SOURCE\\interface\\terminal.c", 0x18d,
                   1);
    system_exit(-1);
  }

  if (line_handle == -1)
    return;

  line = (char *)datum_get(*(void **)0x46c408, line_handle);
  *(int *)(line + 0x120) = 0;

  if (color == NULL)
    color = default_color;

  *(int *)(line + 0x110) = *(int *)((char *)color + 0);
  *(int *)(line + 0x114) = *(int *)((char *)color + 4);
  *(int *)(line + 0x118) = *(int *)((char *)color + 8);
  *(int *)(line + 0x11c) = *(int *)((char *)color + 12);

  text_buf = line + 0xd;
  result = crt_vsnprintf(text_buf, 0xfe, format, (char *)&text);

  if ((int16_t)(result - 1) >= 0xff) {
    csprintf((char *)0x5ab100,
             "terminal_printf call generated %d characters; the buffer "
             "size is %d characters.",
             (int)(int16_t)(result - 1), 0xff);
    display_assert((const char *)0x5ab100,
                   "c:\\halo\\SOURCE\\interface\\terminal.c", 0x19d, 1);
    system_exit(-1);
  }

  *(uint8_t *)(line + 0xc) = (crt_strstr(text_buf, "|t") != NULL);
  terminal_string_process_tabs(text_buf);
}

/* terminal_draw — render the terminal overlay (input line + scrollback).
 *
 * Draws the active input line (from DAT_0046c414) at the bottom of the
 * screen with a blinking cursor, then walks the line list from head
 * ([0x46c40c]) upward, rendering each entry with a fade effect based on
 * its age counter.
 *
 * Layout (terminal state at 0x46c414):
 *   +0x84  = draw_string color for the input line (16 bytes)
 *   +0x94  = tab stop array for the input line
 *   +0xb3  = NUL terminator inserted before copying input text
 *   +0xb4  = continuation of text buffer (csstrcpy source)
 *   +0x1b3 = NUL terminator for the input line area
 *   +0x1ba = int16 cursor position (column offset into text_buf)
 *
 * Line datum layout (at datum_get result):
 *   +0x08  = next handle (int; -1 if tail)
 *   +0x0c  = bool: has tab markers
 *   +0x0d  = text buffer (nul-terminated)
 *   +0x110 = float color[4] (r,g,b,a stored sequentially)
 *   +0x120 = int age counter (incremented each frame by terminal_age_lines)
 *
 * Globals:
 *   0x46c404 = byte active flag
 *   0x46c408 = data_t * line pool pointer
 *   0x46c40c = int  head handle (-1 if empty)
 *   0x46c414 = void* terminal state pointer
 *   0x46c418 = byte key-activity flag (cursor blink source)
 *   0x31a011 = byte show-scrollback flag
 *   0x2533c0 = float alpha floor (min)
 *   0x2533c8 = float alpha ceiling (max)
 *   0x2533d8 = float alpha start (1.0f typically)
 *   0x2546a4 = float age scale factor
 *   0x282dac = void* tab stop array for scrollback
 *   0x506584 = int32: words = {left_w, right_w} of screen rect
 *   0x506588 = int32: words = {bottom, top_ignored} of screen rect
 *   0x50657c = int32 y-offset (dy, negated for rect2d_offset)
 *   0x50657e = int16 x-offset (dx, negated for rect2d_offset)
 *
 * Confirmed: PUSH 0x1 / CALL 0xdeca0 (interface_get_tag_index) -> font index.
 * Confirmed: PUSH font / PUSH 0x666f6e74 / CALL tag_get -> font tag data.
 * Confirmed: uVar6 = *(short*)(font+4) + *(short*)(font+6) + *(short*)(font+8).
 * Confirmed: rect2d_offset called at 0xe377c and 0xe38cd with &rect, -dx, -dy.
 * Confirmed: FPU clamp at 0xe3860 (lower) and 0xe387a (upper).
 * Confirmed: CALL 0x19b8b0 (draw_string_set_font) at 0xe3795 and 0xe38f9.
 * Confirmed: CALL 0x183e60 (rasterizer_text_draw) at 0xe37e6 and 0xe390c.
 * Confirmed: cursor '_' inserted at text_buf[cursor_pos] (0xe37d2).
 * Confirmed: CALL 0x19b560 (draw_string_set_tab_stops) at 0xe38e3/0xe3918.
 */
void terminal_draw(void)
{
  char *font_tag;
  int16_t dx_base;
  char *line;
  float alpha;
  float local_color[4];
  char text_buf[288];
  int16_t rect[4]; /* [top, left, bottom, right] at EBP-8 */
  int font_tag_index;
  int line_handle;
  int cur_y;
  unsigned short line_height;
  int16_t str_len;
  int cursor_pos;
  int16_t dx;
  int dy_i32;

  font_tag_index = interface_get_tag_index(1);
  dx_base = *(int16_t *)0x50657e;
  if (*(uint8_t *)0x46c404 == 0)
    return;

  font_tag = (char *)tag_get(0x666f6e74, font_tag_index);
  line_height = *(int16_t *)(font_tag + 4) + *(int16_t *)(font_tag + 6) +
                *(int16_t *)(font_tag + 8);

  if (*(char **)0x46c414 != NULL) {
    /* -- Draw active input line at bottom of screen -- */
    text_buf[0] = '\0';
    *(uint8_t *)(*(char **)0x46c414 + 0xb3) = 0;
    FUN_0008dc30(text_buf, *(char **)0x46c414 + 0x94);
    *(uint8_t *)(*(char **)0x46c414 + 0x1b3) = 0;
    str_len = (int16_t)csstrlen(text_buf);
    csstrcpy(text_buf + str_len, *(char **)0x46c414 + 0xb4);

    /* Build rect: bottom of screen minus one line, offset by global position. */
    {
      int screen_bottom = *(int *)0x506588;
      rect[1] = *(int16_t *)0x506586;                  /* left */
      rect[2] = (int16_t)screen_bottom;                /* bottom */
      rect[3] = *(int16_t *)0x50658a;                  /* right */
      rect[0] = (int16_t)(screen_bottom - line_height); /* top */
    }

    dx = -(int16_t)dx_base;
    dy_i32 = -*(int32_t *)0x50657c;
    rect2d_offset(rect, dx, (int16_t)dy_i32);

    draw_string_set_font(font_tag_index, -1, 0, 0, *(char **)0x46c414 + 0x84);

    /* Insert blinking cursor character if key-activity flag is set. */
    if (*(uint8_t *)0x46c418 != 0) {
      cursor_pos = (int)(int16_t)(*(int16_t *)(*(char **)0x46c414 + 0x1ba) + str_len);
      if (text_buf[cursor_pos] == '\0') {
        text_buf[cursor_pos + 1] = '\0';
      }
      text_buf[cursor_pos] = '_';
    }

    rasterizer_text_draw(rect, NULL, NULL, 0, text_buf);
  }

  /* -- Draw scrollback lines -- */
  if (*(uint8_t *)0x31a011 == 0)
    return;

  cur_y = *(int *)0x506588 - (int)line_height;
  line_handle = *(int *)0x46c40c;
  if (line_handle == -1)
    return;

  /* Loop while handle is valid and there is vertical space. */
  while (line_handle != -1 &&
         (int)(int16_t)cur_y - (int)line_height > 0) {
    line = (char *)datum_get(*(void **)0x46c408, line_handle);

    /* Copy all four color components, then overwrite [0] with faded alpha. */
    local_color[0] = *(float *)(line + 0x110);
    local_color[1] = *(float *)(line + 0x114);
    local_color[2] = *(float *)(line + 0x118);
    local_color[3] = *(float *)(line + 0x11c);

    alpha = *(float *)0x2533d8 -
            (float)*(int *)(line + 0x120) * *(float *)0x2546a4;

    /* Clamp alpha to [floor, ceiling]. */
    if (alpha <= *(float *)0x2533c0) {
      alpha = *(float *)0x2533c0;
    } else if (alpha > *(float *)0x2533c8) {
      alpha = *(float *)0x2533c8;
    }
    local_color[0] = alpha * local_color[0];

    /* Build rect for this scrollback line. */
    rect[1] = *(int16_t *)0x506586;          /* left */
    rect[3] = *(int16_t *)0x50658a;          /* right */
    rect[2] = (int16_t)cur_y;                /* bottom (before decrement) */
    cur_y -= (int)line_height;              /* decrement current y */
    rect[0] = (int16_t)cur_y;              /* top (after decrement) */

    dx = -(int16_t)dx_base;
    dy_i32 = -*(int32_t *)0x50657c;
    rect2d_offset(rect, dx, (int16_t)dy_i32);

    /* Set tab stops if this line has tab markers. */
    if (*(uint8_t *)(line + 0xc) != 0) {
      draw_string_set_tab_stops((void *)0x282dac, 3);
    }

    draw_string_set_font(font_tag_index, -1, 0, 0, local_color);
    rasterizer_text_draw(rect, NULL, NULL, 0, line + 0xd);
    draw_string_set_tab_stops((void *)0x282dac, 0);

    line_handle = *(int *)(line + 8);
  }
}
