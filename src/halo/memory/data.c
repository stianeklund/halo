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

/* Find the previous valid element before datum, or before the end if datum==-1.
 * Returns handle (salt<<16|index) or 0xffffffff if none found.
 * 0x119980 / data.obj
 */
unsigned int data_prev_index(data_t *data, int datum)
{
  short *psVar1;
  short sVar2;
  unsigned int uVar3;

  data_verify(data);
  if (!data->valid) {
    display_assert("data->valid", "c:\\halo\\SOURCE\\memory\\data.c", 0x14f, 1);
    system_exit(-1);
  }
  if (datum == -1) {
    uVar3 = (unsigned int)(unsigned short)(data->current_count - 1);
  } else {
    uVar3 = datum - 1;
  }
  sVar2 = (short)uVar3;
  if (sVar2 >= 0 && sVar2 < data->current_count) {
    psVar1 = (short *)((int)sVar2 * data->size + (int)data->data);
    do {
      sVar2 = (short)uVar3;
      if (*psVar1 != 0) {
        return (int)*psVar1 << 16 | (int)sVar2;
      }
      psVar1 = (short *)((int)psVar1 - data->size);
      uVar3--;
    } while (sVar2 >= 0);
  }
  return -1;
}

/* Compact the data array: removes gaps by copying all live elements
 * into a temporary buffer and copying back.
 * 0x119a10 / data.obj
 */
void data_compact(data_t *data)
{
  short sVar1;
  int iVar2;
  short sVar3;
  short *psVar4;
  int iVar5;

  sVar3 = 0;
  iVar2 = (int)debug_malloc((int)data->size * (int)data->maximum_count, 0,
                            "c:\\halo\\SOURCE\\memory\\data.c", 0x1a5);
  data_verify(data);
  if (!data->valid) {
    display_assert("data->valid", "c:\\halo\\SOURCE\\memory\\data.c", 0x1a8, 1);
    system_exit(-1);
  }
  if (iVar2 != 0) {
    psVar4 = data->data;
    sVar1 = 0;
    if (0 < data->current_count) {
      do {
        if (*psVar4 != 0) {
          csmemcpy((void *)(iVar2 + (int)sVar3 * (int)data->size), psVar4,
                   (int)data->size);
          sVar3 = sVar3 + 1;
        }
        sVar1 = sVar1 + 1;
        psVar4 = (short *)((int)psVar4 + data->size);
      } while (sVar1 < data->current_count);
    }
    iVar5 = (int)sVar3;
    csmemcpy(data->data, (void *)iVar2, (int)data->size * iVar5);
    csmemset((void *)(iVar5 * (int)data->size + (int)data->data), 0,
             (int)(data->maximum_count - iVar5) * (int)data->size);
    data->unk_48 = sVar3;
    data->current_count = sVar3;
    *(int16_t *)data->unk_44 = sVar3;
    debug_free((void *)iVar2, "c:\\halo\\SOURCE\\memory\\data.c", 0x1bf);
  }
}

void data_delete_all(data_t *data)
{
  data_verify(data);
  data->valid = 1;
  data_make_valid(data);
}

/* Deserialize a 4-byte big-endian value and delegate to FUN_00110b40.
 * Returns 1 on success, 0 if insufficient data (param_5 <= 3).
 * 0x119b40 / data.obj
 */
int FUN_00119b40(int p1, unsigned int p2, unsigned int *p3, int *p4,
                 unsigned int p5)
{
  int iVar1;

  if (3 < p5) {
    *p3 = (((p2 & 0xff0000) | p2 >> 0x10) >> 8) |
          (((p2 & 0xff00) | p2 << 0x10) << 8);
    *p4 = p5 - 4;
    iVar1 = FUN_00110b40(p3 + 1, p4, p1, p2, 9);
    if (iVar1 == 0) {
      *p4 = *p4 + 4;
      return 1;
    }
  }
  return 0;
}

/* Decode first 4 bytes of buf as a big-endian uint32, if size >= 4.
 * 0x119bb0 / data.obj
 */
unsigned int FUN_00119bb0(unsigned int *buf, unsigned int size)
{
  unsigned int uVar1;

  uVar1 = 0;
  if (3 < size) {
    uVar1 = *buf;
    uVar1 = ((uVar1 & 0xff0000) | uVar1 >> 0x10) >> 8 |
            ((uVar1 << 0x10) | (uVar1 & 0xff00)) << 8;
  }
  return uVar1;
}

/* Byte-swap the first 4 bytes of param_1 (if param_2 > 3), store in *param_4,
 * then call FUN_001179e0 to decode remaining elements.
 * Returns true if FUN_001179e0 returned 0.
 * 0x119bf0 / data.obj (data_encoding.c)
 */
