/* Game sound subsystem — manages looping sounds attached to objects. */

/* Sound-file container dispatch (0x1c6ca0).
 * Tries the AIFF/AIFC container first: FUN_001c6880 sniffs the FORM/AIFF
 * header, FUN_001c6b20 parses it, and FUN_001c6bf0 consumes the parsed
 * result. If either step fails, falls back to the second container family
 * (FUN_001c6d20 sniff / FUN_001c6ed0 parse / FUN_001c6fb0 consume).
 * Returns true when one of the two chains completed.
 * The meaning of param_2..param_4 is unproven; the binary shows only that
 * param_3/param_4 are forwarded to the parse call and
 * param_2/param_3/param_4 to the consume call, and that the consume call's
 * return value is discarded (MOV AL,0x1 follows unconditionally). */
bool FUN_001c6ca0(file_ref_t *info, void *param_2, void *param_3, void *param_4)
{
  if (FUN_001c6880(info)) {
    if (FUN_001c6b20(info, param_3, param_4)) {
      FUN_001c6bf0(param_2, param_3, param_4);
      return 1;
    }
  }
  if (FUN_001c6d20(info)) {
    if (FUN_001c6ed0(info, param_3, param_4)) {
      FUN_001c6fb0(param_2, param_3, param_4);
      return 1;
    }
  }
  return 0;
}

/* Check if a file is a RIFF/WAVE audio container (0x1c6d20).
 * Opens the file, reads the first 12-byte RIFF header chunk (id + size +
 * form type), byte-swaps it via the riff_container_chunk definition at
 * 0x32ec44, then checks that the chunk id is 0x52494646 ('RIFF') and the
 * form type is 0x57415645 ('WAVE'). Closes the file before returning.
 * Mirrors the AIFF sniff FUN_001c6880. */
bool FUN_001c6d20(file_ref_t *info)
{
  int header[3];
  char result;
  char ok;

  result = 0;
  ok = file_open(info, 1);
  if (ok != '\0') {
    ok = file_read_from_position(info, 0, 0xc, header);
    if (ok != '\0') {
      FUN_00118be0((void *)0x32ec44, header, 1);
      if ((header[0] == 0x52494646) && (header[2] == 0x57415645)) {
        result = 1;
      }
    }
    file_close(info);
  }
  return result;
}

/* Parse a RIFF/WAVE container and read out its 'data' chunk (0x1c6ed0).
 * Walks the chunk list starting at file offset 0xc (just past the 12-byte
 * RIFF header): reads a 4-byte chunk id, then the 4-byte chunk size, then
 * byte-swaps the id in place via the riff_chunk_type definition at 0x32ec68.
 * On 'data' (0x64617461) it stores the chunk size through param_2 and reads
 * that many bytes into param_3; otherwise it skips the chunk payload,
 * rounding an odd size up to the next even boundary (RIFF word padding).
 * Returns true only when the data payload was read successfully; the file is
 * closed on every path that opened it.
 * The binary shows param_2 as a dword out-parameter (MOV [ECX],EAX) and
 * param_3 as a raw destination buffer; their callee-side meaning beyond that
 * is unproven. Mirrors the AIFF parse FUN_001c6b20. */
bool FUN_001c6ed0(file_ref_t *info, void *param_2, void *param_3)
{
  int chunk_id;
  unsigned int chunk_size;
  unsigned int padded_size;
  int offset;
  char result;
  char ok;

  result = 0;
  offset = 0xc;
  ok = file_open(info, 1);
  if (ok != '\0') {
    ok = file_read_from_position(info, offset, 4, &chunk_id);
    if (ok != '\0') {
      for (;;) {
        offset += 4;
        ok = file_read_from_position(info, offset, 4, &chunk_size);
        if (ok != '\0') {
          FUN_00118be0((void *)0x32ec68, &chunk_id, 1);
          if (chunk_id == 0x64617461) {
            *(unsigned int *)param_2 = chunk_size;
            offset += 4;
            ok =
              file_read_from_position(info, offset, (int)chunk_size, param_3);
            if (ok != '\0') {
              result = 1;
            }
            break;
          }
          padded_size = chunk_size;
          if ((padded_size & 1) != 0) {
            padded_size++;
          }
          offset += (int)padded_size + 4;
        }
        ok = file_read_from_position(info, offset, 4, &chunk_id);
        if (ok == '\0') {
          file_close(info);
          return result;
        }
      }
    }
    file_close(info);
  }
  return result;
}

void game_sound_initialize(void)
{
  *(void **)0x5054e4 =
    game_state_data_new("object looping sounds", 0x400, 0x34);
  *(void **)0x5054e0 = game_state_malloc("game sound globals", 0, 8);
}

void game_sound_dispose(void)
{
  if (*(void **)0x5054e4 != 0)
    *(void **)0x5054e4 = 0;
}

void game_sound_initialize_for_new_map(void)
{
  if (*(void **)0x5054e4 != 0) {
    ((void (*)(void *))0x119b20)(*(void **)0x5054e4);
    *(int *)(*(char **)0x5054e0 + 4) = -1;
    *(int *)(*(char **)0x5054e0) = 0;
  }
}

/* Drop scripted looping-sound back-references held by lsnd tags (0x1c70b0).
 *
 * Walks every live datum of the object-looping-sounds table (0x5054e4).
 * When the datum's lsnd definition points back at this datum
 * (definition+0x1c == looping_sound_index), the datum must carry the
 * scripted flag (bit 0x10 of entry+0x4) and the back-reference is reset to
 * NONE. Otherwise, when the back-reference is set to some other datum, that
 * datum is fetched and the result discarded — the binary has no compare or
 * branch after the call at 0x1c7134 (only ADD ESP,0x8), so the original
 * source's use of the result left no code in this build. */
