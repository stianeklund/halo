/* 0x2a3a0 — Reset actor path/movement state. Clears the path-active flag,
 * sets is_moving to 1, and zeroes the path step counter. */
void FUN_0002a3a0(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  *(char *)(actor + 0x4a8) = 0;
  *(char *)(actor + 0x484) = 1;
  *(int *)(actor + 0x4a0) = 0;
}

/* actor_path_input_new (0x2a470) — Populate nav state for actor movement.
 *
 * Gets the actor's actr tag speed value (tag+0x8c) as the base speed.
 * If the actor is in a vehicle (actor->vehicle_count at +0x15e > 0):
 *   - Gets the vehicle object via object_get_and_verify_type(actor[0x158], 2)
 *   - Gets the vehi tag via tag_get('vehi', vehicle[0])
 *   - Overrides unit_handle with the vehicle handle (actor[0x158])
 *   - If vehi_tag[0x38c] > constant at 0x2533c0, overrides local_8 with it
 * Calls actor_find_pathfinding_location(actor_handle), then fills nav_state_out
 * via path_input_new and path_input_set_start.
 *
 * Confirmed: datum_get + tag_get('actr', actor[0x58]) at 0x2a481-0x2a491.
 * Confirmed: tag[0x8c] → local_8; actor[0x18] → unit_handle default.
 * Confirmed: object_get_and_verify_type(actor[0x158], 2) → vehicle at 0x2a4b8.
 * Confirmed: tag_get('vehi', vehicle[0]) at 0x2a4c5.
 * Confirmed: unit_handle = actor[0x158] at 0x2a4ca.
 * Confirmed: FPU FCOMP [0x2533c0] with TEST AH,0x41 at 0x2a4db.
 * Confirmed: actor_find_pathfinding_location(actor_handle) at 0x2a4f2.
 * Confirmed: path_input_new(nav, local_8, actor[0x376], unit) at 0x2a509.
 * Confirmed: path_input_set_start(nav, actor+0x168, actor[0x164]) at 0x2a51d.
 */
void actor_path_input_new(int actor_handle, char *nav_state_out)
{
  char *actor;
  char *p;
  int local_8;
  int unit_handle;

  actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  p = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  local_8 = *(int *)(p + 0x8c);
  unit_handle = *(int *)(actor + 0x18);
  if (*(int16_t *)(actor + 0x15e) > 0) {
    p = (char *)object_get_and_verify_type(*(int *)(actor + 0x158), 2);
    p = (char *)tag_get(0x76656869, *(int *)p);
    unit_handle = *(int *)(actor + 0x158);
    if (*(float *)(p + 0x38c) > *(float *)0x2533c0) {
      local_8 = *(int *)(p + 0x38c);
    }
  }
  actor_find_pathfinding_location(actor_handle);
  path_input_new(nav_state_out, local_8, *(unsigned char *)(actor + 0x376),
                 unit_handle);
  path_input_set_start(nav_state_out, (float *)(actor + 0x168),
                       *(int *)(actor + 0x164));
}

/* midpoint3d (0x2a540) — Compute the midpoint of two 3D vectors.
 * out[i] = (a[i] + b[i]) * 0.5f for i in {0,1,2}. */
void midpoint3d(float *a, float *b, float *out)
{
  out[0] = (a[0] + b[0]) * 0.5f;
  out[1] = (a[1] + b[1]) * 0.5f;
  out[2] = (a[2] + b[2]) * 0.5f;
}

/* 0x2a7e0 — Set actor goal destination if not already occupied.
 * Calls actor_set_dormant(actor, 0), then checks actor->goal_slot (+0x418)
 * and vehicle-in-air state. On success writes param_2 to +0x418 and
 * copies two ints from param_3 to +0x41c/+0x420. Returns 1 on success, 0 on
 * failure. */
int actor_move_animation_impulse(int actor_handle, int16_t param_2,
                                 int *param_3)
{
  char *actor;
  char *actor2;
  char result;

  result = 0;
  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  actor_set_dormant(actor_handle, 0);
  actor2 = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(int16_t *)(actor2 + 0x418) == -1) {
    if (*(int *)(actor2 + 0x18) == -1 ||
        !unit_is_busy(*(int *)(actor2 + 0x18))) {
      *(int16_t *)(actor + 0x418) = param_2;
      *(int *)(actor + 0x41c) = *param_3;
      *(int *)(actor + 0x420) = param_3[1];
      result = 1;
    }
  }
  return (int)result;
}

/* 0x2a860 — Clear actor destination and trigger flee movement.
 * Returns 0 if actor has a goal slot, is in a flying vehicle, or
 * actor_action_deny_transition returns true. Otherwise zeroes the swarm flag,
 * copies 12 bytes from the global pointer at 0x31fc38, calls
 * actor_unit_control_stop_animation_impulse, and returns 1. */
int actor_move_force_stop(int actor_handle)
{
  char *actor;
  char *ptr;
  char result;

  result = 0;
  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(int16_t *)(actor + 0x418) == -1) {
    if (*(int *)(actor + 0x18) == -1 || !unit_is_busy(*(int *)(actor + 0x18))) {
      if (!actor_action_deny_transition(actor_handle)) {
        *(char *)(actor + 0x504) = 0;
        ptr = (char *)*(int *)0x31fc38;
        *(int *)(actor + 0x6e0) = *(int *)ptr;
        *(int *)(actor + 0x6e4) = *(int *)(ptr + 4);
        *(int *)(actor + 0x6e8) = *(int *)(ptr + 8);
        actor_unit_control_stop_animation_impulse(actor_handle);
        result = 1;
      }
    }
  }
  return (int)result;
}

/* 0x2b5d0 — actor_move_get_avoidance_direction: initialize trigonometric lookup
 * tables.
 *
 * Confirmed: no arguments, no calls, writes table blocks rooted at
 * 0x6327e0 and 0x6325c0 using constants/tables at 0x25577c..0x25581c.
 * Confirmed: first loop runs 9 iterations (EDX=9), stride 0x1c (7 floats).
 * Confirmed: second stage runs 2 outer iterations × 8 inner iterations,
 * with destination stride 0x38 (14 floats) per inner iteration.
 */
