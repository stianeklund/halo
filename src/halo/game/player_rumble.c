void rumble_initialize(void)
{
  rumble_globals = (char *)game_state_malloc("rumble", 0, 0x82c);
}

void rumble_dispose(void)
{
}

void rumble_initialize_for_new_map(void)
{
  csmemset(rumble_globals, 0, 0x82c);
}

/* Set scripted (script-driven) left/right motor values for the current player.
 * Stored at rumble_globals+0x820 (left) and +0x824 (right).
 *
 * 0xb9b80 / player_rumble.obj */
void rumble_player_set_scripted_values(float left_motor, float right_motor)
{
  *(float *)(rumble_globals + 0x820) = left_motor;
  *(float *)(rumble_globals + 0x824) = right_motor;
}

/* Set the rumble scale multiplier for the current player slot.
 * Stored at rumble_globals+0x828.
 *
 * 0xb9ba0 / player_rumble.obj */
void rumble_player_set_scale(float scale)
{
  *(float *)(rumble_globals + 0x828) = scale;
}

/* Apply a rumble impulse to a unit's player slot. Finds the sub-slot with the
 * highest elapsed timer (most stale), copies the rumble definition into it,
 * then scales the motor columns by scale (lerped) and duration_scale.
 *
 * 0xb9bc0 / player_rumble.obj */
void rumble_player_impulse(short local_player_index, float *rumble_definition,
                           float scale, float duration_scale)
{
  struct rumble_definition_copy { float values[15]; };
  char *player;
  float *impulse;
  float longest;
  float motor_scale;
  int impulse_index;

  player = rumble_globals + (int)local_player_index * 0x208;
  longest = *(float *)(player + 0x1e0);
  impulse = (float *)player;

  assert_halt_msg_at("rumble_definition", "c:\\halo\\SOURCE\\game\\player_rumble.c", 0xa4, rumble_definition != NULL);

  if (longest < *(float *)(player + 0x1e4)) {
    impulse = (float *)(player + 0x3c);
    longest = *(float *)(player + 0x1e4);
  }
  if (longest < *(float *)(player + 0x1e8)) {
    impulse = (float *)(player + 0x78);
    longest = *(float *)(player + 0x1e8);
  }
  if (longest < *(float *)(player + 0x1ec)) {
    impulse = (float *)(player + 0xb4);
    longest = *(float *)(player + 0x1ec);
  }
  if (longest < *(float *)(player + 0x1f0)) {
    impulse = (float *)(player + 0xf0);
    longest = *(float *)(player + 0x1f0);
  }
  if (longest < *(float *)(player + 0x1f4)) {
    impulse = (float *)(player + 0x12c);
    longest = *(float *)(player + 0x1f4);
  }
  if (longest < *(float *)(player + 0x1f8)) {
    impulse = (float *)(player + 0x168);
    longest = *(float *)(player + 0x1f8);
  }
  if (longest < *(float *)(player + 0x1fc)) {
    impulse = (float *)(player + 0x1a4);
  }

  *(struct rumble_definition_copy *)impulse = *(struct rumble_definition_copy *)rumble_definition;

  motor_scale = (1.0f - rumble_definition[10]) * scale + rumble_definition[10];
  impulse[0] *= motor_scale;
  impulse[5] *= motor_scale;
  impulse[1] *= duration_scale;
  impulse[6] *= duration_scale;

  impulse_index = ((int)impulse - (int)player) / 0x3c;
  *(float *)(player + 0x1e0 + impulse_index * 4) = 0.0f;
}

void rumble_player_clear(int16_t local_player_index)
{
  csmemset(rumble_globals + local_player_index * 0x208, 0, 0x208);
}

/* rumble_clear_all_now (0xb9d60)
 *
 * Clears the entire rumble globals block (all 4 player slots) and then
 * calls input_set_rumble(i, 0, 0) for each gamepad that is present.
 * Unlike rumble_dispose_from_old_map, only checks input_has_gamepad — no
 * double pass. */
void rumble_clear_all_now(void)
{
  int i;

  csmemset(rumble_globals, 0, 0x82c);
  for (i = 0; i < 4; i++) {
    if (input_has_gamepad(i))
      input_set_rumble(i, 0, 0);
  }
}

