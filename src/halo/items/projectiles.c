/* Clear bit 1 of projectile flags at offset 0x1dc. */
void projectile_kill_tracer(int projectile_handle)
{
  char *proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  *(uint32_t *)(proj + 0x1dc) &= ~2u;
}

/* Delete all live projectile objects (type_mask 0x20).
 * Walks every projectile in the object table via object_iterator_new/next
 * and calls object_delete on each handle. Used during level teardown or
 * game reset to purge all in-flight projectiles. */
void projectiles_delete_all(void)
{
  object_iter_t iter;

  object_iterator_new(&iter, 0x20, 0);
  while (object_iterator_next(&iter) != NULL) {
    object_delete(iter.last_handle);
  }
}

/* Set the projectile's target handle at offset 0x1e8. */
void projectile_set_target_object_index(int projectile_handle, int target)
{
  char *proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  *(int *)(proj + 0x1e8) = target;
}

/* Set bit 1 of projectile flags at offset 0x1dc. */
void projectile_make_tracer(int projectile_handle)
{
  char *proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  *(uint32_t *)(proj + 0x1dc) |= 2u;
}

/* Compute the negated gravity contribution for a projectile.
 * Multiplies the per-tick gravity constant (DAT_0032512c) by the float
 * stored at projectile_tag+0x1cc (a physics/velocity field in the proj tag),
 * then negates the result.  The sign flip converts a positive tag-stored value
 * into the downward (negative) acceleration component used by the projectile
 * physics integrator.  Leaf function, no callees. */
float projectile_get_ballistic_acceleration(int projectile_tag)
{
  return -(*(float *)0x32512c * *(float *)(projectile_tag + 0x1cc));
}

/* Compute a normalized value for a projectile tag field at offset 0x1e4.
 * If the tag field (e.g. a max-distance or scale parameter) is greater than
 * the global zero reference at 0x2533c0 (0.0f), returns value / field.
 * Otherwise returns the zero reference unchanged.
 * Used by the caller to normalize a float quantity against the tag's field,
 * guarding against division by zero or a zero/unset field. */
float projectile_estimate_time_to_target(void *proj_tag, float value)
{
  float field;
  field = *(float *)((char *)proj_tag + 0x1e4);
  if (field > *(float *)0x2533c0)
    return value / field;
  return *(float *)0x2533c0;
}

/* Return true if any projectile object (type 0x20) exists in the world.
 * Loads the projectile's tag definition as a side effect (cache priming). */
bool dangerous_projectiles_near_player(void)
{
  char iter[16];
  void *projectile;

  object_iterator_new(iter, 0x20, 0);
  projectile = object_iterator_next(iter);
  if (projectile != NULL) {
    tag_get(0x70726f6a, *(int *)projectile);
    return true;
  }
  return false;
}

/* Clear the projectile's target handle if it matches the given value. */
void projectile_handle_deleted_object(int projectile_handle, int target)
{
  char *proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  if (*(int *)(proj + 0x1e8) == target)
    *(int *)(proj + 0x1e8) = -1;
}

/* Escalate the projectile's detonation state (offset 0x1e0) to at least
 * the given state value.  Only updates the field if state is strictly
 * greater than the current value, so the state can only increase
 * (0=none, 1=pending, 2=immediate). Called by FUN_000f9c40 with state=2
 * to force an immediate detonation. */
void FUN_000f7e40(int projectile_handle, int16_t state)
{
  char *proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  if (state > *(int16_t *)(proj + 0x1e0))
    *(int16_t *)(proj + 0x1e0) = state;
}

/* Dispatch a projectile detonation effect based on the type word at tag_def[0].
 * Type 3 (contrail/attached): calls effect_new_attached_from_markers with the
 * attached object handle at tag_def[+0x38] and the marker-slot index at
 * tag_def[+0x3e] (uint16_t), plus hardcoded marker_count=5 and
 * effect_definition=0x31f3a0. Any other type: calls
 * effect_new_unattached_from_markers with NULL translational-velocity,
 * marker_count=5, effect_definition=0x31f3a0, and a trailing integer 1.
 * Both branches pass through the caller-supplied marker_points/marker_forwards
 * arrays and scale_a/scale_b values; unknown tail floats are 0.0/0.0.
 * Receive register args: esi=effect_tag_index, edi=object_index, eax=tag_def,
 * edx=marker_points, ecx=marker_forwards. */
void FUN_000f7e60(int effect_tag_index, int object_index, void *tag_def,
                  float *marker_points, float *marker_forwards, float scale_a,
                  float scale_b)
{
  int attached_handle;
  uint16_t marker_index;

  if (*(int16_t *)tag_def == 3) {
    attached_handle = *(int *)((char *)tag_def + 0x38);
    marker_index = *(uint16_t *)((char *)tag_def + 0x3e);
    effect_new_attached_from_markers(
      effect_tag_index, object_index, attached_handle, marker_index, 5,
      (void *)0x31f3a0, marker_points, marker_forwards, scale_a, scale_b, 0.0f,
      0.0f);
  } else {
    effect_new_unattached_from_markers(
      effect_tag_index, object_index, NULL, 5, (void *)0x31f3a0, marker_points,
      marker_forwards, scale_a, scale_b, 0.0f, 0.0f, 1);
  }
}

/* For each of the 4 scale slots in a projectile's tag definition, compute the
 * current scale value based on the selector enum at tag[0x184 + i*2]:
 *   0 = skip (leave projectile float unchanged)
 *   1 = age fraction: projectile[0x200] / tag[0x1c8]; 0.0 if tag[0x1c8]==0.0
 *   2 = raw float from projectile[0x1f0]
 *   3 = armed flag: 1.0 if projectile[0x1dc] & 0x2, else 0.0
 * Writes the result to projectile[0xd4 + i*4] (four consecutive float slots).
 * Asserts (halts) on any selector value outside 0..3. */
void projectile_export_function_values(int projectile_handle)
{
  char *proj;
  char *tag;
  int16_t *sel_ptr;
  float *out_ptr;
  float local_8;
  int16_t sel;
  int counter;

  proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  tag = (char *)tag_get(0x70726f6a, *(int *)proj);

  sel_ptr = (int16_t *)(tag + 0x184);
  out_ptr = (float *)(proj + 0xd4);
  counter = 4;

  do {
    sel = *sel_ptr;
    if (sel != 0) {
      if (sel == 1) {
        if (*(float *)(tag + 0x1c8) == 0.0f) {
          local_8 = 0.0f;
        } else {
          local_8 = *(float *)(proj + 0x200) / *(float *)(tag + 0x1c8);
        }
      } else if (sel == 2) {
        local_8 = *(float *)(proj + 0x1f0);
      } else if (sel == 3) {
        if (*(uint8_t *)(proj + 0x1dc) & 0x2) {
          local_8 = 1.0f;
        } else {
          local_8 = 0.0f;
        }
      } else {
        display_assert(0, "c:\\halo\\SOURCE\\items\\projectiles.c", 0x622, 1);
        system_exit(-1);
        local_8 = 0.0f;
      }
      *out_ptr = local_8;
    }
    sel_ptr++;
    out_ptr++;
    counter--;
  } while (counter != 0);
}

/* Compute the peak-intercept fraction for a projectile detonation range.
 * Given a tag struct (via ECX) with two radius values at offsets 0x1e4 and
 * 0x1e8, and a range [range_begin, range_end], returns the position within
 * the range at which a parabolic distribution peaked:
 *   result = (r1^2 - r2^2) / (2 * (range_end - range_begin))
 * Returns 0.0 if the two radii are equal or if the range width is zero.
 * Used by the projectile trajectory system to locate the apex of a parabolic
 * detonation-effect distribution along the projectile's travel path. */
float FUN_000f7fa0(void *tag, float range_begin, float range_end)
{
  float delta;
  float r1;
  float r2;

  delta = range_end - range_begin;
  r1 = *(float *)((char *)tag + 0x1e4);
  r2 = *(float *)((char *)tag + 0x1e8);

  if (r1 == r2) {
    return 0.0f;
  }
  if (delta == 0.0f) {
    return 0.0f;
  }
  return (r1 * r1 - r2 * r2) / (delta + delta);
}

/* Arm a projectile and detach it from its parent object.
 * Asserts that the projectile has a valid parent (parent_object_index != NONE
 * at offset 0xcc).  Sets both the "age scale" float at 0x1f0 and the
 * "fade scale" float at 0x1f8 to 1.0, clears bit 3 (0x8) of the flags word
 * at 0x1dc (the "attached-to-parent" flag), then calls
 * object_detach_from_parent to unlink the projectile from the weapon/unit that
 * fired it. Returns 1 (bool true) unconditionally on success. Source line
 * reference: c:\halo\SOURCE\items\projectiles.c line 1845. */
char projectile_handle_parent_destroyed(int projectile_handle)
{
  char *proj;

  proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  if (*(int *)(proj + 0xcc) == -1) {
    display_assert("projectile->object.parent_object_index != NONE",
                   "c:\\halo\\SOURCE\\items\\projectiles.c", 0x735, 1);
    system_exit(-1);
  }
  *(float *)(proj + 0x1f8) = 1.0f;
  *(float *)(proj + 0x1f0) = 1.0f;
  *(uint32_t *)(proj + 0x1dc) &= ~0x8u;
  object_detach_from_parent(projectile_handle);
  return 1;
}

/* Scatter a direction vector using the global random seed.
 * Retrieves the engine-wide random seed via get_global_random_seed_address,
 * then delegates to random_direction3d to produce a randomised unit vector
 * within 'angle' radians of 'forward'.  'zero' is always passed as 0.0
 * at call sites (minimum cone half-angle).  The resulting direction is
 * written to 'result'.  This is the global-seed variant; callers that own a
 * local seed call random_direction3d directly. */
void random_vector_in_cone3d(float *forward, float zero, float angle,
                             float *result)
{
  int *seed = get_global_random_seed_address();
  random_direction3d(seed, forward, zero, angle, result);
}

/*
 * Ballistic arc trajectory solver.
 *
 * Computes the initial aim direction for a projectile that must follow a
 * parabolic arc from origin to target under gravity.  The caller supplies an
 * initial speed budget (speed), the effective per-tick gravity scale (gravity),
 * 3D origin and target vectors, and optional constraints / override values.
 * On success the function fills aim_vector with the normalised launch direction
 * and populates up to five optional output scalars; it returns 1 when a
 * non-trivial arc solution was found and 0 when the fallback minimum-range
 * trajectory is used instead.
 *
 * Algorithm overview
 * ------------------
 * 1. Compute the displacement delta = target - origin.
 * 2. Derive the effective gravity coefficient
 *      a_coeff = max(0, per_tick_const * gravity)
 *    and the quadratic coefficient a = a_coeff^2 * 0.25.
 * 3. Compute the discriminant base (4*a*c, where c = |delta|^2) and verify
 *    that a solution exists (assert 4ac > 0).
 * 4. The maximum time-of-flight for which the quadratic has real solutions is
 *      t_sq_max = sqrt(4*a*c) / (2*a)
 *    (asserted >= 0).  t_max = sqrt(t_sq_max).
 * 5. The minimum time-of-flight (t_min) is:
 *      t_min = sqrt(a_coeff * dz - disc_base)  if that is >= 0, else 0.
 * 6. Choose launch speed V:
 *    - If param_7 != NULL: V = *param_7.
 *    - Else start with V = speed; if param_6 != NULL and *param_6 > 0 then
 *      reduce V to match the desired time (capped at the caller's max speed).
 * 7. If a valid arc solution exists (t_min <= V), solve the quadratic for the
 *    time parameter t_disc and replace V when the result is valid.
 * 8. Build the initial velocity vector (vx,vy,vz) from the chosen t/V values,
 *    normalise it, and write the unit direction to aim_vector.
 * 9. Fill optional outputs: param_10 = time, param_11 = t_param,
 *    param_12 = t_param*time, param_13 = vertical velocity, param_14 =
 *    horizontal speed magnitude.
 *
 * Source ref: c:\halo\SOURCE\items\projectiles.c lines 0x2ee-0x36f.
 */
