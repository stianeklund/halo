/* Internal timing/memory checkpoint. Queries memory stats and stores
 * available kilobytes to a global. The string parameter is a debug
 * label unused in retail. */
void bink_playback_trace(const char *msg)
{
  uint32_t mem_status[8];
  csmemset(mem_status, 0, 0x20);
  mem_status[0] = 0x20;
  xbox_query_global_memory_status(mem_status);
  *(uint32_t *)0x32eb9c = mem_status[3] >> 10;
}

/* Bink video playback system for cinematics and loading screens. */

/* Returns true if a bink video is currently open and the subsystem
 * is initialized. Used by callers to gate rendering and input. */
bool bink_playback_active(void)
{
  if (*(int *)0x4ead60 != 0 && *(uint8_t *)0x4ead58 != 0)
    return true;
  return false;
}

/* Returns true if bink is initialized and was started with flag 0x8
 * (suppress-UI mode). Callers use this to skip rendering UI widgets
 * during attract-mode or other fullscreen bink playback. */
bool bink_playback_suppress_ui(void)
{
  if (*(uint8_t *)0x4ead58 != 0 && (*(uint8_t *)0x4ead5c & 8) != 0)
    return true;
  return false;
}

/* Returns true if a bink video handle is open (regardless of whether
 * the subsystem is initialized). */
bool bink_playback_has_video(void)
{
  return *(int *)0x4ead60 != 0;
}

/* Bump-allocate from the top of the bink memory pool with alignment.
 * Computes a candidate pointer at (pool_base + pool_remaining - size),
 * aligns it down if needed, then decrements the remaining pool size.
 * alignment is passed in EAX, alloc_size in ECX (register args). */
void *bink_memory_pool_alloc(int alignment /* @<eax> */,
                             int alloc_size /* @<ecx> */)
{
  unsigned int ptr;
  unsigned int align = (unsigned int)alignment;
  unsigned int size = (unsigned int)alloc_size;

  ptr = (*(unsigned int *)0x4eae2c - size) + *(unsigned int *)0x4eae24;

  if (align != 0 && (ptr & (align - 1)) != 0) {
    if (align == 0 || (align & (align - 1)) != 0) {
      display_assert("alignment_in_bytes>0 && "
                     "(alignment_in_bytes&(alignment_in_bytes-1))==0",
                     "c:\\halo\\SOURCE\\bink\\bink_playback.c", 0x2b3, 1);
      system_exit(-1);
    }
    {
      unsigned int diff = align - ptr;
      unsigned int padding = diff & (align - 1);
      size += padding;
      ptr -= (align - 1) & diff;
    }
  }

  if (size == 0) {
    display_assert("size_in_bytes>0", "c:\\halo\\SOURCE\\bink\\bink_playback.c",
                   0x2b9, 1);
    system_exit(-1);
  }
  if (*(unsigned int *)0x4eae2c < size) {
    display_assert("bink_globals.memory_pool_size>=size_in_bytes",
                   "c:\\halo\\SOURCE\\bink\\bink_playback.c", 0x2ba, 1);
    system_exit(-1);
  }
  if (*(unsigned int *)0x4eae24 == 0) {
    display_assert("bink_globals.memory_pool_base",
                   "c:\\halo\\SOURCE\\bink\\bink_playback.c", 699, 1);
    system_exit(-1);
  }

  *(unsigned int *)0x4eae2c = *(unsigned int *)0x4eae2c - size;
  return (void *)ptr;
}

/* Returns true if all entries in the bink memory pool allocation table
 * are zero (i.e. no outstanding allocations). The pool is an array of
 * dwords at 0x4eacd0 with a count at 0x4eae30. Used by the texture
 * cache release path to assert that all bink allocations were freed. */
bool bink_memory_pool_is_empty(void)
{
  short i;
  bool empty;

  i = 0;
  empty = true;
  if (0 < *(int *)0x4eae30) {
    int idx = 0;
    do {
      if (*(int *)(idx * 4 + 0x4eacd0) != 0) {
        empty = false;
      }
      i = i + 1;
      idx = (int)i;
    } while (idx < *(int *)0x4eae30);
  }
  return empty;
}

