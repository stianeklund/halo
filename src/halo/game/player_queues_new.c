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
