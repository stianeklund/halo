/*
 * FUN_0010aa60 — fill a 0x400-byte periodic function lookup table for one of
 * 12 types: one(0), zero(1), cosine(2), cosine_variable(3), triangle(4),
 * soft_triangle(5), diagonal_saw(6), saw_wave(7), gaussian(8),
 * stutter_4harmonic x2 (9,10), square_wave(11).
 *
 * gaussian_scratch is filled by FUN_0010a830 (takes buffer via EBX).
 * Samples are computed for indices 0..0x3FF scaled by *(float*)0x28c8f4.
 * Output bytes are normalized: (sample - min) / (max - min) * 255, clamped.
 * Types 6 and 7 skip the min-subtract step (range forced to 0).
 *
 * 0x10aa60 / random_math.obj (periodic_functions.c)
 */
void FUN_0010aa60(short type_index, void *buffer)
{
  float gaussian_scratch[0x400];
  float sample_scratch[0x400];
  float sample;
  float max_val;
  float min_val;
  float phase;
  float phase_var;
  float range;
  float p;
  unsigned char *out;
  int i;
  int byte_val;
  int k;

  FUN_0010a830(gaussian_scratch);

  max_val = -3.4028235e+38f;
  min_val = 3.4028235e+38f;

  for (i = 0; i < 0x400; i++) {
    phase = (float)i * *(float *)0x28c8f4;
    phase_var = gaussian_scratch[i] * *(float *)0x28c8f0;

    switch (type_index) {
    case 0:
      sample = 1.0f;
      break;
    case 1:
      sample = 0.0f;
      break;
    case 2:
      sample = (float)cos((double)(phase * *(float *)0x255a54));
      break;
    case 3:
      sample = (float)cos((double)(phase_var * *(float *)0x255a54));
      break;
    case 4:
      p = (float)fmod((double)phase, *(double *)0x2573d8);
      if (p < *(float *)0x253398)
        sample = p + p;
      else
        sample = *(float *)0x2533c8 -
                 ((p - *(float *)0x253398) + (p - *(float *)0x253398));
      break;
    case 5:
      p = (float)fmod((double)phase_var, *(double *)0x2573d8);
      if (p < *(float *)0x253398)
        sample = p + p;
      else
        sample = *(float *)0x2533c8 -
                 ((p - *(float *)0x253398) + (p - *(float *)0x253398));
      break;
    case 6:
      sample = (float)fmod((double)phase, *(double *)0x2573d8);
      break;
    case 7:
      sample = (float)fmod((double)phase_var, *(double *)0x2573d8);
      break;
    case 8:
      sample =
        random_math_real((unsigned int *)get_global_random_seed_address());
      break;
    case 9:
    case 10:
      sample =
        ((float)cos((double)(phase * *(float *)0x28c8ec)) * (float)cos((double)(phase * *(float *)0x28c8e8)) +
         (float)cos((double)(phase * *(float *)0x28c8e4)) * (float)sin((double)(phase * *(float *)0x2568bc))) *
          *(float *)0x253398 +
        (float)sin((double)(phase * *(float *)0x256980)) * (float)cos((double)(phase * *(float *)0x255a54));
      break;
    case 11:
      p = (float)fmod((double)phase_var, *(double *)0x2573d8);
      sample = p * p;
      break;
    default:
      display_assert(0, "c:\\halo\\SOURCE\\math\\periodic_functions.c", 0x1f3,
                     1);
      system_exit(-1);
      sample = 0.0f;
      break;
    }

    if (sample > max_val)
      max_val = sample;
    if (sample < min_val)
      min_val = sample;
    sample_scratch[i] = sample;
  }

  if ((1 << type_index) & 0xc0)
    range = *(float *)0x2533c0;
  else
    range = max_val - min_val;

  out = (unsigned char *)buffer;
  for (k = 0; k < 0x400; k++) {
    float v = sample_scratch[k];
    if (range != *(float *)0x2533c0) {
      v = (v - min_val) / range;
    }
    v *= *(float *)0x2602c8;
    byte_val = (int)v;
    if (byte_val < 0)
      byte_val = 0;
    if (byte_val > 0xff)
      byte_val = 0xff;
    out[k] = (unsigned char)byte_val;
  }
}

/*
 * periodic_functions_initialize — allocate and pre-compute all
 * periodic/transition function lookup tables used by the animation and effect
 * systems.
 *
 * Two table arrays (each entry is a 0x400-byte buffer of uint8 samples):
 *   0x46e3b8[0..11] — 12 periodic function types (one=1, zero=0, cosine, cosine
 *       variable, diagonal-saw, saw-wave, triangle, soft-triangle, gaussian,
 *       stutter 4-harmonic x2, square-wave).
 *   0x46e3a0[0..5]  — 6 transition function types.
 *
 * Confirmed: cdecl, no args, void return.
 * Confirmed: 0x46e39c = function_tables_initialized byte flag.
 * Confirmed: CALL 0x10b0d0 (get_global_random_seed_address), seed set to
 * 0x20f3f660. Confirmed: PUSH EBX(0); PUSH 0x400; CALL 0x8ee60 (debug_malloc)
 * per table. Confirmed: PUSH EAX; PUSH EDI → CALL 0x10aa60
 * (FUN_0010aa60(type_index, buf)). Confirmed: MOV EBX,EDI; PUSH EAX → CALL
 * 0x10a930 (FUN_0010a930 BX=type, buf). Confirmed: CMP DI,0xc loop limit (12);
 * CMP DI,0x6 (6).
 */
