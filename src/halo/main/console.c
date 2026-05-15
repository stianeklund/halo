/*
 * main/console.c — in-game debug console
 * XBE source: c:\halo\SOURCE\main\console.c
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0xff400  console_initialize
 *   0xff460  console_initialize_for_new_map
 *   0xff4d0  console_printf
 *   0xff550  console_warning
 *   0xff9c0  console_dispose
 */

#include "common.h"

/* Console state globals */
static char *console_prompt(void)
{
  return (char *)0x46cff8;
}

static char *console_input_buffer(void)
{
  return (char *)0x46d018;
}

static int16_t *console_history_head(void)
{
  return (int16_t *)0x46d91e;
}

static int16_t *console_history_count(void)
{
  return (int16_t *)0x46d91c;
}

static int16_t *console_history_browse_index(void)
{
  return (int16_t *)0x46d920;
}

static char *console_is_open(void)
{
  return (char *)0x46cf60;
}

static char *console_terminal_state(void)
{
  return (char *)0x46cf64;
}

static char *console_telnet_enabled(void)
{
  return (char *)0x46d924;
}

/*
 * console_initialize — set up initial console state.
 *
 * Copies 16 bytes of default console color from 0x31f9b8 to 0x46cfe8,
 * initializes the prompt string, and resets history indices.
 *
 * Confirmed: qmemcpy-like 4-dword copy from 0x31f9b8 to 0x46cfe8.
 * Confirmed: CALL 0x8dff0 (csstrcpy) with "halo( ".
 * Confirmed: history head = 0xffff, count = 0, browse = 0xffff.
 */
void console_initialize(void)
{
  *(uint32_t *)0x46cfe8 = *(uint32_t *)0x31f9b8;
  *(uint32_t *)0x46cfec = *(uint32_t *)0x31f9bc;
  *(uint32_t *)0x46cff0 = *(uint32_t *)0x31f9c0;
  *(uint32_t *)0x46cff4 = *(uint32_t *)0x31f9c4;
  csstrcpy(console_prompt(), "halo( ");
  *console_input_buffer() = 0;
  *console_history_head() = -1;
  *console_history_count() = 0;
  *console_history_browse_index() = -1;
}

/* console_initialize_for_new_map — no-op stub. */
void console_initialize_for_new_map(void)
{
}

/* Flush and close the console terminal if it's currently active. */
void console_flush(void)
{
  if (*(uint8_t *)0x46cf60 != 0) {
    terminal_dispose((void *)0x46cf64);
    *(uint8_t *)0x46cf60 = 0;
  }
}

/* Returns true if the console is currently open/active. */
bool console_is_active(void)
{
  return *(uint8_t *)0x46cf60;
}

/*
 * console_printf — print a formatted message to the console.
 *
 * If channel is non-zero, shows the terminal overlay first. Formats
 * the message with vsprintf into a 1024-byte stack buffer, outputs it
 * via terminal_output, and optionally logs to telnet.
 *
 * Confirmed: SUB ESP, 0x400 — 1024-byte buffer.
 * Confirmed: local_305 = 0 (null-terminates at byte 255 of the buffer).
 * Confirmed: CALL 0x1da209 (vsprintf) with va_list at [EBP+0x10].
 * Confirmed: CALL 0xe3a10 (terminal_output) with (NULL, format_str, buffer).
 * Confirmed: CALL 0xe34a0 (terminal_show) if channel != 0.
 * Confirmed: if telnet flag at 0x46d924, calls csstrcat +
 * debug_string_to_display.
 */
void console_printf(int channel, const char *format, ...)
{
  char buffer[1024];
  char *arglist = (char *)&format + sizeof(format);

  if (channel != 0) {
    terminal_show();
  }

  vsprintf(buffer, format, arglist);
  buffer[255] = 0;

  terminal_output(NULL, (const char *)0x257984, buffer);

  if (*console_telnet_enabled() != 0) {
    csstrcat(buffer, (const char *)0x261f2c, 0x400);
    debug_string_to_display(buffer, 1);
  }
}

/*
 * console_warning — print a warning message to the console with a
 * predefined warning color.
 *
 * Similar to console_printf but uses a fixed color pointer from
 * 0x2ee6d0 instead of NULL, and has no channel argument.
 *
 * Confirmed: no terminal_show call.
 * Confirmed: color pointer loaded from [0x2ee6d0] (indirect).
 * Confirmed: va_list starts at [EBP+0xc].
 */
void console_warning(const char *format, ...)
{
  char buffer[1024];
  char *arglist = (char *)&format + sizeof(format);

  vsprintf(buffer, format, arglist);
  buffer[255] = 0;

  terminal_output(*(void **)0x2ee6d0, (const char *)0x257984, buffer);

  if (*console_telnet_enabled() != 0) {
    csstrcat(buffer, (const char *)0x261f2c, 0x400);
    debug_string_to_display(buffer, 1);
  }
}

static char *console_history_ring(void)
{
  return (char *)0x46d124;
}

