
char *player_effect_get(int16_t local_player_index)
{
  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  assert_halt(player_effect_globals);
  return player_effect_globals + local_player_index * 0xec;
}

void player_effect_initialize(void)
{
  player_effect_globals = (char *)game_state_malloc("player effects", 0, 0x3ec);
  assert_halt(player_effect_globals);
}

void player_effect_dispose(void)
{
}

void player_effect_initialize_for_new_map(void)
{
  csmemset(player_effect_globals, 0, 0x3ec);
  *(_WORD *)(player_effect_globals + 0x3c0) = 0xFFFF;
  *(_DWORD *)(player_effect_globals + 0x3e8) = game_time_get();
}

void player_effect_dispose_from_old_map(void)
{
}

/* Forward the two script-supplied motor values to the rumble system.
 *
 * Disassembly (0xa2920..0xa2929, 9 bytes):
 *   55            PUSH EBP
 *   8B EC         MOV  EBP,ESP
 *   5D            POP  EBP
 *   E9 57 72 01 00  JMP 0xb9b80   ; rumble_player_set_scripted_values
 *
 * A pure identical-forward tail call: no argument reload, no `add esp`, and
 * — decisively — no FILD/FSTP conversion code.  MSVC only collapses a wrapper
 * to that bare JMP when the parameter list matches the callee's exactly, so
 * both parameters are `float`, like rumble_player_set_scripted_values(float,
 * float) at 0xb9b80.  The kb decl previously read `(int param_1, float
 * param_2)`, inferred from the HaloScript caller at 0xc3030 where argument 1
 * is pushed with `MOV EDX,[EAX]; PUSH EDX` while argument 2 uses the
 * `PUSH ECX; FSTP [ESP]` float idiom.  That mixed shape is MSVC scheduling,
 * not a type signal — the callee proves it: 0xb9b80 stores BOTH parameters
 * into float slots yet marshals the first with `FLD [EBP+8]; FSTP
 * [EAX+0x820]` and the second with a plain dword `MOV ECX,[EBP+0xc];
 * MOV [EAX+0x824],ECX`.  The 0xc3030 call site was corrected to read its
 * first argument as a float lvalue so the dword is still forwarded verbatim
 * instead of being run through an int-to-float conversion.
 *
 * 0xa2920 / player_effects.obj */
void scripted_player_effect_set_rumble(float left_motor, float right_motor)
{
  rumble_player_set_scripted_values(left_motor, right_motor);
}

/* player_effect_get_damage_indicators -- copy the local player's four damage
 * indicator bytes to `out`, then age each live indicator by the elapsed game
 * time, saturating at 0xff.
 *
 * Confirmed (0xa2a10..0xa2a6f):
 *   - PUSH [EBP+8] / CALL 0xa2690: the raw dword is forwarded to
 *     player_effect_get(int16_t).
 *   - LEA ESI,[EAX+0xe4]: the indicator array lives at effect+0xe4 and is
 *     4 bytes wide (PUSH 0x4 / PUSH ESI / PUSH ECX / CALL csmemcpy, ADD
 * ESP,0x10). The same +0xe4/4 window is cleared in player_effect_update.
 *   - The copy happens BEFORE the aging pass, so `out` receives the previous
 *     tick's values.
 *   - Loop is a 4-iteration countdown (MOV EDI,4 / INC ESI / DEC EDI / JNZ)
 *     that skips zero entries (CMP byte ptr [ESI],0x0 / JZ).
 *   - Saturation test is signed on the widened sum: MOVSX EDX,AX (int16_t
 *     game_time_get_elapsed) + MOVZX EAX,byte ptr [ESI], CMP EDX,0xff,
 *     JGE -> 0xff.
 *   - game_time_get_elapsed() is called a SECOND time on the non-saturating
 *     path (CALL 0x000b5ae0 at 0xa2a3d and again at 0xa2a52); the sum is
 *     recomputed rather than reused, so both calls are preserved here.
 *
 * 0xa2a10 / player_effects.obj */
