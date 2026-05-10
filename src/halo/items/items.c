/* Activate the pickup sound effect for an equipment item.
 * Looks up the equipment tag definition ('eqip') and plays the
 * pickup sound (tag field at +0x31c) at full volume (scale=1.0). */
void item_activate_equipment_effect(int equipment_handle)
{
  int *equip_obj;
  char *tag_def;
  int sound_tag;

  equip_obj = (int *)object_get_and_verify_type(equipment_handle, 8);
  tag_def = (char *)tag_get(0x65716970, *equip_obj);
  sound_tag = *(int *)(tag_def + 0x31c);
  if (sound_tag != NONE) {
    sound_impulse_start(sound_tag, 1.0f);
  }
}

/* Play the pickup sound for an equipment tag (0xf67f0).
 * Reads the pickup sound tag index at equipment_tag+0x31c and plays it. */
void FUN_000f67f0(int equipment_tag_index)
{
  int tag_data = (int)tag_get(0x65716970, equipment_tag_index);
  if (*(int *)(tag_data + 0x31c) != -1) {
    sound_impulse_start(*(int *)(tag_data + 0x31c), 1.0f);
  }
}

/* Iterate all item objects (type 0x1c) and return true if any have
 * a positive danger count, indicating a dangerous item is near a player. */
bool dangerous_items_near_player(void)
{
  char iter[16];
  void *item;

  object_iterator_new(iter, 0x1c, 1);
  for (item = object_iterator_next(iter); item != NULL;
       item = object_iterator_next(iter)) {
    if (*(int16_t *)((char *)item + 0x1a8) > 0)
      return true;
  }
  return false;
}

/* Attach or detach an item from a unit.
 * When unit_handle is valid: sets item flags bit 0 (attached), conditionally
 * sets bit 1 (player-controlled) based on unit's player handle at +0x1c8,
 * copies the player handle to item+0x70, removes item from garbage list,
 * clears bits 3 and 5 of item flags, and resets the item's scenario location
 * at +0x48.
 * When unit_handle is NONE: clears bits 0 and 1, detaching the item. */
void item_attach_to_unit(int item_handle, int unit_handle)
{
  char *item_obj;
  char *unit_obj;
  uint32_t flags;

  item_obj = (char *)object_get_and_verify_type(item_handle, 0x1c);
  if (unit_handle != NONE) {
    unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);
    flags = *(uint32_t *)(item_obj + 0x1a4);
    flags |= 1;
    *(uint32_t *)(item_obj + 0x1a4) = flags;
    if (*(int *)(unit_obj + 0x1c8) == NONE) {
      flags = (flags & ~2u) | 1;
    } else {
      flags |= 3;
    }
    *(uint32_t *)(item_obj + 0x1a4) = flags;
    *(int *)(item_obj + 0x70) = *(int *)(unit_obj + 0x1c8);
    object_set_garbage_flag(item_handle, 0);
    *(uint32_t *)(item_obj + 0x1a4) = *(uint32_t *)(item_obj + 0x1a4) & ~0x28u;
    *(int *)(item_obj + 0x48) = NONE;
    *(int16_t *)(item_obj + 0x4c) = (int16_t)NONE;
    scenario_location_reset((int *)(item_obj + 0x48));
  } else {
    *(uint32_t *)(item_obj + 0x1a4) = *(uint32_t *)(item_obj + 0x1a4) & ~3u;
  }
}

/* Initialize the danger countdown for an item (0xf6af0).
 * If the item's danger count at +0x1a8 is zero, spawns an effect using
 * the tag reference at +0x2f4 of the 'item' tag definition (detonation
 * or fuse warning), then sets the countdown to a random value in the
 * range [tag+0x2e0, tag+0x2e4] scaled by 30.0 (ticks per second).
 * Called from item_set_position when the item is flagged for detonation
 * and the game engine is not running (campaign/solo mode). */