void game_sound_clear(void)
{
  int looping_sound_index;
  void *entry;
  void *definition;
  int scripting_index;

  for (looping_sound_index = data_next_index(*(data_t **)0x5054e4, -1);
       looping_sound_index != -1;
       looping_sound_index =
         data_next_index(*(data_t **)0x5054e4, looping_sound_index)) {
    entry = datum_get(*(data_t **)0x5054e4, looping_sound_index);
    definition = tag_get(0x6c736e64, *(int *)((char *)entry + 0xc));

    scripting_index = *(int *)((char *)definition + 0x1c);
    if (scripting_index == looping_sound_index) {
      if ((*(uint8_t *)((char *)entry + 4) & 0x10) == 0) {
        display_assert(
          "TEST_FLAG(sound->flags, _game_looping_sound_scripted_bit)",
          "c:\\halo\\SOURCE\\sound\\game_sound.c", 0xaf, 1);
        system_exit(-1);
      }
      *(int *)((char *)definition + 0x1c) = -1;
    } else if (scripting_index != -1) {
      datum_get(*(data_t **)0x5054e4, scripting_index);
    }
  }
}

/* Re-establish scripted looping-sound back-references after a game-state
 * restore (0x1c7160).
 *
 * Pass 1 walks every live datum of the object-looping-sounds table
 * (0x5054e4). Only scripted data participate (bit 0x10 of entry+0x4); the
 * datum's 'lsnd' definition is resolved from entry+0xc. When bit 0x2 of the
 * definition's first flags byte is set the datum is dropped
 * (datum_delete); otherwise the definition's runtime_scripting_sound_index
 * (+0x1c) is re-pointed at this datum. This is the inverse of
 * game_sound_clear(), which tears those back-references down.
 *
 * Pass 2 iterates every loaded 'snd!' tag through the tag-group iterator
 * (20-byte state at EBP-0x14) and resets each tag's field at +0x90 to NONE.
 */
void game_sound_restore(void)
{
  char iterator[20];
  int looping_sound_index;
  void *entry;
  unsigned char *definition;
  int tag_index;

  for (looping_sound_index = data_next_index(*(data_t **)0x5054e4, -1);
       looping_sound_index != -1;
       looping_sound_index =
         data_next_index(*(data_t **)0x5054e4, looping_sound_index)) {
    entry = datum_get(*(data_t **)0x5054e4, looping_sound_index);
    if ((*(uint8_t *)((char *)entry + 4) & 0x10) != 0) {
      definition =
        (unsigned char *)tag_get(0x6c736e64, *(int *)((char *)entry + 0xc));
      if ((*definition & 2) == 0) {
        *(int *)(definition + 0x1c) = looping_sound_index;
      } else {
        datum_delete(*(data_t **)0x5054e4, looping_sound_index);
      }
    }
  }

  FUN_001b9b60((int)iterator, 0x736e6421);
  tag_index = FUN_001b9b80((int)iterator);
  while (tag_index != -1) {
    *(int *)((char *)tag_get(0x736e6421, tag_index) + 0x90) = -1;
    tag_index = FUN_001b9b80((int)iterator);
  }
}

/* Delete a looping-sound datum entry from the object-looping-sounds table
 * (0x1c7330).
 *
 * Preconditions checked via assert:
 *   - The lsnd tag's runtime_scripting_sound_index must NOT point at this
 *     entry (the caller is responsible for clearing tag+0x1c before calling
 *     here if it was set).
 *   - If runtime_scripting_sound_index != NONE, it must reference a valid
 *     live datum.
 *
 * Then unconditionally deletes the datum from DAT_005054e4.
 */
void game_looping_sound_delete(int sound_handle)
{
  void *entry;
  void *tag;
  int scripting_index;

  entry = datum_get(*(data_t **)0x5054e4, sound_handle);
  tag = tag_get(0x6c736e64, *(int *)((char *)entry + 0xc));

  scripting_index = *(int *)((char *)tag + 0x1c);
  if (scripting_index == sound_handle) {
    display_assert(
      "definition->runtime_scripting_sound_index!=looping_sound_index",
      "c:\\halo\\SOURCE\\sound\\game_sound.c", 0x118, 1);
    system_exit(-1);
  }

  scripting_index = *(int *)((char *)tag + 0x1c);
  if (scripting_index != -1) {
    if (datum_get(*(data_t **)0x5054e4, scripting_index) == 0) {
      display_assert(
        "definition->runtime_scripting_sound_index==NONE || "
        "game_looping_sound_get(definition->runtime_scripting_sound_index)",
        "c:\\halo\\SOURCE\\sound\\game_sound.c", 0x119, 1);
      system_exit(-1);
    }
  }

  datum_delete(*(data_t **)0x5054e4, sound_handle);
}

/* Play a spatialized sound impulse at a world location (0x1c73d0).
 * Copies 44 bytes of location data into a sound_params struct with
 * spatialization_mode=1 (positional), then forwards to sound_start. */
void unattached_impulse_sound_new(int sound_tag_index, void *location,
                                  float scale)
{
  char sound_params[0x40];

  if (!location) {
    display_assert("location", "c:\\halo\\SOURCE\\sound\\game_sound.c", 0x148,
                   1);
    system_exit(-1);
  }
  if (scale < 0.0f || !(scale <= 1.0f)) {
    error(2, "DIAG scale OOB in 0x1c73d0: scale=%f tag=0x%x", (double)scale,
          sound_tag_index);
    display_assert("scale>=0.f && scale<=1.f",
                   "c:\\halo\\SOURCE\\sound\\game_sound.c", 0x149, 1);
    system_exit(-1);
  }

  {
    int *dst = (int *)(sound_params + 0x0c);
    int *src = (int *)location;
    int i;
    for (i = 0; i < 11; i++)
      dst[i] = src[i];
  }
  *(float *)(sound_params + 0x04) = scale;
  *(int16_t *)(sound_params + 0x00) = 1;
  *(float *)(sound_params + 0x08) = 1.0f;

  sound_start(sound_tag_index, sound_params, NONE, 0, 0, 0);
}

