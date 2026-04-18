/* Sound manager — low-level sound system lifecycle and rendering. */

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
      error(2,
            "attempt to play a sound that was not a mono 22k compressed "
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

        /* 0x1cba20: check if sound can play (EAX = sound_tag_index).
         * Returns bool in AL. */
        {
          int _eax = sound_tag_index;
          __asm__ __volatile__(
            "call *%[fn]"
            : "+a"(_eax)
            : [fn] "r"((void *)0x1cba20)
            : "ecx", "edx", "esi", "memory", "cc");
          can_play = (uint8_t)_eax;
        }
        if (!can_play)
          goto done;

        /* 0x1cd5a0: allocate channel (EAX = source ptr, stack arg =
         * priority float). Returns channel index (short) in AX, or -1
         * on failure. */
        {
          int _eax = (int)source;
          int pri_bits;
          csmemcpy(&pri_bits, &priority, 4);
          __asm__ __volatile__(
            "pushl %[pri]\n\t"
            "call *%[fn]\n\t"
            "addl $4, %%esp"
            : "+a"(_eax)
            : [fn] "r"((void *)0x1cd5a0),
              [pri] "r"(pri_bits)
            : "ecx", "edx", "esi", "edi", "memory", "cc");
          channel_index = (short)_eax;
        }
        if (channel_index == -1)
          goto done;

        /* 0x1cbb00: check promotion (EAX = sound_tag_index).
         * Returns 0=play, 1=promote, 2+=reject. */
        {
          int _eax = sound_tag_index;
          __asm__ __volatile__(
            "call *%[fn]"
            : "+a"(_eax)
            : [fn] "r"((void *)0x1cbb00)
            : "ecx", "edx", "esi", "edi", "memory", "cc");
          promotion_result = (short)_eax;
        }

        if (promotion_result != 0) {
          if (promotion_result == 1) {
            /* Promote: recurse with the promotion sound tag. */
            result = sound_start(
              *(int *)((char *)sound_tag + 0x7c), source,
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

        sound_entry =
          (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);

        /* 0x1ccca0: compute distance (EAX = channel_index,
         * EDI = source). Returns float distance in ST(0). Then
         * multiply by constant 8.9647 and convert to int. */
        {
          int _eax = (int)(short)channel_index;
          int _edi = (int)source;
          float dist;
          __asm__ __volatile__(
            "call *%[fn]\n\t"
            "fstps %[out]"
            : "+a"(_eax), "+D"(_edi), [out] "=m"(dist)
            : [fn] "r"((void *)0x1ccca0)
            : "ecx", "edx", "esi", "memory", "cc");
          ftol_result = (int)(*(float *)0x2c1288 * dist);
        }

        /* Fill in sound entry fields. */
        *(int *)(sound_entry + 0x8) = sound_tag_index;
        *(short *)(sound_entry + 0x8c) = (short)-1;
        *(short *)(sound_entry + 0x6) = channel_index;
        *(short *)(sound_entry + 0x2) = 0;

        /* Compute random scale for this sound instance. */
        {
          float rscale = sound_compute_random_scale(
            *(int *)((char *)sound_tag + 0x14),
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
            display_assert(
              "sound->track_data",
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

        permutation_index = sound_select_permutation(
          sound_tag, pitch_range_index, -1);
        *(short *)(sound_entry + 0x90) = permutation_index;

        *(int *)(sound_entry + 0xa8) = 0;
        *(int *)(sound_entry + 0xa4) = 0;
        *(short *)(sound_entry + 0x94) = (short)-1;

        /* Look up the permutation element in the sound tag and
         * request it from the sound cache. */
        {
          void *tag_data =
            tag_get(0x736e6421, *(int *)(sound_entry + 0x8));
          pitch_range_element = tag_block_get_element(
            (char *)tag_data + 0x98,
            (int)pitch_range_index, 0x48);
          perm_block = (char *)pitch_range_element + 0x3c;
          perm_element = tag_block_get_element(
            perm_block, (int)permutation_index, 0x7c);
          sound_cache_request_sound(perm_element, 0, 1, 0);
        }

        /* Set timing: if ftol_result > 250, delay start by that
         * amount relative to current timestamp, and set bit 0 of
         * flags. */
        if (ftol_result > 250) {
          *(uint8_t *)(sound_entry + 4) =
            *(uint8_t *)(sound_entry + 4) | 1;
          *(int *)(sound_entry + 0x84) =
            *(int *)0x4eaf4c + ftol_result;
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
