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

void input_get_device_states(void)
{
  uint32_t insertions, removals;
  uint32_t mu_insertions, mu_removals;
  uint32_t change_flags;
  int *handles;
  uint32_t mask;
  int i, j;
  int result;
  char state[24];
  uint8_t *gamepad;
  int16_t *sticks;
  uint8_t raw;
  uint32_t counter;
  int16_t value;
  int normalized;

  change_flags = 0;
  result = ((int(__stdcall *)(void *, uint32_t *, uint32_t *))0x24c954)(
    (void *)0x24b29c, &insertions, &removals);
  if (result != 0) {
    handles = (int *)0x46ba3c;
    for (i = 0; i < 4; i++) {
      mask = 1 << i;
      if (removals & mask) {
        if (handles[i] == 0) {
          display_assert("input_globals.gamepad_handles[gamepad_index]",
                         "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x217, 1);
          system_exit(-1);
        }
        ((void(__stdcall *)(int))0x24c1b8)(handles[i]);
        handles[i] = 0;
      }
      if (insertions & mask) {
        if (handles[i] != 0) {
          display_assert("input_globals.gamepad_handles[gamepad_index]==NULL",
                         "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x21e, 1);
          system_exit(-1);
        }
        handles[i] = ((int(__stdcall *)(void *, int, int, int))0x24c143)(
          (void *)0x24b29c, i, 0, 0);
        if (handles[i] == 0) {
          result = ((int(__stdcall *)(void))0x1d2240)();
          error(2, "XInputOpen (gamepad) failed (#%d) during input_update()",
                result);
        }
      }
    }
    if (removals & 1)
      change_flags |= 0x1;
    if (removals & 2)
      change_flags |= 0x2;
    if (removals & 4)
      change_flags |= 0x4;
    if (removals & 8)
      change_flags |= 0x8;
    if (insertions & 1)
      change_flags |= 0x1000;
    if (insertions & 2)
      change_flags |= 0x2000;
    if (insertions & 4)
      change_flags |= 0x4000;
    if (insertions & 8)
      change_flags |= 0x8000;
  }

  result = ((int(__stdcall *)(void *, uint32_t *, uint32_t *))0x24c954)(
    (void *)0x24b218, &mu_insertions, &mu_removals);
  if (result != 0) {
    if (mu_removals & 0x1)
      change_flags |= 0x10;
    if (mu_removals & 0x10000)
      change_flags |= 0x20;
    if (mu_removals & 0x2)
      change_flags |= 0x40;
    if (mu_removals & 0x20000)
      change_flags |= 0x80;
    if (mu_removals & 0x4)
      change_flags |= 0x100;
    if (mu_removals & 0x40000)
      change_flags |= 0x200;
    if (mu_removals & 0x8)
      change_flags |= 0x400;
    if (mu_removals & 0x80000)
      change_flags |= 0x800;
    if (mu_insertions & 0x1)
      change_flags |= 0x10000;
    if (mu_insertions & 0x10000)
      change_flags |= 0x20000;
    if (mu_insertions & 0x2)
      change_flags |= 0x40000;
    if (mu_insertions & 0x20000)
      change_flags |= 0x80000;
    if (mu_insertions & 0x4)
      change_flags |= 0x100000;
    if (mu_insertions & 0x40000)
      change_flags |= 0x200000;
    if (mu_insertions & 0x8)
      change_flags |= 0x400000;
    if (mu_insertions & 0x80000)
      change_flags |= 0x800000;
  }

  ((void (*)(uint32_t))0xce840)(change_flags);

  handles = (int *)0x46ba3c;
  gamepad = (uint8_t *)0x46ba4c;
  sticks = (int16_t *)0x46ba1a;
  for (i = 0; i < 4; i++) {
    if (*handles != 0) {
      result = ((int(__stdcall *)(int, void *))0x24c3b6)(*handles, state);
      if (result < 0) {
        error(2, "XGetState (gamepad) failed (#%d) during input_update()",
              result);
      } else {
        /* analog buttons (8): smoothing with hysteresis */
        for (j = 0; j < 8; j++) {
          raw = ((uint8_t *)(state + 6))[((uint8_t *)0x281150)[j]];
          gamepad[j] = raw;
          if (raw > gamepad[j + 8]) {
            counter = gamepad[j + 0x10] + 1;
            if (counter > 255)
              counter = 255;
          } else {
            counter = 0;
          }
          gamepad[j + 0x10] = (uint8_t)counter;
          raw = gamepad[j];
          if (gamepad[j + 0x10] != 0) {
            if (raw < 0x20)
              raw = 0;
            else
              raw -= 0x20;
            if (raw > gamepad[j + 8])
              gamepad[j + 8] = raw;
          } else {
            if (raw > 0xbf)
              raw = 0xff;
            else
              raw += 0x40;
            if (raw < gamepad[j + 8])
              gamepad[j + 8] = raw;
          }
        }

        /* digital buttons (8): press duration */
        for (j = 0; j < 8; j++) {
          if (((uint8_t *)0x281158)[j] & state[4]) {
            counter = gamepad[j + 0x18] + 1;
            if (counter > 255)
              counter = 255;
          } else {
            counter = 0;
          }
          gamepad[j + 0x18] = (uint8_t)counter;
        }

        /* raw stick values */
        sticks[-1] = *(int16_t *)(state + 14);
        sticks[0] = *(int16_t *)(state + 16);
        sticks[1] = *(int16_t *)(state + 18);
        sticks[2] = *(int16_t *)(state + 20);

        /* normalize left stick X */
        value = *(int16_t *)(state + 14);
        if (value > 9000) {
          normalized = ((int)(value - 9000) * 0x7fff) / 0x5cd7;
        } else if (value < -9000) {
          normalized = -((-9000 - (int)value) * 0x8000 / 0x5cd7);
        } else {
          normalized = 0;
        }
        *(int16_t *)(gamepad + 0x20) = (int16_t)normalized;

        /* normalize left stick Y */
        value = *(int16_t *)(state + 16);
        if (value > 9000) {
          normalized = ((int)(value - 9000) * 0x7fff) / 0x5cd7;
        } else if (value < -9000) {
          normalized = -((-9000 - (int)value) * 0x8000 / 0x5cd7);
        } else {
          normalized = 0;
        }
        *(int16_t *)(gamepad + 0x22) = (int16_t)normalized;

        /* normalize right stick X */
        value = *(int16_t *)(state + 18);
        if (value > 9000) {
          normalized = ((int)(value - 9000) * 0x7fff) / 0x5cd7;
        } else if (value < -9000) {
          normalized = -((-9000 - (int)value) * 0x8000 / 0x5cd7);
        } else {
          normalized = 0;
        }
        *(int16_t *)(gamepad + 0x24) = (int16_t)normalized;

        /* normalize right stick Y */
        value = *(int16_t *)(state + 20);
        if (value > 9000) {
          normalized = ((int)(value - 9000) * 0x7fff) / 0x5cd7;
        } else if (value < -9000) {
          normalized = -((-9000 - (int)value) * 0x8000 / 0x5cd7);
        } else {
          normalized = 0;
        }
        *(int16_t *)(gamepad + 0x26) = (int16_t)normalized;
      }
    }
    handles++;
    gamepad += 0x28;
    sticks += 4;
  }
}

void input_update(void)
{
  *(char *)0x46ba38 = 0;
  if (!*(char *)0x46ba39) {
    ((void(__stdcall *)(int))0x1cfaec)(*(int *)0x46bb24);
    *(char *)0x46ba39 = 1;
  }
  ((void (*)(void))0xcfdb0)();
  ((void (*)(void *))0xce620)((void *)0x46ba4c);
  ((void (*)(void *))0xce620)((void *)0x46ba74);
  ((void (*)(void *))0xce620)((void *)0x46ba9c);
  ((void (*)(void *))0xce620)((void *)0x46bac4);
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
