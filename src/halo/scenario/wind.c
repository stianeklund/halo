/* c:\halo\SOURCE\scenario\wind.c
 *
 * Scenario wind system. Declarations arrive through the force-included
 * src/common.h (types.h + generated decl.h); no explicit includes.
 */

/* Per-palette-entry wind record. Array lives at 0x5060c8, stride 0x20.
 *   +0x00 valid                (byte)
 *   +0x04 t                    (lerp param, random-walked, clamped [0,1])
 *   +0x08 pitch_perturbation   (random-walked, clamped [-1,1])
 *   +0x0C yaw_perturbation     (random-walked, clamped [-1,1])
 *   +0x10 velocity             (lerp of wind velocity min..max by t)
 *   +0x14 direction[3]         (world-space wind direction, scaled)
 */
typedef struct wind_record {
  unsigned char valid;
  float t;
  float pitch_perturbation;
  float yaw_perturbation;
  float velocity;
  float direction[3];
} wind_record;

/* Layout guard: the record stride is 0x20 in the binary. */
typedef char wind_record_size_check[(sizeof(wind_record) == 0x20) ? 1 : -1];

/* 0x18ffe0 — wind_update
 *
 * Per-frame update of the scenario wind palette. Asserts that the wind
 * globals are initialized (wind_globals.initialized @ 0x5057c0), bumps the
 * wind update tick counter (0x5064c8), then walks the scenario wind palette
 * tag block (scenario + 0x1b4, element stride 0xf0). For each element with a
 * valid 'wind' tag reference (element+0x8c != -1) it:
 *   - random-walks t, yaw_perturbation, pitch_perturbation by +/-0.01 with
 *     clamping (t to [0,1], the two perturbations to [-1,1]);
 *   - lerps the record velocity between the tag's min/max (wind[0]/wind[1]);
 *   - converts the element's base direction (element+0x90) to angles,
 *     perturbs yaw/pitch by the tag's scale factors (wind[3]/wind[2]) times
 *     the perturbation states times 0.5, converts back to a direction vector;
 *   - scales that direction by (element+0x9c) * velocity;
 *   - marks the record valid.
 * Records whose element has no 'wind' tag (index == -1) are marked invalid.
 * Finally publishes the palette count to wind_globals.count (0x5060c4, int16).
 *
 * Constants: 0.0f=0x2533c0, 1.0f=0x2533c8, -1.0f=0x255e94, 0.5f=0x253398,
 *            +0.01f=0x25bb10, -0.01f=0x2b22a0.
 */

/* 0x18ff00 — sample the wind direction plus position/time-hashed turbulence
 * into out[3]. Starts from the current wind direction vector (deref of the
 * pointer global 0x31fc38, copied as raw dwords), then for each axis k:
 *   phase  = (wind_tick * timescale[k] * scale + position[k]) * 8.0f
 *   index  = (int)fabs(phase) & 0x3f            (x87 fabs + 2^23 magic-add)
 *   out   += noise_table[index + 64k]           (float triplets @ 0x5057c4)
 * and finally scales out by magnitude/3. The per-axis timescales are the
 * immediates 0.1f/0.2f/0.07f. The magnitude parameter's stack slot is reused
 * as the phase spill temp after its entry read, matching the original frame.
 * Register ABI: out in EAX, position in EDX; scale and magnitude on the
 * stack (caller-cleaned). Constants: 1/3=0x259ec0, 8.0f=0x253f78,
 * 2^23=0x2b229c. Wind tick 0x5064c8 is re-read every iteration. Sole caller
 * FUN_00190240 (also wind.c). */
