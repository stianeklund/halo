/* sound_pitch_push_sample (0x1c7b00)
 *
 * Conditionally applies a pitch sample to an object's pitch-track field.
 * Only runs if the object looping sounds table (0x5054e4) is initialized
 * (byte +0x24 != 0).  Validates the object handle as type 3 (biped|vehicle)
 * via object_try_and_get_and_verify_type before forwarding to
 * sound_object_apply_pitch_delta (0x1ac2f0). */
void sound_pitch_push_sample(int object_handle, float pitch)
{
  if (*(uint8_t *)(*(int *)0x5054e4 + 0x24) != 0) {
    if (object_try_and_get_and_verify_type(object_handle, 3) != 0) {
      sound_object_apply_pitch_delta(object_handle, pitch);
    }
  }
}

/* Return a pointer to the sound class definition at class_index.
 * The definitions live in a static table at 0x32ed08 with a stride of 0x2c.
 * Originally inlined from sound_classes.h; compiled into sound_manager.obj.
 * Asserts that class_index is in range [0,0x33), the class name is non-empty,
 * and the per-definition and per-object instance limits are <= 0x10. */
void *sound_class_get_definition(short class_index)
{
  int idx = (int)class_index;
  void *definition = (void *)(0x32ed08 + idx * 0x2c);

  assert_halt(class_index >= 0 && class_index < 0x33);
  assert_halt(((const char **)0x32f5d0)[idx][0]);
  assert_halt(*(short *)definition <= 0x10);
  assert_halt(*(short *)((char *)definition + 2) <= 0x10);

  return definition;
}

/* Return the minimum-distance attenuation for a sound tag.
 * Reads the per-tag min_distance override at tag offset 0x8.
 * If it is zero, falls back to the min_distance field (offset 0x18)
 * of the sound class definition looked up via the tag's class_index
 * at offset 0x4. */
float sound_class_get_min_distance(int sound_tag_index)
{
  void *sound_tag = tag_get(0x736e6421, sound_tag_index);
  float min_distance = *(float *)((char *)sound_tag + 0x8);

  if (min_distance == 0.0f) {
    short class_index = *(short *)((char *)sound_tag + 0x4);
    void *class_def = sound_class_get_definition(class_index);
    min_distance = *(float *)((char *)class_def + 0x18);
  }

  return min_distance;
}

/* Sample a pitch value from a permutation's mouth data (0x1c8f20).
 *
 * Clamps permutation_index into [0, mouth_data.size-1], reads the
 * corresponding byte from the mouth data block via FUN_001c8d90,
 * and normalizes it to [0.0, 1.0] by dividing by 255.
 * If the permutation has no mouth data (size == 0), logs an error
 * and returns 0.0f. */
float sound_get_permutation_pitch(int permutation_block_ptr,
                                  short permutation_index)
{
  int mouth_data_size = *(int *)((char *)permutation_block_ptr + 0x54);
  int clamped_index;

  if (mouth_data_size != 0) {
    if (permutation_index < 0) {
      clamped_index = 0;
    } else {
      clamped_index = mouth_data_size - 1;
      if ((int)permutation_index <= clamped_index) {
        clamped_index = (int)permutation_index;
      }
    }

    {
      uint8_t *byte_ptr =
        (uint8_t *)FUN_001c8d90(permutation_block_ptr, clamped_index);
      int byte_value = (int)*byte_ptr;
      return (float)byte_value * (1.0f / 255.0f);
    }
  }

  error(2, "but how can you speak if you have no mouth data? (permutation %s)",
        permutation_block_ptr);
  return 0.0f;
}

/* Empty on Xbox — no per-map sound initialization needed. */
void sound_initialize_for_new_map(void)
{
}

/* Fade out all active sounds and then stop the sound manager for map unload.
 *
 * If the sound manager is initialized and hardware is present:
 *   1. Trigger a linear fade-out on every active sound in the sounds table
 *      (0.3s fade, mode=linear, fade_out only — fade_in_index=NONE).
 *   2. Spin-loop calling sound_update() until 300 ms have elapsed since the
 *      fade was started (blocking wait for the fade to complete).
 *   3. After the wait, if the fade flag (0x4eaf42) is set, clear it, call
 *      vtable+0x28(0) to stop hardware output, and record the timestamp.
 * Finally, unconditionally stop all channels (FUN_001cd540) and re-validate
 * the looping-sounds table so it is ready for the next map. */
/* sound_set_music_enabled (0x1cb8a0)
 *
 * Sets the sound system's music/fade flag at 0x4eaf42. If the new state
 * differs from the current one, updates the flag, calls the sound driver's
 * vtable+0x28 method with the enabled parameter, and -- when disabling
 * (enabled=0) -- records the current timestamp so the fade-out timing in
 * sound_dispose_from_old_map has a reference point.
 */
void sound_set_music_enabled(int enabled)
{
  char cur = *(char *)0x4eaf42;
  char val = (char)enabled;

  if (val != cur) {
    *(char *)0x4eaf42 = val;
    (*(void (**)(int))(*(int *)0x4eaf48 + 0x28))(enabled);
    if (val == '\0')
      *(unsigned int *)0x4eaf4c = system_milliseconds();
  }
}

/* sound_get_previous_ms (0x1cb8e0)
 *
 * Returns the sound system's last-recorded millisecond timestamp.
 */
unsigned int FUN_001cb8e0(void)
{
  return *(unsigned int *)0x4eaf4c;
}

/* Check whether a sound tag can currently play.
 *
 * sound_tag_index is passed in EAX (register argument).
 *
 * Loads the sound tag, checks that pitch range 0 has at least one permutation,
 * then looks up the sound class definition for the tag's class index. If the
 * class definition's "suppress" byte (offset 0x28) is zero, the sound is
 * allowed to play. Returns true (1) if playable, false (0) otherwise. */
bool sound_can_play(int sound_tag_index /* @<eax> */)
{
  void *sound_tag;
  void *pitch_range_element;
  void *class_def;

  sound_tag = tag_get(0x736e6421, sound_tag_index);
  if (*(int *)((char *)sound_tag + 0x98) != 0) {
    pitch_range_element =
      tag_block_get_element((char *)sound_tag + 0x98, 0, 0x48);
    if (*(int *)((char *)pitch_range_element + 0x3c) != 0) {
      class_def = sound_class_get_definition(*(short *)((char *)sound_tag + 4));
      if (*(char *)((char *)class_def + 0x28) == '\0') {
        return 1;
      }
    }
  }
  return 0;
}

/* Return a pointer to the sound listener entry for a local player.
 * The listeners table lives at 0x4eaf58 with a stride of 0x44.
 * Asserts that listener_index is in [0, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS). */
