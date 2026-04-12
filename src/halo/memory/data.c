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

void data_make_invalid(data_t *data)
{
  data_verify(data);
  data->valid = 0;
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

void data_delete_all(data_t *data)
{
  data_verify(data);
  data->valid = 1;
  data_make_valid(data);
}