void actor_move_get_avoidance_direction(void)
{
  float(*table_a)[7] = (float(*)[7])0x6327e0;
  float(*table_b)[7] = (float(*)[7])0x6325c0;
  float(*basis)[3] = (float(*)[3])0x632780;

  const float *angle_table_9 = (const float *)0x2557cc;
  const float *scale_table_9 = (const float *)0x2557a8;
  const float *length_table_9 = (const float *)0x255784;
  const float k_angle = *(const float *)0x255780;
  const float k_length = *(const float *)0x25577c;
  const float k_base = *(const float *)0x255778;

  const float *outer_angles = (const float *)0x25581c;
  const float *outer_scales = (const float *)0x255814;
  const float *inner_angles = (const float *)0x2557f4;
  const float k_inner_base = *(const float *)0x2557f0;

  int i;
  int row;
  int col;
  float angle, sin_angle, cos_angle, scaled_angle, sin_scaled, scaled_len;
  float sin_outer, cos_outer, row_scale;
  float inner, cos_inner, sin_inner;
  int index;

  for (i = 0; i < 9; i++) {
    angle = angle_table_9[i];
    sin_angle = sinf(angle);
    cos_angle = cosf(angle);
    scaled_angle = k_angle * scale_table_9[i];
    sin_scaled = sinf(scaled_angle);
    scaled_len = k_length * length_table_9[i];

    table_a[i][0] = k_base;
    table_a[i][1] = 0.0f;
    table_a[i][2] = scaled_len * cos_angle;
    table_a[i][3] = scaled_len * sin_angle;
    table_a[i][4] = cosf(scaled_len);
    table_a[i][5] = sin_scaled * scaled_angle;
    table_a[i][6] = sin_scaled * sin_angle;
  }

  for (row = 0; row < 2; row++) {
    sin_outer = sinf(outer_angles[row]);
    cos_outer = cosf(outer_angles[row]);
    row_scale = outer_scales[row];

    for (col = 0; col < 8; col++) {
      inner = inner_angles[col];
      cos_inner = cosf(inner);
      sin_inner = sinf(inner);
      index = row * 8 + col;

      basis[index][0] = 0.0f;
      basis[index][1] = cos_inner;
      basis[index][2] = sin_inner;

      table_b[index][0] = k_inner_base;
      table_b[index][1] = row_scale * basis[index][0];
      table_b[index][2] = row_scale * basis[index][1];
      table_b[index][3] = row_scale * basis[index][2];
      table_b[index][4] = cos_outer;
      table_b[index][5] = sin_outer * basis[index][1];
      table_b[index][6] = sin_outer * basis[index][2];
    }
  }
}

/* actor_path_3d_available (0x2b720) — Check if vehicle actor should brake.
 *
 * If actor is in a type-4 vehicle state (actor[0x15e] == 4):
 *   - Reads vehicle tag stopping distance (vehi_tag[0x388])
 *   - If stopping distance > 0 and actor's speed factor (actor[0x5ec]) >
 * threshold:
 *     - Computes delta vector from actor position to dest_pos
 *     - Normalizes delta (getting distance)
 *     - If distance > 0 and dot(normalized_delta, facing) > threshold:
 *       returns 0 (should brake)
 * Writes stopping distance to *dist_out if non-NULL.
 * Returns 1 (don't brake) by default.
 *
 * Confirmed: datum_get at 0x2b733. BL=1 default at 0x2b74c.
 * Confirmed: CMP word [ESI+0x15e],4 at 0x2b73d.
 * Confirmed: object_get_and_verify_type(actor[0x158], 2) at 0x2b75d.
 * Confirmed: tag_get('vehi', vehicle[0]) at 0x2b76a.
 * Confirmed: vehi[0x388] → local_8 at 0x2b76f.
 * Confirmed: FCOMP [0x2533c0] checks at 0x2b77e, 0x2b7d1.
 * Confirmed: FCOMP [0x2555d0] speed check at 0x2b795.
 * Confirmed: normalize3d(&delta) at 0x2b7cc.
 * Confirmed: dot product z*fz + y*fy + x*fx at 0x2b7e1-0x2b7fe.
 * Confirmed: FCOMP [0x253d54] dot threshold at 0x2b800.
 * Confirmed: dist_out write if non-NULL at 0x2b80f-0x2b819.
 */
char actor_path_3d_available(int actor_handle, float *dest_pos, float *dist_out)
{
  char *actor;
  char *vehi;
  float local_8;
  float delta[3];
  char result;

  actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  local_8 = 0.0f;
  result = 1;
  if (*(int16_t *)(actor + 0x15e) == 4) {
    vehi = (char *)object_get_and_verify_type(*(int *)(actor + 0x158), 2);
    vehi = (char *)tag_get(0x76656869, *(int *)vehi);
    local_8 = *(float *)(vehi + 0x388);
    if (local_8 > *(float *)0x2533c0 &&
        *(float *)(actor + 0x5ec) > *(float *)0x2555d0) {
      delta[0] = dest_pos[0] - *(float *)(actor + 0x12c);
      delta[1] = dest_pos[1] - *(float *)(actor + 0x130);
      delta[2] = dest_pos[2] - *(float *)(actor + 0x134);
      if (normalize3d(delta) > *(float *)0x2533c0 &&
          delta[0] * *(float *)(actor + 0x174) +
              delta[1] * *(float *)(actor + 0x178) +
              delta[2] * *(float *)(actor + 0x17c) >
            *(float *)0x253d54) {
        result = 0;
      }
    }
  }
  if (dist_out != (float *)0) {
    *dist_out = local_8;
  }
  return result;
}

/*
 * 0x2cdb0 — actor_path_refresh: Compute and populate the actor's path control
 * state for the current movement mode.
 *
 * This function is the per-tick "where should I go?" resolver for actors. It
 * reads the actor's movement source type (actor[0x46c]) to determine how to
 * fill the actor's destination fields (actor[0x488..0x494]) and navigation
 * state (actor[0x4a8]). After resolving the target, it initiates pathfinding
 * and sets actor[0x4a4]=1 when successful.
 *
 * Arguments:
 *   actor_handle   — datum handle identifying the actor.
 *   store_distance — if non-zero, writes the computed 3D distance to the
 *                    destination into actor[0x4a0].
 *   override_path  — if non-NULL (and actor is not mounted), use this
 *                    pre-computed path instead of computing a new one.
 *
 * Returns 1 if pathfinding succeeded (or a target was found), 0 on failure.
 *
 * Movement source types (actor[0x46c]):
 *   0 — none / disabled (early-return, mark ready).
 *   1 — disabled variant (same early-return).
 *   2 — absolute world-space position stored in actor[0x470..0x47c].
 *   3 — AI squad order position (scenario squads block).
 *   4 — encounter squad order (scenario encounter/squad/order blocks).
 *   5 — prop (perception object) position (from prop datum at actor[0x470]).
 *
 * Confirmed: cdecl, 3 args, char return.
 * Confirmed: ESI=actor ptr, EDI=&actor[0x488] after switch cases.
 * Confirmed: BL carries the function return value (0 or 1).
 * Confirmed float constants: 0.0f at 0x2533c0, threshold at 0x255d1c,
 *   threshold2 at 0x253398.
 */