void FUN_0018ff00(float *out, float *position, float scale, float magnitude)
{
  float timescale[3];
  float mag;
  int *dir;
  float *ts;
  volatile float *phase;
  volatile int *phase_bits;
  int bank;
  int count;
  int idx;

  /* The original spills the loop's phase value into the magnitude arg slot
   * ([ebp+0xc]) with FSTP — rounding to single precision BOTH before the
   * 0x7fffffff fabs-mask AND after the 2^23 magic-add. clang otherwise
   * folds the mask to FABS and keeps extended precision (one rounding),
   * which yields a different noise index for phase values near a rounding
   * boundary. volatile access through the param slot forces the exact
   * store/reload sequence. */
  phase = &magnitude;
  phase_bits = (volatile int *)&magnitude;

  mag = magnitude * *(float *)0x259ec0;
  dir = *(int **)0x31fc38; /* pointer loaded once, matching MOV ECX,[mem] */
  ((int *)out)[0] = dir[0];
  ((int *)out)[1] = dir[1];
  ((int *)out)[2] = dir[2];
  timescale[0] = 0.1f;
  timescale[1] = 0.2f;
  timescale[2] = 0.07f;
  ts = timescale;
  bank = 0;
  count = 3;
  do {
    *phase = ((float)*(int *)0x5064c8 * *ts * scale + *position) *
             *(float *)0x253f78;
    *phase_bits &= 0x7fffffff;
    *phase = *phase + *(float *)0x2b229c;
    idx = (*phase_bits & 0x3f) + bank;
    /* volatile view: the original rounds each running sum to single
     * precision in [out] every iteration (FADD mem; FSTP mem) and reloads
     * it next pass; without this clang carries extended precision in ST
     * across iterations (1-ULP drift). */
    ((volatile float *)out)[0] += *(float *)(0x5057c4 + idx * 12);
    ((volatile float *)out)[1] += *(float *)(0x5057c8 + idx * 12);
    ((volatile float *)out)[2] += *(float *)(0x5057cc + idx * 12);
    ts++;
    position++;
    bank += 0x40;
    count--;
  } while (count != 0);
  ((volatile float *)out)[0] *= mag;
  ((volatile float *)out)[1] *= mag;
  ((volatile float *)out)[2] *= mag;
}

void wind_update(void)
{
  int *block;
  short i;
  char *elem;
  float *wind;
  wind_record *rec;
  unsigned int *seed;
  float delta;
  float scale;
  float angles[2];

  block = (int *)scenario_get();
  if (*(char *)0x5057c0 == 0) {
    display_assert("wind_globals.initialized",
                   "c:\\halo\\SOURCE\\scenario\\wind.c", 0x59, 1);
    system_exit(-1);
  }
  (*(int *)0x5064c8)++;
  block = (int *)((char *)block + 0x1b4);

  for (i = 0; i < *block; i++) {
    elem = (char *)tag_block_get_element(block, i, 0xf0);
    rec = &((wind_record *)0x5060c8)[i];
    if (*(int *)(elem + 0x8c) == -1) {
      rec->valid = 0;
      continue;
    }
    wind = (float *)tag_get(0x77696e64 /* 'wind' */, *(int *)(elem + 0x8c));

    seed = random_math_get_local_seed_address();
    delta = (random_range(seed, 0, 2) != 0) ? 0.01f : -0.01f;
    rec->t = rec->t + delta;
    if (rec->t < 0.0f) {
      rec->t = 0.0f;
    } else if (rec->t > 1.0f) {
      rec->t = 1.0f;
    }

    seed = random_math_get_local_seed_address();
    delta = (random_range(seed, 0, 2) != 0) ? 0.01f : -0.01f;
    rec->yaw_perturbation = rec->yaw_perturbation + delta;
    if (rec->yaw_perturbation < -1.0f) {
      rec->yaw_perturbation = -1.0f;
    } else if (rec->yaw_perturbation > 1.0f) {
      rec->yaw_perturbation = 1.0f;
    }

    seed = random_math_get_local_seed_address();
    delta = (random_range(seed, 0, 2) != 0) ? 0.01f : -0.01f;
    rec->pitch_perturbation = rec->pitch_perturbation + delta;
    if (rec->pitch_perturbation < -1.0f) {
      rec->pitch_perturbation = -1.0f;
    } else if (rec->pitch_perturbation > 1.0f) {
      rec->pitch_perturbation = 1.0f;
    }

    rec->velocity = (wind[1] - wind[0]) * rec->t + wind[0];
    vector_to_angles(angles, (float *)(elem + 0x90));
    angles[1] = angles[1] + wind[3] * rec->yaw_perturbation * 0.5f;
    angles[0] = angles[0] + wind[2] * rec->pitch_perturbation * 0.5f;
    angles_to_vector(rec->direction, angles);

    scale = *(float *)(elem + 0x9c) * rec->velocity;
    rec->direction[0] = rec->direction[0] * scale;
    rec->direction[1] = rec->direction[1] * scale;
    rec->direction[2] = rec->direction[2] * scale;
    rec->valid = 1;
  }

  *(int16_t *)0x5060c4 = (int16_t)*block;
}