/* Release a bink memory pool allocation. Searches the allocation table at
 * 0x4eacd0 (up to 0x4eae30 entries) for a pointer matching ptr, and zeroes
 * that slot when found. If the pointer is not found (or the pool is empty),
 * calls display_assert and exits — "bink just confused the hell out of me (2)".
 * Bracketed by bink_playback_trace calls (memory checkpoint) before the
 * search and after the successful free. Calling convention: __stdcall (RET 4).
 */
void __stdcall bink_memory_pool_free(int ptr)
{
  uint32_t mem_status[8];
  int count;
  int i;

  csmemset(mem_status, 0, 0x20);
  mem_status[0] = 0x20;
  xbox_query_global_memory_status(mem_status);
  *(uint32_t *)0x32eb9c = mem_status[3] >> 10;

  count = *(int *)0x4eae30;
  i = 0;
  if (0 < count) {
    do {
      if (*(int *)(i * 4 + 0x4eacd0) == ptr) {
        *(int *)(i * 4 + 0x4eacd0) = 0;
        if (i < count) {
          goto found;
        }
        break;
      }
      i = i + 1;
    } while (i < count);
  }
  display_assert("### FATAL_ERROR bink just confused the hell out of me (2)",
                 "c:\\halo\\SOURCE\\bink\\bink_playback.c", 0x339, 1);
  system_exit(-1);

found:
  csmemset(mem_status, 0, 0x20);
  mem_status[0] = 0x20;
  xbox_query_global_memory_status(mem_status);
  *(uint32_t *)0x32eb9c = mem_status[3] >> 10;
}

/* Render the bink frame quad on screen with optional debug overlay.
 *
 * Builds a 4-vertex textured quad from the current display bounds and
 * video dimensions, centered on screen unless flag bit 0x10 is set
 * (fullscreen stretch). Calls the sprite renderer to draw the quad.
 *
 * If the debug flag at 0x4ead54 is set (never in retail), draws
 * a text overlay with Bink frame timing statistics. */
