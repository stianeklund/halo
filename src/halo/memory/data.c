int data_allocation_size(__int16 count, __int16 size)
{
  return size * count + sizeof(data_t);
}

void data_initialize(data_t *data, char *name, __int16 maximum_count,
                     __int16 size)
{
  assert_halt(maximum_count > 0);
  assert_halt(size > 0);
  assert_halt(name);
  assert_halt(data);

  csmemset(data, 0, sizeof(*data));
  csstrncpy(data->name, name, sizeof(data->name) - 1);
  data->maximum_count = maximum_count;
  data->size = size;
  data->magic = 0x64407440;
  data->data = &data[1];
  data->valid = 0;
}

int datum_absolute_index_to_index(data_t *data, int absolute_index)
{
  int16_t identifier;
  int16_t index;
  int16_t *datum;

  if (absolute_index == NONE)
    return 0;

  assert_halt(data->valid);

  identifier = (int16_t)(absolute_index >> 16);
  if (identifier == 0 && data->identifier_zero_invalid) {
    assert_halt_msg(0, "identifier || !data->identifier_zero_invalid");
  }

  index = (int16_t)absolute_index;
  if (index >= 0 && index < data->current_count) {
    datum = (int16_t *)((char *)data->data + data->size * index);
    if (*datum != 0) {
      if (identifier == 0)
        return (int)datum;
      if (*datum == identifier)
        return (int)datum;
    }
  }
  return 0;
}

void *datum_get(data_t *data, int datum_handle)
{
  int16_t identifier; // salt (upper 16 bits)
  int16_t index; // index (lower 16 bits)
  int16_t *datum;

  assert_halt(data->valid);

  identifier = (int16_t)(datum_handle >> 16);
  if (identifier == 0 && data->identifier_zero_invalid) {
    assert_halt_msg(0, "identifier || !data->identifier_zero_invalid");
  }

  index = (int16_t)datum_handle;
  if (index >= 0 && index < data->current_count) {
    datum = (int16_t *)((char *)data->data + data->size * index);
    if (*datum != 0) {
      if (identifier == 0)
        return datum;
      if (identifier == *datum)
        return datum;
    }
  }

  display_assert(csprintf(error_string_buffer,
                          "%s index #%d (0x%x) is unused or changed",
                          data->name, datum_handle & 0xffff, datum_handle),
                 __FILE__, __LINE__, true);
  system_exit(-1);
  return NULL;
}

void data_verify(data_t *data)
{
  assert_halt(data);
  assert_halt(data->data);
  assert_halt(data->magic == 0x64407440);
  assert_halt(data->maximum_count >= 0);
  assert_halt(data->current_count >= 0);
  assert_halt(data->current_count <= data->maximum_count);
  assert_halt(*(int16_t *)data->unk_44 >= 0);
  assert_halt(*(int16_t *)data->unk_44 <= data->maximum_count);
  assert_halt(data->unk_48 >= 0);
  assert_halt(data->unk_48 <= data->current_count);
}

/*
 * datum_initialize — zero a datum slot and stamp it with the next identifier.
 *
 * Zeroes the datum buffer (data->size bytes), copies the current identifier
 * counter (data->unk_50) into the datum's first 2 bytes, then increments the
 * counter. Wraps 0 → 0x8000 to keep valid (non-null) identifiers.
 *
 * Confirmed: ESI = data_t*, EDI = datum buffer pointer (both register args).
 * Confirmed: CALL 0x8db80 (csmemset) with (EDI, 0, data->size).
 * Confirmed: 16-bit copy [ESI+0x32] → [EDI]; INC word [ESI+0x32]; wrap
 * 0→0x8000.
 */
void datum_initialize(data_t *data, void *datum)
{
  csmemset(datum, 0, (int)data->size);
  *(int16_t *)datum = *(int16_t *)((char *)data + 0x32);
  *(int16_t *)((char *)data + 0x32) += 1;
  if (*(int16_t *)((char *)data + 0x32) == 0)
    *(int16_t *)((char *)data + 0x32) = 0x8000;
}

data_t *data_new(char *name, int16_t maximum_count, int16_t size)
{
  data_t *data;

  data = (data_t *)debug_malloc((int)maximum_count * (int)size + sizeof(data_t),
                                0, "c:\\halo\\SOURCE\\memory\\data.c", 0x29);
  if (data)
    data_initialize(data, name, maximum_count, size);
  return data;
}

void data_dispose(data_t *data)
{
  data_verify(data);
  csmemset(data, 0, sizeof(data_t));
  debug_free(data, "c:\\halo\\SOURCE\\memory\\data.c", 0x59);
}

void data_make_invalid(data_t *data)
{
  data_verify(data);
  data->valid = 0;
}

