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