void item_detonate(int item_handle)
{
  char *item_obj;
  char *item_tag;
  int *seed;
  float rnd;

  item_obj = (char *)object_get_and_verify_type(item_handle, 0x1c);
  item_tag = (char *)tag_get(0x6974656d, *(int *)item_obj);

  if (*(int16_t *)(item_obj + 0x1a8) == 0) {
    FUN_0009ec30(
      *(int *)(item_tag + 0x2f4), item_handle,
      item_handle, /* dup-args-ok: same handle as source and target */
      NONE, 0, 0, 0, 0);
    seed = get_global_random_seed_address();
    rnd = random_real_range(seed, *(float *)(item_tag + 0x2e0),
                            *(float *)(item_tag + 0x2e4));
    *(int16_t *)(item_obj + 0x1a8) = (int16_t)(int)(rnd * 30.0f);
  }
}

/* Update an item's angular velocity state (0xf6b80).
 * Computes the magnitude of the angular velocity vector at +0x3c..+0x44.
 * If nonzero: sets flag bit 2 (has angular velocity) at +0x1a4, normalizes
 * the angular velocity into the direction vector at +0x1c8..+0x1d0 (unless
 * object flag bit 5 at +0x4 is set, indicating externally driven rotation),
 * then stores sin(magnitude) at +0x1d4 and cos(magnitude) at +0x1d8.
 * If zero: clears flag bit 2 and sets sin=0, cos=1 (identity rotation). */
void FUN_000f6b80(int item_handle)
{
  char *item_obj;
  float x, y, z;
  float mag;
  float inv_mag;

  item_obj = (char *)object_get_and_verify_type(item_handle, 0x1c);
  x = *(float *)(item_obj + 0x3c);
  y = *(float *)(item_obj + 0x40);
  z = *(float *)(item_obj + 0x44);
  mag = sqrtf(x * x + y * y + z * z);

  if (mag != 0.0f) {
    *(uint32_t *)(item_obj + 0x1a4) = *(uint32_t *)(item_obj + 0x1a4) | 4;

    if (!(*(uint8_t *)(item_obj + 0x4) & 0x20)) {
      inv_mag = 1.0f / mag;
      *(float *)(item_obj + 0x1c8) = inv_mag * x;
      *(float *)(item_obj + 0x1cc) = inv_mag * y;
      *(float *)(item_obj + 0x1d0) = inv_mag * z;
    }

    *(float *)(item_obj + 0x1d4) = sinf(mag);
    *(float *)(item_obj + 0x1d8) = cosf(mag);
  } else {
    *(uint32_t *)(item_obj + 0x1a4) = *(uint32_t *)(item_obj + 0x1a4) & ~4u;
    *(float *)(item_obj + 0x1d4) = 0.0f;
    *(float *)(item_obj + 0x1d8) = 1.0f;
  }
}

/* item_set_position (0xf6d60)
 *
 * Apply a velocity/position delta to an item and update its angular velocity.
 * Manages collision user depth, ground clamping, angular tumble, and garbage
 * flag state. Called each tick to move free-floating items (grenades, weapons,
 * equipment on the ground).
 *
 * If the item is grounded (flag bit 3 at +0x1a4) and the delta magnitude is
 * above a small epsilon, the item is repositioned to just above the ground
 * plane via a "ground point" marker lookup and plane projection. Otherwise
 * position is simply accumulated. Angular velocity gets a tumble impulse from
 * cross(global_up, delta) scaled by a random factor and pi/2, or a random
 * angular jolt from the ground normal when velocity is near zero.
 *
 * Confirmed: 3 cdecl args (item_handle, position, flag).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with type_mask 0x1c.
 * Confirmed: CALL 0x1ba140 (tag_get) with 'item' (0x6974656d).
 * Confirmed: CALL 0x8d9f0 (display_assert) for collision depth checks.
 * Confirmed: CALL 0xa8e30 (game_engine_running) for flag-dependent branch.
 * Confirmed: CALL 0xf6af0 (item_detonate) if flag set and engine not running.
 * Confirmed: CALL 0x140f10 (object_get_markers_by_string_id) for "ground
 * point". Confirmed: CALL 0x18e3f0 (global_collision_bsp_get) to get collision
 * BSP. Confirmed: CALL 0x19b210 (tag_block_get_element) at bsp+0x3c. Confirmed:
 * CALL 0x99640 (FUN_00099640) for plane extraction. Confirmed: CALL 0x12f80
 * (vector3d_scale_add) for ground projection. Confirmed: CALL 0x143be0
 * (object_translate) for repositioning item. Confirmed: CALL 0x12170 (FUN_00012170)
 * for vector magnitude. Confirmed: CALL 0x10b0d0
 * (get_global_random_seed_address). Confirmed: CALL 0x10b240 (random_math_real)
 * for random scale. Confirmed: CALL 0x13010 (normalize3d) for cross product
 * normalization. Confirmed: CALL 0x10b380 (random_seed_get_direction3d) for
 * degenerate case. Confirmed: CALL 0x121e0 (FUN_000121e0) for random angle
 * [-pi/4, pi/4]. Confirmed: CALL 0x213c0 (vector3d_add) for angular velocity
 * accumulation. Confirmed: CALL 0xf6b80 (FUN_000f6b80) with item_handle in EAX.
 * Confirmed: CALL 0x13d920 (object_set_garbage_flag) with (handle, 0).
 * Confirmed: global collision depth at 0x4761d8 (int16_t).
 * Confirmed: collision user stack at 0x5a8c80.
 * Confirmed: global up vector pointer at 0x31fc44 → {0, 0, 1}.
 * Confirmed: epsilon constant at 0x253f44 = ~0.0001f.
 * Confirmed: offset constant at 0x2533e8 = 0.05f.
 * Confirmed: zero constant at 0x2533c0 = 0.0f.
 * Confirmed: pi/2 constant at 0x2568bc = ~1.5708f.
 */
