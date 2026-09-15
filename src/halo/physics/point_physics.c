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
 * Confirmed via disassembly 0x150840-0x1508a9. Sole caller:
 * compute_ground_plane at 0x150d48 (unconditional call, not yet ported).
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

/* 0x1508b0 — render debug geometry for one point-physics instance.
 * state is a small stack/live record: state[0]=object datum handle,
 * state[1]=point-physics definition tag data, state+0x8 = the instance's
 * 4x3 transform matrix (its rows at +0xc and +0x24 are also passed on
 * directly as the forward/up vectors for the origin marker).
 * Resolves the object's 'obje' tag, draws the instance origin marker scaled
 * by obje_tag+0x4, then walks the 0x80-byte tag block at definition+0x74
 * (per-point entries), transforming point +0x38 and vectors +0x44/+0x50 into
 * world space and drawing a sphere of radius +0x68 plus a second marker
 * scaled by +0x68 * 0.5f.
 * Confirmed via disassembly 0x1508b0-0x1508b5. Loop index is a short:
 * INC ECX / MOVSX ECX,CX before the CMP against the reloaded block count,
 * and state[1] is reloaded from the parameter every iteration.
 * 0x2ee6c4 is the debug-draw default color pointer, 0x253398 is 0.5f.
 */
void FUN_001508b0(int *state)
{
  float *matrix;
  int *object;
  void *obje_tag;
  void *elem;
  short i;
  float local_34[3];
  float local_28[3];
  float local_1c[3];
  float local_10[3];

  object = (int *)object_get_and_verify_type(*state, -1);
  obje_tag = tag_get(0x6f626a65 /* 'obje' */, *object);

  matrix = (float *)(state + 2);
  matrix_transform_point(matrix, (float *)(state[1] + 0xc), local_1c);
  FUN_0018a990(1, local_1c, (float *)(state + 3), (float *)(state + 9),
               *(float *)((char *)obje_tag + 0x4));

  for (i = 0; i < *(int *)(state[1] + 0x74); i++) {
    elem = tag_block_get_element((void *)(state[1] + 0x74), i, 0x80);
    matrix_transform_point(matrix, (float *)((char *)elem + 0x38), local_10);
    matrix_transform_vector(matrix, (float *)((char *)elem + 0x44), local_34);
    matrix_transform_vector(matrix, (float *)((char *)elem + 0x50), local_28);
    FUN_00189540(1, local_10, *(float *)((char *)elem + 0x68),
                 *(void **)0x2ee6c4);
    FUN_0018a990(1, local_10, local_34, local_28,
                 *(float *)((char *)elem + 0x68) * *(const float *)0x253398);
  }
}

/* 0x150c80 — recompute a mass point's ground plane from a downward collision
 * test.  `mass_point` arrives in ECX (MOV EBX,ECX at 0x150c94); the two stack
 * parameters are the collision-test ignore handle ([EBP+8], forwarded as
 * FUN_0014ec30's param6) and the per-point definition element ([EBP+0xc],
 * whose float at +0x68 is the point radius — same field FUN_001508b0 draws the
 * debug sphere with).
 * mass_point layout used here (offsets from disassembly 0x150c80-0x150dcd):
 *   +0x00 dword flags (bit 2 = "no ground"), +0x04 float position[3],
 *   +0x60 float ground_plane[4], +0x70 int16 ground_material_type,
 *   +0x74 float ground plane distance.
 * The +0x70/-1 sentinel and material-type range are named by the assert string
 * at 0x150db3 ("mass_point->ground_material_type==NONE || ...").
 * 0x32513c..0x325148 is the default (flat, downward) plane constant, copied as
 * four dwords exactly as the reference does (MOV EDX,[0x32513c] / MOV
 * [ECX],EDX). x87 addend order at 0x150cc2-0x150ceb is per-site: (k*z + j*y) +
 * i*x, then FSUB plane_d, then FSUBR radius. FUN_0014ec30's search_radius and
 * dist_a are the SAME value in the reference (MOV ECX,[EAX+0x68]; PUSH ECX;
 * PUSH 0; MOV EDX,ECX; PUSH EDX) — the repeated argument is binary-confirmed,
 * not a transcription slip. hit_info (0x2c bytes at EBP-0x30) fields consumed:
 * +0x00 plane distance, +0x10..+0x1c plane, +0x20 object handle, +0x28 flag
 * byte (bit 3), +0x2a collision-function index (loaded into SI for
 * FUN_00150840).
 */
