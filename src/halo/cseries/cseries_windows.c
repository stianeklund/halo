void system_exit(int code)
{
  ((void __attribute__((noreturn)) (*)(int))0x1029a0)(code);
  __builtin_unreachable();
}

uint32_t system_milliseconds(void)
{
  return ((uint32_t(*)(void))0x1d0581)();
}