void player_effect_get_damage_indicators(int player_index, void *out)
{
  unsigned char *indicators;
  int count;
  int aged;

  indicators =
    (unsigned char *)(player_effect_get((int16_t)player_index) + 0xe4);
  csmemcpy(out, indicators, 4);
  count = 4;
  do {
    if (*indicators != 0) {
      if ((int)game_time_get_elapsed() + (int)*indicators < 0xff) {
        aged = game_time_get_elapsed() + *indicators;
      } else {
        aged = 0xff;
      }
      *indicators = (unsigned char)aged;
    }
    indicators++;
    count--;
  } while (count != 0);
}

/* player_effect_clear_damage_indicators -- zero the local player's four damage
 * indicator bytes.
 *
 * Confirmed (0xa2a70..0xa2a8f):
 *   - MOV EAX,[EBP+8] / PUSH EAX / CALL 0xa2690: the raw dword is forwarded to
 *     player_effect_get(int16_t), same shape as 0xa2a10.
 *   - ADD EAX,0xe4: the same 4-byte indicator array at effect+0xe4 that
 *     player_effect_get_damage_indicators copies out of.
 *   - PUSH 0x4 / PUSH 0x0 / PUSH EAX / CALL 0x8db80 -> csmemset(buf, 0, 4).
 *   - The single ADD ESP,0x10 retires the callee argument of
 *     player_effect_get together with csmemset's three; it is not a 4-argument
 *     csmemset call.
 *
 * 0xa2a70 / player_effects.obj */
void player_effect_clear_damage_indicators(int player_index)
{
  char *effect;

  effect = player_effect_get((int16_t)player_index);
  csmemset(effect + 0xe4, 0, 4);
}

void player_effect_update(void)
{
  int16_t local_player_index;
  int player_index;
  void *player;
  char *effect;

  local_player_index = (int16_t)local_player_get_next(-1);
  while (local_player_index != -1) {
    player_index = local_player_get_player_index(local_player_index);
    if (player_index != -1) {
      player = datum_get(player_data,
                         local_player_get_player_index(local_player_index));
      if (*(int *)((char *)player + 0x34) != -1) {
        local_player_index = (int16_t)local_player_get_next(local_player_index);
        continue;
      }
    }
    effect = player_effect_get(local_player_index);
    csmemset(effect + 0xe4, 0, 4);
    csmemset(player_effect_get(local_player_index), 0, 0xec);
    rumble_clear_for_local_player(local_player_index);
    local_player_index = (int16_t)local_player_get_next(local_player_index);
  }
}

/* player_effect_set_from_descriptor -- apply an effect descriptor to a player's
 * effect state. Internal helper at 0xa2ab0.
 *
 * The original binary passes the descriptor in EBX as a register arg;
 * we pass it explicitly since all callers are in this TU.
 *
 * Confirmed: copies 56 bytes (14 dwords) from descriptor to effect+0x18.
 * Confirmed: scales effect+0x28 by intensity_scale.
 * Confirmed: sets effect+0xde to (short)(intensity_scale * effect->field_28).
 * Confirmed: clamps effect+0x3c to [0.0f, max] where max comes from descriptor.
 * Confirmed: sets bit 0 at effect+0xe8.
 */
