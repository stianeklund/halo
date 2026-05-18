/* Validate that a circular queue structure is not corrupt (0x118d70).
 * Checks: non-null pointer, signature == "circ" (0x63697263), non-null buffer,
 * positive size, and read/write offsets within [0, size). If any check fails,
 * reports the corruption via display_assert and halts with system_exit(-1). */
void FUN_00118d70(int queue)
{
  int size;

  if (queue != 0 && *(int *)(queue + 0x04) == 0x63697263 &&
      *(int *)(queue + 0x14) != 0 &&
      (size = *(int *)(queue + 0x10), size > 0) &&
      *(int *)(queue + 0x08) >= 0 && *(int *)(queue + 0x08) < size &&
      *(int *)(queue + 0x0c) >= 0 && *(int *)(queue + 0x0c) < size) {
    return;
  }

  display_assert(csprintf((char *)0x5ab100,
                          "the circular queue @%p appears to be corrupt.",
                          (void *)queue),
                 "c:\\halo\\SOURCE\\memory\\circular_queue.c", 0xcc, 1);
  system_exit(-1);
}

/* Return the number of free bytes available in a circular queue (0x118e90).
 * Computes: buffer_size - used - 1, where used = (write_offset - read_offset),
 * wrapping around via buffer_size when write_offset < read_offset. The -1
 * accounts for the sentinel gap that distinguishes full from empty. */
unsigned int circular_queue_free_space(int queue)
{
  int used;

  FUN_00118d70(queue);

  used = *(int *)(queue + 0x0c) - *(int *)(queue + 0x08);
  if (used < 0) {
    used = used + *(int *)(queue + 0x10);
  }
  return *(int *)(queue + 0x10) - used - 1;
}

/* Allocate and initialize a circular queue with a buffer of param_2 bytes.
 * Returns the queue pointer or NULL if allocation fails.
 * 0x118de0 / circular_queue.obj
 */
int *circular_queue_new(int param_1, int param_2)
{
    int *puVar1;

    puVar1 = (int *)debug_malloc(param_2 + 0x19, 0,
                                  "c:\\halo\\SOURCE\\memory\\circular_queue.c", 0x34);
    if (puVar1 != (int *)0) {
        csmemset(puVar1, 0, 0x18);
        *puVar1 = param_1;
        puVar1[1] = 0x63697263;
        puVar1[4] = param_2 + 1;
        puVar1[5] = (int)(puVar1 + 6);
        FUN_00118d70((int)puVar1);
    }
    return puVar1;
}

/* Free a circular queue and its memory. 0x118e40 / circular_queue.obj */
void circular_queue_delete(int queue)
{
    FUN_00118d70(queue);
    debug_free((void *)queue, "c:\\halo\\SOURCE\\memory\\circular_queue.c", 0x48);
}


/* Enqueue data into a circular queue, wrapping around the buffer boundary
 * if necessary (0x118ec0). Returns true if the data was enqueued, or false
 * if the queue does not have enough space. Handles the wrap-around case by
 * splitting the copy into two parts: one to the end of the buffer, and one
 * from the beginning. Asserts validity of the queue before and after. */
bool FUN_00118ec0(int queue, void *data, int data_size)
{
  int write_offset;
  int used;
  int remaining;

  FUN_00118d70(queue);
  assert_halt(data && data_size > 0 && data_size < *(int *)(queue + 0x10));

  FUN_00118d70(queue);

  write_offset = *(int *)(queue + 0x0c);
  used = write_offset - *(int *)(queue + 0x08);
  if (used < 0) {
    used = used + *(int *)(queue + 0x10);
  }

  if (used + data_size >= *(int *)(queue + 0x10)) {
    return 0;
  }

  remaining = *(int *)(queue + 0x10) - write_offset;
  if (data_size >= remaining) {
    csmemcpy((void *)(*(int *)(queue + 0x14) + write_offset), data, remaining);
    data = (char *)data + remaining;
    *(int *)(queue + 0x0c) = 0;
    data_size = data_size - remaining;
  }

  if (data_size > 0) {
    csmemcpy((void *)(*(int *)(queue + 0x14) + *(int *)(queue + 0x0c)), data,
             data_size);
    *(int *)(queue + 0x0c) = *(int *)(queue + 0x0c) + data_size;
  }

  assert_halt(*(int *)(queue + 0x0c) >= 0 &&
              *(int *)(queue + 0x0c) < *(int *)(queue + 0x10));
  return 1;
}
