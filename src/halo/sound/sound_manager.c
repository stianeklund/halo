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
