void input_abstraction_dispose(void)
{
  csmemset((void *)0x46b820, 0, 0xdc);
}

void input_abstraction_initialize(void)
{
  int i;
  char *entry;

  csmemset((void *)0x46b820, 0, 0xdc);
  entry = (char *)0x46b828;
  for (i = 0; i < 4; i++) {
    *(float *)(entry - 8) = 120.0f;
    *(float *)(entry - 4) = 60.0f;
    entry[0] = 0;
    entry[1] = 4;
    entry[2] = 2;
    entry[3] = 3;
    entry[4] = 1;
    entry[5] = 5;
    entry[6] = 6;
    entry[7] = 7;
    entry[8] = 0xc;
    entry[9] = 0xd;
    entry[10] = 0xe;
    entry[11] = 0xf;
    *(int16_t *)(entry + 12) = 0;
    entry[14] = 0;
    entry[15] = 0;
    *(char *)(0x46b8f4 + i) = (char)input_has_gamepad(i);
    entry += 0x18;
  }
  *(int *)0x46b8f0 = system_milliseconds();
  *(char *)0x46b8f8 = 1;
}