void *sound_listener_get(short listener_index /* @<si> */)
{
  short index = listener_index;

  assert_halt(index >= 0 && index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  return (void *)(0x4eaf58 + (int)index * 0x44);
}

/* Check whether a sound should be promoted to its promotion sound.
 *
 * sound_tag_index is passed in EAX (register argument).
 *
 * Loads the sound tag and checks if it has a promotion count (tag+0x80).
 * If so, updates the promotion accumulator (tag+0x88) based on the time
 * elapsed since the last check (tag+0x8c vs global timestamp 0x4eaf4c).
 * Clamps the accumulator to zero if negative, then adds the promotion
 * interval (tag+0x84). If the accumulator exceeds
 * promotion_count * promotion_interval, either:
 *   - Returns 1 (promote) if a promotion sound tag is set (tag+0x7c != -1),
 *     resetting the accumulator to zero.
 *   - Returns 2 (reject) if no promotion sound, subtracting the interval.
 * Returns 0 if no promotion is needed. */
int16_t sound_check_promotion(int sound_tag_index /* @<eax> */)
{
  char *sound_tag;
  short promotion_count;
  int delta;
  int accumulator;
  int interval;
  int total;

  sound_tag = (char *)tag_get(0x736e6421, sound_tag_index);
  promotion_count = *(short *)(sound_tag + 0x80);
  if (promotion_count == 0)
    return 0;

  /* Update accumulator: add elapsed time delta. */
  delta = *(int *)(sound_tag + 0x8c) - *(int *)0x4eaf4c;
  accumulator = *(int *)(sound_tag + 0x88) + delta;
  *(int *)(sound_tag + 0x88) = accumulator;

  /* Clamp negative accumulator to zero. */
  if (accumulator < 0)
    accumulator = 0;
  *(int *)(sound_tag + 0x88) = accumulator;

  /* Record current timestamp. */
  *(int *)(sound_tag + 0x8c) = *(int *)0x4eaf4c;

  /* Add promotion interval. */
  interval = *(int *)(sound_tag + 0x84);
  total = accumulator + interval;
  *(int *)(sound_tag + 0x88) = total;

  /* Check if accumulated time exceeds promotion threshold. */
  if (total > (int)promotion_count * interval) {
    if (*(int *)(sound_tag + 0x7c) != -1) {
      *(int *)(sound_tag + 0x88) = 0;
      return 1;
    }
    *(int *)(sound_tag + 0x88) = total - interval;
    return 2;
  }

  return 0;
}

/* sound_update_channel_attenuation (0x1cc310)
 *
 * Advance the attenuation envelope for a sound datum. Computes the
 * interpolation parameter t from the transition start/end tick fields
 * (+0xa4, +0xa8) relative to the global sound timestamp at 0x4eaf4c.
 *
 * Three envelope shapes are selected by the short at +0x92:
 *   0 (linear): t is used directly.
 *   1 (power):  t is warped through pow(t, 1/2.5) or 1-pow(1-t, 1/2.5)
 *               depending on whether target > current attenuation.
 *   default:    assert -- invalid envelope type.
 *
 * When t reaches 1.0 the transition start/end fields are cleared.
 * Returns lerp(current_atten, target_atten, shaped_t).
 * If no transition is active (start == end), returns 1.0 (fully audible). */
float sound_update_channel_attenuation(int sound_handle)
{
  char *sound_entry;
  int start_tick;
  int end_tick;
  float t;

  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);
  start_tick = *(int *)(sound_entry + 0xa4);
  end_tick = *(int *)(sound_entry + 0xa8);

  if (start_tick == end_tick) {
    return 1.0f;
  }

  /* Compute t = (current_tick - start) / (end - start), clamped to [0, 1]. */
  t = (float)(*(int *)0x4eaf4c - start_tick) / (float)(end_tick - start_tick);
  if (t < 0.0f) {
    t = 0.0f;
  } else if (t > 1.0f) {
    t = 1.0f;
  }

  /* Apply envelope shape based on type at +0x92. */
  switch (*(short *)(sound_entry + 0x92)) {
  case 0:
    /* Linear: use t directly. */
    break;
  case 1:
    /* Power curve: ease-in or ease-out depending on direction. */
    if (*(float *)(sound_entry + 0xa0) > *(float *)(sound_entry + 0x9c)) {
      t = (float)pow((double)t, (double)(1.0f / 2.5f));
    } else {
      t = (float)(1.0 - pow((double)(1.0f - t), (double)(1.0f / 2.5f)));
    }
    break;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0xa76, 1);
    system_exit(-1);
    break;
  }

  /* When t reaches 1.0, clear the transition. */
  if (t == 1.0f) {
    *(int *)(sound_entry + 0xa8) = 0;
    *(int *)(sound_entry + 0xa4) = 0;
  }

  /* Lerp between current and target attenuation. */
  return (*(float *)(sound_entry + 0xa0) - *(float *)(sound_entry + 0x9c)) * t +
         *(float *)(sound_entry + 0x9c);
}

/* sound_stop_channel (0x1cca60)
 *
 * Stop and release the channel currently holding a sound, then delete the
 * sound datum. sound_handle is passed in EBX (register argument).
 *
 * If the sound has a playing channel (playing_channel_index != NONE at +0x8c):
 *   1. Clear the channel's sound_handle to NONE (-1) in the channel table
 *      at 0x4fc3a0 (stride 0x18).
 *   2. Call sound_channel_stop (0x1cc140, @<di>) to release cache-sound
 *      references and stop the hardware channel.
 *   3. Clear the sound's playing_channel_index to NONE.
 *
 * If the sound has no playing channel but has flags bit 1 set (+0x4 & 2):
 *   Navigate the sound tag's pitch_range -> permutation hierarchy and call
 *   sound_cache_sound_finished (0x1be090) to decrement the cache refcount.
 *
 * If the sound's type (+0x2) is non-zero (looping sound):
 *   Look up the looping-sound datum via datum_absolute_index_to_index
 *   on the looping-sounds table (0x4fdba0). If found, decrement
 *   playing_count (+0x50) and clear the track entry at
 *   +0xd4 + track_index*4 if it matches sound_handle.
 *
 * Clear the sound tag's last-played field (+0x94) if it matches sound_handle.
 * Assert that the sound's playing_channel_index is NONE after processing.
 * Finally, delete the sound datum from the sounds table (0x4fdba4). */
void sound_stop_channel(int sound_handle /* @<ebx> */)
{
  char *sound_entry;
  void *tag_ptr;
  short playing_channel_index;

  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);
  tag_ptr = tag_get(0x736e6421, *(int *)(sound_entry + 0x8));

  playing_channel_index = *(short *)(sound_entry + 0x8c);

  if (playing_channel_index == -1) {
    /* No active channel -- release cache sound if flags bit 1 set. */
    if ((*(uint8_t *)(sound_entry + 0x4) & 2) != 0) {
      void *pitch_range;
      void *permutation;

      pitch_range = tag_block_get_element(
        (char *)tag_get(0x736e6421, *(int *)(sound_entry + 0x8)) + 0x98,
        (int)*(short *)(sound_entry + 0x8e), 0x48);
      permutation = tag_block_get_element(
        (char *)pitch_range + 0x3c, (int)*(short *)(sound_entry + 0x90), 0x7c);
      sound_cache_sound_finished((int)permutation);
    }
  } else {
    /* Active channel -- stop it. */
    assert_halt(playing_channel_index >= 0 &&
                playing_channel_index < *(short *)0x4eb0b4);

    /* Clear the channel's sound_handle to NONE. */
    *(int *)(0x4fc3a0 + (int)playing_channel_index * 0x18) = -1;

    /* Release cache sounds and stop hardware for this channel.
     * Re-read playing_channel_index into DI for the register-arg callee. */
    sound_channel_stop(*(short *)(sound_entry + 0x8c));

    /* Clear the sound's playing_channel_index. */
    *(short *)(sound_entry + 0x8c) = -1;
  }

  /* If this is a looping sound (type != 0), update the looping-sound entry. */
  if (*(short *)(sound_entry + 0x2) != 0) {
    int looping_sound = datum_absolute_index_to_index(
      *(data_t **)0x4fdba0, *(int *)(sound_entry + 0xc));

    if (looping_sound != 0) {
      char *ls = (char *)looping_sound;
      short track_index = *(short *)(sound_entry + 0x94);

      /* Decrement the looping sound's playing count. */
      (*(short *)(ls + 0x50))--;

      /* Clear the track entry if it matches our sound_handle. */
      if (*(int *)(ls + 0xd4 + track_index * 4) == sound_handle) {
        *(int *)(ls + 0xd4 + track_index * 4) = -1;
      }
    }
  }

  /* Clear the sound tag's last-played field if it matches our handle. */
  if (*(int *)((char *)tag_ptr + 0x94) == sound_handle) {
    *(int *)((char *)tag_ptr + 0x94) = -1;
  }

  /* Assert the sound's playing_channel_index is now NONE. */
  {
    char *verify = (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);
    assert_halt(*(short *)(verify + 0x8c) == -1);
  }

  /* Delete the sound datum. */
  datum_delete(*(data_t **)0x4fdba4, sound_handle);
}

