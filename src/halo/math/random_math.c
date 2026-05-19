/* Fill a 0x400-byte periodic function lookup table for one of 6 types:
 * raw(0), pow_a(1), pow_b(2), pow_c(3), pow_d(4), sine_wave(5).
 * Each sample is scaled by *(float*)0x2602c8 then clamped to [0, 255].
 * type_index is passed in BX (sign-extended to EBX for the switch).
 *
 * 0x10a930 / random_math.obj
 */
void FUN_0010a930(int16_t type_index, void *buffer)
{
  float phase;
  float sample;
  int result;
  int i;

  for (i = 0; i < 0x400; i++) {
    phase = (float)i * *(float *)0x28c8e0;
    switch (type_index) {
    case 0:
      sample = phase;
      break;
    case 1:
      sample = (float)pow((double)phase, *(double *)0x25fea8);
      break;
    case 2:
      sample = (float)pow((double)phase, *(double *)0x28c8d8);
      break;
    case 3:
      sample = (float)pow((double)phase, *(double *)0x28c8d0);
      break;
    case 4:
      sample = (float)pow((double)phase, *(double *)0x281de8);
      break;
    case 5:
      sample = (sinf(phase * *(float *)0x256980 - *(float *)0x2568bc) + 1.0f) *
               *(float *)0x253398;
      break;
    default:
      display_assert(0, "c:\\halo\\SOURCE\\math\\periodic_functions.c", 0x19b,
                     1);
      system_exit(-1);
      break;
    }
    result = (int)(sample * *(float *)0x2602c8);
    if (result < 0)
      result = 0;
    else if (result > 0xff)
      result = 0xff;
    ((unsigned char *)buffer)[i] = (unsigned char)result;
  }
}

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
      sample = ((float)cos((double)(phase * *(float *)0x28c8ec)) *
                  (float)cos((double)(phase * *(float *)0x28c8e8)) +
                (float)cos((double)(phase * *(float *)0x28c8e4)) *
                  (float)sin((double)(phase * *(float *)0x2568bc))) *
                 *(float *)0x253398 +
               (float)sin((double)(phase * *(float *)0x256980)) *
                 (float)cos((double)(phase * *(float *)0x255a54));
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

/* Factorial: n! for n>=0; returns 1 for n<=1, 0 for n<0.
 * 0x10add0 / random_math.obj (probability.c)
 */
int FUN_0010add0(short param_1)
{
  int iVar1;
  int iVar2;
  unsigned int uVar3;

  iVar1 = 0;
  if (param_1 >= 0) {
    iVar1 = 1;
    if (param_1 > 1) {
      iVar2 = (int)param_1;
      uVar3 = (unsigned int)(unsigned short)(param_1 - 1);
      do {
        iVar1 = iVar1 * iVar2;
        iVar2 = iVar2 - 1;
        uVar3 = uVar3 - 1;
      } while (uVar3 != 0);
    }
  }
  return iVar1;
}

/* Falling factorial: param_1 * (param_1-1) * ... * (param_1-param_2+1).
 * Returns 1 if param_2==0; 0 if param_2>param_1 or param_2<0.
 * 0x10ae00 / random_math.obj (probability.c)
 */
int FUN_0010ae00(short param_1, short param_2)
{
  int iVar1;
  unsigned int uVar2;

  iVar1 = 0;
  if (param_1 >= param_2 && param_2 >= 0) {
    iVar1 = 1;
    if (param_2 != 0) {
      uVar2 = (unsigned int)(unsigned short)param_2;
      do {
        iVar1 = iVar1 * (int)param_1;
        param_1 = param_1 - 1;
        uVar2 = uVar2 - 1;
      } while (uVar2 != 0);
    }
  }
  return iVar1;
}

/* Binomial coefficient C(param_1, param_2).
 * Uses symmetry: if param_2 > param_1-param_2, substitutes param_1-param_2.
 * 0x10ae30 / random_math.obj (probability.c)
 */
int FUN_0010ae30(int param_1, int param_2)
{
  int iVar1;
  int iVar2;
  short sVar3;
  unsigned int uVar4;

  iVar1 = 0;
  sVar3 = (short)param_2;
  if ((short)param_1 >= sVar3 && sVar3 >= 0) {
    if ((int)(short)param_1 - (int)sVar3 < (int)sVar3)
      param_2 = param_1 - param_2;
    iVar1 = FUN_0010ae00((short)param_1, (short)param_2);
    if ((short)param_2 > 1) {
      iVar2 = (int)(short)param_2;
      uVar4 = (unsigned int)(unsigned short)((short)param_2 - 1);
      do {
        iVar1 = iVar1 / iVar2;
        iVar2 = iVar2 - 1;
        uVar4 = uVar4 - 1;
      } while (uVar4 != 0);
    }
  }
  return iVar1;
}