char actor_path_refresh(int actor_handle, char store_distance,
                        void *override_path)
{
  /* All C89 declarations at top of function scope. */
  char *actor;
  short move_src;
  char had_path;
  char path_found;
  char path_found2;
  float saved_pos[3]; /* [EBP-0x18..-0x10]: copy of old actor[0x488..0x490] */
  char *tag; /* [EBP-0xc]: actor tag pointer from tag_get */
  float dist; /* [EBP-0x8]: 3D distance actor→destination */
  char local_nav[44]; /* [EBP-0x60]: nav-state struct (waypoint init output) */
  static char
    large_buf[0x1408c]; /* [EBP+0xfffebf14]: path-build scratch 82060 bytes */
  void
    *path_state; /* allocated path cache slot from ai_debug_get_path_storage */
  int scenario;
  int squad_elem;
  int order_elem;
  int order_elem2;
  short order_idx;
  int prop;
  int game_tick;
  unsigned int actor_handle_u;
  int ai_idx;
  float dist_sq_saved;

  /* datum_get confirmed at 0x0002cdcb: PUSH EAX(actor_handle), PUSH
   * ECX(0x6325a4) */
  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  move_src = *(short *)(actor + 0x46c);
  had_path = 0;

  /* If move_src != 0 and != 1, save old destination and set had_path. */
  if (move_src != 0 && move_src != 1) {
    saved_pos[0] = *(float *)(actor + 0x488);
    saved_pos[1] = *(float *)(actor + 0x48c);
    saved_pos[2] = *(float *)(actor + 0x490);
    had_path = 1;
  }

  /*
   * Early-return conditions — actor is busy, paused, or at a terminal state:
   *   actor[0x160] != 0 (some "is_doing" flag)
   *   move_src == 0 or 1 (no movement source)
   *   move_src == 3 && actor[0x3bb] != 0 (squad-order terminal condition)
   * In all cases: re-fetch actor, clear fields, set is_moving=1, return 1.
   * Confirmed at 0x0002d2fb: second datum_get, then BL (=1) is returned.
   */
  if (*(char *)(actor + 0x160) != '\0' || move_src == 0 || move_src == 1 ||
      (move_src == 3 && *(char *)(actor + 0x3bb) != '\0')) {
    /* Second datum_get at 0x0002d305 */
    actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
    *(int *)(actor + 0x4a0) = 0;
    *(char *)(actor + 0x4a8) = 0;
    *(char *)(actor + 0x484) = 1;
    return '\x01';
  }

  /* Clear navigation state fields for this tick. */
  *(char *)(actor + 0x4a8) = 0;
  *(char *)(actor + 0x484) = 0;
  *(int *)(actor + 0x4a0) = 0;
  *(char *)(actor + 0x506) = 0;

  /* Resolve destination by movement source type. */
  switch (move_src) {
  case 2:
    /*
     * Absolute position: copy actor[0x470..0x47c] directly.
     * Confirmed at 0x0002ce6f: LEA EDI,[ESI+0x488]; copy 3 dwords from
     * [ESI+0x470]; then [ESI+0x494] = [ESI+0x47c].
     */
    *(unsigned int *)(actor + 0x488) = *(unsigned int *)(actor + 0x470);
    *(unsigned int *)(actor + 0x48c) = *(unsigned int *)(actor + 0x474);
    *(unsigned int *)(actor + 0x490) = *(unsigned int *)(actor + 0x478);
    *(unsigned int *)(actor + 0x494) = *(unsigned int *)(actor + 0x47c);
    break;

  case 3:
    /*
     * Squad order position: look up the order waypoint from the scenario
     * squads block, indexed by actor[0x34] (squad handle low word).
     *
     * Disasm 0x0002cf62: tag_block_get_element chain (batch ESP cleanup
     * at 0x0002cfbb). Sequence:
     *   global_scenario_get() -> scenario+0x42c = &squads_block
     *   tag_block_get_element(&squads_block, squad_idx, 0xb0) -> squad
     *   tag_block_get_element(squad+0x98, actor[0x470], 0x18) -> order
     *   Copy order[0..8] -> actor[0x488..0x490], order[0x14] -> actor[0x494]
     */
    if (*(unsigned int *)(actor + 0x34) == 0xffffffff) {
      goto LAB_fail;
    }
    ai_idx = (int)(*(unsigned int *)(actor + 0x34) & 0xffff);
    scenario = (int)global_scenario_get();
    squad_elem =
      (int)tag_block_get_element((void *)(scenario + 0x42c), ai_idx, 0xb0);
    order_elem = (int)tag_block_get_element(
      (void *)(squad_elem + 0x98), (int)(short)*(short *)(actor + 0x470), 0x18);
    *(unsigned int *)(actor + 0x488) = *(unsigned int *)(order_elem + 0);
    *(unsigned int *)(actor + 0x48c) = *(unsigned int *)(order_elem + 4);
    *(unsigned int *)(actor + 0x490) = *(unsigned int *)(order_elem + 8);
    *(unsigned int *)(actor + 0x494) = *(unsigned int *)(order_elem + 0x14);
    break;

  case 4:
    /*
     * Encounter order position: look up in scenario encounters ->
     * squads -> orders, indexed by actor[0x34] (encounter handle low
     * word), actor[0x3a] (squad index), actor[0x470] (order index).
     *
     * Disasm 0x0002cec7-0x0002cf5d: same ESP batch pattern.
     * actor[0x494] = order_entry[0x4c] (facing handle).
     */
    if (*(unsigned int *)(actor + 0x34) == 0xffffffff) {
      goto LAB_fail;
    }
    ai_idx = (int)(*(unsigned int *)(actor + 0x34) & 0xffff);
    scenario = (int)global_scenario_get();
    squad_elem =
      (int)tag_block_get_element((void *)(scenario + 0x42c), ai_idx, 0xb0);
    order_elem = (int)tag_block_get_element(
      (void *)(squad_elem + 0x80), (int)(short)*(short *)(actor + 0x3a), 0xe8);
    order_idx = *(short *)(actor + 0x470);
    if (order_idx < 0) {
      goto LAB_fail;
    }
    if ((int)order_idx >= *(int *)(order_elem + 0xc4)) {
      goto LAB_fail;
    }
    order_elem2 = (int)tag_block_get_element((void *)(order_elem + 0xc4),
                                             (int)order_idx, 0x50);
    *(unsigned int *)(actor + 0x488) = *(unsigned int *)(order_elem2 + 0);
    *(unsigned int *)(actor + 0x48c) = *(unsigned int *)(order_elem2 + 4);
    *(unsigned int *)(actor + 0x490) = *(unsigned int *)(order_elem2 + 8);
    *(unsigned int *)(actor + 0x494) = *(unsigned int *)(order_elem2 + 0x4c);
    break;

  case 5:
    /*
     * Prop position: actor[0x470] is a prop datum handle. Fetch the prop
     * from prop_data (DAT_005ab23c). Validate it is in a valid-prop state
     * (prop[0x24] in [4,5]), then copy position fields.
     *
     * actor[0x99] selects between two prop position fields:
     *   ==0: prop[0xf0..0xf8] (normal position)
     *   !=0: prop[0xc8..0xd0] (vehicle/mounted position)
     * actor[0x494] = prop[0xec] (velocity handle).
     * actor[0x498] = actor[0x474] (facing yaw carry-over).
     */
    prop = (int)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x470));
    if ((*(short *)(prop + 0x24) < 4) || (*(short *)(prop + 0x24) > 5)) {
      /* Prop state invalid: notify and continue (don't abort). */
      actor_perception_find_prop_pathfinding_location(actor_handle,
                                                      *(int *)(actor + 0x470));
    }
    if (*(char *)(actor + 0x99) != '\0') {
      *(unsigned int *)(actor + 0x488) = *(unsigned int *)(prop + 0xc8);
      *(unsigned int *)(actor + 0x48c) = *(unsigned int *)(prop + 0xcc);
      *(unsigned int *)(actor + 0x490) = *(unsigned int *)(prop + 0xd0);
    } else {
      *(unsigned int *)(actor + 0x488) = *(unsigned int *)(prop + 0xf0);
      *(unsigned int *)(actor + 0x48c) = *(unsigned int *)(prop + 0xf4);
      *(unsigned int *)(actor + 0x490) = *(unsigned int *)(prop + 0xf8);
    }
    *(unsigned int *)(actor + 0x494) = *(unsigned int *)(prop + 0xec);
    *(unsigned int *)(actor + 0x498) = *(unsigned int *)(actor + 0x474);
    goto LAB_check_dest;

  default:
    display_assert((char *)0, "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0xb7f, 1);
    system_exit(-1);
    goto LAB_fail;
  }

  /* Cases 2/3/4 fall through here; case 5 jumps to LAB_check_dest. */
  *(int *)(actor + 0x498) = 0;

