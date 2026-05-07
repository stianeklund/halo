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
void FUN_000f8ee0(int projectile_handle, float *acceleration)
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
 *      params via FUN_00136750, then applies area damage via FUN_00137d20.
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
 *   9. Emits hit effects (FUN_0009ee40 / FUN_0009f0e0) conditioned on
 *      the collision normal dot product.
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
   * the 32-bit effect-tag handle passed to FUN_0009ee40 / FUN_0009f0e0.
   * The binary uses the same 4-byte stack slot (raw int copy in MOV, not FPU).
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

  /* Damage params buffer (0xac bytes; see FUN_00136750). */
  char damage_params[0xac];

  /* Marker/position arrays for FUN_0009ee40 / FUN_0009f0e0. */
  float marker_points[5 * 3]; /* 5 marker positions (float[3] each) */
  float marker_forwards[3];
  float marker_up[3];

  /* Surface-decompose buffers for result type 2 (deflect). */
  float proj_component[3];
  float perp_component[3];

  /* Copy of col_result position at [ESI+0xc..0x10]. */
  float col_pos[3];
  float col_pos2[3];

  /* Velocity copy passed to normalize / normalised direction. */
  float vel_local[3];

  /* Up-vector buffer. */
  float up_buf[3];

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
    FUN_00136750(damage_params, *(int *)(proj_tag + 0x230));
    /* Copy marker positions from col_result. */
    col_pos[0] = *(float *)((char *)col_result + 0x18);
    col_pos[1] = *(float *)((char *)col_result + 0x1c);
    col_pos[2] = *(float *)((char *)col_result + 0x20);
    col_pos2[0] = *(float *)((char *)col_result + 0x18);
    col_pos2[1] = *(float *)((char *)col_result + 0x1c);
    col_pos2[2] = *(float *)((char *)col_result + 0x20);
    /* vel_local = in_velocity copy, then normalize. */
    vel_local[0] = in_velocity[0];
    vel_local[1] = in_velocity[1];
    vel_local[2] = in_velocity[2];
    normalize3d(vel_local);
    /* Call area damage. */
    /* FUN_00137d20: last arg is the direction pointer (ESI+0x24) cast to uint.
     */
    FUN_00137d20(damage_params, *(int *)((char *)col_result + 0x38),
                 (short)col_result[0x1f], (short)col_result[0x1e],
                 (short)col_result[0x27],
                 (unsigned int)(uintptr_t)((char *)col_result + 0x24));
    /* Update tag_idx from result if valid. */
    if ((short)*(int16_t *)((char *)col_result + 0x40) != -1) {
      tag_idx = (int)(short)*(int16_t *)((char *)col_result + 0x40);
    }
    vel_scale = *(float *)((char *)col_result + 0x44);
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
  ang_dot = ang_speed - *(float *)((char *)col_result + 0x2c) * in_velocity[2] -
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
              (deflect_dot == *(float *)((char *)tag_elem + 0x30))) {
          use_alt = 1;
        }
      }
      if (!use_alt &&
          *(float *)((char *)tag_elem + 0x38) != *(float *)0x2533c0) {
        if (ang_dot < *(float *)((char *)tag_elem + 0x34) ||
            (ang_dot < *(float *)((char *)tag_elem + 0x38)) ==
              (ang_dot == *(float *)((char *)tag_elem + 0x38))) {
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
      if (ftemp >= *(float *)((char *)tag_elem + 0x28)) {
        det_result = (int16_t) * (int16_t *)((char *)tag_elem + 0x24);
        /* raw int copy: effect tag handle from alt result */
        tag_idx = *(int *)((char *)tag_elem + 0x48);
      }
    } else {
      /* raw int copy: effect tag handle from default result */
      tag_idx = *(int *)((char *)tag_elem + 0x10);
    }
  }

  /* ------------------------------------------------------------------ */
  /* 6. Collision state == 2 with bit 0x8: breakable surface.           */
  /* ------------------------------------------------------------------ */
  if (col_result[0] == 2 && (*(uint8_t *)((char *)col_result + 0x4c) & 0x8)) {
    FUN_00136750(damage_params, *(int *)(proj_tag + 0x230));
    col_pos[0] = *(float *)((char *)col_result + 0x18);
    col_pos[1] = *(float *)((char *)col_result + 0x1c);
    col_pos[2] = *(float *)((char *)col_result + 0x20);
    col_pos2[0] = *(float *)((char *)col_result + 0x18);
    col_pos2[1] = *(float *)((char *)col_result + 0x1c);
    col_pos2[2] = *(float *)((char *)col_result + 0x20);
    vel_local[0] = in_velocity[0];
    vel_local[1] = in_velocity[1];
    vel_local[2] = in_velocity[2];
    normalize3d(vel_local);
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
    /* FUN_00146a90: breakable surface damage.
     * arg1 = MOVZX byte [ESI+0x4d] (zero-extended to 32 bits via AX).
     * arg2 = &damage_params, arg3 = *(int*)(col_result+0x44). */
    FUN_00146a90((int)(uint32_t)(*(uint8_t *)((char *)col_result + 0x4d)),
                 damage_params, *(int *)((char *)col_result + 0x44));
  }

  /* ------------------------------------------------------------------ */
  /* 7. Write collision position back to hit_pos (param_2).             */
  /* ------------------------------------------------------------------ */
  hit_pos[0] = *(float *)((char *)col_result + 0x18);
  hit_pos[1] = *(float *)((char *)col_result + 0x1c);
  hit_pos[2] = *(float *)((char *)col_result + 0x20);

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
      hit_pos[0] -= *(float *)((char *)col_result + 0x24) * *(float *)0x255ef8;
      hit_pos[1] -= *(float *)((char *)col_result + 0x28) * *(float *)0x255ef8;
      hit_pos[2] -= *(float *)((char *)col_result + 0x2c) * *(float *)0x255ef8;
    } else if (col_result[0] == 3) {
      ftemp = *(float *)0x2533c8 - *(float *)((char *)tag_elem + 0x90);
      in_velocity[0] *= ftemp;
      in_velocity[1] *= ftemp;
      in_velocity[2] *= ftemp;
      proj[0x79] = *(int *)((char *)col_result + 0x38);
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
    random_direction3d((int *)seed, in_velocity, 0.0f,
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
    int16_t scale_mode = *(int16_t *)((char *)tag_elem + 0x5c);
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
    dir_x = dir_x * *(float *)0x255e94;
    /* dir_z unchanged (MSVC no-op scheduled store, preserved for marker_up). */
    dir_y = dir_y * *(float *)0x255e94;

    up_buf[0] = *(float *)(*(int *)0x31fc50);
    up_buf[1] = *(float *)(*(int *)0x31fc50 + 4);
    up_buf[2] = *(float *)(*(int *)0x31fc50 + 8);

    marker_forwards[0] = *(float *)((char *)col_result + 0x24);
    marker_forwards[1] = *(float *)((char *)col_result + 0x28);
    marker_forwards[2] = *(float *)((char *)col_result + 0x2c);

    /* arg1 = &vel_local (normalised dir), arg2 = col_result+0x24 (normal),
     * arg3 = marker_up output.  EAX still held ESI+0x24 at the PUSH. */
    FUN_0010c8e0(vel_local, (float *)((char *)col_result + 0x24), marker_up);

    /* Fill 5 marker_points entries with col_result position. */
    mp = marker_points;
    for (k = 0; k < 5; k++) {
      mp[0] = *(float *)((char *)col_result + 0x18);
      mp[1] = *(float *)((char *)col_result + 0x1c);
      mp[2] = *(float *)((char *)col_result + 0x20);
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
      FUN_0009ee40(
        tag_idx, projectile_handle, *(int *)((char *)col_result + 0x38),
        (uint16_t)col_result[0x1f], 5, (void *)0x31f3a0, marker_points,
        marker_forwards, scale_a, scale_b, 0.0f, 0.0f);
    } else {
      FUN_0009f0e0(tag_idx, projectile_handle, 0, 5, (void *)0x31f3a0,
                   marker_points, marker_forwards, scale_a, scale_b, 0.0f, 0.0f,
                   1);
    }
  }

  /* ------------------------------------------------------------------ */
  /* Secondary effect: destroy/detonate tag effect.                      */
  /* ------------------------------------------------------------------ */
  if (!(proj[0x77] & 0x20) && ((proj[0x77] & 0x10) || det_result == 4)) {
    if (col_result[0] == 3) {
      FUN_0009ee40(*(int *)(proj_tag + 0x200), projectile_handle,
                   *(int *)((char *)col_result + 0x38),
                   (uint16_t)col_result[0x1f], 5, (void *)0x31f3a0,
                   marker_points, marker_forwards, scale_a, scale_b, 0.0f,
                   0.0f);
    } else {
      FUN_0009f0e0(*(int *)(proj_tag + 0x200), projectile_handle, 0, 5,
                   (void *)0x31f3a0, marker_points, marker_forwards, scale_a,
                   scale_b, 0.0f, 0.0f, 1);
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
    FUN_00143be0(projectile_handle, hit_pos,
                 (float *)((char *)col_result + 0xc));
    if (col_result[0] == 3) {
      object_attach_to_parent(*(int *)((char *)col_result + 0x38),
                              projectile_handle, (int16_t)col_result[0x1f]);
    }
    /* Compute spin rate scale if tag "attach on impact" flag (bit 2) set. */
    if ((*(uint8_t *)(proj_tag + 0x17c) & 0x4)) {
      ftemp = *(float *)(proj_tag + 0x1c0) * *(float *)0x253394;
      if (ftemp >= *(float *)0x2533c8) {
        proj[0x7d] = (int)(*(float *)0x2533c8 / ftemp);
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
