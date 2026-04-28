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

/* Copy/stage camera command block from director into observer state (0x8b060).
 * Validates the command struct (pointed to by observer+0x4): checks forward/up
 * perpendicular, position/orientation in range, velocity valid, distance/FOV/
 * timer bounded. Then adjusts 5 component timers in the command based on the
 * observer's current timers and mode bytes, and finally copies the command
 * struct (0x68 bytes) into the observer at offset +0x8. */
void observer_update_command(int16_t local_player_index)
{
  char *observer;
  char *command;
  float *timer_out;
  uint8_t *mode_bytes;
  float *obs_timers;
  int i;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;
  command = *(char **)(observer + 0x4);
  timer_out = (float *)(command + 0x54);
  mode_bytes = (uint8_t *)(command + 0x4c);
  obs_timers = (float *)(observer + 0x5c);

  if (command == NULL ||
      ((*(uint8_t *)command & 1) &&
       (!valid_real_normal3d_perpendicular((float *)(command + 0x24),
                                           (float *)(command + 0x30)) ||
        (*(uint32_t *)(command + 0x4) & 0x7f800000) == 0x7f800000 ||
        *(float *)(command + 0x4) < *(float *)0x266e98 ||
        *(float *)(command + 0x4) > *(float *)0x266e94 ||
        (*(uint32_t *)(command + 0x8) & 0x7f800000) == 0x7f800000 ||
        *(float *)(command + 0x8) < *(float *)0x266e98 ||
        *(float *)(command + 0x8) > *(float *)0x266e94 ||
        (*(uint32_t *)(command + 0xc) & 0x7f800000) == 0x7f800000 ||
        *(float *)(command + 0xc) < *(float *)0x266e98 ||
        *(float *)(command + 0xc) > *(float *)0x266e94 ||
        (*(uint32_t *)(command + 0x10) & 0x7f800000) == 0x7f800000 ||
        *(float *)(command + 0x10) < *(float *)0x266e98 ||
        *(float *)(command + 0x10) > *(float *)0x266e94 ||
        (*(uint32_t *)(command + 0x14) & 0x7f800000) == 0x7f800000 ||
        *(float *)(command + 0x14) < *(float *)0x266e98 ||
        *(float *)(command + 0x14) > *(float *)0x266e94 ||
        (*(uint32_t *)(command + 0x18) & 0x7f800000) == 0x7f800000 ||
        *(float *)(command + 0x18) < *(float *)0x266e98 ||
        *(float *)(command + 0x18) > *(float *)0x266e94 ||
        !real_vector3d_valid((float *)(command + 0x3c)) ||
        (*(uint32_t *)(command + 0x1c) & 0x7f800000) == 0x7f800000 ||
        *(float *)(command + 0x1c) < *(float *)0x2533c0 ||
        *(float *)(command + 0x1c) > *(float *)0x266e94 ||
        (*(uint32_t *)(command + 0x20) & 0x7f800000) == 0x7f800000 ||
        *(float *)(command + 0x20) < *(float *)0x255ef8 ||
        *(float *)(command + 0x20) > *(float *)0x2568bc ||
        (*(uint32_t *)(command + 0x48) & 0x7f800000) == 0x7f800000 ||
        *(float *)(command + 0x48) < *(float *)0x2533c0 ||
        *(float *)(command + 0x48) > *(float *)0x266e90))) {
    char *msg = csprintf(
      (char *)0x5ab100,
      "Invalid camera command.\n"
      "F: (%f, %f, %f) U: (%f, %f, %f)\n"
      "P: (%f, %f, %f) O: (%f, %f, %f)\n"
      "D: %f V: (%f, %f, %f), FOV: %f, T: %f, FL: %ld",
      (double)*(float *)(command + 0x24), (double)*(float *)(command + 0x28),
      (double)*(float *)(command + 0x2c), (double)*(float *)(command + 0x30),
      (double)*(float *)(command + 0x34), (double)*(float *)(command + 0x38),
      (double)*(float *)(command + 0x04), (double)*(float *)(command + 0x08),
      (double)*(float *)(command + 0x0c), (double)*(float *)(command + 0x10),
      (double)*(float *)(command + 0x14), (double)*(float *)(command + 0x18),
      (double)*(float *)(command + 0x1c), (double)*(float *)(command + 0x3c),
      (double)*(float *)(command + 0x40), (double)*(float *)(command + 0x44),
      (double)*(float *)(command + 0x20), (double)*(float *)(command + 0x48),
      *(uint32_t *)command);
    display_assert(msg, "c:\\halo\\SOURCE\\camera\\observer.c", 0x172, 1);
    system_exit(-1);
  }

  if (*(uint8_t *)(*(char **)(observer + 0x4)) & 1) {
    for (i = 5; i != 0; i--) {
      if ((*mode_bytes & 1) == 0) {
        command = *(char **)(observer + 0x4);
        if (*(float *)(command + 0x48) < *obs_timers &&
            (*(uint8_t *)command & 8) == 0) {
          if (*obs_timers <= *(float *)0x253f40)
            *timer_out = *obs_timers;
          else
            *timer_out = *(float *)0x253f40;
        } else {
          *timer_out = *(float *)(command + 0x48);
        }
      } else if ((*mode_bytes & 2) == 0 && *timer_out < *obs_timers) {
        if (*obs_timers <= *(float *)0x253f40)
          *timer_out = *obs_timers;
        else
          *timer_out = *(float *)0x253f40;
      }

      timer_out++;
      obs_timers++;
      mode_bytes++;
    }

    {
      uint32_t *src = (uint32_t *)*(char **)(observer + 0x4);
      uint32_t *dst = (uint32_t *)(observer + 0x8);
      for (i = 0x1a; i != 0; i--)
        *dst++ = *src++;
    }
  }
}

