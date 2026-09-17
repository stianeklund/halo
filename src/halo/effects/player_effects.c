
#include "x87_math.h"

__declspec(noinline) char *player_effect_get(int16_t local_player_index)
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

/* player_effect_add_continuous_effect (0xa27a0) */
void player_effect_add_continuous_effect(short player_index, uint32_t tag_index, float current_distance)
{
  char *effect;
  float *cdmg_tag;
  real scale;
  real factor;
  real blend;
  real val_a;
  real val_b;

  cdmg_tag = (float *)tag_get(0x63646d67, tag_index); /* 'cdmg' */
  if (current_distance < cdmg_tag[1]) {
    effect = player_effect_get(player_index);
    scale = 1.0f - (current_distance - cdmg_tag[0]) / (cdmg_tag[0] - cdmg_tag[1]);
    if (scale < 0.0f) {
      scale = 0.0f;
    } else if (scale > 1.0f) {
      scale = 1.0f;
    }

    factor = (real)FUN_0010a5e0(*(uint16_t *)((char *)cdmg_tag + 0x58), (real)game_time_get() / cdmg_tag[0x17]);
    blend = ((factor * cdmg_tag[0x18]) + (1.0f - cdmg_tag[0x18])) * scale;

    if (*(int16_t *)(effect + 0xdc) >= 1) {
      *(uint16_t *)(effect + 0xdc) = 0;
      csmemset(effect + 0xcc, 0, 0x10);
    }

    val_a = blend * cdmg_tag[0x11];
    if (val_a <= 0.0f) {
      val_a = 0.0f;
    }
    *(float *)(effect + 0xd4) += val_a;

    val_b = blend * cdmg_tag[0x12];
    if (val_b <= 0.0f) {
      val_b = 0.0f;
    }
    *(float *)(effect + 0xd8) += val_b;

    *(float *)(effect + 0xcc) += scale * cdmg_tag[9];
    *(float *)(effect + 0xd0) += scale * cdmg_tag[10];
  }
}

/* scripted_player_effect_set_rotation (0xa28e0) */
void scripted_player_effect_set_rotation(float pitch, float yaw, float roll)
{
  *(float *)(player_effect_globals + 0x3d0) = pitch * 0.017453292f;
  *(float *)(player_effect_globals + 0x3d4) = yaw * 0.017453292f;
  *(float *)(player_effect_globals + 0x3d8) = roll * 0.017453292f;
}

/* scripted_player_effect_set_rumble (0xa2920) */
void scripted_player_effect_set_rumble(float left_motor, float right_motor)
{
  rumble_player_set_scripted_values(left_motor, right_motor);
}

/* player_telefrag_effect_stop (0xa2930) */
void player_telefrag_effect_stop(int player_handle)
{
  char *player;
  int local_player_index;

  player = (char *)datum_get(*(data_t **)0x5aa6d4, player_handle);
  local_player_index = *(int16_t *)(player + 2);

  if (local_player_index != -1) {
    player_effect_get((int16_t)local_player_index);
    rumble_set_direct_motors((short)local_player_index, 0, 0);
  }
}

/* player_effect_screen_fade_in (0xa2970) */
void player_effect_screen_fade_in(float target_intensity, int transition_ticks, int hold_ticks, short color_index)
{
  *(float *)(player_effect_globals + 0x3b0) = target_intensity;
  *(int32_t *)(player_effect_globals + 0x3b4) = transition_ticks;
  *(int32_t *)(player_effect_globals + 0x3b8) = hold_ticks;
  *(int16_t *)(player_effect_globals + 0x3c0) = color_index;
  *(int8_t *)(player_effect_globals + 0x3c2) = 0;
  *(uint32_t *)(player_effect_globals + 0x3bc) = game_time_get();
}

/* player_effect_screen_fade_out (0xa29c0) */
void player_effect_screen_fade_out(float target_intensity, int transition_ticks, int hold_ticks, short color_index)
{
  *(float *)(player_effect_globals + 0x3b0) = target_intensity;
  *(int32_t *)(player_effect_globals + 0x3b4) = transition_ticks;
  *(int32_t *)(player_effect_globals + 0x3b8) = hold_ticks;
  *(int16_t *)(player_effect_globals + 0x3c0) = color_index;
  *(int8_t *)(player_effect_globals + 0x3c2) = 1;
  *(uint32_t *)(player_effect_globals + 0x3bc) = game_time_get();
}