void bink_playback_render_frame(void)
{
  char text_buf[1024];
  char summary_buf[256];
  short screen_pos[4];
  float vertices[20]; /* 4 verts x 5 floats (x, y, u, v, color) */
  short top_y, left_x, right_x, bottom_y;
  short sVar9;
  int counter;
  int i;
  short display_top, display_left, display_bottom, display_right;
  int horiz_span, vert_span;
  short video_w, video_h;

  display_top = *(int16_t *)0x325654;
  display_left = *(int16_t *)0x325656;
  display_bottom = *(int16_t *)0x325658;
  display_right = *(int16_t *)0x32565a;

  if ((*(uint32_t *)0x4ead5c & 0x10) != 0) {
    /* Fullscreen stretch: use display bounds directly. */
    top_y = display_top;
    left_x = display_left;
    bottom_y = display_bottom;
    right_x = display_right;
  } else {
    /* Center the video in the display area. */
    horiz_span = (int)(display_right - display_left);
    vert_span = (int)(display_bottom - display_top);
    video_w = *(int16_t *)0x4ead64;
    video_h = *(int16_t *)0x4ead66;
    left_x = (short)((horiz_span - (int)video_w) / 2);
    right_x = (short)((horiz_span + (int)video_w) / 2);
    top_y = (short)((vert_span - (int)video_h) / 2);
    bottom_y = (short)((vert_span + (int)video_h) / 2);
  }

  sVar9 = 0;
  counter = 1;
  i = 0;
  do {
    float u_val, v_val, x_val, y_val;

    /* u coordinate: 0 for left side, video_width for right side. */
    if ((counter & 2) != 0) {
      u_val = (float)(int)*(int16_t *)0x4ead64;
    } else {
      u_val = 0.0f;
    }

    /* v coordinate: 0 for top, video_height for bottom. */
    if (sVar9 > 1) {
      v_val = (float)(int)*(int16_t *)0x4ead66;
    } else {
      v_val = 0.0f;
    }

    /* x position. */
    if ((counter & 2) != 0) {
      x_val = (float)(int)right_x;
    } else {
      x_val = (float)(int)left_x;
    }

    /* y position. */
    if (sVar9 < 2) {
      y_val = (float)(int)top_y;
    } else {
      y_val = (float)(int)bottom_y;
    }

    vertices[i * 5 + 0] = x_val;
    vertices[i * 5 + 1] = y_val;
    vertices[i * 5 + 2] = u_val;
    vertices[i * 5 + 3] = v_val;
    *(uint32_t *)&vertices[i * 5 + 4] = 0xffffffff; /* white, full alpha */

    sVar9 = sVar9 + 1;
    counter = counter + 1;
    i++;
  } while (sVar9 < 4);

  rasterizer_sprites_render((void *)0x4ead98, vertices);

  if (*(uint8_t *)0x4ead54 != 0) {
    /* Debug overlay: gather Bink timing stats and draw on screen. */
    int bink_handle = *(int *)0x4ead60;
    int16_t frame_info[14];
    float scale;

    frame_info[0] = 0xfa; /* max frame count for averaging */
    frame_info[1] = 0;
    frame_info[2] = 0;
    frame_info[3] = 0;
    frame_info[4] = 0;
    frame_info[5] = 0;
    frame_info[9] = 0;
    frame_info[10] = 0;

    BinkGetFrameBuffersInfo((void *)bink_handle, frame_info, 0);

    /* Compute scale factor: 1.0 / total_time. The decompiler shows
     * complex unsigned-to-float conversions for each timing field,
     * then multiplication by scale. Faithfully reproduce this. */
    {
      float total_f = (float)(int)frame_info[8]; /* FIXME: uint32 at offset */
      /* The original reads dword-sized fields from the frame_info struct
       * at offsets that alias over the short array. These are actually
       * dword timing fields from BinkGetFrameBuffersInfo. Use hardcoded
       * pointer arithmetic to match the original exactly. */
      int *fi = (int *)frame_info;
      int total_time = fi[8]; /* offset 0x10 from frame_info base */

      if (total_time < 0)
        total_f = total_f + *(float *)0x25fb8c;
      scale = 1.0f / total_f;

      /* Format timing stats into text buffer. */
      crt_sprintf(
        text_buf,
        "FramesTime=|t%.02f|nFrameVideoDecompTime=|t%.02f|n"
        "FrameAudioDecompTime=|t%.02f|nFrameReadTime=|t%.02f|n"
        "FrameIdleReadTime=|t%.02f|nFrameThreadReadTime=|t%.02f|n"
        "FramesBlitTime=|t%.02f|n|nFrames=|t%d",
        (double)((float)fi[7] < 0 ? (float)fi[7] + *(float *)0x25fb8c :
                                    (float)fi[7]) *
          scale,
        (double)((float)fi[6] < 0 ? (float)fi[6] + *(float *)0x25fb8c :
                                    (float)fi[6]) *
          scale,
        (double)((float)fi[5] < 0 ? (float)fi[5] + *(float *)0x25fb8c :
                                    (float)fi[5]) *
          scale,
        (double)((float)fi[4] < 0 ? (float)fi[4] + *(float *)0x25fb8c :
                                    (float)fi[4]) *
          scale,
        (double)((float)fi[3] < 0 ? (float)fi[3] + *(float *)0x25fb8c :
                                    (float)fi[3]) *
          scale,
        (double)((float)fi[2] < 0 ? (float)fi[2] + *(float *)0x25fb8c :
                                    (float)fi[2]) *
          scale,
        (double)((float)fi[1] < 0 ? (float)fi[1] + *(float *)0x25fb8c :
                                    (float)fi[1]) *
          scale,
        total_time);
    }

    /* Offset screen position by display origin. */
    {
      short dx = *(int16_t *)0x32565c;
      short dy = *(int16_t *)0x325660;
      screen_pos[0] = dx;
      screen_pos[1] = dy;
      screen_pos[2] = dx;
      screen_pos[3] = dy;
    }
    rect2d_offset(screen_pos, 0, 0x20);

    /* Set up text rendering. */
    interface_draw_text(1, 5, 0, 0, -1, 1);
    draw_string_set_color(*(const void **)0x2ee6d4);
    draw_string_set_tab_stops(&frame_info[0], 1);
    rasterizer_text_draw(screen_pos, NULL, (void *)&frame_info[8], -4,
                         text_buf);

    /* Check if enough frames have passed to update stats. */
    if (*(int *)0x4ead88 - *(int *)0x4ead84 > 0x1c) {
      BinkGetSummary((void *)*(int *)0x4ead60, summary_buf);

      *(int *)0x4ead8c = *(int *)(summary_buf + 0x2c) - *(int *)0x4ead7c;
      *(int *)0x4ead90 = *(int *)(summary_buf + 0x30) - *(int *)0x4ead80;
      *(int *)0x4ead94 = *(int *)0x4ead88 - *(int *)0x4ead84;
      *(int *)0x4ead7c = *(int *)(summary_buf + 0x2c);
      *(int *)0x4ead80 = *(int *)(summary_buf + 0x30);
      *(int *)0x4ead84 = *(int *)0x4ead88;
    }

    /* Draw skipped frames / blits stats. */
    {
      int skipped_frames = *(int *)0x4ead8c;
      int skipped_blits = *(int *)0x4ead90;
      int frame_count = *(int *)0x4ead94;

      /* Advance screen position using the cursor y-offset written by the
       * previous draw (high word of the dword stored at frame_info[8]). */
      screen_pos[0] = (int16_t)(frame_info[9] + 0x1f);

      crt_sprintf(text_buf, "SkippedFrames=|t%d (%d)|nSkippedBlits=|t%d|n",
                  skipped_frames, frame_count, skipped_blits);
      draw_string_set_color(*(const void **)0x2ee6d0);
      rasterizer_text_draw(screen_pos, NULL, (void *)&frame_info[8], -4,
                           text_buf);
    }
  }
}