LAB_check_dest:
  /*
   * Validate destination. Two branches:
   *
   * B) actor[0x99]!=0 (mounted): call actor_path_3d_available to check whether
   * the destination is accessible for a mounted actor; output dist. Confirmed
   * at 0x0002ceab-0x0002cebf: JZ skip (actor[0x99]==0) PUSH
   * LEA[EBP-0xc](&dist); PUSH EDI(&actor[0x488]); PUSH ECX CALL
   * actor_path_3d_available
   *
   * A) actor[0x99]==0 (on foot): if actor[0x498]==0.0f, check
   *    actor[0x494]!=-1. If -1, fail. If actor[0x498]!=0.0f, fall through.
   *    Confirmed at 0x0002d096-0x0002d0b3.
   */
  if (*(char *)(actor + 0x99) != '\0') {
    path_found =
      actor_path_3d_available(actor_handle, (float *)(actor + 0x488), &dist);
    if (path_found == '\0') {
      goto LAB_fail;
    }
  } else {
    if (*(float *)(actor + 0x498) == 0.0f) {
      path_found = (char)(*(int *)(actor + 0x494) != -1);
      if (path_found == '\0') {
        goto LAB_fail;
      }
    }
  }

  /* Try fast path: actor is already navigating to the same destination. */
  path_found = actor_test_destination(actor_handle);
  if (path_found != '\0') {
    if (!had_path) {
      goto LAB_path_ok;
    }
    /*
     * Had a previous destination endpoint. Compute squared distance between
     * the saved endpoint (saved_pos) and the new destination (actor[0x488]).
     * If close enough (dist_sq <= threshold at 0x255d1c), return 1 quickly.
     * If destination has changed significantly, fall through to do a full
     * re-path.
     * Confirmed at 0x0002d0d6-0x0002d0ee:
     *   LEA EDX,[EBP-0x18](saved_pos); PUSH EDI(&actor[0x488]); PUSH EDX
     *   CALL distance_squared3d  (FUN_000121a0 = 0x000121a0)
     *   FCOMP [0x255d1c]; FNSTSW AX; TEST AH,0x41; JNZ 0x2d32a (return 1)
     * JNZ taken when: AH & 0x41 != 0 → C3|C0 set → FPU flags for <=
     *   So jump to return-1 when dist_sq <= threshold.
     *   Fall through (full repath) when dist_sq > threshold.
     */
    dist_sq_saved =
      (float)distance_squared3d(saved_pos, (float *)(actor + 0x488));
    if (dist_sq_saved <= *(float *)0x255d1c) {
      goto LAB_path_ok;
    }
    /* Destination changed significantly: fall through to full pathfinding. */
  }

  /*
   * actor_test_destination failed. Compute actual 3D distance from actor
   * position to destination, allocate path cache, and run the pathfinder.
   *
   * tag_get at 0x0002d0f7: PUSH [ESI+0x58]; PUSH 0x61637472 ('rtra'='actr')
   * FUN_0001ad60 at 0x0002d10d: PUSH EDI(&actor[0x488]); PUSH &actor[0x12c]
   *   returns float in FPU; FSTP [EBP-0x8] -> dist
   * game_time_get at 0x0002d12c: no args -> current game tick
   * Confirmed at 0x0002d131: MOV [EBX+4],EAX (path slot timestamp)
   */
  tag = (char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  dist =
    (float)FUN_0001ad60((float *)(actor + 0x12c), (float *)(actor + 0x488));
  actor_handle_u = (unsigned int)actor_handle;
  game_tick = game_time_get();
  *(int *)((actor_handle_u & 0xffff) * 0x657c + *(int *)0x331f58 + 4) =
    game_tick;

  /* Select pathfinding mode: mounted (vehicle) vs on-foot vs override. */
  if (*(char *)(actor + 0x99) != '\0') {
    /*
     * Mounted: use scenario-based vehicle pathfinding (path_3d_build_path).
     * Args confirmed at 0x0002d13e-0x0002d155:
     *   pre-push: &actor[0x4a8], &actor[0x488](EDI), 0, &actor[0x12c]
     *   scenario_get() -> push EAX
     *   CALL path_3d_build_path(scenario, &actor[0x12c], 0, &actor[0x488],
     *                     &actor[0x4a8])
     * ADD ESP,0x14 = 5 args.
     */
    path_found =
      path_3d_build_path((int)scenario_get(), (int *)(actor + 0x12c), 0,
                         (int *)(actor + 0x488), (char *)(actor + 0x4a8));
  } else if (override_path != (void *)0) {
    /*
     * Caller provided a pre-computed path override.
     * Assert: actor[0x480] (dest_object) must be NONE (-1).
     * Then set up override_path as the navigation state:
     *   FUN_0005e0d0(override_path, &actor[0x494], actor[0x498], 0)
     *   path_state_build_path(override_path, &actor[0x4a8])
     * Confirmed at 0x0002d164-0x0002d1bb.
     */
    if (*(int *)(actor + 0x480) != -1) {
      display_assert("actor->control.path.destination_orders."
                     "ignore_target_object_index == NONE",
                     "c:\\halo\\SOURCE\\ai\\actor_moving.c", 0xbbc, 1);
      system_exit(-1);
    }
    FUN_0005e0d0(override_path, (float *)(actor + 0x494),
                 *(int *)(actor + 0x498), 0);
    path_found = path_state_build_path((unsigned int)override_path,
                                       (unsigned int *)(actor + 0x4a8));
  } else {
    /*
     * Normal on-foot pathfinding pipeline:
     *  1. actor_path_input_new(actor_handle, local_nav): initialize nav-state
     * struct (actor position, facing, vehicle info, etc.).
     *  2. paths_dispose(local_nav, actor[0x480]): if ignore_object!=-1,
     *     store it at local_nav+0xc.
     *  3. (Optional) path_input_set_attractor: encode movement-constraint
     * orders into local_nav when actor has standing orders (actor[0x280]>0,
     *     actor[0x28a]==0, tag flag bit 4 clear). Float arg 0x41200000=10.0f.
     *  4. ai_debug_get_path_storage(actor_handle): allocate/find path cache
     * slot.
     *  5. path_state_new(local_nav, large_buf, path_state): init path-build
     *     state in large_buf from local_nav and the cache slot.
     *  6. FUN_0005e0d0(large_buf, &actor[0x488], actor[0x494], actor[0x498]):
     *     set destination in path-build state.
     *  7. FUN_0005ff70(large_buf): run pathfinder; returns 1 on success.
     *  8. path_state_build_path(large_buf, &actor[0x4a8]): extract waypoint
     * result into actor nav-control struct. Returns 1 if path is usable.
     *
     * Disasm confirmed:
     *   local_nav at [EBP-0x60] (44 bytes)
     *   large_buf at [EBP+0xfffebf14] (82060 bytes = 0x1408c)
     */
    actor_path_input_new(actor_handle, local_nav);
    if (*(int *)(actor + 0x480) != -1) {
      paths_dispose(local_nav, *(int *)(actor + 0x480));
    }
    if ((*(short *)(actor + 0x280) > 0) && (*(char *)(actor + 0x28a) == '\0') &&
        ((*(unsigned char *)(tag + 4) & 0x10) == 0)) {
      path_input_set_attractor(
        local_nav, (float *)(actor + 0x2b0), *(float *)(actor + 0x294),
        *(unsigned int *)(actor + 0x28c),
        (unsigned int)0x41200000); /* 10.0f as bit pattern */
    }
    path_state = ai_debug_get_path_storage(actor_handle);
    path_state_new(local_nav, large_buf, path_state);
    FUN_0005e0d0(large_buf, (float *)(actor + 0x488), *(int *)(actor + 0x494),
                 *(int *)(actor + 0x498));
    path_found = FUN_0005ff70((unsigned int *)large_buf);
    if (path_found != '\0') {
      path_found2 = path_state_build_path((unsigned int)large_buf,
                                          (unsigned int *)(actor + 0x4a8));
      path_found = path_found2 ? '\x01' : '\0';
    }
  }

  /* Mark path-computation attempted this tick. */
  *(char *)(actor + 0x4a4) = 1;
  if (store_distance != '\0') {
    *(float *)(actor + 0x4a0) = dist;
  }

  if (path_found != '\0') {
    /*
     * Pathfinding succeeded. Hysteresis check: if the actor was already
     * moving (actor[0x4bc]>0.0f) and the new distance is less than the
     * expected move distance (dist < actor[0x498]) AND the delta is small
     * (dist - actor[0x4bc] < threshold), reset the path to avoid jitter.
     * Confirmed at 0x0002d2ad-0x0002d2f2:
     *   FLD [ESI+0x4bc]; FCOMP 0.0f; TEST AH,0x41; JNZ done
     *   FLD dist; FCOMP [ESI+0x498]; TEST AH,0x5; JP done
     *   FLD dist; FSUB [ESI+0x4bc]; FCOMP [0x253398]; TEST AH,0x5; JP done
     *   CALL FUN_0002a3a0(actor_handle)
     */
    if ((*(float *)(actor + 0x4bc) > 0.0f) &&
        (dist < *(float *)(actor + 0x498)) &&
        (dist - *(float *)(actor + 0x4bc) < *(float *)0x253398)) {
      FUN_0002a3a0(actor_handle);
    }
    return path_found;
  }

LAB_fail:
  FUN_0002a3a0(actor_handle);
  return '\0';

LAB_path_ok:
  *(char *)(actor + 0x4a4) = 1;
  if (store_distance != '\0') {
    *(float *)(actor + 0x4a0) = dist;
  }
  return '\x01';
}