int data_new_datum(data_t *data, int handle)
{
  int16_t index;
  int16_t identifier;
  int16_t *datum;

  data_verify(data);
  assert_halt(data->valid);

  index = (int16_t)handle;
  identifier = (int16_t)((uint32_t)handle >> 16);
  if (index >= 0 && index < data->maximum_count && identifier != 0) {
    datum = (int16_t *)((char *)data->data + data->size * index);
    if (*datum == 0) {
      data->unk_48 = data->unk_48 + 1;
      if (data->current_count <= index)
        data->current_count = index + 1;
      csmemset(datum, 0, data->size);
      *datum = *(int16_t *)data->unk_50;
      *(int16_t *)data->unk_50 = *(int16_t *)data->unk_50 + 1;
      if (*(int16_t *)data->unk_50 == 0)
        *(uint16_t *)data->unk_50 = 0x8000;
      *datum = identifier;
      return (int)identifier << 16 | (int)index;
    }
  }
  return NONE;
}

int data_new_at_index(data_t *data)
{
  int16_t search;
  int size;
  int16_t *datum;

  data_verify(data);
  assert_halt(data->valid);

  search = *(int16_t *)data->unk_44;
  size = (int)data->size;
  datum = (int16_t *)((char *)data->data + size * search);
  if (search < data->maximum_count) {
    do {
      if (*datum == 0) {
        csmemset(datum, 0, size);
        *datum = *(int16_t *)data->unk_50;
        *(int16_t *)data->unk_50 = *(int16_t *)data->unk_50 + 1;
        if (*(int16_t *)data->unk_50 == 0)
          *(uint16_t *)data->unk_50 = 0x8000;
        data->unk_48 = data->unk_48 + 1;
        *(int16_t *)data->unk_44 = search + 1;
        if (data->current_count <= search)
          data->current_count = search + 1;
        return (int)*datum << 16 | (int)search;
      }
      search++;
      datum = (int16_t *)((char *)datum + size);
    } while (search < data->maximum_count);
  }
  return NONE;
}

void datum_delete(data_t *data, int datum_handle)
{
  int16_t *datum;
  int16_t index;

  datum = (int16_t *)datum_get(data, datum_handle);
  *datum = 0;
  index = (int16_t)datum_handle;
  if (index < *(int16_t *)data->unk_44)
    *(int16_t *)data->unk_44 = index;
  if (index + 1 == (int)data->current_count) {
    do {
      datum = (int16_t *)((char *)datum - data->size);
      data->current_count = data->current_count - 1;
      if (data->current_count < 1)
        break;
    } while (*datum == 0);
  }
  data->unk_48 = data->unk_48 - 1;
}

void data_make_valid(data_t *data)
{
  int16_t i;

  data_verify(data);
  assert_halt(data->valid);

  data->current_count = 0;
  data->unk_48 = 0;
  *(int16_t *)data->unk_44 = 0;
  csstrncpy(data->unk_50, data->name, 2);
  *(uint16_t *)data->unk_50 |= 0x8000;

  for (i = 0; i < data->maximum_count; i++) {
    *(int16_t *)((char *)data->data + data->size * i) = 0;
  }
}

void data_iterator_new(data_iter_t *iter, data_t *data)
{
  data_verify(data);
  assert_halt(data->valid);

  iter->data = data;
  iter->cookie = (unsigned int)data ^ 0x69746572;
  iter->index = 0;
  iter->datum_handle = -1;
}

void *data_iterator_next(data_iter_t *iterator)
{
  int16_t index; // cx
  void *result; // eax
  int handle; // edx
  size_t size; // [esp+14h] [ebp+8h]

  assert_halt_msg(iterator->cookie == ((int)iterator->data->name ^ 0x69746572),
                  "uninitialized iterator passed to iterator_next()");
  data_verify(iterator->data);
  assert_halt(iterator->data->valid);

  index = iterator->index;
  size = iterator->data->size;
  result = (char *)iterator->data->data + iterator->data->size * index;
  if (index >= iterator->data->current_count) {
    iterator->index = index;
    return 0;
  } else {
    while (1) {
      handle = index++ | (*(__int16 *)result << 16);
      if (*(_WORD *)result)
        break;
      result = (char *)result + size;
      if (index >= iterator->data->current_count) {
        iterator->index = index;
        return 0;
      }
    }
    iterator->datum_handle = handle;
    iterator->index = index;
  }
  return result;
}

int data_next_index(data_t *data, int prev_index)
{
  int16_t index;
  int16_t *datum;

  index = (int16_t)(prev_index + 1);
  data_verify(data);
  assert_halt(data->valid);

  if (index >= 0 && index < data->current_count) {
    datum = (int16_t *)((char *)data->data + data->size * index);
    do {
      if (*datum != 0)
        return (int)*datum << 16 | (int)index;
      index++;
      datum = (int16_t *)((char *)datum + data->size);
    } while (index < data->current_count);
  }
  return NONE;
}

void data_delete_all(data_t *data)
{
  data_verify(data);
  data->valid = 1;
  data_make_valid(data);
}