char projectile_aim_ballistic(float speed, float gravity, float *origin,
                              float *target, int param_5, float *param_6,
                              float *param_7, char param_8, float *aim_vector,
                              float *param_10, float *param_11, float *param_12,
                              float *param_13, float *param_14)
{
  /* Local variables mirror the original MSVC stack frame layout.
   * Frame size: SUB ESP,0x34 (= 52 bytes = 13 float slots + 1 char).
   *
   * Locals at EBP-N (Ghidra names):
   *   EBP-0x34 = aim_vec[0]   (float buffer for normalize3d)
   *   EBP-0x30 = aim_vec[1]
   *   EBP-0x2c = aim_vec[2]   (also holds aim_z)
   *   EBP-0x28 = dx
   *   EBP-0x24 = dy
   *   EBP-0x20 = dz
   *   EBP-0x1c = t_max
   *   EBP-0x18 = two_a
   *   EBP-0x14 = dist_sq
   *   EBP-0x10 = disc_base (-sqrt(4ac)), then t_min
   *   EBP-0x0c = c4 = 4*a*c
   *   EBP-0x08 = b = a_coeff*dz
   *   EBP-0x01 = result (char)
   *
   * Parameter slots reused as float temps by the original MSVC code:
   *   EBP+0x08 (speed)   -> t_sol
   *   EBP+0x0c (gravity) -> a (quadratic coefficient a = a_coeff^2*0.25)
   *   EBP+0x10 (origin)  -> a_coeff, then t_sol*V_out product
   *   EBP+0x14 (target)  -> V (chosen speed), then V_out
   *
   * Source ref: c:\halo\SOURCE\items\projectiles.c lines 0x2ee-0x36f.
   */
  /* Declare as contiguous array so normalize3d reads all three components.
   * MSVC placed local_38/34/30 adjacent (EBP-0x34/-0x30/-0x2c); clang may
   * insert a gap (local_14 lands between Y and Z), causing normalize3d to
   * read the wrong Z and leave it unnormalized → assert_valid_real_normal3d. */
  float aim_xyz[3]; /* [0]=local_38 (X), [1]=local_34 (Y), [2]=local_30 (Z) */
#define local_38 aim_xyz[0]
#define local_34 aim_xyz[1]
#define local_30 aim_xyz[2]
  float local_2c; /* dx */
  float local_28; /* dy */
  float local_24; /* dz */
  float local_20; /* t_max */
  float local_1c; /* two_a = 2*a */
  float local_18; /* dist_sq */
  float local_14; /* disc_base = -sqrt(4*a*c), then t_min */
  float local_10; /* c4 = 4*a*c */
  float local_c; /* b = a_coeff*dz */
  char local_5; /* result flag: 1=arc, 0=fallback */
  float a; /* quadratic coeff a = a_coeff^2 * 0.25 */
  float a_coeff; /* effective gravity: max(0, per_tick*gravity) */
  float V; /* chosen launch speed, then V_out at output stage */
  float fVar1; /* scratch */
  float fVar2; /* scratch */
  float partial; /* dy^2 + dx^2 partial sum for interleaved dist_sq */

  /* 1. Displacement = target - origin.
   * local_5 is set to 1 here to match the original's instruction order:
   * FSUB,FSTP(dx),MOVB(1),FSUB,FSTP(dy),FSUB,FSTP(dz). */
  local_2c = target[0] - origin[0];
  local_5 = 1;
  local_28 = target[1] - origin[1];
  local_24 = target[2] - origin[2];

  /* 2. Partial distance sum (dy^2 + dx^2) computed first.
   * The original interleaves this with the gravity computation:
   * partial stays on the FPU stack as st1 while a_coeff/a are computed,
   * then dz^2 is added to partial to complete dist_sq. */
  partial = local_28 * local_28 + local_2c * local_2c;

  /* 3. Effective gravity coefficient, clamped to zero. */
  a_coeff = *(float *)0x32512c * gravity;
  if (a_coeff < *(float *)0x2533c0) {
    a_coeff = 0.0f;
  }

  /* 4. Quadratic coefficient a = a_coeff^2 * 0.25.
   * Two-step to force a_coeff*a_coeff before *0.25 (matches MSVC operand
   * order). */
  fVar1 = a_coeff * a_coeff;
  a = fVar1 * *(float *)0x25337c;

  /* 5. Complete dist_sq by adding dz^2 to partial sum. */
  local_18 = local_24 * local_24 + partial;

  /* 6. c4 = dist_sq * a * 4.0; assert > 0.
   * Two-step to force dist_sq*a before *4.0 (matches MSVC operand order). */
  fVar1 = local_18 * a;
  local_10 = fVar1 * *(float *)0x2533d8;
  if (local_10 <= *(float *)0x2533c0) {
    display_assert("4.0f * a * c > 0.0f",
                   "c:\\halo\\SOURCE\\items\\projectiles.c", 0x2f8, 1);
    system_exit(-1);
  }

  /* 7. disc_base = -sqrt(c4); two_a = 2*a. */
  local_14 = -sqrtf(local_10);
  local_1c = a + a;

  /* t_sq_max = -disc_base / two_a; assert >= 0. */
  V = -local_14 / local_1c;
  if (V < *(float *)0x2533c0) {
    display_assert("t_squared_max >= 0.0f",
                   "c:\\halo\\SOURCE\\items\\projectiles.c", 0x2fc, 1);
    system_exit(-1);
  }
  local_20 = sqrtf(V); /* t_max */

  /* 8. b = a_coeff * dz; t_min = sqrt(b - disc_base) if >= 0, else 0.
   * Branch polarity: original falls through to the zero path, jumps to sqrt. */
  local_c = a_coeff * local_24;
  if (local_c - local_14 < *(float *)0x2533c0) {
    local_14 = 0.0f;
  } else {
    local_14 = sqrtf(local_c - local_14);
  }

  /* 9. Choose launch speed V. */
  if (param_7 != NULL) {
    V = *param_7;
  } else {
    V = speed;
    if ((param_6 != NULL) && (*param_6 > *(float *)0x2533c0)) {
      fVar2 = local_20 * *param_6;
      fVar2 = fVar2 * fVar2;
      fVar1 = local_c - -(fVar2 * a + local_18 / fVar2);
      if (fVar1 <= *(float *)0x2533c0) {
        display_assert("v_desired_sq > 0.0f",
                       "c:\\halo\\SOURCE\\items\\projectiles.c", 0x326, 1);
        system_exit(-1);
      }
      fVar1 = sqrtf(fVar1);
      if (speed > fVar1) {
        V = fVar1;
      }
    }
  }

  /* 10. If V >= t_min, try to find an arc solution. */
  if (V >= local_14) {
    fVar1 = local_c - V * V;
    fVar2 = fVar1 * fVar1 - local_10;
    if ((fVar1 < *(float *)0x2533c0) && (fVar2 >= *(float *)0x2533c0)) {
      speed =
        (sqrtf(fVar2) * (float)(int)((unsigned int)(param_8 != '\0') * 2 + -1) -
         fVar1) /
        local_1c;
      if (*(float *)0x2533c0 < speed) {
        speed = sqrtf(speed);
        goto LAB_output;
      }
    }
  }
  local_5 = 0;
  speed = local_20; /* t_sol = t_max */
  V = local_14; /* V_out = t_min */

LAB_output:
  /* 11. Build velocity direction (dx/t, dy/t, a_coeff*t*0.5 + dz/t). */
  fVar1 = *(float *)0x2533c8 / speed;
  local_38 = local_2c * fVar1;
  local_34 = local_28 * fVar1;
  local_30 = fVar1 * local_24 + speed * a_coeff * *(float *)0x253398;

  /* Precompute sqrt(aim_y^2 + aim_x^2) for param_14 output, stored early. */
  fVar2 = local_34 * local_34 + local_38 * local_38;
  fVar2 = sqrtf(fVar2);

  /* Store aim_z before normalize overwrites local_30. */
  fVar1 = local_30;

  /* Compute t_sol*V_out product into a_coeff slot (mirrors MSVC FSTP EBP+0x10).
   */
  a_coeff = speed * V;

  if (normalize3d(aim_xyz) == *(float *)0x2533c0) {
    /* Degenerate: fall back to displacement direction. */
    local_38 = local_2c;
    local_5 = 0;
    local_34 = local_28;
    local_30 = local_24;
    if (normalize3d(aim_xyz) == *(float *)0x2533c0) {
      /* Degenerate displacement: use global up vector. */
      local_30 = *(float *)(*(int *)0x31fc44 + 8);
      local_38 = *(float *)(*(int *)0x31fc44);
      local_34 = *(float *)(*(int *)0x31fc44 + 4);
    }
  }

  if (aim_vector == NULL) {
    display_assert("result_aim_vector",
                   "c:\\halo\\SOURCE\\items\\projectiles.c", 0x363, 1);
    system_exit(-1);
  }

  aim_vector[0] = local_38;
  aim_vector[1] = local_34;
  aim_vector[2] = local_30;

  if (param_12 != NULL) {
    *param_12 = a_coeff; /* t_sol * V_out */
  }
  if (param_10 != NULL) {
    *param_10 = V; /* V_out */
  }
  if (param_13 != NULL) {
    *param_13 = fVar1; /* aim_z before normalize */
  }
  if (param_14 != NULL) {
    *param_14 = fVar2; /* sqrt(aim_y^2 + aim_x^2) */
  }
  if (param_11 != NULL) {
    *param_11 = speed; /* t_sol */
  }
  return local_5;
#undef local_38
#undef local_34
#undef local_30
}

/* Compute the straight-line aim vector and travel parameters for a projectile
 * with no ballistic arc (no gravity).
 * Subtracts origin from target to form the direction delta, normalises it in
 * place (normalize3d overwrites the local vector with the unit vector and
 * returns the original length as the distance), then writes outputs:
 *   aim_vector  - normalised direction from origin to target (required,
 * asserted non-NULL) out_dist    - optional: raw length of the origin→target
 * vector out_speed   - optional: copy of the input speed out_t       -
 * optional: travel time = dist / speed; 0.0 if speed <= 0.0 Returns 1 (bool
 * true) unconditionally. Source ref: c:\halo\SOURCE\items\projectiles.c line
 * 0x399 (921). */
int projectile_aim_linear(float speed, float *origin, float *target,
                          float *aim_vector, float *out_speed, float *out_t,
                          float *out_dist)
{
  float local_vec[3];
  float dist;
  float t;

  local_vec[0] = target[0] - origin[0];
  local_vec[1] = target[1] - origin[1];
  local_vec[2] = target[2] - origin[2];

  dist = normalize3d(local_vec);

  if (speed <= *(float *)0x2533c0) {
    t = 0.0f;
  } else {
    t = dist / speed;
  }

  if (aim_vector == NULL) {
    display_assert("result_aim_vector",
                   "c:\\halo\\SOURCE\\items\\projectiles.c", 0x399, 1);
    system_exit(-1);
  }

  aim_vector[0] = local_vec[0];
  aim_vector[1] = local_vec[1];
  aim_vector[2] = local_vec[2];

  if (out_dist != NULL) {
    *out_dist = dist;
  }
  if (out_speed != NULL) {
    *out_speed = speed;
  }
  if (out_t != NULL) {
    *out_t = t;
  }

  return 1;
}

/* Resolve the launch speed for a projectile and compute its aim direction.
 *
 * If param_4 is NULL the speed is read from the projectile tag definition at
 * offset 0x1e4 (pointer to float).  Otherwise param_4 is dereferenced to get
 * the speed value.  The resolved speed is stored back into param_4 so both
 * call paths share the same code below.
 *
 * If the projectile tag has the ballistic-arc flag set (bit 1 of byte at
 * param_1+0x17c) AND the per-arc gravity value at param_1+0x1cc is > 0.0f,
 * projectile_aim_ballistic (ballistic arc solver) is called to compute a curved
 * trajectory.  Otherwise the simpler straight-line aim helper
 * projectile_aim_linear is used.  param_13, when non-NULL, receives 0 for the
 * arc path and 1 for the straight-line path. */
void projectile_aim(int projectile_tag, int param_2, int param_3, void *param_4,
                    int param_5, int param_6, int param_7, int param_8,
                    int param_9, int param_10, int param_11, int param_12,
                    void *param_13)
{
  float speed;
  char *out;

  if (param_4 == NULL) {
    speed = *(float *)(projectile_tag + 0x1e4);
  } else {
    speed = *(float *)param_4;
  }

  out = (char *)param_13;

  if ((*(unsigned char *)(projectile_tag + 0x17c) & 2) &&
      (*(float *)(projectile_tag + 0x1cc) > *(float *)0x2533c0)) {
    projectile_aim_ballistic(speed, *(float *)(projectile_tag + 0x1cc),
                             (float *)param_2, (float *)param_3, param_5,
                             (float *)param_6, (float *)param_7, (char)param_8,
                             (float *)param_9, (float *)param_10,
                             (float *)param_11, (float *)param_12, 0, 0);
    if (out != NULL) {
      *out = 0;
    }
  } else {
    projectile_aim_linear(speed, (float *)param_2, (float *)param_3,
                          (float *)param_9, (float *)param_10,
                          (float *)param_11, (float *)param_12);
    if (out != NULL) {
      *out = 1;
    }
  }
}

/* Compute projectile velocity direction and speed angle from the velocity
 * stored at obj+0x3c (vx/vy/vz). If the speed magnitude is non-zero, sets
 * the velocity-valid flag (bit 0 at obj+0x1dc), stores the normalized
 * direction into obj+0x214..0x21c, and stores sin(speed)/cos(speed) at
 * obj+0x220/0x224. If speed is zero, clears the flag and stores sin=0,
 * cos=1. Takes projectile_handle in EAX (register arg). */
void FUN_000f8590(int projectile_handle)
{
  char *obj;
  float vx, vy, vz;
  float speed;
  float inv_speed;
  uint32_t flags;

  obj = (char *)object_get_and_verify_type(projectile_handle, 0x20);

  vx = *(float *)(obj + 0x3c);
  vy = *(float *)(obj + 0x40);
  vz = *(float *)(obj + 0x44);
  speed = sqrtf(vx * vx + vy * vy + vz * vz);

  flags = *(uint32_t *)(obj + 0x1dc);

  if (speed != *(float *)0x2533c0) {
    inv_speed = *(float *)0x2533c8 / speed;
    *(uint32_t *)(obj + 0x1dc) = flags | 0x1u;
    *(float *)(obj + 0x214) = inv_speed * vx;
    *(float *)(obj + 0x218) = inv_speed * vy;
    *(float *)(obj + 0x21c) = inv_speed * vz;
    *(float *)(obj + 0x220) = sinf(speed);
    *(float *)(obj + 0x224) = cosf(speed);
  } else {
    *(uint32_t *)(obj + 0x1dc) = flags & ~0x1u;
    *(float *)(obj + 0x220) = 0.0f;
    *(float *)(obj + 0x224) = *(float *)0x2533c8;
  }
}

/* Initialise the projectile's parabolic detonation-radius cache fields
 * (proj+0x20c, proj+0x210, proj+0x208, proj+0x204).
 *
 * Fetches the 'proj' tag definition for the object, then selects between two
 * sets of tag range fields based on bit 4 of the object flags byte at proj+4:
 *
 *   bit4 SET   (detonating state):
 *     range_begin = tag+0x1dc, range_end = tag+0x1e0
 *     compare field for threshold: tag+0x1dc
 *   bit4 CLEAR (non-detonating state):
 *     range_begin = tag+0x1d0, range_end = tag+0x1d4
 *     compare field for threshold: tag+0x1d0
 *
 * For both branches:
 *   proj+0x20c = FUN_000f7fa0(tag, range_begin, range_end)  [parabolic apex]
 *   proj+0x210 = tag+0x1e0  [raw end-range copy]
 *   If range_begin > 0.0:
 *     proj+0x208 = range_begin / tag+0x1e4
 *   Else (range_begin <= 0.0 or zero threshold):
 *     proj+0x204 = 1.0f
 *     proj+0x208 = 0.0f
 *
 * Called with projectile_handle in EAX (register arg). */
void FUN_000f8640(int projectile_handle)
{
  char *proj;
  char *tag_def;
  float range_begin;
  float ratio;

  proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  tag_def = (char *)tag_get(0x70726f6a, *(int *)proj);

  if (*(uint8_t *)(proj + 0x4) & 0x10) {
    /* detonating branch: use tag offsets 0x1dc/0x1e0 */
    *(float *)(proj + 0x20c) = FUN_000f7fa0(
      tag_def, *(float *)(tag_def + 0x1dc), *(float *)(tag_def + 0x1e0));
    *(int *)(proj + 0x210) = *(int *)(tag_def + 0x1e0);
    range_begin = *(float *)(tag_def + 0x1dc);
  } else {
    /* non-detonating branch: use tag offsets 0x1d0/0x1d4 */
    *(float *)(proj + 0x20c) = FUN_000f7fa0(
      tag_def, *(float *)(tag_def + 0x1d0), *(float *)(tag_def + 0x1d4));
    *(int *)(proj + 0x210) = *(int *)(tag_def + 0x1e0);
    range_begin = *(float *)(tag_def + 0x1d0);
  }

  if (range_begin > *(float *)0x2533c0) {
    ratio = range_begin / *(float *)(tag_def + 0x1e4);
    *(float *)(proj + 0x208) = ratio;
    return;
  }

  *(float *)(proj + 0x204) = 1.0f;
  *(float *)(proj + 0x208) = 0.0f;
}

/*
 * Swept-sphere lateral collision test for a projectile (0xf8720).
 *
 * Tests whether a projectile's movement from its current world position to
 * new_pos would collide with the BSP. Three separate ray/segment casts are
 * performed:
 *
 *   1. A direct centre-line cast from proj_pos toward new_pos (flags 0x1000e9).
 *   2. A lateral cast offset +radius in the cross(up, delta) direction (0x89).
 *   3. A lateral cast offset -radius in that same direction (0x89).
 *
 * The cross direction is the unit vector perpendicular to both the world-up
 * vector and the movement delta.  If the delta is parallel to world-up the
 * function falls back to the global default forward vector.
 *
 * radius = *(float *)(tag_def + 0x1a0)
 * If radius < ~1e-7 the lateral sweeps are skipped and returns 0.
 *
 * Register args: projectile_handle@<eax>, new_pos@<edi>
 * Stack arg:     collision_result (int16_t *)
 * Returns: 1 if any cast hit, 0 if all missed.
 *
 * Confirmed: PUSH 0x20; PUSH EAX -> object_get_and_verify_type(handle, 0x20).
 * Confirmed: PUSH ECX; PUSH 0x70726f6a -> tag_get('proj', obj->tag_index).
 * Confirmed: ESI = LEA [EBX+0xc] = &obj->position (float[3] at offset 0xc).
 * Confirmed: EBX+0x1e4 = proj.field_1e4 used as max_distance arg.
 * Confirmed: first FUN_0014df70 call: flags=0x1000e9, origin=&proj_pos,
 *            dir=&delta, max_dist=proj.field_1e4, result=param_1.
 * Confirmed: EAX=[0x31fc44] -> global_up_vector_ptr (float*).
 * Confirmed: cross = cross(up_vec, delta) from FLD/FMUL/FSUBP sequences.
 * Confirmed: CALL 0x13010 -> normalize3d; FCOMP [0x2533c0] -> compare to 0.
 * Confirmed: fallback to EAX=[0x31fc40] -> global_default_fwd_ptr (float*).
 * Confirmed: tag_def[0x1a0] = sweep radius; FCOMP [0x253f44] ~= 1e-7.
 * Confirmed: second call flags=0x89, origin=[EBP-0x1c], dir=[EBP-0x10].
 * Confirmed: FUN_000130d0(0x89, [EBP-0x40], [EBP-0x34], ..) for neg-side.
 * Confirmed: FSTP ST0 at 0xf8898 discards -radius; ST2 (new_pos.z+r*cz) used
 *            directly in FSUB [EBP-0x14] at 0xf88d3 to compute dir.z.
 */
