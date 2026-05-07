/* Clear bit 1 of projectile flags at offset 0x1dc. */
void FUN_000f7cb0(int projectile_handle)
{
  char *proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  *(uint32_t *)(proj + 0x1dc) &= ~2u;
}

/* Delete all live projectile objects (type_mask 0x20).
 * Walks every projectile in the object table via object_iterator_new/next
 * and calls object_delete on each handle. Used during level teardown or
 * game reset to purge all in-flight projectiles. */
void FUN_000f7ce0(void)
{
  object_iter_t iter;

  object_iterator_new(&iter, 0x20, 0);
  while (object_iterator_next(&iter) != NULL) {
    object_delete(iter.last_handle);
  }
}

/* Set the projectile's target handle at offset 0x1e8. */
void FUN_000f7d30(int projectile_handle, int target)
{
  char *proj = (char *)object_get_and_verify_type(projectile_handle, 0x20);
  *(int *)(proj + 0x1e8) = target;
}

/* Set bit 1 of projectile flags at offset 0x1dc. */
void FUN_000f7d50(int projectile_handle)
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
float FUN_000f7d80(int projectile_tag)
{
  return -(*(float *)0x32512c * *(float *)(projectile_tag + 0x1cc));
}

/* Compute a normalized value for a projectile tag field at offset 0x1e4.
 * If the tag field (e.g. a max-distance or scale parameter) is greater than
 * the global zero reference at 0x2533c0 (0.0f), returns value / field.
 * Otherwise returns the zero reference unchanged.
 * Used by the caller to normalize a float quantity against the tag's field,
 * guarding against division by zero or a zero/unset field. */
float FUN_000f7da0(void *proj_tag, float value)
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
void FUN_000f7e10(int projectile_handle, int target)
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
 * Type 3 (contrail/attached): calls FUN_0009ee40 with the attached object
 * handle at tag_def[+0x38] and the marker-slot index at tag_def[+0x3e]
 * (uint16_t), plus hardcoded marker_count=5 and effect_definition=0x31f3a0. Any
 * other type: calls FUN_0009f0e0 with NULL translational-velocity,
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
    FUN_0009ee40(effect_tag_index, object_index, attached_handle, marker_index,
                 5, (void *)0x31f3a0, marker_points, marker_forwards, scale_a,
                 scale_b, 0.0f, 0.0f);
  } else {
    FUN_0009f0e0(effect_tag_index, object_index, NULL, 5, (void *)0x31f3a0,
                 marker_points, marker_forwards, scale_a, scale_b, 0.0f, 0.0f,
                 1);
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
void FUN_000f7ec0(int projectile_handle)
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
char FUN_000f8000(int projectile_handle)
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
void FUN_000f8070(float *forward, float zero, float angle, float *result)
{
  int *seed = get_global_random_seed_address();
  random_direction3d(seed, forward, zero, angle, result);
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
int FUN_000f8410(float speed, float *origin, float *target, float *aim_vector,
                 float *out_speed, float *out_t, float *out_dist)
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
  void *effect_def[2]; /* effect definition passed to FUN_0009f0e0 */

  /* burst-limit locals */
  char *parent_obj; /* type-any parent object ptr */
  int sibling; /* datum handle of current sibling in child list */
  char *sibling_base; /* type-any sibling object ptr */
  char *sibling_proj; /* type-0x20 sibling projectile ptr */
  int count; /* count of active same-type siblings */

  /* world position and orientation buffers */
  float pos[3]; /* projectile world position ([EBP-0x30]) */
  float fwd[3]; /* projectile forward vector ([EBP-0x54]) */
  float up_buf[3]; /* up-vector from *0x31fc50 ([EBP-0x48..0x40]) */
  float parent_pos[3]; /* parent world position (local_b8) */
  float saved_vel[3]; /* saved proj object position at 0xc..0x14 */

  /* damage params for area damage (0xac bytes as in FUN_00136750) */
  char damage_params[0xac];
  float fwd2[3]; /* forward buf for area-damage FUN_00141360 ([EBP-0x74]) */
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

      FUN_00143be0(projectile_handle, parent_pos, (void *)0);
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
  FUN_00141360(projectile_handle, fwd, up_buf);

  /* Copy *0x31fc50 (global up vector ptr) into up_buf slots.
   * These stores ([EBP-0x48,-0x44,-0x40]) are interleaved with PUSH setup
   * in MSVC codegen; they are part of the stack frame used by FUN_0009f0e0
   * but not actually read by it (the marker_forwards arg is &fwd, not &up_buf).
   */
  up_buf[0] = (*(float **)0x31fc50u)[0];
  up_buf[1] = (*(float **)0x31fc50u)[1];
  up_buf[2] = (*(float **)0x31fc50u)[2];

  FUN_0009f0e0(effect_tag, *(int *)(proj + 0x74), (float *)0, 2, effect_def,
               pos, fwd, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

  /* --- Block 4: Area damage (if parent valid and tag has splash entry). --- */
  if ((*(int *)(proj + 0xcc) != -1) && (*(int *)(proj_tag + 0x220) != -1)) {
    FUN_00136750(damage_params, *(int *)(proj_tag + 0x220));

    /* Set "area damage" flag bit 3 (at damage_params+4). */
    *(uint32_t *)(damage_params + 4) |= 8u;

    /* Forward only (no up needed). */
    FUN_00141360(projectile_handle, fwd2, (float *)0);

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

    FUN_00137d20(damage_params, *(int *)(proj + 0xcc), (short)-1, (short)-1,
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
    FUN_0009f0e0(det_effect, *(int *)(proj + 0x74), (float *)0, 2, effect_def,
                 pos, fwd, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
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
int FUN_000f8d30(int projectile_handle)
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
  FUN_000f7ec0(projectile_handle);
  FUN_000f8640(projectile_handle);

  /* Set active + detonating-armed flag bits. */
  *(uint32_t *)(proj + 0x4) |= 0xc0000u;

  return 1;
}
