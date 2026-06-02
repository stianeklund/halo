char *system_stristr(const char *str, const char *substr)
{
  char c;
  char sc;
  int len;

  c = *substr;
  if (c == '\0')
    return (char *)str;

  len = csstrlen(substr + 1);
  for (;;) {
    do {
      sc = *str++;
      if (sc == '\0')
        return NULL;
    } while (sc != c);
    if (((int(__cdecl *)(const char *, const char *, size_t))0x1e6596)(
          str, substr + 1, len) == 0)
      return (char *)(str - 1);
  }
}

uint32_t system_string_hash(const char *str)
{
  uint32_t crc;

  crc_new(&crc);
  crc_checksum_buffer(&crc, (void *)str, csstrlen(str));
  return crc;
}

void display_debug_string(const char *str)
{
  ((void(__stdcall *)(const char *))0x1d0370)(str);
}

void system_exit(int code)
{
  error(2, "system_exit(%d) — spinning instead of halt_and_catch_fire", code);
  for (;;) {
  }
  __builtin_unreachable();
}

int system_unique_identifiers_equal(const void *id1, const void *id2)
{
  uint8_t zeros[16];

  csmemset(zeros, 0, 16);
  if (csmemcmp(id1, zeros, 16) != 0) {
    if (csmemcmp(id1, id2, 16) == 0)
      return 1;
  }
  return 0;
}

uint32_t system_milliseconds(void)
{
  return ((uint32_t(*)(void))0x1d0581)();
}

uint32_t system_seconds(void)
{
  return crt_time(NULL);
}

void system_get_user_name(char *buffer, int16_t max_len)
{
  csstrncpy(buffer, "xbox", max_len);
}

void *system_calloc(int count, int size)
{
  return ((void *(__stdcall *)(uint32_t, uint32_t))0x1d0c48)(0x40,
                                                             count * size);
}

void *system_malloc(int size)
{
  return ((void *(__stdcall *)(uint32_t, uint32_t))0x1d0c48)(0, size);
}

void system_free(void *ptr)
{
  ((void *(__stdcall *)(void *))0x1d0c16)(ptr);
}

void *system_realloc(void *ptr, int size)
{
  if (size < 0) {
    display_assert("size>=0", "c:\\halo\\SOURCE\\cseries\\cseries_windows.c",
                   0x9c, 1);
    halt_and_catch_fire();
  }
  if (ptr == NULL) {
    if (size == 0) {
      display_assert("pointer||size",
                     "c:\\halo\\SOURCE\\cseries\\cseries_windows.c", 0x9d, 1);
      halt_and_catch_fire();
    }
    return ((void *(__stdcall *)(uint32_t, uint32_t))0x1d0c48)(0, size);
  }
  if (size != 0)
    return ((void *(__stdcall *)(void *, uint32_t, uint32_t))0x1d0c65)(ptr,
                                                                       size, 2);

  ((void(__stdcall *)(void *))0x1d0c16)(ptr);
  return NULL;
}

uint32_t system_get_used_memory_size(uint32_t *output)
{
  return ((uint32_t(__stdcall *)(void *))0x1d0c02)(output);
}

void FUN_0008e480(uint32_t *output)
{
  uint32_t status[8];

  csmemset(status, 0, 0x20);
  status[0] = 0x20;
  xbox_query_global_memory_status(status);
  csmemset(output, 0, 8);
  output[0] = status[3];
  output[1] = status[2];
}

const char *system_exception_name(uint32_t code /* @<ecx> */)
{
  if (code < 0xc0000090) {
    if (code == 0xc000008f)
      return "EXCEPTION_FLT_INEXACT_RESULT";
    if (code < 0xc0000026) {
      if (code == 0xc0000025)
        return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
      if (code < 0x80000005) {
        if (code == 0x80000004)
          return "EXCEPTION_SINGLE_STEP";
        if (code == 0x80000002)
          return "EXCEPTION_DATATYPE_MISALIGNMENT";
        if (code == 0x80000003)
          return "EXCEPTION_BREAKPOINT";
      } else {
        if (code == 0xc0000005)
          return "EXCEPTION_ACCESS_VIOLATION";
      }
    } else {
      if (code == 0xc000008c)
        return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
      if (code == 0xc000008d)
        return "EXCEPTION_FLT_DENORMAL_OPERAND";
      if (code == 0xc000008e)
        return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    }
  } else {
    switch (code) {
    case 0xc0000090:
      return "EXCEPTION_FLT_INVALID_OPERATION";
    case 0xc0000091:
      return "EXCEPTION_FLT_OVERFLOW";
    case 0xc0000092:
      return "EXCEPTION_FLT_STACK_CHECK";
    case 0xc0000093:
      return "EXCEPTION_FLT_UNDERFLOW";
    case 0xc0000094:
      return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case 0xc0000095:
      return "EXCEPTION_INT_OVERFLOW";
    case 0xc0000096:
      return "EXCEPTION_PRIV_INSTRUCTION";
    }
  }
  return NULL;
}
