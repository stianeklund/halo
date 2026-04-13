void input_flush(void)
{
  csmemset(byte_46BA4C, 0, 0xA0u);
  csmemset(byte_46BB38, 0, 0x68u);
  csmemset(byte_46BBA0, 0, 0x68u);
  word_46BC08 = 0;
  word_46BC0A = 0;
  csmemset(dword_46BC0C, 0, 0x100u);
}

bool input_has_gamepad(int16_t gamepad_index)
{
  assert_halt(gamepad_index >= 0 && gamepad_index < MAXIMUM_GAMEPADS);
  return ((int *)0x46ba3c)[gamepad_index] != 0;
}

void *input_get_gamepad_state(int gamepad_index)
{
  int16_t index;

  index = (int16_t)gamepad_index;
  assert_halt(index >= 0 && index < MAXIMUM_GAMEPADS);
  if (((int *)0x46ba3c)[index] != 0) {
    if (*(char *)0x46ba38)
      return (void *)0x46baec;
    return byte_46BA4C + index * 0x28;
  }
  return NULL;
}

void input_frame_begin(void)
{
  input_get_device_states();
  input_frame_tick = 1;
}

void input_frame_end(void)
{
  input_frame_tick = 0;
}