void compute_ground_plane(int ignore_object_handle, void *point_definition,
                          void *mass_point_arg)
{
  unsigned char *mass_point;
  unsigned char *plane;
  float *position;
  int hit_object;
  short material_type;
  unsigned char los_scratch[44040];
  unsigned char hit_info[0x2c];

  mass_point = (unsigned char *)mass_point_arg;
  plane = mass_point + 0x60;
  position = (float *)(mass_point + 4);

  *(int *)(plane + 0x0) = *(int *)0x32513c;
  *(int *)(plane + 0x4) = *(int *)0x325140;
  *(int *)(plane + 0x8) = *(int *)0x325144;
  *(int *)(plane + 0xc) = *(int *)0x325148;
  *(short *)(mass_point + 0x70) = -1;

  *(float *)(mass_point + 0x74) = *(float *)((char *)point_definition + 0x68) -
                                  ((*(float *)(plane + 0x8) * position[2] +
                                    *(float *)(plane + 0x4) * position[1] +
                                    *(float *)(plane + 0x0) * position[0]) -
                                   *(float *)(plane + 0xc));

  if (FUN_0014ec30(0xc0a0, position,
                   *(float *)((char *)point_definition + 0x68), 0.0f,
                   *(float *)((char *)point_definition + 0x68),
                   ignore_object_handle, los_scratch) &&
      collision_features_test_los(los_scratch, position, hit_info)) {
    *(int *)(plane + 0x0) = *(int *)(hit_info + 0x10);
    *(int *)(plane + 0x4) = *(int *)(hit_info + 0x14);
    *(int *)(plane + 0x8) = *(int *)(hit_info + 0x18);
    *(int *)(plane + 0xc) = *(int *)(hit_info + 0x1c);
    *(int *)(mass_point + 0x74) = *(int *)(hit_info + 0x00);

    hit_object = *(int *)(hit_info + 0x20);
    *(short *)(mass_point + 0x70) =
      FUN_00150840(hit_object, *(short *)(hit_info + 0x2a));

    if ((hit_info[0x28] & 8) == 0 &&
        (hit_object == -1 ||
         ((1 << (object_get_type(hit_object) & 0x1f)) & 0x40) != 0)) {
      *(int *)mass_point = *(int *)mass_point & 0xfffffffb;
    } else {
      *(int *)mass_point = *(int *)mass_point | 4;
    }

    if (hit_object != -1) {
      object_deplete_shield(hit_object);
    }
  }

  material_type = *(short *)(mass_point + 0x70);
  if (material_type != -1 && (material_type < 0 || material_type > 0x20)) {
    display_assert("mass_point->ground_material_type==NONE || "
                   "(mass_point->ground_material_type>=0 && "
                   "mass_point->ground_material_type<NUMBER_OF_MATERIAL_TYPES)",
                   "c:\\halo\\SOURCE\\physics\\physics.c", 0x152, 1);
    system_exit(-1);
  }
}

/* 0x152350 — collide one unit against every object inside its bounding
 * radius.  `unit_index` arrives in EBX (never initialized inside the
 * function; sole caller FUN_00154270 at 0x1544be).
 *
 * Stack frame (0x2094 via _chkstk), derived from the EBP displacements at
 * 0x152350-0x1524c2:
 *   EBP-0x2094 (0x2000)  int found[0x800]      object_find_in_radius output
 *   EBP-0x94   (0x3c)    FUN_001509c0 context for this unit  (state_self)
 *   EBP-0x58   (0x3c)    FUN_001509c0 context for the other object
 *   EBP-0x1c   (0x10)    FUN_0014c8e0 collision-bsp context  (a_ctx)
 *   EBP-0xc / -0x8       loop counter / cursor
 *   EBP-0x1              byte result of FUN_0014c8e0 (AL only)
 * The 0x3c / 0x10 context sizes match the same two callees' contexts already
 * documented in collision_bsp.c (EBP-0x64 = 60 bytes, EBP-0x28 = 16 bytes).
 *
 * FUN_0014c8e0's result is consumed as a byte (MOV [EBP-0x1],AL) and reused
 * twice: as object_find_in_radius' type_mask (XOR EAX,EAX / SETNZ AL / ADD
 * EAX,2 -> 2 or 3) and as the "model_instance_valid" assert predicate.
 *
 * object_find_in_radius' pushes at 0x15238d-0x1523b0 (last arg pushed first):
 * 0x800, found, [EAX+0x5c] (radius, plain dword copy of the float),
 * EAX+0x50 (position), EAX+0x48 (cluster info), type_mask, 1.
 * The 9-dword ADD ESP,0x24 also folds in the two pushes of the preceding
 * object_get_and_verify_type call — not extra arguments.
 *
 * Per-object dispatch is on the unsigned byte at header+3 (MOVZX / SUB EAX,0
 * / DEC EAX): 0 = biped, 1 = unit, anything else skipped.
 *
 * The vehicle-side test at 0x152439-0x152449 is FLD [state_other[1]] /
 * FCOMP [0x2533c0] / TEST AH,0x41 / JNZ skip, i.e. fall through to
 * FUN_00151ec0 only when *state_other[1] > 0x2533c0.  The index compare uses
 * both handles masked to their low 16 bits (AND ESI/ECX,0xffff; CMP; JL).
 *
 * FUN_00151ec0 and physics_compute_biped_collision are both called with two
 * stack arguments (LEA/PUSH pairs at 0x15244b-0x152456 and
 * 0x15249a-0x15249f, both cleaned by the shared ADD ESP,0x8 at 0x1524a4);
 * their kb.json `void(void)` declarations were placeholders.
 */
