/* 0x1544d0 - accumulate float by delta and clamp/wrap within bounds. */
void FUN_001544d0(float *param_1, float *param_2, char param_3, float param_4)
{
  param_4 = param_4 + *param_1;
  *param_1 = param_4;
  if (param_4 < param_2[1]) {
    if (param_3 != '\0') {
      *param_1 = (param_2[0] - param_2[1]) + param_4;
      return;
    }
    *param_1 = param_2[1];
    return;
  }
  if (param_4 > *param_2) {
    if (param_3 != '\0') {
      *param_1 = param_4 - (*param_2 - param_2[1]);
      return;
    }
    *param_1 = *param_2;
  }
}

/* 0x1546b0 */
void FUN_001546b0(float *param_1, float *param_2, float *param_3, char param_4,
                  float param_5)
{
  FUN_00154540(param_2, param_3 + 2, param_5);
  FUN_001544d0(param_1, param_3, param_4, *param_2);
}

/* 0x1547d0 — step point physics towards target position; returns 1 if target
 * reached/reset */
char FUN_001547d0(float *out_pos, float *out_vel, void *point_phys, float dt,
                  float target_pos, float accel)
{
  float current_pos;

  current_pos = *out_pos;
  if (FUN_001546f0(current_pos, dt, target_pos, point_phys) > 1.0f) {
    FUN_00154540(out_vel, (char *)point_phys + 8, accel);
    FUN_001544d0(out_pos, (float *)point_phys, 0, *out_vel);
    if (FUN_001546f0(*out_pos, dt, target_pos, point_phys) <= current_pos) {
      return 0;
    }
  }

  *out_pos = target_pos;
  *out_vel = 0.0f;
  return 1;
}

void point_physics_initialize_for_new_map(void)
{
  *(float *)0x476200 = *(float *)0x325134 * *(float *)0x29d954;
  *(float *)0x4761fc = *(float *)0x325130 * *(float *)0x29d954;
}

void point_physics_dispose_from_old_map(void)
{
}

/* Scale a point-physics density value by volume (scale^3). */
float point_physics_definition_get_mass(int tag_data, float scale)
{
  return scale * *(float *)(tag_data + 4) * scale * scale;
}

/* 0x154a20 — render point physics debugging / debug point */
void FUN_00154a20(void *obj, float *point, float val)
{
  void *color;

  color = *(void **)0x2ee6d0;
  if ((*(unsigned char *)obj & 2) == 0) {
    color = *(void **)0x2ee6d4;
  }

  FUN_00189150(1, point, val, color);
}
