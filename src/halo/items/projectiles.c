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
