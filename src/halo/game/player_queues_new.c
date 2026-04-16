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