/* player_effect_get_damage_indicators (0xa2a10) */
void player_effect_get_damage_indicators(int player_index, void *out)
{
  unsigned char *indicators;
  int count;
  int aged;

  indicators = (unsigned char *)player_effect_get((int16_t)player_index) + 0xe4;
  csmemcpy(out, indicators, 4);

  for (count = 4; count > 0; count--, indicators++) {
    if (*indicators != 0) {
      aged = (int)game_time_get_elapsed() + (int)*indicators;
      if (aged >= 0xff) {
        *indicators = 0xff;
      } else {
        *indicators = (unsigned char)((int)game_time_get_elapsed() + (int)*indicators);
      }
    }
  }
}

/* player_effect_clear_damage_indicators (0xa2a70) */
void player_effect_clear_damage_indicators(int player_index)
{
  char *effect;

  effect = player_effect_get((int16_t)player_index);
  csmemset(effect + 0xe4, 0, 4);
}

/* effect_scale_factor (0xa2a90) */
float effect_scale_factor(float min_value, float scale)
{
  return (1.0f - min_value) * scale + min_value;
}

/* player_effect_update_screen_flash (0xa2ab0) */
void player_effect_update_screen_flash(void)
{
}

static void player_effect_set_from_descriptor(int player_index, char *effect,
                                              float intensity,
                                              float intensity_scale,
                                              void *descriptor)
{
  int16_t desc_type;
  int16_t desc_priority;
  float desc_duration;
  float desc_max;
  float desc_min;
  int16_t effect_priority;
  int16_t effect_timer;
  int16_t *enabled_array;
  float scaled_duration;
  float clamped_value;

  (void)player_index;

  desc_type = *(int16_t *)descriptor;
  desc_priority = *((int16_t *)descriptor + 1);
  desc_duration = *(float *)((char *)descriptor + 0x10);
  desc_max = *(float *)((char *)descriptor + 0x20);
  desc_min = *(float *)((char *)descriptor + 0x24);
  effect_priority = *(int16_t *)(effect + 0x1a);
  effect_timer = *(int16_t *)(effect + 0xde);
  enabled_array = (int16_t *)0x2ef7e0;

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

/* FUN_000a2ba0 (0xa2ba0) / player_effect_update_camera_shake */
void FUN_000a2ba0(int unit_index, float damage_amount, float scale, float *effect_data /* @<eax> */, void *effect /* @<ebx> */)
{
  float scaled_intensity;
  float scaled_val;
  float current_val;
  int i;
  char *eff;

  (void)unit_index;
  eff = (char *)effect;
  game_time_get();
  scaled_intensity = scale * 30.0f;
  scaled_val = (1.0f - effect_data[10]) * damage_amount + effect_data[10];
  current_val = (float)*(int16_t *)(eff + 0xe2);

  if ((scaled_intensity * effect_data[0] <= current_val && scaled_val <= *(float *)(eff + 0xac)) &&
      (scaled_val < *(float *)(eff + 0xac) || scaled_intensity * effect_data[0] <= current_val)) {
    return;
  }

  for (i = 0; i < 18; i++) {
    *(float *)(eff + 0x84 + i * 4) = effect_data[i];
  }
  *(float *)(eff + 0xac) = scaled_val;
  *(float *)(eff + 0x84) = scaled_intensity * *(float *)(eff + 0x84);
  *(int16_t *)(eff + 0xe2) = (int16_t)*(float *)(eff + 0x84);
  *(uint8_t *)(eff + 0xe8) |= 4;
  *(float *)(eff + 0xa4) = scaled_intensity * *(float *)(eff + 0xa4);
}

/* effect_scale_value (0xa2c70) */
float effect_scale_value(int function_index, float scale, float current, float duration)
{
  return transition_function_evaluate((short)function_index, 1.0f - (current / duration)) * scale;
}

/* player_effect_update (0xa2ca0) */
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

/* player_effect_continuous_refresh (0xa2d30) */
void player_effect_continuous_refresh(uint32_t tag_index, void *position)
{
  int16_t i;
  int player_index;
  char *player;
  int unit_index;
  vector3_t victim_pos;
  float *pos;
  float dx, dy, dz, dist;

  pos = (float *)position;
  for (i = 0; i < 4; i++) {
    player_index = local_player_get_player_index(i);
    if (player_index != -1) {
      player = (char *)datum_get(*(data_t **)0x5aa6d4, player_index);
      unit_index = *(int *)(player + 0x34);
      if (unit_index != -1) {
        object_get_world_position(unit_index, &victim_pos);
        dx = pos[0] - victim_pos.x;
        dy = pos[1] - victim_pos.y;
        dz = pos[2] - victim_pos.z;
        dist = x87_sqrt(dx * dx + dy * dy + dz * dz);
        player_effect_add_continuous_effect(i, tag_index, dist);
      }
    }
  }
}

/* scripted_player_effect_set_translation (0xa2dc0) */
void scripted_player_effect_set_translation(int param_1, float param_2,
                                            float param_3)
{
  char *globals;

  globals = player_effect_globals;
  *(int *)(globals + 0x3c4) = param_1;
  *(float *)(globals + 0x3c8) = param_2;
  *(float *)(globals + 0x3cc) = param_3;
}

/* scripted_player_effect_start (0xa2df0) */
void scripted_player_effect_start(uint32_t tag_index, float transition_seconds)
{
  int16_t ticks;

  ticks = (int16_t)(transition_seconds * 30.0f);
  *(uint32_t *)(player_effect_globals + 0x3dc) = tag_index;
  *(int16_t *)(player_effect_globals + 0x3e0) = ticks;
  *(int16_t *)(player_effect_globals + 0x3e2) = ticks;
  *(uint32_t *)(player_effect_globals + 0x3e4) = (*(uint32_t *)(player_effect_globals + 0x3e4) & ~2) | 1;
}

/* scripted_player_effect_stop (0xa2e40) */
void scripted_player_effect_stop(float transition_seconds)
{
  int16_t ticks;

  ticks = (int16_t)(transition_seconds * 30.0f);
  *(int16_t *)(player_effect_globals + 0x3e0) = ticks;
  *(int16_t *)(player_effect_globals + 0x3e2) = ticks;
  *(uint32_t *)(player_effect_globals + 0x3e4) |= 2;
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

/* player_effect_screen_flash (0xa2e80) */
void player_effect_screen_flash(int player_handle, float intensity)
{
  char *flash_color;
  int16_t descriptor[28];

  csmemset(descriptor, 0, sizeof(descriptor));
  flash_color = *(char **)0x2ee6c4;
  csmemcpy((char *)descriptor + 0x28, flash_color, 16);

  descriptor[0] = 1;
  descriptor[1] = 2;
  *(float *)((char *)descriptor + 0x10) = 1.0f;
  *(float *)((char *)descriptor + 0x20) = intensity;
  *(float *)((char *)descriptor + 0x24) = 0.0f;

  player_effect_apply(player_handle, descriptor, intensity);
}

/* player_telefrag_effect_start (0xa2ed0) */
void player_telefrag_effect_start(int player_handle, float intensity)
{
  char *effect;
  int16_t descriptor[28];
  float effect_data[18];
  char *player;
  int local_player_index;
  float *flash_color;

  csmemset(descriptor, 0, sizeof(descriptor));
  csmemset(effect_data, 0, sizeof(effect_data));

  player = (char *)datum_get(player_data, player_handle);
  local_player_index = *(int16_t *)(player + 2);

  if (local_player_index != -1) {
    effect = player_effect_get((int16_t)local_player_index);

    effect_data[2] = (float)(intensity * 0.01f);

    flash_color = *(float **)0x2ee6c4;
    *(float *)((char *)descriptor + 0x28) = flash_color[0];
    *(float *)((char *)descriptor + 0x2c) = flash_color[1];
    *(float *)((char *)descriptor + 0x30) = flash_color[2];
    *(float *)((char *)descriptor + 0x34) = flash_color[3];

    effect_data[0] = 1.0f;
    descriptor[0] = 1;
    descriptor[1] = 2;
    *(float *)((char *)descriptor + 0x10) = 1.0f;
    *(float *)((char *)descriptor + 0x20) = intensity;
    *(float *)((char *)descriptor + 0x24) = 0.0f;

    rumble_set_direct_motors((short)local_player_index, *(int *)&intensity,
                             *(int *)&intensity);
    player_effect_set_from_descriptor(local_player_index, effect, intensity,
                                      1.0f, descriptor);
    FUN_000a2ba0(local_player_index, intensity, 1.0f, effect_data /* @<eax> */,
                 (void *)effect /* @<ebx> */);
  }
}

/* player_effect_get_screen_flash (0xa2fc0) */
void player_effect_get_screen_flash(short local_player_index, void *flash_out)
{
  uint16_t *out;
  char *globals;
  char *effect;
  int elapsed;
  real fade_progress;
  real flash_intensity;
  real total_ticks;
  real current_ticks;
  real max_intensity;
  int16_t timer;

  out = (uint16_t *)flash_out;
  globals = player_effect_globals;

  if (!out) {
    assert_halt(0);
  }

  if (!console_is_active()) {
    if (*(int16_t *)(globals + 0x3c0) != -1 &&
        (*(int8_t *)(globals + 0x3c2) || ((int)game_time_get() - *(int *)(globals + 0x3bc) <= *(int16_t *)(globals + 0x3c0)))) {
      *out = 1;
      *(uint32_t *)(out + 3) = *(uint32_t *)(globals + 0x3b0);
      *(uint32_t *)(out + 4) = *(uint32_t *)(globals + 0x3b4);
      *(uint32_t *)(out + 5) = *(uint32_t *)(globals + 0x3b8);
      *(uint32_t *)(out + 2) = 0x3f800000; /* 1.0f */

      if (*(int16_t *)(globals + 0x3c0) >= 1) {
        elapsed = (int)game_time_get() - *(int *)(globals + 0x3bc);
        fade_progress = (real)elapsed / (real)*(int16_t *)(globals + 0x3c0);
        if (fade_progress < 0.0f) {
          fade_progress = 0.0f;
        } else if (fade_progress > 1.0f) {
          fade_progress = 1.0f;
        }
        flash_intensity = transition_function_evaluate(5, fade_progress);
      } else {
        flash_intensity = 1.0f;
      }

      if (!*(int8_t *)(globals + 0x3c2)) {
        flash_intensity = 1.0f - flash_intensity;
      }
      if (flash_intensity < 0.0f) {
        flash_intensity = 0.0f;
      } else if (flash_intensity > 1.0f) {
        flash_intensity = 1.0f;
      }
      *(float *)(out + 1) = flash_intensity;
    } else if (local_player_index != -1) {
      effect = player_effect_get(local_player_index);
      *(int16_t *)(globals + 0x3c0) = -1;
      if (*(int16_t *)(effect + 0xde) > 0 || (*(uint8_t *)(effect + 0xe8) & 1)) {
        *(uint8_t *)(effect + 0xe8) &= ~1;
        *out = *(uint16_t *)((*(int16_t *)(effect + 0x18)) * 2 + 0x2ef7e0);
        *(uint32_t *)(out + 2) = *(uint32_t *)(effect + 0x40);
        *(uint32_t *)(out + 3) = *(uint32_t *)(effect + 0x44);
        *(uint32_t *)(out + 4) = *(uint32_t *)(effect + 0x48);
        *(uint32_t *)(out + 5) = *(uint32_t *)(effect + 0x4c);

        if (*(float *)(effect + 0x28) <= 0.0f) {
          *(float *)(out + 1) = *(float *)(effect + 0x3c);
        } else {
          total_ticks = *(float *)(effect + 0x28);
          current_ticks = (real)*(int16_t *)(effect + 0xde);
          max_intensity = *(float *)(effect + 0x3c);
          *(float *)(out + 1) = transition_function_evaluate(*(int16_t *)(effect + 0x2c), (current_ticks / total_ticks) * max_intensity);
        }
        timer = (int16_t)game_time_get_elapsed();
        *(int16_t *)(effect + 0xde) -= timer;
      }
    }
  }
}

/* get_shake_matrix (0xa32e0) */
void get_shake_matrix(float shake_trans, float shake_rot, void *matrix)
{
  vector3_t rand_vec;
  float *m;
  unsigned int *seed;

  m = (float *)matrix;
  seed = random_math_get_local_seed_address();
  if (shake_rot != 0.0f) {
    random_seed_get_direction3d(seed, (float *)&rand_vec);
    FUN_001092d0(m, (float *)&rand_vec, x87_fsin(shake_rot), x87_fcos(shake_rot));
  }
  if (shake_trans != 0.0f) {
    random_seed_get_direction3d(seed, (float *)&rand_vec);
    m[10] = rand_vec.x * shake_trans;
    m[11] = rand_vec.y * shake_trans;
    m[12] = rand_vec.z * shake_trans;
  }
}

/* player_effect_get_camera_effect_matrix (0xa3370) */
void player_effect_get_camera_effect_matrix(short local_player_index, void *matrix_out)
{
  char *globals;
  char *effect;
  real intensity;
  real rot_scale, trans_scale;
  float shake_matrix[13];
  real progress;
  real duration;
  int16_t timer;
  float *mout;
  unsigned int *seed;
  float rand_y, rand_p, rand_r;

  mout = (float *)matrix_out;
  globals = player_effect_globals;

  if (!mout) {
    assert_halt(0);
  }
  if (local_player_index == -1) {
    return;
  }
  game_time_get();

  if (*(uint8_t *)(globals + 0x3e4) & 1) {
    intensity = *(float *)(globals + 0x3dc);
    csmemcpy(mout, (void *)0x31fc60, 0x34);
    timer = *(int16_t *)(globals + 0x3e0);
    if (timer >= 1) {
      duration = (real)*(int16_t *)(globals + 0x3e2);
      if (*(uint8_t *)(globals + 0x3e4) & 2) {
        intensity *= ((real)timer / duration);
      } else {
        intensity *= (1.0f - (real)timer / duration);
      }
      *(int16_t *)(globals + 0x3e0) -= (int16_t)game_time_get_elapsed();
    } else if (*(uint8_t *)(globals + 0x3e4) & 2) {
      *(uint32_t *)(globals + 0x3e4) &= ~1;
      rumble_player_set_scripted_values(0.0f, 0.0f);
    }

    if (!(*(uint8_t *)(globals + 0x3e4) & 1)) {
      return;
    }
    if (intensity < 0.0f) {
      intensity = 0.0f;
    } else if (intensity > 1.0f) {
      intensity = 1.0f;
    }

    rumble_player_set_scripted_values(intensity, intensity);
    seed = random_math_get_local_seed_address();
    rand_y = random_real_range((int *)seed, -1.0f, 1.0f) * *(float *)(globals + 0x3d0) * intensity;
    rand_p = random_real_range((int *)seed, -1.0f, 1.0f) * *(float *)(globals + 0x3d4) * intensity;
    rand_r = random_real_range((int *)seed, -1.0f, 1.0f) * *(float *)(globals + 0x3d8) * intensity;
    FUN_00109e90(mout, rand_y, rand_p, rand_r);

    mout[10] = random_real_range((int *)seed, -1.0f, 1.0f) * *(float *)(globals + 0x3c4) * intensity;
    mout[11] = random_real_range((int *)seed, -1.0f, 1.0f) * *(float *)(globals + 0x3c8) * intensity;
    mout[12] = random_real_range((int *)seed, -1.0f, 1.0f) * *(float *)(globals + 0x3cc) * intensity;
    return;
  }

  effect = player_effect_get(local_player_index);
  csmemcpy(mout, (void *)0x31fc60, 0x34);

  if (*(int16_t *)(effect + 0xe2) > 0 || (*(uint8_t *)(effect + 0xe8) & 4)) {
    csmemcpy(shake_matrix, (void *)0x31fc60, 0x34);
    if (*(uint8_t *)(effect + 0xe8) & 4) {
      rot_scale = 1.0f;
    } else {
      progress = 1.0f - (*(float *)(effect + 0x84) - (real)*(int16_t *)(effect + 0xe2)) / *(float *)(effect + 0x84);
      rot_scale = transition_function_evaluate(*(int16_t *)(effect + 0x88), progress) * *(float *)(effect + 0xac);
    }

    progress = (*(float *)(effect + 0x84) - (real)*(int16_t *)(effect + 0xe2)) / *(float *)(effect + 0xa4);
    intensity = (1.0f - *(float *)(effect + 0xa8) + FUN_0010a5e0(*(int16_t *)(effect + 0xa0), progress) * *(float *)(effect + 0xa8)) * rot_scale;

    trans_scale = intensity * *(float *)(effect + 0x8c);
    if (trans_scale < 0.0f) trans_scale = 0.0f;

    rot_scale = intensity * *(float *)(effect + 0x90);
    if (rot_scale < 0.0f) rot_scale = 0.0f;

    *(uint8_t *)(effect + 0xe8) &= ~4;
    get_shake_matrix(trans_scale + *(float *)(effect + 0xd4), rot_scale + *(float *)(effect + 0xd8), shake_matrix);
    rumble_player_impulse((short)local_player_index, (float *)(effect + 0xcc), *(float *)(effect + 0xd0), 1.0f);

    *(int16_t *)(effect + 0xdc) += (int16_t)game_time_get_elapsed();
    if (*(int16_t *)(effect + 0xdc) >= 1) {
      *(uint16_t *)(effect + 0xdc) = 0;
      csmemset(effect + 0xcc, 0, 0x10);
    }

    *(int16_t *)(effect + 0xe2) -= (int16_t)game_time_get_elapsed();
    matrix4x3_multiply(mout, shake_matrix, mout);
  }
}

/* FUN_000a3890 (0xa3890) / player_effect_update_camera_impulse */
void FUN_000a3890(int unit_index, float *rumble_def, void *direction, float damage_amount, float scale, float *effect /* @<eax> */)
{
  float scaled_intensity;
  float scaled_val;
  float current_val;
  int i;
  float dir[3];
  float facing_angles[2];
  float cam_fwd[3];
  char *eff;
  float *facing;
  float angle;
  unsigned int *seed;
  float impulse_yaw, impulse_pitch;
  float impulse[2];

  eff = (char *)effect;
  game_time_get();
  scaled_intensity = scale * 30.0f;
  scaled_val = (1.0f - rumble_def[6]) * damage_amount + rumble_def[6];
  current_val = (float)*(int16_t *)(eff + 0xe0);

  if (current_val < *(float *)(eff + 0x50) || *(float *)(eff + 0x68) < scaled_val ||
      (*(float *)(eff + 0x68) <= scaled_val && current_val < scaled_intensity * rumble_def[0])) {
    dir[0] = ((float *)direction)[0];
    dir[1] = ((float *)direction)[1];
    dir[2] = 0.0f;
    normalize3d(dir);

    facing = player_control_get_facing_angles((short)unit_index);
    facing_angles[0] = facing[0];
    facing_angles[1] = facing[1];
    angles_to_vector(cam_fwd, facing_angles);
    cam_fwd[2] = 0.0f;
    normalize3d(cam_fwd);

    if (fabsf((dir[0] * dir[0] + dir[1] * dir[1]) - 1.0f) < 0.0001f &&
        fabsf((cam_fwd[0] * cam_fwd[0] + cam_fwd[1] * cam_fwd[1]) - 1.0f) < 0.0001f) {
      angle = signed_angle_between_vectors2d(cam_fwd, dir);
      for (i = 0; i < 13; i++) {
        *(float *)(eff + 0x50 + i * 4) = rumble_def[i];
      }
      *(float *)(eff + 0x50) = scaled_intensity * *(float *)(eff + 0x50);
      *(float *)(eff + 0x68) = scaled_val;
      *(int16_t *)(eff + 0xe0) = (int16_t)*(float *)(eff + 0x50);

      *(float *)eff = x87_fcos(angle);
      *((float *)eff + 1) = x87_fsin(angle);
      *((float *)eff + 2) = 0.0f;

      seed = random_math_get_local_seed_address();
      impulse_pitch = random_real_range((int *)seed, *(float *)(eff + 0x60), *(float *)(eff + 0x64));
      impulse_yaw = random_real_range((int *)seed, 0.0f, 6.2831853f);

      cross_product3d((float *)0x31fc44, (float *)eff, (float *)eff + 3);
      normalize3d((float *)eff + 3);
      rotate_vector3d_by_sincos((float *)eff + 3, (float *)eff, x87_fsin(impulse_yaw), x87_fcos(impulse_yaw));

      *((float *)eff + 3) *= impulse_pitch;
      *((float *)eff + 4) *= impulse_pitch;
      *((float *)eff + 5) *= impulse_pitch;

      *(uint8_t *)(eff + 0xe8) |= 2;
    }
  }

  scaled_intensity = (1.0f - rumble_def[9]) * damage_amount + rumble_def[9];
  player_control_get_facing_direction((short)unit_index, cam_fwd);
  impulse[0] = ((cam_fwd[0] * *(float *)0x31fc4c - cam_fwd[2] * *(float *)0x31fc44) * ((float *)direction)[1] +
                (cam_fwd[1] * *(float *)0x31fc44 - cam_fwd[0] * *(float *)0x31fc48) * ((float *)direction)[2] +
                (cam_fwd[2] * *(float *)0x31fc48 - cam_fwd[1] * *(float *)0x31fc4c) * ((float *)direction)[0]) *
               rumble_def[8] * scaled_intensity;
  impulse[1] = (cam_fwd[1] * ((float *)direction)[1] + cam_fwd[2] * ((float *)direction)[2] + cam_fwd[0] * ((float *)direction)[0]) *
               rumble_def[8] * scaled_intensity;
  FUN_000b8cf0((short)unit_index, impulse);
}

/* FUN_000a3b80 (0xa3b80) / player_effect_start */
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
