void lock_global_random_seed(void)
{
  *(int *)0x46e3f0 = *(int *)0x46e3f0 + 1;
}

void unlock_global_random_seed(void)
{
  assert_halt(*(int *)0x46e3f0 > 0);
  *(int *)0x46e3f0 = *(int *)0x46e3f0 - 1;
}

int *get_global_random_seed_address(void)
{
  if (game_engine_running() && *(int *)0x46e3f0 != 0) {
    display_assert(
      "you should not be using global random(); use local random() instead",
      "c:\\halo\\SOURCE\\math\\random_math.c", 0x38, 1);
    system_exit(-1);
  }
  return (int *)0x46e3f4;
}

/* Returns the address of the thread-local random seed.
 * Used by callers that need a local (non-global) RNG state
 * to avoid contention with the global seed. */
unsigned int *random_math_get_local_seed_address(void)
{
  return (unsigned int *)0x46e3f8;
}

/* Empty in retail; presumably logged seed state to debug output. */
void random_seed_debug_log(bool a1)
{
}

/* Generate a random float in [0.0, ~1.0] from an LCG seed.
 * Advances *seed with the Numerical Recipes LCG (a=0x19660d, c=0x3c6ef35f),
 * extracts the upper 16 bits (0..65535), and normalizes to approximately
 * [0.0, 1.0] by dividing by 65535. */
float random_math_real(unsigned int *seed)
{
  unsigned int s;

  s = *seed * 0x19660d + 0x3c6ef35f;
  *seed = s;
  return (float)(s >> 16) / 65535.0f;
}

/* Generate a random float in [min, max] using the same LCG as random_range.
 * Advances *seed, extracts the upper 16 bits (0..65535), normalizes to
 * [0.0, 1.0] by dividing by 65535, then scales into [min, max]. */
float random_real_range(int *seed, float min, float max)
{
  unsigned int s;

  s = (unsigned int)*seed * 0x19660d + 0x3c6ef35f;
  *seed = (int)s;
  return (float)(s >> 16) / 65535.0f * (max - min) + min;
}

/* Advance an LCG seed and return the upper 16 bits. */
uint16_t random_seed_step(unsigned int *seed)
{
  unsigned int s;

  s = *seed * 0x19660d + 0x3c6ef35f;
  *seed = s;
  return (uint16_t)(s >> 16);
}

/* Generate a random int16 in [min, max) using a linear congruential
 * generator.  Advances *seed with the classic Numerical Recipes
 * LCG (a=0x19660d, c=0x3c6ef35f), then maps the upper 16 bits of
 * the new seed into the requested range. */
int16_t random_range(unsigned int *seed, int16_t min, int16_t max)
{
  unsigned int s;

  s = *seed * 0x19660d + 0x3c6ef35f;
  *seed = s;
  return (int16_t)(((int)(max - min) * (int)(s >> 16) >> 16) + (int)min);
}

/* Look up a precomputed unit direction from the random direction table
 * (0x10b300). Asserts the table is initialized and index is in range. Copies 3
 * floats from table[index] to result and returns result. Register args: index
 * in SI, result in EBX. */
float *random_direction_table_get_element(int16_t index, float *result)
{
  float *element = (float *)(*(int *)0x46e3e8 + (int)index * 12);

  assert_halt(*(int *)0x46e3e8);
  assert_halt(index >= 0 && index < *(int16_t *)0x46e3ec);

  result[0] = element[0];
  result[1] = element[1];
  result[2] = element[2];

  return result;
}

/* Advance the LCG seed and look up a random direction (0x10b380).
 * Inlines the LCG step and index computation, then calls
 * random_direction_table_get_element with the computed index. */
void random_seed_get_direction3d(unsigned int *seed, float *out)
{
  unsigned int s;
  int16_t table_size;
  int16_t index;

  s = *seed * 0x19660d + 0x3c6ef35f;
  *seed = s;

  table_size = *(int16_t *)0x46e3ec;
  index = (int16_t)(((int)table_size * (int)(s >> 16)) >> 16);

  random_direction_table_get_element(index, out);
}

/* Generate a random 3D direction within a cone around a forward vector.
 *
 * Picks a random unit vector from a precomputed sphere table, computes the
 * cross product with 'forward' to obtain a perpendicular rotation axis,
 * normalizes it, then rotates 'forward' around that axis by a random angle
 * in [zero, angle].  If the random vector is (anti)parallel to forward
 * (cross product magnitude <= 0), the function bails out and returns
 * the forward vector unmodified in 'result'.
 *
 * Parameters:
 *   seed    - LCG random state (advanced twice: once for table index,
 *             once for the rotation angle)
 *   forward - input direction vector (3 floats)
 *   zero    - minimum rotation angle (radians)
 *   angle   - maximum rotation angle (radians)
 *   result  - output direction vector (3 floats), initially set to forward
 */
