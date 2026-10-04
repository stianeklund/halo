/* Update-queue state. Layouts are read off this file's own accesses:
 * update_server_new/update_client_new clear exactly 0x410c/0x10494 bytes,
 * the rings are 0x20 x 0x208 (server, index & 0x1f) and 0x80 x 0x208
 * (client, index & 0x7f), and the queue data_t pools are
 * data_new(..., 0x10, 0x28). "initialized", "queue_index" and
 * "queue->current_action.desired_facing.*" come from this file's asserts. */

/* size=0x204 — one server update: action count (0 = empty, 0xffff = gap
 * marker written by update_client_handle_server_update, valid 1..16) and
 * one action per queue datum. */
typedef struct server_update_t {
  uint16_t action_count;        /* 0x00 */
  uint8_t pad_02[2];            /* 0x02 */
  player_action_t actions[16];  /* 0x04 */
} server_update_t;
cs(server_update_t, 0x204);
co(server_update_t, action_count, 0x00);
co(server_update_t, actions, 0x04);

/* size=0x208 — one ring entry */
typedef struct update_t {
  int update_number;       /* 0x00 */
  server_update_t update;  /* 0x04 */
} update_t;
cs(update_t, 0x208);
co(update_t, update_number, 0x00);
co(update_t, update, 0x04);

/* size=0x28 — element of the "update server queues" pool */
typedef struct update_server_queue_datum_t {
  int16_t datum_salt;              /* 0x00 data_t element prefix */
  uint8_t pad_02[2];               /* 0x02 */
  int next_update_number;          /* 0x04 */
  player_action_t current_action;  /* 0x08 */
} update_server_queue_datum_t;
cs(update_server_queue_datum_t, 0x28);
co(update_server_queue_datum_t, next_update_number, 0x04);
co(update_server_queue_datum_t, current_action, 0x08);

/* size=0x28 — element of the "update client queues" pool. The three index
 * names are from update_client_dequeue's "queue->..." asserts; the rest
 * mirror the player_action_t fields they are copied from/to. */
typedef struct update_client_queue_datum_t {
  int16_t datum_salt;              /* 0x00 data_t element prefix */
  uint8_t pad_02[2];               /* 0x02 */
  uint32_t buttons;                /* 0x04 */
  uint32_t latched_buttons;        /* 0x08 kept as buttons & 0x4d0 */
  real desired_facing_yaw;         /* 0x0c */
  real desired_facing_pitch;       /* 0x10 */
  real throttle_x;                 /* 0x14 */
  real throttle_y;                 /* 0x18 */
  real primary_trigger;            /* 0x1c */
  int16_t desired_weapon_index;    /* 0x20 */
  int16_t desired_grenade_index;   /* 0x22 */
  int16_t desired_zoom_level;      /* 0x24 */
  uint8_t pad_26[2];               /* 0x26 */
} update_client_queue_datum_t;
cs(update_client_queue_datum_t, 0x28);
co(update_client_queue_datum_t, buttons, 0x04);
co(update_client_queue_datum_t, latched_buttons, 0x08);
co(update_client_queue_datum_t, desired_facing_yaw, 0x0c);
co(update_client_queue_datum_t, desired_facing_pitch, 0x10);
co(update_client_queue_datum_t, throttle_x, 0x14);
co(update_client_queue_datum_t, throttle_y, 0x18);
co(update_client_queue_datum_t, primary_trigger, 0x1c);
co(update_client_queue_datum_t, desired_weapon_index, 0x20);
co(update_client_queue_datum_t, desired_grenade_index, 0x22);
co(update_client_queue_datum_t, desired_zoom_level, 0x24);

/* size=0x410c at 0x4570c0 */
typedef struct update_server_globals_t {
  bool initialized;                  /* 0x00 */
  uint8_t pad_01[3];                 /* 0x01 */
  int next_update_number_to_build;   /* 0x04 */
  data_t *queues;                    /* 0x08 */
  update_t updates[0x20];            /* 0x0c */
} update_server_globals_t;
cs(update_server_globals_t, 0x410c);
co(update_server_globals_t, next_update_number_to_build, 0x04);
co(update_server_globals_t, queues, 0x08);
co(update_server_globals_t, updates, 0x0c);

