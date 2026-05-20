/* Initialize an array header struct: store element_size and zero count/head.
 * Asserts that the table pointer is non-null and element_size > 0.
 * 0x117b20 / circular_queue.obj (array.c line 16-17) */
void array_new(int *table, int element_size)
{
  if (table == (int *)0x0) {
    display_assert("array", "c:\\halo\\SOURCE\\memory\\array.c", 0x10, 1);
    system_exit(-1);
  }
  if (element_size < 1) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\array.c", 0x11,
                   1);
    system_exit(-1);
  }
  table[0] = element_size;
  table[1] = 0;
  table[2] = 0;
}

/* Dispose of a dynamic array: free its element buffer and reset fields.
 * Asserts non-null array, non-negative count, and consistency between
 * count and element pointer. Frees via debug_realloc(ptr, 0) and stores
 * the (NULL) return back into the elements field.
 * 0x117cf0 / circular_queue.obj (array.c line 73) */
void FUN_00117cf0(int *param_1)
{
  if (param_1 == (int *)0x0) {
    display_assert("array", "c:\\halo\\SOURCE\\memory\\array.c", 0x49, 1);
    system_exit(-1);
  }
  if ((int)param_1[1] < 0) {
    display_assert("array->count>=0", "c:\\halo\\SOURCE\\memory\\array.c", 0x4a,
                   1);
    system_exit(-1);
  }
  if ((param_1[1] != 0) != (param_1[2] != 0)) {
    display_assert("(array->count!=0)==(array->elements!=NULL)",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x4b, 1);
    system_exit(-1);
  }
  param_1[0] = (int)0xffffffff;
  param_1[1] = (int)0xffffffff;
  if (param_1[2] != 0) {
    param_1[2] = (int)debug_realloc((void *)param_1[2], 0,
                                    "c:\\halo\\SOURCE\\memory\\array.c", 0x50);
  }
}

/* Return the address of an element at the given index in a dynamic array.
 * Validates array pointer, element_size > 0, element_size == param_3,
 * non-negative count, pointer/count consistency, and index in [0, count).
 * Returns element_size * index + elements (raw address).
 * 0x117ee0 / circular_queue.obj (array.c line 125) */
int FUN_00117ee0(int *array, int index, int element_size)
{
  if (array == (int *)0x0) {
    display_assert("array", "c:\\halo\\SOURCE\\memory\\array.c", 0x7d, 1);
    system_exit(-1);
  }
  if (array[0] < 1) {
    display_assert("array->element_size>0", "c:\\halo\\SOURCE\\memory\\array.c",
                   0x7e, 1);
    system_exit(-1);
  }
  if (array[0] != element_size) {
    display_assert("array->element_size==element_size",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x7f, 1);
    system_exit(-1);
  }
  if (array[1] < 0) {
    display_assert("array->count>=0", "c:\\halo\\SOURCE\\memory\\array.c", 0x80,
                   1);
    system_exit(-1);
  }
  if ((array[1] != 0) != (array[2] != 0)) {
    display_assert("(array->count!=0)==(array->elements!=NULL)",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x81, 1);
    system_exit(-1);
  }
  if ((index < 0) || (array[1] <= index)) {
    display_assert("index>=0 && index<array->count",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x82, 1);
    system_exit(-1);
  }
  return array[0] * index + array[2];
}

/* Initialize a small fixed-size array: zero the count byte and fill
 * all element slots with 0xff (sentinel/invalid). Validates all inputs.
 * 0x118190 / circular_queue.obj (array.c line 171) */
void FUN_00118190(unsigned char *count, int elements, short element_size,
                  short maximum_count)
{
  if (count == (unsigned char *)0x0) {
    display_assert("count", "c:\\halo\\SOURCE\\memory\\array.c", 0xab, 1);
    system_exit(-1);
  }
  if (elements == 0) {
    display_assert("elements", "c:\\halo\\SOURCE\\memory\\array.c", 0xac, 1);
    system_exit(-1);
  }
  if (element_size < 1) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\array.c", 0xad,
                   1);
    system_exit(-1);
  }
  if (0xff < (int)maximum_count) {
    display_assert("maximum_count<=UNSIGNED_CHAR_MAX",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0xae, 1);
    system_exit(-1);
  }
  *count = 0;
  csmemset((void *)elements, 0xffffffff,
           (int)element_size * (int)maximum_count);
}