static void player_effect_set_from_descriptor(int player_index, char *effect,
                                              float intensity,
                                              float intensity_scale,
                                              void *descriptor)
{
  int16_t desc_type = *(int16_t *)descriptor;
  int16_t desc_priority = *((int16_t *)descriptor + 1);
  float desc_duration = *(float *)((char *)descriptor + 0x10);
  float desc_max = *(float *)((char *)descriptor + 0x20);
  float desc_min = *(float *)((char *)descriptor + 0x24);
  int16_t effect_priority = *(int16_t *)(effect + 0x1a);
  int16_t effect_timer = *(int16_t *)(effect + 0xde);
  int16_t *enabled_array = (int16_t *)0x2ef7e0;
  float scaled_duration;
  float clamped_value;

  (void)
    player_index; /* original binary receives this in ESI but never uses it */

  scaled_duration = intensity_scale * desc_duration;

  if (((effect_priority <= desc_priority) ||
       ((float)effect_timer <= scaled_duration)) &&
      (enabled_array[desc_type] != 0)) {
    csmemcpy(effect + 0x18, descriptor, 0x38);

    *(float *)(effect + 0x28) = intensity_scale * *(float *)(effect + 0x28);

    *(int16_t *)(effect + 0xde) = (int16_t)(*(float *)(effect + 0x28));

    clamped_value = 0.0f;
    if (0.0f <= ((1.0f - desc_min) * intensity + desc_min)) {
      if (((1.0f - desc_min) * intensity + desc_min) <= desc_max) {
        clamped_value = (1.0f - desc_min) * intensity + desc_min;
      } else {
        clamped_value = desc_max;
      }
    }
    *(float *)(effect + 0x3c) = clamped_value;
    *(uint8_t *)(effect + 0xe8) |= 1;
  }
}

void player_effect_apply(int player_handle, void *effect_descriptor,
                         float intensity)
{
  int16_t unit_index;
  void *player;
  char *effect;

  if (player_handle == -1)
    return;

  player = datum_get(player_data, player_handle);
  unit_index = *(int16_t *)((char *)player + 2);

  if (unit_index == -1)
    return;

  effect = player_effect_get(unit_index);
  player_effect_set_from_descriptor(unit_index, effect, intensity,
                                    intensity * 30.0f, effect_descriptor);
}

/* player_effect_apply_damage (0xa3b80) — Apply damage-related effects to a
 * player.
 *
 * Uses the damage effect tag (jpt!) to set screen shake, vibration, and
 * directional damage indicators based on the angle of incoming damage
 * relative to the player's camera orientation.
 *
 * Confirmed: datum_get(*(data_t**)0x5aa6d4, player_handle) for player data.
 * Confirmed: assert_halt on direction != NULL.
 * Confirmed: lock_random_seed / unlock_random_seed bracket the entire function.
 * Confirmed: tag_get('jpt!', *damage_params) for tag lookup.
 * Confirmed: player_effect_set_from_descriptor(sVar1, effect, param_4, 1.0f,
 * jpt+0x24). Confirmed: *(unsigned int*)(player+0x1c8) & 0x100 checks vehicle
 * driver flag. Confirmed: Global floats: 0x2533c0=0.0f, 0x2533c8=1.0f,
 * 0x25fea8=~0.0, 0x254a58=~0.7854 (PI/4), 0x26af48=~2.3562 (3*PI/4),
 * 0x2568bc=~1.5708 (PI/2). Confirmed: local_player_get_player_index called
 * twice (original binary artifact). Confirmed: camera+0x20 is forward vector,
 * +0x2c is up vector. Confirmed: effect flags at +0xe4 (right), +0xe5
 * (forward), +0xe6 (down), +0xe7 (side).
 */