/* Compute quintic Hermite acceleration coefficients for observer interpolation
 * (0x8b470). Validates the observer command state (forward/up perpendicular,
 * position/orientation in range, velocity valid, distance/FOV/timer bounded).
 * When mode bit 0 is set and timer > delta_time, computes snap/jerk/accel/vel/
 * pos/extra polynomial coefficients for each of 5 observer components.
 * Component 0 receives additional velocity-dependent correction terms. */
void observer_compute_accelerations(int16_t local_player_index)
{
  char *observer;
  char *mode_ptr;
  float *snap_ptr, *jerk_ptr, *accel_ptr, *vel_ptr, *pos_ptr, *extra_ptr;
  float *accel_out_ptr, *vel_out_ptr, *result_ptr;
  float *timers;
  int16_t comp;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;

  snap_ptr = (float *)(observer + 0x158);
  accel_ptr = (float *)(observer + 0x1b0);
  pos_ptr = (float *)(observer + 0x208);
  jerk_ptr = (float *)(observer + 0x184);
  timers = (float *)(observer + 0x5c);
  vel_ptr = (float *)(observer + 0x1dc);
  extra_ptr = (float *)(observer + 0x234);
  vel_out_ptr = (float *)(observer + 0xe8);
  accel_out_ptr = (float *)(observer + 0x120);
  result_ptr = (float *)(observer + 0x260);

  mode_ptr = observer + 0x8;

  /* Validate observer command state when mode bit 0 is set */
  if (mode_ptr == NULL ||
      ((*(uint8_t *)mode_ptr & 1) &&
       (!valid_real_normal3d_perpendicular((float *)(observer + 0x2c),
                                           (float *)(observer + 0x38)) ||
        (*(uint32_t *)(observer + 0xc) & 0x7f800000) == 0x7f800000 ||
        *(float *)(observer + 0xc) < *(float *)0x266e98 ||
        *(float *)(observer + 0xc) > *(float *)0x266e94 ||
        (*(uint32_t *)(observer + 0x10) & 0x7f800000) == 0x7f800000 ||
        *(float *)(observer + 0x10) < *(float *)0x266e98 ||
        *(float *)(observer + 0x10) > *(float *)0x266e94 ||
        (*(uint32_t *)(observer + 0x14) & 0x7f800000) == 0x7f800000 ||
        *(float *)(observer + 0x14) < *(float *)0x266e98 ||
        *(float *)(observer + 0x14) > *(float *)0x266e94 ||
        (*(uint32_t *)(observer + 0x18) & 0x7f800000) == 0x7f800000 ||
        *(float *)(observer + 0x18) < *(float *)0x266e98 ||
        *(float *)(observer + 0x18) > *(float *)0x266e94 ||
        (*(uint32_t *)(observer + 0x1c) & 0x7f800000) == 0x7f800000 ||
        *(float *)(observer + 0x1c) < *(float *)0x266e98 ||
        *(float *)(observer + 0x1c) > *(float *)0x266e94 ||
        (*(uint32_t *)(observer + 0x20) & 0x7f800000) == 0x7f800000 ||
        *(float *)(observer + 0x20) < *(float *)0x266e98 ||
        *(float *)(observer + 0x20) > *(float *)0x266e94 ||
        !real_vector3d_valid((float *)(observer + 0x44)) ||
        (*(uint32_t *)(observer + 0x24) & 0x7f800000) == 0x7f800000 ||
        *(float *)(observer + 0x24) < *(float *)0x2533c0 ||
        *(float *)(observer + 0x24) > *(float *)0x266e94 ||
        (*(uint32_t *)(observer + 0x28) & 0x7f800000) == 0x7f800000 ||
        *(float *)(observer + 0x28) < *(float *)0x255ef8 ||
        *(float *)(observer + 0x28) > *(float *)0x2568bc ||
        (*(uint32_t *)(observer + 0x50) & 0x7f800000) == 0x7f800000 ||
        *(float *)(observer + 0x50) < *(float *)0x2533c0 ||
        *(float *)(observer + 0x50) > *(float *)0x266e90))) {
    char *msg = csprintf(
      (char *)0x5ab100,
      "Invalid camera command.\n"
      "F: (%f, %f, %f) U: (%f, %f, %f)\n"
      "P: (%f, %f, %f) O: (%f, %f, %f)\n"
      "D: %f V: (%f, %f, %f), FOV: %f, T: %f, FL: %ld",
      (double)*(float *)(observer + 0x2c), (double)*(float *)(observer + 0x30),
      (double)*(float *)(observer + 0x34), (double)*(float *)(observer + 0x38),
      (double)*(float *)(observer + 0x3c), (double)*(float *)(observer + 0x40),
      (double)*(float *)(observer + 0x0c), (double)*(float *)(observer + 0x10),
      (double)*(float *)(observer + 0x14), (double)*(float *)(observer + 0x18),
      (double)*(float *)(observer + 0x1c), (double)*(float *)(observer + 0x20),
      (double)*(float *)(observer + 0x24), (double)*(float *)(observer + 0x44),
      (double)*(float *)(observer + 0x48), (double)*(float *)(observer + 0x4c),
      (double)*(float *)(observer + 0x28), (double)*(float *)(observer + 0x50),
      *(uint32_t *)(observer + 0x8));
    display_assert(msg, "c:\\halo\\SOURCE\\camera\\observer.c", 0x1f6, 1);
    system_exit(-1);
  }

  /* Compute polynomial coefficients for each of 5 components */
  for (comp = 0; comp < 5; comp++) {
    if ((*(uint8_t *)(observer + 0x8) & 1) && *(float *)0x335718 < *timers) {
      float f = 1.0f / *timers;
      float f2 = f * f;
      float f3 = f2 * f;
      float f4 = f3 * f;
      int16_t size = ((int16_t *)0x2ee6b8)[comp];
      int16_t j;

      for (j = 0; j < size; j++) {
        int idx = (int)j;
        int off = idx * 4;

        *(float *)((char *)snap_ptr + off) =
          f3 * *(float *)((char *)accel_out_ptr + off) * *(float *)0x253398 -
          (f4 * *(float *)((char *)vel_out_ptr + off) * *(float *)0x254644 +
           f4 * f * *(float *)((char *)result_ptr + off) * *(float *)0x254640);

        *(float *)((char *)jerk_ptr + off) =
          f3 * *(float *)((char *)vel_out_ptr + off) * *(float *)0x2548f4 +
          f4 * *(float *)((char *)result_ptr + off) * *(float *)0x254cc0 -
          f2 * *(float *)((char *)accel_out_ptr + off);

        *(float *)((char *)accel_ptr + off) =
          f * *(float *)((char *)accel_out_ptr + off) * *(float *)0x253398 -
          (f2 * *(float *)((char *)vel_out_ptr + off) * *(float *)0x2533d8 +
           f3 * *(float *)((char *)result_ptr + off) * *(float *)0x253f34);

        *(int *)((char *)vel_ptr + off) = 0;
        *(int *)((char *)pos_ptr + off) = 0;
        *(int *)((char *)extra_ptr + off) = *(int *)((char *)result_ptr + off);

        if (comp == 0) {
          float fv = *(float *)(observer + 0x44 + off) * *(float *)0x253394;
          *(float *)((char *)snap_ptr + off) -= f4 * fv * *(float *)0x254644;
          *(float *)((char *)jerk_ptr + off) += f3 * fv * *(float *)0x253f78;
          *(float *)((char *)accel_ptr + off) -= f2 * fv * *(float *)0x254640;
          *(float *)((char *)pos_ptr + off) += fv;
        }
      }
    }

    {
      int size = (int)((int16_t *)0x2ee6b8)[comp] * 4;
      snap_ptr = (float *)((char *)snap_ptr + size);
      jerk_ptr = (float *)((char *)jerk_ptr + size);
      accel_ptr = (float *)((char *)accel_ptr + size);
      vel_ptr = (float *)((char *)vel_ptr + size);
      pos_ptr = (float *)((char *)pos_ptr + size);
      extra_ptr = (float *)((char *)extra_ptr + size);
      vel_out_ptr = (float *)((char *)vel_out_ptr + size);
      accel_out_ptr = (float *)((char *)accel_out_ptr + size);
      result_ptr = (float *)((char *)result_ptr + size);
      timers++;
    }
  }
}