/* Set direct rumble motor values for a player slot. */
void rumble_player_continuous(short local_player_index, int left_motor,
                              int right_motor)
{
  char *slot = rumble_globals + (int)local_player_index * 0x208;
  *(int *)(slot + 0x200) = left_motor;
  *(int *)(slot + 0x204) = right_motor;
}

void rumble_dispose_from_old_map(void)
{
  int i;

  for (i = 0; (int16_t)i < 4; i++)
    input_set_rumble(i, 0, 0);

  csmemset(rumble_globals, 0, 0x82c);

  for (i = 0; i < 4; i++) {
    if (input_has_gamepad(i))
      input_set_rumble(i, 0, 0);
  }
}

uint32_t rumble_calculate(char *slot)
{
  float motors[2];
  float *effect_ptr;
  float *timer_ptr;
  float timer_val;
  float blend;
  float scaled;
  uint16_t left, right;
  int i, j;

  motors[0] = *(float *)(slot + 0x200);
  motors[1] = *(float *)(slot + 0x204);
  effect_ptr = (float *)(slot + 4);
  timer_ptr = (float *)(slot + 0x1e0);

  i = 8;
  do {
    timer_val = *timer_ptr;
    for (j = 0; j < 2; j++) {
      if (timer_val < *effect_ptr) {
        blend = *(float *)0x2533c8 - timer_val / *effect_ptr;
        if (blend < *(float *)0x2533c0)
          blend = 0.0f;
        else if (blend > *(float *)0x2533c8)
          blend = 1.0f;
        motors[j] += transition_function_evaluate(
                       *(uint16_t *)((char *)effect_ptr + 4), blend) *
                     effect_ptr[-1];
      }
      effect_ptr += 5;
    }
    effect_ptr += 5;
    timer_ptr++;
    i--;
  } while (i);

  if (*(float *)(rumble_globals + 0x828) != *(float *)0x2533c0) {
    motors[0] +=
      *(float *)(rumble_globals + 0x820) * *(float *)(rumble_globals + 0x828);
    motors[1] +=
      *(float *)(rumble_globals + 0x824) * *(float *)(rumble_globals + 0x828);
  }

  scaled = motors[0] * *(float *)0x2647cc;
  if (scaled < *(float *)0x2533c0)
    scaled = 0.0f;
  else if (scaled > *(float *)0x2647cc)
    scaled = 65535.0f;
  left = (uint16_t)(int)scaled;

  scaled = motors[1] * *(float *)0x2647cc;
  if (scaled < *(float *)0x2533c0)
    scaled = 0.0f;
  else if (scaled > *(float *)0x2647cc)
    scaled = 65535.0f;
  right = (uint16_t)(int)scaled;

  return ((uint32_t)right << 16) | (uint32_t)left;
}

void rumble_update(void)
{
  int local_player_index;
  char *player;
  union {
    uint32_t packed;
    struct {
      uint16_t left;
      uint16_t right;
    } m;
  } motors;
  int player_handle;
  int controller_index;

  for (local_player_index = 0; (int16_t)local_player_index < 4; local_player_index++) {
    player = rumble_globals + local_player_index * 0x208;
    motors.packed = rumble_calculate(player);
    *(float *)(player + 0x1e0) += *(float *)0x2546a4;
    *(float *)(player + 0x1e4) += *(float *)0x2546a4;
    *(float *)(player + 0x1e8) += *(float *)0x2546a4;
    *(float *)(player + 0x1ec) += *(float *)0x2546a4;
    *(float *)(player + 0x1f0) += *(float *)0x2546a4;
    *(float *)(player + 0x1f4) += *(float *)0x2546a4;
    *(float *)(player + 0x1f8) += *(float *)0x2546a4;
    *(float *)(player + 0x1fc) += *(float *)0x2546a4;
    player_handle = local_player_get_player_index(local_player_index);
    if (player_handle != NONE) {
      controller_index = *(int16_t *)((char *)datum_get(player_data, player_handle) + 2);
      if (controller_index != NONE) {
        if (!player_ui_rumble_disabled(
              player_ui_get_single_player_local_player_from_controller(controller_index))) {
          input_set_rumble(controller_index, motors.m.left, motors.m.right);
        } else {
          input_set_rumble(controller_index, 0, 0);
        }
      }
    } else {
      input_set_rumble(local_player_index, 0, 0);
    }
  }
}
