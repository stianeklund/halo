/* Recorded animation thread system — plays back scripted unit animations
 * for cinematics and AI scripted sequences. */

/* Allocate animation thread data array and debug tracking buffer. */
void recorded_animations_initialize(void)
{
  *(void **)0x44df04 = game_state_data_new("recorded animations", 0x40, 100);
  if (!*(void **)0x44df04) {
    display_assert("animation_threads",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x6c,
                   1);
    system_exit(-1);
  }

  *(void **)0x44df0c = ((void *(*)(int, int, const char *, int))0x8ee60)(
    0x400, 0, "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x6f);
  if (!*(void **)0x44df0c) {
    display_assert("animation_threads_debug",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x70,
                   1);
    system_exit(-1);
  }
}

/* Free the debug tracking buffer. */
void recorded_animations_dispose(void)
{
  if (*(void **)0x44df0c != 0) {
    ((void (*)(void *, const char *, int))0x8ef70)(
      *(void **)0x44df0c, "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c",
      0x7b);
    *(void **)0x44df0c = 0;
  }
}

/* Mark animation thread data as invalid for old map disposal. */
void recorded_animations_dispose_from_old_map(void)
{
  ((void (*)(void *))0x119550)(*(void **)0x44df04);
}

/* Advance all active recorded animation threads by one tick.
 *
 * For each allocated thread, either:
 *   - dispose it (object gone, or finished flag set) by clearing debug slot,
 *     restoring the unit's animation-driven flags, and deleting the datum;
 *   - otherwise, tick its per-type event stream via vtable dispatch, sanity
 *     check against the recorded debug state, and apply the resulting frame
 *     to the unit. The vtable returns "still has events" — the finished bit
 *     is set when the vtable reports zero (stream exhausted).
 */
void recorded_animations_update(void)
{
  data_iter_t iter;
  char *thread;
  char *dbg_slot;
  char stream_active;
  int *relative_ticks;
  uint16_t flags;
  int dbg_index;
  void **vtable;
  int stream_delta;
  scenario_t *scenario;
  char *anim_def;
  char *msg;

  data_iterator_new(&iter, *(data_t **)0x44df04);
  thread = (char *)data_iterator_next(&iter);
  while (thread != NULL) {
    if (((int (*)(int, int))0x13d640)(*(int *)(thread + 4), 3) == 0) {
      datum_delete(*(data_t **)0x44df04, iter.datum_handle);
    } else {
      flags = *(uint16_t *)(thread + 0xa);
      if ((flags & 1) == 0) {
        /* Active thread: tick the per-type event stream via vtable. The
         * callback returns nonzero while events remain in the stream and
         * zero once the stream is exhausted. */
        *(int16_t *)(thread + 8) = *(int16_t *)(thread + 8) - 1;
        relative_ticks = (int *)(thread + 0xc);
        vtable = (void **)((void **)0x2eebb0)[*(int16_t *)(thread + 0x60)];
        stream_active = ((char (*)(char *, char *, int *, int *))vtable[1])(
                          thread + 0x54, thread + 0x14, relative_ticks,
                          (int *)(thread + 0x10)) ?
                          1 :
                          0;
        if (*relative_ticks < 0) {
          display_assert("thread->relative_ticks>=0",
                         "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c",
                         0x15b, 1);
          system_exit(-1);
        }
        dbg_index = (iter.datum_handle & 0xffff) * 0x10;
        dbg_slot = (char *)(dbg_index + *(int *)0x44df0c);
        if (*dbg_slot != 0) {
          stream_delta = *(int *)(thread + 0x10) - *(int *)(dbg_slot + 4);
          /* Assert holds when stream_delta is below the recorded length, or
           * exactly at the end while events are still being produced. */
          if (!(stream_delta < *(int *)(dbg_slot + 8) ||
                (stream_delta == *(int *)(dbg_slot + 8) &&
                 stream_active != 0))) {
            display_assert(
              "thread->event_stream-thread_debug->event_stream_start<"
              "thread_debug->stream_length||(thread->event_stream-thread_debug"
              "->event_stream_start==thread_debug->stream_length&&finished)",
              "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x162, 1);
            system_exit(-1);
          }
        }
        *relative_ticks = *relative_ticks + 1;
        ((void (*)(int, char *))0x1af990)(*(int *)(thread + 4), thread + 0x14);
        /* Stream exhausted → set finished bit so next tick takes the
         * cleanup path. Stream still active → keep the thread alive. */
        if (stream_active != 0)
          *(uint8_t *)(thread + 0xa) = *(uint8_t *)(thread + 0xa) & 0xfe;
        else
          *(uint8_t *)(thread + 0xa) = *(uint8_t *)(thread + 0xa) | 1;
      } else {
        /* Finished thread: clean up and delete. */
        dbg_index = (iter.datum_handle & 0xffff) * 0x10;
        dbg_slot = (char *)(dbg_index + *(int *)0x44df0c);
        if (*dbg_slot != 0 && (flags & 2) == 0 &&
            *(int16_t *)(thread + 8) != 0) {
          scenario = global_scenario_get();
          anim_def = (char *)tag_block_get_element(
            (char *)scenario + 0x36c, *(int16_t *)(dbg_slot + 0xc), 0x40);
          msg = csprintf((char *)0x5ab100, "animation %s appears corrupt",
                         anim_def);
          display_assert(
            msg, "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x175, 1);
          system_exit(-1);
        }
        *dbg_slot = 0;
        ((void (*)(int, int))0x1a9a50)(*(int *)(thread + 4),
                                       (*(uint8_t *)(thread + 0xa) >> 2) & 1);
        ((void (*)(int, int))0x1a9a90)(*(int *)(thread + 4), 0);
        ((void (*)(int, int))0x1adf10)(*(int *)(thread + 4), 0);
        ((void (*)(int, int))0x13ff50)(*(int *)(thread + 4), 1);
        if ((*(uint8_t *)(thread + 0xa) & 8) != 0)
          ((void (*)(int))0xc99e0)(*(int *)(thread + 4));
        if ((*(uint8_t *)(thread + 0xa) & 0x10) != 0)
          ((void (*)(int, int))0x1b5610)(*(int *)(thread + 4), 1);
        datum_delete(*(data_t **)0x44df04, iter.datum_handle);
      }
    }
    thread = (char *)data_iterator_next(&iter);
  }
}

/* Clear animation threads and zero the debug buffer for a new map. */
void recorded_animations_initialize_for_new_map(void)
{
  ((void (*)(void *))0x119b20)(*(void **)0x44df04);
  if (!*(void **)0x44df0c) {
    display_assert("animation_threads_debug",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x99,
                   1);
    system_exit(-1);
  }
  csmemset(*(void **)0x44df0c, 0, 0x400);
}
