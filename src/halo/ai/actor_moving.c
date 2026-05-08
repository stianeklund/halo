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

/* 0x2b5d0 — FUN_0002b5d0: initialize trigonometric lookup tables.
 *
 * Confirmed: no arguments, no calls, writes table blocks rooted at
 * 0x6327e0 and 0x6325c0 using constants/tables at 0x25577c..0x25581c.
 * Confirmed: first loop runs 9 iterations (EDX=9), stride 0x1c (7 floats).
 * Confirmed: second stage runs 2 outer iterations × 8 inner iterations,
 * with destination stride 0x38 (14 floats) per inner iteration.
 */
void FUN_0002b5d0(void)
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

/* 0x2d350 — FUN_0002d350: Update actor path state and compute target
 * destination.
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
void FUN_0002d350(int actor_handle)
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
    FUN_0002cdb0(actor_handle, 0, 0);
  }

  FUN_0002a580(actor_handle);

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
         * FUN_00049ac0: actor_describe_name(actor_handle, -1, 1, buf, 0x200)
         * Disasm 0x2d518-0x2d529:
         *   PUSH 0x200; PUSH EDX(local_218); PUSH 1; PUSH -1; PUSH EBX
         */
        FUN_00049ac0(actor_handle, -1, 1, name_buf, 0x200);
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
      dist = __builtin_sqrtf(dx * dx + dy * dy + dz * dz);

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