bool FUN_00119bf0(unsigned int *param_1, unsigned int param_2, int param_3,
                  unsigned int *param_4)
{
  unsigned char bVar1;
  unsigned int uVar1;
  int iVar2;

  bVar1 = 0;
  uVar1 = 0;
  if (3 < param_2) {
    uVar1 = *param_1;
    uVar1 = ((uVar1 & 0xff0000) | uVar1 >> 0x10) >> 8 |
            ((uVar1 << 0x10) | (uVar1 & 0xff00)) << 8;
  }
  *param_4 = uVar1;
  iVar2 = FUN_001179e0(param_3, param_4, param_1 + 1, param_2);
  if (iVar2 == 0) {
    bVar1 = 1;
  }
  return bVar1;
}

/* Initialize a data encoding state struct with buffer and size.
 * Zeroes the 16-byte struct, then sets buf and buf_size fields.
 * 0x119c50 / data.obj
 */
void FUN_00119c50(int *state, int buf, int buf_size)
{
  if (buf == 0) {
    display_assert("buffer", "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x19,
                   1);
    system_exit(-1);
  }
  if (buf_size < 0) {
    display_assert("buffer_size>=0",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x1a, 1);
    system_exit(-1);
  }
  csmemset(state, 0, 0x10);
  *state = buf;
  state[2] = buf_size;
}

/* Write count elements of element_size bytes from src into the encoding state
 * buffer. Handles byte-swap for multi-byte elements. 0x119cc0 / data.obj
 */
int FUN_00119cc0(int *param_1, int param_2, short param_3, int param_4)
{
  int iVar1;
  int iVar2;

  if (param_1 == (int *)0 || *param_1 == 0 || param_1[1] < 0 ||
      param_1[2] <= param_1[1]) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x2b, 1);
    system_exit(-1);
  }
  switch (param_4) {
  case 1:
    iVar1 = (int)param_3;
    break;
  case -8:
    iVar1 = (int)param_3 << 3;
    break;
  case -4:
    iVar1 = (int)param_3 << 2;
    break;
  case -2:
    iVar1 = (int)param_3 << 1;
    break;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x33, 1);
    system_exit(-1);
    iVar1 = param_4;
    break;
  }
  if (param_1[1] + iVar1 <= param_1[2] && (char)param_1[3] == '\0') {
    iVar2 = *param_1 + param_1[1];
    if (param_2 == 0) {
      csmemset((void *)iVar2, 0, iVar1);
    } else {
      csmemcpy((void *)iVar2, (void *)param_2, iVar1);
    }
    if (param_4 != 1) {
      FUN_00118620((void *)iVar2, (int)param_3, param_4);
    }
    param_1[1] = param_1[1] + iVar1;
    return (char)param_1[3] == '\0';
  }
  *(char *)(param_1 + 3) = 1;
  return (char)param_1[3] == '\0';
}

/* Encode a value into the minimum byte width needed for the given maximum
 * value range and write it to the encoding state buffer.
 * maximum_value<256 -> 1 byte, <65536 -> 2 bytes, else 4 bytes.
 * Returns true if the encoding state overflow flag is still clear.
 * 0x119df0 / data.obj (data_encoding.c:0x54)
 */
bool FUN_00119df0(int *param_1, int param_2, int param_3)
{
  unsigned char byte_val;
  int val;

  if (param_3 <= 0) {
    display_assert("maximum_value>0",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x54, 1);
    system_exit(-1);
  }
  if (param_3 < 0x100) {
    if (param_1 == (int *)0 || *param_1 == 0 || param_1[1] < 0 ||
        param_1[2] <= param_1[1]) {
      display_assert("state && state->buffer && state->offset>=0 && "
                     "state->offset<state->buffer_size",
                     "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x2b, 1);
      system_exit(-1);
    }
    if (param_1[1] + 1 <= param_1[2] && *(char *)(param_1 + 3) == '\0') {
      byte_val = (unsigned char)param_2;
      csmemcpy((void *)(*param_1 + param_1[1]), &byte_val, 1);
      param_1[1] = param_1[1] + 1;
    } else {
      *(char *)(param_1 + 3) = 1;
    }
    return *(char *)(param_1 + 3) == '\0';
  }
  val = param_2;
  if (param_3 < 0x10000) {
    FUN_00119cc0(param_1, (int)&val, 1, -2);
  } else {
    FUN_00119cc0(param_1, (int)&val, 1, -4);
  }
  return *(char *)(param_1 + 3) == '\0';
}

/* Append encoded structures into a bitstream state buffer.
 * Multiplies the field size (bs_definition[+4] short) by count to get
 * the copy size. If the copy fits and no overflow, memcpy's the source
 * structures then calls FUN_00118be0 to encode/endian-swap them in place.
 * Sets overflow flag and returns false if the copy would overflow.
 * Returns true when overflow byte is still zero after the operation. */
bool FUN_00119ef0(int *param_1, int param_2, short param_3, int param_4)
{
  short sVar1;
  int iVar2;
  char *overflow;
  int dest;

  if ((param_1 == (int *)0x0) || (*param_1 == 0) || (param_1[1] < 0) ||
      (param_1[2] <= param_1[1])) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x6e, 1);
    system_exit(-1);
  }
  if (param_2 == 0) {
    display_assert("source_structures",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x6f, 1);
    system_exit(-1);
  }
  if (param_4 == 0) {
    display_assert("bs_definition", "c:\\halo\\SOURCE\\memory\\data_encoding.c",
                   0x70, 1);
    system_exit(-1);
  }
  sVar1 = *(short *)(param_4 + 4) * param_3;
  overflow = (char *)(param_1 + 3);
  if (0 < sVar1) {
    iVar2 = (int)sVar1;
    if ((iVar2 + param_1[1] <= param_1[2]) && (*overflow == '\0')) {
      dest = *param_1 + param_1[1];
      csmemcpy((void *)dest, (void *)param_2, iVar2);
      FUN_00118be0((void *)param_4, (void *)dest, (int)param_3);
      param_1[1] = param_1[1] + iVar2;
      return *overflow == '\0';
    }
    *overflow = 1;
    return *overflow == '\0';
  }
  return *overflow == '\0';
}

