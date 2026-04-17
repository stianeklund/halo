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