/* Sound manager — low-level sound system lifecycle and rendering. */

/* Compute listener distance squared for a channel/source pair. */
extern float FUN_001ccbe0(int channel_index, void *source);

/* Compute distance for random scale checks. */
extern float FUN_001ccca0(int channel_index, void *source);

/* Allocate a sound channel for a source based on its spatialization mode.
 *
 * source is passed in EAX (register argument); priority is on the stack.
 *
 * Behavior depends on source->spatialization_mode (short at offset 0):
 *   - Mode 0 (none): returns channel 0 immediately.
 *   - Mode 2 (single listener): computes distance via 0x1ccbe0 with
 *     channel=-1 and the source pointer. If distance >= priority, returns
 *     the channel index; otherwise returns 0.
 *   - Other modes (1 = listener-relative): iterates over up to 4 local
 *     player listener slots (0x4eaf58 + i*0x44), computing distance for
 *     each active listener. Tracks the closest listener. If found, calls
 *     0x1c8310 to evaluate channel suitability. Returns the best channel
 *     index, or -1 if priority^2 < min distance or source[0x3c] == 1.0f.
 */
int16_t sound_allocate_channel(void *source /* @<eax> */, float priority)
{
  short spatialization_mode;
  int best_channel;
  float best_dist_sq;
  int i;
  char *listener_ptr;
  float sqrt_dist;

  spatialization_mode = *(short *)source;
  best_channel = -1;

  if (spatialization_mode == 0)
    return 0;

  if (spatialization_mode == 2) {
    /* Single listener: compute distance with channel=-1. */
    {
      float dist_result = FUN_001ccbe0(-1, source);
      if (dist_result >= priority)
        return (short)best_channel;
      return 0;
    }
  }

  /* Mode 1 / other: iterate over local player listeners. */
  best_dist_sq = 3.4028235e+38f; /* FLT_MAX (0x7f7fffff) */
  listener_ptr = (char *)0x4eaf58;

  for (i = 0; (short)i < 4; i++, listener_ptr += 0x44) {
    if ((short)i < 0 || (short)i >= 4) {
      display_assert("index>=0 && index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x430, 1);
      system_exit(-1);
    }
    if (*listener_ptr != '\0') {
      /* Compute distance squared for this listener. */
      float this_dist = FUN_001ccbe0(i, source);
      if (this_dist < best_dist_sq) {
        best_dist_sq = this_dist;
        best_channel = i;
      }
    }
  }

  if ((short)best_channel != -1) {
    /* Evaluate channel suitability with sqrt of best distance. */
    sqrt_dist = __builtin_sqrtf(best_dist_sq);
    sound_compute_source_obstruction(best_channel, source, sqrt_dist);
  }

  if (priority * priority < best_dist_sq)
    return -1;
  if (*(int *)((char *)source + 0x3c) == 0x3f800000)
    return -1;

  return (short)best_channel;
}

/* sound_start (0x1ce180)
 *
 * Attempts to start playing a sound from the given sound tag.
 *
 * 1. Loads the sound tag data via tag_get('snd!', sound_tag_index).
 * 2. Validates track_data_size <= 0x30 and source spatialization.
 * 3. For certain sound classes (0x2c, 0x2e, 0x2f), extends the minimum
 *    fade-out deadline and optionally forces spatialization to none.
 * 4. If sound system is initialized and hardware present, checks encoding
 *    compatibility (mono 22k or stereo 22k/44k compressed), volume/distance
 *    culling via random distance check, priority, and channel availability.
 * 5. Allocates a sound datum, fills in tag index, channel, source data,
 *    track data, pitch range, permutation, and timing info.
 * 6. Returns the new sound datum handle, or -1 on failure.
 */
int sound_start(int sound_tag_index, void *source, int object_handle,
                int track_data, void *track_data_ptr, int track_data_size)
{
  int result = -1;
  void *sound_tag;
  float source_scale;
  short sound_class;
  int game_time;
  int fade_deadline;
  short channel_index;
  short promotion_result;
  int sound_handle;
  char *sound_entry;
  int ftol_result;
  short pitch_range_index;
  short permutation_index;
  void *pitch_range_element;
  void *perm_block;
  void *perm_element;

  sound_tag = tag_get(0x736e6421, sound_tag_index);
  source_scale = *(float *)((char *)source + 4);

  /* Assert: track_data_size <= MAXIMUM_SOUND_CALLBACK_DATA (0x30 = 48) */
  if ((short)track_data_size > 0x30) {
    display_assert("track_data_size<=MAXIMUM_SOUND_CALLBACK_DATA",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x240, 1);
    system_exit(-1);
  }

  /* If spatialization_mode != 0, validate source forward vector. */
  if (*(short *)source != 0) {
    if (!valid_real_normal3d((float *)((char *)source + 0x18))) {
      display_assert(
        "source->spatialization_mode==_sound_spatialization_mode_none || "
        "valid_real_normal3d(&source->location.forward)",
        "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x242, 1);
      system_exit(-1);
    }
  }

  /* Check sound class for fade-related classes (0x2c, 0x2e, 0x2f). */
  sound_class = *(short *)((char *)sound_tag + 4);
  if (sound_class == 0x2c || sound_class == 0x2e || sound_class == 0x2f) {
    game_time = game_time_get();
    fade_deadline =
      game_time + *(int *)((char *)sound_tag + 0x84) * 30 / 1000 + 10;
    if (fade_deadline > *(int *)0x4eaf44)
      *(int *)0x4eaf44 = fade_deadline;
    /* If a global flag is set, force spatialization to none. */
    if (*(char *)0x4fc383 != '\0')
      *(short *)source = 0;
  }

  /* Class 0x2f always forces spatialization to none. */
  if (*(short *)((char *)sound_tag + 4) == 0x2f)
    *(short *)source = 0;

  if (*(uint8_t *)0x4eaf40 != 0 && *(uint8_t *)0x4eaf41 != 0) {
    /* Check encoding compatibility: must be 1 pitch range, and either
     * (0 channels + 0 compression) or (1 channel). */
    if (*(short *)((char *)sound_tag + 0x6e) != 1 ||
        !((*(short *)((char *)sound_tag + 0x6c) == 0 &&
           *(short *)((char *)sound_tag + 6) == 0) ||
          *(short *)((char *)sound_tag + 0x6c) == 1)) {
      error(2, "attempt to play a sound that was not a mono 22k compressed "
               "sound or a stereo 22k or 44k compressed sound.");
      goto done;
    }

    /* Volume/distance culling: skip if both source scale and sound
     * skip_fraction are zero (always audible). */
    if (*(float *)((char *)source + 4) != 0.0f ||
        *(float *)((char *)sound_tag + 0x40) != 0.0f) {
      float random_val;
      {
        unsigned int *seed = random_math_get_local_seed_address();
        random_val = random_math_real(seed);
      }
      /* Compute audible distance threshold:
       * (skip_fraction_max - skip_fraction_min) * source_scale
       *   + skip_fraction_min) * sound_tag->max_distance
       * If random_val exceeds this, sound is culled. */
      {
        float skip_min = *(float *)((char *)sound_tag + 0x3c);
        float skip_max = *(float *)((char *)sound_tag + 0x54);
        float max_dist = *(float *)((char *)sound_tag + 0x10);
        float threshold =
          ((skip_max - skip_min) * source_scale + skip_min) * max_dist;
        if (random_val <= threshold)
          goto done;
      }

      /* Compute sound priority. */
      {
        float priority;
        int can_play;
        {
          /* 0x1c8d10: sound_get_default_priority(sound_tag_index) */
          priority = sound_get_default_priority(sound_tag_index);
        }

        can_play = sound_can_play(sound_tag_index);
        if (!can_play)
          goto done;

        channel_index = sound_allocate_channel(source, priority);
        if (channel_index == -1)
          goto done;

        promotion_result = sound_check_promotion(sound_tag_index);

        if (promotion_result != 0) {
          if (promotion_result == 1) {
            /* Promote: recurse with the promotion sound tag. */
            result = sound_start(*(int *)((char *)sound_tag + 0x7c), source,
                                 object_handle, track_data, track_data_ptr,
                                 track_data_size);
            return result;
          }
          /* Reject (promotion_result >= 2). */
          return -1;
        }

        /* Allocate a new sound datum. */
        sound_handle = data_new_at_index(*(data_t **)0x4fdba4);
        result = sound_handle;
        if (sound_handle == -1)
          goto done;

        sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);

        /* 0x1ccca0: compute distance (EAX = channel_index,
         * EDI = source). Returns float distance in ST(0). Then
         * multiply by constant 8.9647 and convert to int. */
        {
          float dist = FUN_001ccca0(channel_index, source);
          ftol_result = (int)(*(float *)0x2c1288 * dist);
        }

        /* Fill in sound entry fields. */
        *(int *)(sound_entry + 0x8) = sound_tag_index;
        *(short *)(sound_entry + 0x8c) = (short)-1;
        *(short *)(sound_entry + 0x6) = channel_index;
        *(short *)(sound_entry + 0x2) = 0;

        /* Compute random scale for this sound instance. */
        {
          float rscale =
            sound_compute_random_scale(*(int *)((char *)sound_tag + 0x14),
                                       *(int *)((char *)sound_tag + 0x18),
                                       *(float *)((char *)sound_tag + 0x44),
                                       *(float *)((char *)sound_tag + 0x5c),
                                       *(float *)((char *)source + 4));
          *(float *)(sound_entry + 0x88) = rscale;
        }

        /* Copy source struct (0x40 bytes = 16 dwords) into sound
         * entry at offset 0x14. */
        *(short *)(sound_entry + 0x4) = 0;
        *(int *)(sound_entry + 0xc) = object_handle;
        csmemcpy(sound_entry + 0x14, source, 0x40);

        /* Store track_data flag and copy track data if present. */
        *(int *)(sound_entry + 0x10) = track_data;
        if (track_data != 0) {
          if (sound_entry + 0x54 == 0) {
            display_assert("sound->track_data",
                           "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x28e,
                           1);
            system_exit(-1);
          }
          csmemcpy(sound_entry + 0x54, track_data_ptr,
                   (int)(short)track_data_size);
        }

        /* Select pitch range and permutation. */
        pitch_range_index = sound_select_pitch_range(
          sound_tag, *(float *)(sound_entry + 0x88), -1);
        *(short *)(sound_entry + 0x8e) = pitch_range_index;

        permutation_index =
          sound_select_permutation(sound_tag, pitch_range_index, -1);
        *(short *)(sound_entry + 0x90) = permutation_index;

        *(int *)(sound_entry + 0xa8) = 0;
        *(int *)(sound_entry + 0xa4) = 0;
        *(short *)(sound_entry + 0x94) = (short)-1;

        /* Look up the permutation element in the sound tag and
         * request it from the sound cache. */
        {
          void *tag_data = tag_get(0x736e6421, *(int *)(sound_entry + 0x8));
          pitch_range_element = tag_block_get_element(
            (char *)tag_data + 0x98, (int)pitch_range_index, 0x48);
          perm_block = (char *)pitch_range_element + 0x3c;
          perm_element =
            tag_block_get_element(perm_block, (int)permutation_index, 0x7c);
          sound_cache_request_sound(perm_element, 0, 1, 0);
        }

        /* Set timing: if ftol_result > 250, delay start by that
         * amount relative to current timestamp, and set bit 0 of
         * flags. */
        if (ftol_result > 250) {
          *(uint8_t *)(sound_entry + 4) = *(uint8_t *)(sound_entry + 4) | 1;
          *(int *)(sound_entry + 0x84) = *(int *)0x4eaf4c + ftol_result;
          return result;
        }
        *(int *)(sound_entry + 0x84) = *(int *)0x4eaf4c;
        return result;
      }
    }
  }

done:
  return result;
}

/* sound_update_channel (0x1ccf80)
 *
 * Apply volume/pan/pitch updates for a non-music sound channel.
 *
 * Resolves the sound datum and tag, then computes a composite volume
 * from the tag's gain range (tag+0x40..0x58), the sound_entry's
 * interpolation fraction (+0x18), per-entry gain (+0x1c), class gain
 * (sound_class_get_gain), and the incoming attenuation scalar.
 * Non-ambient classes (not 0x2c/0x2e/0x2f) additionally scale by
 * the global volume at 0x4eb0b0.
 *
 * If the sound's playing channel index (+0x8c) is NONE (-1), this is a
 * new channel: build a full properties struct (pitch, max_distance,
 * pan, volume, direction, class flags), assert the permutation is
 * cache-loaded, then push properties and start the new permutation via
 * sound_channel_set_properties and sound_channel_start_new.
 *
 * If the channel is already playing, compute volume from the existing
 * permutation's gain and push it directly to the sound driver via
 * vtable+0x34 (volume-only update, flag=1).
 *
 * Finally, call vtable+0x1c to commit the channel update. */
void sound_update_channel(int channel_index, float attenuation)
{
  short si = (short)channel_index;
  int *channel_ptr;
  char *sound_entry;
  char *tag_ptr;
  short class_index;
  float class_gain;
  float fraction;
  float volume;
  int pitch_range;
  int permutation;
  void *class_def;

  /* Properties struct: 0x20 bytes (8 floats).
   *   +0x00 float min_distance (pitch)
   *   +0x04 float max_distance
   *   +0x08 float pan
   *   +0x0C float gain/volume
   *   +0x10 float direction[3] (from tag+0x1c..0x24)
   *   +0x1C int   class_def_flags (class_def+0x10) */
  float properties[8];

  /* Validate channel_index. */
  if (si < 0 || si >= *(short *)0x4eb0b4) {
    display_assert("index>=0 && index<sound_manager_globals.channel_count",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
    system_exit(-1);
  }

  /* Resolve channel, sound entry, and tag. */
  channel_ptr = (int *)(0x4fc3a0 + (int)si * 0x18);
  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, *channel_ptr);
  tag_ptr = (char *)tag_get(0x736e6421, *(int *)(sound_entry + 0x8));

  /* Cache interpolation fraction from sound_entry+0x18. */
  fraction = *(float *)(sound_entry + 0x18);

  /* Get class gain. */
  class_index = *(short *)(tag_ptr + 0x4);
  class_gain = sound_class_get_gain((int)class_index);

  /* Non-ambient classes scale by global volume. */
  if (class_index != 0x2c && class_index != 0x2e && class_index != 0x2f) {
    class_gain = class_gain * *(float *)0x4eb0b0;
  }

  /* Compute composite volume:
   * lerp(tag+0x40, tag+0x58, fraction) * sound_entry+0x1c * class_gain * attenuation */
  volume = (*(float *)(tag_ptr + 0x58) - *(float *)(tag_ptr + 0x40)) * fraction
           + *(float *)(tag_ptr + 0x40);
  volume = volume * *(float *)(sound_entry + 0x1c) * class_gain * attenuation;

  if (*(short *)(sound_entry + 0x8c) == -1) {
    /* New channel: build full properties and start permutation. */
    pitch_range = (int)tag_block_get_element(tag_ptr + 0x98,
                     (int)*(short *)(sound_entry + 0x8e), 0x48);
    permutation = (int)tag_block_get_element((char *)pitch_range + 0x3c,
                     (int)*(short *)(sound_entry + 0x90), 0x7c);

    /* Volume: permutation+0x24 * tag+0x28 * composite volume. */
    properties[3] = *(float *)(permutation + 0x24) * *(float *)(tag_ptr + 0x28) * volume;

    /* Pan: sound_entry+0x88 * pitch_range+0x30. */
    properties[2] = *(float *)(sound_entry + 0x88) * *(float *)(pitch_range + 0x30);

    /* Pitch: min-distance from sound class. */
    properties[0] = sound_class_get_min_distance(*(int *)(sound_entry + 0x8));

    /* Direction from tag+0x1c..0x24. */
    *(int *)&properties[4] = *(int *)(tag_ptr + 0x1c);
    *(int *)&properties[5] = *(int *)(tag_ptr + 0x20);
    *(int *)&properties[6] = *(int *)(tag_ptr + 0x24);

    /* Max distance: FLT_MAX. */
    *(int *)&properties[1] = 0x7f7fffff;

    /* Class definition flags. */
    class_def = sound_class_get_definition(class_index);
    *(int *)&properties[7] = *(int *)((char *)class_def + 0x10);

    /* Assert permutation is cache-loaded. */
    if (!sound_cache_request_sound((void *)permutation, 0, 0, 0)) {
      display_assert("sound_cache_sound_loaded(permutation)",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x79a, 1);
      system_exit(-1);
    }

    /* Push properties and start the new permutation.
     * sound_channel_set_properties: SI=channel_index, EBX=0 (full update).
     * sound_channel_start_new: DI=channel_index, EBX=permutation. */
    sound_channel_set_properties(si, 0, properties);
    sound_channel_start_new(si, permutation);

    /* Record this channel as playing in the sound entry. */
    *(short *)(sound_entry + 0x8c) = si;
  } else {
    /* Existing channel: update volume from current permutation. */
    properties[3] = *(float *)(*(int *)((char *)channel_ptr + 0x10) + 0x24)
                    * *(float *)(tag_ptr + 0x28) * volume;

    /* Validate channel_index again (matches original). */
    if (si < 0 || si >= *(short *)0x4eb0b4) {
      display_assert("index>=0 && index<sound_manager_globals.channel_count",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
      system_exit(-1);
    }

    /* Push volume-only update (flag=1) directly through the sound driver. */
    (*(void (**)(int, void *, int))(*(int *)0x4eaf48 + 0x34))(
        channel_index, properties, 1);
  }

  /* Commit the channel update. */
  (*(void (**)(int))(*(int *)0x4eaf48 + 0x1c))(channel_index);
}

/* sound_update_music_channel (0x1cdc30)
 *
 * Apply volume/pan/pitch updates for a music-class (looping) channel.
 *
 * Resolves the sound datum, its tag (snd!), and the parent looping-sound
 * datum (lsnd).  Computes a composite volume from the tag's gain range
 * (tag+0x40..0x58), the looping-sound track gain (track+0x4), per-entry
 * gain (sound_entry+0x1c), class gain (sound_class_get_gain), the tag's
 * overall gain scalar (tag+0x28), and the incoming attenuation.
 * Non-ambient classes (not 0x2c/0x2e/0x2f) additionally scale by the
 * global volume at 0x4eb0b0.
 *
 * If the sound's playing channel index (+0x8c) is NONE (-1), this is a
 * new channel: build properties, assert the permutation is cache-loaded,
 * then push via sound_channel_set_properties and start_new.
 *
 * If already playing, advance the channel queue (sound_channel_update_status),
 * handle pitch-range crossfading, and manage permutation sequencing for
 * looped playback (select next permutation, handle linked-permutation
 * chains, and looping-sound iteration transitions).
 *
 * Finally, apply the permutation's gain scalar, push properties, and
 * commit the channel update via vtable+0x1c. */
void sound_update_music_channel(int channel_index, float attenuation)
{
  short si = (short)channel_index;
  int *channel_ptr;
  char *sound_entry;
  char *tag_ptr;
  char *looping_sound;
  int *track_channel_ptr;
  char *lsnd_tag;
  char *track_tag;
  short class_index;
  float class_gain;
  float fraction;
  float local_c; /* gain from tag gain range * sound_entry+0x88 */
  int pitch_range;
  int permutation;

  /* Properties struct: 0x20 bytes (8 floats).
   *   +0x00 float min_distance (pitch)
   *   +0x04 float max_distance
   *   +0x08 float pan (gain)
   *   +0x0C float volume
   *   +0x10 float direction[3] (from tag+0x1c..0x24)
   *   +0x1C int   class_def_flags (class_def+0x10) */
  float properties[8];

  /* Validate channel_index. */
  if (si < 0 || si >= *(short *)0x4eb0b4) {
    display_assert("index>=0 && index<sound_manager_globals.channel_count",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
    system_exit(-1);
  }

  /* Resolve channel, sound entry, tag, and looping-sound. */
  channel_ptr = (int *)(0x4fc3a0 + (int)si * 0x18);
  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, *channel_ptr);
  tag_ptr = (char *)tag_get(0x736e6421, *(int *)(sound_entry + 0x8));
  looping_sound = (char *)datum_get(*(data_t **)0x4fdba0,
                                    *(int *)(sound_entry + 0xc));
  track_channel_ptr = (int *)(looping_sound + 0xd4 +
                              (int)*(short *)(sound_entry + 0x94) * 4);
  lsnd_tag = (char *)tag_get(0x6c736e64, *(int *)(looping_sound + 0x4));
  track_tag = (char *)tag_block_get_element(lsnd_tag + 0x3c,
                 (int)*(short *)(sound_entry + 0x94), 0xa0);

  /* Cache interpolation fraction from sound_entry+0x18. */
  fraction = *(float *)(sound_entry + 0x18);

  /* Compute local_c: lerp(tag+0x44, tag+0x5c, fraction) * sound_entry+0x88. */
  local_c = ((*(float *)(tag_ptr + 0x5c) - *(float *)(tag_ptr + 0x44)) * fraction
             + *(float *)(tag_ptr + 0x44)) * *(float *)(sound_entry + 0x88);

  /* Assert this is not an impulse sound. */
  if (*(short *)(sound_entry + 2) == 0) {
    display_assert("sound->type!=_sound_impulse",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x9c4, 1);
    system_exit(-1);
  }

  /* Pitch: min-distance from sound class. */
  properties[0] = sound_class_get_min_distance(*(int *)(sound_entry + 0x8));

  /* Max distance: FLT_MAX. */
  *(int *)&properties[1] = 0x7f7fffff;

  /* Direction from tag+0x1c..0x24. */
  *(int *)&properties[4] = *(int *)(tag_ptr + 0x1c);
  *(int *)&properties[5] = *(int *)(tag_ptr + 0x20);
  *(int *)&properties[6] = *(int *)(tag_ptr + 0x24);

  /* Class definition flags. */
  {
    void *class_def = sound_class_get_definition(*(unsigned short *)(tag_ptr + 0x4));
    *(int *)&properties[7] = *(int *)((char *)class_def + 0x10);
  }

  /* Get class gain. */
  class_index = *(short *)(tag_ptr + 0x4);
  class_gain = sound_class_get_gain((int)(unsigned short)class_index);

  /* Non-ambient classes scale by global volume. */
  if (class_index != 0x2c && class_index != 0x2e && class_index != 0x2f) {
    class_gain = class_gain * *(float *)0x4eb0b0;
  }

  /* Compute composite volume:
   * lerp(tag+0x40, tag+0x58, fraction) * track+0x4 * tag+0x28 *
   * sound_entry+0x1c * class_gain * attenuation */
  properties[3] = (*(float *)(tag_ptr + 0x58) - *(float *)(tag_ptr + 0x40)) * fraction
                  + *(float *)(tag_ptr + 0x40);
  properties[3] = properties[3] * *(float *)(track_tag + 0x4)
                  * *(float *)(tag_ptr + 0x28)
                  * *(float *)(sound_entry + 0x1c)
                  * class_gain * attenuation;

  if (*(short *)(sound_entry + 0x8c) == -1) {
    /* New channel: build full properties and start permutation. */
    pitch_range = (int)tag_block_get_element(tag_ptr + 0x98,
                     (int)*(short *)(sound_entry + 0x8e), 0x48);
    permutation = (int)tag_block_get_element((char *)pitch_range + 0x3c,
                     (int)*(short *)(sound_entry + 0x90), 0x7c);

    /* Volume: permutation+0x24 * existing volume. */
    properties[3] = properties[3] * *(float *)(permutation + 0x24);

    /* Pan: local_c * pitch_range+0x30. */
    properties[2] = local_c * *(float *)(pitch_range + 0x30);

    /* Assert permutation is cache-loaded. */
    if (!sound_cache_request_sound((void *)permutation, 0, 0, 0)) {
      display_assert("sound_cache_sound_loaded(permutation)",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x9da, 1);
      system_exit(-1);
    }

    /* Push properties and start the new permutation. */
    sound_channel_set_properties(si, 0, properties);
    sound_channel_start_new(si, permutation);

    /* Record this channel as playing in the sound entry. */
    *(short *)(sound_entry + 0x8c) = si;

    /* Commit the channel update. */
    (*(void (**)(int))(*(int *)0x4eaf48 + 0x1c))(channel_index);
    return;
  }

  /* Existing channel: resolve pitch range block. */
  pitch_range = (int)tag_block_get_element(tag_ptr + 0x98,
                   (int)*(short *)(sound_entry + 0x8e), 0x48);

  /* Validate the existing playing channel index. */
  {
    short playing_channel = *(short *)(sound_entry + 0x8c);
    if (playing_channel < 0 || playing_channel >= *(short *)0x4eb0b4) {
      display_assert("index>=0 && index<sound_manager_globals.channel_count",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
      system_exit(-1);
    }
  }

  /* Volume crossfade toward target. */
  {
    short playing_channel = *(short *)(sound_entry + 0x8c);
    int playing_idx = (int)playing_channel;
    float channel_rate = *(float *)(0x4fc3ac + playing_idx * 0x18);
    local_c = sound_volume_crossfade(local_c,
                 channel_rate * *(float *)(pitch_range + 0x20),
                 *(float *)(tag_ptr + 0x2c));
  }

  /* Pan: local_c * pitch_range+0x30. */
  properties[2] = local_c * *(float *)(pitch_range + 0x30);

  /* Pitch-range crossfade: if the sound is in looping state and the
   * fade endpoints differ (or target attenuation not reached), try
   * selecting a new pitch range. If it changed, allocate a new looping
   * entry and start a crossfade. */
  if (*(short *)(sound_entry + 0x2) == 2 &&
      (*(int *)(sound_entry + 0xa4) == *(int *)(sound_entry + 0xa8) ||
       *(float *)(sound_entry + 0xa0) != *(float *)0x2533c0)) {
    short new_pitch_range = sound_select_pitch_range(
        tag_ptr, local_c, (int)(unsigned short)*(short *)(sound_entry + 0x8e));
    if (new_pitch_range != *(short *)(sound_entry + 0x8e)) {
      if (*channel_ptr == *track_channel_ptr &&
          *(char *)0x4eaf43 == '\0') {
        int new_entry = sound_create_looping_entry(
            *(int *)(sound_entry + 0x8),
            *(int *)(sound_entry + 0xc),
            (int)(unsigned short)*(short *)(sound_entry + 0x94), 2);
        if (new_entry != -1) {
          sound_start_fade(1, *(float *)0x2c1278, new_entry, *channel_ptr);
          *track_channel_ptr = new_entry;
        }
      }
    }
  }

  /* Check if the sound should stop or continue. */
  if (*(short *)(sound_entry + 0x2) == 4)
    goto final_update;

  if (*(short *)(sound_entry + 0x2) == 1 && (*(uint8_t *)track_tag & 1) != 0)
    goto final_update;

  {
    short status = sound_channel_update_status(*(short *)(sound_entry + 0x8c));
    if (status == 2 &&
        (*(uint8_t *)(sound_entry + 0x4) & 8) == 0) {
      /* Channel finished playing; check if permutation/next-sound chain
       * means we should skip to final update rather than queue more. */
      if (*(short *)(channel_ptr[4] + 0x2a) != -1)
        goto final_update;
      if (*(int *)(sound_entry + 0x98) == -1)
        goto final_update;
    }

    /* Permutation sequencing. */
    if (*(int *)(sound_entry + 0x98) == -1 ||
        (channel_ptr[4] != 0 && *(short *)(channel_ptr[4] + 0x2a) != -1)) {
      /* Next permutation ready or queued. */
      if ((*(uint8_t *)(sound_entry + 0x4) & 8) == 0) {
        short next_perm = sound_select_permutation(
            tag_ptr, *(unsigned short *)(sound_entry + 0x8e),
            *(unsigned short *)(sound_entry + 0x90));
        if (next_perm == -1) {
          if ((*(uint8_t *)tag_ptr & 2) == 0) {
            display_assert(
              "TEST_FLAG(definition->flags, _sound_definition_linked_permutations_bit)",
              "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0xa1c, 1);
            system_exit(-1);
          }
          if ((*(uint8_t *)lsnd_tag & 2) == 0) {
            next_perm = sound_select_permutation(
                tag_ptr, *(unsigned short *)(sound_entry + 0x8e), -1);
            if (next_perm != -1)
              goto set_next_perm;
          } else {
            *(short *)(sound_entry + 0x2) = 4;
            *(uint8_t *)(looping_sound + 0x4e) = 1;
          }
        } else {
set_next_perm:
          *(uint8_t *)(sound_entry + 0x4) |= 8;
          *(short *)(sound_entry + 0x90) = next_perm;
        }
      }
    } else {
      /* No next permutation and no queued: start next looping iteration. */
      sound_start_next_looping_permutation(*channel_ptr);
      tag_ptr = (char *)tag_get(0x736e6421, *(int *)(sound_entry + 0x8));
      pitch_range = (int)tag_block_get_element(tag_ptr + 0x98,
                       (int)*(short *)(sound_entry + 0x8e), 0x48);
    }

    /* Get the current permutation and try to cache-load it. */
    permutation = (int)tag_block_get_element((char *)pitch_range + 0x3c,
                     (int)*(short *)(sound_entry + 0x90), 0x7c);
    if (*(short *)(sound_entry + 0x2) != 4 &&
        sound_cache_request_sound((void *)permutation, 0, 1, 1)) {
      *(uint8_t *)(sound_entry + 0x4) &= ~8;
      sound_channel_start_new((short)channel_index, permutation);

      /* If both the next-sound field (+0x98) and the permutation's
       * next-permutation link (+0x2a) are NONE, the chain has ended:
       * advance type 1->2 (start looping) or 3->4 (stop). */
      if (*(int *)(sound_entry + 0x98) == -1 &&
          *(short *)(permutation + 0x2a) == -1) {
        if (*(short *)(sound_entry + 0x2) == 1) {
          *(short *)(sound_entry + 0x2) = 2;
        } else if (*(short *)(sound_entry + 0x2) == 3) {
          *(short *)(sound_entry + 0x2) = 4;
        }
      }
    }
  }

final_update:
  /* Final permutation gain and property push. */
  permutation = (int)tag_block_get_element((char *)pitch_range + 0x3c,
                   (int)*(short *)(sound_entry + 0x90), 0x7c);
  properties[3] = properties[3] * *(float *)(permutation + 0x24);

  sound_channel_set_properties(si, 0, properties);

  /* Commit the channel update. */
  (*(void (**)(int))(*(int *)0x4eaf48 + 0x1c))(channel_index);
}

/* sound_update_music (0x1ceda0)
 *
 * Per-channel tick for spatialized sound playback. Iterates the global
 * channel table (0x4fc3a0, stride 0x18 bytes, count at 0x4eb0b4) and, for
 * each channel that holds a live sound:
 *
 *   1. Resolves the sound datum and its tag, then updates the channel's
 *      attenuation curve via sound_update_channel_attenuation (0x1cc310,
 *      @<eax>). If the computed attenuation and the sound entry's target
 *      attenuation (sound_entry+0xa0) both reach 0.0f, the sound has
 *      faded out — stop it via sound_stop_channel (0x1cca60, @<ebx>)
 *      and mark the channel free (-1).
 *   2. Otherwise, dispatches on the sound's spatialization mode
 *      (sound_entry+0x14). Two top-level branches select on the channel
 *      flags bit 0 (+0x4 & 1):
 *        - BIT SET: drive the sound driver directly. Mode 0 asserts,
 *          mode 1 transforms position/forward/up into the listener's
 *          frame (matrix3x3 transforms + velocity compensation via
 *          listener+0x38..0x40 scaled by 30.0) and issues vtable+0x30
 *          with the transformed triple plus sound_entry+0x4c/0x50 and
 *          listener[+1]. Mode 2 issues vtable+0x30 with the raw
 *          world-space position.
 *        - BIT CLEAR: compute an audible-volume scalar. Copy the source
 *          position, then for mode 1 fetch the listener via
 *          sound_listener_get (0x1cbac0, @<si>) and transform the
 *          position through real_matrix3x3_transform_vector (0x1096e0).
 *          For modes 1 and 2, scale the current attenuation by
 *          1 - (sqrt(|pos|^2) - min_dist) / (max_dist - min_dist) using
 *          sound_get_default_priority variants (0x1c8d50 min-dist,
 *          0x1c8d10 max-dist), clamped to [0, 1]. Mode 0 leaves the
 *          attenuation unchanged.
 *   3. Update the per-channel volume/pan/pitch state. If the sound's
 *      channel kind (sound_entry+0x2) is 0, invoke sound_update_channel
 *      (0x1ccf80); otherwise invoke sound_update_music_channel
 *      (0x1cdc30). Both take (channel_index, attenuation).
 *   4. If the sound class is marked "pitch-track" (class_def+0x8) and
 *      the sound's update hook (sound_entry+0x10) matches the pitch
 *      callback at 0x1c7a10, evaluate the next pitch sample via
 *      sound_get_random_permutation_pitch (0x1c8f20) driven by the
 *      channel's current ftol-truncated phase (channel+0x8 -> int) and
 *      channel+0x10, then forward it to the pitch sink at 0x1c7b00 with
 *      sound_entry+0xc. */
void sound_update_music(void)
{
  short channel_count;
  int i;
  int *channel;
  char *sound_entry;
  void *tag_ptr;
  int tag_handle;
  float attenuation;
  float audible_scale;
  /* Must be contiguous: dsound reads this as location_t (pos/fwd/up). */
  struct {
    float position[3];
    float forward[3];
    float up[3];
  } location;
  char *listener;
  void *class_def;

  channel_count = *(short *)0x4eb0b4;
  if (channel_count <= 0)
    return;

  for (i = 0; (short)i < channel_count; i++) {
    if ((short)i < 0 || (short)i >= *(short *)0x4eb0b4) {
      display_assert("index>=0 && index<sound_manager_globals.channel_count",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
      system_exit(-1);
    }

    /* Channel struct: stride 0x18 bytes starting at 0x4fc3a0.
     *   +0x00 int sound_handle (-1 if free)
     *   +0x04 uint32 flags (bit 0: active playback path)
     *   +0x08 float phase_accumulator
     *   +0x10 int pitch_param */
    channel = (int *)(0x4fc3a0 + (short)i * 0x18);
    if (channel[0] == -1)
      continue;

    sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, channel[0]);
    tag_handle = *(int *)(sound_entry + 0x8);
    tag_ptr = tag_get(0x736e6421, tag_handle);

    /* Update attenuation curve for this channel. EAX arg is the
     * channel's sound_handle (register-passed). */
    attenuation = sound_update_channel_attenuation(channel[0]);
    audible_scale = attenuation;

    /* Faded out: both current and target reached 0.0f. */
    if (attenuation == *(float *)0x2533c0 &&
        *(float *)(sound_entry + 0xa0) == *(float *)0x2533c0) {
      sound_stop_channel(channel[0]);
      channel[0] = -1;
      continue;
    }

    if ((*(uint8_t *)((char *)channel + 0x4) & 1) != 0) {
      /* Active playback path: drive sound driver. */
      short mode = *(short *)(sound_entry + 0x14);
      if (mode == 0) {
        display_assert(0, "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x7d4, 1);
        system_exit(-1);
      } else if (mode == 1) {
        /* Listener-relative spatialization. */
        listener = (char *)sound_listener_get(*(short *)(sound_entry + 0x6));
        if (*listener == '\0') {
          display_assert("listener->valid",
                         "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x7db, 1);
          system_exit(-1);
        }

        real_matrix3x3_transform_vector(listener + 4,
                                        (void *)(sound_entry + 0x20),
                                        (void *)location.position);
        real_matrix4x3_transform_point(listener + 4, sound_entry + 0x2c,
                                       location.forward);
        real_matrix3x3_transform_vector(
          listener + 4, (void *)(sound_entry + 0x38), (void *)location.up);

        /* Scale up-vector by 30.0 and subtract listener velocity
         * (listener+0x38..0x40). */
        location.up[0] =
          location.up[0] * *(float *)0x253394 - *(float *)(listener + 0x38);
        location.up[1] =
          location.up[1] * *(float *)0x253394 - *(float *)(listener + 0x3c);
        location.up[2] =
          location.up[2] * *(float *)0x253394 - *(float *)(listener + 0x40);

        (*(void (**)(int, int, void *, int, int, int))(*(int *)0x4eaf48 +
                                                       0x30))(
          i, 1, location.position, *(int *)(sound_entry + 0x4c),
          *(int *)(sound_entry + 0x50), (int)listener[1]);
      } else if (mode == 2) {
        (*(void (**)(int, int, void *, int, int, int))(
          *(int *)0x4eaf48 + 0x30))(i, 1, (void *)(sound_entry + 0x20), 0, 0,
                                    0);
      } else {
        display_assert(0, "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x7ec, 1);
        system_exit(-1);
      }
    } else {
      /* Inactive/culled path: compute audible_scale from attenuation. */
      short mode;
      location.position[0] = *(float *)(sound_entry + 0x20);
      location.position[1] = *(float *)(sound_entry + 0x24);
      location.position[2] = *(float *)(sound_entry + 0x28);
      mode = *(short *)(sound_entry + 0x14);
      if (mode != 0) {
        if (mode == 1) {
          listener = (char *)sound_listener_get(*(short *)(sound_entry + 0x6));
          if (*listener == '\0') {
            display_assert("listener->valid",
                           "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x7fa,
                           1);
            system_exit(-1);
          }
          real_matrix3x3_transform_vector(listener + 4,
                                          (void *)(sound_entry + 0x20),
                                          (void *)location.position);
        } else if (mode != 2) {
          display_assert(0, "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x80a,
                         1);
          system_exit(-1);
        }

        /* Audible falloff: 1 - (sqrt(dist^2) - min) / (max - min),
         * clamped to [0, 1]. min/max come from the sound's class
         * definition (tag+class_index). */
        {
          float min_dist = sound_class_get_min_distance(tag_handle);
          float max_dist = sound_get_default_priority(tag_handle);
          float sq = location.position[0] * location.position[0] +
                     location.position[1] * location.position[1] +
                     location.position[2] * location.position[2];
          float falloff =
            *(float *)0x2533c8 -
            (__builtin_sqrtf(sq) - min_dist) / (max_dist - min_dist);
          if (falloff < *(float *)0x2533c0)
            falloff = *(float *)0x2533c0;
          else if (falloff > *(float *)0x2533c8)
            falloff = *(float *)0x2533c8;
          audible_scale = falloff * audible_scale;
        }
      }
    }

    /* Push updated volume/pan/pitch (music vs regular channel). */
    if (*(short *)(sound_entry + 2) == 0)
      sound_update_channel(i, audible_scale);
    else
      sound_update_music_channel(i, audible_scale);

    /* Pitch-track hook: if class_def says pitch-track and this
     * sound's update callback is 0x1c7a10, push the next pitch
     * sample computed from channel+0x8 (phase, truncated to int) and
     * channel+0x10. */
    class_def = sound_class_get_definition(*(short *)((char *)tag_ptr + 4));
    if (*(char *)((char *)class_def + 8) != '\0' &&
        *(void **)(sound_entry + 0x10) == (void *)0x1c7a10) {
      int phase_int = (int)*(float *)((char *)channel + 0x8);
      float pitch_value = sound_get_permutation_pitch(channel[4], phase_int);
      sound_pitch_push_sample(*(int *)(sound_entry + 0xc), pitch_value);
    }
  }
}

/* sound_pump (0x1cf2f0)
 *
 * Stripped-down sound tick used during fade-out spin loops.  Locks the
 * sound driver, updates timing + music, unlocks, then runs game_sound_update.
 */
void FUN_001cf2f0(void)
{
  int current_ms;

  *(uint8_t *)0x4eaf43 = 1;
  if (*(uint8_t *)0x4eaf40 != 0 && *(uint8_t *)0x4eaf41 != 0) {
    (*(void (**)(void))(*(int *)0x4eaf48 + 0x10))();
    if (*(uint8_t *)0x4eaf42 == 0) {
      current_ms = system_milliseconds();
      *(float *)0x4eaf50 = (float)(current_ms - *(int *)0x4eaf4c) * 0.03f;
      *(int *)0x4eaf4c = current_ms;
      sound_update_music();
    }
    (*(void (**)(void))(*(int *)0x4eaf48 + 0x14))();
  }
  xbox_sound_cache_idle();
  *(uint8_t *)0x4eaf43 = 0;
}

void sound_dispose_from_old_map(void)
{
  unsigned int start_ms;
  int sound_index;
  float fade_end_ms;
  unsigned int now_ms;
  float now_ms_f;

  if (*(uint8_t *)0x4eaf42 == 0) {
    /* Only attempt fade if both initialized and hardware_present. */
    if (*(uint8_t *)0x4eaf40 == 0 || *(uint8_t *)0x4eaf41 == 0)
      goto skip_fade;

    /* Record start time and iterate all active sounds, triggering fade-out. */
    start_ms = ((unsigned int (*)(void))0x8e370)();
    sound_index = ((int (*)(void *, int))0x1198f0)(*(void **)0x4fdba4, -1);
    if (sound_index != -1) {
      do {
        /* sound_manager_fade: mode=0 (linear), seconds=0.3f,
         * fade_in_sound_index=NONE (-1), fade_out_sound_index=sound_index */
        ((void (*)(short, float, int, int))0x1cc8f0)(0, 0.3f, -1, sound_index);
        sound_index =
          ((int (*)(void *, int))0x1198f0)(*(void **)0x4fdba4, sound_index);
      } while (sound_index != -1);

      /* Compute deadline: start_ms + 300.0f ms (constant at 0x2c1a60). */
      fade_end_ms = (float)start_ms + 300.0f;

      /* Spin until current time >= fade_end_ms, pumping sound each iteration.
       * Unsigned→float: if MSB set, add 2^32 to correct the signed cast. */
      while (1) {
        now_ms = ((unsigned int (*)(void))0x8e370)();
        now_ms_f = (float)(int)now_ms;
        if ((int)now_ms < 0)
          now_ms_f = now_ms_f + 4294967296.0f;
        if (fade_end_ms <= now_ms_f)
          break;
        ((void (*)(void))0x1cf2f0)();
      }
    }

    if (*(uint8_t *)0x4eaf42 == 0)
      goto skip_fade;
  }

  /* Clear the fading flag, stop hardware output, record current timestamp. */
  *(uint8_t *)0x4eaf42 = 0;
  (*(void (**)(int))((*(uint8_t **)0x4eaf48) + 0x28))(0);
  *(unsigned int *)0x4eaf4c = ((unsigned int (*)(void))0x8e370)();

skip_fade:
  /* Stop all active sound channels and reset channel count. */
  ((void (*)(void))0x1cd540)();

  /* Re-validate the looping-sounds table for the next map if present. */
  if (*(void **)0x4fdba0 != 0)
    ((void (*)(void *))0x119720)(*(void **)0x4fdba0);
}

/* Per-frame sound rendering tick.
 *
 * Guarded by profiling markers (profile_enter/exit_private on "sound_render").
 * If the sound system is initialized (0x4eaf40) and hardware is present
 * (0x4eaf41):
 *   1. Call vtable+0x10 on the sound driver (lock / begin-frame).
 *   2. If not currently fading (0x4eaf42 == 0):
 *      a. Compute delta_ms = (current_ms - previous_ms) * 0.03f and store
 *         to the global sound delta (0x4eaf50). Update previous_ms (0x4eaf4c).
 *      b. Convert delta to integer ticks and pass to sound_cache_update
 *         (0x1c8c00) via the sound_listener_update result (0x1d9068).
 *      c. Run the sound subsystem pipeline: sound_update_channels (0x1ce9c0),
 *         sound_update_sources (0x1cf100), sound_update_output (0x1cd690),
 *         sound_update_effects (0x1cf360), sound_update_music (0x1ceda0).
 *      d. Toggle the per-frame flip flag at 0x4eaf54.
 *   3. Call vtable+0x14 on the sound driver (unlock / end-frame).
 * If not fading, also call game_sound_update (0x1bded0). */
void sound_render(void)
{
  int current_ms;
  float delta;

  /* Profiling: enter "sound_render" section. */
  if (*(uint8_t *)0x449ef1 != 0 && *(uint8_t *)0x32f6f0 != 0)
    profile_enter_private((void *)0x32f6e8);

  if (*(uint8_t *)0x4eaf40 != 0 && *(uint8_t *)0x4eaf41 != 0) {
    /* Lock / begin-frame on the sound driver. */
    (*(void (**)(void))(*(int *)0x4eaf48 + 0x10))();

    if (*(uint8_t *)0x4eaf42 == 0) {
      /* Compute time delta in sound-system units (ms * 0.03). */
      current_ms = system_milliseconds();
      delta = (float)(current_ms - *(int *)0x4eaf4c) * 0.03f;
      *(int *)0x4eaf4c = current_ms;
      *(float *)0x4eaf50 = delta;

      /* Update sound subsystems. The truncated delta is passed to the
       * cache/listener update chain. Original calls __ftol2 (0x1d9068) to
       * truncate the float delta; we use a plain C cast. */
      ((void (*)(int))0x1c8c00)((int)*(float *)0x4eaf50);
      ((void (*)(void))0x1ce9c0)();
      ((void (*)(void))0x1cf100)();
      ((void (*)(void))0x1cd690)();
      ((void (*)(void))0x1cf360)();
      ((void (*)(void))0x1ceda0)();

      /* Toggle per-frame flip flag. */
      *(uint8_t *)0x4eaf54 = *(uint8_t *)0x4eaf54 == 0;
    }

    /* Unlock / end-frame on the sound driver. */
    (*(void (**)(void))(*(int *)0x4eaf48 + 0x14))();
  }

  /* Update game sound (ambient/scripted) when not fading. */
  if (*(uint8_t *)0x4eaf42 == 0)
    ((void (*)(void))0x1bded0)();

  /* Profiling: exit "sound_render" section. */
  if (*(uint8_t *)0x449ef1 != 0 && *(uint8_t *)0x32f6f0 != 0)
    profile_exit_private((void *)0x32f6e8);
}