/* next_combination — advance a k-combination of [0,base) to the next in
 * lexicographic order. param_3 is an array of param_2 short indices.
 * If any index is out of range, the array is zeroed.
 * Returns 1 on success, 0 if the last combination was already reached.
 * 0x10ae80 / random_math.obj (probability.c)
 */
char FUN_0010ae80(short param_1, unsigned int param_2, unsigned int *param_3)
{
  short *psVar1;
  short sVar2;
  unsigned int uVar3;
  int iVar4;
  short sVar5;
  short sVar6;
  short *fill_p;
  unsigned short fill_n;

  assert_halt(param_1 > 0);
  sVar6 = (short)param_2;
  assert_halt(sVar6 > 0);
  assert_halt((int)param_3);
  sVar5 = 0;
  if (0 < sVar6) {
    do {
      sVar2 = *(short *)((int)param_3 + (int)sVar5 * 2);
      if ((sVar2 < 0) || (param_1 <= sVar2))
        goto LAB_0010af53;
      sVar5 = sVar5 + 1;
    } while (sVar5 < sVar6);
  }
  uVar3 = param_2 - 1;
  if (-1 < (short)uVar3) {
    do {
      if ((int)*(short *)((int)param_3 + (short)uVar3 * 2) < (int)param_1 - 1) {
        psVar1 = (short *)((int)param_3 + (short)uVar3 * 2);
        *psVar1 = *psVar1 + 1;
        iVar4 = uVar3 + 1;
        if ((short)iVar4 < sVar6) {
          param_3 = (unsigned int *)((int)param_3 + (short)iVar4 * 2);
          param_2 = param_2 - iVar4;
        LAB_0010af53:
          fill_p = (short *)param_3;
          fill_n = (unsigned short)param_2;
          while (fill_n != 0) {
            *fill_p++ = 0;
            fill_n--;
          }
        }
        return 1;
      }
      uVar3 = uVar3 - 1;
    } while (-1 < (short)uVar3);
  }
  return 0;
}

/* next_combination_strict — advance a k-combination of [0,base) in strictly
 * increasing order. param_3 must be strictly increasing; if invalid, resets to
 * {0,1,...,count-1}. Returns 1 on success, 0 if already at last combination.
 * 0x10af70 / random_math.obj (probability.c)
 */
