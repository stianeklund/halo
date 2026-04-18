/* Internal timing/memory checkpoint. Queries memory stats and stores
 * available kilobytes to a global. The string parameter is a debug
 * label unused in retail. */
void bink_playback_trace(const char *msg);

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

/* Internal: render the bink frame quad on screen with debug overlay. */
void bink_playback_render_frame(void);

/* Initialize the bink playback globals and register callbacks. */
void bink_playback_initialize(void)
{
  csmemset((void *)0x4ead58, 0, 0xd8);
  BinkSetSoundSystem((void *)0x1c5ab0, (void *)0x1c5ca0);
  *(uint8_t *)0x4ead58 = 1;
}

/* Internal: release texture cache memory stolen for bink playback. */
void bink_playback_release_texture_cache(void);

/* Internal: decode and blit the current bink frame to the texture. */
void bink_playback_decode_frame(void);

/* Stop the currently playing bink video. Closes the bink handle,
 * releases texture cache memory, restores the pregame loading flag,
 * reloads the main menu if the flag was set, and marks event time. */
void bink_playback_stop(void)
{
  if (*(uint8_t *)0x4ead58 == 0)
    return;

  /* If events were suppressed during playback, re-enable them. */
  if ((*(uint32_t *)0x4ead5c & 4) != 0) {
    event_manager_suppress(0);
  }

  /* Close the bink handle if one is open. */
  if (*(int *)0x4ead60 != 0) {
    BinkClose((void *)*(int *)0x4ead60);
    *(int *)0x4ead60 = 0;
  }

  /* Release bink texture cache memory. */
  bink_playback_release_texture_cache();

  /* If the main menu flag was set, reload the main menu. */
  if ((*(uint32_t *)0x4ead5c & 0x20) != 0) {
    main_menu_load();
  }

  /* Restore the pregame loading flag from saved value and clear flags.
   * The original loads the saved byte, clears the flags dword, then
   * stores - preserving the load across the clear. */
  {
    uint8_t saved = *(uint8_t *)0x32eba0;
    *(uint32_t *)0x4ead5c = 0;
    *(uint8_t *)0x31fa96 = saved;
  }

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

/* Internal: check whether the video should stop (end of file, skip
 * button, etc.) and jump to bink_playback_stop if so. */
void bink_playback_check_stop(void);

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

  bink_playback_decode_frame();
  *(uint8_t *)0x4ead59 = 0;

skip_flush:
  bink_playback_render_frame();
  *(int *)0x4ead88 += 1;

  if (*(uint8_t *)0x31fa96 != 0)
    return;

  bink_playback_check_stop();
}

/* Open and begin playing a bink video file. Allocates memory from the
 * texture cache, opens the bink file, sets up the D3D texture, and
 * configures the render state for fullscreen video playback.
 *
 * flags bit 0x80: high-res mode (16MB pool instead of 4MB)
 * flags bit 0x04: suppress event manager during playback
 * flags bit 0x20: reload main menu when video stops
 * Other flag bits are stored and checked by bink_playback_check_stop. */