/* Play a one-shot sound impulse at a given volume scale.
 * Builds a minimal sound source descriptor (spatialization_mode=0,
 * scale, gain=1.0f) and forwards to sound_start (0x1ce180) with
 * no object attachment, no track data. */
int sound_impulse_start(int sound_tag_index, float scale)
{
  /* 64-byte sound source descriptor — only first 12 bytes matter here.
   * offset 0x00: int16_t spatialization_mode
   * offset 0x04: float   scale
   * offset 0x08: float   gain */
  char source[0x40];

  assert_halt(scale >= 0.f && scale <= 1.f);

  *(int16_t *)(source + 0x00) = 0;
  *(float *)(source + 0x04) = scale;
  *(float *)(source + 0x08) = 1.0f;

  /* original returns the sound datum handle (or -1) from this tail call;
   * callers like hud_sounds_update (FUN_000d70b0) store it for
   * sound_stop_impulse. */
  return sound_start(sound_tag_index, source, NONE, 0, 0, 0);
}

/* Remaining game-time ticks for a scripted sound, clamped at zero.
 * `handle` is a 'snd!' tag index (NONE => 0).  Field +0x90 of the sound
 * tag holds an absolute game time (NONE when nothing is scheduled); its
 * meaning beyond "compared against game_time_get()" is unproven. */
int scripted_sound_time(int handle)
{
  char *sound_tag;
  int remaining;
  remaining = 0;
  remaining = handle;
  if (handle != NONE) {
    sound_tag = (char *)tag_get(0x736e6421, remaining);
    if ((*((int *)(sound_tag + 0x90))) != NONE) {
      remaining = (*((int *)(sound_tag + 0x90))) - game_time_get();
      return (remaining > 0) ? (remaining) : (0);
    }
  }
  return remaining;
}

/* Stop the impulse sound a scripted sound is currently playing.
 * `handle` is a 'snd!' tag index (NONE => nothing to do).  Field +0x94 of
 * the sound tag holds the playing impulse's sound datum handle (NONE when
 * idle); +0x90 is the scheduled game time read by scripted_sound_time.
 * Both are reset to NONE after the stop. */
void scripted_sound_stop(int handle)
{
  char *sound_tag;
  int sound_index;

  if (handle != NONE) {
    sound_tag = (char *)tag_get(0x736e6421, handle);
    sound_index = *((int *)(sound_tag + 0x94));
    if (sound_index != NONE) {
      sound_stop_impulse(sound_index);
      *((int *)(sound_tag + 0x94)) = NONE;
      *((int *)(sound_tag + 0x90)) = NONE;
    }
  }
}

/* Pre-load ('predict') every sound referenced by a looping-sound (lsnd) tag so
 * the scripted foley plays without a cache stall (0x1c75a0).
 *
 * Walks the lsnd playlist block at lsnd+0x3c (0xa0-byte elements, same block
 * and element size as game_sound_music_has_vehicle_sound).  For each entry
 * whose snd! reference at +0x4c is valid, the snd! tag's block at +0x98 is
 * examined; only a single-element block (count == 1) is considered.  Element
 * size 0x48 there and 0x7c for the nested block at +0x3c identify these as the
 * snd! pitch-ranges and per-pitch-range permutations blocks.  The first
 * permutation is handed to sound_cache_request_sound(ptr, false, true, false)
 * — a non-blocking load request whose result the original discards
 * (0x1c762d: no test of EAX after the CALL). */
void scripted_foley_predict(int handle)
{
  void *lsnd_tag;
  int *playlist; /* pointer to the block at lsnd+0x3c (count, ptr) */
  int16_t i;
  void *element;
  int sound_handle;
  void *snd_tag;
  int *pitch_ranges;
  void *pitch_range;
  int *permutations;
  void *permutation;

  if (handle == NONE) {
    return;
  }

  lsnd_tag = tag_get(0x6c736e64, handle);
  playlist = (int *)((char *)lsnd_tag + 0x3c);

  i = 0;
  while ((int)i < *playlist) {
    element = tag_block_get_element(playlist, (int)i, 0xa0);
    sound_handle = *(int *)((char *)element + 0x4c);
    if (sound_handle != NONE) {
      snd_tag = tag_get(0x736e6421, sound_handle);
      pitch_ranges = (int *)((char *)snd_tag + 0x98);
      if (*pitch_ranges == 1) {
        pitch_range = tag_block_get_element(pitch_ranges, 0, 0x48);
        permutations = (int *)((char *)pitch_range + 0x3c);
        if (*permutations != 0) {
          permutation = tag_block_get_element(permutations, 0, 0x7c);
          sound_cache_request_sound(permutation, false, true, false);
        }
      }
    }
    i = i + 1;
  }
}

/* Set or clear the 'alternate' flag (bit 0x8) of the scripted looping sound
 * owned by an lsnd tag (0x1c76c0).
 *
 * handle is an lsnd tag index; the tag's runtime scripting-sound index at
 * lsnd+0x1c indexes the object-looping-sound table (0x5054e4) — the same field
 * and table game_looping_sound_delete uses.  Both the tag index and the stored
 * sound index are checked against NONE before use; there is no datum_get NULL
 * check in the original.  Flag word is entry+0x4, the same word carrying
 * _game_looping_sound_scripted_bit (0x10). */