/* size=0x10494 at 0x45b1d0 */
typedef struct update_client_globals_t {
  bool initialized;                             /* 0x00 */
  uint8_t pad_01[3];                            /* 0x01 */
  int next_update_number_to_dequeue;            /* 0x04 */
  int latest_update_number_received;            /* 0x08 */
  player_action_t saved_action_collection[4];   /* 0x0c */
  int current_local_player;                     /* 0x8c */
  data_t *queues;                               /* 0x90 */
  update_t updates[0x80];                       /* 0x94 */
} update_client_globals_t;
cs(update_client_globals_t, 0x10494);
co(update_client_globals_t, next_update_number_to_dequeue, 0x04);
co(update_client_globals_t, latest_update_number_received, 0x08);
co(update_client_globals_t, saved_action_collection, 0x0c);
co(update_client_globals_t, current_local_player, 0x8c);
co(update_client_globals_t, queues, 0x90);
co(update_client_globals_t, updates, 0x94);

#define update_server_globals (*(update_server_globals_t *)0x4570c0)
#define update_client_globals (*(update_client_globals_t *)0x45b1d0)

/* Reserve a server-side update-queue slot for a player datum handle.
 *
 * Server-side mirror of update_client_add_player (0xb8f00): allocates a
 * datum in the update-server queue data_t at 0x4570c8 keyed by the caller's
 * player handle. NONE (-1) is fatal.
 *
 * Naming is INFERRED, not string-proven: the assert file string is
 * player_queues_new.c and the assert line (0xeb = 235) falls between
 * update_server_start (0xcf) and update_server_build_server_update (0x11a), i.e. the
 * exact source position mirroring update_client_add_player relative to
 * update_client_start. Shape is byte-for-byte the client sibling with the
 * server global substituted. */