void periodic_functions_initialize(void)
{
  int i;
  void *buf;
  int *tables;

  if (*(uint8_t *)0x46e39c) {
    display_assert("!function_tables_initialized",
                   "c:\\halo\\SOURCE\\math\\periodic_functions.c", 0x43, 1);
    system_exit(-1);
  }
  *(uint8_t *)0x46e39c = 1;
  *(int *)0x46e3f4 = 0x20f3f660;

  tables = (int *)0x46e3b8;
  for (i = 0; i < 12; i++, tables++) {
    buf = debug_malloc(0x400, 0, "c:\\halo\\SOURCE\\math\\periodic_functions.c",
                       0x4e);
    *tables = (int)buf;
    if (!buf) {
      *(uint8_t *)0x46e39c = 0;
    } else {
      FUN_0010aa60(i, buf);
    }
  }

  tables = (int *)0x46e3a0;
  for (i = 0; i < 6; i++, tables++) {
    buf = debug_malloc(0x400, 0, "c:\\halo\\SOURCE\\math\\periodic_functions.c",
                       0x60);
    *tables = (int)buf;
    if (!buf) {
      *(uint8_t *)0x46e39c = 0;
    } else {
      FUN_0010a930(i, buf);
    }
  }
}

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

/*
 * random_math_initialize — seed the local RNG and load the direction geosphere
 * table used by random_direction and related functions.
 *
 * Seeds 0x46e3f8 (local seed) from XOR of system_seconds, system_milliseconds,
 * and CRT rand(). Loads geosphere tag (type 0x10) and copies its vertex array
 * (3 floats each) into a debug_malloc buffer. Stores buffer pointer at 0x46e3e8
 * and vertex count at 0x46e3ec.
 *
 * Confirmed: cdecl, no args, void return.
 * Confirmed: CALL 0x8e380 (system_seconds), CALL 0x8e370 (system_milliseconds),
 *            CALL 0x1d9d06 (rand) — XOR into [0x46e3f8].
 * Confirmed: PUSH 0x10; CALL 0x1087b0 — load geosphere tag, assert non-null.
 * Confirmed: MOVSX EAX,[ESI+0xc] * 12 = byte size; debug_malloc to 0x46e3e8.
 * Confirmed: MOV CX,[ESI+0xc]; MOV [0x46e3ec],CX — store vertex count (16-bit).
 * Confirmed: Source data at [tag+4]; 12-byte vertex stride (3 floats).
 * Confirmed: PUSH ESI; CALL 0x1056e0 — release tag after copy.
 */
void random_math_initialize(void)
{
  unsigned int seed;
  void *tag;
  int16_t count;
  int16_t i;
  float *src;
  float *dst;

  seed = system_seconds() ^ system_milliseconds() ^ (unsigned int)rand();
  *(unsigned int *)0x46e3f8 = seed;

  tag = FUN_001087b0(0x10);
  if (!tag) {
    display_assert("random_direction_geosphere",
                   "c:\\halo\\SOURCE\\math\\random_math.c", 0xae, 1);
    system_exit(-1);
  }

  count = *(int16_t *)((char *)tag + 0xc);
  *(void **)0x46e3e8 = debug_malloc(
    (int)count * 12, 0, "c:\\halo\\SOURCE\\math\\random_math.c", 0xb0);
  *(int16_t *)0x46e3ec = count;

  i = 0;
  src = *(float **)((char *)tag + 4);
  dst = *(float **)0x46e3e8;
  while (i < *(int16_t *)((char *)tag + 0xc)) {
    dst[i * 3 + 0] = src[i * 3 + 0];
    dst[i * 3 + 1] = src[i * 3 + 1];
    dst[i * 3 + 2] = src[i * 3 + 2];
    i++;
  }

  FUN_001056e0(tag);
}

/* Free the precomputed random direction geosphere table allocated by
 * random_math_initialize. The table pointer is stored at 0x46e3e8. */
void random_math_dispose(void)
{
  debug_free(*(void **)0x46e3e8, "c:\\halo\\SOURCE\\math\\random_math.c", 200);
}

/* Generate a random float in [0.0, ~1.0] from an LCG seed.
 * Advances *seed with the Numerical Recipes LCG (a=0x19660d, c=0x3c6ef35f),
 * extracts the upper 16 bits (0..65535), and normalizes to approximately
 * [0.0, 1.0] by dividing by 65535. */
__declspec(noinline) float random_math_real(unsigned int *seed)
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

  /* cos(2*theta) = 2*dot^2/product - 1, clamped to [-1, 1].
   * The original stores dot and product to memory as 32-bit floats and
   * reloads them (FSTP/FLD round-trip), truncating x87 80-bit excess
   * precision. Match that by forcing intermediates through memory. */
  {
    volatile float dot_mem = dot;
    volatile float prod_mem = product;
    cos2theta = 2.0f * (dot_mem / prod_mem) * dot_mem - 1.0f;
  }
  if (cos2theta <= -1.0f)
    cos2theta = -1.0f;
  else if (cos2theta >= 1.0f)
    cos2theta = 1.0f;
  else if (cos2theta != cos2theta)
    return 0.0f;

  half_angle = acosf(cos2theta) * 0.5f;

  if (half_angle != half_angle) {
    half_angle = 0.0f;
  }
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