void FUN_000a3b80(int player_handle, void *damage_params, void *direction,
                  float damage_amount, float scale)
{
  char *player;
  int16_t unit_index;
  char *jpt_tag;
  char *effect;
  int driver_handle;
  int driver_type_valid;
  int damage_type_valid;
  void *camera;
  float attacker_pos[3];
  vector3_t victim_pos;
  float delta[3];
  float rotated_delta[3];
  float length;
  float angle;

  player = (char *)datum_get(*(data_t **)0x5aa6d4, player_handle);
  unit_index = *(int16_t *)(player + 2);

  if ((int)direction == 0) {
    assert_halt(0);
  }

  lock_global_random_seed();

  if (unit_index != -1) {
    jpt_tag = (char *)tag_get(0x6a707421, *(int *)damage_params);
    effect = player_effect_get(unit_index);

    player_effect_set_from_descriptor(unit_index, effect, damage_amount, 1.0f,
                                      (void *)(jpt_tag + 0x24));
    FUN_000a3890(unit_index, (float *)(jpt_tag + 0x98), direction,
                 damage_amount, 1.0f, (float *)effect /* @<eax> */);
    FUN_000a2ba0(unit_index, damage_amount, 1.0f,
                 (float *)(jpt_tag + 0xcc) /* @<eax> */,
                 (void *)effect /* @<ebx> */);
    rumble_player_impulse((short)unit_index, (float *)(jpt_tag + 0x5c),
                          damage_amount, 1.0f);

    if (*(int *)(jpt_tag + 0x120) != -1) {
      sound_impulse_start(*(int *)(jpt_tag + 0x120), 1.0f);
    }

    if ((*(float *)0x2533c0 < scale) &&
        (*(int *)((char *)damage_params + 0xc) != -1)) {
      if ((*(unsigned int *)(jpt_tag + 0x1c8) & 0x100) != 0) {
        *(unsigned char *)(effect + 0xe6) = 1;
        unlock_global_random_seed();
        return;
      }

      driver_handle = local_player_get_player_index(unit_index);
      if (driver_handle == -1) {
        driver_handle = -1;
      } else {
        driver_handle = local_player_get_player_index(unit_index);
        player = (char *)datum_get(*(data_t **)0x5aa6d4, driver_handle);
        driver_handle = *(int *)(player + 0x34);
      }

      driver_type_valid =
        (int)object_try_and_get_and_verify_type(driver_handle, 3) != 0;
      damage_type_valid = (int)object_try_and_get_and_verify_type(
                            *(int *)((char *)damage_params + 0xc), -1) != 0;

      if (driver_type_valid && damage_type_valid) {
        camera = observer_get_camera(unit_index);
        if (camera != (void *)0) {
          unit_get_head_position(driver_handle, attacker_pos);
          object_get_world_position(*(int *)((char *)damage_params + 0xc),
                                    &victim_pos);

          delta[0] = victim_pos.x - attacker_pos[0];
          delta[1] = victim_pos.y - attacker_pos[1];
          delta[2] = victim_pos.z - attacker_pos[2];

          cross_product3d((float *)((char *)camera + 0x20),
                          (float *)((char *)camera + 0x2c), attacker_pos);

          rotated_delta[0] = attacker_pos[0] * delta[0] +
                             attacker_pos[1] * delta[1] +
                             attacker_pos[2] * delta[2];
          rotated_delta[1] = delta[0] * *(float *)((char *)camera + 0x20) +
                             delta[1] * *(float *)((char *)camera + 0x24) +
                             delta[2] * *(float *)((char *)camera + 0x28);
          rotated_delta[2] = delta[0] * *(float *)((char *)camera + 0x2c) +
                             delta[1] * *(float *)((char *)camera + 0x30) +
                             delta[2] * *(float *)((char *)camera + 0x34);

          length = normalize3d(rotated_delta);
          if (length != 0.0f) {
            if ((0.0f < fabsf(rotated_delta[2]))) {
              if (rotated_delta[2] <= 0.0f) {
                *(unsigned char *)(effect + 0xe6) = 1;
              } else {
                *(unsigned char *)(effect + 0xe4) = 1;
              }
            }

            angle = (float)atan2(rotated_delta[1], rotated_delta[0]);
            if ((angle < *(float *)0x254a58) || (*(float *)0x26af48 < angle)) {
              if ((*(float *)0x2568bc < fabsf(angle))) {
                *(unsigned char *)(effect + 0xe5) = 1;
                unlock_global_random_seed();
                return;
              }
              *(unsigned char *)(effect + 0xe7) = 1;
            }
          }
        }
      }
    }
  }

  unlock_global_random_seed();
}
