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
  float *ts_base;
  int bank;
  int count;
  int idx;
  const float(*table)[3] = (const float(*)[3])0x5057c4;

  typedef struct {
    float v[3];
  } vec3_t;
  mag = magnitude * *(float *)0x259ec0;
  *(vec3_t *)out = *(const vec3_t *)*(const void **)0x31fc38;
  timescale[0] = 0.1f;
  timescale[1] = 0.2f;
  timescale[2] = 0.07f;
  ts_base = (float *)((char *)timescale - (unsigned int)position);
  bank = 0;
  count = 3;
  do {
    float ts_val = *(float *)((char *)position + (unsigned int)ts_base);
    float pos_val = *position;
    *(volatile float *)&magnitude =
      ((float)*(int *)0x5064c8 * ts_val * scale + pos_val) * *(float *)0x253f78;
    *(volatile int *)&magnitude &= 0x7fffffff;
    *(volatile float *)&magnitude =
      *(volatile float *)&magnitude + *(float *)0x2b229c;
    idx = (short)(*(char *)&magnitude & 0x3f) + bank;
    out[0] = out[0] + table[idx][0];
    out[1] = out[1] + table[idx][1];
    out[2] = out[2] + table[idx][2];
    position++;
    bank += 0x40;
    count--;
  } while (count != 0);
  out[0] = out[0] * mag;
  out[1] = out[1] * mag;
  out[2] = out[2] * mag;
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
    {
      float val_t = rec->t + delta;
      rec->t = val_t;
      if (val_t < 0.0f)
        val_t = 0.0f;
      else if (val_t > 1.0f)
        val_t = 1.0f;
      rec->t = val_t;
    }

    seed = random_math_get_local_seed_address();
    delta = (random_range(seed, 0, 2) != 0) ? 0.01f : -0.01f;
    {
      float val_yaw = rec->yaw_perturbation + delta;
      rec->yaw_perturbation = val_yaw;
      if (val_yaw < -1.0f)
        val_yaw = -1.0f;
      else if (val_yaw > 1.0f)
        val_yaw = 1.0f;
      rec->yaw_perturbation = val_yaw;
    }

    seed = random_math_get_local_seed_address();
    delta = (random_range(seed, 0, 2) != 0) ? 0.01f : -0.01f;
    {
      float val_pitch = rec->pitch_perturbation + delta;
      rec->pitch_perturbation = val_pitch;
      if (val_pitch < -1.0f)
        val_pitch = -1.0f;
      else if (val_pitch > 1.0f)
        val_pitch = 1.0f;
      rec->pitch_perturbation = val_pitch;
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

/* 0x190380 — build the wind noise table at 0x5057c4.
 *
 * The table is 3 banks x 64 float triplets (bank stride 0x300 = 64*0xc).
 * Every 8th triplet of a bank is a "node" (node stride 0x60 = 8 triplets);
 * the 7 triplets between two nodes are filled by interpolation.
 *
 * Phase 1 seeds the 8 nodes of each of the 3 banks with a random unit
 * direction. The seed pointer is fetched between the two argument pushes
 * (PUSH ESI; CALL 0x10b0d0; PUSH EAX; CALL 0x10b380), so the getter call
 * must stay inside the argument list.
 *
 * Phase 2 walks node i (0..7) x sub-sample j (1..7) x bank (0..2) and calls
 * FUN_00089a20 with the four wrapped ring neighbours (i-1, i, i+1, i+2 mod 8
 * of the same bank), the abscissa of the first node ((float)(i-1)), the node
 * spacing (1.0f, pushed as the raw immediate 0x3f800000) and the sample
 * abscissa t = (float)j * K + (float)i, where K is the float at 0x268ed0.
 * The multiply-then-add association is load-bearing for the x87 shape.
 *
 * The three wrapped indices are computed from a loop-carried byte (i+1) with
 * 8-bit arithmetic in the original; the &7 wrap is the semantics, the byte
 * width is MSVC's own lowering.
 *
 * Sole caller: wind_initialize_for_new_map (0x190500).
 */
void FUN_00190380(void)
{
  float *base;
  float *row_ptr;
  float *node_ptr;
  float *entry_ptr;
  float *out_ptr;
  float *node_i_ptr;
  float t;
  float x_i;
  float x_im1;
  int im1;
  int ip1;
  int ip2;
  int i;
  int j;
  int idx;
  int outer_count;
  int mid_count;
  int inner_count;

  base = (float *)0x5057c4;

  row_ptr = base;
  outer_count = 8;
  do {
    node_ptr = row_ptr;
    inner_count = 3;
    do {
      random_seed_get_direction3d(
        (unsigned int *)get_global_random_seed_address(), node_ptr);
      node_ptr += 0xc0;
      inner_count--;
    } while (inner_count != 0);
    row_ptr += 0x18;
    outer_count--;
  } while (outer_count != 0);

  i = 0;
  row_ptr = base + 3;
  outer_count = 8;
  do {
    im1 = (i - 1) & 7;
    ip1 = (i + 1) & 7;
    ip2 = (i + 2) & 7;
    x_i = (float)i;
    x_im1 = (float)(i - 1);
    entry_ptr = row_ptr;
    j = 1;
    mid_count = 7;
    do {
      t = (float)j * *(float *)0x268ed0 + x_i;
      node_i_ptr = row_ptr - 3;
      out_ptr = entry_ptr;
      idx = 0;
      inner_count = 3;
      do {
        FUN_00089a20(out_ptr, base + (idx + im1) * 0x18, node_i_ptr,
                     base + (idx + ip1) * 0x18, base + (idx + ip2) * 0x18,
                     x_im1, 1.0f, t);
        idx += 8;
        node_i_ptr += 0xc0;
        out_ptr += 0xc0;
        inner_count--;
      } while (inner_count != 0);
      entry_ptr += 3;
      j++;
      mid_count--;
    } while (mid_count != 0);
    row_ptr += 0x18;
    i++;
    outer_count--;
  } while (outer_count != 0);
}

/* 0x190500 — wind_initialize_for_new_map
 *
 * Called once per map load. Touches the scenario (scenario_get() is invoked
 * purely for its own asserts/side effects; the returned pointer is discarded
 * at this call site), asserts that the wind globals are NOT yet initialized
 * (!wind_globals.initialized @ 0x5057c0, wind.c:65), then zeroes the whole
 * wind_globals block (0x5057c0, 0xd0c bytes) and sets the initialized byte
 * afterwards -- the store order is load-bearing, since the byte lives inside
 * the memset range. Tail-calls FUN_00190380 to build the derived state.
 */
void wind_initialize_for_new_map(void)
{
  scenario_get();
  if (*(char *)0x5057c0 != 0) {
    display_assert("!wind_globals.initialized",
                   "c:\\halo\\SOURCE\\scenario\\wind.c", 0x41, 1);
    system_exit(-1);
  }
  csmemset((void *)0x5057c0, 0, 0xd0c);
  *(char *)0x5057c0 = 1;
  FUN_00190380();
}