void bink_playback_start(const char *filename, unsigned int flags)
{
  uint32_t mem_status[8];
  int *bink;
  unsigned int pool_size;
  unsigned int frame_buf_size;
  int ds_handle;
  int ds_ok;
  void *pool_base;
  void *frame_buf;
  unsigned int hi_res;
  unsigned int bink_flags;
  short bpp;
  int bits_total;

  /* Query memory stats and update global available-KB counter. */
  csmemset(mem_status, 0, 0x20);
  mem_status[0] = 0x20;
  xbox_query_global_memory_status(mem_status);
  *(uint32_t *)0x32eb9c = mem_status[3] >> 10;

  /* Early out if subsystem not initialized or precache in progress. */
  if (*(uint8_t *)0x4ead58 == 0)
    return;
  if (cache_files_precache_in_progress())
    return;

  /* Assert if a video is already playing. */
  if (*(int *)0x4ead60 != 0) {
    display_assert("there is already a bink movie being played",
                   "c:\\halo\\SOURCE\\bink\\bink_playback.c", 0x19e, 1);
    system_exit(-1);
  }

  /* Determine memory pool size based on high-res flag.
   * hi_res (bit 0x80): 0x1000000 (16MB), normal: 0x400000 (4MB). */
  hi_res = flags & 0x80;
  pool_size = (-(unsigned int)(hi_res != 0) & 0xc00000) + 0x400000;
  *(uint32_t *)0x4eae2c = pool_size;
  *(uint32_t *)0x4eae28 = 0;

  /* Steal physical memory from the texture cache for bink. */
  pool_base = xbox_texture_cache_steal_memory(pool_size);
  *(uint32_t *)0x4eae24 = (uint32_t)pool_base;

  /* Assert pool size is page-aligned (4KB). */
  {
    unsigned int remainder = pool_size & 0x80000fff;
    int aligned = 0;
    if ((int)remainder < 0)
      aligned = (remainder - 1 | 0xfffff000) == 0xffffffff;
    else
      aligned = remainder == 0;
    if (!aligned) {
      display_assert("0 == (bink_globals.memory_pool_size % CPU_PAGE_SIZE)",
                     "c:\\halo\\SOURCE\\bink\\bink_playback.c", 0x1a8, 1);
      system_exit(-1);
    }
  }

  /* Set memory protection on the pool (read/write). */
  physical_memory_protect(pool_base, pool_size, 2);

  /* If pool allocation failed, log error and bail. */
  if (*(uint32_t *)0x4eae24 == 0) {
    error(0, "### ERROR bink failed to fucking steal some fucking "
             "memory from the fucking texture cache");
    bink_playback_release_texture_cache();
    return;
  }

  /* Set up DirectSound for bink audio. */
  bink_playback_trace("begin BinkOpen");
  ds_handle = bink_get_dsound_handle();
  ds_ok = 0;
  if (ds_handle != 0) {
    bink_playback_trace("begin BinkSoundUseDirectSound");
    if (BinkSoundUseDirectSound((void *)0x231e80, (void *)ds_handle) != 0) {
      ds_ok = 1;
    }
    bink_playback_trace("end BinkSoundUseDirectSound");
    if (ds_ok)
      goto sound_ok;
  }
  error(2, "### ERROR no DirectSound for bink");

sound_ok:
  /* Configure bink memory limit for high-res mode. */
  if (hi_res != 0) {
    BinkSetMemory(0xc00000);
    bink_flags = 0x1000000;
  } else {
    bink_flags = 0;
  }

  /* Open the bink file. */
  bink = (int *)BinkOpen(filename, bink_flags);
  *(int **)0x4ead60 = bink;
  bink_playback_trace("end BinkOpen");

  if (bink != NULL) {
    /* Calculate page-aligned frame buffer size:
     * width * height * 4 bytes per pixel, rounded up to 4KB page. */
    frame_buf_size = (unsigned int)(bink[1] * bink[0] * 4 + 0xfff) & 0xfffff000;

    /* Allocate frame buffer from the bink memory pool.
     * 0x1c5990 takes @eax=alignment(0x80), @ecx=size. */
    {
      int _eax = 0x80;
      int _ecx = (int)frame_buf_size;
      __asm__ __volatile__("movl %[ecx_in], %%ecx\n\t"
                           "call *%[fn]\n\t"
                           : "+a"(_eax), "+c"(_ecx)
                           : [ecx_in] "g"(_ecx), [fn] "r"((void *)0x1c5990)
                           : "edx", "memory", "cc");
      frame_buf = (void *)_eax;
    }

    /* Set memory protection on the frame buffer (read/write/nocache). */
    bink_playback_trace("begin XPhysicalProtect");
    physical_memory_protect(frame_buf, frame_buf_size, 0x404);
    bink_playback_trace("end XPhysicalProtect");

    /* Fill the frame buffer with random data (noise pattern). */
    {
      int i = 0;
      if (0 < (int)frame_buf_size) {
        do {
          int rval = rand();
          *((uint8_t *)frame_buf + i) = (uint8_t)(rval >> 8);
          i++;
        } while (i < (int)frame_buf_size);
      }
    }

    /* Store video dimensions as shorts. */
    *(int16_t *)0x4ead64 = (int16_t)bink[0]; /* width */
    *(int16_t *)0x4ead66 = (int16_t)bink[1]; /* height */

    /* Build the D3D texture header format dword.
     * Encodes width, height in the NV2A texture format. */
    {
      int w = (int)*(int16_t *)0x4ead64;
      int h = (int)*(int16_t *)0x4ead66;
      unsigned int fmt =
        ((((unsigned int)(w * 4) >> 6) - 1) << 12 | (unsigned int)(h - 1))
          << 12 |
        (unsigned int)(w - 1);
      *(uint32_t *)0x4ead50 = fmt;
    }

    /* Set up D3D texture resource descriptor at 0x4ead40. */
    *(uint32_t *)0x4ead6c = 0;
    *(uint32_t *)0x4ead70 = 3; /* D3DFMT_LIN_X8R8G8B8 */
    *(uint32_t *)0x4ead74 = 4; /* bytes per pixel */
    *(uint8_t *)0x4ead68 = 0;
    *(uint32_t *)0x4ead78 = 0x4ead40; /* texture pointer */
    *(uint32_t *)0x4ead40 = 0x40001; /* D3D resource common */
    *(uint32_t *)0x4ead44 = 0;
    *(uint32_t *)0x4ead48 = 0;
    *(uint32_t *)0x4ead4c = 0x11e29; /* texture format */

    /* Register the D3D resource with the GPU. */
    D3DResource_Register((void *)0x4ead40, frame_buf);

    /* Initialize the render quad vertex data. */
    csmemset((void *)0x4ead98, 0, 0x8c);

    /* Set render scale factors to 1.0f. */
    *(uint32_t *)0x4eadd8 = 0x3f800000; /* 1.0f */
    *(uint32_t *)0x4eaddc = 0x3f800000; /* 1.0f */
    *(uint32_t *)0x4eadc0 = 0x3f800000; /* 1.0f */
    *(uint32_t *)0x4eadc4 = 0x3f800000; /* 1.0f */
    *(uint32_t *)0x4ead98 = 0;
    *(uint8_t *)0x4eae22 = 0;
    *(uint16_t *)0x4eae20 = 7;

    /* Set up a bitmap header for the bink frame texture. */
    *(uint32_t *)0x4ead10 = 0x6269746d; /* "bitm" tag */
    *(int16_t *)0x4ead14 = *(int16_t *)0x4ead64; /* width */
    *(int16_t *)0x4ead16 = *(int16_t *)0x4ead66; /* height */
    *(int16_t *)0x4ead18 = 1;
    *(int16_t *)0x4ead1a = 0;
    *(int16_t *)0x4ead1c = 10; /* bitmap format (a8r8g8b8) */
    *(int16_t *)0x4ead1e = 0x10;

    /* Calculate bitmap data size from bits-per-pixel. */
    bpp = bitmap_format_bits_per_pixel(10);
    bits_total =
      (int)bpp * (int)*(int16_t *)0x4ead16 * (int)*(int16_t *)0x4ead14;
    *(int *)0x4ead2c = (int)(bits_total + (bits_total >> 31 & 7)) >> 3;

    /* Set bitmap registration and hardware pointers. */
    *(int *)0x4ead30 = -1;
    *(int *)0x4ead34 = -1;
    *(int *)0x4ead3c = -1;
    *(uint32_t *)0x4ead38 = *(uint32_t *)0x4ead78;
    *(uint32_t *)0x4eada4 = 0x4ead10;

    /* Store flags and optionally suppress event manager. */
    *(uint32_t *)0x4ead5c = flags;
    if ((flags & 4) != 0) {
      bink_playback_trace("begin event_manager");
      event_manager_flush();
      event_manager_suppress(1);
      bink_playback_trace("end event_manager");
    }

    /* Decode and blit the first frame. */
    bink_playback_decode_frame();

    /* Save the pregame loading flag, then clear it and all counters. */
    *(uint8_t *)0x32eba0 = *(uint8_t *)0x31fa96;
    *(uint32_t *)0x4ead7c = 0;
    *(uint32_t *)0x4ead80 = 0;
    *(uint32_t *)0x4ead84 = 0;
    *(uint32_t *)0x4ead88 = 0;
    *(uint32_t *)0x4ead8c = 0;
    *(uint32_t *)0x4ead90 = 0;
    *(uint32_t *)0x4ead94 = 0;
    *(uint8_t *)0x31fa96 = 0;
    return;
  }

  /* BinkOpen failed - log error and release texture cache memory. */
  error(2, "### ERROR failed to open bink file '%s'", filename);
  bink_playback_release_texture_cache();
}

/* Update bink playback state (called once per frame outside rendering). */
void bink_playback_update(void)
{
  if (*(uint8_t *)0x31fa96 != 0)
    bink_playback_check_stop();
}