void physics_compute_unit_collisions(int unit_index)
{
  char has_model;
  short found_count;
  unsigned int remaining;
  unsigned int other_index;
  unsigned int object_type;
  int *cursor;
  void *object;
  void *other;
  void *header;
  void *biped;
  int a_ctx[4];
  int state_self[15];
  int state_other[15];
  int found[0x800];

  has_model = (char)FUN_0014c8e0(a_ctx, unit_index);

  if (FUN_001509c0(state_self, unit_index) == 0) {
    return;
  }

  object = object_get_and_verify_type(unit_index, 2);
  found_count = (short)object_find_in_radius(
    1, (unsigned int)((has_model != 0) + 2), (char *)object + 0x48,
    (float *)((char *)object + 0x50), *(float *)((char *)object + 0x5c), found,
    0x800);

  if (found_count <= 0) {
    return;
  }

  remaining = (unsigned int)(unsigned short)found_count;
  cursor = found;

  do {
    other_index = (unsigned int)*cursor;
    header = datum_get(*(data_t **)0x5a8d50, (int)other_index);
    object_type = *(unsigned char *)((char *)header + 3);

    switch (object_type) {
    case 0:
      biped = object_get_and_verify_type((int)other_index, 1);
      if (has_model == 0) {
        display_assert("model_instance_valid",
                       "c:\\halo\\SOURCE\\physics\\physics.c", 0x29d, 1);
        system_exit(-1);
      }
      if ((*(unsigned char *)((char *)biped + 0xb6) & 4) == 0) {
        physics_compute_biped_collision(a_ctx, (int)other_index);
      }
      break;
    case 1:
      if (other_index != (unsigned int)unit_index &&
          FUN_001509c0(state_other, (int)other_index) != 0) {
        other = object_get_and_verify_type((int)other_index, 2);
        if ((int)(other_index & 0xffff) <
              (int)((unsigned int)unit_index & 0xffff) ||
            (*(unsigned char *)((char *)other + 4) & 0x20) != 0 ||
            *(float *)state_other[1] > *(const float *)0x2533c0) {
          FUN_00151ec0(state_self, state_other);
        }
      }
      break;
    default:
      break;
    }

    cursor++;
    remaining--;
  } while (remaining != 0);
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

/* 0x154750 — advance a point-physics scalar toward its target position with
 * no velocity term; returns 1 when the target is snapped to.
 * Confirmed: FST [EBP-4] keeps the first FUN_001546f0 result, FCOMP [0x2533c0]
 * + TEST AH,0x44 / JNP takes the snap path only on equality; the second FCOMP
 * [EBP-4] + JP falls through (returns 0) only when the two results are equal.
 * The [EBP+0x10] slot is forwarded verbatim to both callees (float arg 2 of
 * FUN_001546f0, char param_3 of FUN_001544d0), same as FUN_001547d0. */
char FUN_00154750(float *out_pos, void *point_phys, float dt, float target_pos,
                  float scale)
{
  float remaining;

  remaining = FUN_001546f0(*out_pos, dt, target_pos, point_phys);
  if (remaining != *(const float *)0x2533c0) {
    FUN_001544d0(out_pos, (float *)point_phys, *(char *)&dt, remaining * scale);
    if (FUN_001546f0(*out_pos, dt, target_pos, point_phys) == remaining) {
      return 0;
    }
  }

  *out_pos = target_pos;
  return 1;
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
