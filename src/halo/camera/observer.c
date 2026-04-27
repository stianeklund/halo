/* Camera observer — tracks camera position/orientation per player. */

void observer_initialize(void)
{
}

/* Initialize an observer result struct with default camera orientation,
 * zero velocities, and signature markers (0x8a350). Sets the camera up/forward
 * vectors from globals, zeros the integration working area, then copies the
 * template vectors into the active camera state. */
void observer_result_initialize(void *observer)
{
  char *obs = (char *)observer;
  float *up = *(float **)0x31fc3c;
  float *fwd = *(float **)0x31fc44;
  float *pos;

  *(float *)(obs + 0xd0) = up[0];
  *(float *)(obs + 0xd4) = up[1];
  *(float *)(obs + 0xd8) = up[2];
  *(float *)(obs + 0xdc) = fwd[0];
  *(float *)(obs + 0xe0) = fwd[1];
  *(float *)(obs + 0xe4) = fwd[2];
  *(int *)(obs + 0xcc) = 0x3f5f66f3;

  pos = *(float **)0x31fc1c;
  *(float *)(obs + 0x74) = pos[0];
  *(float *)(obs + 0x78) = pos[1];
  *(float *)(obs + 0x7c) = pos[2];

  *(int16_t *)(obs + 0x84) = -1;
  *(int *)(obs + 0x80) = -1;

  {
    float *zero = *(float **)0x31fc38;
    *(float *)(obs + 0x88) = zero[0];
    *(float *)(obs + 0x8c) = zero[1];
    *(float *)(obs + 0x90) = zero[2];
  }

  up = *(float **)0x31fc3c;
  *(float *)(obs + 0x94) = up[0];
  *(float *)(obs + 0x98) = up[1];
  *(float *)(obs + 0x9c) = up[2];

  fwd = *(float **)0x31fc44;
  *(float *)(obs + 0xa0) = fwd[0];
  *(float *)(obs + 0xa4) = fwd[1];
  *(float *)(obs + 0xa8) = fwd[2];
  *(int *)(obs + 0xac) = 0x3f5f66f3;

  csmemset(obs + 0x8, 0, 0x68);

  *(float *)(obs + 0x2c) = *(float *)(obs + 0xd0);
  *(float *)(obs + 0x30) = *(float *)(obs + 0xd4);
  *(float *)(obs + 0x34) = *(float *)(obs + 0xd8);
  *(float *)(obs + 0x38) = *(float *)(obs + 0xdc);
  *(float *)(obs + 0x3c) = *(float *)(obs + 0xe0);
  *(float *)(obs + 0x40) = *(float *)(obs + 0xe4);
  *(int *)(obs + 0x28) = *(int *)(obs + 0xcc);

  *(int *)(obs + 0x298) = 0x72616421;
  *(int *)(obs + 0x0) = 0x72616421;
  *(uint8_t *)(obs + 0x70) = 1;
  *(uint8_t *)(obs + 0x71) = 0;
}

/* Initialize observers for all 4 players. Calls observer_result_initialize
 * with ESI pointing to each player's observer data (base 0x33571c,
 * stride 0x29c). */