void scripted_looping_sound_set_alternate(int handle, bool alternate)
{
  void *lsnd_tag;
  void *entry;
  int looping_sound_index;

  if (handle == NONE) {
    return;
  }

  lsnd_tag = tag_get(0x6c736e64, handle);
  looping_sound_index = *(int *)((char *)lsnd_tag + 0x1c);
  if (looping_sound_index == NONE) {
    return;
  }

  entry = datum_get(*(data_t **)0x5054e4, looping_sound_index);
  if (alternate) {
    *(uint32_t *)((char *)entry + 4) |= 0x8;
  } else {
    *(uint32_t *)((char *)entry + 4) &= ~0x8u;
  }
}

/* Start a looping sound that is not attached to an object (0x1c7710).
 * tag_get() is called for its validation side effect only (the result is
 * discarded in the original). game_looping_sound_new() is given object
 * index NONE-or-caller-supplied, an empty marker name, and scale_index NONE.
 * On success the datum's flag bit 0 is set and param_3 is stored raw at +0x8.
 * param_3 is forwarded as an untyped dword: callers pass float scale bits
 * (sound_looping_start @0x1c8510 casts to float; hud_messaging passes the
 * raw int of a float tag field), so the store must not convert. */
int unattached_looping_sound_start(int sound_tag, int param_2, int param_3)
{
  int looping_sound_index;
  void *entry;

  tag_get(0x6c736e64, sound_tag);

  /* (void *)0x25386f is an unrecovered .rdata string constant (marker name). */
  looping_sound_index =
    game_looping_sound_new(param_2, sound_tag, (void *)0x25386f, -1);

  if (looping_sound_index != NONE) {
    entry = datum_get(*(data_t **)0x5054e4, looping_sound_index);
    *(unsigned int *)((char *)entry + 0x4) =
      *(unsigned int *)((char *)entry + 0x4) | 1;
    *(int *)((char *)entry + 0x8) = param_3;
  }

  return looping_sound_index;
}

/* Stop an unattached looping sound (0x1c7770). Sets flag bit 1 on the
 * looping-sound datum, which unattached_looping_sound_start (0x1c7710)
 * marks with bit 0. The original does NOT null-check the datum_get()
 * result - it dereferences unconditionally; preserved here. */
void unattached_looping_sound_stop(int sound_index)
{
  void *entry;

  entry = datum_get(*(data_t **)0x5054e4, sound_index);
  *(unsigned int *)((char *)entry + 0x4) =
    *(unsigned int *)((char *)entry + 0x4) | 2;
}

bool FUN_001c7a10(int object_handle, void *attachment_data, void *source)
{
  void *object;
  int location[2];
  float *node_matrix;
  int marker_index;

  object = object_try_and_get_and_verify_type(object_handle, -1);

  assert_halt(attachment_data);
  assert_halt(source);

  if (object != 0) {
    object_get_location(object_handle, location);

    if ((int16_t)location[1] != -1) {
      if (*(int16_t *)((char *)attachment_data + 2) == -1) {
        marker_index = 0;
      } else {
        marker_index = (int)*(int16_t *)((char *)attachment_data + 2);
      }

      node_matrix =
        (float *)object_get_node_matrix(object_handle, marker_index);

      *(int *)((char *)source + 0x30) = location[0];
      *(int *)((char *)source + 0x34) = location[1];

      matrix_transform_point(node_matrix,
                             (float *)((char *)attachment_data + 4),
                             (float *)((char *)source + 0xc));
      matrix_transform_vector(node_matrix,
                              (float *)((char *)attachment_data + 0x10),
                              (float *)((char *)source + 0x18));
      object_get_root_location(object_handle, (float *)((char *)source + 0x24),
                               0);

      return true;
    }
  }

  return false;
}

bool sound_cluster_is_audible(void *location)
{
  int16_t cluster_index;

  cluster_index = *(int16_t *)((char *)location + 4);
  if (cluster_index >= -1) {
    if ((int)cluster_index < *(int *)((char *)scenario_get() + 0x134)) {
      if (cluster_index != -1 &&
          ((((uint32_t *)0x5054a0)[(int)cluster_index >> 5] &
            (1u << ((uint8_t)cluster_index & 0x1f))) != 0)) {
        return true;
      }
      return false;
    }
  }

  display_assert(
    "location->cluster_index>=NONE && "
    "location->cluster_index<global_structure_bsp_get()->clusters.count",
    "c:\\halo\\SOURCE\\sound\\game_sound.c", 0x364, 1);
  system_exit(-1);
}

/* Detach a scripted looping sound from its lsnd tag (0x1c7ca0).
 *
 * The lsnd tag index arrives in EAX (0x1c7cab: PUSH EAX; PUSH 'lsnd'); the
 * second parameter is a byte read from [EBP+0x8].  The tag's runtime
 * scripting-sound index at lsnd+0x1c indexes the object-looping-sound table
 * (0x5054e4) — the same field and table game_looping_sound_delete and
 * scripted_looping_sound_set_alternate use.
 *
 * Flag word is entry+0x4: bit 0x10 (_game_looping_sound_scripted_bit) is
 * cleared, bit 0x2 is set, and bit 0x4 is set only when param_2 is non-zero.
 * The original resolves the datum TWICE with two separate datum_get calls,
 * re-loading lsnd+0x1c for the second one (0x1c7cd4), so both calls are
 * preserved here.  The lsnd+0x1c store of NONE happens between the second
 * datum_get and the conditional bit-0x4 store (0x1c7cf0 precedes 0x1c7cf9).
 * There is no datum_get NULL check in the original. */