/* Poll all 4 gamepad slots and return true if any digital button or
 * either analog trigger is in the "just pressed" state (value == 1).
 * Used to detect a skip request during bink video playback.
 * Checks trigger bytes at offsets 0x1c and 0x1d, and 8 digital
 * button bytes starting at offset 0x10 in the gamepad state struct. */
bool bink_playback_check_any_button(void)
{
  short pad;
  bool pressed;

  pressed = false;
  pad = 0;
  do {
    if (pressed) {
      return pressed;
    }
    {
      void *state = input_get_gamepad_state((int)pad);
      if (state != NULL) {
        if (*(char *)((int)state + 0x1c) == 1 ||
            *(char *)((int)state + 0x1d) == 1) {
          pressed = true;
        } else {
          short btn = 0;
          do {
            if (*(char *)((int)btn + 0x10 + (int)state) == 1) {
              pressed = true;
              break;
            }
            btn = btn + 1;
          } while (btn < 8);
        }
      }
    }
    pad = pad + 1;
  } while (pad < 4);
  return pressed;
}

/* Bink texture lock adapter. Reorders arguments from the original
 * __fastcall register layout (flags@EAX, rect@ECX, locked_rect@EDX,
 * texture+level on stack) into the standard D3DTexture_LockRect
 * cdecl call. Returns 0. Used as a Bink SDK callback. */
int FUN_001c6170(unsigned int flags, void *rect, void *locked_rect,
                 void *texture, unsigned int level)
{
  D3DTexture_LockRect(texture, level, locked_rect, rect, flags);
  return 0;
}

/* Initialize the bink playback globals and register callbacks. */
void bink_playback_initialize(void)
{
  csmemset((void *)0x4ead58, 0, 0xd8);
  BinkSetSoundSystem((void *)0x1c5ab0, (void *)0x1c5ca0);
  *(uint8_t *)0x4ead58 = 1;
}

/* Release texture cache memory stolen for bink playback. Asserts that
 * all bink pool allocations have been freed before returning memory
 * back to the texture cache. Clears pool base, texture pointer, and
 * pool size globals. */
void bink_playback_release_texture_cache(void)
{
  if (*(uint8_t *)0x4ead58 == 0)
    return;
  if (*(int *)0x4eae24 == 0)
    return;

  if (!bink_memory_pool_is_empty()) {
    display_assert(
      "we released the texture cache but we still had memory allocated",
      "c:\\halo\\SOURCE\\bink\\bink_playback.c", 0x299, 1);
    system_exit(-1);
  }
  xbox_texture_cache_return_memory();
  *(uint32_t *)0x4eae24 = 0;
  *(uint32_t *)0x4ead78 = 0;
  *(uint32_t *)0x4eae2c = 0;
}