/* 0x2d350 — actor_destination_update: Update actor path state and compute
 * target destination.
 *
 * Called every tick for an actor. Has three main branches:
 *
 * 1. PATH ACTIVE (actor[0x4a8] != 0):
 *    Walks the actor's waypoint path. Each tick it checks whether the actor
 *    has reached the current path node (within 0.15 world units, 2D) or is
 *    close enough to the segment (within 0.25 units perpendicular). If so,
 *    it advances path_step_index. When path is exhausted (step_index+1 >=
 *    step_count), sets path_final_step and either calls actor_path_stop
 *    (if path_loop) or logs "fell off end of unfinished path" (debug).
 *    Then copies the current waypoint to actor[0x50c] as the movement target
 *    and computes actor[0x518] = target - actor_position. Validates that
 *    the distance is less than 1,000,000 world units ("tau ceti" guard).
 *
 * 2. MOVEMENT TYPE != 4 (no far-movement):
 *    Resets path state (has_destination=0, final_step=0, is_moving=1) and
 *    clears path_active. Returns.
 *
 * 3. MOVEMENT TYPE == 4 (far_movement, seek/flee):
 *    Computes a step 3.0 world units ahead (or behind) along the actor's
 *    facing vector. Direction sign: +1 if actor[0x5ec] <= 0.9f, else -1.
 *    Writes target offset and absolute position into actor[0x518..0x514].
 *
 * Confirmed: cdecl, 1 arg (actor_handle), void return.
 * Confirmed: EBX=actor_handle, ESI=actor_ptr, EDI=&actor[0x4a8].
 * Confirmed float constants: 0.0225f=near_sq(0.15), 0.0625f=seg_sq(0.25),
 *   0.0f=zero, 1000000.0f=tau_ceti_sq, 3.0f=step_dist, 0.9f=facing_thresh.
 */
