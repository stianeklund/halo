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