/* Decode the current bink frame and blit it to the D3D texture.
 * Calls BinkDoFrame + BinkNextFrame to advance decoding, then locks
 * the texture surface and copies the decoded frame data into it
 * via BinkCopyToBuffer. Temporarily sets the D3D status word to 6
 * during the lock/copy to signal the GPU. */
void bink_playback_decode_frame(void)
{
  int locked_rect[2]; /* [0]=Pitch, [1]=pBits */

  BinkDoFrame((void *)*(int *)0x4ead60);
  BinkNextFrame((void *)*(int *)0x4ead60);
  *(uint16_t *)0x325652 = 6;
  D3DTexture_LockRect((void *)*(int *)0x4ead78, 0, locked_rect, 0, 0);
  BinkCopyToBuffer((void *)*(int *)0x4ead60, (void *)locked_rect[1],
                   locked_rect[0], *(int *)(*(int *)0x4ead60 + 4), 0, 0,
                   *(uint32_t *)0x4ead70 | 0x80000000);
  *(uint16_t *)0x325652 = 0;
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

/* Check whether the video should stop (end of file, user skip, etc.)
 * and call bink_playback_stop if so.
 *
 * When the pregame loading flag is set, BinkWait is called once and
 * needs_decode is set based on the result. When loading is not active,
 * BinkWait is called in a busy loop until the frame is ready.
 *
 * The video stops if:
 *  - The user pressed a button (flag bit 2 = skippable), unless
 *    flag bit 0x40 is set and ui widget initialization is still running.
 *  - The video reached the last frame and flag bit 1 (loop) is not set. */
void bink_playback_check_stop(void)
{
  int wait_result;
  uint8_t flags;

  if (*(uint8_t *)0x4ead58 == 0)
    return;
  if (*(int *)0x4ead60 == 0)
    return;

  if (*(uint8_t *)0x31fa96 != 0) {
    /* Pregame loading active: single BinkWait call. */
    wait_result = BinkWait((void *)*(int *)0x4ead60);
    *(uint8_t *)0x4ead59 = (uint8_t)(wait_result == 0);
  } else {
    /* Not loading: busy-wait until frame is ready. */
    do {
      wait_result = BinkWait((void *)*(int *)0x4ead60);
    } while (wait_result != 0);
    *(uint8_t *)0x4ead59 = 1;
  }

  flags = *(uint8_t *)0x4ead5c;

  /* Check if user wants to skip the video. */
  if ((flags & 2) != 0) {
    if ((flags & 0x40) == 0 || !ui_widget_initialization_in_progress()) {
      if (bink_playback_check_any_button()) {
        bink_playback_stop();
        return;
      }
    }
    flags = *(uint8_t *)0x4ead5c;
  }

  /* Check if video has ended. */
  if (*(int *)0x4ead60 == 0 || *(int *)(*(int *)0x4ead60 + 0xc) ==
                                 *(int *)(*(int *)0x4ead60 + 0x8) - 1) {
    if ((flags & 1) == 0) {
      bink_playback_stop();
      return;
    }
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

    /* Allocate frame buffer from the bink memory pool. */
    frame_buf = bink_memory_pool_alloc(0x80, (int)frame_buf_size);

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

/* Check if a file is an AIFF or AIFC audio container.
 * Opens the file, reads the first 12-byte AIFF header chunk (FORM + size +
 * type), byte-swaps it via the aiff_container_chunk definition, then checks
 * that the chunk ID is 0x464f524d ('FORM') and the file type is either
 * 0x41494646 ('AIFF') or 0x41494643 ('AIFC'). Closes the file before
 * returning. Returns true if the file is a valid AIFF/AIFC container. */
bool FUN_001c6880(file_ref_t *info)
{
  int header[3];
  char result;
  char ok;

  result = 0;
  ok = file_open(info, 1);
  if (ok != '\0') {
    ok = file_read_from_position(info, 0, 0xc, header);
    if (ok != '\0') {
      FUN_00118be0((void *)0x32ebbc, header, 1);
      if ((header[0] == 0x464f524d) &&
          ((header[2] == 0x41494646) || (header[2] == 0x41494643))) {
        result = 1;
      }
    }
    file_close(info);
  }
  return result;
}