void actor_destination_update(int actor_handle)
{
  extern data_t *actor_data;
  char *actor;
  char *path_ctl;
  char exhausted;
  int step_idx;
  int step_cnt;
  int cur_off;
  int next_off;
  float cur_x, cur_y, next_x, next_y;
  float to_cur_x, to_cur_y;
  float seg_x, seg_y;
  float dot_seg_to_cur, dot_seg_facing;
  float t, perp_x, perp_y, perp_sq;
  float dist_sq;
  char name_buf[0x200];
  float *node;
  float dx, dy, dz, dist;
  int sign_val;
  float step;

  actor = (char *)datum_get(actor_data, actor_handle);

  if (*(char *)(actor + 0x4c) != '\0' && *(char *)(actor + 0x4a4) == '\0' &&
      *(char *)(actor + 0x13) == '\0') {
    actor_path_refresh(actor_handle, 0, 0);
  }

  actor_test_destination(actor_handle);

  path_ctl = actor + 0x4a8;
  if (*(char *)(actor + 0x4a8) != '\0') {
    exhausted = '\0';

    while (1) {
      step_idx = (int)*(signed char *)(actor + 0x4c2);
      step_cnt = (int)*(signed char *)(actor + 0x4c1);

      if (step_idx + 1 >= step_cnt) {
        exhausted = '\x01';
        break;
      }

      cur_off = (step_idx + 2) * 0x10;
      next_off = (step_idx + 3) * 0x10;

      cur_x = *(float *)(path_ctl + cur_off);
      cur_y = *(float *)(path_ctl + cur_off + 4);
      next_x = *(float *)(path_ctl + next_off);
      next_y = *(float *)(path_ctl + next_off + 4);

      to_cur_x = cur_x - *(float *)(actor + 0x12c);
      to_cur_y = cur_y - *(float *)(actor + 0x130);

      seg_x = next_x - cur_x;
      seg_y = next_y - cur_y;

      /* Load path_final_step flag (actor+0x506). */
      if (*(char *)(actor + 0x506) == '\0') {
        /* Check whether to use simple distance or projected segment test. */
        if (*(char *)(actor + 0x504) != '\0' &&
            *(char *)(actor + 0x507) != '\0') {
          /* Segment projection test.
           *
           * dot_seg_to_cur = dot(seg_dir, to_cur)
           * dot_seg_facing = dot(seg_dir, actor_facing)
           *
           * Skip advance if:
           *   - dot_seg_facing <= 0.0f (not facing toward next step), OR
           *   - dot_seg_to_cur >= 0.0f (actor already past current node)
           *
           * If both pass, compute perpendicular distance from actor to segment
           * and advance only if perp_dist_sq < 0.0625f.
           *
           * Disassembly verified operand order:
           *   0x2d425: FLD [EBP-0x10] (seg_y) FMUL [EBP-0x8] (to_cur_y)
           *   0x2d42b: FLD [EBP-0x14] (seg_x) FMUL [EBP-0xc] (to_cur_x)
           *   FADDP => dot_seg_to_cur = seg_y*to_cur_y + seg_x*to_cur_x
           *   0x2d433: FLD [EBP-0x10] (seg_y) FMUL [ESI+0x178] (facing_y)
           *   0x2d43f: FLD [EBP-0x14] (seg_x) FMUL [ESI+0x174] (facing_x)
           *   FADDP => dot_seg_facing = seg_y*facing_y + seg_x*facing_x
           */
          dot_seg_to_cur = seg_y * to_cur_y + seg_x * to_cur_x;
          dot_seg_facing = seg_y * *(float *)(actor + 0x178) +
                           seg_x * *(float *)(actor + 0x174);

          /* FCOMP [0x2533c0]=0.0f; TEST AH,0x41; JNZ => jump if <= 0 */
          if (dot_seg_facing <= 0.0f) {
            break;
          }
          /* FCOM [0x2533c0]=0.0f; TEST AH,0x5; JP => jump if >= 0 */
          if (dot_seg_to_cur >= 0.0f) {
            break;
          }

          /* Perpendicular distance from actor to segment line.
           * t = -dot_seg_to_cur (positive, since dot_seg_to_cur < 0)
           * perp = to_cur + t * seg_dir
           * perp_sq = perp.x^2 + perp.y^2
           *
           * Disasm 0x2d461-0x2d47b:
           *   FCHS   => t = -dot_seg_to_cur (ST0 now t)
           *   FLD seg_x; FMUL ST1 => seg_x * t
           *   FADD to_cur_x => perp_x = seg_x*t + to_cur_x
           *   FLD seg_y; FMUL ST2 => seg_y * t (ST2 = t)
           *   FADD to_cur_y => perp_y = seg_y*t + to_cur_y
           *   FLD ST0; FMUL ST1 => perp_y*perp_y
           *   FLD ST2; FMUL ST3 => perp_x*perp_x (ST3=perp_x)
           *   Wait: at 0x2d473: FLD ST0 = perp_y, FMUL ST1 = perp_y*perp_y
           *         0x2d477: FLD ST2 = perp_x, FMUL ST3 = perp_x*perp_y ... no
           *
           * Re-trace FPU stack at 0x2d461:
           *   ST0 = dot_seg_to_cur (the one from FADDP at 0x2d431)
           *   dot_seg_facing was computed 0x2d433-0x2d445, then FCOMP popped it
           *   So at 0x2d461: ST0 = dot_seg_to_cur
           *   FCHS => ST0 = t = -dot_seg_to_cur
           *   0x2d463: FLD seg_x (ST0=seg_x, ST1=t)
           *   0x2d466: FMUL ST1 => ST0 = seg_x*t; ST1=t
           *   0x2d468: FADD to_cur_x => ST0 = perp_x; ST1=t
           *   0x2d46b: FLD seg_y (ST0=seg_y, ST1=perp_x, ST2=t)
           *   0x2d46e: FMUL ST2 => ST0 = seg_y*t; ST1=perp_x; ST2=t
           *   0x2d470: FADD to_cur_y => ST0=perp_y; ST1=perp_x; ST2=t
           *   0x2d473: FLD ST0 => ST0=perp_y; ST1=perp_y; ST2=perp_x; ST3=t
           *   0x2d475: FMUL ST1 => ST0=perp_y*perp_y; ST1=perp_y; ST2=perp_x
           *   0x2d477: FLD ST2 => ST0=perp_x; ST1=perp_y*perp_y; ST2=perp_y;
           * ST3=perp_x 0x2d479: FMUL ST3 => ST0=perp_x*perp_x;
           * ST1=perp_y*perp_y 0x2d47b: FADDP => ST0=perp_x*perp_x+perp_y*perp_y
           * = perp_sq
           */
          t = -dot_seg_to_cur;
          perp_x = seg_x * t + to_cur_x;
          perp_y = seg_y * t + to_cur_y;
          perp_sq = perp_x * perp_x + perp_y * perp_y;

          /* FCOMP [0x255d90]=0.0625f; TEST AH,0x5; JP => jump if >= 0.0625f */
          if (perp_sq >= 0.0625f) {
            break;
          }
        } else {
          /* Simple 2D distance-to-current-node check.
           * Disasm 0x2d48b-0x2d499:
           *   FLD to_cur_y; FMUL to_cur_y  => to_cur_y^2
           *   FLD to_cur_x; FMUL to_cur_x  => to_cur_x^2
           *   FADDP => dist_sq
           *   FCOMP [0x255d8c]=0.0225f; TEST AH,0x5; JP => jump if >= 0.0225f
           */
          dist_sq = to_cur_y * to_cur_y + to_cur_x * to_cur_x;
          if (dist_sq >= 0.0225f) {
            break;
          }
        }
      }

      /* Advance to next step. */
      *(signed char *)(actor + 0x4c2) += 1;
      *(char *)(actor + 0x506) = '\0';
    }

    /* Handle path-exhausted or final-step state. */
    if (*(char *)(actor + 0x506) != '\0') {
      if (exhausted == '\0') {
        /* Reached the final step but loop says we shouldn't be here. */
        display_assert("final_step", "c:\\halo\\SOURCE\\ai\\actor_moving.c",
                       0xb4, 1);
        system_exit(-1);
      }

      if (*(char *)(actor + 0x4c0) != '\0') {
        /* Path has a loop/done handler — call actor_path_stop. */
        FUN_0002a3a0(actor_handle);
      } else if (*(char *)0x5aca62 != '\0') {
        /* Debug: log "fell off end of unfinished path".
         * ai_debug_describe_actor: actor_describe_name(actor_handle, -1, 1,
         * buf, 0x200) Disasm 0x2d518-0x2d529: PUSH 0x200; PUSH EDX(local_218);
         * PUSH 1; PUSH -1; PUSH EBX
         */
        ai_debug_describe_actor(actor_handle, -1, 1, name_buf, 0x200);
        error(2, "%s: fell off end of unfinished path %d/%d", name_buf,
              (int)*(signed char *)(actor + 0x4c1), 4);
      }
    }

    /* If path_active and (has_destination or not is_moving), set the
     * current target position from the path node at step_index. */
    if (*path_ctl != '\0' && (*(char *)(actor + 0x504) != '\0' ||
                              *(char *)(actor + 0x484) == '\0')) {
      *(char *)(actor + 0x504) = '\x01';

      /* Copy node position: actor[0x4c8 + step_index*0x10] → actor[0x50c].
       * Disasm 0x2d574-0x2d5a1: MOVSX EDX,byte[ESI+0x4c2]; SHL EDX,4;
       *   LEA ECX,[EDX+ESI+0x4c8]; copy 3 dwords to [ESI+0x50c].
       */
      step_idx = (int)*(signed char *)(actor + 0x4c2);
      node = (float *)(actor + 0x4c8 + step_idx * 0x10);

      *(float *)(actor + 0x50c) = node[0];
      *(float *)(actor + 0x510) = node[1];
      *(float *)(actor + 0x514) = node[2];

      /* Compute vector from actor to target. */
      *(float *)(actor + 0x518) =
        *(float *)(actor + 0x50c) - *(float *)(actor + 0x12c);
      *(float *)(actor + 0x51c) =
        *(float *)(actor + 0x510) - *(float *)(actor + 0x130);
      *(float *)(actor + 0x520) =
        *(float *)(actor + 0x514) - *(float *)(actor + 0x134);

      /* Sanity check: if distance^2 < 1,000,000 (i.e. < 1000 units), OK.
       * Disasm 0x2d5d0-0x2d605: FPU computes sqrt(dx^2+dy^2+dz^2), then
       *   FCOMP [0x255d50]=1000000.0f; TEST AH,0x1; JNZ => jump if < 1000000.
       * TEST AH,0x1 = test C0 (ST0 < mem). JNZ = jump if C0 set (sqrt <
       * 1000000). But FSQRT was done before, so we compare sqrt (distance)
       * against sqrt(1000000) = 1000? No — looking again at disasm: FSQRT at
       * 0x2d5f2 FSTP ST3 at 0x2d5f4 (saves result into ST3 slot, discards from
       * top) FSTP ST0 twice (discards remaining ST0, ST1) Then FCOMP [0x255d50]
       * at 0x2d5fa After FSTP ST3: the stack shrinks, so the FCOMP operand is
       * the distance value itself (the sqrt result stored into ST3 then brought
       * to top via the STPs). Actually:
       *   At 0x2d5d0: FLD dz -> FLD dy -> FLD dx -> FLD ST0 (=dx)
       *   0x2d5e4: FMUL ST1 => dx*dx; stack: dx*dx, dx, dy, dz
       *   0x2d5e6: FLD ST2 (=dy); FMUL ST3 (=dy) => dy*dy
       *   0x2d5ea: FADDP => dx*dx+dy*dy; stack: sum, dx, dy, dz
       *   0x2d5ec: FLD ST3 (=dz); FMUL ST4 (=dz) => dz*dz
       *   0x2d5f0: FADDP => sum+dz*dz; stack: dist_sq, dx, dy, dz
       *   0x2d5f2: FSQRT => dist; stack: dist, dx, dy, dz
       *   0x2d5f4: FSTP ST3 => ST3=dist, pops: stack: dx, dy, dist
       *     (FSTP ST3 stores ST0 into ST3 slot then pops ST0)
       *     After: ST0=dx, ST1=dy, ST2=dist, ST3=dz was at ST3
       *     Wait: FSTP STn stores ST0 into STn then pops. After FSQRT:
       *       ST0=dist, ST1=dx, ST2=dy, ST3=dz
       *     FSTP ST3: ST3 = dist, pop ST0: ST0=dx, ST1=dy, ST2=dz -> wait
       *     Actually FSTP ST3 sets ST3=ST0=dist, then increments stack pointer
       *     (pops ST0). So new stack: ST0=dx, ST1=dy, ST2=dz, ST3=dist
       *   0x2d5f6: FSTP ST0 => discard dx; ST0=dy, ST1=dz, ST2=dist
       *   0x2d5f8: FSTP ST0 => discard dy; ST0=dz, ST1=dist
       *   Hmm, but FCOMP at 0x2d5fa uses 1 operand and pops ST0.
       *   We need dist to be in ST0. Let me re-read disasm...
       *   0x2d5f4: FSTP ST3 => stores dist into position 3 (which is dz), pops
       * ST0 After: ST0=dx, ST1=dy, ST2=dz(overwritten=dist) 0x2d5f6: FSTP ST0
       * => pops ST0=dx, discards it After: ST0=dy, ST1=dist 0x2d5f8: FSTP ST0
       * => pops ST0=dy, discards it After: ST0=dist 0x2d5fa: FCOMP
       * [0x255d50]=1000000.0f => compares dist to 1000000.0f TEST AH,0x1 =>
       * test C0 (ST0<mem). JNZ => jump if dist < 1000000.0f
       *
       * So we compare distance (not distance^2) to 1,000,000. This is
       * "tau ceti" = 1 million world units (absurd distance).
       */
      dx = *(float *)(actor + 0x518);
      dy = *(float *)(actor + 0x51c);
      dz = *(float *)(actor + 0x520);
      dist = sqrtf(dx * dx + dy * dy + dz * dz);

      /* Jump past error if distance is sane (< 1,000,000 units). */
      if (dist < 1000000.0f) {
        return;
      }

      /* Insanely far target: log error and clear path. */
      error(2, "pathfinding is attempting to walk to tau ceti");
      *path_ctl = '\0';
      return;
    }
    return;
  }

  /* No active path. Check movement mode. */
  if (*(short *)(actor + 0x15e) != 4) {
    /* Not far-movement: reset path destination and target state. */
    *(char *)(actor + 0x504) = '\0';
    *(char *)(actor + 0x506) = '\0';
    *(char *)(actor + 0x484) = '\x01';

    /* Re-fetch actor (second datum_get call in this branch, confirmed at
     * 0x2d6ea). */
    actor = (char *)datum_get(actor_data, actor_handle);
    *(char *)(actor + 0x4a8) = '\0';
    *(char *)(actor + 0x484) = '\x01';
    *(int *)(actor + 0x4a0) = 0;
    return;
  }

  /* Movement type == 4: far_movement. Compute step along facing vector.
   *
   * actor[0x5ec]: if > 0.9f → sign=-1 (backward), else sign=+1 (forward).
   * Disasm 0x2d632-0x2d693:
   *   FLD [ESI+0x5ec]; FCOMP [0x2555d0]=0.9f; FNSTSW AX
   *   TEST AH,0x41; JNZ 0x2d649 (set AL=0); else: MOV AL,1
   *   XOR EDX,EDX; TEST AL,AL; SETZ DL   => DL=1 if AL==0, DL=0 if AL!=0
   *   LEA EDX,[EDX+EDX-1]                => EDX = DL*2 - 1
   *     if actor[0x5ec]>0.9: AL=1,DL=0 → EDX=-1
   *     if actor[0x5ec]<=0.9: AL=0,DL=1 → EDX=1
   *   FILD [EBP-0x8] (=EDX); FMUL [0x254644]=3.0f  => sign * 3.0
   */
  *(char *)(actor + 0x504) = '\x01';
  *(char *)(actor + 0x506) = '\0';

  /* FCOMP test: if actor[0x5ec] <= 0.9f → sign=+1, else sign=-1 */
  if (*(float *)(actor + 0x5ec) > 0.9f) {
    sign_val = -1;
  } else {
    sign_val = 1;
  }

  step = (float)sign_val * 3.0f;

  *(float *)(actor + 0x518) = step * *(float *)(actor + 0x174);
  *(float *)(actor + 0x51c) = step * *(float *)(actor + 0x178);
  *(float *)(actor + 0x520) = step * *(float *)(actor + 0x17c);

  *(float *)(actor + 0x50c) =
    *(float *)(actor + 0x12c) + *(float *)(actor + 0x518);
  *(float *)(actor + 0x510) =
    *(float *)(actor + 0x130) + *(float *)(actor + 0x51c);
  *(float *)(actor + 0x514) =
    *(float *)(actor + 0x134) + *(float *)(actor + 0x520);
}