/* Apply observer polynomial update and orthogonalize result vectors (0x8ba10).
 * For each of 5 observer components, validates velocities (assert_valid_real),
 * then either copies defaults (when timer expired and mode active), evaluates
 * a quintic polynomial (when timer active), or negates velocity*delta_time
 * (when timer expired but mode not active). For the last component (index 4,
 * forward/up vectors), applies axis-angle rotation via
 * rotate_vector3d_by_sincos instead of simple addition. After the loop,
 * orthogonalizes the forward/up vectors via Gram-Schmidt if they are no longer
 * orthonormal. */
void observer_compute_update(int16_t local_player_index)
{
  char *observer;
  char *result_ptr;
  char *default_ptr;
  char *snap_ptr;
  char *jerk_ptr;
  char *accel_ptr;
  char *vel_ptr;
  float *velocities;
  char *pos_ptr;
  char *extra_ptr;
  float *timers;
  float scratch[14];
  float *scratch_ptr;
  int16_t comp;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;

  result_ptr = observer + 0xb0;
  default_ptr = observer + 0x0c;
  snap_ptr = observer + 0x158;
  jerk_ptr = observer + 0x184;
  accel_ptr = observer + 0x1b0;
  vel_ptr = observer + 0x1dc;
  velocities = (float *)(observer + 0xe8);
  pos_ptr = observer + 0x208;
  extra_ptr = observer + 0x234;
  timers = (float *)(observer + 0x5c);
  scratch_ptr = scratch;

  /* Validate all 11 velocity floats */
  {
    float *vp = velocities;
    int count = 0xb;
    do {
      if ((*(uint32_t *)vp & 0x7f800000u) == 0x7f800000u) {
        char *msg =
          csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
                   "observer->velocities.n[parameter_index]", *(uint32_t *)vp,
                   (double)*vp);
        display_assert(msg, "c:\\halo\\SOURCE\\camera\\observer.c", 0x2f4, 1);
        system_exit(-1);
      }
      vp++;
      count--;
    } while (count != 0);
  }

  for (comp = 0; comp < 5; comp++) {
    float elapsed = *timers - *(float *)0x335718;

    if (elapsed <= *(float *)0x2533c0 && (*(uint8_t *)(observer + 0x8) & 1)) {
      /* Timer expired and mode active: copy defaults */
      int16_t j = 0;
      if (j < ((int16_t *)0x2ee6ac)[comp]) {
        do {
          *(uint32_t *)(result_ptr + j * 4) =
            *(uint32_t *)(default_ptr + j * 4);
          j++;
        } while (j < ((int16_t *)0x2ee6ac)[comp]);
      }
    } else {
      /* Compute update values */
      if (elapsed <= *(float *)0x2533c0) {
        /* Timer expired, mode not active: negate velocity*delta_time */
        int16_t j = 0;
        if (j < ((int16_t *)0x2ee6b8)[comp]) {
          do {
            scratch_ptr[j] = -(*(float *)0x335718 * velocities[j]);
            j++;
          } while (j < ((int16_t *)0x2ee6b8)[comp]);
        }
      } else {
        /* Timer active: evaluate quintic polynomial */
        float t2 = elapsed * elapsed;
        float t3 = t2 * elapsed;
        float t4 = t3 * elapsed;
        float t5 = t4 * elapsed;
        int16_t j = 0;

        if (j < ((int16_t *)0x2ee6b8)[comp]) {
          do {
            int idx = (int)j;
            int off = idx * 4;
            j++;
            scratch_ptr[idx] = t5 * *(float *)(snap_ptr + off) +
                               t4 * *(float *)(jerk_ptr + off) +
                               t3 * *(float *)(accel_ptr + off) +
                               t2 * *(float *)(vel_ptr + off) +
                               elapsed * *(float *)(pos_ptr + off) +
                               *(float *)(extra_ptr + off);
          } while (j < ((int16_t *)0x2ee6b8)[comp]);
        }
      }

      if (comp < 4) {
        /* Components 0-3: add scratch to result */
        int16_t j = 0;
        if (j < ((int16_t *)0x2ee6b8)[comp]) {
          do {
            int idx = (int)j;
            int off = idx * 4;
            j++;
            *(float *)(result_ptr + off) =
              scratch_ptr[idx] + *(float *)(result_ptr + off);
          } while (j < ((int16_t *)0x2ee6b8)[comp]);
        }
      } else {
        /* Component 4 (forward/up vectors): axis-angle rotation */
        float axis[3];
        float mag;

        axis[0] = scratch_ptr[0];
        axis[1] = scratch_ptr[1];
        axis[2] = scratch_ptr[2];
        mag = sqrtf(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);

        if (fabsf(mag) >= (float)*(double *)0x2533d0) {
          float inv_mag = *(float *)0x2533c8 / mag;
          axis[0] = axis[0] * inv_mag;
          axis[1] = axis[1] * inv_mag;
          axis[2] = axis[2] * inv_mag;

          if (mag != *(float *)0x2533c0) {
            float sin_val = sinf(mag);
            float cos_val = cosf(mag);
            rotate_vector3d_by_sincos((float *)result_ptr, axis, sin_val,
                                      cos_val);
            rotate_vector3d_by_sincos((float *)(result_ptr + 0xc), axis,
                                      sin_val, cos_val);
          }
        }
      }
    }

    {
      int result_advance = (int)((int16_t *)0x2ee6ac)[comp] * 4;
      int vel_advance = (int)((int16_t *)0x2ee6b8)[comp] * 4;
      default_ptr += result_advance;
      result_ptr += result_advance;
      velocities += (int)((int16_t *)0x2ee6b8)[comp];
      snap_ptr += vel_advance;
      jerk_ptr += vel_advance;
      accel_ptr += vel_advance;
      vel_ptr += vel_advance;
      scratch_ptr += (int)((int16_t *)0x2ee6b8)[comp];
      pos_ptr += vel_advance;
      extra_ptr += vel_advance;
      timers++;
    }
  }

  /* Orthogonalize forward/up vectors if needed */
  {
    float *up = (float *)(observer + 0xd0);
    float *fwd = (float *)(observer + 0xdc);
    float check;

    /* Check if up is unit length */
    check =
      (up[0] * up[0] + up[1] * up[1] + up[2] * up[2]) - *(float *)0x2533c8;
    if ((*(uint32_t *)&check & 0x7f800000u) == 0x7f800000u ||
        fabsf(check) >= (float)*(double *)0x2549d8) {
      goto orthogonalize;
    }

    /* Check if forward is unit length */
    check = (fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]) -
            *(float *)0x2533c8;
    if ((*(uint32_t *)&check & 0x7f800000u) == 0x7f800000u ||
        fabsf(check) >= (float)*(double *)0x2549d8) {
      goto orthogonalize;
    }

    /* Check if up and forward are perpendicular */
    check = up[2] * fwd[2] + up[0] * fwd[0] + fwd[1] * up[1];
    if ((*(uint32_t *)&check & 0x7f800000u) == 0x7f800000u ||
        fabsf(check) >= (float)*(double *)0x2549d8) {
      goto orthogonalize;
    }

    return;

  orthogonalize: {
    float right[3];
    float mag;

    /* right = cross(fwd, up) */
    right[0] = up[2] * fwd[1] - fwd[2] * up[1];
    right[1] = up[0] * fwd[2] - up[2] * fwd[0];
    right[2] = up[1] * fwd[0] - up[0] * fwd[1];

    /* fwd = cross(up, right) */
    fwd[0] = right[2] * up[1] - right[1] * up[2];
    fwd[1] = right[0] * up[2] - right[2] * up[0];
    fwd[2] = right[1] * up[0] - right[0] * up[1];

    /* Normalize up */
    mag = sqrtf(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
    if (fabsf(mag) >= (float)*(double *)0x2533d0) {
      float inv = *(float *)0x2533c8 / mag;
      up[0] = inv * up[0];
      up[1] = inv * up[1];
      up[2] = inv * up[2];
    }

    /* Normalize forward */
    mag = sqrtf(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
    if (fabsf(mag) >= (float)*(double *)0x2533d0) {
      float inv = *(float *)0x2533c8 / mag;
      fwd[0] = inv * fwd[0];
      fwd[1] = inv * fwd[1];
      fwd[2] = inv * fwd[2];
    }
  }
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