/* Resize a small fixed-size array to new_count elements. Zeroes newly
 * allocated slots or fills freed slots with 0xff. Returns 1 on success,
 * 0 if new_count is out of range [0, maximum_count).
 * 0x118260 / circular_queue.obj (array.c line 191) */
int FUN_00118260(unsigned char *count, int elements, short element_size,
                 short maximum_count, short new_count)
{
  unsigned int uVar1;
  unsigned int uVar2;

  if (count == (unsigned char *)0x0) {
    display_assert("count && *count>=0", "c:\\halo\\SOURCE\\memory\\array.c",
                   0xbf, 1);
    system_exit(-1);
  }
  if (elements == 0) {
    display_assert("elements", "c:\\halo\\SOURCE\\memory\\array.c", 0xc0, 1);
    system_exit(-1);
  }
  if (element_size < 1) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\array.c", 0xc1,
                   1);
    system_exit(-1);
  }
  if (0xff < (int)maximum_count) {
    display_assert("maximum_count<=UNSIGNED_CHAR_MAX",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0xc2, 1);
    system_exit(-1);
  }
  if ((-1 < (int)new_count) && ((int)new_count < (int)maximum_count)) {
    if ((unsigned int)*count != (unsigned int)(int)new_count) {
      uVar1 = (int)element_size * (unsigned int)*count + elements;
      uVar2 = (int)element_size * (int)new_count + elements;
      if (uVar1 < uVar2) {
        csmemset((void *)uVar1, 0, uVar2 - uVar1);
        *count = (unsigned char)new_count;
        return 1;
      }
      csmemset((void *)uVar2, 0xffffffff, uVar1 - uVar2);
      *count = (unsigned char)new_count;
    }
    return 1;
  }
  return 0;
}

/* Append a new zeroed element to a small fixed-size array.
 * Returns the index of the new element, or 0xffff if the array is full.
 * 0x118370 / circular_queue.obj (array.c line 230) */
unsigned short FUN_00118370(unsigned char *count, int elements,
                            short element_size, short maximum_count)
{
  unsigned char bVar1;

  if (count == (unsigned char *)0x0) {
    display_assert("count && *count>=0", "c:\\halo\\SOURCE\\memory\\array.c",
                   0xe6, 1);
    system_exit(-1);
  }
  if (elements == 0) {
    display_assert("elements", "c:\\halo\\SOURCE\\memory\\array.c", 0xe7, 1);
    system_exit(-1);
  }
  if (element_size < 1) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\array.c", 0xe8,
                   1);
    system_exit(-1);
  }
  if (0xff < (int)maximum_count) {
    display_assert("maximum_count<=UNSIGNED_CHAR_MAX",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0xe9, 1);
    system_exit(-1);
  }
  bVar1 = *count;
  if ((short)(unsigned short)bVar1 < maximum_count) {
    *count = bVar1 + 1;
    csmemset((void *)((int)(short)(unsigned short)bVar1 * (int)element_size +
                      elements),
             0, (int)element_size);
    return (unsigned short)bVar1;
  }
  return 0xffff;
}

/* Return the address of a specific element by index in a small fixed-size
 * array. Validates all inputs including index < *count.
 * 0x118460 / circular_queue.obj (array.c line 251) */
int FUN_00118460(unsigned char count, int elements, short element_size,
                 short index)
{
  if (count == 0) {
    display_assert("count>0", "c:\\halo\\SOURCE\\memory\\array.c", 0xfb, 1);
    system_exit(-1);
  }
  if (elements == 0) {
    display_assert("elements", "c:\\halo\\SOURCE\\memory\\array.c", 0xfc, 1);
    system_exit(-1);
  }
  if (element_size < 1) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\array.c", 0xfd,
                   1);
    system_exit(-1);
  }
  if ((index < 0) || ((short)(unsigned short)count <= index)) {
    display_assert("index>=0 && index<count",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0xfe, 1);
    system_exit(-1);
  }
  return (int)element_size * (int)index + elements;
}

/* Remove an element from a small fixed-size array by index, shifting
 * subsequent elements down and filling the vacated slot with 0xff.
 * 0x118520 / circular_queue.obj (array.c line 265) */