void update_server_add_player(int handle)
{
  int queue_index;
  queue_index = data_new_datum(update_server_globals.queues, handle);
  if (queue_index == -1) {
    display_assert("queue_index!=NONE",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0xeb, 1);
    system_exit(-1);
  }
}

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
  if (update_client_globals.initialized != 0) {
    display_assert("!update_client_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x146, 1);
    system_exit(-1);
  }
  csmemset(&update_client_globals, 0, sizeof(update_client_globals));
  update_client_globals.queues = data_new("update client queues", 0x10, 0x28);
  if (update_client_globals.queues != NULL) {
    csmemset(update_client_globals.updates, 0xFF,
             sizeof(update_client_globals.updates));
    update_client_globals.latest_update_number_received = -1;
    update_client_globals.next_update_number_to_dequeue = 0;
    update_client_globals.initialized = 1;
    return true;
  }
  return update_client_globals.initialized;
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

  if (update_client_globals.initialized == 0) {
    display_assert("update_client_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x168, 1);
    system_exit(-1);
  }
  data_delete_all(update_client_globals.queues);
  data_make_valid(update_client_globals.queues);
  data_iterator_new(&iter, player_data);
  while (data_iterator_next(&iter) != NULL) {
    queue_index = data_new_datum(update_client_globals.queues, (int)iter.datum_handle);
    if (queue_index == -1) {
      display_assert("queue_index!=NONE",
                     "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x176, 1);
      system_exit(-1);
    }
  }
}

void update_client_add_player(int handle)
{
  int queue_index;
  queue_index = data_new_datum(update_client_globals.queues, handle);
  if (queue_index == -1) {
    display_assert("queue_index!=NONE",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x182, 1);
    system_exit(-1);
  }
}

void update_client_queue(void *data)
{
  qmemcpy(&update_client_globals
               .saved_action_collection[update_client_globals.current_local_player],
          data, sizeof(player_action_t));
  update_client_globals.current_local_player =
      update_client_globals.current_local_player + 1;
}

void update_client_queue_push(void)
{
  update_client_globals.current_local_player = 0;
  csmemset(update_client_globals.saved_action_collection, 0,
           sizeof(update_client_globals.saved_action_collection));
}

/* Return the number of queued action ticks (inclusive range from
 * first_action_index to last_action_index in the client globals). */
int update_client_get_maximum_actions(void)
{
  return update_client_globals.latest_update_number_received - update_client_globals.next_update_number_to_dequeue +
         1; /* hazard-ok: value-arithmetic (queue count = last-first+1) */
}

/* Copy the current client action collection from update_client_globals.
 * Asserts that action_collection is non-NULL and the client subsystem is
 * initialized, then copies 0x80 (128) bytes from update_client_globals+0x0c
 * (address 0x45b1dc) into the caller-provided buffer. */
void update_client_build_client_update(void *action_collection)
{
  if (!action_collection || update_client_globals.initialized == 0) {
    display_assert("action_collection && update_client_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x244, 1);
    system_exit(-1);
  }
  csmemcpy(action_collection, update_client_globals.saved_action_collection,
           sizeof(update_client_globals.saved_action_collection));
}

int player_new_queue(int handle)
{
  int queue_index;
  queue_index = data_new_datum(update_server_globals.queues, handle);
  if (queue_index == -1) {
    display_assert("queue_index!=NONE",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x292, 1);
    system_exit(-1);
  }
  return queue_index;
}

/* Look up a snapshot buffer entry by index. Returns a pointer into the
 * circular buffer at 0x4570cc (32 entries of 0x208 bytes each), or NULL
 * if the index is outside the valid window [current - 32, current).
 * snapshot_index is passed in EAX (register arg). */
void *update_server_get_update(int snapshot_index /* @<eax> */)
{
  if (update_server_globals.initialized == 0) {
    display_assert("update_server_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x29e, 1);
    system_exit(-1);
  }

  if (snapshot_index < update_server_globals.next_update_number_to_build &&
      snapshot_index >= update_server_globals.next_update_number_to_build - 0x20) {
    return &update_server_globals.updates[snapshot_index & 0x1f];
  }
  return (void *)0;
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
  if (update_server_globals.initialized != 0) {
    display_assert("!update_server_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0xac, 1);
    system_exit(-1);
  }
  csmemset(&update_server_globals, 0, sizeof(update_server_globals));
  update_server_globals.queues = data_new("update server queues", 0x10, 0x28);
  if (update_server_globals.queues != NULL) {
    csmemset(update_server_globals.updates, 0,
             sizeof(update_server_globals.updates));
    if (update_client_new()) {
      update_server_globals.initialized = 1;
      return true;
    }
  }
  return update_server_globals.initialized;
}

/* Tear down both server and client update queue subsystems.
 *
 * Disposes the server queue data_t at 0x4570c8 if non-null, clears
 * server initialized flag and 0x4570c4. Then disposes the client queue
 * data_t at 0x45b260 if non-null, and resets client globals
 * (first_action_index=0, initialized=0, last_action_index=-1). */
void update_server_delete(void)
{
  if (update_server_globals.queues != NULL) {
    data_dispose(update_server_globals.queues);
    update_server_globals.queues = NULL;
  }
  update_server_globals.initialized = 0;
  update_server_globals.next_update_number_to_build = 0;
  if (update_client_globals.queues != NULL) {
    data_dispose(update_client_globals.queues);
    update_client_globals.queues = NULL;
  }
  update_client_globals.next_update_number_to_dequeue = 0;
  update_client_globals.initialized = 0;
  update_client_globals.latest_update_number_received = -1;
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

  if (update_server_globals.initialized == 0) {
    display_assert("update_server_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0xcf, 1);
    system_exit(-1);
  }
  data_delete_all(update_server_globals.queues);
  data_make_valid(update_server_globals.queues);
  data_iterator_new(&iter, player_data);
  while (data_iterator_next(&iter) != NULL) {
    queue_index = data_new_datum(update_server_globals.queues, (int)iter.datum_handle);
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
void update_server_build_server_update(int machine_index, void *update_buf,
                              int *update_number)
{
  update_server_queue_datum_t *queue;
  update_t *update;

  system_milliseconds();

  queue = NULL;

  if (update_buf == NULL || update_number == NULL ||
      update_server_globals.initialized == 0) {
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
    queue = (update_server_queue_datum_t *)datum_get(
        update_server_globals.queues, machine_index);
    if (queue->next_update_number <
        update_server_globals.next_update_number_to_build) {
      *update_number = queue->next_update_number;
    } else {
      *update_number = -1;
      return;
    }
  }

  if (*update_number != -1) {
    /* Look up the update buffer entry for this snapshot index. */
    update = (update_t *)update_server_get_update(*update_number);
    if (update != NULL) {
      csmemcpy(update_buf, &update->update, sizeof(server_update_t));
    }
    if (queue != NULL) {
      queue->next_update_number = queue->next_update_number + 1;
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
bool update_client_dequeue(void *action_buf)
{
  int first;
  update_t *update;
  uint16_t action_count;
  data_t *queue;
  update_client_queue_datum_t *datum;
  player_action_t *src;
  int16_t i;
  int idx;
  player_action_t *out;
  int16_t desired_weapon;
  int16_t desired_grenade;
  int16_t desired_zoom;

  if (update_client_globals.initialized == 0) {
    display_assert("update_client_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1af, 1);
    system_exit(-1);
  }

  first = update_client_globals.next_update_number_to_dequeue;
  if (first >= first + 0x80)
    return false;

  update = &update_client_globals.updates[first & 0x7f];
  if (update == NULL)
    return false;

  if (first > update_client_globals.latest_update_number_received)
    return false;

  action_count = update->update.action_count;
  if (action_count == 0 || action_count > 0x10)
    return false;

  /* Loop 1: copy action data from the ring entry into the queue datums.
   * Dword copies (not float loads) in the original, kept as such. */
  queue = update_client_globals.queues;
  i = 0;
  if (i < queue->current_count) {
    datum = (update_client_queue_datum_t *)queue->data;
    do {
      idx = (int)i;
      if (idx < (int)update->update.action_count) {
        src = &update->update.actions[idx];
        datum->buttons = src->buttons;
        *(uint32_t *)&datum->desired_facing_yaw =
            *(uint32_t *)&src->desired_facing_yaw;
        *(uint32_t *)&datum->desired_facing_pitch =
            *(uint32_t *)&src->desired_facing_pitch;
        *(uint32_t *)&datum->throttle_x = *(uint32_t *)&src->throttle_x;
        *(uint32_t *)&datum->throttle_y = *(uint32_t *)&src->throttle_y;
        *(uint32_t *)&datum->primary_trigger =
            *(uint32_t *)&src->primary_trigger;
        datum->desired_weapon_index = src->desired_weapon_index;
        datum->desired_grenade_index = src->desired_grenade_index;
        datum->desired_zoom_level = src->desired_zoom_level;

        desired_weapon = datum->desired_weapon_index;
        if (desired_weapon != -1 &&
            (desired_weapon < 0 || desired_weapon >= 4)) {
          display_assert(
            "(NONE == queue->desired_weapon_index) || "
            "(queue->desired_weapon_index>=0 && "
            "queue->desired_weapon_index<MAXIMUM_WEAPONS_PER_UNIT)",
            "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1c9, 1);
          system_exit(-1);
        }
        desired_grenade = datum->desired_grenade_index;
        if (desired_grenade != -1 &&
            (desired_grenade < 0 || desired_grenade >= 2)) {
          display_assert(
            "(NONE == queue->desired_grenade_index) || "
            "(queue->desired_grenade_index>=0 && "
            "queue->desired_grenade_index<NUMBER_OF_UNIT_GRENADE_TYPES)",
            "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1ca, 1);
          system_exit(-1);
        }
        queue = update_client_globals.queues;
        desired_zoom = datum->desired_zoom_level;
        if (desired_zoom != -1 && desired_zoom < 0) {
          display_assert("(NONE == queue->desired_zoom_level) || "
                         "(queue->desired_zoom_level>=0)",
                         "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1cb,
                         1);
          system_exit(-1);
          queue = update_client_globals.queues;
        }
      }
      i++;
      datum++;
    } while (i < queue->current_count);
  }

  /* Loop 2: newly pressed buttons (~latched & current) go out, the latch
   * keeps current & 0x4d0, the rest of the action is copied as dwords. */
  i = 0;
  if (i < queue->current_count) {
    datum = (update_client_queue_datum_t *)queue->data;
    do {
      out = (player_action_t *)action_buf + (int)i;
      out->buttons = ~datum->latched_buttons & datum->buttons;
      datum->latched_buttons = datum->buttons & 0x4d0;
      *(uint32_t *)&out->desired_facing_yaw =
          *(uint32_t *)&datum->desired_facing_yaw;
      *(uint32_t *)&out->desired_facing_pitch =
          *(uint32_t *)&datum->desired_facing_pitch;
      *(uint32_t *)&out->throttle_x = *(uint32_t *)&datum->throttle_x;
      *(uint32_t *)&out->throttle_y = *(uint32_t *)&datum->throttle_y;
      *(uint32_t *)&out->primary_trigger =
          *(uint32_t *)&datum->primary_trigger;
      out->desired_weapon_index = datum->desired_weapon_index;
      out->desired_grenade_index = datum->desired_grenade_index;
      out->desired_zoom_level = datum->desired_zoom_level;

      desired_weapon = out->desired_weapon_index;
      if (desired_weapon != -1 && (desired_weapon < 0 || desired_weapon >= 4)) {
        display_assert(
          "(NONE == actions[queue_index].desired_weapon_index) || "
          "(actions[queue_index].desired_weapon_index>=0 && "
          "actions[queue_index].desired_weapon_index<MAXIMUM_WEAPONS_PER_"
          "UNIT)",
          "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1e6, 1);
        system_exit(-1);
      }
      desired_grenade = out->desired_grenade_index;
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
      desired_zoom = out->desired_zoom_level;
      if (desired_zoom != -1 && desired_zoom < 0) {
        display_assert("(NONE == actions[queue_index].desired_zoom_level) || "
                       "(actions[queue_index].desired_zoom_level>=0)",
                       "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x1e8, 1);
        system_exit(-1);
      }
      i++;
      datum++;
    } while (i < update_client_globals.queues->current_count);
  }

  update_client_globals.next_update_number_to_dequeue = update_client_globals.next_update_number_to_dequeue + 1;
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
int update_client_get_maximum_possible_server_time(void)
{
  int first;
  int last;
  int tick;
  update_t *update;
  uint16_t action_count;

  first = update_client_globals.next_update_number_to_dequeue;
  last = update_client_globals.latest_update_number_received;
  tick = first;

  if (first <= last) {
    do {
      if (tick < first)
        break;
      if (tick >= first + 0x80)
        break;

      update = &update_client_globals.updates[tick & 0x7f];
      if (update == NULL)
        break;

      action_count = update->update.action_count;
      if (action_count <= 0)
        break;
      if (action_count > 16)
        break;

      tick++;
    } while (tick <= last);
  }

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
void update_server_apply_actions(int machine_index, void *actions)
{
  int *player_list;
  int player_handle;
  update_server_queue_datum_t *queue;
  player_action_t *src;
  player_action_t *next_src;
  int i;
  uint32_t pitch_bits;
  uint32_t yaw_bits;

  player_list = (int *)machine_get_player_list(machine_index);

  if (update_server_globals.initialized == 0) {
    display_assert("update_server_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x22a, 1);
    system_exit(-1);
  }

  i = 0;
  src = (player_action_t *)actions;
  do {
    player_handle = player_list[i];
    if (player_handle != -1) {
      queue = (update_server_queue_datum_t *)datum_get(
          update_server_globals.queues, player_handle);
      next_src = src + 1;

      /* REP MOVSD: copy 8 dwords (0x20 bytes) into current_action */
      qmemcpy(&queue->current_action, src, sizeof(player_action_t));
      src = next_src;

      /* assert_valid_real on current_action.desired_facing.pitch */
      pitch_bits = *(uint32_t *)&queue->current_action.desired_facing_pitch;
      if ((pitch_bits & 0x7f800000u) == 0x7f800000u) {
        display_assert(
          csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
                   "queue->current_action.desired_facing.pitch", pitch_bits,
                   (double)queue->current_action.desired_facing_pitch),
          "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x238, 1);
        system_exit(-1);
      }

      /* assert_valid_real on current_action.desired_facing.yaw */
      yaw_bits = *(uint32_t *)&queue->current_action.desired_facing_yaw;
      if ((yaw_bits & 0x7f800000u) == 0x7f800000u) {
        display_assert(
          csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
                   "queue->current_action.desired_facing.yaw", yaw_bits,
                   (double)queue->current_action.desired_facing_yaw),
          "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x239, 1);
        system_exit(-1);
      }
    }
    i++;
  } while (i < 4);
}

/* Store a client update snapshot into the action ring-buffer at 0x45b264.
 *
 * The ring buffer holds 0x80 slots of 0x208 bytes each (base 0x45b264).
 * Each slot consists of a 4-byte sequence index followed by 0x204 bytes
 * of update data. The slot is selected by (sequence_index & 0x7f).
 *
 * If sequence_index is within [first_action_index, first_action_index+0x80):
 *   - Writes sequence_index at slot+0x00.
 *   - Copies 0x204 bytes from data into slot+0x04 via csmemcpy.
 *   - If sequence_index > last_action_index, updates last_action_index.
 *     If there is a gap (last_action_index+1 < sequence_index before update),
 *     marks slot+0x04 with 0xFFFF (16-bit) to signal the gap.
 *
 * If out of range, checks whether we are the network server or client.
 * If neither, and not in the main menu, logs three error messages.
 *
 * Tail-calls main_menu_is_active() when game_connection() returns 0..2. */
void update_client_handle_server_update(void *data, int sequence_index)
{
  /* first_action_index (0x45b1d4), last_action_index (0x45b1d8) */
  int first_idx;
  update_t *update;
  int last_idx;
  const char *map_name;
  int conn;

  first_idx = update_client_globals.next_update_number_to_dequeue;
  if (sequence_index >= first_idx && sequence_index < first_idx + 0x80) {
    update = &update_client_globals.updates[sequence_index & 0x7f];
    /* Binary has TEST EAX,EAX; JZ — always non-NULL but preserved. */
    if (update != NULL) {
      update->update_number = sequence_index;
      csmemcpy(&update->update, data, sizeof(server_update_t));
      last_idx = update_client_globals.latest_update_number_received;
      if (sequence_index > last_idx) {
        if (last_idx + 1 < sequence_index) {
          /* Gap in sequence: mark first word of data area invalid. */
          update->update.action_count = 0xffff;
        }
        update_client_globals.latest_update_number_received = sequence_index;
      }
      goto done;
    }
  }

  if (global_network_game_client_get() != (void *)0) {
    if (global_network_game_server_get() != (void *)0) {
      goto done;
    }
  }
  if (!main_menu_is_active()) {
    map_name = main_get_map_name();
    error(2, "failed to get an update (#%d); sp scenario= '%s'", sequence_index,
          map_name);
    error(2, "if you're in a multiplayer game, you might be out of sync now");
    error(2,
          "if you're playing single player/coop, you can probably ignore this");
  }

done:
  conn = game_connection();
  if (conn >= 0 && conn <= 2) {
    main_menu_is_active();
  }
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
void update_server_next_update(void)
{
  int old_index;
  update_t *entry;
  server_update_t *update;
  int16_t i;
  data_t *queue;
  update_server_queue_datum_t *datum;

  old_index = update_server_globals.next_update_number_to_build;

  if (update_server_globals.initialized == 0) {
    display_assert("update_server_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0xfa, 1);
    system_exit(-1);
  }

  update_server_globals.next_update_number_to_build = old_index + 1;

  /* Look up the circular update buffer entry for old_index. */
  entry = (update_t *)update_server_get_update(old_index);
  if (entry == NULL) {
    display_assert("update", "c:\\halo\\SOURCE\\game\\player_queues_new.c",
                   0x100, 1);
    system_exit(-1);
  }

  entry->update_number = old_index;
  update = &entry->update;
  update->action_count = 0;

  queue = update_server_globals.queues;
  i = 0;
  if (i < queue->current_count) {
    datum = (update_server_queue_datum_t *)queue->data;
    do {
      csmemcpy(&entry->update.actions[i], &datum->current_action,
               sizeof(player_action_t));
      update->action_count++;
      i++;
      datum++;
    } while (i < update_server_globals.queues->current_count);
  }

  /* Push the snapshot into the client ring (0xb97b0). */
  update_client_handle_server_update(update, old_index);
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
 * update_server_next_update(), then retrieves the update data into a
 * local 0x204-byte buffer via update_server_build_server_update(0, ..., &ticks).
 * The ticks parameter is passed by address to update_server_build_server_update,
 * which may modify it as the update number. */
void update_client_local_ticks(int16_t ticks)
{
  char local_actions[0x80];
  char update_buf[0x204];
  int tick_count;

  if (game_connection() != 0) {
    display_assert("game_connection()==_game_connection_local",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x20b, 1);
    system_exit(-1);
  }

  if (update_client_globals.initialized == 0) {
    display_assert("action_collection && update_client_globals.initialized",
                   "c:\\halo\\SOURCE\\game\\player_queues_new.c", 0x244, 1);
    system_exit(-1);
  }

  csmemcpy(local_actions, update_client_globals.saved_action_collection, 0x80);
  update_server_apply_actions(0, local_actions);

  if (ticks > 0) {
    tick_count = (uint16_t)ticks;
    do {
      update_server_next_update();
      update_server_build_server_update(0, update_buf, (int *)&ticks);
      tick_count--;
    } while (tick_count != 0);
  }
}

/* 0xb8e00 — update_client_delete */
void update_client_delete(void)
{
  if (*(data_t **)0x45b260 == NULL) {
    *(int *)0x45b1d8 = -1;
    *(int *)0x45b1d4 = 0;
    *(uint8_t *)0x45b1d0 = 0;
    return;
  }
  data_dispose(*(data_t **)0x45b260);
  *(uint8_t *)0x45b1d0 = 0;
  *(int *)0x45b1d4 = 0;
  *(int *)0x45b1d8 = -1;
  *(data_t **)0x45b260 = NULL;
}

/* 0xb90a0 — update_client_get_update */
void *update_client_get_update(int sequence_index)
{
  if (sequence_index >= *(int *)0x45b1d4 && sequence_index < *(int *)0x45b1d4 + 0x80) {
    return (void *)(0x45b264 + (sequence_index & 0x7f) * 0x208);
  }
  return NULL;
}

/* update_queues_reset_and_fill_with_lies (0xb9880) — after-load callback in
 * the game_state_revert table at 0x32eaa8.
 *
 * Re-seats both update queues on the restored game time:
 *   - server: next_update_number_to_build = 0, updates[] zeroed;
 *   - client: updates[] filled with 0xff, saved_action_collection zeroed,
 *     latest_update_number_received = -1, next_update_number_to_dequeue = 0;
 *     then ring entries are faked for the last (up to) 0x80 ticks before the
 *     current game time (update_number, action_count 1, zeroed actions).
 *     Entries are written from updates[0] upward, not at (tick & 0x7f).
 *     Finally next-to-dequeue = time, server next-to-build = time, latest
 *     received = time - 1, stored in that order;
 *   - server again: update_server_start, then the queue datum at absolute
 *     index 0 of the server pool gets next_update_number = next-to-build;
 *     otherwise, if the client is initialized, update_client_start (the
 *     original tail-jumps to 0xb8e40 from a block placed after the RET).
 *
 * The window start is max(time - 0x80, 0): LEA/TEST/SETL/DEC/AND, signed.
 * The loop compare (CMP EDI,EBX; JGE / JL) is signed too. */
void update_queues_reset_and_fill_with_lies(void)
{
  int game_time;
  int update_number;
  update_t *update;
  update_server_queue_datum_t *queue;

  if (update_server_globals.initialized) {
    update_server_globals.next_update_number_to_build = 0;
    csmemset(update_server_globals.updates, 0,
             sizeof(update_server_globals.updates));
  }
  if (update_client_globals.initialized) {
    csmemset(update_client_globals.updates, 0xff,
             sizeof(update_client_globals.updates));
    csmemset(update_client_globals.saved_action_collection, 0,
             sizeof(update_client_globals.saved_action_collection));
    update_client_globals.latest_update_number_received = -1;
    update_client_globals.next_update_number_to_dequeue = 0;
    game_time = game_time_get();
    update_number = game_time - 0x80 < 0 ? 0 : game_time - 0x80;
    update = update_client_globals.updates;
    for (; update_number < game_time; update_number++) {
      update->update_number = update_number;
      update->update.action_count = 1;
      csmemset(update->update.actions, 0, sizeof(update->update.actions));
      update++;
    }
    update_client_globals.next_update_number_to_dequeue = game_time;
    update_server_globals.next_update_number_to_build = game_time;
    update_client_globals.latest_update_number_received = game_time - 1;
  }
  if (update_server_globals.initialized) {
    update_server_start();
    queue = (update_server_queue_datum_t *)datum_get(
        update_server_globals.queues, 0);
    queue->next_update_number =
        update_server_globals.next_update_number_to_build;
  } else if (update_client_globals.initialized) {
    update_client_start();
  }
}