/* 0x2d850 — Set actor movement to far-movement mode (move_type=4,
 * dest=param_2).
 *
 * Clears the actor's 3b8 (movement dormant flag), calls actor_set_dormant to
 * wake the actor, then checks if the actor is already in far-movement mode
 * heading to param_2. If not, sets up the movement block at +0x400..+0x417,
 * copies it to the active slot at +0x46c, and kicks off a path refresh
 * (store_distance=1). If already at the target, checks if the actor is still
 * active (+0x4c) and not sleeping (+0x4a4), and if so refreshes the path
 * (store_distance=0). Returns the result of actor_path_refresh, or 1 if
 * no refresh was needed.
 *
 * Confirmed: datum_get(0x6325a4, actor_handle) at 0x2d860.
 * Confirmed: OR EDI,-1 / MOV DI,[ESI+0x3b8] = 0xffff at 0x2d869/0x2d86d.
 * Confirmed: actor_set_dormant(actor_handle, 0) at 0x2d874.
 * Confirmed: CMP [ESI+0x46c], 4 / CMP [ESI+0x470], CX at 0x2d886-0x2d893.
 * Confirmed: MOV [ESI+0x404],CX; MOV [ESI+0x414],EDI=-1; MOV [ESI+0x402],0;
 *            MOV [ESI+0x400],4 at 0x2d8be-0x2d8da.
 * Confirmed: REP MOVSD ECX=6 from ESI=actor+0x400 to EDI=actor+0x46c at
 * 0x2d8e5. Confirmed: actor_path_refresh(actor_handle,1,0) at 0x2d8e7.
 * Confirmed: actor_path_refresh(actor_handle,0,0) at 0x2d8ab.
 * Confirmed: return 1 via MOV AL,1 at 0x2d8f6.
 */