bool FUN_000f8720(int projectile_handle, float *new_pos,
                  int16_t *collision_result)
{
  char *proj;
  void *tag_def;
  float *proj_pos; /* &obj->position (float[3] at proj+0xc) */
  float *up_vec;
  float *fwd_vec;
  float dx, dy, dz; /* movement delta: new_pos - proj_pos */
  float radius;
  /* cross direction: cross(up_vec, delta), stored in dir1 then normalized */
  float delta[3]; /* movement direction for centre-line cast */
  float dir1[3]; /* normalized cross direction; later reused as sweep dir */
  /* positive-side origin: proj_pos + radius * cross */
  float origin1[3];
  /* positive-side endpoint (x,y); z held on FPU and used directly */
  float pt_b1x, pt_b1y, pt_b1z;
  /* negative-side origin: proj_pos - radius * cross */
  float pt_a2[3];
  /* negative-side endpoint: new_pos - radius * cross */
  float pt_b2[3];

  proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  tag_def = tag_get(0x70726f6a, *(int *)proj);
  proj_pos = (float *)(proj + 0xc);

  dx = new_pos[0] - proj_pos[0];
  dy = new_pos[1] - proj_pos[1];
  dz = new_pos[2] - proj_pos[2];

  /* 1. Centre-line collision test (flags 0x1000e9). */
  delta[0] = dx;
  delta[1] = dy;
  delta[2] = dz;
  if (FUN_0014df70(0x1000e9, proj_pos, delta, *(int *)(proj + 0x1e4),
                   collision_result)) {
    return 1;
  }

  /* Check sweep radius; if too small skip lateral tests. */
  radius = *(float *)((char *)tag_def + 0x1a0);
  if (radius < *(float *)0x253f44) {
    return 0;
  }

  /* Compute cross direction: cross(up_vec, delta). */
  up_vec = *(float **)0x31fc44;
  dir1[0] = dz * up_vec[1] - dy * up_vec[2];
  dir1[1] = dx * up_vec[2] - dz * up_vec[0];
  dir1[2] = dy * up_vec[0] - dx * up_vec[1];

  if (normalize3d(dir1) == *(float *)0x2533c0) {
    /* Degenerate (delta parallel to up): fall back to default forward. */
    fwd_vec = *(float **)0x31fc40;
    dir1[0] = fwd_vec[0];
    dir1[1] = fwd_vec[1];
    dir1[2] = fwd_vec[2];
  }

  /* Build positive-side origin: proj_pos + radius * cross_dir. */
  origin1[0] = dir1[0] * radius + proj_pos[0];
  origin1[1] = dir1[1] * radius + proj_pos[1];
  origin1[2] = dir1[2] * radius + proj_pos[2];

  /* Build positive-side endpoint: new_pos + radius * cross_dir. */
  pt_b1x = dir1[0] * radius + new_pos[0];
  pt_b1y = dir1[1] * radius + new_pos[1];
  pt_b1z = dir1[2] * radius + new_pos[2];

  /* Build negative-side origin: proj_pos - radius * cross_dir. */
  pt_a2[0] = dir1[0] * (-radius) + proj_pos[0];
  pt_a2[1] = dir1[1] * (-radius) + proj_pos[1];
  pt_a2[2] = dir1[2] * (-radius) + proj_pos[2];

  /* Build negative-side endpoint: new_pos - radius * cross_dir. */
  pt_b2[0] = dir1[0] * (-radius) + new_pos[0];
  pt_b2[1] = dir1[1] * (-radius) + new_pos[1];
  pt_b2[2] = dir1[2] * (-radius) + new_pos[2];

  /* Compute sweep direction for positive-side cast: pt_b1 - origin1. */
  dir1[0] = pt_b1x - origin1[0];
  dir1[1] = pt_b1y - origin1[1];
  dir1[2] = pt_b1z - origin1[2];

  /* 2. Positive-side lateral collision test (flags 0x89). */
  if (FUN_0014df70(0x89, origin1, dir1, *(int *)(proj + 0x1e4),
                   collision_result)) {
    return 1;
  }

  /* 3. Negative-side lateral collision test: segment pt_a2 -> pt_b2 (flags
   * 0x89). */
  if (FUN_000130d0(0x89, pt_a2, pt_b2, *(int *)(proj + 0x1e4),
                   collision_result)) {
    return 1;
  }

  return 0;
}

/* Detonate a projectile: fire effects, apply area damage, notify AI.
 *
 * param_1 (projectile_handle): datum handle of the projectile object.
 * param_2 (has_hit_count): non-zero if the projectile has accumulated a hit
 *   counter (used to decide whether to update the contrail attachment state).
 * param_3 (current_time): current game time in seconds; used to compute the
 *   contrail delta-time argument.
 *
 * Flow:
 *   1. If the tag has the burst-fire flag (bit 3 at tag+0x17c) and the
 *      projectile has a parent with more than 6 siblings of the same type,
 *      kill or randomize the excess siblings' velocities and then
 *      detach from the parent and reposition at the burst centre.
 *   2. If has_hit_count and the projectile has a valid contrail attachment,
 *      compute node matrices and set the contrail state.
 *   3. Fire the primary effect at the projectile's world position.
 *   4. If the projectile has a parent and the tag has splash-damage data
 *      (tag+0x220), apply area damage to the scene.
 *   5. Look up the detonation-effect index (proj+0x1e2); fire a per-slot
 *      secondary effect if the index is valid.
 *   6. Notify the AI subsystem of the detonation position.
 *
 * Binary: 0x000f8920 in projectiles.obj.
 * Confirmed: prototype from caller FUN_000f9c40 @ 0xfab65
 *   (SETZ AL for param_2, EDX=[EBP-0x18] for param_3, ECX=[EBP+8] for param_1).
 */