void item_set_position(int item_handle, float *position, int flag)
{
  char *item_obj;
  char *item_tag;
  int16_t marker_count;
  char marker_buf[0x6c];
  float plane[4];
  float cross[3];
  float vel_mag;
  float new_pos[3];
  float scaled_dir[3];
  float *up;
  float scale;
  int bsp;
  int *plane_ref;
  float dot;

  item_obj = (char *)object_get_and_verify_type(item_handle, 0x1c);
  item_tag = (char *)tag_get(0x6974656d, *(int *)item_obj);

  /* Early out if item has flag bit 5 set at +0x1a4 */
  if (*(uint8_t *)(item_obj + 0x1a4) & 0x20)
    return;

  /* Collision depth guard */
  if (*(int16_t *)0x4761d8 >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\items\\items.c", 0x218, 1);
    system_exit(-1);
  }

  {
    int16_t depth = *(int16_t *)0x4761d8;
    *(int16_t *)(0x5a8c80 + (int)depth * 2) = 0xb;
    *(int16_t *)0x4761d8 = depth + 1;
  }

  /* Only process if parent object handle (obj+0xCC) is NONE */
  if (*(int *)(item_obj + 0xcc) == NONE) {
    /* If flag param is set, game engine not running, and tag flag bit 1 set,
     * call item_detonate (possibly spawns pickup effect or similar) */
    if ((char)flag != 0) {
      if (!game_engine_running()) {
        if (*(uint8_t *)(item_tag + 0x17c) & 2) {
          item_detonate(item_handle);
        }
      }
    }

    /* Check ground flag (bit 3 of item_flags at +0x1a4) */
    if (!(*(uint8_t *)(item_obj + 0x1a4) & 0x8)) {
      /* Not grounded: just clear the "needs update" flag bit 5 at +0x04 */
      *(uint32_t *)(item_obj + 0x04) =
        *(uint32_t *)(item_obj + 0x04) & 0xffffffdf;
    } else if (*(float *)0x253f44 <= position[0] * position[0] +
                                       position[1] * position[1] +
                                       position[2] * position[2]) {
      /* Grounded and velocity magnitude squared >= epsilon:
       * Try to reposition item to ground plane */
      marker_count = object_get_markers_by_string_id(
        item_handle, (void *)0x28aa90, marker_buf, 1);
      if (marker_count != 0) {
        /* Ground point marker found: project position onto ground plane */
        bsp = (int)global_collision_bsp_get();
        plane_ref = (int *)tag_block_get_element(
          (void *)(bsp + 0x3c), (int)*(int16_t *)(item_obj + 0x1aa), 0xc);
        FUN_00099640(bsp, *plane_ref, plane);

        /* Compute offset from plane: 0.05 - (dot(normal, ground_pos) -
         * distance) */
        dot = plane[0] * *(float *)(marker_buf + 0x60) +
              plane[1] * *(float *)(marker_buf + 0x64) +
              plane[2] * *(float *)(marker_buf + 0x68);
        scale = *(float *)0x2533e8 - (dot - plane[3]);

        vector3d_scale_add((float *)(marker_buf + 0x60), plane, scale, new_pos);
        object_translate(item_handle, new_pos, 0);
      }
      /* Clear "needs update" bit 5 at +0x04 and ground bit 3 at +0x1a4 */
      *(uint32_t *)(item_obj + 0x04) =
        *(uint32_t *)(item_obj + 0x04) & 0xffffffdf;
      *(uint32_t *)(item_obj + 0x1a4) =
        *(uint32_t *)(item_obj + 0x1a4) & 0xfffffff7;
    }

    /* Accumulate position delta onto object position at +0x18 */
    *(float *)(item_obj + 0x18) += position[0];
    *(float *)(item_obj + 0x1c) += position[1];
    *(float *)(item_obj + 0x20) += position[2];

    /* Determine angular velocity update path */
    if (*(int *)(item_obj + 0x1b0) != NONE ||
        !(*(uint8_t *)(item_obj + 0x1a4) & 0x8) ||
        *(float *)0x253f44 <= FUN_00012170(position)) {
      /* Normal tumble: compute angular velocity from cross product */
      vel_mag = sqrtf(position[0] * position[0] + position[1] * position[1] +
                      position[2] * position[2]);
      if (vel_mag < *(float *)0x253f44) {
        unsigned int *seed = (unsigned int *)get_global_random_seed_address();
        vel_mag = random_math_real(seed);
      }

      /* cross = cross(global_up, position_delta) */
      up = *(float **)0x31fc44;
      cross[2] = up[0] * position[1] - up[1] * position[0];
      cross[1] = position[0] * up[2] - up[0] * position[2];
      cross[0] = up[1] * position[2] - position[1] * up[2];

      if (normalize3d(cross) <= *(float *)0x2533c0) {
        /* Degenerate cross product: use random direction */
        unsigned int *seed = (unsigned int *)get_global_random_seed_address();
        random_seed_get_direction3d(seed, cross);
      }

      {
        unsigned int *seed = (unsigned int *)get_global_random_seed_address();
        float factor = random_math_real(seed) * vel_mag * *(float *)0x2568bc;
        *(float *)(item_obj + 0x3c) += cross[0] * factor;
        *(float *)(item_obj + 0x40) += cross[1] * factor;
        *(float *)(item_obj + 0x44) += cross[2] * factor;
      }
    } else {
      /* Slow/grounded path: apply random angular jolt from ground normal */
      marker_count = object_get_markers_by_string_id(
        item_handle, (void *)0x28aa90, marker_buf, 1);
      if (marker_count == 0) {
        /* No marker: use global up vector as normal */
        up = *(float **)0x31fc44;
        cross[0] = up[0];
        cross[1] = up[1];
        cross[2] = up[2];
      } else {
        /* Use ground point marker normal (at marker+0x54) */
        cross[0] = *(float *)(marker_buf + 0x54);
        cross[1] = *(float *)(marker_buf + 0x58);
        cross[2] = *(float *)(marker_buf + 0x5c);
      }

      /* Random angle in [-pi/4, pi/4] */
      scale = FUN_000121e0(-1.5707963f, 1.5707963f);
      scaled_dir[0] = cross[0] * scale;
      scaled_dir[1] = cross[1] * scale;
      scaled_dir[2] = cross[2] * scale;

      /* angular_velocity += scaled_dir */
      vector3d_add(
        (float *)(item_obj + 0x3c), scaled_dir,
        (float *)(item_obj + 0x3c)); /* dup-args-ok: in-place accumulation */
    }

    /* Update item velocity/angular state and clear garbage flag */
    FUN_000f6b80(item_handle);
    object_set_garbage_flag(item_handle, 0);
  }

  /* Collision depth unguard */
  if (*(int16_t *)0x4761d8 < 2) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\items\\items.c", 0x28b, 1);
    system_exit(-1);
  }
  *(int16_t *)0x4761d8 = *(int16_t *)0x4761d8 - 1;
}