/*
 * FUN_00119ff0 — data_encoding.c encode_element_array (line 0x8d–0xa5)
 * Encodes a single-element array into the bit-stream state buffer.
 * param_1: encoding state (int[4]: buffer ptr, offset, buffer_size,
 * overflow_flag) param_2: element type selector (1=byte, -2=uint16, -4=int32,
 * -8=int64, else=assert) param_3: source array pointer param_4: element_count
 * (value, not pointer; used as count and value to encode) param_5:
 * bs_definition pointer
 */
bool FUN_00119ff0(int *param_1, int param_2, int param_3, int param_4,
                  int param_5)
{
  int *piVar1;
  int local_8[2];
  unsigned char byte_val;

  piVar1 = param_1;
  /* assert: state valid */
  if (param_1 == (int *)0 || *param_1 == 0 || param_1[1] < 0 ||
      param_1[2] <= param_1[1]) {
    display_assert("state && state->buffer && state->offset>=0 && "
                   "state->offset<state->buffer_size",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x8d, 1);
    system_exit(-1);
  }
  /* assert: source_array */
  if (param_3 == 0) {
    display_assert("source_array", "c:\\halo\\SOURCE\\memory\\data_encoding.c",
                   0x8e, 1);
    system_exit(-1);
  }
  /* assert: bs_definition */
  if (param_5 == 0) {
    display_assert("bs_definition", "c:\\halo\\SOURCE\\memory\\data_encoding.c",
                   0x8f, 1);
    system_exit(-1);
  }
  /* assert: element_count >= 0 */
  if (param_4 < 0) {
    display_assert("element_count>=0",
                   "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x90, 1);
    system_exit(-1);
  }

  switch (param_2) {
  case 1:
    /* assert: element_count <= UNSIGNED_CHAR_MAX */
    if (param_4 > 0xff) {
      display_assert("element_count<=UNSIGNED_CHAR_MAX",
                     "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x96, 1);
      system_exit(-1);
    }
    byte_val = (unsigned char)param_4;
    /* inner state assert (same as FUN_00119df0:0x2b) */
    if (piVar1 == (int *)0 || *piVar1 == 0 || piVar1[1] < 0 ||
        piVar1[2] <= piVar1[1]) {
      display_assert("state && state->buffer && state->offset>=0 && "
                     "state->offset<state->buffer_size",
                     "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x2b, 1);
      system_exit(-1);
    }
    if (piVar1[2] < piVar1[1] + 1 || *(char *)(piVar1 + 3) != '\0') {
      *(char *)(piVar1 + 3) = 1;
    } else {
      csmemcpy((void *)(*piVar1 + piVar1[1]), (void *)&byte_val, 1);
      piVar1[1] = piVar1[1] + 1;
    }
    break;
  case -2:
    /* assert: element_count <= UNSIGNED_SHORT_MAX */
    if (param_4 > 0xffff) {
      display_assert("element_count<=UNSIGNED_SHORT_MAX",
                     "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0x9a, 1);
      system_exit(-1);
    }
    param_1 = (int *)param_4;
    FUN_00119cc0(piVar1, (int)&param_1, 1, (int)0xfffffffe);
    break;
  case -4:
    param_1 = (int *)param_4;
    FUN_00119cc0(piVar1, (int)&param_1, 1, (int)0xfffffffc);
    break;
  case -8:
    local_8[0] = param_4;
    local_8[1] = param_4 >> 31;
    FUN_00119cc0(piVar1, (int)local_8, 1, (int)0xfffffff8);
    break;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\memory\\data_encoding.c", 0xa5, 1);
    system_exit(-1);
    break;
  }

  FUN_00119ef0(piVar1, param_3, (short)param_4, param_5);
  return *(char *)(piVar1 + 3) == '\0';
}
