/* Initialize the client-side update queue subsystem.
 *
 * Asserts that the client globals are NOT already initialized, then zeros
 * the entire 0x10494-byte update_client_globals block at 0x45b1d0.
 * Allocates a data_t with 16 slots of 0x28 bytes each for "update client
 * queues". On success, fills the 0x10400-byte action buffer at 0x45b264
 * with 0xFF, sets first_action_index=0, last_action_index=-1, and marks
 * initialized. Returns true on success, or the current initialized state
 * (false) on allocation failure. */
bool update_client_new(void)
{
  if (*(uint8_t *)0x45b1d0 != 0) {
    display_assert("!update_client_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x146, 1);
    system_exit(-1);
  }
  csmemset((void *)0x45b1d0, 0, 0x10494);
  *(data_t **)0x45b260 = data_new("update client queues", 0x10, 0x28);
  if (*(data_t **)0x45b260 != NULL) {
    csmemset((void *)0x45b264, 0xFF, 0x10400);
    *(int *)0x45b1d8 = -1;
    *(int *)0x45b1d4 = 0;
    *(uint8_t *)0x45b1d0 = 1;
    return true;
  }
  return *(uint8_t *)0x45b1d0;
}

/* Reset the client-side action queue storage and allocate one queue slot
 * per currently-active player datum.
 *
 * update_client_globals lives at 0x45b1d0 (byte "initialized" flag at
 * +0x00, queue data_t* at +0x90 = 0x45b260). The function asserts the
 * module was initialized, then:
 *   1. data_delete_all(queue) to reset all slots
 *   2. data_make_valid(queue) to mark the table live again
 *   3. Iterate every active player handle in player_data and reserve a
 *      queue datum keyed by that handle. NONE from data_new_datum is
 *      fatal (asserts "queue_index!=NONE"). */
void update_client_start(void)
{
  data_iter_t iter;
  int queue_index;

  if (*(uint8_t *)0x45b1d0 == 0) {
    display_assert("update_client_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x168, 1);
    system_exit(-1);
  }
  data_delete_all(*(data_t **)0x45b260);
  data_make_valid(*(data_t **)0x45b260);
  data_iterator_new(&iter, player_data);
  while (data_iterator_next(&iter) != NULL) {
    queue_index = data_new_datum(*(data_t **)0x45b260, (int)iter.datum_handle);
    if (queue_index == -1) {
      display_assert("queue_index!=NONE",
                     "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x176, 1);
      system_exit(-1);
    }
  }
}

/* Return the number of queued action ticks (inclusive range from
 * first_action_index to last_action_index in the client globals). */
int update_get_maximum_actions(void)
{
  return *(int *)0x45b1d8 - *(int *)0x45b1d4 + 1;
}

/* Initialize the server-side update queue subsystem.
 *
 * Asserts that the server globals are NOT already initialized, then zeros
 * the entire 0x410c-byte update_server_globals block at 0x4570c0.
 * Allocates a data_t with 16 slots of 0x28 bytes each for "update server
 * queues". On success, zeros the 0x4100-byte buffer at 0x4570cc, then
 * calls update_client_new to initialize the client side as well.
 * Returns true if both succeed, or the current initialized state on
 * failure. */
bool update_server_new(void)
{
  if (*(uint8_t *)0x4570c0 != 0) {
    display_assert("!update_server_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0xac, 1);
    system_exit(-1);
  }
  csmemset((void *)0x4570c0, 0, 0x410c);
  *(data_t **)0x4570c8 = data_new("update server queues", 0x10, 0x28);
  if (*(data_t **)0x4570c8 != NULL) {
    csmemset((void *)0x4570cc, 0, 0x4100);
    if (update_client_new()) {
      *(uint8_t *)0x4570c0 = 1;
      return true;
    }
  }
  return *(uint8_t *)0x4570c0;
}

/* Tear down both server and client update queue subsystems.
 *
 * Disposes the server queue data_t at 0x4570c8 if non-null, clears
 * server initialized flag and 0x4570c4. Then disposes the client queue
 * data_t at 0x45b260 if non-null, and resets client globals
 * (first_action_index=0, initialized=0, last_action_index=-1). */
void update_server_delete(void)
{
  if (*(data_t **)0x4570c8 != NULL) {
    data_dispose(*(data_t **)0x4570c8);
    *(data_t **)0x4570c8 = NULL;
  }
  *(uint8_t *)0x4570c0 = 0;
  *(int *)0x4570c4 = 0;
  if (*(data_t **)0x45b260 != NULL) {
    data_dispose(*(data_t **)0x45b260);
    *(data_t **)0x45b260 = NULL;
  }
  *(int *)0x45b1d4 = 0;
  *(uint8_t *)0x45b1d0 = 0;
  *(int *)0x45b1d8 = -1;
}

/* Prepare both server and client queues for a new frame.
 *
 * Asserts server globals are initialized. Resets the server queue via
 * data_delete_all + data_make_valid, then iterates every active player
 * datum and allocates a corresponding slot in the server queue (asserts
 * on failure). Finally calls update_client_start to do the same for the
 * client queue. */
void update_server_start(void)
{
  data_iter_t iter;
  int queue_index;

  if (*(uint8_t *)0x4570c0 == 0) {
    display_assert("update_server_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0xcf, 1);
    system_exit(-1);
  }
  data_delete_all(*(data_t **)0x4570c8);
  data_make_valid(*(data_t **)0x4570c8);
  data_iterator_new(&iter, player_data);
  while (data_iterator_next(&iter) != NULL) {
    queue_index = data_new_datum(*(data_t **)0x4570c8, (int)iter.datum_handle);
    if (queue_index == -1) {
      display_assert("queue_index!=NONE",
                     "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0xdd, 1);
      system_exit(-1);
    }
  }
  update_client_start();
}

/* Retrieve the next server update snapshot for a given machine.
 *
 * Calls system_milliseconds() for a timing checkpoint (return discarded).
 * Asserts that update_buf, update_number, and server globals are valid.
 * If machine_index != -1, looks up the machine's queue datum via datum_get
 * and compares its stored snapshot counter (datum+4) against the global
 * snapshot index (0x4570c4). If the datum is already at or past the current
 * index, sets *update_number = -1 and returns (no update available).
 * Otherwise, sets *update_number to the datum's counter.
 *
 * If *update_number != -1, looks up the server update buffer entry for
 * that snapshot index (via the internal helper at 0xb9040 with @eax),
 * and copies 0x204 bytes from entry+4 into update_buf. Then increments
 * the datum's snapshot counter. */
void update_server_get_update(int machine_index, void *update_buf,
                              int *update_number)
{
  void *datum_ptr;
  void *update_entry;

  system_milliseconds();

  datum_ptr = NULL;

  if (update_buf == NULL || update_number == NULL ||
      *(uint8_t *)0x4570c0 == 0) {
    display_assert(
      "update && update_number && update_server_globals.initialized",
      "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x11a, 1);
    system_exit(-1);
  }

  if (machine_index != -1) {
    if (machine_index >= 4) {
      display_assert("machine_index<MAXIMUM_NETWORK_MACHINE_COUNT",
                     "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x11e, 1);
      system_exit(-1);
    }
    datum_ptr = datum_get(*(data_t **)0x4570c8, machine_index);
    if (*(int *)((char *)datum_ptr + 4) >= *(int *)0x4570c4) {
      *update_number = -1;
      return;
    }
    *update_number = *(int *)((char *)datum_ptr + 4);
  }

  if (*update_number != -1) {
    /* Call internal helper at 0xb9040 with snapshot index in EAX.
     * Returns pointer to the update buffer entry, or NULL. */
    {
      int _eax = *update_number;
      __asm__ __volatile__("call *%[fn]"
                           : "+a"(_eax)
                           : [fn] "r"((void *)0xb9040)
                           : "ecx", "edx", "memory", "cc");
      update_entry = (void *)_eax;
    }
    if (update_entry != NULL) {
      csmemcpy(update_buf, (char *)update_entry + 4, 0x204);
    }
    if (datum_ptr != NULL) {
      *(int *)((char *)datum_ptr + 4) = *(int *)((char *)datum_ptr + 4) + 1;
    }
  }
}

/* Collect current player actions from the client action queue.
 *
 * Asserts that update_client_globals is initialized. Reads the next action
 * buffer slot (circular, indexed by first_action_index & 0x7F) and copies
 * action data from each slot entry into the corresponding queue datum.
 *
 * Loop 1: For each datum in the client queue (data_t at 0x45b260), if the
 *   datum index is within the slot's action_count, copies the 0x1E-byte
 *   player action (buttons, facing, throttle, trigger, weapon/grenade/zoom
 *   indices) from the action buffer slot into the datum at offsets +0x04
 *   through +0x24. Validates desired_weapon_index (range [0,3] or NONE),
 *   desired_grenade_index (range [0,1] or NONE), and desired_zoom_level
 *   (>= 0 or NONE).
 *
 * Loop 2: For each datum, computes newly-pressed buttons as ~prev & new
 *   (where prev = datum+0x08, new = datum+0x04), stores that in the output
 *   buffer. Updates datum+0x08 = new & 0x4D0 (persistent button mask for
 *   bits 4,6,7,10). Copies the remaining action fields into the output.
 *   Re-validates weapon/grenade/zoom on the output copy.
 *
 * On success, increments first_action_index and returns true.
 * Returns false if the action buffer has no valid data available. */
bool player_control_get_current_actions(void *action_buf)
{
  int first;
  int slot_addr;
  uint16_t action_count;
  data_t *queue;
  char *datum_ptr;
  char *src;
  int16_t i;
  int idx;
  char *out;
  int16_t desired_weapon;
  int16_t desired_grenade;
  int16_t desired_zoom;

  if (*(uint8_t *)0x45b1d0 == 0) {
    display_assert("update_client_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1af, 1);
    system_exit(-1);
  }

  first = *(int *)0x45b1d4;
  if (first >= first + 0x80)
    return false;

  slot_addr = (first & 0x7f) * 0x208 + 0x45b264;
  if (slot_addr == 0)
    return false;

  if (first > *(int *)0x45b1d8)
    return false;

  action_count = *(uint16_t *)(slot_addr + 4);
  if (action_count == 0 || action_count > 0x10)
    return false;

  /* Loop 1: copy action data from the action buffer slot into queue datums. */
  queue = *(data_t **)0x45b260;
  i = 0;
  if (i < queue->current_count) {
    datum_ptr = (char *)queue->data + 0x20;
    do {
      idx = (int)i;
      if (idx < (int)(uint16_t)(*(uint16_t *)(slot_addr + 4))) {
        src = (char *)(slot_addr + 8 + idx * 0x20);
        *(uint32_t *)(datum_ptr - 0x1c) = *(uint32_t *)(src + 0x00);
        *(uint32_t *)(datum_ptr - 0x14) = *(uint32_t *)(src + 0x04);
        *(uint32_t *)(datum_ptr - 0x10) = *(uint32_t *)(src + 0x08);
        *(uint32_t *)(datum_ptr - 0x0c) = *(uint32_t *)(src + 0x0c);
        *(uint32_t *)(datum_ptr - 0x08) = *(uint32_t *)(src + 0x10);
        *(uint32_t *)(datum_ptr - 0x04) = *(uint32_t *)(src + 0x14);
        *(int16_t *)(datum_ptr + 0x00) = *(int16_t *)(src + 0x18);
        *(int16_t *)(datum_ptr + 0x02) = *(int16_t *)(src + 0x1a);
        *(int16_t *)(datum_ptr + 0x04) = *(int16_t *)(src + 0x1c);

        desired_weapon = *(int16_t *)(datum_ptr + 0x00);
        if (desired_weapon != -1 &&
            (desired_weapon < 0 || desired_weapon >= 4)) {
          display_assert(
            "(NONE == queue->desired_weapon_index) || "
            "(queue->desired_weapon_index>=0 && "
            "queue->desired_weapon_index<MAXIMUM_WEAPONS_PER_UNIT)",
            "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1c9, 1);
          system_exit(-1);
        }
        desired_grenade = *(int16_t *)(datum_ptr + 0x02);
        if (desired_grenade != -1 &&
            (desired_grenade < 0 || desired_grenade >= 2)) {
          display_assert(
            "(NONE == queue->desired_grenade_index) || "
            "(queue->desired_grenade_index>=0 && "
            "queue->desired_grenade_index<NUMBER_OF_UNIT_GRENADE_TYPES)",
            "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1ca, 1);
          system_exit(-1);
        }
        queue = *(data_t **)0x45b260;
        desired_zoom = *(int16_t *)(datum_ptr + 0x04);
        if (desired_zoom != -1 && desired_zoom < 0) {
          display_assert("(NONE == queue->desired_zoom_level) || "
                         "(queue->desired_zoom_level>=0)",
                         "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1cb,
                         1);
          system_exit(-1);
          queue = *(data_t **)0x45b260;
        }
      }
      i++;
      datum_ptr += 0x28;
    } while (i < queue->current_count);
  }

  /* Loop 2: compute newly-pressed buttons, update persistent flags, copy
   * action data into the output buffer (param_1). */
  i = 0;
  if (i < queue->current_count) {
    char *di = (char *)queue->data + 0x08;
    do {
      out = (char *)action_buf + (int)i * 0x20;
      *(uint32_t *)(out + 0x00) = ~(*(uint32_t *)di) & *(uint32_t *)(di - 0x04);
      *(uint32_t *)di = *(uint32_t *)(di - 0x04) & 0x4d0;
      *(uint32_t *)(out + 0x04) = *(uint32_t *)(di + 0x04);
      *(uint32_t *)(out + 0x08) = *(uint32_t *)(di + 0x08);
      *(uint32_t *)(out + 0x0c) = *(uint32_t *)(di + 0x0c);
      *(uint32_t *)(out + 0x10) = *(uint32_t *)(di + 0x10);
      *(uint32_t *)(out + 0x14) = *(uint32_t *)(di + 0x14);
      *(int16_t *)(out + 0x18) = *(int16_t *)(di + 0x18);
      *(int16_t *)(out + 0x1a) = *(int16_t *)(di + 0x1a);
      *(int16_t *)(out + 0x1c) = *(int16_t *)(di + 0x1c);

      desired_weapon = *(int16_t *)(out + 0x18);
      if (desired_weapon != -1 && (desired_weapon < 0 || desired_weapon >= 4)) {
        display_assert(
          "(NONE == actions[queue_index].desired_weapon_index) || "
          "(actions[queue_index].desired_weapon_index>=0 && "
          "actions[queue_index].desired_weapon_index<MAXIMUM_WEAPONS_PER_"
          "UNIT)",
          "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1e6, 1);
        system_exit(-1);
      }
      desired_grenade = *(int16_t *)(out + 0x1a);
      if (desired_grenade != -1 &&
          (desired_grenade < 0 || desired_grenade >= 2)) {
        display_assert(
          "(NONE == actions[queue_index].desired_grenade_index) || "
          "(actions[queue_index].desired_grenade_index>=0 && "
          "actions[queue_index].desired_grenade_index<NUMBER_OF_UNIT_"
          "GRENADE_TYPES)",
          "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1e7, 1);
        system_exit(-1);
      }
      desired_zoom = *(int16_t *)(out + 0x1c);
      if (desired_zoom != -1 && desired_zoom < 0) {
        display_assert("(NONE == actions[queue_index].desired_zoom_level) || "
                       "(actions[queue_index].desired_zoom_level>=0)",
                       "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1e8, 1);
        system_exit(-1);
      }
      i++;
      di += 0x28;
    } while (i < (*(data_t **)0x45b260)->current_count);
  }

  *(int *)0x45b1d4 = *(int *)0x45b1d4 + 1;
  return true;
}

/* Scan the client action buffer forward from first_action_index and return
 * the tick index at which valid contiguous actions end.
 *
 * The action buffer at 0x45b264 is organized as 128 circular slots, each
 * 0x208 bytes. At offset +4 within each slot is a uint16_t action count.
 * The function walks from first_action_index toward last_action_index,
 * checking that each slot's action count is in the range [1, 16]. It
 * stops (and returns the current tick) as soon as a slot is empty (0),
 * over-full (>16), or the tick exceeds the valid range. The returned
 * value represents the "game time" — the furthest tick for which the
 * client has submitted valid action data. */
int update_get_game_time(void)
{
  int first;
  int last;
  int tick;
  int slot_addr;
  uint16_t action_count;

  first = *(int *)0x45b1d4;
  last = *(int *)0x45b1d8;
  tick = first;

  if (first > last)
    goto done;

  for (;;) {
    if (tick < first)
      break;
    if (tick >= first + 0x80)
      break;

    slot_addr = (tick & 0x7f) * 0x208 + 0x45b264;
    if (slot_addr == 0)
      break;

    action_count = *(uint16_t *)(slot_addr + 4);
    if (action_count == 0)
      break;
    if (action_count > 16)
      break;

    tick++;
    if (tick > last)
      break;
  }

done:
  return tick;
}

/* Apply player actions from a network machine into the server queue.
 *
 * Retrieves the player list for the given machine_index via
 * machine_get_player_list. Asserts that server globals are initialized.
 * Iterates over 4 player slots in the machine's player list. For each
 * valid (non-NONE) player handle, looks up the corresponding datum in the
 * server queue via datum_get, then copies 0x20 bytes (8 dwords via REP
 * MOVSD) from the actions buffer into the datum at offset +0x08.
 *
 * After each copy, validates the desired_facing.pitch (datum+0x10) and
 * desired_facing.yaw (datum+0x0c) floats with assert_valid_real checks
 * (rejects NaN/Inf values where the exponent bits are all 1s).
 *
 * The actions pointer advances by 0x20 bytes per player slot. */
void update_server_apply_actions(int16_t machine_index, void *actions)
{
  int *player_list;
  int player_handle;
  char *datum_ptr;
  char *src;
  char *next_src;
  int i;
  uint32_t pitch_bits;
  uint32_t yaw_bits;

  player_list = (int *)machine_get_player_list(machine_index);

  if (*(uint8_t *)0x4570c0 == 0) {
    display_assert("update_server_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x22a, 1);
    system_exit(-1);
  }

  i = 0;
  src = (char *)actions;
  do {
    player_handle = player_list[i];
    if (player_handle != -1) {
      datum_ptr = (char *)datum_get(*(data_t **)0x4570c8, player_handle);
      next_src = src + 0x20;

      /* REP MOVSD: copy 8 dwords (0x20 bytes) from src to datum+8 */
      csmemcpy(datum_ptr + 8, src, 0x20);
      src = next_src;

      /* assert_valid_real on desired_facing.pitch (datum+0x10) */
      pitch_bits = *(uint32_t *)(datum_ptr + 0x10);
      if ((pitch_bits & 0x7f800000u) == 0x7f800000u) {
        char *msg =
          csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
                   "queue->current_action.desired_facing.pitch", pitch_bits,
                   (double)*(float *)(datum_ptr + 0x10));
        display_assert(msg, "c:\\halo\\SOURCE\\game\\player_queues_new.c",
                       0x238, 1);
        system_exit(-1);
      }

      /* assert_valid_real on desired_facing.yaw (datum+0x0c) */
      yaw_bits = *(uint32_t *)(datum_ptr + 0x0c);
      if ((yaw_bits & 0x7f800000u) == 0x7f800000u) {
        char *msg =
          csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
                   "queue->current_action.desired_facing.yaw", yaw_bits,
                   (double)*(float *)(datum_ptr + 0x0c));
        display_assert(msg, "c:\\halo\\SOURCE\\game\\player_queues_new.c",
                       0x239, 1);
        system_exit(-1);
      }
    }
    i++;
  } while (i < 4);
}

/* Create a server-side update snapshot from the current server queue state.
 *
 * Asserts server globals are initialized. Saves the current snapshot index
 * from 0x4570c4, then increments it. Looks up the update buffer entry for
 * the old index via the internal helper at 0xb9040 (@eax register arg).
 * Asserts the entry is non-null.
 *
 * Stores the old snapshot index as the entry's tick number (entry+0x00),
 * zeros the action count (entry+0x04, word). Then iterates over all datums
 * in the server queue (data_t at 0x4570c8), copying 0x20 bytes from each
 * datum's action data (datum+0x08) into sequential 0x20-byte slots in the
 * update entry starting at entry+0x08. Increments the action count for
 * each datum copied.
 *
 * Finally, calls the internal store function at 0xb97b0 to push the
 * snapshot into the client-side action buffer. */
void update_server_create_snapshot(void)
{
  int old_index;
  void *entry;
  int16_t *action_count_ptr;
  int16_t i;
  data_t *queue;
  char *datum_data;

  if (*(uint8_t *)0x4570c0 == 0) {
    display_assert("update_server_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0xfa, 1);
    system_exit(-1);
  }

  old_index = *(int *)0x4570c4;
  *(int *)0x4570c4 = *(int *)0x4570c4 + 1;

  /* Call internal helper at 0xb9040 with old_index in EAX.
   * Returns pointer to the circular update buffer entry. */
  {
    int _eax = old_index;
    __asm__ __volatile__("call *%[fn]"
                         : "+a"(_eax)
                         : [fn] "r"((void *)0xb9040)
                         : "ecx", "edx", "memory", "cc");
    entry = (void *)_eax;
  }
  if (entry == NULL) {
    display_assert("update", "c:\\halo\\SOURCE\\game\\player_queues_new.c",
                   0x100, 1);
    system_exit(-1);
  }

  *(int *)entry = old_index;
  action_count_ptr = (int16_t *)((char *)entry + 4);
  *action_count_ptr = 0;

  queue = *(data_t **)0x4570c8;
  i = 0;
  if (i < queue->current_count) {
    datum_data = (char *)queue->data + 8;
    do {
      csmemcpy((char *)entry + 8 + (int)i * 0x20, datum_data, 0x20);
      (*action_count_ptr)++;
      i++;
      datum_data += 0x28;
    } while (i < (*(data_t **)0x4570c8)->current_count);
  }

  /* Call internal store function at 0xb97b0(action_count_ptr, old_index)
   * to push the snapshot into the client action buffer. */
  ((void(__cdecl *)(int16_t *, int))0xb97b0)(action_count_ptr, old_index);
}

/* Apply queued client actions for the given number of simulation ticks.
 *
 * Asserts that this is a local (non-networked) game connection and that
 * the client update globals are initialized. Copies the current 0x80-byte
 * action state from update_client_globals+0x0C (0x45b1dc) into a local
 * buffer, then applies those actions to the server queue via
 * update_server_apply_actions(0, ...).
 *
 * For each tick, creates a new server update snapshot via
 * update_server_create_snapshot(), then retrieves the update data into a
 * local 0x204-byte buffer via update_server_get_update(0, ..., &ticks).
 * The ticks parameter is passed by address to update_server_get_update,
 * which may modify it as the update number. */
void update_client_apply_actions(int16_t ticks)
{
  char local_actions[0x80];
  char update_buf[0x204];
  int tick_count;

  if (game_connection() != 0) {
    display_assert("game_connection()==_game_connection_local",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x20b, 1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x45b1d0 == 0) {
    display_assert("action_collection && update_client_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x244, 1);
    system_exit(-1);
  }

  csmemcpy(local_actions, (void *)0x45b1dc, 0x80);
  update_server_apply_actions(0, local_actions);

  if (ticks > 0) {
    tick_count = (uint16_t)ticks;
    do {
      update_server_create_snapshot();
      update_server_get_update(0, update_buf, (int *)&ticks);
      tick_count--;
    } while (tick_count != 0);
  }
}
