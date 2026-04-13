void terminal_update(void)
{
  if (*(uint8_t *)0x46c404 != 0) {
    ((bool (*)(void))0xe3580)();
    if (!((bool (*)(void))0xff4c0)()) {
      ((void (*)(void))0xe3640)();
    }
  }
}
