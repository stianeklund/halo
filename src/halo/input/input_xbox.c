void input_flush(void)
{
  csmemset(byte_46BA4C, 0, 0xA0u);
  csmemset(byte_46BB38, 0, 0x68u);
  csmemset(byte_46BBA0, 0, 0x68u);
  word_46BC08 = 0;
  word_46BC0A = 0;
  csmemset(dword_46BC0C, 0, 0x100u);
}

bool input_key_is_down(uint16_t key_code)
{
  int16_t key = (int16_t)key_code;
  uint8_t a, b;

  if (*(char *)0x46ba38)
    return false;
  switch (key - 0x69) {
  case 0:
    a = *(uint8_t *)0x46bb7c;
    b = *(uint8_t *)0x46bb71;
    return a < b ? b : a;
  case 1:
    a = *(uint8_t *)0x46bb84;
    b = *(uint8_t *)0x46bb7d;
    return a < b ? b : a;
  case 2:
    a = *(uint8_t *)0x46bb7e;
    b = *(uint8_t *)0x46bb82;
    return a <= b ? b : a;
  case 3:
    a = *(uint8_t *)0x46bb7f;
    b = *(uint8_t *)0x46bb81;
    return a <= b ? b : a;
  default:
    assert_halt(key >= 0 && key < 0x68);
    return ((uint8_t *)0x46bb38)[key];
  }
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

void input_set_rumble(int16_t gamepad_index, uint16_t left, uint16_t right)
{
  assert_halt(gamepad_index >= 0 && gamepad_index < MAXIMUM_GAMEPADS);
  if (!((bool (*)(int16_t))0xe0b00)(gamepad_index)) {
    *(uint16_t *)(0x46bb14 + gamepad_index * 4) = left;
    *(uint16_t *)(0x46bb16 + gamepad_index * 4) = right;
  }
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
