/* 0x150840 — look up a collision-function attribute by index (compare
 * FUN_0014da80, same 0x234/0x48/+0x24 tag-block shape once the 'coll' tag is
 * resolved). If collision_fn_index == -1, returns -1 (OR AX,0xffff; only the
 * low 16 bits of the return register are set — no MOVSX/MOVZX — matching the
 * `short` return here).
 * If object_index != -1: resolves the object's 'obje' tag, follows +0x7c to
 * its 'coll' tag, indexes the 0x48-byte block at coll_tag+0x234 by
 * collision_fn_index, and returns the int16_t at element+0x24.
 * Else: indexes the 0x14-byte block at scenario_get()+0xa4 by
 * collision_fn_index and returns the int16_t at element+0x12 (scenario-level
 * collision function table, used when there is no object).
 * Confirmed via disassembly 0x150840-0x1508a9. Sole caller: compute_ground_plane
 * at 0x150d48 (unconditional call, not yet ported).
 */
short FUN_00150840(int object_index, short collision_fn_index)
{
  int *obj;
  void *obje_tag;
  void *coll_tag;
  void *elem;

  if (collision_fn_index == -1) {
    return -1;
  }

  if (object_index != -1) {
    obj = (int *)object_get_and_verify_type(object_index, -1);
    obje_tag = tag_get(0x6f626a65 /* 'obje' */, *obj);
    coll_tag =
      tag_get(0x636f6c6c /* 'coll' */, *(int *)((char *)obje_tag + 0x7c));
    elem =
      tag_block_get_element((char *)coll_tag + 0x234, collision_fn_index, 0x48);
    return *(short *)((char *)elem + 0x24);
  }

  elem = tag_block_get_element((char *)scenario_get() + 0xa4,
                               collision_fn_index, 0x14);
  return *(short *)((char *)elem + 0x12);
}

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
  float initial_pos;

  initial_pos = *out_pos;
  *(float *)&point_phys = FUN_001546f0(*out_pos, dt, target_pos, point_phys);
  if (*(float *)&point_phys > 0.0f) {
    FUN_00154540(out_vel, (char *)point_phys + 8,
                 *(float *)&point_phys * accel);
    FUN_001544d0(out_pos, (float *)point_phys, *(char *)&dt, *out_vel);
    if (FUN_001546f0(initial_pos, dt, target_pos, point_phys) <=
        *(float *)&point_phys) {
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