char actor_move_to_move_position(int actor_handle, int16_t param_2)
{
  char *iVar1;
  int iVar3;
  int *puVar4;
  short *psVar5;

  iVar1 = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  *(int16_t *)(iVar1 + 0x3b8) = -1;
  actor_set_dormant(actor_handle, 0);
  if ((*(int16_t *)(iVar1 + 0x46c) != 4) ||
      (*(int16_t *)(iVar1 + 0x470) != param_2)) {
    *(int16_t *)(iVar1 + 0x404) = param_2;
    *(int *)(iVar1 + 0x414) = -1;
    *(char *)(iVar1 + 0x402) = 0;
    *(int16_t *)(iVar1 + 0x400) = 4;
    puVar4 = (int *)(iVar1 + 0x400);
    psVar5 = (short *)(iVar1 + 0x46c);
    for (iVar3 = 6; iVar3 != 0; iVar3--) {
      *(int *)psVar5 = *puVar4;
      puVar4++;
      psVar5 += 2;
    }
    return actor_path_refresh(actor_handle, 1, 0);
  }
  if ((*(char *)(iVar1 + 0x4c) != '\0') && (*(char *)(iVar1 + 0x4a4) == '\0')) {
    return actor_path_refresh(actor_handle, 0, 0);
  }
  return 1;
}

/* 0x2d9b0 — Set actor movement to encounter-path mode (move_type=5,
 * dest=encounter_handle, dist=distance).
 *
 * Clears actor+0x3b8, wakes the actor, then checks if it is already in mode 5
 * with the same encounter handle and distance. If so, either refreshes the path
 * (store_distance=0, if actor is active/not-sleeping) or returns 1. Otherwise
 * sets up the movement block at +0x400: mode=5, encounter_handle at +0x404,
 * distance at +0x408, path node from encounter+0x110 (fallback +0x18) at
 * +0x414, copies the 24-byte block to the active slot at +0x46c, then calls
 * actor_path_refresh(store_distance=1).
 *
 * Confirmed: datum_get(0x6325a4, actor_handle) at 0x2d9c0.
 * Confirmed: actor_set_dormant(actor_handle, 0) at 0x2d9c7.
 * Confirmed: CMP [EDI],5 / CMP [ESI+0x470],ECX / FCOMP [EBP+0x10] at
 * 0x2d9e1-0x2d9f8. Confirmed: datum_get(0x5ab23c, encounter_handle) at 0x2da2c.
 * Confirmed: encounter+0x110 fallback to encounter+0x18 at 0x2da5c-0x2da6d.
 * Confirmed: REP MOVSD ECX=6 from actor+0x400 to actor+0x46c at 0x2da83.
 * Confirmed: actor_path_refresh(actor_handle,1,0) at 0x2da85.
 * Confirmed: actor_path_refresh(actor_handle,0,0) at 0x2da1c.
 * Confirmed: return 1 via MOV AL,1 at 0x2da94.
 */
char actor_move_to_prop(int actor_handle, int encounter_handle, float distance)
{
  char *actor;
  char *encounter;
  int node_handle;
  int *active_state;
  int *pending_state;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  *(int16_t *)(actor + 0x3b8) = -1;
  actor_set_dormant(actor_handle, 0);
  active_state = (int *)(actor + 0x46c);
  if (*(int16_t *)active_state == 5 &&
      *(int *)(actor + 0x470) == encounter_handle &&
      *(float *)(actor + 0x474) == distance) {
    if (*(char *)(actor + 0x4c) == 0) {
      return 1;
    }
    if (*(char *)(actor + 0x4a4) != 0) {
      return 1;
    }
    return actor_path_refresh(actor_handle, 0, 0);
  }
  encounter = (char *)datum_get(*(data_t **)0x5ab23c, encounter_handle);
  *(int *)(actor + 0x404) = encounter_handle;
  *(int16_t *)(actor + 0x400) = 5;
  *(char *)(actor + 0x402) = 0;
  *(float *)(actor + 0x408) = distance;
  node_handle = *(int *)(encounter + 0x110) == -1 ? *(int *)(encounter + 0x18) :
                                                    *(int *)(encounter + 0x110);
  *(int *)(actor + 0x414) = node_handle;
  pending_state = (int *)(actor + 0x400);
  for (node_handle = 6; node_handle != 0; node_handle--) {
    *active_state++ = *pending_state++;
  }
  return actor_path_refresh(actor_handle, 1, 0);
}