void FUN_001c7ca0(int in_EAX, bool param_2)
{
  void *lsnd_tag;
  void *entry;
  void *entry_again;
  int looping_sound_index;

  if (in_EAX == NONE) {
    return;
  }

  lsnd_tag = tag_get(0x6c736e64, in_EAX);
  looping_sound_index = *(int *)((char *)lsnd_tag + 0x1c);
  if (looping_sound_index == NONE) {
    return;
  }

  entry = datum_get(*(data_t **)0x5054e4, looping_sound_index);
  *(uint32_t *)((char *)entry + 4) = *(uint32_t *)((char *)entry + 4) & ~0x10u;

  entry_again =
    datum_get(*(data_t **)0x5054e4, *(int *)((char *)lsnd_tag + 0x1c));
  *(uint32_t *)((char *)entry_again + 4) =
    *(uint32_t *)((char *)entry_again + 4) | 2;

  *(int *)((char *)lsnd_tag + 0x1c) = NONE;

  if (param_2) {
    *(uint32_t *)((char *)entry + 4) = *(uint32_t *)((char *)entry + 4) | 4;
  }
}

/* Check whether the lsnd tag (index passed in EAX) has a playlist
 * containing any snd! entry whose flags field (int16 at offset 4)
 * equals 0x20.  The 0x20 flag identifies sounds that should play for
 * vehicle-related music contexts.  Called by
 * game_sound_music_stop_for_vehicle (0x1c7d70) with the current
 * looping-sound entry's lsnd tag index (entry+0xc) in EAX to decide
 * whether to suppress normal music playback.
 *
 * Returns true if any playlist entry references a 'snd!' tag with
 * flags == 0x20; false otherwise.
 */
bool game_sound_music_has_vehicle_sound(int in_EAX)
{
  void *lsnd_tag;
  int *playlist; /* pointer to the block at lsnd+0x3c (count, ptr) */
  int16_t i;
  void *element;
  int sound_handle;
  void *snd_tag;

  /* The lsnd tag index arrives in EAX (0x1c7d13: PUSH EAX; PUSH 'lsnd').
   * The caller (0x1c7d70) loads it from the looping-sound entry+0xc. */
  lsnd_tag = tag_get(0x6c736e64, in_EAX);
  playlist = (int *)((char *)lsnd_tag + 0x3c);

  i = 0;
  while ((int)i < *playlist) {
    element = tag_block_get_element(playlist, (int)i, 0xa0);
    sound_handle = *(int *)((char *)element + 0x4c);
    if (sound_handle != -1) {
      snd_tag = tag_get(0x736e6421, sound_handle);
      if (*(int16_t *)((char *)snd_tag + 0x4) == 0x20) {
        return true;
      }
    }
    i = i + 1;
  }
  return false;
}

/* Suppress non-vehicle music when a vehicle sound should take over
 * (0x1c7d70).
 *
 * Iterates all active looping-sound entries.  For each entry whose music
 * override slot ([+0x10]) is NONE (-1), and whose lsnd tag reference ([+0xc])
 * is valid, and where game_sound_music_has_vehicle_sound() is true, the
 * function fetches the lsnd tag and, if it currently has a playing sound
 * ([tag+0x1c] != -1):
 *   - Clears bit 0x10 (non-vehicle music flag) from the playing entry flags.
 *   - Sets  bit 0x02 (stop/fade flag) on the playing entry flags.
 *   - Clears the tag's runtime handle ([tag+0x1c] = NONE).
 *   - Sets  bit 0x04 on the playing entry flags.
 *
 * Called from sound_looping_start (0x1c8510) when the lsnd definition has
 * bit 0x04 set in its flags, before starting the new looping sound.
 */
void game_sound_music_stop_for_vehicle(void)
{
  int handle;
  char *entry;
  void *lsnd_tag;
  int playing_handle;
  char *playing_entry;

  for (handle = data_next_index(*(data_t **)0x5054e4, -1); handle != -1;
       handle = data_next_index(*(data_t **)0x5054e4, handle)) {
    entry = (char *)datum_get(*(data_t **)0x5054e4, handle);
    if (*(int *)(entry + 0x10) != -1)
      continue;
    if (!game_sound_music_has_vehicle_sound(*(int *)(entry + 0xc)))
      continue;
    if (*(int *)(entry + 0xc) == -1)
      continue;
    lsnd_tag = tag_get(0x6c736e64, *(int *)(entry + 0xc));
    playing_handle = *(int *)((char *)lsnd_tag + 0x1c);
    if (playing_handle == -1)
      continue;
    playing_entry = (char *)datum_get(*(data_t **)0x5054e4, playing_handle);
    *(uint32_t *)(playing_entry + 0x4) &= ~0x10u;
    /* Re-fetch using the same handle (result is the same pointer). */
    *(uint32_t *)((char *)datum_get(*(data_t **)0x5054e4, playing_handle) +
                  0x4) |= 0x2;
    *(int *)((char *)lsnd_tag + 0x1c) = -1;
    *(uint32_t *)(playing_entry + 0x4) |= 0x4;
  }
}

void game_sound_dispose_from_old_map(void)
{
  if (*(void **)0x5054e4 != 0 && *(uint8_t *)(*(char **)0x5054e4 + 0x24) != 0) {
    ((void (*)(void))0x1c70b0)();
    ((void (*)(void *))0x119550)(*(void **)0x5054e4);
  }
}

/* Start a sound at a position with a directional forward vector (0x1c7e70).
 * Builds a callback_data struct with marker/position/forward and a sound_params
 * struct, calls FUN_001c7a10 to resolve attachment, then sound_start. */
