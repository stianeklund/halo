void collision_log_initialize(void)
{
  csmemset((void *)0x5a5e40, 0, 0x2298);
  assert_halt(*(int16_t *)0x4761d8 < 0x20);
  *(int16_t *)(0x5a8c80 + *(int16_t *)0x4761d8 * 2) = 0;
  *(int16_t *)0x4761d8 = *(int16_t *)0x4761d8 + 1;
}

/* Helper shared by begin/continue_period. Asserts no period is active,
 * validates time_period is in [0,3), zeros the scratch buffer at 0x5a80e0,
 * and sets the current period. time_period passed in ESI. */
void collision_log_period_helper(int time_period /* @<esi> */, int begin_flag)
{
  int16_t period = (int16_t)time_period;

  if (*(int16_t *)0x325058 != -1) {
    display_assert("collision_usage_current_period == NONE",
                   "c:\\halo\\SOURCE\\physics\\collision_usage.c", 0xa7, 1);
    system_exit(-1);
  }
  if (period < 0 || period >= 3) {
    display_assert(
      "(time_period >= 0) && (time_period < NUMBER_OF_COLLISION_TIME_PERIODS)",
      "c:\\halo\\SOURCE\\physics\\collision_usage.c", 0xa8, 1);
    system_exit(-1);
  }
  csmemset((void *)0x5a80e0, 0, 0xb88);
  *(int16_t *)0x325058 = period;
}

void collision_log_begin_period(__int16 time_period)
{
  collision_log_period_helper((int)time_period, 1);
}

void collision_log_continue_period(__int16 time_period)
{
  collision_log_period_helper((int)time_period, 0);
}

void collision_log_end_period(void)
{
  int16_t period;
  int i;
  int *src;
  int *dst;

  period = *(int16_t *)0x325058;
  assert_halt(period >= 0 && period < 3);
  *(char *)0x5a80e0 = 1;
  src = (int *)0x5a80e0;
  dst = (int *)(0x5a5e40 + (int)period * 0xb88);
  for (i = 0x2e2; i != 0; i--) {
    *dst++ = *src++;
  }
  *(int16_t *)0x325058 = -1;
}

/* 0x14d840 — returns the current collision user if collision logging is
 * active, or -1 if logging is disabled / not applicable.
 * Validates depth > 0, user in [0,22), collision_function in [0,8),
 * game in progress, not in editor, logging enabled, and period set. */
short FUN_0014d840(short collision_function /* @<edi> */)
{
  short user;

  assert_halt(*(int16_t *)0x4761d8 > 0);

  user = *(int16_t *)(0x5a8c7e + *(int16_t *)0x4761d8 * 2);
  assert_halt(user >= 0 && user < 0x16);
  assert_halt(collision_function >= 0 && collision_function < 8);

  if (!game_in_progress())
    return -1;
  if (game_in_editor())
    return -1;
  if (*(char *)0x325054 == 0)
    return -1;
  if (*(int16_t *)0x325058 == -1)
    return -1;

  assert_halt(*(int16_t *)0x325058 >= 0 && *(int16_t *)0x325058 < 3);

  return user;
}

/* 0x14d940 — wraps QueryPerformanceCounter (FUN_001d33e6) */
void collision_log_query_counter(void *counter)
{
  FUN_001d33e6(counter);
}

/* 0x14d950 — records elapsed time for a collision function type.
 * Computes (current_time - start) as 64-bit and accumulates into
 * per-type and per-user counters in the scratch buffer at 0x5a80e0. */
void collision_log_add_time(short collision_function, unsigned int start_lo,
                            int start_hi)
{
  unsigned int current[2];
  short user;
  unsigned int elapsed_lo;
  int elapsed_hi;
  int type_off;
  int user_off;
  unsigned int prev;

  FUN_001d33e6(current);

  user = FUN_0014d840(collision_function);
  if (user == -1)
    return;

  elapsed_lo = current[0] - start_lo;
  elapsed_hi = ((int)current[1] - start_hi) - (unsigned int)(current[0] < start_lo);

  type_off = (int)collision_function * 0x170;
  prev = *(unsigned int *)(0x5a80f0 + type_off);
  *(unsigned int *)(0x5a80f0 + type_off) = prev + elapsed_lo;
  *(int *)(0x5a80f4 + type_off) +=
    elapsed_hi + (unsigned int)((prev + elapsed_lo) < prev);

  user_off = ((int)collision_function * 0x17 + (int)user) * 0x10;
  prev = *(unsigned int *)(0x5a8100 + user_off);
  *(unsigned int *)(0x5a8100 + user_off) = prev + elapsed_lo;
  *(int *)(0x5a8104 + user_off) +=
    elapsed_hi + (unsigned int)((prev + elapsed_lo) < prev);
}

