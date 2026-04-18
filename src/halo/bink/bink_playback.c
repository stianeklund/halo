/* Bink video playback system for cinematics and loading screens. */

/* Returns true if a bink video is currently open and the subsystem
 * is initialized. Used by callers to gate rendering and input. */
bool bink_playback_active(void)
{
  if (*(int *)0x4ead60 != 0 && *(uint8_t *)0x4ead58 != 0)
    return true;
  return false;
}

/* Returns true if a bink video handle is open (regardless of whether
 * the subsystem is initialized). */
bool bink_playback_has_video(void)
{
  return *(int *)0x4ead60 != 0;
}

/* Initialize the bink playback globals and register callbacks. */
void bink_playback_initialize(void)
{
  csmemset((void *)0x4ead58, 0, 0xd8);
  ((void (*)(void *, void *))0x231490)((void *)0x1c5ab0, (void *)0x1c5ca0);
  *(uint8_t *)0x4ead58 = 1;
}

/* Stop the currently playing bink video. Closes the bink handle,
 * releases texture cache memory, restores the pregame loading flag,
 * reloads the main menu if the flag was set, and marks event time. */
void bink_playback_stop(void)
{
  if (*(uint8_t *)0x4ead58 == 0)
    return;

  /* If events were suppressed during playback, re-enable them. */
  if ((*(uint32_t *)0x4ead5c & 4) != 0) {
    ((void (*)(int))0xdc240)(0);
  }

  /* Close the bink handle if one is open. */
  if (*(int *)0x4ead60 != 0) {
    ((void (*)(int))0x231220)(*(int *)0x4ead60);
    *(int *)0x4ead60 = 0;
  }

  /* Release bink texture cache memory. */
  ((void (*)(void))0x1c61d0)();

  /* If the main menu flag was set, reload the main menu. */
  if ((*(uint32_t *)0x4ead5c & 0x20) != 0) {
    main_menu_load();
  }

  /* Restore the pregame loading flag from saved value and clear flags. */
  *(uint8_t *)0x31fa96 = *(uint8_t *)0x32eba0;
  *(uint32_t *)0x4ead5c = 0;

  event_manager_mark_time();
}

/* Dispose: stop any playing video and clear globals. */
void bink_playback_dispose(void)
{
  if (*(uint8_t *)0x4ead58 != 0) {
    bink_playback_stop();
    csmemset((void *)0x4ead58, 0, 0xd8);
  }
}

/* Render the current bink frame if a video is playing. Handles
 * the pregame/loading flag to decide when to flush. */
void bink_playback_render(void)
{
  if (*(uint8_t *)0x4ead58 == 0)
    return;
  if (*(int *)0x4ead60 == 0)
    return;

  if (*(uint8_t *)0x31fa96 != 0) {
    *(uint8_t *)0x4ead59 = 1;
  } else if (*(uint8_t *)0x4ead59 == 0) {
    goto skip_flush;
  }

  ((void (*)(void))0x1c6230)();
  *(uint8_t *)0x4ead59 = 0;

skip_flush:
  ((void (*)(void))0x1c5d70)();
  *(int *)0x4ead88 += 1;

  if (*(uint8_t *)0x31fa96 != 0)
    return;

  ((void (*)(void))0x1c6340)();
}

/* Update bink playback state (called once per frame outside rendering). */
void bink_playback_update(void)
{
  if (*(uint8_t *)0x31fa96 != 0)
    ((void (*)(void))0x1c6340)();
}