int object_impulse_sound_new(int object_handle, int tag_index, int16_t marker,
                             float *position, float *forward, float scale)
{
  char sound_params[0x40];
  char callback_data[0x1c];

  if (!position || !forward) {
    display_assert("position && forward",
                   "c:\\halo\\SOURCE\\sound\\game_sound.c", 0x12c, 1);
    system_exit(-1);
  }
  if (scale < 0.0f || scale > 1.0f) {
    error(2, "DIAG scale OOB in 0x1c7e70: scale=%f obj=0x%x tag=0x%x marker=%d",
          (double)scale, object_handle, tag_index, (int)marker);
    error(2, "DIAG pos=(%f,%f,%f) fwd=(%f,%f,%f)", (double)position[0],
          (double)position[1], (double)position[2], (double)forward[0],
          (double)forward[1], (double)forward[2]);
    display_assert("scale>=0.f && scale<=1.f",
                   "c:\\halo\\SOURCE\\sound\\game_sound.c", 0x12d, 1);
    system_exit(-1);
  }

  *(float *)(callback_data + 4) = position[0];
  *(float *)(callback_data + 8) = position[1];
  *(float *)(callback_data + 12) = position[2];
  *(float *)(callback_data + 16) = forward[0];
  *(float *)(callback_data + 20) = forward[1];
  *(float *)(callback_data + 24) = forward[2];

  *(int16_t *)(sound_params + 0) = 1;
  *(float *)(sound_params + 8) = 1.0f;
  *(int16_t *)(callback_data + 2) = marker;
  *(int16_t *)(sound_params + 0x34) = -1;

  if (!FUN_001c7a10(object_handle, callback_data, sound_params))
    return -1;

  *(float *)(sound_params + 4) = scale;
  return sound_start(tag_index, sound_params, object_handle, (int)&FUN_001c7a10,
                     callback_data, 0x1c);
}

/* sound_looping_stop (0x1c80e0)
 *
 * If sound_tag_index is valid, resolves the
 * `lsnd` tag and checks the
 * runtime looping-sound handle at tag+0x1c. When
 * present, clears bit 0x10
 * and sets bit 0x02 in the looping-sound entry
 * flags, then clears tag+0x1c
 * back to NONE (-1). */
void sound_looping_stop(int sound_tag_index)
{
  void *tag;
  int looping_sounds_handle;
  void *entry;

  if (sound_tag_index == -1)
    return;

  tag = tag_get(0x6c736e64, sound_tag_index);
  looping_sounds_handle = *(int *)((char *)tag + 0x1c);

  if (looping_sounds_handle != -1) {
    entry = datum_get(*(data_t **)0x5054e4, looping_sounds_handle);
    *(uint32_t *)((char *)entry + 4) &= 0xffffffef;

    entry = datum_get(*(data_t **)0x5054e4, *(int *)((char *)tag + 0x1c));
    *(uint32_t *)((char *)entry + 4) |= 2;

    *(int *)((char *)tag + 0x1c) = -1;
  }
}

/* Update the game sound subsystem for one tick.
 *
 * - Determines the current
 * sound environment (BSP cluster) via
 *   scenario_get_sound_environment, updates DirectSound
 * EAX/environment state via sound_manager_set_sound_environment, then
 * recalculates per-cluster audibility via FUN_001c7b40.
 * - Manages the music looping sound slot (globals[1]): starts, stops,
 *   or replaces it when the ambient sound environment changes.
 * - Iterates every active entry in the object-looping-sounds table and
 *   for each one either:
 *     (a) removes the entry if its object handle is invalid (NONE),
 *     (b) removes the entry and clears the tag's runtime index if the
 *         backing object no longer exists in the world,
 *     (c) calls FUN_001c77a0 to update the looping sound playback state
 *         when the sound is located in a visible cluster, or
 *     (d) advances without action otherwise.
 * - Increments the global tick counter (globals[0]) at exit.
 */