void FUN_000f8920(int projectile_handle, char has_hit_count, float current_time)
{
  char *proj; /* projectile object data pointer (type 0x20) */
  char *proj_tag; /* projectile tag data pointer (group 'proj') */
  int effect_tag; /* current effect tag index; updated in burst block */
  void *effect_def[2]; /* effect definition passed to
                          effect_new_unattached_from_markers */

  /* burst-limit locals */
  char *parent_obj; /* type-any parent object ptr */
  int sibling; /* datum handle of current sibling in child list */
  char *sibling_base; /* type-any sibling object ptr */
  char *sibling_proj; /* type-0x20 sibling projectile ptr */
  int count; /* count of active same-type siblings */

  /* world position and orientation buffers */
  float pos[3]; /* projectile world position ([EBP-0x30]) */
  /* MSVC stack overlap: fwd[0..2] at [EBP-0x54], up_buf at [EBP-0x48] are
   * contiguous. Effects system indexes forwards[1] when marker_count=2 and
   * the event matches "gravity", reading what MSVC laid out as up_buf. */
  float fwd[6]; /* [0..2]=forward, [3..5]=up (second marker forward) */
  float up_buf[3]; /* temp for *0x31fc50; copied into fwd[3..5] */
  float parent_pos[3]; /* parent world position (local_b8) */
  float saved_vel[3]; /* saved proj object position at 0xc..0x14 */

  /* damage params for area damage (0xac bytes as in damage_data_new) */
  char damage_params[0xac];
  float fwd2[3]; /* forward buf for area-damage object_get_orientation
                    ([EBP-0x74]) */
  float pos2[3]; /* world pos for area-damage FUN_001412f0 ([EBP-0x8c]) */

  /* secondary detonation effect */
  short det_idx; /* proj->detonation_effect_index at proj+0x1e2 */
  void *det_entry; /* pointer to tag-block element */
  int det_effect;

  /* AI notification */
  short ai_volume;

  /* --- Setup. --- */
  proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  proj_tag = (char *)tag_get(0x70726f6a, *(int *)proj);
  effect_tag = *(int *)(proj_tag + 0x1b8);

  /* Effect definition struct: two pointer-sized globals on the stack. */
  effect_def[0] = (void *)0x25386fu; /* DAT_0025386f */
  effect_def[1] = (void *)0x26ad40u; /* "gravity" marker name string */

  /* --- Block 1: Burst-fire sibling count limit. ---
   * Condition: tag flag bit 3 at +0x17c (burst-limit enabled),
   *            proj not already detonating (bit 6 at +0x1dc clear),
   *            projectile has a valid parent (proj+0xcc != -1). */
  if (((*(uint8_t *)(proj_tag + 0x17c) & 8u) != 0) &&
      ((*(uint8_t *)(proj + 0x1dc) & 0x40u) == 0) &&
      (*(int *)(proj + 0xcc) != -1)) {
    parent_obj = (char *)object_get_and_verify_type(*(int *)(proj + 0xcc), -1);
    sibling = *(int *)(parent_obj + 0xc8); /* first child */
    count = 0;

    /* Count siblings with the same tag group that are not detonating. */
    while (sibling != -1) {
      sibling_base = (char *)object_get_and_verify_type(sibling, -1);
      if (*(int *)sibling_base == *(int *)proj) {
        sibling_proj = (char *)object_get_and_verify_type(sibling, 0x20);
        if ((*(uint8_t *)(sibling_proj + 0x1dc) & 0x40u) == 0)
          count++;
      }
      sibling = *(int *)(sibling_base + 0xc4);
    }

    /* Proceed if: parent has no weapon (short at +0x64 == 0),
     * game engine not running or game_engine_running() returns true,
     * and (short)count > 6. */
    if ((*(short *)(parent_obj + 0x64) == 0) &&
        ((*(int *)((char *)object_get_and_verify_type(*(int *)(proj + 0xcc),
                                                      1) +
                   0x1c8) == -1) ||
         game_engine_running()) &&
        ((short)count > 6)) {
      sibling = *(int *)(parent_obj + 0xc8);
      while (sibling != -1) {
        sibling_base = (char *)object_get_and_verify_type(sibling, -1);
        if (*(int *)sibling_base == *(int *)proj) {
          sibling_proj = (char *)object_get_and_verify_type(sibling, 0x20);
          if ((*(uint8_t *)(sibling_proj + 0x1dc) & 0x40u) == 0) {
            /* Second lookup: get proj ptr for mutation. */
            sibling_proj = (char *)object_get_and_verify_type(sibling, 0x20);
            if ((short)count <= 6) {
              /* Remaining (<=6): mark detonating, randomize vel. */
              *(uint32_t *)(sibling_proj + 0x1dc) |= 0x40u;
              *(float *)(sibling_proj + 0x1f0) *= random_math_real(
                (unsigned int *)get_global_random_seed_address());
              *(float *)(sibling_proj + 0x1f8) *= random_math_real(
                (unsigned int *)get_global_random_seed_address());
            } else {
              /* Excess (>6): zero angular/linear velocity. */
              *(float *)(sibling_proj + 0x1f0) = 0.0f;
              *(float *)(sibling_proj + 0x1f8) = 0.0f;
            }
            count--;
          }
        }
        sibling = *(int *)(sibling_base + 0xc4);
      }

      /* Switch effect to burst-centre effect (tag+0x198). */
      effect_tag = *(int *)(proj_tag + 0x198);

      /* Detach from parent and reposition at burst centre. */
      object_get_world_position(*(int *)(proj + 0xcc), (vector3_t *)parent_pos);
      object_detach_from_parent(projectile_handle);

      /* Save projectile position from obj+0xc. */
      saved_vel[0] = *(float *)(proj + 0xc);
      saved_vel[1] = *(float *)(proj + 0x10);
      saved_vel[2] = *(float *)(proj + 0x14);

      object_translate(projectile_handle, parent_pos, (void *)0);
      object_try_place(projectile_handle, saved_vel);
      object_update_children_recursive(projectile_handle);
    }
  }

  /* (EBX restored from [EBP-8] = proj_tag; EDI restored from [EBP+8] = param_1.
   * In C these variables are unchanged.) */

  /* --- Block 2: Contrail attachment update. ---
   * Only if has_hit_count, and attachment index and contrail handle are valid.
   */
  if (has_hit_count && (*(int *)(proj + 0x1ec) != -1) &&
      (*(int *)(proj + 0xfc + *(int *)(proj + 0x1ec) * 4) != -1)) {
    object_compute_node_matrices(projectile_handle);

    /* contrail_set_state_for_object(contrail_handle, reset_points, dt):
     * dt = (DAT_0028ab38) * (*(float*)0x2533c8 - current_time).
     * Binary: FLD 0x2533c8; FSUB [EBP+0x10]; FMUL 0x28ab38;
     *         FSTP [ESP]; PUSH 0; PUSH ECX; CALL 0x986d0.
     * This is the push-then-fstp float argument pattern. */
    contrail_set_state_for_object(
      *(int *)(proj + 0xfc + *(int *)(proj + 0x1ec) * 4), 0,
      (*(float *)0x2533c8u - current_time) * *(float *)0x28ab38u);
  }

  /* --- Block 3: Primary effect at projectile world position. --- */
  object_get_world_position(projectile_handle, (vector3_t *)pos);
  object_get_orientation(projectile_handle, fwd, up_buf);

  /* MSVC stack overlap: up_buf at [EBP-0x48] is the second marker forward
   * (forwards[1]) when the effects system indexes with marker_count=2. */
  up_buf[0] = (*(float **)0x31fc50u)[0];
  up_buf[1] = (*(float **)0x31fc50u)[1];
  up_buf[2] = (*(float **)0x31fc50u)[2];
  fwd[3] = up_buf[0];
  fwd[4] = up_buf[1];
  fwd[5] = up_buf[2];

  effect_new_unattached_from_markers(effect_tag, *(int *)(proj + 0x74),
                                     (float *)0, 2, effect_def, pos, fwd, 0.0f,
                                     0.0f, 0.0f, 0.0f, 1.0f);

  /* --- Block 4: Area damage (if parent valid and tag has splash entry). --- */
  if ((*(int *)(proj + 0xcc) != -1) && (*(int *)(proj_tag + 0x220) != -1)) {
    damage_data_new(damage_params, *(int *)(proj_tag + 0x220));

    /* Set "area damage" flag bit 3 (at damage_params+4). */
    *(uint32_t *)(damage_params + 4) |= 8u;

    /* Forward only (no up needed). */
    object_get_orientation(projectile_handle, fwd2, (float *)0);

    /* Get world position for damage origin. */
    object_get_world_position(projectile_handle, (vector3_t *)pos2);

    /* Store position into damage_params (offsets from disasm:
     * [EBP-0x80] = damage_params+0x28, [EBP-0x7c] = +0x2c, [EBP-0x78] = +0x30).
     * Confirmed by: MOV [EBP-0x80],EDX; MOV [EBP-0x7c],EAX; MOV [EBP-0x78],ECX
     * where damage_params base = [EBP-0xa8]. */
    *(float *)(damage_params + 0x28) = pos2[0];
    *(float *)(damage_params + 0x2c) = pos2[1];
    *(float *)(damage_params + 0x30) = pos2[2];

    /* Object-index and team fields.
     * [EBP-0x9c] = damage_params+0x0c = obj+0x74 (object index).
     * [EBP-0xa0] = damage_params+0x08 = obj+0x70.
     * [EBP-0x98] = damage_params+0x10 = word at obj+0x68 (team). */
    *(int *)(damage_params + 0x0c) = *(int *)(proj + 0x74);
    *(int *)(damage_params + 0x08) = *(int *)(proj + 0x70);
    *(short *)(damage_params + 0x10) = *(short *)(proj + 0x68);

    object_cause_damage(
      damage_params, *(int *)(proj + 0xcc), (short)-1,
      (short)-1, /* dup-args-ok: verified 3x PUSH -1 at 0x000f8c5e/6c/7a */
      (short)-1, 0u);
  }

  /* --- Block 5: Secondary detonation effect by per-slot index. ---
   * proj->detonation_effect_index at proj+0x1e2 (short).
   * If -1: skip. If negative (but not -1): use global null entry 0x31ed08.
   * If out of range: same null entry. Otherwise: tag_block_get_element. */
  det_idx = *(short *)(proj + 0x1e2);

  if (det_idx != (short)-1) {
    if (det_idx < 0) {
      det_entry = (void *)0x31ed08u;
    } else if ((int)det_idx >= *(int *)(proj_tag + 0x240)) {
      det_entry = (void *)0x31ed08u;
    } else {
      det_entry =
        tag_block_get_element((void *)(proj_tag + 0x240), (int)det_idx, 0xa0);
    }

    det_effect = *(int *)((char *)det_entry + 0x74);
    effect_new_unattached_from_markers(det_effect, *(int *)(proj + 0x74),
                                       (float *)0, 2, effect_def, pos, fwd,
                                       0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  }

  /* --- Block 6: Notify AI of detonation position. ---
   * FUN_000425c0(object_handle, position, effect_type, volume, count).
   * proj_tag+0x1f0 = AI sound volume category (short).
   * effect_type=2 (explosive), count=1. */
  ai_volume = *(short *)(proj_tag + 0x1f0);
  FUN_000425c0(projectile_handle, pos, (short)2, ai_volume, (short)1);
}

/* Initialise a newly-created projectile object.
 *
 * Called once per projectile spawn.  Sets up all per-object state that cannot
 * be baked into the tag definition:
 *
 *   - Sets the object active-flags bit (0x2000) and the fuze-type field
 *     (proj+0x1dc = 2).
 *   - Clears the target-object handle (proj+0x1e8 = -1) and the per-tick
 *     counter word (proj+0x1e0 = 0; proj+0x1e2 = 0xFFFF).
 *   - Resolves and stores the root-parent handle for the owner object
 *     (proj+0x1e4 = object_get_root_parent(proj[0x1d])).
 *   - Computes per-tick speed and range decay factors:
 *       proj+0x1f4 = 1.0f / (initial_speed * 30.0f)  if >= 1 tick worth
 *       proj+0x1fc = 1.0f / (max_range   * 30.0f)    if >= 1 tick worth
 *     (30.0f = TICKS_PER_SECOND at 0x253394; 1.0f = 0x2533c8).
 *   - Walks the tag's material-response block (tag+0x140, element size 0x48)
 *     and records the first index whose type-tag == 'cont' (0x636f6e74) at
 *     proj+0x1ec (-1 if none).
 *   - Applies one tick of velocity to position:
 *       proj+0x18..0x20 (pos.xyz) += tag[0x1e4] * proj+0x24..0x2c (vel.xyz)
 *   - Tests whether the new position is inside a valid world region via
 *     FUN_0018f3e0(&proj[0x48], &proj[0x50], NULL).  Sets or clears bit 4
 *     of the object flags word (proj+0x4) accordingly.
 *   - Calls FUN_000f8590 (velocity direction cache) and FUN_000f8640
 *     (detonation radius cache) with the projectile handle in EAX.
 *   - Sets flags bits 0xc0000 (active + detonating-armed) in proj+0x4.
 *   - Returns 1 (success).
 *
 * Layout notes (all relative to proj = object_get_and_verify_type(handle,
 * 0x20)): proj+0x04  object flags (uint32) proj+0x18  position.xyz (3 floats)
 *   proj+0x24  velocity.xyz / direction (3 floats, pre-normalised by spawner)
 *   proj+0x48  location struct (passed as arg1 to FUN_0018f3e0)
 *   proj+0x50  world position (passed as arg2 to FUN_0018f3e0)
 *   proj+0x74  parent object handle (int)
 *   proj+0x1dc fuze type (uint32; 2 = standard)
 *   proj+0x1e0 counter word (uint16)
 *   proj+0x1e2 counter high word (uint16; init 0xFFFF)
 *   proj+0x1e4 root-parent handle (int)
 *   proj+0x1e8 target-object handle (int; -1 = none)
 *   proj+0x1ec contact-material index (int; -1 = none)
 *   proj+0x1f4 speed decay factor (float)
 *   proj+0x1fc range decay factor (float)
 *
 * Disasm-verified: call at 0x000f8eaf passes handle in EAX (FUN_000f8590);
 * call at 0x000f8ebf passes handle in EAX (FUN_000f8640).
 * All cdecl stack args confirmed from PUSH/ADD-ESP pairs. */
int projectile_new(int projectile_handle)
{
  char *proj; /* projectile object base (type 0x20) */
  char *proj_tag; /* projectile tag data ('proj') */
  float initial_speed; /* tag+0x1bc / random range result */
  float speed_factor; /* initial_speed * 30.0f */
  float range_factor; /* tag+0x1a4 * 30.0f */
  int root_parent; /* result of object_get_root_parent */
  void *mat_block; /* material response block element pointer */
  int mat_count; /* number of material response entries */
  int mat_idx; /* loop index into material response block */
  int *seed; /* random seed pointer from get_global_random_seed_address */

  proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  proj_tag = (char *)tag_get(0x70726f6a, *(int *)proj);

  /* Set active flag bit, fuze type, and clear target/counter fields. */
  *(uint32_t *)(proj + 0x4) |= 0x2000u;
  *(uint32_t *)(proj + 0x1dc) = 2;
  *(int *)(proj + 0x1e8) = -1;
  *(uint16_t *)(proj + 0x1e0) = 0;
  *(uint16_t *)(proj + 0x1e2) = 0xFFFF;

  /* Resolve and store root-parent handle. */
  root_parent = object_get_root_parent(*(int *)(proj + 0x74));
  *(int *)(proj + 0x1e4) = root_parent;

  /* Compute per-tick speed decay factor from initial_speed tag field.
   * Tag+0x17c bit2: if set, initial_speed is a fixed float (tag+0x1bc).
   * Otherwise: random_real_range(seed, tag+0x1bc, tag+0x1c0). */
  if (*(uint8_t *)(proj_tag + 0x17c) & 4) {
    initial_speed = *(float *)(proj_tag + 0x1bc);
  } else {
    seed = get_global_random_seed_address();
    initial_speed = random_real_range(seed, *(float *)(proj_tag + 0x1bc),
                                      *(float *)(proj_tag + 0x1c0));
  }

  /* Store 1.0 / (initial_speed * 30.0) if speed exceeds one-tick threshold. */
  speed_factor = initial_speed * *(float *)0x253394;
  if (*(float *)0x2533c8 <= speed_factor) {
    *(float *)(proj + 0x1f4) = *(float *)0x2533c8 / speed_factor;
  }
  /* Compute per-tick range decay factor from max-range tag field (tag+0x1a4).
   */
  range_factor = *(float *)(proj_tag + 0x1a4) * *(float *)0x253394;
  if (*(float *)0x2533c8 <= range_factor) {
    *(float *)(proj + 0x1fc) = *(float *)0x2533c8 / range_factor;
  }

  /* Find first 'cont' (contact) material response entry in tag block.
   * tag+0x140 = tag_block_t: { int count; ... }; element size = 0x48.
   * Stores the matching index into proj+0x1ec, or -1 if none found. */
  *(int *)(proj + 0x1ec) = -1;
  mat_count = *(int *)(proj_tag + 0x140);
  mat_idx = 0;
  while (mat_idx < mat_count) {
    mat_block =
      tag_block_get_element((void *)(proj_tag + 0x140), mat_idx, 0x48);
    if (*(int *)mat_block == 0x636f6e74) {
      *(int *)(proj + 0x1ec) = mat_idx;
      break;
    }
    mat_idx++;
  }

  /* Apply one tick of initial velocity to position.
   * proj+0x18..0x20 = position.xyz
   * proj+0x24..0x2c = velocity direction (unit vector * initial_speed from
   * spawner) tag+0x1e4 = initial speed (used as the tick delta multiplier here)
   */
  {
    float tick_speed = *(float *)(proj_tag + 0x1e4);
    *(float *)(proj + 0x18) += tick_speed * *(float *)(proj + 0x24);
    *(float *)(proj + 0x1c) += tick_speed * *(float *)(proj + 0x28);
    *(float *)(proj + 0x20) += tick_speed * *(float *)(proj + 0x2c);
  }

  /* Test world-region validity.  FUN_0018f3e0 returns non-zero if the position
   * is inside a valid region.  Bit 4 of object flags = "region-valid" marker.
   */
  if (FUN_0018f3e0((void *)(proj + 0x48), (void *)(proj + 0x50), 0)) {
    *(uint32_t *)(proj + 0x4) |= 0x10u;
  } else {
    *(uint32_t *)(proj + 0x4) &= ~0x10u;
  }

  /* Update velocity direction cache and detonation radius cache.
   * Both functions take the handle in EAX (register-arg convention). */
  FUN_000f8590(projectile_handle);
  projectile_export_function_values(projectile_handle);
  FUN_000f8640(projectile_handle);

  /* Set active + detonating-armed flag bits. */
  *(uint32_t *)(proj + 0x4) |= 0xc0000u;

  return 1;
}

/*
 * Apply an acceleration vector to a projectile's translational velocity and
 * add a random scatter impulse.
 *
 * param_1: projectile datum handle
 * param_2: pointer to a 3-float acceleration vector (x, y, z)
 *
 * Only operates when the projectile has no parent object (proj+0xcc == -1).
 * When active:
 *   1. Validates param_2 (acceleration) and proj+0x18 (translational velocity)
 *      with assert_valid_real_vector2d checks.
 *   2. Adds the acceleration to the object's translational velocity at
 *      proj+0x18..0x20.
 *   3. Generates a random unit direction via random_seed_get_direction3d and
 *      scales it by magnitude(acceleration) * random_real * (PI/2).
 *   4. Adds the scaled random vector to the projectile impulse velocity at
 *      proj+0x3c..0x44.
 *   5. Rebuilds the velocity direction cache (FUN_000f8590).
 *   6. Clears bit 5 of the object flags word at proj+0x4.
 *
 * Disasm-verified: FUN_000f8590 called with projectile_handle in EAX.
 * Deferred stack cleanup: ADD ESP,0x10 at 0xf906b cleans 4 accumulated pushes
 * (random_seed_get_direction3d args + random_math_real arg +
 * real_vector3d_valid arg). The float constant at 0x2568bc = PI/2 (0x3FC90FDB).
 */
void projectile_accelerate(int projectile_handle, float *acceleration)
{
  char *proj; /* projectile object base (type 0x20) */
  char *vel; /* pointer to proj+0x18 (translational velocity xyz) */
  float *seed; /* engine-wide random seed pointer */
  float dir[3]; /* random unit direction from random_seed_get_direction3d */
  float sq_mag; /* squared magnitude of acceleration vector */
  float magnitude; /* magnitude of acceleration vector */
  float rand_real; /* random float in [0,1) from random_math_real */
  float scale; /* scatter scale: rand_real * magnitude * (PI/2) */

  proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  tag_get(0x70726f6a, *(int *)proj);

  if (!real_vector3d_valid(acceleration)) {
    csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
             "acceleration", (double)acceleration[0], (double)acceleration[1],
             (double)acceleration[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\items\\projectiles.c",
                   0x3ef, 1);
    system_exit(-1);
  }

  /* Only apply acceleration when the projectile has no parent. */
  if (*(int *)(proj + 0xcc) == -1) {
    vel = proj + 0x18;

    if (!real_vector3d_valid((float *)vel)) {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&projectile->object.translational_velocity",
               (double)*(float *)(proj + 0x18), (double)*(float *)(proj + 0x1c),
               (double)*(float *)(proj + 0x20));
      display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\items\\projectiles.c",
                     0x3f3, 1);
      system_exit(-1);
    }

    /* Add acceleration to object translational velocity (proj+0x18..0x20). */
    *(float *)(proj + 0x18) += acceleration[0];
    *(float *)(proj + 0x1c) += acceleration[1];
    *(float *)(proj + 0x20) += acceleration[2];

    /* Get a random direction vector into dir[3]. */
    seed = (float *)get_global_random_seed_address();
    random_seed_get_direction3d((unsigned int *)seed, dir);

    /* Compute squared magnitude, then scale = sqrt(sq_mag) * rand * PI/2. */
    sq_mag = acceleration[0] * acceleration[0] +
             acceleration[1] * acceleration[1] +
             acceleration[2] * acceleration[2];

    seed = (float *)get_global_random_seed_address();
    rand_real = random_math_real((unsigned int *)seed);
    magnitude = sqrtf(sq_mag);
    scale = rand_real * magnitude * *(float *)0x2568bc;

    /* Add random scatter to projectile impulse velocity (proj+0x3c..0x44). */
    *(float *)(proj + 0x3c) += dir[0] * scale;
    *(float *)(proj + 0x40) += dir[1] * scale;
    *(float *)(proj + 0x44) += dir[2] * scale;

    /* Rebuild velocity direction cache. */
    FUN_000f8590(projectile_handle);

    /* Clear object flag bit 5 ("motion-pending" or similar). */
    *(uint32_t *)(proj + 0x4) &= ~0x20u;

    if (!real_vector3d_valid((float *)vel)) {
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&projectile->object.translational_velocity",
               (double)*(float *)(proj + 0x18), (double)*(float *)(proj + 0x1c),
               (double)*(float *)(proj + 0x20));
      display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\items\\projectiles.c",
                     0x405, 1);
      system_exit(-1);
    }
  }
}

/*
 * FUN_000f90d0 - Projectile collision response handler.
 *
 * Called when a projectile has collided (FUN_000f9c40 calls this after
 * FUN_000f8720 reports a hit).  The function:
 *   1. Resolves the projectile object and its tag record.
 *   2. Copies the incoming velocity vector (in_velocity) to a local,
 *      normalises it, and replaces it with the zero vector if it is already
 *      zero (degenerate direction).
 *   3. Computes a 0-1 "detonation fraction" from the tag's range parameters
 *      (local_24, used later as an alpha/scale weight).
 *   4. If the collision state is 3 (world surface) and the tag has a valid
 *      detonation_causer, asserts the collision object index, builds damage
 *      params via damage_data_new, then applies area damage via
 * object_cause_damage.
 *   5. Selects the detonation result type (0-4) from a tag element block,
 *      using random tests for probability-gated outcomes.
 *   6. Handles a second pass for collision state 2 (shield/object bounce)
 *      with bit 0x8 set, applying breakable-surface damage via FUN_00146a90.
 *   7. Writes the collision surface normal/position back to hit_pos.
 *   8. Applies the detonation result:
 *        0 - attach / deactivate
 *        1 - attach at rest
 *        2 - deflect velocity via surface projection
 * (FUN_0010b910/0010c510/0010c8e0) 3 - ricochet: velocity reflected by (1 -
 * elasticity) * direction 4 - detonate: spawn effects, optionally attach to
 * target, set flags
 *   9. Emits hit effects (effect_new_attached_from_markers /
 * effect_new_unattached_from_markers) conditioned on the collision normal dot
 * product.
 *  10. For detonation result 4 and "attach on impact" tag flag, computes
 *      a spin-rate scale and stores it at proj+0x1f4.
 *
 * ABI:
 *   Prototype : void FUN_000f90d0(int projectile_handle, float *hit_pos,
 *                                 float param_3,
 *                                 float *velocity@<eax>,
 *                                 int16_t *col_result@<esi>)
 *   in_EAX    : velocity vector (float[3]); saved to EDI at entry.
 *   in_ESI    : collision result buffer (int16_t *); unaff_ESI in Ghidra.
 *   param_1   : projectile object handle.
 *   param_2   : hit world-position (float *); output written back.
 *   param_3   : time/distance scale passed from caller (float, EBP-0x18 in
 *               FUN_000f9c40); used as the collision normal speed component.
 *
 * Source line refs from assert strings:
 *   0x47c (line 1148) collision->object_index!=NONE
 *   0x5cf (line 1487) default detonation result
 */