/* 0x14ec30 — Collision test against BSP surfaces and nearby objects.
 * Performs a BSP surface collision query with the given search radius,
 * optionally adds BSP collision features, then iterates over objects in
 * nearby clusters (filtered by type mask in flags bits 8-19) and tests
 * each marked object via FUN_0014ea10. Returns true if any collision
 * features were recorded (any of the 3 feature counts is nonzero). */
bool FUN_0014ec30(int flags, float *pos, float search_radius, float dist_b,
                  float dist_a, int param6, void *scratch)
{
  void *scenario;
  void *bsp;
  char *bsp_surface_data;
  char bsp_hit;
  unsigned char test_objects;
  void *cluster_block;
  void *elem;
  int16_t cluster_idx;
  int iter_state;
  int obj_handle;
  int i;

  collision_features_init(scratch);

  if ((flags & 0x20) == 0 && (flags & 0xc0) == 0)
    goto check_result;

  scenario = scenario_get();
  bsp = global_collision_bsp_get();
  test_objects = (unsigned char)((unsigned int)flags >> 7) & 1;
  if (*(char *)0x4761f8 != 0) {
    test_objects = 0;
  }

  collision_log_add_call(2);
  collision_log_query_counter((void *)0x4761e0);

  /* Adjust search radius by the global epsilon at 0x255d90 (0.0625f) */
  search_radius = search_radius + *(float *)0x255d90;

  /* Large local buffer for collision results — _chkstk allocated 0x1018 bytes */
  {
    int results_buf[0x406]; /* 0x1018 bytes */

    bsp_surface_data = breakable_surfaces_get_bsp_surface_data();
    bsp_hit = (char)FUN_001493b0((int)bsp, 0x100, (int)bsp_surface_data,
                                 (int)pos, *(int *)&search_radius,
                                 results_buf);

    if (bsp_hit && (flags & 0x20) != 0) {
      collision_features_add((int)bsp, results_buf, 0, *(int *)&dist_b,
                             *(int *)&dist_a, -1, scratch);
    }

    if (test_objects != 0 && results_buf[0x303] > 0) {
      if ((flags & 0xfff00) == 0) {
        flags = flags | 0xfff00;
      }

      structures_cluster_marker_begin();
      object_reset_markers();

      cluster_block = (void *)((char *)scenario + 0xe0);
      i = 0;
      if (results_buf[0x303] > 0) {
        do {
          elem = tag_block_get_element(
            cluster_block,
            results_buf[0x304 + i] & 0x7fffffff,
            0x10);
          cluster_idx = *(int16_t *)((char *)elem + 8);
          if (structure_cluster_mark(cluster_idx)) {
            obj_handle = cluster_partition_object_iter_first(
              &iter_state, cluster_idx);
            while (obj_handle != -1) {
              if (object_mark(obj_handle)) {
                FUN_0014ea10((unsigned int)flags, obj_handle, pos,
                             search_radius, dist_b, dist_a, param6, (int)scratch);
              }
              obj_handle = cluster_partition_object_iter_next(&iter_state);
            }
          }
          i++;
        } while ((int)(int16_t)i < results_buf[0x303]);
      }

      object_marker_end();
      FUN_00198540();
    }

    collision_log_add_time(2, *(unsigned int *)0x4761e0,
                           *(int *)0x4761e4);
  }

check_result:
  if (*(int16_t *)scratch == 0 &&
      *((int16_t *)scratch + 1) == 0 &&
      *((int16_t *)scratch + 2) == 0) {
    return 0;
  }
  return 1;
}

/* 0x14d9d0 — increments call count for a collision function type */
void collision_log_add_call(short collision_function)
{
  short user;

  user = FUN_0014d840(collision_function);
  if (user == -1)
    return;

  *(int *)(0x5a80e8 + (int)collision_function * 0x170) += 1;
  *(int *)(0x5a80f8 + ((int)collision_function * 0x17 + (int)user) * 0x10) += 1;
}