void game_sound_update(float dt)
{
  /* Out-params from scenario_get_sound_environment: sound_env_tag_index,
   * sound_env_data ptr, and a changed flag (bool). 0x18f600 writes a pointer
   * value INTO sound_env_data; cb9b0 then receives that pointer BY VALUE. */
  int sound_env_tag_index; /* [EBP-0x8]  tag index, or -1 */
  void *sound_env_data; /* [EBP-0xc]  pointer to EAX data block, written
                           by 0x18f600 */
  uint8_t env_changed; /* [EBP-0x1]  non-zero if env changed */

  /* 8-byte location struct returned by object_get_location (cluster_index etc.)
   */
  int location[2]; /* [EBP-0x14] */

  int looping_sounds_handle;
  void *entry;
  int object_handle;
  void *object;
  int object_looping_sounds; /* data_t* — *(int*)0x5054e4 */
  int music_handle; /* current music looping-sound slot handle */

  /* Determine the current sound environment (BSP cluster the listener is
   * in) and whether it changed since last tick. */
  scenario_get_sound_environment(&sound_env_tag_index, &sound_env_data,
                                 &env_changed);

  /* Copy the new environment block into the DirectSound globals area.
   * Original: `MOV EAX, [EBP-0xc]; PUSH EAX` — pass the POINTER VALUE
   * written by 0x18f600, not its address. */
  ((void (*)(void *))0x1cb9b0)(sound_env_data);

  /* Recompute per-BSP-cluster audibility bitmask for all players. */
  ((void (*)(void))0x1c7b40)();

  /* --- Music looping sound management ----------------------------------- */
  /* DAT_005054e0 points to the 8-byte game_sound_globals block:
   *   [0] = tick counter  (int)
   *   [1] = music looping sound handle  (int, -1 = none) */

  if (sound_env_tag_index == -1) {
    /* No sound environment: kill the music looping sound if one is active. */
    music_handle = *(int *)(*(int *)0x5054e0 + 4);
    if (music_handle != -1) {
      entry = datum_get(*(data_t **)0x5054e4, music_handle);
      *(uint32_t *)((char *)entry + 4) |= 2; /* set stop flag */
      *(int *)(*(int *)0x5054e0 + 4) = -1;
    }
  } else {
    music_handle = *(int *)(*(int *)0x5054e0 + 4);
    if (music_handle == -1) {
      /* Start a new music looping sound for this environment. */
      music_handle =
        ((int (*)(int, int, int))0x1c7710)(sound_env_tag_index, -1, 0x3f800000);
      *(int *)(*(int *)0x5054e0 + 4) = music_handle;
    } else {
      /* Check whether the environment changed. */
      entry = datum_get(*(data_t **)0x5054e4, music_handle);
      if (*(int *)((char *)entry + 0xc) != sound_env_tag_index) {
        /* Environment changed: stop old music and start new. */
        entry = datum_get(*(data_t **)0x5054e4, music_handle);
        *(uint32_t *)((char *)entry + 4) |= 2; /* set stop flag */
        music_handle = ((int (*)(int, int, int))0x1c7710)(sound_env_tag_index,
                                                          -1, 0x3f800000);
        *(int *)(*(int *)0x5054e0 + 4) = music_handle;
      }
    }
  }

  /* --- Per-entry update loop -------------------------------------------- */
  object_looping_sounds = *(int *)0x5054e4;

  looping_sounds_handle = data_next_index((data_t *)object_looping_sounds, -1);

  while (looping_sounds_handle != -1) {
    entry = datum_get((data_t *)object_looping_sounds, looping_sounds_handle);

    object_handle = *(int *)((char *)entry + 0x10);
    if (object_handle == -1) {
      /* No object attached — remove the entry outright. */
      ((void (*)(int, void *))0x1c77a0)(looping_sounds_handle, (void *)0);
    } else if ((*(uint8_t *)((char *)entry + 4) & 1) == 0 ||
               object_try_and_get_and_verify_type(
                 *(int *)((char *)entry + 0x10), -1) != NULL) {
      /* Object exists or scripted: check if it lives in an audible cluster.
       * object_get_and_verify_type = object_get_and_verify_type(handle,
       * type_mask=-1) */
      object = object_get_and_verify_type(*(int *)((char *)entry + 0x10), -1);
      if ((*(uint32_t *)((char *)object + 4) & 0x800) != 0) {
        /* Object is in a visible cluster — get its location and maybe
         * update the looping sound. */
        int is_audible;
        object_get_location(*(int *)((char *)entry + 0x10), (void *)location);
        is_audible = sound_cluster_is_audible((void *)location);
        if ((uint8_t)is_audible != 0) {
          ((void (*)(int, int *))0x1c77a0)(looping_sounds_handle, location);
        }
      }
    } else {
      /* Object no longer exists in the world: clear the tag's runtime
       * scripting sound index if it pointed at this slot, then remove. */
      void *tag = tag_get(0x6c736e64, *(int *)((char *)entry + 0xc));
      if (*(int *)((char *)tag + 0x1c) == looping_sounds_handle) {
        *(int *)((char *)tag + 0x1c) = -1;
      }
      game_looping_sound_delete(looping_sounds_handle);
    }

    looping_sounds_handle =
      data_next_index((data_t *)object_looping_sounds, looping_sounds_handle);
  }

  /* Increment global tick counter. */
  *(int *)(*(int *)0x5054e0) += 1;
}

/* sound_compute_source_obstruction (0x1c8310)
 *
 * Computes sound obstruction/occlusion factors for an absolute-spatialized
 * sound source relative to a given player's observer camera.
 *
 * 1. Gets the observer camera for `channel_index` (a local player index).
 * 2. Pushes a collision user onto the global collision stack (user type 0x10).
 * 3. Asserts that `source->spatialization_mode ==
 * _sound_spatialization_mode_absolute`.
 * 4. Initialises source+0x38 = 0.6f (default gain), source+0x3C = 1.0f.
 * 5. If both source and camera have valid cluster indices:
 *    a. Queries the BSP cluster sound encoding between the two clusters.
 *    b. Converts the 7-bit encoding to a float distance (0..~256).
 *    c. If distance < 256.0 (not fully occluded):
 *       - Looks up cluster audibility data for the camera cluster.
 *       - If the source cluster is audible from the camera cluster,
 *         sets source+0x38 = 0.45f and raycasts from camera to source.
 *       - If the raycast finds no LOS, zeroes both fields.
 *       - If source+0x38 is non-zero, computes an obstruction factor
 *         from the cluster distance and sqrt_dist, clamped to [0, 1],
 *         and stores it in source+0x3C.
 * 6. Pops the collision user.
 */