char FUN_0010af70(short param_1, int param_2, short *param_3)
{
  short sVar1;
  short sVar2;
  unsigned int uVar3;
  int iVar4;
  short *psVar5;
  short sVar6;

  sVar6 = (short)param_2;
  assert_halt(param_1 >= sVar6);
  assert_halt(sVar6 > 0);
  assert_halt((int)param_3);
  sVar2 = 0;
  if (0 < sVar6) {
    do {
      sVar1 = param_3[sVar2];
      if ((sVar1 < 0) || (param_1 <= sVar1) ||
          (0 < sVar2 && (sVar1 <= param_3[sVar2 - 1]))) {
        iVar4 = 0;
        do {
          *param_3 = (short)iVar4;
          iVar4 = iVar4 + 1;
          param_3 = param_3 + 1;
        } while ((short)iVar4 < sVar6);
        return 1;
      }
      sVar2 = sVar2 + 1;
    } while (sVar2 < sVar6);
  }
  uVar3 = (unsigned int)((int)sVar6 - 1);
  if ((short)uVar3 >= 0) {
    do {
      sVar2 = (short)uVar3;
      if ((int)param_3[sVar2] < ((int)param_1 - (int)sVar6) + (int)sVar2) {
        param_3[sVar2] = param_3[sVar2] + 1;
        iVar4 = uVar3 + 1;
        if ((short)iVar4 < sVar6) {
          psVar5 = param_3 + (short)iVar4;
          uVar3 = (param_2 - iVar4) & 0xffff;
          do {
            *psVar5 = psVar5[-1] + 1;
            psVar5 = psVar5 + 1;
            uVar3 = uVar3 - 1;
            iVar4 = 0;
          } while (uVar3 != 0);
        }
        return 1;
      }
      uVar3 = uVar3 - 1;
    } while ((short)uVar3 >= 0);
  }
  return 0;
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

/* Returns the current global random seed value.
 *
 * 0x10b110 / random_math.obj
 */
unsigned int get_random_seed(void)
{
  return *(unsigned int *)0x46e3f4;
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

/* Quaternion multiply: out = q1 * q2. Handles aliasing: if q1 or q2
 * overlaps out, a local copy is made before computing. All arrays are
 * 4-element [x, y, z, w] (index 3 = w).
 *
 * 0x10b9c0 / random_math.obj
 */
void FUN_0010b9c0(float *q1, float *q2, float *out)
{
  float buf[4];

  if (q1 == out) {
    buf[0] = q1[0];
    buf[1] = q1[1];
    buf[3] = q1[3];
    buf[2] = q1[2];
    q1 = buf;
  }
  if (q2 == out) {
    buf[0] = q2[0];
    buf[1] = q2[1];
    buf[2] = q2[2];
    buf[3] = q2[3];
    q2 = buf;
  }
  out[0] = (q1[3] * q2[0] + q2[2] * q1[1] + q2[3] * q1[0]) - q2[1] * q1[2];
  out[1] = (q1[3] * q2[1] + q2[3] * q1[1] + q1[2] * q2[0]) - q1[0] * q2[2];
  out[2] = (q1[3] * q2[2] + q2[1] * q1[0] + q2[3] * q1[2]) - q2[0] * q1[1];
  out[3] = ((q2[3] * q1[3] - q1[0] * q2[0]) - q2[1] * q1[1]) - q1[2] * q2[2];
}

/* Quaternion LERP: blend q1 toward q2 by t, writing result to out.
 * fVar1 = 1.0f - t. If dot(q1,q2) < 0, t is negated (shortest arc).
 * out[i] = t*q2[i] + (1-t)*q1[i] for i in {0,1,2,3}.
 * FPU-WARN reviewed: ECX/EDX register assignment differs from reference,
 * all flagged ops are commutative multiplications — no semantic difference.
 * 0x10ba90 / random_math.obj
 */
void FUN_0010ba90(float *q1, float *q2, float t, float *out)
{
  float fVar1;

  fVar1 = *(float *)0x2533c8 - t;
  if (q2[3] * q1[3] + *q1 * *q2 + q2[1] * q1[1] + q2[2] * q1[2] <
      *(float *)0x2533c0) {
    t = -t;
  }
  *out = t * *q2 + fVar1 * *q1;
  out[1] = fVar1 * q1[1] + t * q2[1];
  out[2] = fVar1 * q1[2] + t * q2[2];
  out[3] = fVar1 * q1[3] + t * q2[3];
}

/* Normalize a 2D vector v in-place; returns the input pointer.
 * If the vector is zero-length (|v|^2 == 0.0f), it is left unchanged.
 * 0x10c290 / random_math.obj
 */
float *FUN_0010c290(float *v)
{
  float mag_sq;
  float inv_mag;

  mag_sq = v[1] * v[1] + v[0] * v[0];
  if (mag_sq != *(float *)0x2533c0) {
    inv_mag = *(float *)0x2533c8 / sqrtf(mag_sq);
    v[0] = inv_mag * v[0];
    v[1] = inv_mag * v[1];
    return v;
  }
  return v;
}

/* Normalize a 3-element float vector in-place.
 * Returns the input pointer. If zero-length, vector is left unchanged.
 *
 * 0x10c2e0 / random_math.obj
 */
float *FUN_0010c2e0(float *v)
{
  float len_sq;
  float inv_len;
  len_sq = v[2] * v[2] + v[1] * v[1] + v[0] * v[0];
  if (len_sq != 0.0f) {
    inv_len = 1.0f / sqrtf(len_sq);
    v[0] = inv_len * v[0];
    v[1] = inv_len * v[1];
    v[2] = inv_len * v[2];
  }
  return v;
}

/* Returns the magnitude of the cross product of two 3-element vectors.
 * |cross(v1,v2)| = |(v1 x v2)|.
 *
 * 0x10c340 / random_math.obj
 */
float FUN_0010c340(float *v1, float *v2)
{
  float cx;
  float cy;
  float cz;
  cx = v2[2] * v1[1] - v1[2] * v2[1];
  cy = v1[2] * v2[0] - v2[2] * v1[0];
  cz = v1[0] * v2[1] - v2[0] * v1[1];
  return sqrtf(cy * cy + cx * cx + cz * cz);
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

/* Linear interpolation of two 3D vectors, then normalize result into out.
 * Computes out[i] = t*v2[i] + (1-t)*v1[i], then normalizes out in-place.
 *
 * 0x10c780 / random_math.obj
 */
void FUN_0010c780(float *v1, float *v2, float t, float *out)
{
  float s;
  s = 1.0f - t;
  out[0] = t * v2[0] + s * v1[0];
  out[1] = t * v2[1] + s * v1[1];
  out[2] = t * v2[2] + s * v1[2];
  FUN_0010c2e0(out);
}

/* Spherical rotation: rotate param_1 toward param_2 by blend factor param_3
 * in [0, 1].  Writes normalised result into param_4.
 *
 * The blend is slerp-like:
 *   t = param_3
 *   angle = FUN_0010c510(param_1, param_2)   (angle between the 3-D vectors)
 *   if t < 0.5:
 *     rotate param_1 by (angle * t) around cross(param_1, param_2)
 *   else:
 *     rotate param_2 by (angle * (1 - t)) around cross(param_2, param_1)
 * The cross-product axis is normalised by normalize3d before use.
 * If the axis magnitude is 0 (parallel vectors), no rotation is applied.
 *
 * Threshold for branch selection: 0.5f (_DAT_00253398).
 * 1.0f constant: _DAT_002533c8.
 *
 * 0x10c7d0 / random_math.obj
 */
void FUN_0010c7d0(float *param_1, float *param_2, float param_3, float *param_4)
{
  float axis[3];
  float angle;
  float blend;

  angle = FUN_0010c510(param_1, param_2);
  if (param_3 < 0.5f) {
    /* Rotate from param_1 side: scale angle by t, axis = cross(param_1, param_2) */
    blend = angle * param_3;
    axis[0] = param_2[2] * param_1[1] - param_2[1] * param_1[2];
    axis[1] = param_2[0] * param_1[2] - param_2[2] * param_1[0];
    axis[2] = param_2[1] * param_1[0] - param_1[1] * param_2[0];
    param_4[0] = param_1[0];
    param_4[1] = param_1[1];
    param_4[2] = param_1[2];
  } else {
    /* Rotate from param_2 side: scale angle by (1-t), axis = cross(param_2, param_1) */
    blend = angle * (1.0f - param_3);
    axis[0] = param_2[1] * param_1[2] - param_2[2] * param_1[1];
    axis[1] = param_2[2] * param_1[0] - param_2[0] * param_1[2];
    axis[2] = param_1[1] * param_2[0] - param_2[1] * param_1[0];
    param_4[0] = param_2[0];
    param_4[1] = param_2[1];
    param_4[2] = param_2[2];
  }
  if (normalize3d(axis) > 0.0f) {
    rotate_vector3d_by_sincos(param_4, axis, sinf(blend), cosf(blend));
  }
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

/* Spherical rotation: rotate v1 toward v2 by angle t (radians), writing
 * normalized result into out. Uses the cross product axis and dot product
 * to blend v2 against v1, then rescales to preserve the original magnitude.
 *
 * 0x10c920 / random_math.obj
 */
void FUN_0010c920(float *v1, float *v2, float t, float *out)
{
  float fVar1;
  float fVar2;
  float fVar3;
  float fVar4;
  float fVar5;

  fVar1 = sqrtf(v1[2] * v1[2] + v1[1] * v1[1] + v1[0] * v1[0]);
  fVar2 = 1.0f / fVar1;
  fVar4 = v2[2] * v1[1] - v2[1] * v1[2];
  fVar5 = v2[0] * v1[2] - v2[2] * v1[0];
  fVar3 = v2[1] * v1[0] - v2[0] * v1[1];
  fVar3 = sqrtf(fVar5 * fVar5 + fVar4 * fVar4 + fVar3 * fVar3) * fVar2;
  t = t * fVar3;
  t = ((v1[0] * v2[0] + v2[1] * v1[1] + v2[2] * v1[2]) * fVar2 * t +
       sqrtf(1.0f - t * t) * fVar3) /
      t;
  out[0] = t * v2[0] + v1[0];
  out[1] = t * v2[1] + v1[1];
  fVar2 = t * v2[2] + v1[2];
  out[2] = fVar2;
  fVar1 = fVar1 / sqrtf(fVar2 * fVar2 + out[1] * out[1] + out[0] * out[0]);
  out[0] = fVar1 * out[0];
  out[1] = fVar1 * out[1];
  out[2] = fVar1 * out[2];
}