void observer_initialize_for_new_map(void)
{
  int16_t i;
  char *entry = (char *)0x33571c;

  for (i = 0; i < 4; i++) {
    assert_halt(i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
    observer_result_initialize(entry);
    entry += 0x29c;
  }
}

void observer_dispose_from_old_map(void)
{
}

/* Return a pointer to the observer camera result for a local player.
 * Base at 0x33571c, stride 0x29c, camera result at offset +0x74.
 * Validates the cluster index against the current BSP. */
void *observer_get_camera(unsigned __int16 local_player_index)
{
  int16_t idx = (int16_t)local_player_index;
  char *entry;

  if (idx == -1)
    return 0;

  assert_halt(idx >= 0 && idx < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  entry = (char *)0x33571c + (int)idx * 0x29c;

  if (*(int16_t *)(entry + 0x84) < -1 ||
      (int)*(int16_t *)(entry + 0x84) >=
        *(int *)((char *)scenario_get() + 0x134)) {
    display_assert("observer->result.location.cluster_index>=NONE && "
                   "observer->result.location.cluster_index<"
                   "global_structure_bsp_get()->clusters.count",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x12d, 1);
    system_exit(-1);
  }

  return (void *)(entry + 0x74);
}

/* Apply spring acceleration to observer state (0x8a660).
 * For each of 5 observer components, evaluates a cubic polynomial
 * (2*vel + t*accel*K1 + t^2*jerk*K2 + t^3*snap*K3) using the component's
 * timer. If any element exceeds its threshold, resets the timer (and any
 * other timers sharing the same value) to zero. */
void observer_apply_acceleration(int16_t local_player_index)
{
  char *observer;
  float *snap_ptr, *jerk_ptr, *accel_ptr, *vel_ptr, *output;
  float *timers, *timers_base;
  int16_t *sizes;
  float *thresholds;
  int16_t comp;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;
  snap_ptr = (float *)(observer + 0x158);
  jerk_ptr = (float *)(observer + 0x184);
  accel_ptr = (float *)(observer + 0x1b0);
  vel_ptr = (float *)(observer + 0x1dc);
  output = (float *)(observer + 0x120);
  timers_base = (float *)(observer + 0x5c);
  timers = timers_base;
  sizes = (int16_t *)0x2ee6b8;
  thresholds = (float *)0x26738c;

  for (comp = 0; comp < 5; comp++) {
    float t = *timers - *(float *)0x335718;
    int16_t size = *sizes;

    if (t <= 0.0f) {
      csmemset(output, 0, (int)size << 2);
    } else {
      float t_sq = t * t;
      float t_cu = t_sq * t;
      int16_t j;

      for (j = 0; j < size; j++) {
        float result = t_cu * snap_ptr[j] * *(float *)0x254cd0 +
                       t_sq * jerk_ptr[j] * *(float *)0x254cc8 +
                       t * accel_ptr[j] * *(float *)0x254640 + vel_ptr[j] +
                       vel_ptr[j];
        output[j] = result;

        if (result > *thresholds || result < -*thresholds) {
          int16_t k;
          float *tp = timers_base;
          for (k = 0; k < 5; k++) {
            if (k != comp && *tp == *timers)
              *tp = 0.0f;
            tp++;
          }
          *timers = 0.0f;
        }
      }
    }

    snap_ptr += size;
    jerk_ptr += size;
    output += size;
    accel_ptr += size;
    vel_ptr += size;
    timers++;
    thresholds++;
    sizes++;
  }
}

/* Integrate observer spring state (0x8a830). For each of 5 components,
 * evaluates a quartic polynomial (pos + 2*t*vel + t^2*accel*K1 + t^3*jerk*K2
 * + t^4*snap*K3) when the timer is active. When expired, either zeros the
 * output or applies a negated ratio correction from the result buffer. */
void observer_integrate(int16_t local_player_index)
{
  char *observer;
  float *result_ptr, *snap_ptr, *jerk_ptr, *accel_ptr, *vel_ptr, *pos_ptr;
  float *output, *timers;
  uint8_t *byte_flags;
  int16_t *sizes;
  float ratio;
  int count;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  ratio = (float)(*(double *)0x2573d8 / *(float *)0x335718);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;
  result_ptr = (float *)(observer + 0x260);
  snap_ptr = (float *)(observer + 0x158);
  jerk_ptr = (float *)(observer + 0x184);
  accel_ptr = (float *)(observer + 0x1b0);
  vel_ptr = (float *)(observer + 0x1dc);
  pos_ptr = (float *)(observer + 0x208);
  timers = (float *)(observer + 0x5c);
  byte_flags = (uint8_t *)(observer + 0x54);
  output = (float *)(observer + 0xe8);
  sizes = (int16_t *)0x2ee6b8;

  for (count = 5; count != 0; count--) {
    float t = *timers - *(float *)0x335718;
    int16_t size = *sizes;

    if (t <= 0.0f) {
      uint32_t mode = *(uint32_t *)(observer + 0x8);
      if ((mode & 1) && ((*byte_flags & 2) || (mode & 8))) {
        csmemset(output, 0, (int)size << 2);
      } else if ((mode & 1) && size > 0) {
        int16_t i;
        for (i = 0; i < size; i++)
          output[i] = -(ratio * result_ptr[i]);
      }
    } else {
      float t_sq = t * t;
      float t_cu = t_sq * t;
      float t_q4 = t_cu * t;
      int16_t i;

      for (i = 0; i < size; i++) {
        float v = t * vel_ptr[i];
        output[i] = v + v + t_sq * accel_ptr[i] * *(float *)0x254644 +
                    t_cu * jerk_ptr[i] * *(float *)0x2533d8 +
                    t_q4 * snap_ptr[i] * *(float *)0x254cc4 + pos_ptr[i];
      }
    }

    result_ptr += size;
    snap_ptr += size;
    jerk_ptr += size;
    accel_ptr += size;
    vel_ptr += size;
    output += size;
    pos_ptr += size;
    timers++;
    byte_flags++;
    sizes++;
  }
}

/* Compute observer velocities from current and target state (0x8ccf0).
 * Dispatches to FUN_0008c440 with pointers into the observer struct:
 * velocities at +0xc, result at +0x260, and integration state at +0xb0. */
void observer_compute_velocities(int16_t local_player_index)
{
  char *observer;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;
  FUN_0008c440(observer + 0xc, observer + 0x260, observer + 0xb0);
}

/* Update observer position timers and integration (0x8cd40).
 * Validates the player index, checks if the observer is paused (bit 0x20
 * of the byte pointed to by observer+0x4), then dispatches five internal
 * sub-update functions and clamps 5 timer floats at observer+0x5c. */
void observer_update_positions(int16_t local_player_index)
{
  int i;
  char *observer;
  float *timers;
  float val;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)(int16_t)local_player_index * 0x29c;
  timers = (float *)(observer + 0x5c);

  if ((*(unsigned char *)(*(int *)(observer + 0x4)) & 0x20) == 0) {
    observer_compute_velocities(local_player_index);
    observer_compute_accelerations(local_player_index);
    observer_apply_acceleration(local_player_index);
    observer_integrate(local_player_index);
    observer_compute_update(local_player_index);

    for (i = 5; i != 0; i--) {
      val = *timers - *(float *)0x335718;
      if (val <= *(float *)0x2533c0) {
        val = *(float *)0x2533c0;
      }
      *timers = val;
      timers++;
    }
  }
}

/* Per-tick observer update for all local players (0x8cde0).
 * Saves the frame's delta-time into the global at 0x335718, then walks
 * each of MAXIMUM_NUMBER_OF_LOCAL_PLAYERS observers (stride 0x29c from
 * 0x33571c), verifies the header/trailer OBSERVER_SIGNATURE ('!dar' =
 * 0x72616421) and that updated_for_frame is clear, marks it set, and
 * dispatches three observer sub-updates:
 *   - observer_update_command (0x8b060): copies/stages camera block from
 *     director into observer state
 *   - observer_update_positions (0x8cd40): time-dependent integration,
 *     skipped when delta_time matches the cached value at 0x2533c0
 *   - observer_update_result (0x8c4b0): derives the final observer camera
 *     result from staged and integrated state */
void observer_update(float delta_time)
{
  int16_t i;
  char *observer = (char *)0x33571c;

  *(float *)0x335718 = delta_time;

  for (i = 0; i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; i++, observer += 0x29c) {
    if (local_player_get_player_index(i) == -1)
      continue;

    if (i < 0 || i >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
      display_assert("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72, 1);
      system_exit(-1);
    }

    if (*(int *)(observer + 0x0) != 0x72616421 ||
        *(int *)(observer + 0x298) != 0x72616421) {
      display_assert("observer->header_signature==OBSERVER_SIGNATURE && "
                     "observer->trailer_signature==OBSERVER_SIGNATURE",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x108, 1);
      system_exit(-1);
    }

    if (*(char *)(observer + 0x70) != 0) {
      display_assert("!observer->updated_for_frame",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x109, 1);
      system_exit(-1);
    }

    *(char *)(observer + 0x70) = 1;

    observer_update_command(i);

    if (*(float *)0x335718 != *(float *)0x2533c0) {
      observer_update_positions(i);
    }

    observer_update_result(i);

    if (*(int *)(observer + 0x0) != 0x72616421 ||
        *(int *)(observer + 0x298) != 0x72616421) {
      display_assert("observer->header_signature==OBSERVER_SIGNATURE && "
                     "observer->trailer_signature==OBSERVER_SIGNATURE",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x117, 1);
      system_exit(-1);
    }
  }
}