void FUN_00118520(unsigned char *count, int elements, short element_size,
                  short index)
{
  int iVar1;
  int iVar3;
  int iVar4;
  unsigned char bVar2;

  if ((count == (unsigned char *)0x0) || (*count == 0)) {
    display_assert("count && *count>0", "c:\\halo\\SOURCE\\memory\\array.c",
                   0x109, 1);
    system_exit(-1);
  }
  if (elements == 0) {
    display_assert("elements", "c:\\halo\\SOURCE\\memory\\array.c", 0x10a, 1);
    system_exit(-1);
  }
  if (element_size < 1) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\array.c", 0x10b,
                   1);
    system_exit(-1);
  }
  if ((index < 0) || ((short)(unsigned short)*count <= index)) {
    display_assert("index>=0 && index<*count",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x10c, 1);
    system_exit(-1);
  }
  bVar2 = *count - 1;
  *count = bVar2;
  iVar4 = (int)index;
  if (iVar4 < (int)(unsigned int)bVar2) {
    iVar3 = (int)element_size;
    iVar1 = iVar3 * iVar4 + elements;
    csmemmove((void *)iVar1, (const void *)(iVar3 + iVar1),
              ((unsigned int)bVar2 - iVar4) * iVar3);
  }
  csmemset((void *)((unsigned int)*count * (int)element_size + elements),
           0xffffffff, (int)element_size);
}

/* Compute the byte size described by a byte-swap definition by walking
 * the code array with null data. Returns the computed size.
 * 0x118ba0 / circular_queue.obj (byte_swapping.c) */
int FUN_00118ba0(const char *name, int *codes)
{
  int definition[4];
  int out_size;
  int out_step;

  definition[0] = (int)name;
  definition[1] = 0;
  definition[2] = (int)codes;
  definition[3] = 0x62797377;
  out_size = 0;
  out_step = 0;
  FUN_001187f0(definition, 0, codes, &out_size, &out_step);
  return out_size;
}

/* Byte-swap all count instances of data according to the given definition.
 * Validates the definition on first use (computes and caches size). If data
 * is non-null and count > 0, walks each element through the byte-swap codes.
 * 0x118be0 / circular_queue.obj (byte_swapping.c line 77) */
void FUN_00118be0(void *definition, void *data, int count)
{
  int *def;
  int computed_size;
  int out_step;
  int i;

  if (definition == (void *)0x0) {
    display_assert("definition", "c:\\halo\\SOURCE\\memory\\byte_swapping.c",
                   0x4d, 1);
    system_exit(-1);
  }
  def = (int *)definition;
  if ((*(char *)((int *)def + 4) == '\0') && (def[1] >= 0)) {
    FUN_001187f0(def, 0, (int *)def[2], &computed_size, &out_step);
    if (computed_size != def[1]) {
      display_assert(csprintf((char *)0x5ab100,
                              "%s bs data @%p is #%d but should be #%d bytes",
                              (const char *)def[0], def, computed_size, def[1]),
                     "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0x58, 1);
      system_exit(-1);
    }
    *(char *)((int *)def + 4) = '\x01';
  }
  if ((data != (void *)0x0) && (count > 0)) {
    for (i = 0; i < count; i++) {
      FUN_001187f0(def, def[1] * i + (int)data, (int *)def[2], (int *)0x0,
                   (int *)0x0);
    }
  }
}

/* Build a byte-swap definition on the stack and invoke FUN_00118be0 to
 * byte-swap data_count instances of data. Validates codes, data_count,
 * and size before constructing the definition struct.
 * 0x118cb0 / circular_queue.obj (byte_swapping.c line 40) */
void FUN_00118cb0(const char *name, int size, int *codes, int data_count,
                  void *data)
{
  int definition[5];

  if (codes == (int *)0x0) {
    display_assert("codes", "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0x28,
                   1);
    system_exit(-1);
  }
  if (data_count < 0) {
    display_assert("data_count>=0", "c:\\halo\\SOURCE\\memory\\byte_swapping.c",
                   0x29, 1);
    system_exit(-1);
  }
  if (size < 0) {
    display_assert("size>=0", "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0x2a,
                   1);
    system_exit(-1);
  }
  definition[0] = (int)name;
  definition[1] = size;
  definition[2] = (int)codes;
  definition[3] = 0x62797377;
  *(char *)&definition[4] = (data != (void *)0x0);
  FUN_00118be0(definition, data, data_count);
}

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

/* Allocate and initialize a circular queue with a buffer of param_2 bytes.
 * Returns the queue pointer or NULL if allocation fails.
 * 0x118de0 / circular_queue.obj
 */
int *circular_queue_new(int param_1, int param_2)
{
  int *puVar1;

  puVar1 = (int *)debug_malloc(
    param_2 + 0x19, 0, "c:\\halo\\SOURCE\\memory\\circular_queue.c", 0x34);
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