void FUN_000f90d0(int projectile_handle, float *hit_pos, float param_3,
                  float *in_velocity /* @<eax> */,
                  int16_t *col_result /* @<esi> */)
{
  int *proj; /* object_get_and_verify_type(projectile_handle, 0x20) */
  char *proj_tag; /* tag_get('proj', proj[0]) - void * base address       */
  char *tag_elem; /* tag element for the current detonation result index  */
  char *dtag_elem; /* tag element for the secondary (bounce) pass          */

  /* Normalised incoming velocity / local working copy. */
  float dir_x, dir_y, dir_z;

  /* "Detonation fraction": how far along the projectile's range we are. */
  float det_frac;

  /* Per-tick velocity scale (from caller, stored in param_3). */
  float vel_scale;

  /* Detonation result: 0=none/attach, 1=attached_rest, 2=deflect,
   *                    3=ricochet, 4=detonate.                             */
  int16_t det_result; /* low 16 bits of local_1c / local_18 */

  /* Effect scale weights (alpha, intensity). */
  float scale_a, scale_b;

  /* Squared magnitude of velocity after response processing. */
  float vel_sq;

  /* local_4 / local_8: initially the tag element index (int16_t range), later
   * the 32-bit effect-tag handle passed to effect_new_attached_from_markers /
   * effect_new_unattached_from_markers. The binary uses the same 4-byte stack
   * slot (raw int copy in MOV, not FPU).
   */
  int tag_idx;

  /* Miscellaneous temporaries. */
  float ftemp;
  float ang_dot; /* dot product of velocity with surface normal       */
  float deflect_dot; /* normal-speed component for deflect case           */
  float ang_speed; /* angular speed for deflect case                    */
  int obj_handle;
  char *pTemp; /* used for object_get_and_verify_type returns (void*) */
  int iTemp;
  uint32_t uTemp;
  short sTemp;
  float *seed;

  /* Damage params buffer (0xac bytes; see damage_data_new). */
  char damage_params[0xac];

  /* Marker/position arrays for effect_new_attached_from_markers /
   * effect_new_unattached_from_markers. MSVC stack overlap: marker_count=5 but
   * only marker_forwards[0] is explicitly set.  In the MSVC layout the next 4
   * float[3] locals are contiguous and read as forwards[1..4] by the effects
   * system: [0] "normal"            — collision surface normal [1] "incident"
   * — scaled normalised velocity [2] "negative incident" — normalised velocity
   *   [3] "reflection"        — velocity x normal (cross product)
   *   [4] "gravity"           — global up vector (*0x31fc50)  */
  float marker_points[5 * 3];
  float marker_forwards[5 * 3];
  /* Surface-decompose buffers for result type 2 (deflect). */
  float proj_component[3];
  float perp_component[3];

  /* Copy of col_result position at [ESI+0xc..0x10]. */
  float col_pos[3];
  float col_pos2[3];

  /* Velocity copy passed to normalize / normalised direction. */
  float vel_local[3];

  /* ------------------------------------------------------------------ */
  /* 1. Resolve object and tag.                                          */
  /* ------------------------------------------------------------------ */
  proj = (int *)object_get_and_verify_type(projectile_handle, 0x20);
  proj_tag = (char *)tag_get(0x70726f6a /* 'proj' */, proj[0]);

  /* 2. Copy velocity to local and normalise.                            */
  dir_x = in_velocity[0];
  dir_y = in_velocity[1];
  dir_z = in_velocity[2];

  /* local_8 = word [ESI+0x34] (current detonation-result index).       */
  tag_idx = (int)(int16_t)(col_result[0x1a]);

  vel_scale = param_3;

  /* 000f9112..000f912f: normalise vel_local; FPU result (magnitude) is
   * compared to 0 and also reused for the detonation-fraction calculation.
   * Binary calls normalize3d exactly once; the ST(0) result is compared
   * against [0x2533c0] (0.0f) and then reused in the fraction calculation. */
  vel_local[0] = dir_x;
  vel_local[1] = dir_y;
  vel_local[2] = dir_z;
  ftemp = normalize3d(vel_local); /* single call; magnitude in ftemp */

  /* If velocity is zero, replace direction with the global up vector. */
  if (ftemp == *(float *)0x2533c0) {
    dir_x = *(float *)(*(int *)0x31fc44);
    dir_y = *(float *)(*(int *)0x31fc44 + 4);
    dir_z = *(float *)(*(int *)0x31fc44 + 8);
  }

  /* 3. Compute detonation fraction (det_frac = local_24).              */
  /* Range: [proj_tag+0x1e8] = near_time, [proj_tag+0x1e4] = far_time.  */
  if (*(float *)(proj_tag + 0x1e8) == *(float *)(proj_tag + 0x1e4)) {
    det_frac = 1.0f;
  } else {
    /* ftemp = magnitude from normalize3d; reused from FPU at 0xf9156. */
    det_frac = (ftemp - *(float *)(proj_tag + 0x1e8)) /
               (*(float *)(proj_tag + 0x1e4) - *(float *)(proj_tag + 0x1e8));
    if (det_frac < *(float *)0x2533c0) {
      det_frac = 0.0f;
    } else if (det_frac > *(float *)0x2533c8) {
      det_frac = 1.0f;
    }
  }

  /* 4. Initial det_result = -1 (tag index word stored in low 16).      */
  det_result = -1;

  /* ------------------------------------------------------------------ */
  /* Collision state == 3 (world surface) + detonation causer present.  */
  /* ------------------------------------------------------------------ */
  if (col_result[0] == 3 && *(int *)(proj_tag + 0x230) != -1) {
    /* assert collision->object_index != NONE */
    if (*(int *)((char *)col_result + 0x38) == -1) {
      display_assert("collision->object_index!=NONE",
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x47c, 1);
      system_exit(-1);
    }
    damage_data_new(damage_params, *(int *)(proj_tag + 0x230));
    *(uint32_t *)(damage_params + 4) |= 8u;
    *(int *)(damage_params + 0x08) = *(int *)((char *)proj + 0x70);
    *(int *)(damage_params + 0x0c) = *(int *)((char *)proj + 0x74);
    *(short *)(damage_params + 0x10) = *(short *)((char *)proj + 0x68);
    *(float *)(damage_params + 0x40) = det_frac;
    /* Copy marker positions from col_result. */
    col_pos[0] = *(float *)((char *)col_result + 0x18);
    col_pos[1] = *(float *)((char *)col_result + 0x1c);
    col_pos[2] = *(float *)((char *)col_result + 0x20); /* buf-alias-ok */
    col_pos2[0] = *(float *)((char *)col_result + 0x18);
    col_pos2[1] = *(float *)((char *)col_result + 0x1c);
    col_pos2[2] = *(float *)((char *)col_result + 0x20); /* buf-alias-ok */
    /* vel_local = in_velocity copy, then normalize. */
    vel_local[0] = in_velocity[0];
    vel_local[1] = in_velocity[1];
    vel_local[2] = in_velocity[2];
    normalize3d(vel_local);
    /* Call area damage. */
    /* object_cause_damage: last arg is the direction pointer (ESI+0x24) cast to
     * uint.
     */
    object_cause_damage(damage_params, *(int *)((char *)col_result + 0x38),
                        (short)col_result[0x1f], (short)col_result[0x1e],
                        (short)col_result[0x27],
                        (unsigned int)(uintptr_t)((char *)col_result + 0x24));
    /* Update tag_idx from area-damage result material type.
     * object_cause_damage writes the resolved material type to
     * damage_params+0x4C and velocity scale to damage_params+0x48.  Original
     * binary reads from [EBP-0x40] and [EBP-0x44] which are
     * damage_params+0x4C/0x48 (damage_params base = EBP-0x8C). */
    if ((short)*(int16_t *)(damage_params + 0x4c) != -1) {
      tag_idx = (int)(short)*(int16_t *)(damage_params + 0x4c);
    }
    vel_scale = *(float *)(damage_params + 0x48);
  }

  /* Write det_result index to projectile obj+0x1e2 (word).             */
  sTemp = (int16_t)tag_idx;
  *(int16_t *)((char *)proj + 0x1e2) = sTemp;

  /* ------------------------------------------------------------------ */
  /* 5. Select tag element (detonation result block).                    */
  /* ------------------------------------------------------------------ */
  if (sTemp < 0 || *(int *)(proj_tag + 0x240) <= (int)sTemp) {
    tag_elem = (char *)0x31ed08; /* sentinel / null record */
  } else {
    tag_elem = (char *)tag_block_get_element((void *)(proj_tag + 0x240),
                                             (int)sTemp, 0xa0);
  }

  /* Compute angular-speed randomness. */
  /* local_10 = -[tag_elem+0x64]; call get_global_random_seed /
   * random_real_range */
  ftemp = -*(float *)((char *)tag_elem + 0x64);
  seed = (float *)get_global_random_seed_address();
  ang_speed =
    random_real_range((int *)seed, ftemp, 0.0f); /* note: 2nd arg pushed 0x0 */
  /* local_38: surface-normal dot with incoming velocity (projection). */
  ang_dot =
    ang_speed -
    *(float *)((char *)col_result + 0x2c) * in_velocity[2] - /* buf-alias-ok */
    *(float *)((char *)col_result + 0x28) * in_velocity[1] -
    *(float *)((char *)col_result + 0x24) * in_velocity[0];

  /* local_10 = [tag_elem+0x60]; compute angular displacement. */
  ftemp = *(float *)((char *)tag_elem + 0x60);
  seed = (float *)get_global_random_seed_address();
  deflect_dot = random_real_range((int *)seed, -ftemp, ftemp);

  {
    float ang_offset;
    /* FUN_0010c510(&in_velocity, col_result+0x24) -> angle between vel and
     * normal. */
    ang_offset =
      FUN_0010c510(in_velocity, (float *)((char *)col_result + 0x24));
    deflect_dot = ang_offset - *(float *)0x2568bc + deflect_dot;
  }

  /* ------------------------------------------------------------------ */
  /* Probability check for alternate detonation result.                  */
  /* ------------------------------------------------------------------ */
  det_result = (int16_t) * (int16_t *)((char *)tag_elem + 0x2);
  {
    int use_alt;
    use_alt = 0;
    if (*(int16_t *)((char *)tag_elem + 0x24) == 0) {
      use_alt = 1;
    } else {
      /* Check angular-displacement ranges. */
      if (*(float *)((char *)tag_elem + 0x30) != *(float *)0x2533c0) {
        if (deflect_dot < *(float *)((char *)tag_elem + 0x2c) ||
            (deflect_dot < *(float *)((char *)tag_elem + 0x30)) ==
              (deflect_dot ==
               *(float *)((char *)tag_elem + 0x30))) { /* buf-alias-ok */
          use_alt = 1;
        }
      }
      if (!use_alt &&
          *(float *)((char *)tag_elem + 0x38) != *(float *)0x2533c0) {
        if (ang_dot < *(float *)((char *)tag_elem + 0x34) ||
            (ang_dot < *(float *)((char *)tag_elem + 0x38)) ==
              (ang_dot ==
               *(float *)((char *)tag_elem + 0x38))) { /* buf-alias-ok */
          use_alt = 1;
        }
      }
      if (!use_alt && (*(char *)((char *)tag_elem + 0x26) & 1)) {
        if (col_result[0] != 3) {
          use_alt = 1;
        } else {
          iTemp = (object_try_and_get_and_verify_type(
                     *(int *)((char *)col_result + 0x38), 3) != 0) ?
                    1 :
                    0;
          if (iTemp == 0) {
            use_alt = 1;
          }
        }
      }
    }
    if (!use_alt) {
      /* Random probability gate. */
      seed = (float *)get_global_random_seed_address();
      ftemp = random_math_real((unsigned int *)seed);
      if (ftemp >= *(float *)((char *)tag_elem + 0x28)) { /* buf-alias-ok */
        det_result =
          (int16_t) * (int16_t *)((char *)tag_elem + 0x24); /* buf-alias-ok */
        /* raw int copy: effect tag handle from alt result */
        tag_idx = *(int *)((char *)tag_elem + 0x48); /* buf-alias-ok */
      }
    } else {
      /* raw int copy: effect tag handle from default result */
      tag_idx = *(int *)((char *)tag_elem + 0x10);
    }
  }

  /* ------------------------------------------------------------------ */
  /* 6. Collision state == 2 with bit 0x8: breakable surface.           */
  /* ------------------------------------------------------------------ */
  if (col_result[0] == 2 &&
      (*(uint8_t *)((char *)col_result + 0x4c) & 0x8)) { /* buf-alias-ok */
    damage_data_new(damage_params, *(int *)(proj_tag + 0x230));
    /* MSVC stack overlap: in the original binary col_pos/col_pos2/vel_local
     * overlap the damage_params buffer. Write directly into damage_params. */
    *(uint32_t *)(damage_params + 0x04) |= 8;
    *(float *)(damage_params + 0x1c) = *(float *)((char *)col_result + 0x18);
    *(float *)(damage_params + 0x20) = *(float *)((char *)col_result + 0x1c);
    *(float *)(damage_params + 0x24) =
      *(float *)((char *)col_result + 0x20); /* buf-alias-ok */
    *(float *)(damage_params + 0x28) = *(float *)((char *)col_result + 0x18);
    *(float *)(damage_params + 0x2c) = *(float *)((char *)col_result + 0x1c);
    *(float *)(damage_params + 0x30) =
      *(float *)((char *)col_result + 0x20); /* buf-alias-ok */
    *(float *)(damage_params + 0x34) = in_velocity[0];
    *(float *)(damage_params + 0x38) = in_velocity[1];
    *(float *)(damage_params + 0x3c) = in_velocity[2];
    normalize3d((float *)(damage_params + 0x34));
    /* Resolve bounce pass tag element (result unused; matches original code).
     */
    sTemp = col_result[0x1a];
    if (sTemp < 0 || *(int *)(proj_tag + 0x240) <= (int)sTemp) {
      dtag_elem = (char *)0x31ed08;
    } else {
      dtag_elem = (char *)tag_block_get_element((void *)(proj_tag + 0x240),
                                                (int)sTemp, 0xa0);
    }
    (void)dtag_elem;
    /* Material type at offset 0x4C — required by FUN_00146a90's early-out check
     */
    *(int16_t *)(damage_params + 0x4c) = (int16_t)sTemp;
    /* Cluster/leaf from collision result */
    *(int *)(damage_params + 0x14) = *(int *)((char *)col_result + 0x0c);
    *(int *)(damage_params + 0x18) = *(int *)((char *)col_result + 0x10);
    FUN_00146a90((int)(uint32_t)(*(uint8_t *)((char *)col_result + 0x4d)),
                 damage_params, *(int *)((char *)col_result + 0x44));
  }

  /* ------------------------------------------------------------------ */
  /* 7. Write collision position back to hit_pos (param_2).             */
  /* ------------------------------------------------------------------ */
  hit_pos[0] = *(float *)((char *)col_result + 0x18);
  hit_pos[1] = *(float *)((char *)col_result + 0x1c);
  hit_pos[2] = *(float *)((char *)col_result + 0x20); /* buf-alias-ok */

  /* ------------------------------------------------------------------ */
  /* 8. Apply detonation result.                                         */
  /* ------------------------------------------------------------------ */
  if (det_result == 3) {
    /* Ricochet. */
    if (col_result[0] == 0) {
      /* Toggle bit 0x10 in proj[1]. */
      uTemp = (uint32_t)proj[1];
      if (uTemp & 0x10) {
        uTemp &= ~0x10u;
      } else {
        uTemp |= 0x10u;
      }
      proj[1] = (int)uTemp;
      FUN_000f8640(projectile_handle);
      /* Subtract normal-component contribution from hit_pos. */
      hit_pos[0] -= *(float *)((char *)col_result + 0x24) * /* buf-alias-ok */
                    *(float *)0x255ef8;
      hit_pos[1] -= *(float *)((char *)col_result + 0x28) * /* buf-alias-ok */
                    *(float *)0x255ef8;
      hit_pos[2] -= *(float *)((char *)col_result + 0x2c) * /* buf-alias-ok */
                    *(float *)0x255ef8;
    } else if (col_result[0] == 3) {
      ftemp = *(float *)0x2533c8 - *(float *)((char *)tag_elem + 0x90);
      in_velocity[0] *= ftemp;
      in_velocity[1] *= ftemp;
      in_velocity[2] *= ftemp;
      proj[0x79] = *(int *)((char *)col_result + 0x38); /* buf-alias-ok */
    } else {
      /* Not state 0 or 3 — check tag's max speed. */
      if (*(float *)(proj_tag + 0x1c0) == *(float *)0x2533c0) {
        det_result = 1;
      } else {
        proj[0x77] = proj[0x77] | 0x14;
        det_result = 4;
      }
      goto apply_default_velocity;
    }
  } else if (det_result == 2) {
    /* Deflect: project velocity onto/off surface normal. */
    /* arg3 = perp (EBP+0xffffff58), arg4 = proj (EBP+0xffffff64).
     * Decompiler note: Ghidra swaps proj/perp labels; the disassembly at
     * 000f9726 uses [EBP+0xffffff64] as one component and 0xffffff58 as
     * the other; verified from FUN_0010b910 body: EBP+0x10 = proj_out,
     * EBP+0x14 = perp_out, so arg order is (v, n, perp_out, proj_out). */
    FUN_0010b910(in_velocity, (float *)((char *)col_result + 0x24),
                 perp_component, proj_component);
    /* new_vel = (1 - tag[0x9c]) * proj - (1 - tag[0x98]) * perp
     * (0x9726: FMUL proj[0xffffff64], 0x9738: FMUL perp[0xffffff58]) */
    in_velocity[0] =
      (1.0f - *(float *)((char *)tag_elem + 0x9c)) * proj_component[0] -
      (1.0f - *(float *)((char *)tag_elem + 0x98)) * perp_component[0];
    in_velocity[1] =
      (1.0f - *(float *)((char *)tag_elem + 0x9c)) * proj_component[1] -
      (1.0f - *(float *)((char *)tag_elem + 0x98)) * perp_component[1];
    in_velocity[2] =
      (1.0f - *(float *)((char *)tag_elem + 0x9c)) * proj_component[2] -
      (1.0f - *(float *)((char *)tag_elem + 0x98)) * perp_component[2];
    goto apply_speed_scale;
  } else {
  apply_default_velocity:
    /* Use global forward direction. */
    in_velocity[0] = *(float *)(*(int *)0x31fc38);
    in_velocity[1] = *(float *)(*(int *)0x31fc38 + 4);
    in_velocity[2] = *(float *)(*(int *)0x31fc38 + 8);
  }

apply_speed_scale:
  /* Optional: random speed scale in [tag+0x60]. */
  if (*(float *)((char *)tag_elem + 0x60) != *(float *)0x2533c0) {
    seed = (float *)get_global_random_seed_address();
    random_direction3d(
      (int *)seed, in_velocity,
      0.0f, /* dup-args-ok: in-place, verified PUSH EDI x2 at 0x000f95f3/59 */
      *(float *)((char *)tag_elem + 0x60), in_velocity);
  }

  /* Optional: speed magnitude randomisation [tag+0x64]. */
  if (*(float *)((char *)tag_elem + 0x64) != *(float *)0x2533c0) {
    float lo, hi; /* C89: hoist from inner block */
    ftemp = normalize3d(in_velocity);
    if (ftemp != *(float *)0x2533c0) {
      lo = -*(float *)((char *)tag_elem + 0x64);
      hi = *(float *)((char *)tag_elem + 0x64);
      seed = (float *)get_global_random_seed_address();
      ftemp += random_real_range((int *)seed, lo, hi);
      in_velocity[0] = ftemp * in_velocity[0];
      in_velocity[1] = ftemp * in_velocity[1];
      in_velocity[2] = ftemp * in_velocity[2];
    }
  }

  /* Compute squared speed. */
  vel_sq = in_velocity[0] * in_velocity[0] + in_velocity[1] * in_velocity[1] +
           in_velocity[2] * in_velocity[2];

  /* Clamp-to-minimum-speed check. */
  if (det_result != 4) {
    if (vel_sq < *(float *)(proj_tag + 0x1c4) * *(float *)(proj_tag + 0x1c4)) {
      pTemp = (char *)object_get_and_verify_type(projectile_handle, 0x20);
      if (*(int16_t *)(pTemp + 0x1e0) < 1) {
        *(int16_t *)(pTemp + 0x1e0) = 1;
      }
    }
  }

  /* Gravity / vertical velocity threshold check. */
  if (vel_sq < *(float *)0x253f44) {
    proj[0x77] = proj[0x77] | 0x10;
    if (*(float *)((char *)col_result + 0x2c) > *(float *)0x2533e4) {
      proj[1] = proj[1] | 0x20;
      if (*(float *)(proj_tag + 0x1c0) == *(float *)0x2533c0) {
        pTemp = (char *)object_get_and_verify_type(projectile_handle, 0x20);
        if (*(int16_t *)(pTemp + 0x1e0) < 1) {
          *(int16_t *)(pTemp + 0x1e0) = 1;
        }
      }
    }
  }

  /* ------------------------------------------------------------------ */
  /* Detonation result scale (scale_a / local_20).                       */
  /* ------------------------------------------------------------------ */
  scale_a = 0.0f;
  {
    int16_t scale_mode =
      *(int16_t *)((char *)tag_elem + 0x5c); /* buf-alias-ok */
    if (scale_mode == 0) {
      scale_a = det_frac;
    } else if (scale_mode == 1) {
      scale_a = deflect_dot * *(float *)0x28ac24;
      if (scale_a < *(float *)0x2533c0) {
        scale_a = 0.0f;
      } else if (scale_a > *(float *)0x2533c8) {
        scale_a = 1.0f;
      }
    }
    /* else: scale_a stays 0.0 (from LAB_000f9862 fallthrough) */
  }

  /* Clamp scale_a. */
  if (scale_a < *(float *)0x2533c0) {
    scale_a = 0.0f;
  } else if (scale_a > *(float *)0x2533c8) {
    scale_a = 1.0f;
  }

  /* scale_b (local_18 / local_14) — intensity weight. */
  scale_b = vel_scale;
  if (scale_b < *(float *)0x2533c0) {
    scale_b = 0.0f;
  } else if (scale_b > *(float *)0x2533c8) {
    scale_b = 1.0f;
  }

  /* ------------------------------------------------------------------ */
  /* Build marker arrays for effect functions.                            */
  /* ------------------------------------------------------------------ */
  {
    int k;
    float *mp;
    {
      float scale_f = *(float *)0x255e94;
      float *up_ptr = *(float **)0x31fc50;

      /* [0] "normal" — collision surface normal */
      marker_forwards[0] =
        *(float *)((char *)col_result + 0x24); /* buf-alias-ok */
      marker_forwards[1] =
        *(float *)((char *)col_result + 0x28); /* buf-alias-ok */
      marker_forwards[2] =
        *(float *)((char *)col_result + 0x2c); /* buf-alias-ok */

      /* [1] "incident" — scaled normalised velocity */
      marker_forwards[3] = vel_local[0] * scale_f;
      marker_forwards[4] = vel_local[1] * scale_f;
      marker_forwards[5] = vel_local[2] * scale_f;

      /* [2] "negative incident" — normalised velocity */
      marker_forwards[6] = vel_local[0];
      marker_forwards[7] = vel_local[1];
      marker_forwards[8] = vel_local[2];

      /* [3] "reflection" — cross product of velocity and surface normal */
      FUN_0010c8e0(vel_local, (float *)((char *)col_result + 0x24),
                   marker_forwards + 9);

      /* [4] "gravity" — global up vector */
      marker_forwards[12] = up_ptr[0];
      marker_forwards[13] = up_ptr[1];
      marker_forwards[14] = up_ptr[2];
    }

    /* Fill 5 marker_points entries with col_result position. */
    mp = marker_points;
    for (k = 0; k < 5; k++) {
      mp[0] = *(float *)((char *)col_result + 0x18);
      mp[1] = *(float *)((char *)col_result + 0x1c);
      mp[2] = *(float *)((char *)col_result + 0x20); /* buf-alias-ok */
      mp += 3;
    }
  }

  /* ------------------------------------------------------------------ */
  /* 9. Emit hit effect (if ang_dot > threshold).                        */
  /* ------------------------------------------------------------------ */
  if (ang_dot > *(float *)0x28ac20) {
    if (col_result[0] == 3) {
      /* effect_update(tag, proj_handle, attached_obj, node_idx, 5, def, pts,
       * fwds, sA, sB, 0, 0) */
      effect_new_attached_from_markers(
        tag_idx, projectile_handle, *(int *)((char *)col_result + 0x38),
        (uint16_t)col_result[0x1f], 5, (void *)0x31f3a0, marker_points,
        marker_forwards, scale_a, scale_b, 0.0f, 0.0f);
    } else {
      effect_new_unattached_from_markers(
        tag_idx, projectile_handle, 0, 5, (void *)0x31f3a0, marker_points,
        marker_forwards, scale_a, scale_b, 0.0f, 0.0f, 1);
    }
  }

  /* ------------------------------------------------------------------ */
  /* Secondary effect: destroy/detonate tag effect.                      */
  /* ------------------------------------------------------------------ */
  if (!(proj[0x77] & 0x20) && ((proj[0x77] & 0x10) || det_result == 4)) {
    if (col_result[0] == 3) {
      effect_new_attached_from_markers(
        *(int *)(proj_tag + 0x200), projectile_handle,
        *(int *)((char *)col_result + 0x38), (uint16_t)col_result[0x1f], 5,
        (void *)0x31f3a0, marker_points, marker_forwards, scale_a, scale_b,
        0.0f, 0.0f);
    } else {
      effect_new_unattached_from_markers(
        *(int *)(proj_tag + 0x200), projectile_handle, 0, 5, (void *)0x31f3a0,
        marker_points, marker_forwards, scale_a, scale_b, 0.0f, 0.0f, 1);
    }
  }

  /* ------------------------------------------------------------------ */
  /* 10. Final detonation result dispatch.                               */
  /* ------------------------------------------------------------------ */
  switch (det_result) {
  case 0:
    pTemp = (char *)object_get_and_verify_type(projectile_handle, 0x20);
    if (*(int16_t *)(pTemp + 0x1e0) < 2) {
      *(int16_t *)(pTemp + 0x1e0) = 2;
      return;
    }
    break;

  case 1:
    pTemp = (char *)object_get_and_verify_type(projectile_handle, 0x20);
    if (*(int16_t *)(pTemp + 0x1e0) < 1) {
      *(int16_t *)(pTemp + 0x1e0) = 1;
      return;
    }
    break;

  case 2:
  case 3:
    break;

  case 4: {
    /* Detonate. */
    if (col_result[0] == 3 && (*(uint8_t *)(proj_tag + 0x17c) & 0x8)) {
      /* Kill all projectiles of same tag attached to this object. */
      int *parent_obj;
      int *child_obj;
      char *child_proj_p;
      parent_obj = (int *)object_get_and_verify_type(
        *(int *)((char *)col_result + 0x38), -1);
      obj_handle = parent_obj[0x32]; /* first child at proj+0xc8 */
      tag_idx = 0;
      while (obj_handle != -1) {
        child_obj = (int *)object_get_and_verify_type(obj_handle, -1);
        if (child_obj[0] == proj[0] /* same tag */) {
          child_proj_p = (char *)object_get_and_verify_type(obj_handle, 0x20);
          if (!(*(uint8_t *)(child_proj_p + 0x1dc) & 0x40)) {
            char *cp2;
            cp2 = (char *)object_get_and_verify_type(obj_handle, 0x20);
            *(int *)(cp2 + 0x1f8) = 0;
            *(int *)(cp2 + 0x1f0) = 0;
            tag_idx = tag_idx + 1;
          }
        }
        if ((int16_t)tag_idx > 5) {
          proj[0x77] = proj[0x77] | 0x80;
          break;
        }
        obj_handle = child_obj[0x31];
      }
    }
    /* Reset object forward/up to global zero vector. */
    {
      int *zv = (int *)(*(int *)0x31fc38);
      proj[6] = zv[0];
      proj[7] = zv[1];
      proj[8] = zv[2];
      proj[0xf] = zv[0];
      proj[0x10] = zv[1];
      proj[0x11] = zv[2];
    }
    proj[1] = proj[1] | 0x20;
    proj[0x77] = proj[0x77] | 0x8;
    object_translate(projectile_handle, hit_pos,
                     (float *)((char *)col_result + 0xc));
    if (col_result[0] == 3) {
      object_attach_to_parent(*(int *)((char *)col_result + 0x38),
                              projectile_handle, (int16_t)col_result[0x1f]);
    }
    /* Compute spin rate scale if tag "attach on impact" flag (bit 2) set. */
    if ((*(uint8_t *)(proj_tag + 0x17c) & 0x4)) {
      ftemp = *(float *)(proj_tag + 0x1c0) * *(float *)0x253394;
      if (ftemp >= *(float *)0x2533c8) {
        *(float *)((char *)proj + 0x1f4) = *(float *)0x2533c8 / ftemp;
      }
    }
    break;
  } /* case 4 */

  default:
    display_assert(0, "c:\\halo\\SOURCE\\items\\projectiles.c", 0x5cf, 1);
    system_exit(-1);
    return;
  }
}