void random_direction3d(int *seed, float *forward, float zero, float angle,
                        float *result)
{
  float random_vec[3];
  float cross[3];
  int16_t index;
  float random_angle;
  float sin_val, cos_val;

  /* Copy forward to result */
  result[0] = forward[0];
  result[1] = forward[1];
  result[2] = forward[2];

  /* Pick a random direction from the precomputed sphere table.
   * Inlines: index = random_range(seed, 0, table_size) then table lookup. */
  index = random_range((unsigned int *)seed, 0, *(int16_t *)0x46e3ec);
  random_direction_table_get_element(index, random_vec);

  /* Cross product: cross = random_vec x forward */
  cross[0] = random_vec[2] * forward[1] - random_vec[1] * forward[2];
  cross[1] = random_vec[0] * forward[2] - random_vec[2] * forward[0];
  cross[2] = random_vec[1] * forward[0] - random_vec[0] * forward[1];

  /* Normalize the rotation axis; bail if degenerate (parallel vectors) */
  if (normalize3d(cross) <= *(float *)0x2533c0)
    return;

  /* Random rotation angle in [zero, angle] (inlines random_real_range) */
  random_angle = random_real_range(seed, zero, angle);
  sin_val = sinf(random_angle);
  cos_val = cosf(random_angle);

  /* Rotate result around the cross axis by the random angle */
  rotate_vector3d_by_sincos(result, cross, sin_val, cos_val);
}

/* Decompose vector v into components parallel and perpendicular to normal n.
 *
 * Computes dot(n,n) (magnitude-squared of n).  If n is the zero vector
 * (dot(n,n) == 0.0f), proj_out is zeroed and perp_out is set to v unchanged.
 * Otherwise:
 *   proj_out  = (dot(v,n) / dot(n,n)) * n   (projection of v onto n)
 *   perp_out  = v - proj_out                 (component of v perpendicular to
 * n)
 *
 * All vectors are 3-component float arrays.
 * The zero branch uses integer stores (XOR + MOV) to avoid redundant FPU ops.
 *
 * 0x10b910 / random_math.obj
 */
void FUN_0010b910(float *v, float *n, float *proj_out, float *perp_out)
{
  float nn;
  float scale;

  nn = (n[2] * n[2] + n[1] * n[1]) + n[0] * n[0];
  if (nn != 0.0f) {
    scale = ((v[2] * n[2] + v[1] * n[1]) + v[0] * n[0]) / nn;
    proj_out[0] = scale * n[0];
    proj_out[1] = scale * n[1];
    proj_out[2] = scale * n[2];
    perp_out[0] = v[0] - proj_out[0];
    perp_out[1] = v[1] - proj_out[1];
    perp_out[2] = v[2] - proj_out[2];
  } else {
    proj_out[0] = 0.0f;
    proj_out[1] = 0.0f;
    proj_out[2] = 0.0f;
    perp_out[0] = v[0];
    perp_out[1] = v[1];
    perp_out[2] = v[2];
  }
}

/* Compute the angle (radians) between two 3D vectors v1 and v2.
 *
 * Returns acos(dot(v1,v2) / (|v1| * |v2|)), the geometric angle between
 * the two input vectors.  If either vector is zero-length (product of squared
 * magnitudes == 0), returns 0.0f.
 *
 * Implementation uses the double-angle identity to avoid a sqrt:
 *   cos(2*theta) = 2*cos^2(theta) - 1
 *               = 2 * dot^2 / (|v1|^2 * |v2|^2) - 1
 * acos(cos(2*theta)) * 0.5 = theta  (when dot >= 0)
 * PI - acos(cos(2*theta)) * 0.5 = theta  (when dot < 0, due to range wrapping)
 * The cos(2*theta) value is clamped to [-1, 1] before acos to guard against
 * floating-point overshoot.
 *
 * 0x10c510 / random_math.obj
 */
float FUN_0010c510(float *v1, float *v2)
{
  float product;
  float dot;
  float cos2theta;
  float half_angle;

  /* Product of squared magnitudes: |v1|^2 * |v2|^2 */
  product = (v1[0] * v1[0] + v1[1] * v1[1] + v1[2] * v1[2]) *
            (v2[0] * v2[0] + v2[1] * v2[1] + v2[2] * v2[2]);

  if (product == 0.0f)
    return 0.0f;

  /* dot(v1, v2) */
  dot = v1[2] * v2[2] + v1[1] * v2[1] + v1[0] * v2[0];

  /* cos(2*theta) = 2*dot^2/product - 1, clamped to [-1, 1] */
  cos2theta = 2.0f * (dot / product) * dot - 1.0f;
  if (cos2theta <= -1.0f)
    cos2theta = -1.0f;
  else if (cos2theta >= 1.0f)
    cos2theta = 1.0f;

  half_angle = acosf(cos2theta) * 0.5f;

  if (dot < 0.0f)
    return 3.1415927f - half_angle;
  return half_angle;
}

/* Reflect vector v about surface normal n into out.
 *
 * Computes: out = v - 2*dot(v,n)*n
 *
 * All three vectors are 3-component float arrays.
 * The dot product is computed first, doubled via FADD ST0,ST0,
 * then each output component is v[i] - 2*dot*n[i].
 *
 * 0x10c8e0 / random_math.obj
 */
void FUN_0010c8e0(float *v, float *n, float *out)
{
  float dot2;

  dot2 = (v[0] * n[0] + v[1] * n[1] + v[2] * n[2]) * 2.0f;
  out[0] = v[0] - dot2 * n[0];
  out[1] = v[1] - dot2 * n[1];
  out[2] = v[2] - dot2 * n[2];
}