void sound_compute_source_obstruction(int channel_index, void *source,
                                      float sqrt_dist)
{
  float *camera;
  int16_t source_cluster;
  float direction[3];
  int16_t collision_result[40]; /* 80 bytes */
  uint8_t sound_encoding;
  uint32_t encoding_bits;
  float cluster_distance;
  void *bsp;
  uint32_t *audibility;
  int source_cluster_int;
  float obstruction;
  float clamped;

  camera = (float *)observer_get_camera(channel_index);

  /* Push collision user (type 0x10). */
  if (*(int16_t *)0x4761d8 >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\sound\\game_sound.c", 0x370, 1);
    system_exit(-1);
  }
  {
    int depth = (int)*(int16_t *)0x4761d8;
    *(int16_t *)0x4761d8 += 1;
    *(int16_t *)(0x5a8c80 + depth * 2) = 0x10;
  }

  /* Default gain and obstruction factor.
   * These stores are placed before the assert in the original binary
   * (MSVC instruction scheduling interleaves them with the CMP/JZ). */
  *(float *)((char *)source + 0x38) = 0.6f;
  *(float *)((char *)source + 0x3c) = 1.0f;

  assert_halt_msg(
    *(int16_t *)source == 1,
    "source->spatialization_mode==_sound_spatialization_mode_absolute");

  source_cluster = *(int16_t *)((char *)source + 0x34);
  if (source_cluster != -1 && *(int16_t *)((char *)camera + 0x10) != -1) {
    /* Query cluster sound path encoding between camera and source clusters. */
    bsp = scenario_get();
    sound_encoding = structure_bsp_cluster_sound_encoding(
      bsp, *(int16_t *)((char *)camera + 0x10), source_cluster);
    encoding_bits = (uint32_t)(sound_encoding & 0x7f);
    cluster_distance = (float)(int)encoding_bits * *(float *)0x256148;

    if (cluster_distance < *(float *)0x2642a0) {
      source_cluster_int = (int)*(int16_t *)((char *)source + 0x34);

      /* Get cluster audibility bitfield for the camera's cluster. */
      bsp = scenario_get();
      audibility = structure_bsp_get_cluster_sound_data(
        bsp, *(int16_t *)((char *)camera + 0x10));

      /* Check if the source cluster is audible from the camera cluster. */
      if ((audibility[source_cluster_int >> 5] &
           (1u << ((uint8_t)source_cluster_int & 0x1f))) != 0) {
        *(float *)((char *)source + 0x38) = 0.45f;

        /* Raycast from camera to source to check line of sight. */
        direction[0] = *(float *)((char *)source + 0x0c) - camera[0];
        direction[1] = *(float *)((char *)source + 0x10) - camera[1];
        direction[2] = *(float *)((char *)source + 0x14) - camera[2];

        if (!FUN_0014df70(0xc0e1, camera, direction, -1, collision_result)) {
          /* No line of sight — fully occluded. */
          *(float *)((char *)source + 0x38) = 0.0f;
          *(float *)((char *)source + 0x3c) = 0.0f;
        }
      }

      /* Compute obstruction factor if gain is non-zero. */
      if (*(float *)((char *)source + 0x38) != *(float *)0x2533c0) {
        obstruction =
          *(float *)0x2533c8 - sqrt_dist / (cluster_distance + sqrt_dist);
        *(float *)((char *)source + 0x3c) = obstruction;
        clamped = obstruction * *(float *)0x256870;
        if (clamped < *(float *)0x2533c0)
          clamped = *(float *)0x2533c0;
        else if (clamped > *(float *)0x2533c8)
          clamped = *(float *)0x2533c8;
        *(float *)((char *)source + 0x3c) = clamped;
      }
    }
  }

  /* Pop collision user. */
  if (*(int16_t *)0x4761d8 <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\sound\\game_sound.c", 0x39c, 1);
    system_exit(-1);
  }
  *(int16_t *)0x4761d8 -= 1;
}

/* sound_looping_start (0x1c8510)
 *
 * Starts a looping-sound definition
 * (`lsnd`) for an optional object.
 * - Resolves the tag definition and first
 * calls sound_looping_stop to end
 *   any prior runtime instance for this
 * definition.
 * - Asserts that definition+0x1c (runtime_scripting_sound_index)
 * is NONE.
 * - If definition flags has bit 0x04 set, calls
 *   game_sound_music_stop_for_vehicle (0x1c7d70) before start.
 * - Calls 0x1c7710(sound_tag_index, object_index, scale), stores
 * returned
 *   looping-sound handle into definition+0x1c, and when valid sets
 * bit 0x10
 *   in the looping-sound entry flags at entry+0x04.
 */
void sound_looping_start(int sound_tag_index, int object_index, float scale)
{
  void *definition;
  int looping_sound_handle;
  void *entry;

  if (sound_tag_index == -1)
    return;

  definition = tag_get(0x6c736e64, sound_tag_index);
  sound_looping_stop(sound_tag_index);

  assert_halt_msg(*(int *)((char *)definition + 0x1c) == -1,
                  "definition->runtime_scripting_sound_index==NONE");

  if ((*(uint8_t *)definition & 4) != 0)
    game_sound_music_stop_for_vehicle();

  looping_sound_handle =
    ((int (*)(int, int, float))0x1c7710)(sound_tag_index, object_index, scale);
  *(int *)((char *)definition + 0x1c) = looping_sound_handle;

  if (looping_sound_handle != -1) {
    entry = datum_get(*(data_t **)0x5054e4, looping_sound_handle);
    *(uint32_t *)((char *)entry + 4) |= 0x10;
  }
}

/* game_sound_set_music_volume (0x1c8c80)
 *
 * For each of the 0x33 sound
 * classes whose name string contains
 * sound_name as a substring, calls
 * sound_class_get(i) (0x1c89d0, register-arg SI) and writes the clamped volume
 * and transition_ticks into the returned record: float  at [record+0x00] =
 * clamp(volume, 0.0f, 1.0f) int16_t at [record+0x08] = max(transition_ticks, 0)
 *
 * sound_class_get (0x1c89d0) expects its index in SI and returns a
 * pointer to the 0xc-byte class record in EAX.  Called via inline asm
 * because knowledge.py does not support @si register-arg functions.
 *
 * The class name string pointers live in a table at 0x32f5d0
 * (0x33 pointers, one per sound class).
 */
void game_sound_set_music_volume(const char *sound_name, float volume,
                                 int16_t transition_ticks)
{
  int i;
  const char **table = (const char **)0x32f5d0;
  float clamped;
  float *record;

  for (i = 0; i < 0x33; i++) {
    /* Skip empty class name slots. */
    if (table[i][0] == '\0')
      continue;
    /* Check whether this class name contains sound_name. */
    if (crt_strstr(table[i], sound_name) == NULL)
      continue;

    record = (float *)sound_class_get((int16_t)i);

    /* Clamp volume to [0.0f, 1.0f]. */
    clamped = volume;
    if (clamped < *(float *)0x2533c0)
      clamped = *(float *)0x2533c0;
    else if (clamped > *(float *)0x2533c8)
      clamped = *(float *)0x2533c8;

    *record = clamped;

    /* transition_ticks clamped to >= 0. */
    *(int16_t *)((char *)record + 0x8) =
      (transition_ticks < 0) ? 0 : transition_ticks;
  }
}