/*
 * Main projectile physics update tick (FUN_000f9c40).
 *
 * Processes one game-tick worth of physics for a single projectile:
 *   1. Contrail cleanup: if the projectile is not already detonating and has
 *      an active contrail sub-object, deletes it and clears the reference.
 *   2. Time/distance accumulation: advances the per-tick timer fields.
 *   3. Detonation flag: sets the detonating flag (bit 5) when the detonation
 *      timer (_DAT_002533c8) has elapsed, and forces the detonation state to
 *      at least 1 when the max-range field (_DAT_002533c8) is exceeded.
 *   4. Exports function values, then enters the main physics do-while loop:
 *      a. Copies current velocity/speed; validates the velocity vector.
 *      b. Guided-missile steering: if a target is locked, rotates the
 *         velocity vector toward the target using sin/cos.
 *      c. Speed clamping / deceleration based on max-range remaining.
 *      d. Gravity integration.
 *      e. Range limit (local_64 fraction) that proportionally clamps
 *         displacement and flags detonation.
 *      f. Computes new_pos; validates it.
 *      g. Collision depth push/pop around FUN_000f8720.
 *      h. Bounce limit (max 10) or detonation-state check -> time=0.
 *      i. On collision hit: adjusts velocity for bounce, calls FUN_000f90d0
 *         for detonation effects and FUN_000425c0 for sound, increments
 *         bounce counter.
 *      j. Accumulates total distance; proximity-checks up to 4 local players
 *         for 3D sound trigger (unattached_impulse_sound_new).
 *      k. Updates orientation (forward/up) from velocity direction.
 *      l. Translates object to new_pos; writes back velocity.
 *      m. If bounced: updates contrail node-matrices and contrail state.
 *   5. Post-loop detonation handling: FUN_000f8920 (velocity/impact) or
 *      object_delete.
 *   6. Validates axes; exits profiling section.
 */