#define CONSOLE_HISTORY_SLOTS 8
#define CONSOLE_HISTORY_SLOT_SIZE 255

/* Submit the current console input buffer as a command. Advances the
 * history ring, copies the input to the new slot, resets the browse
 * index, and evaluates via hs_console_evaluate. */
void console_submit_command(void)
{
  int16_t head = *console_history_head();
  head = (head + 1) & 7;
  *console_history_head() = head;

  csstrcpy(console_history_ring() + head * CONSOLE_HISTORY_SLOT_SIZE,
           console_input_buffer());

  {
    int16_t count = *console_history_count();
    if (count + 1 < 9)
      *console_history_count() = count + 1;
    else
      *console_history_count() = 8;
  }

  *console_history_browse_index() = -1;
  hs_console_evaluate(console_input_buffer());
}

/* Returns a pointer one past the last space, '(', or '"' in the console
 * input buffer. Used to find the start of the most recent token. */
char *FUN_000ff640(void)
{
  char *result;
  char *sp;
  char *lp;
  char *qp;

  result = (char *)0x46d018;
  sp = strrchr(result, 0x20) + 1;
  lp = strrchr(result, 0x28) + 1;
  qp = strrchr(result, 0x22) + 1;
  if (result <= sp)
    result = sp;
  if (result <= lp)
    result = lp;
  if (result <= qp)
    result = qp;
  return result;
}

/*
 * console_process_enter — tab-complete the current console token.
 *
 * Finds the start of the current token (text after last ' ', '(', or '"'),
 * enumerates all matching HS tokens, prints them, then writes back the
 * longest case-insensitive common prefix to the input buffer and updates
 * the cursor position.
 */
void console_process_enter(void)
{
  char *token_array[256];
  char accum[1024];
  char *token_start;
  char *sp;
  char *lp;
  char *qp;
  int16_t token_count;
  int16_t match_len;
  int16_t cap;
  int16_t i;
  int16_t idx;
  int32_t batch_mod;
  int next_a;
  int next_b;
  int clen;
  bool large_list;

  token_start = console_input_buffer();
  sp = strrchr(console_input_buffer(), 0x20);
  lp = strrchr(console_input_buffer(), 0x28);
  qp = strrchr(console_input_buffer(), 0x22);

  if (token_start <= sp + 1)
    token_start = sp + 1;
  if (token_start <= lp + 1)
    token_start = lp + 1;
  if (token_start <= qp + 1)
    token_start = qp + 1;

  token_count = FUN_000c4580(token_start, 0xffffffff, token_array, 0x100);
  if (token_count != 0) {
    match_len = 0x7fff;
    large_list = (int16_t)token_count > 0x10;
    accum[0] = 0;
    console_printf(0, (const char *)0x25386f);

    for (idx = 0; idx < (int16_t)token_count; idx++) {
      clen = csstrlen(token_array[idx]) - 1;
      if ((uint32_t)clen < (uint32_t)(int32_t)match_len)
        cap = (int16_t)clen;
      else
        cap = match_len;

      i = 0;
      if (crt_toupper((unsigned char)token_array[idx][0]) ==
          crt_toupper((unsigned char)token_array[0][0])) {
        do {
          if ((int16_t)cap < i)
            break;
          i++;
          next_a = crt_toupper((unsigned char)token_array[0][i]);
          next_b = crt_toupper((unsigned char)token_array[idx][i]);
        } while (next_b == next_a);
      }
      match_len = i - 1;

      if (!large_list) {
        console_printf(0, token_array[idx]);
      } else {
        FUN_0008dc30(accum, token_array[idx]);
        FUN_0008dc30(accum, (char *)0x28094c);
        batch_mod = (int32_t)idx & 0x80000003;
        if (batch_mod < 0)
          batch_mod = (batch_mod - 1 | 0xfffffffc) + 1;
        if (batch_mod == 3) {
          console_printf(0, accum);
          accum[0] = 0;
        }
      }
    }

    if (large_list) {
      batch_mod = (int32_t)(int16_t)token_count - 1 & 0x80000003;
      if (batch_mod < 0)
        batch_mod = (batch_mod - 1 | 0xfffffffc) + 1;
      if (batch_mod != 3)
        console_printf(0, accum);
    }

    csstrncpy(token_start, token_array[0], (size_t)(match_len + 1));
    token_start[match_len + 1] = 0;
    *(int16_t *)0x46d11e = (int16_t)(uintptr_t)token_start + 0x2fe9 + match_len;
  }
}

/*
 * console_startup — execute commands from the init file on startup.
 *
 * Reads either "d:\init.txt" or "editor_d:\init.txt" depending on
 * whether we're in the editor, and evaluates each line as a HaloScript
 * console command. Lines are stored in the 8-slot history ring buffer.
 *
 * Confirmed: CALL 0x977f0 (game_in_editor) to select init file path.
 * Confirmed: fopen with mode "r" at 0x2658a4.
 * Confirmed: fgets(buffer, 0xc7=199, stream) into 200-byte stack buffer.
 * Confirmed: csstrtok(buffer, "\r\n\t") to strip line endings.
 * Confirmed: history ring at 0x46d124, 8 slots * 255 bytes each.
 * Confirmed: head = (head + 1) % 8, count = min(count + 1, 8).
 * Confirmed: CALL 0xc50c0 (hs_console_evaluate), logs "init: %s" on success.
 */
