/*
 * interface/terminal.c — in-game terminal overlay for console/debug output
 * XBE source: c:\halo\SOURCE\interface\terminal.c
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0xe34a0  terminal_show
 *   0xe34e0  terminal_open
 *   0xe3560  terminal_dispose
 *   0xe3940  terminal_get_line
 *   0xe39e0  terminal_update
 *   0xe3a10  terminal_output
 */

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
  edit_text_initialize(*(void **)(0x46c414) + 0x1b4);
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