int FUN_000f9c40(int projectile_handle)
{
  char *proj; /* object data for the projectile                     */
  char *proj_tag; /* tag data ('proj') for the projectile               */
  int contrail_handle; /* handle of the attached contrail sub-object         */
  int time_tick; /* game_time_get() result for seeding randomness      */
  int player_idx; /* loop var: local player index (0-3)                 */
  int player_handle; /* result of local_player_get_player_index            */
  int player_obj; /* pointer to player object data                      */
  int obj_handle; /* misc object handle from datum_get result           */
  float dot; /* dot product for sound proximity check              */
  float mag_sq; /* magnitude-squared for proximity                    */
  float sound_range; /* sound trigger range from tag                       */
  /* Velocity components (float[3] at proj+0x18). */
  float vel_x, vel_y, vel_z;

  /* Average velocity for position integration (float[3]). */
  float avg_vel_x, avg_vel_y, avg_vel_z;

  /* New position after one tick (float[3]). */
  float new_pos_x, new_pos_y, new_pos_z;

  /* Guidance target position. */
  float target_pos_x, target_pos_y, target_pos_z;

  /* Local copies for perpendicular decomposition. */
  float perp_vec[3]; /* perpendicular component (local_bc area)           */
  float angles_out[3]; /* angles_to_vector output area (local_9c area)      */
  float steer_delta[3]; /* delta toward target (local_88 area)               */

  /* Forward / up vector copies for the orientation update. */
  float fwd_copy[3]; /* copy of proj->forward (local_7c area)             */

  /* Intermediate buffers for FUN_000178d0 / cross_product3d. */
  float cross_buf[3]; /* result of cross_product3d (local_10c area)        */
  float cross_buf2[3]; /* second cross buffer (local_f4 area = location)    */

  /* Distance/range helpers. */
  float speed; /* |velocity| at start of tick (local_3c)            */
  float speed_prev; /* copy of speed before deceleration (local_20)      */
  float range_frac; /* fraction of tick before max-range hit (local_64)  */
  float dist_at_hit; /* deceleration-adjusted distance (local_30)        */
  float dist_post; /* post-decel speed (local_3c updated)               */

  /* Gravity / deceleration temps. */
  float gravity; /* local_8 — gravity deceleration magnitude          */
  float decel_frac; /* fVar2 — generic float scratch                     */
  float tmp_frac; /* fVar5/fVar6 — secondary scratch                   */

  /* Steering helpers. */
  float target_dist; /* distance to guidance target (local_8 scratch)     */
  float steer_frac; /* lateral blend weight (local_2c)                   */

  /* Bounce count, time remaining, found-sound flag, hit flag. */
  int bounce_count; /* local_38: number of bounces so far in this tick   */
  float time_remaining; /* local_1c: fraction of tick remaining (starts 1.0) */
  char found_sound; /* local_21: set once a proximity sound is triggered  */
  char hit_flag; /* local_15: set when FUN_000f8720 reports a hit      */
  char col_hit; /* local_15 updated after detonation check            */

  /* Saved object target index (proj+0x1e4). */
  int saved_target; /* local_90: *(int*)(proj+0x1e4) at tick start       */

  /* Steering turn rate. */
  float steer_turn_rate; /* local_34: steering turn rate (radians/tick)     */

  /* Collision result buffer (at least 0x3c bytes per FUN_000f90d0 usage). */
  int16_t collision_result[0x1e]; /* 0x3c bytes                              */

  /* Collision normal z component for bounce-angle check. */
  float col_normal_z; /* local_13c: collision normal z component            */

  /* Temp for object-data pointer (object_get_and_verify_type returns int). */
  int obj_type_f; /* pointer to target object data (used as int for field access) */

  /* Location passed to unattached_impulse_sound_new reuses cross_buf2.       */

  /* Misc integer temps. */
  int cd_slot; /* collision-depth slot index                        */
  int tmp_int; /* miscellaneous integer scratch                     */

  /* ----------------------------------------------------------------------- */
  /* 1. Resolve object and tag data.                                         */
  /* ----------------------------------------------------------------------- */
  proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  proj_tag = (char *)tag_get(0x70726f6a, *(int *)proj);

  time_remaining = 1.0f;
  bounce_count = 0;
  found_sound = '\0';

  /* Cache proj_tag in local_28 equivalent. */

  /* ----------------------------------------------------------------------- */
  /* 2. Profiling entry.                                                     */
  /* ----------------------------------------------------------------------- */
  if ((*(char *)0x449ef1 != '\0') && (*(char *)0x31edb0 != '\0')) {
    profile_enter_private(*(void **)0x31eda8);
  }

  /* ----------------------------------------------------------------------- */
  /* 3. Contrail cleanup.                                                    */
  /* ----------------------------------------------------------------------- */
  if (((*(uint8_t *)(proj + 0x1dc) & 2) == 0) &&
      (*(int *)(proj + 0x1ec) != -1)) {
    contrail_handle = *(int *)(proj + 0xfc + *(int *)(proj + 0x1ec) * 4);
    if (contrail_handle != -1) {
      contrail_delete(contrail_handle);
    }
    *(int *)(proj + 0xfc + *(int *)(proj + 0x1ec) * 4) = -1;
    *(int *)(proj + 0x1ec) = -1;
  }

  /* ----------------------------------------------------------------------- */
  /* 4. Time/distance accumulation.                                          */
  /* ----------------------------------------------------------------------- */
  *(float *)(proj + 0x1f8) =
    *(float *)(proj + 0x1fc) + *(float *)(proj + 0x1f8);
  *(float *)(proj + 0x204) =
    *(float *)(proj + 0x208) + *(float *)(proj + 0x204);

  /* ----------------------------------------------------------------------- */
  /* 5. Detonation flag check.                                               */
  /* ----------------------------------------------------------------------- */
  {
    uint8_t det_type;
    det_type = 0;
    if ((*(int16_t *)(proj_tag + 0x180) == 1) ||
        (*(int16_t *)(proj_tag + 0x180) == 2)) {
      det_type = (uint8_t)((*(uint32_t *)(proj + 0x1dc) >> 4) & 1);
    } else {
      det_type = 1;
    }
    {
      uint32_t flags = *(uint32_t *)(proj + 0x1dc);
      if (((flags & 0x20) != 0) || ((flags & 8) != 0) || (det_type != 0)) {
        if ((flags & 0x20) == 0) {
          *(uint32_t *)(proj + 0x1dc) = flags | 0x20;
        }
        {
          float timer_val = *(float *)(proj + 500) + *(float *)(proj + 0x1f0);
          *(float *)(proj + 0x1f0) = timer_val;
          if (*(float *)0x2533c8 <= timer_val) {
            char *tmp_proj2 =
              (char *)object_get_and_verify_type(projectile_handle, 0x20);
            if (*(int16_t *)(tmp_proj2 + 0x1e0) < 1) {
              *(int16_t *)(tmp_proj2 + 0x1e0) = 1;
            }
          }
        }
      }
    }
  }

  /* ----------------------------------------------------------------------- */
  /* 6. Export function values.                                              */
  /* ----------------------------------------------------------------------- */
  projectile_export_function_values(projectile_handle);

  /* ----------------------------------------------------------------------- */
  /* 7. Main physics do-while loop.                                          */
  /* ----------------------------------------------------------------------- */
  do {
    float *pfVel; /* pointer to proj velocity (proj+0x18) */

    /* Bail conditions. */
    if ((*(int16_t *)(proj + 0x1e0) != 0) &&
        (((*(int16_t *)(proj + 0x1e0) != 1) ||
          (*(float *)(proj + 0x1fc) == *(float *)0x2533c0) ||
          (*(float *)0x2533c8 <= *(float *)(proj + 0x1f8))))) {
      break;
    }
    if (((*(uint8_t *)(proj + 0x1dc) & 8) != 0) ||
        ((*(uint8_t *)(proj + 4) & 0x20) != 0) ||
        (*(int *)(proj + 0xcc) != -1)) {
      break;
    }

    pfVel = (float *)(proj + 0x18);
    vel_x = *pfVel;
    vel_y = *(float *)(proj + 0x1c);
    vel_z = *(float *)(proj + 0x20);
    avg_vel_x = *pfVel;
    avg_vel_y = *(float *)(proj + 0x1c);
    avg_vel_z = *(float *)(proj + 0x20);
    saved_target = *(int *)(proj + 0x1e4);
    hit_flag = '\0';

    speed = sqrtf(*(float *)(proj + 0x20) * *(float *)(proj + 0x20) +
                  *(float *)(proj + 0x1c) * *(float *)(proj + 0x1c) +
                  *pfVel * *pfVel);
    dist_at_hit = speed;
    speed_prev = speed;

    if (!real_vector3d_valid(pfVel)) {
      display_assert(
        csprintf((char *)0x5ab100,
                 "%s: assert_valid_real_vector2d(%f, %f, %f)",
                 "&projectile->object.translational_velocity",
                 (double)*pfVel, (double)*(float *)(proj + 0x1c),
                 (double)*(float *)(proj + 0x20)),
        "c:\\halo\\SOURCE\\items\\projectiles.c", 0x146, 1);
      system_exit(-1);
    }

    /* -------------------------------------------------------------------- */
    /* 7a. Guided-missile steering.                                          */
    /* -------------------------------------------------------------------- */
    if ((*(int *)(proj + 0x1e8) != -1) &&
        (*(float *)0x2533c0 < *(float *)(proj_tag + 0x1ec))) {
      obj_type_f = (int)object_get_and_verify_type(
        *(int *)(proj + 0x1e8), 0xffffffff);
      steer_turn_rate = *(float *)(proj_tag + 0x1ec) * *(float *)0x2546a4;
      if (((1u << (*(uint8_t *)(obj_type_f + 100) & 0x1f)) & 3u) != 0) {
        tmp_int = (int)object_get_and_verify_type(*(int *)(proj + 0x1e8), 3);
        if (*(int *)(tmp_int + 0x1c8) != -1) {
          steer_turn_rate *= (float)FUN_000b5590(0x13);
        }
      }
      target_dist = (float)FUN_0001ad60((float *)(obj_type_f + 0x50),
                                        (float *)(proj + 0x50));
      if (target_dist <= *(float *)0x253f34) {
        if ((target_dist > *(float *)0x253f40) &&
            ((steer_frac = (target_dist - *(float *)0x253f40) *
                           *(float *)0x268ed0) >= *(float *)0x2533c0)) {
          if (steer_frac > *(float *)0x2533c8) {
            steer_frac = 1.0f;
          }
        } else {
          steer_frac = 0.0f;
        }
      } else {
        steer_frac = 1.0f;
      }
      FUN_001a9520(*(int *)(proj + 0x1e8), &target_pos_x);
      time_tick = game_time_get();
      target_dist =
        (float)(time_tick + (projectile_handle >> 0x10) * 7 & 0xffff);
      {
        float angle_noise1 =
          (float)FUN_0010a5e0(10,
                               (float)(int)target_dist * *(float *)0x26f2e0);
        angles_out[0] = (float)(angle_noise1 * *(float *)0x255a54);
        time_tick = game_time_get();
        target_dist =
          (float)(time_tick + (projectile_handle >> 0x10) * 3 & 0xffff);
        {
          float angle_noise2 =
            (float)FUN_0010a5e0(10,
                                 (float)(int)target_dist * *(float *)0x26f2e0);
          angles_out[2] =
            (float)(*(float *)0x256980 - angle_noise2 * *(float *)0x2568bc);
        }
      }
      angles_out[1] = angles_out[0];
      angles_to_vector(perp_vec, angles_out);
      target_pos_x = perp_vec[0] * steer_frac + target_pos_x;
      target_pos_y = perp_vec[1] * steer_frac + target_pos_y;
      target_pos_z = perp_vec[2] * steer_frac + target_pos_z;
      steer_delta[0] = target_pos_x - *(float *)(proj + 0xc);
      steer_delta[1] = target_pos_y - *(float *)(proj + 0x10);
      steer_delta[2] = target_pos_z - *(float *)(proj + 0x14);
      cross_product3d(pfVel, steer_delta, cross_buf);
      dot = steer_delta[0] * *pfVel + steer_delta[1] * *(float *)(proj + 0x1c) +
            steer_delta[2] * *(float *)(proj + 0x20);
      if ((*(float *)0x2533c0 < dot) &&
          ((float)normalize3d(cross_buf) > *(float *)0x2533c0)) {
        float steer_cos, steer_sin;
#ifdef XDK_BUILD
        __asm fld steer_turn_rate __asm fcos __asm fstp steer_cos
        __asm fld steer_turn_rate __asm fsin __asm fstp steer_sin
#else
        steer_cos = cosf(steer_turn_rate);
        steer_sin = sinf(steer_turn_rate);
#endif
        rotate_vector3d_by_sincos(pfVel, cross_buf, steer_sin, steer_cos);
      }
    }

    if (!real_vector3d_valid(&vel_x)) {
      display_assert("projectile velocity is bad after steering.",
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x19d, 1);
      system_exit(-1);
    }

    /* -------------------------------------------------------------------- */
    /* 7b. Speed clamping / deceleration.                                   */
    /* -------------------------------------------------------------------- */
    if (*(float *)0x2533c8 <= *(float *)(proj + 0x204)) {
      if ((speed_prev <= *(float *)(proj_tag + 0x1e8)) ||
          (*(float *)(proj + 0x20c) == *(float *)0x2533c0)) {
        /* Already at or below max speed, or no deceleration. */
        if (((*(float *)(proj_tag + 0x1c8) != *(float *)0x2533c0) ||
             ((*(float *)(proj_tag + 0x1c0) != *(float *)0x2533c0) ||
              !(*(float *)(proj_tag + 0x1c4) <= *(float *)(proj_tag + 0x1e8)))) ||
            ((*(float *)(proj + 0x20c) == *(float *)0x2533c0) &&
             (*(float *)(proj + 0x200) < *(float *)(proj + 0x210)))) {
          if ((speed_prev < *(float *)(proj_tag + 0x1e8)) &&
              (*(float *)0x2533c0 < speed_prev)) {
            decel_frac =
              (*(float *)(proj_tag + 0x1e8) / speed_prev) * *(float *)0x28ace8;
            vel_x *= decel_frac;
            vel_y *= decel_frac;
            vel_z *= decel_frac;
          }
        } else {
          FUN_000f7e40(projectile_handle, 2);
        }
      } else {
        decel_frac = time_remaining * *(float *)(proj + 0x20c);
        dist_at_hit = speed_prev - decel_frac;
        if (!(dist_at_hit <= *(float *)(proj_tag + 0x1e8))) {
          /* Speed stays above min: normal decel. */
          dist_post = speed_prev - decel_frac * *(float *)0x253398;
          decel_frac = dist_at_hit / speed_prev;
          vel_x *= decel_frac;
          vel_y *= decel_frac;
          vel_z *= decel_frac;
          avg_vel_x = (vel_x + *pfVel) * *(float *)0x253398;
          avg_vel_y = (vel_y + *(float *)(proj + 0x1c)) * *(float *)0x253398;
          avg_vel_z = (vel_z + *(float *)(proj + 0x20)) * *(float *)0x253398;
        } else {
          /* Speed will cross min: split tick. */
          decel_frac = (speed_prev - *(float *)(proj_tag + 0x1e8)) / decel_frac;
          dist_at_hit = *(float *)(proj_tag + 0x1e8) * *(float *)0x28ace8;
          tmp_frac = *(float *)0x2533c8 - decel_frac;
          dist_post =
            tmp_frac * *(float *)(proj_tag + 0x1e8) +
            (dist_at_hit + speed_prev) * decel_frac * *(float *)0x253398;
          decel_frac = dist_at_hit / speed_prev;
          vel_x *= decel_frac;
          vel_y *= decel_frac;
          vel_z *= decel_frac;
          avg_vel_x = tmp_frac * vel_x +
                      (vel_x + *pfVel) * decel_frac * *(float *)0x253398;
          avg_vel_y = tmp_frac * vel_y + (vel_y + *(float *)(proj + 0x1c)) *
                                           decel_frac * *(float *)0x253398;
          avg_vel_z = tmp_frac * vel_z + (vel_z + *(float *)(proj + 0x20)) *
                                           decel_frac * *(float *)0x253398;
        }
      }
    }

    if (!real_vector3d_valid(&vel_x)) {
      display_assert("projectile velocity is bad after deceleration.",
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x1cd, 1);
      system_exit(-1);
    }

    /* -------------------------------------------------------------------- */
    /* 7c. Gravity integration.                                             */
    /* -------------------------------------------------------------------- */
    if ((*(uint8_t *)(proj + 4) & 0x10) == 0) {
      gravity = *(float *)(proj_tag + 0x1cc);
    } else {
      gravity = *(float *)(proj_tag + 0x1d8);
    }
    gravity = *(float *)0x32512c * gravity;
    vel_z -= gravity * time_remaining;
    avg_vel_z -= gravity * time_remaining * *(float *)0x253398;

    if (!real_vector3d_valid(&vel_x)) {
      display_assert("projectile velocity is bad after gravity.",
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x1dc, 1);
      system_exit(-1);
    }

    /* -------------------------------------------------------------------- */
    /* 7d. Max-range limit.                                                 */
    /* -------------------------------------------------------------------- */
    if ((*(float *)(proj_tag + 0x1c8) == *(float *)0x2533c0) ||
        (dist_post * time_remaining + *(float *)(proj + 0x200) <=
         *(float *)(proj_tag + 0x1c8))) {
      range_frac = 1.0f;
    } else {
      if (dist_post == *(float *)0x2533c0) {
        range_frac = 0.0f;
      } else {
        range_frac =
          ((*(float *)(proj_tag + 0x1c8) - *(float *)(proj + 0x200)) /
           dist_post) *
          time_remaining;
      }
      tmp_int = (int)object_get_and_verify_type(projectile_handle, 0x20);
      if (*(int16_t *)(tmp_int + 0x1e0) < 1) {
        *(int16_t *)(tmp_int + 0x1e0) = 1;
      }
    }

    /* -------------------------------------------------------------------- */
    /* 7e. New position computation.                                        */
    /* -------------------------------------------------------------------- */
    decel_frac = range_frac * time_remaining;
    new_pos_x = avg_vel_x * decel_frac + *(float *)(proj + 0xc);
    new_pos_y = avg_vel_y * decel_frac + *(float *)(proj + 0x10);
    new_pos_z = avg_vel_z * decel_frac + *(float *)(proj + 0x14);

    if (!valid_real_point3d(&new_pos_x)) {
      display_assert(
        csprintf((char *)0x5ab100,
                 "%s: assert_valid_real_point3d(%f, %f, %f)",
                 "&new_position",
                 (double)new_pos_x, (double)new_pos_y, (double)new_pos_z),
        "c:\\halo\\SOURCE\\items\\projectiles.c", 0x1f9, 1);
      system_exit(-1);
    }

    /* -------------------------------------------------------------------- */
    /* 7f. Collision depth push.                                            */
    /* -------------------------------------------------------------------- */
    if (*(int16_t *)0x4761d8 > 0x1f) {
      display_assert("global_current_collision_user_depth < "
                     "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x1fb, 1);
      system_exit(-1);
    }
    cd_slot = (int)*(int16_t *)0x4761d8;
    *(int16_t *)0x4761d8 = *(int16_t *)0x4761d8 + 1;
    *(int16_t *)((char *)0x5a8c80 + cd_slot * 2) = 0xe;

    /* -------------------------------------------------------------------- */
    /* 7g. Bounce-limit / detonation check, collision test.                */
    /* -------------------------------------------------------------------- */
    if ((short)bounce_count == 10) {
      tmp_int = (int)object_get_and_verify_type(projectile_handle, 0x20);
      if (*(int16_t *)(tmp_int + 0x1e0) < 1) {
        *(int16_t *)(tmp_int + 0x1e0) = 1;
      }
      time_remaining = 0.0f;
    } else if (*(int16_t *)(proj + 0x1e0) == 2) {
      time_remaining = 0.0f;
    } else {
      hit_flag = '\x01';
      col_hit =
        (char)FUN_000f8720(projectile_handle, &new_pos_x, collision_result);
      if (col_hit == '\0') {
        time_remaining = 0.0f;
      } else {
        /* After hit: adjust time and post-decel velocity. */
        time_remaining =
          *(float *)0x2533c8 - *(float *)((char *)collision_result + 0x14);
        vel_z += gravity * time_remaining;
        if (dist_at_hit != *(float *)0x2533c0) {
          decel_frac = time_remaining * *(float *)(proj + 0x20c) + dist_at_hit;
          if (decel_frac > speed_prev) {
            decel_frac = speed_prev;
          }
          decel_frac /= dist_at_hit;
          vel_x *= decel_frac;
          vel_y *= decel_frac;
          vel_z *= decel_frac;
        }
        /* Set bit 2 if normal angle exceeds threshold. */
        col_normal_z = *(float *)((char *)collision_result + 0x2c);
        if (*(float *)0x2533e4 < col_normal_z) {
          *(uint32_t *)(proj + 0x1dc) |= 4;
        }
        *(int *)(proj + 0x1e4) = -1;
        FUN_000f90d0(projectile_handle, &new_pos_x, time_remaining, &vel_x,
                     (int16_t *)collision_result);
        bounce_count++;
        FUN_000425c0(projectile_handle,
                     (float *)((char *)collision_result + 0x18), 1,
                     *(int16_t *)(proj_tag + 0x182), 1);
        if ((*(uint8_t *)(proj + 0x1dc) & 8) != 0) {
          hit_flag = '\0';
        }
      }
    }

    /* -------------------------------------------------------------------- */
    /* 7h. Collision depth pop.                                             */
    /* -------------------------------------------------------------------- */
    if (*(int16_t *)0x4761d8 < 2) {
      display_assert("global_current_collision_user_depth > 1",
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x230, 1);
      system_exit(-1);
    }
    *(int16_t *)0x4761d8 = *(int16_t *)0x4761d8 - 1;

    /* -------------------------------------------------------------------- */
    /* 7i. Post-hit processing.                                             */
    /* -------------------------------------------------------------------- */
    if (hit_flag != '\0') {
      /* Accumulate total travel distance. */
      {
        float dx, dy, dz;
        dx = new_pos_x - *(float *)(proj + 0xc);
        dy = new_pos_y - *(float *)(proj + 0x10);
        dz = new_pos_z - *(float *)(proj + 0x14);
        *(float *)(proj + 0x200) +=
          sqrtf(dx * dx + dy * dy + dz * dz);
      }

      /* Proximity sound check (up to 4 local players). */
      if ((found_sound == '\0') && (*(int *)(proj_tag + 0x210) != -1)) {
        for (player_idx = 0; (short)player_idx < 4; player_idx++) {
          player_handle = local_player_get_player_index(player_idx);
          if (player_handle != -1) {
            tmp_int = local_player_get_player_index(player_idx);
            obj_handle =
              *(int *)((char *)datum_get(*(void **)0x5aa6d4, tmp_int) + 0x34);
            if ((obj_handle != -1) && (obj_handle != saved_target)) {
              player_obj =
                (int)object_get_and_verify_type(obj_handle, 0xffffffff);
              pfVel = (float *)(player_obj + 0x50);
              sound_range =
                (float)sound_get_default_priority(*(int *)(proj_tag + 0x210));
              steer_delta[0] = *pfVel - *(float *)(proj + 0xc);
              steer_delta[1] =
                *(float *)(player_obj + 0x54) - *(float *)(proj + 0x10);
              steer_delta[2] =
                *(float *)(player_obj + 0x58) - *(float *)(proj + 0x14);
              FUN_0010b910(steer_delta, &new_pos_x, perp_vec, cross_buf2);
              dot = perp_vec[0] * (new_pos_x - *(float *)(proj + 0xc)) +
                    perp_vec[1] * (new_pos_y - *(float *)(proj + 0x10)) +
                    perp_vec[2] * (new_pos_z - *(float *)(proj + 0x14));
              if ((*(float *)0x2533c0 <= dot) &&
                  ((float)FUN_00012170(&new_pos_x) > dot)) {
                mag_sq = (float)FUN_00012170(cross_buf2);
                if (mag_sq < sound_range * sound_range) {
                  /* sound origin = player_pos + (-1)*perp_component; reuse cross_buf2 */
                  vector3d_scale_add((float *)(player_obj + 0x50), cross_buf2,
                                     -1.0f, cross_buf2);
                  unattached_impulse_sound_new(*(int *)(proj_tag + 0x210),
                                               cross_buf2, 1.0f);
                  found_sound = '\x01';
                }
              }
            }
          }
        }
      }

      /* ------------------------------------------------------------------ */
      /* 7j. Orientation update.                                            */
      /* ------------------------------------------------------------------ */
      if (((*(uint8_t *)(proj_tag + 0x17c) & 1) == 0) ||
          ((*(float *)(proj + 0x18) == *(float *)0x2533c0) &&
           (*(float *)(proj + 0x1c) == *(float *)0x2533c0) &&
           (*(float *)(proj + 0x20) == *(float *)0x2533c0))) {
        /* Apply stored rotation (sin/cos at proj+0x220/0x224) if flag set. */
        if ((*(uint8_t *)(proj + 0x1dc) & 1) != 0) {
          float *fwd = (float *)(proj + 0x24);
          float *up  = (float *)(proj + 0x30);
          rotate_vector3d_by_sincos(up,  fwd,
                                    *(float *)(proj + 0x220),
                                    *(float *)(proj + 0x224));
          rotate_vector3d_by_sincos(fwd, (float *)(proj + 0x214),
                                    *(float *)(proj + 0x220),
                                    *(float *)(proj + 0x224));
        }
      } else {
        /* Align forward to current velocity direction. */
        {
          float *fwd = (float *)(proj + 0x24);
          float *up  = (float *)(proj + 0x30);
          fwd_copy[0] = *(float *)(proj + 0x18);
          fwd_copy[1] = *(float *)(proj + 0x1c);
          fwd_copy[2] = *(float *)(proj + 0x20);
          if ((float)normalize3d(fwd_copy) > *(float *)0x2533c0) {
            fwd[0] = fwd_copy[0];
            fwd[1] = fwd_copy[1];
            fwd[2] = fwd_copy[2];
            cross_product3d(up, fwd, cross_buf2);
            cross_product3d(fwd, cross_buf2, up);
            if ((float)normalize3d(up) == *(float *)0x2533c0) {
              perpendicular3d(fwd, up);
              normalize3d(up);
            }
          }
        }
      }

      /* ------------------------------------------------------------------ */
      /* 7k. Translate object to new position.                              */
      /* ------------------------------------------------------------------ */
      object_translate(projectile_handle, &new_pos_x,
                       (void *)((char *)collision_result + 0x0c));
      /* Write back velocity. */
      {
        float local_1c_cmp = time_remaining;
        *(float *)(proj + 0x18) = vel_x;
        *(float *)(proj + 0x1c) = vel_y;
        *(float *)(proj + 0x20) = vel_z;
        if ((local_1c_cmp != *(float *)0x2533c0) &&
            ((short)bounce_count != 0) && (*(int *)(proj + 0x1ec) != -1) &&
            (*(int *)(proj + 0xfc + *(int *)(proj + 0x1ec) * 4) != -1)) {
          object_compute_node_matrices(projectile_handle);
          contrail_set_state_for_object(
            *(int *)(proj + 0xfc + *(int *)(proj + 0x1ec) * 4), 0,
            (*(float *)0x2533c8 - time_remaining) * *(float *)0x28ab38);
        }
      }
    }

    if (!real_vector3d_valid((float *)(proj + 0x18))) {
      display_assert(
        csprintf((char *)0x5ab100,
                 "%s: assert_valid_real_vector2d(%f, %f, %f)",
                 "&projectile->object.translational_velocity",
                 (double)*(float *)(proj + 0x18),
                 (double)*(float *)(proj + 0x1c),
                 (double)*(float *)(proj + 0x20)),
        "c:\\halo\\SOURCE\\items\\projectiles.c", 0x2a0, 1);
      system_exit(-1);
    }
  } while (*(float *)0x2533c0 < time_remaining);

  /* ----------------------------------------------------------------------- */
  /* 8. Post-loop detonation dispatch.                                       */
  /* ----------------------------------------------------------------------- */
  if (*(int16_t *)(proj + 0x1e0) == 1) {
    float fval = *(float *)(proj + 0x1fc);
    if (fval == *(float *)0x2533c0 ||
        *(float *)(proj + 0x1f8) >= *(float *)0x2533c8) {
      FUN_000f8920(projectile_handle, (char)(bounce_count == 0),
                   time_remaining);
      object_delete(projectile_handle);
    }
  } else if (*(int16_t *)(proj + 0x1e0) == 2) {
    object_delete(projectile_handle);
  }

  /* ----------------------------------------------------------------------- */
  /* 9. Validate axes and profiling exit.                                    */
  /* ----------------------------------------------------------------------- */
  if (!valid_real_normal3d_perpendicular((float *)(proj + 0x24),
                                         (float *)(proj + 0x30))) {
    display_assert(
      csprintf((char *)0x5ab100,
               "%s, %s: assert_valid_real_vector3d_axes2"
               "(%f, %f, %f / %f, %f, %f)",
               "&projectile->object.forward",
               "&projectile->object.up",
               (double)*(float *)(proj + 0x24),
               (double)*(float *)(proj + 0x28),
               (double)*(float *)(proj + 0x2c),
               (double)*(float *)(proj + 0x30),
               (double)*(float *)(proj + 0x34),
               (double)*(float *)(proj + 0x38)),
      "c:\\halo\\SOURCE\\items\\projectiles.c", 0x2b0, 1);
    system_exit(-1);
  }
  if ((*(char *)0x449ef1 != '\0') && (*(char *)0x31edb0 != '\0')) {
    profile_exit_private(*(void **)0x31eda8);
  }
  return 1;
}

/*
 * Compute the average damage value for the given weapon tag's projectile.
 *
 * Looks up the weapon tag (group 'weap') and retrieves the first element of
 * the trigger block at offset 0x4fc (element size 0x114).  If out_field8 is
 * non-NULL, writes the float at element+8 there (a trigger parameter such as
 * rounds-per-shot or charge time).
 *
 * Then follows the projectile-type reference at element+0xa0 to a 'proj' tag,
 * and from there the impact-damage-effect reference at proj+0x230 ('jpt!').
 * The midpoint of the damage range (min+max)*0.5f is accumulated.  If a
 * second damage-effect reference exists at proj+0x220, its midpoint is added
 * as well.  Returns the total accumulated average damage as a float.
 */
float FUN_000fac20(int weapon_tag_index, float *out_field8)
{
  char *weap_tag;
  char *trigger_elem;
  char *proj_tag;
  char *jpt_tag;
  int proj_ref;
  int jpt_ref;
  float local_float;

  weap_tag = (char *)tag_get(0x77656170, weapon_tag_index);
  trigger_elem =
    (char *)tag_block_get_element((void *)(weap_tag + 0x4fc), 0, 0x114);
  local_float = 0.0f;
  if (out_field8 != (float *)0) {
    *out_field8 = *(float *)(trigger_elem + 0x8);
  }
  proj_ref = *(int *)(trigger_elem + 0xa0);
  if (proj_ref == -1) {
    return local_float;
  }
  proj_tag = (char *)tag_get(0x70726f6a, proj_ref);
  jpt_ref = *(int *)(proj_tag + 0x230);
  if (jpt_ref != -1) {
    jpt_tag = (char *)tag_get(0x6a707421, jpt_ref);
    local_float = (*(float *)(jpt_tag + 0x1d8) + *(float *)(jpt_tag + 0x1d4)) *
                  *(float *)0x253398;
  }
  jpt_ref = *(int *)(proj_tag + 0x220);
  if (jpt_ref == -1) {
    return local_float;
  }
  jpt_tag = (char *)tag_get(0x6a707421, jpt_ref);
  return (*(float *)(jpt_tag + 0x1d8) + *(float *)(jpt_tag + 0x1d4)) *
           *(float *)0x253398 +
         local_float;
}

/*
 * Wrapper: advance animation state by one frame (update_kind=1) for the
 * given animation graph tag and animation state pointer. Thin wrapper around
 * animation_update_internal with a fixed update_kind of 1.
 * No known direct call-graph callers (likely dispatched via function pointer
 * or animation callback table).
 */
void FUN_000face0(int animation_graph_tag_index, short *state, int *out_sound)
{
  animation_update_internal(1, animation_graph_tag_index, state, out_sound);
}

/*
 * Wrapper: choose a random animation from the given animation graph
 * for the given animation index, using update_kind=1 (normal update).
 * Calls model_animation_choose_random(1, animation_graph_tag_index,
 * animation_index). Called by FUN_001b3580 when transitioning a unit
 * to a new animation state after a melee/scripted override.
 */
void FUN_000fad00(int animation_graph_tag_index, int16_t animation_index)
{
  model_animation_choose_random(1, animation_graph_tag_index, animation_index);
}