void console_startup(void)
{
  char buffer[200];
  void *stream;
  const char *init_path;

  if (game_in_editor()) {
    init_path = "editor_d:\\init.txt";
  } else {
    init_path = "d:\\init.txt";
  }

  stream = crt_fopen(init_path, (const char *)0x2658a4);
  if (stream == NULL)
    return;

  while (crt_fgets(buffer, 199, stream) != NULL) {
    int16_t head;
    int16_t count;
    csstrtok(buffer, "\r\n\t");

    head = (*console_history_head() + 1) & 7;
    *console_history_head() = head;

    csstrcpy(console_history_ring() + head * CONSOLE_HISTORY_SLOT_SIZE, buffer);

    count = *console_history_count() + 1;
    if (count > CONSOLE_HISTORY_SLOTS)
      count = CONSOLE_HISTORY_SLOTS;
    *console_history_count() = count;

    *console_history_browse_index() = -1;

    if (hs_console_evaluate(buffer)) {
      error(3, "init: %s", buffer);
    }
  }

  crt_fclose(stream);
}

/*
 * console_dispose — close the console if it is open.
 *
 * Confirmed: checks byte at 0x46cf60 (console_is_open).
 * Confirmed: calls terminal_dispose(0x46cf64) then clears flag.
 */
void console_dispose(void)
{
  if (*console_is_open() != 0) {
    terminal_dispose(console_terminal_state());
    *console_is_open() = 0;
  }
}

static int16_t *console_key_count(void)
{
  return (int16_t *)0x46cf64;
}

static int16_t *console_key_code(int index)
{
  return (int16_t *)(0x46cf68 + index * 4);
}

static void *console_edit_text(void)
{
  return (void *)0x46d118;
}

static char *console_hud_chat_flag(void)
{
  return (char *)0x449ef1;
}

/*
 * console_update — process keyboard input for the debug console.
 *
 * If the console is closed, checks for the tilde key (0x10) to open it.
 * If open, processes key events from the terminal state: tilde closes,
 * enter submits a command, escape/backspace cancels or closes, and
 * up/down arrows browse command history.
 *
 * Returns whether the console is currently open.
 *
 * Confirmed: input_key_is_down(0x10) to toggle open (key 0x10 identity TBD).
 * Confirmed: terminal_open(0x46cf64), terminal_dispose(0x46cf64).
 * Confirmed: key events at 0x46cf68, 4 bytes each, key_code at +2.
 * Confirmed: switch on key_code: 0x10=close, 0x1e=enter, 0x38/0x66=cancel,
 *            0x4d=history up, 0x4e=history down.
 * Confirmed: history ring indexing: (head - browse + 8) % 8 * 255.
 * Confirmed: CALL 0x97330 (edit_text_set_cursor_to_end) with 0x46d118.
 */
bool console_update(void)
{
  int16_t i;

  if (*console_is_open() == 0) {
    if (input_key_is_down(0x10) == 1 && *console_is_open() == 0) {
      *console_input_buffer() = 0;
      *console_is_open() = terminal_open(console_terminal_state());
      *console_hud_chat_flag() = 0;
    }
  } else {
    for (i = 0; i < *console_key_count(); i++) {
      if (*console_key_code(i) == -1) {
        display_assert("key->key_code!=NONE",
                       "c:\\halo\\SOURCE\\main\\console.c", 0xb8, 1);
        system_exit(-1);
      }

      switch (*console_key_code(i)) {
      case 0x10:
        if (*console_is_open() != 0) {
          terminal_dispose(console_terminal_state());
          *console_is_open() = 0;
        }
        break;

      case 0x1e:
        console_process_enter();
        break;

      case 0x38:
      case 0x66:
        if (*console_input_buffer() == 0) {
          if (*console_is_open() != 0) {
            terminal_dispose(console_terminal_state());
            *console_is_open() = 0;
          }
        } else {
          console_submit_command();
          *console_input_buffer() = 0;
        }
        break;

      case 0x4d:
        *console_history_browse_index() += 2;
        /* fall through */
      case 0x4e: {
        int16_t browse;
        int16_t max_idx;
        int idx;
        browse = *console_history_browse_index() - 1;
        if (browse < 1)
          browse = 0;
        max_idx = *console_history_count() - 1;
        if (browse > max_idx)
          browse = max_idx;
        *console_history_browse_index() = browse;

        if (browse != -1) {
          idx = ((int)*console_history_head() - (int)browse + 8) & 7;
          csstrcpy(console_input_buffer(),
                   console_history_ring() + idx * CONSOLE_HISTORY_SLOT_SIZE);
          edit_text_set_cursor_to_end(console_edit_text());
        }
        break;
      }

      default:
        break;
      }
    }
  }

  return *console_is_open();
}
