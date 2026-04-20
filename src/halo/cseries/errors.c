#include <stdarg.h>

/*
 * debug_string_to_display — write a debug message to d:\debug.txt.
 *
 * On the first call, writes a header block with the build version string,
 * reference function name, and address. Then appends the message to the
 * debug log file. If include_timestamp is non-zero, prepends a timestamp
 * in "MM.DD.YY HH:MM:SS  " format.
 *
 * Confirmed: first-call flag at 0x2ee76c; recursive calls for header.
 * Confirmed: fopen with mode "a+b" at 0x267f84.
 * Confirmed: localtime for timestamp; fprintf with "%02d.%02d.%02d ..." format.
 * Confirmed: fprintf(stream, "%s", message) for the actual output.
 * Confirmed: version string at 0x268150.
 */
void debug_string_to_display(const char *message, int include_timestamp)
{
  void *stream;
  void *tm;
  char buf[1024];
  int timeval;

  if (*(uint8_t *)0x2ee76c != 0) {
    *(uint8_t *)0x2ee76c = 0;
    debug_string_to_display((const char *)0x2681a4, 0);
    debug_string_to_display((const char *)0x268150, 1);
    crt_sprintf(buf, (const char *)0x268118, (const char *)0x268134);
    debug_string_to_display(buf, 1);
    crt_sprintf(buf, (const char *)0x268100, (void *)0x8f230);
    debug_string_to_display(buf, 1);
  }

  if (*(uint8_t *)0x5aa8e1 == 0)
    return;

  stream = crt_fopen("d:\\debug.txt", (const char *)0x267f84);
  if (stream == NULL)
    return;

  if (include_timestamp != 0) {
    crt_time(&timeval);
    tm = crt_localtime(&timeval);
    if (tm == NULL) {
      crt_fprintf(stream, (const char *)0x2680b8);
    } else {
      crt_fprintf(stream, (const char *)0x2680d0,
                  *(int *)((char *)tm + 0x10) + 1, *(int *)((char *)tm + 0xc),
                  *(int *)((char *)tm + 0x14) % 100, *(int *)((char *)tm + 0x8),
                  *(int *)((char *)tm + 0x4), *(int *)((char *)tm + 0x0));
    }
  }

  crt_fprintf(stream, (const char *)0x257984, message);
  crt_fclose(stream);
}

/* error() — the central error/warning handler for all subsystems.
 * priority levels:
 *   0 = fatal (calls system_exit after logging)
 *   1 = warning (sets the "has warning" flag)
 *   2 = error (rate-limited, logged to console and buffer)
 *   3 = silent (logged to debug output only, not console)
 * The error message is vsprintf-formatted into a 1024-byte local buffer,
 * "\r\n" is appended, and the result is accumulated into the global error
 * message buffer at 0x5aa8e8 (max 2048 bytes). When the buffer overflows,
 * old messages are trimmed and replaced with a truncation prefix. */
void error(unsigned __int16 priority, const char *format, ...)
{
  char buf[1024];
  char *found;
  int new_size;

  /* validate priority range */
  if (priority < 0 || priority >= 4) {
    display_assert("priority>=0 && priority<NUMBER_OF_ERROR_MESSAGE_PRIORITIES",
                   "c:\\halo\\SOURCE\\cseries\\errors.c", 0x61, 1);
    system_exit(-1);
  }

  /* rate-limit priority-2 errors when rate limiting is enabled */
  if (*(uint8_t *)0x5aa8e4 != 0 && priority == 2) {
    int now = system_milliseconds();
    if (now > *(int *)0x33618c + 900)
      *(int *)0x336190 = 0;
    *(int *)0x33618c = now;
    if (*(int *)0x336190 == 10) {
      ((void (*)(void *, const char *))0xe3a10)(
        *(void **)0x2ee6c4, "too many errors, only printing to debug.txt");
    }
    *(int *)0x336190 = *(int *)0x336190 + 1;
    if (*(int *)0x336190 >= 10)
      priority = 3;
  }

  /* reentrancy guard */
  if (*(uint8_t *)0x5aa8e3 != 0)
    return;
  *(uint8_t *)0x5aa8e3 = 1;

  /* set "has warning" flag for priority 1 */
  if (priority == 1)
    *(uint8_t *)0x5aa8e0 = 1;

  if (format != 0) {
    va_list args;
    va_start(args, format);
    vsprintf(buf, format, (char *)args);
    va_end(args);

    /* append "\r\n" to formatted message */
    ((void (*)(char *, const char *))0x8dc30)(buf, (const char *)0x261f2c);

    /* output to console (unless silent priority) */
    if (priority != 3) {
      ((void (*)(void *, const char *, char *))0xe3a10)(
        *(void **)0x2ee6c4, (const char *)0x257984, buf);
    }

    /* write to debug output */
    debug_string_to_display(buf, 1);

    /* accumulate into global error buffer */
    new_size = csstrlen(buf);

    if ((int)*(int16_t *)0x5aa8e6 + new_size >= 0x800) {
      /* buffer overflow — truncate old messages */
      int prefix_size = csstrlen("[...too many errors to print...]\r\n");
      int skip = prefix_size + 0x400 + new_size;
      int buf_len;
      int copy_size;

      if (skip < 0)
        skip = 0;
      else if (skip > (int)*(int16_t *)0x5aa8e6 - 1)
        skip = (int)*(int16_t *)0x5aa8e6 - 1;

      found = crt_strchr((const char *)(0x5aa8e8 + skip), '\n');
      if (found == (char *)0)
        buf_len = (int)*(int16_t *)0x5aa8e6;
      else
        buf_len = (int)(found - (char *)0x5aa8e8) + 1;

      copy_size = (int)*(int16_t *)0x5aa8e6 - buf_len;

      if (prefix_size + copy_size + new_size >= 0x800) {
        display_assert("prefix_size + copy_size + new_size < "
                       "ERROR_MESSAGE_BUFFER_MAXIMUM_SIZE",
                       "c:\\halo\\SOURCE\\cseries\\errors.c", 0xbf, 1);
        system_exit(-1);
      }

      /* prepend truncation marker */
      csstrncpy((char *)0x5aa8e8, "[...too many errors to print...]\r\n",
                prefix_size);

      /* copy remaining old messages after the marker */
      if (copy_size > 0) {
        ((void (*)(void *, void *, int))0x8dae0)(
          (void *)(0x5aa8e8 + prefix_size), found, copy_size);
      }

      *(int16_t *)0x5aa8e6 = (int16_t)(copy_size + prefix_size);
      *(char *)(0x5aa8e8 + copy_size + prefix_size) = 0;
    }

    if ((int)*(int16_t *)0x5aa8e6 + new_size < 0x800) {
      ((void (*)(char *, char *))0x8dff0)(
        (char *)(0x5aa8e8 + (int)*(int16_t *)0x5aa8e6), buf);
      *(int16_t *)0x5aa8e6 += (int16_t)new_size;
    }
  }

  /* fatal error: exit */
  if (priority == 0)
    system_exit(-5000);

  *(uint8_t *)0x5aa8e3 = 0;
}

bool error_occurred(void)
{
  bool occurred;

  occurred = *(uint8_t *)0x5aa8e0 != 0;
  *(uint8_t *)0x5aa8e0 = 0;
  *(int16_t *)0x5aa8e6 = 0;
  return occurred;
}
