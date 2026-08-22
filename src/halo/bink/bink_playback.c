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

/* Returns non-zero if a bink video is currently open and the subsystem
 * is initialized. Used by callers to gate rendering and input. */
int bink_playback_active(void)
{
  if (*(int *)0x4ead60 != 0 && *(uint8_t *)0x4ead58 != 0)
    return 1;
  return 0;
}

/* Returns non-zero if bink is initialized and was started with flag 0x8
 * (suppress-UI mode). Callers use this to skip rendering UI widgets
 * during attract-mode or other fullscreen bink playback. */
int bink_playback_suppress_ui(void)
{
  if (*(uint8_t *)0x4ead58 != 0 && (*(uint8_t *)0x4ead5c & 8) != 0)
    return 1;
  return 0;
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
  if (*(int *)0x4eae30 > 0) {
    do {
      if (*(int *)(0x4eacd0 + (int)i * 4) != 0) {
        empty = false;
      }
      i++;
    } while ((int)i < *(int *)0x4eae30);
  }
  return empty;
}

/* Bink/RAD memory-allocation callback (0x1c5ab0).
 *
 * Registered together with bink_memory_pool_free by bink_playback_initialize
 * (`PUSH 0x1c5ca0; PUSH 0x1c5ab0; CALL 0x231490` at 0x1c61b4).  The `RET 0x4`
 * at 0x1c5c9b makes it __stdcall with a single dword argument, and the block
 * pointer is returned in EAX (`MOV EAX,ESI` at 0x1c5c95) — i.e. the RAD
 * `new_malloc` shape, with bink_memory_pool_free as the matching `new_free`.
 *
 * Bump-allocates `size` bytes at memory_pool_base + memory_pool_offset, marks
 * the block via physical_memory_protect(..., 4), appends the pointer to the
 * 16-entry block table at 0x4eacd0, then advances the offset by a 0x3000-byte
 * gap clamped to the pool size.
 *
 * Global names come verbatim from the assert text pushed at 0x1c5c60:
 * 0x4eae28 = bink_globals.memory_pool_offset, 0x4eae2c =
 * bink_globals.memory_pool_size.  0x4eae24 = bink_globals.memory_pool_base
 * (named by the assert in bink_memory_pool_alloc above).  0x4eae30 is the
 * block-table count; its meaning beyond "index into 0x4eacd0" is unproven. */
void *__stdcall bink_memory_callback_alloc(unsigned int size)
{
  uint32_t mem_status[8];
  unsigned int offset;
  void *ptr;

  /* Inlined bink_playback_trace memory checkpoint (0x1c5ab8-0x1c5ae6). */
  csmemset(mem_status, 0, 0x20);
  mem_status[0] = 0x20;
  xbox_query_global_memory_status(mem_status);
  *(uint32_t *)0x32eb9c = mem_status[3] >> 10;

  if (*(int *)0x4eae30 > 0 && *(int *)0x4eacd0 == 0) {
    if (!bink_memory_pool_is_empty()) {
      display_assert(
        "### FATAL_ERROR bink just confused the hell out of me (1)",
        "c:\\halo\\SOURCE\\bink\\bink_playback.c", 0x2df, 1);
      system_exit(-1);
    }
    if (bink_memory_pool_is_empty()) {
      *(int *)0x4eae30 = 0;
      *(uint32_t *)0x4eae28 = 0;
    }
  }

  offset = *(unsigned int *)0x4eae28;

  /* CMP/JA at 0x1c5b4a is unsigned; the block-count and base tests at
   * 0x1c5b4e/0x1c5b5d are signed and pointer-null respectively. */
  if (offset + size > *(unsigned int *)0x4eae2c || *(int *)0x4eae30 >= 0x10 ||
      *(uint32_t *)0x4eae24 == 0) {
    display_assert("!\"bink memory allocation should not fail\"",
                   "c:\\halo\\SOURCE\\bink\\bink_playback.c", 0x2f7, 1);
    system_exit(-1);
  }

  ptr = (void *)(*(uint32_t *)0x4eae24 + offset);
  *(unsigned int *)0x4eae28 = offset + size;
  physical_memory_protect(ptr, size, 4);

  if (*(int *)0x4eae28 > *(int *)0x4eae2c) {
    display_assert(
      csprintf((char *)0x5ab100,
               "### FATAL_ERROR bink needs more memory (requested %d bytes "
               "over the %d-byte limit)",
               *(int *)0x4eae28 - *(int *)0x4eae2c, *(int *)0x4eae2c),
      "c:\\halo\\SOURCE\\bink\\bink_playback.c", 0x312, 1);
    system_exit(-1);
  }

  if (*(int *)0x4eae30 >= 0x10) {
    display_assert("### FATAL_ERROR bink needs more pointer blocks",
                   "c:\\halo\\SOURCE\\bink\\bink_playback.c", 0x313, 1);
    system_exit(-1);
  }

  *(uint32_t *)(*(int *)0x4eae30 * 4 + 0x4eacd0) = (uint32_t)ptr;
  *(int *)0x4eae30 = *(int *)0x4eae30 + 1;

  /* Inlined bink_playback_trace memory checkpoint (0x1c5c14-0x1c5c4c). */
  csmemset(mem_status, 0, 0x20);
  mem_status[0] = 0x20;
  xbox_query_global_memory_status(mem_status);
  *(uint32_t *)0x32eb9c = mem_status[3] >> 10;

  if (*(int *)0x4eae28 >= *(int *)0x4eae2c) {
    display_assert(
      "bink_globals.memory_pool_offset < bink_globals.memory_pool_size",
      "c:\\halo\\SOURCE\\bink\\bink_playback.c", 0x31f, 1);
    system_exit(-1);
  }

  *(int *)0x4eae28 = *(int *)0x4eae28 + 0x3000;
  if (*(int *)0x4eae28 > *(int *)0x4eae2c)
    *(int *)0x4eae28 = *(int *)0x4eae2c;

  return ptr;
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
  char text_buf[4096];
  char summary_buf[40];
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
    int16_t frame_info[20];
    float scale;
    unsigned int *fi;
    unsigned int total_time;

    frame_info[0] = 0xfa; /* max frame count for averaging */
    frame_info[1] = 0;
    frame_info[2] = 0;
    frame_info[3] = 0;
    frame_info[4] = 0;
    frame_info[5] = 0;
    frame_info[9] = 0;
    frame_info[10] = 0;

    BinkGetFrameBuffersInfo((void *)bink_handle, frame_info, 0);

    fi = (unsigned int *)frame_info;
    total_time = fi[8];
    scale = 1.0f / (float)total_time;

    /* Format timing stats into text buffer. */
    crt_sprintf(
      text_buf,
      "FramesTime=|t%.02f|nFrameVideoDecompTime=|t%.02f|n"
      "FrameAudioDecompTime=|t%.02f|nFrameReadTime=|t%.02f|n"
      "FrameIdleReadTime=|t%.02f|nFrameThreadReadTime=|t%.02f|n"
      "FramesBlitTime=|t%.02f|n|nFrames=|t%d",
      (double)((float)fi[7] * scale),
      (double)((float)fi[6] * scale),
      (double)((float)fi[5] * scale),
      (double)((float)fi[4] * scale),
      (double)((float)fi[3] * scale),
      (double)((float)fi[2] * scale),
      (double)((float)fi[1] * scale),
      total_time);

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
    interface_draw_text(1, -1, 0, 0, 5, 0);
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
  bool pressed;
  short pad;
  short btn;
  char *state;

  pressed = false;
  pad = 0;
  do {
    if (pressed) {
      break;
    }
    state = (char *)input_get_gamepad_state(pad);
    if (state != NULL) {
      if (state[0x1c] == 1 || state[0x1d] == 1) {
        pressed = true;
      } else {
        btn = 0;
        do {
          if (state[btn + 0x10] == 1) {
            pressed = true;
            break;
          }
          btn++;
        } while (btn < 8);
      }
    }
    pad++;
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

/* Parse the COMM chunk of an AIFF/AIFC container (0x1c6900).
 *
 * Walks the chunk list from file offset 0xc in 8-byte {id, size} steps,
 * byte-swapping each header via the "aiff_chunk" definition at 0x32ebe4,
 * skipping payloads (odd sizes rounded up — AIFF word padding) until 'COMM'
 * is found.  Reads the 22-byte COMM payload, byte-swaps it via the
 * "aiff_format_info_p1" definition at 0x32ec18, then matches the 10-byte
 * 80-bit extended sampleRate against the three rates the engine supports.
 *
 * `format_out` receives the sample rate as a dword at +0, numChannels as a
 * word at +4 and sampleSize as a word at +8 (0x1c6a37-0x1c6a4c); the same
 * three slots are written by the RIFF/WAVE counterpart at 0x1c6d90
 * (0x1c6e8d-0x1c6e9e), which is what pins the layout.  An unrecognised rate
 * stores -1 and fails.  Success additionally requires either an 18-byte COMM
 * (plain AIFF, no compressionType field) or a compressionType of 'NONE'.
 *
 * TU note: the assert in FUN_001c6c00 below names this cluster's source file
 * as "c:\halo\SOURCE\sound\sound_import\sound_import.c", not bink_playback.c;
 * it lives here only because FUN_001c6880 already did. */
bool FUN_001c6900(file_ref_t *file, void *format_out)
{
  bool result;
  uint32_t chunk[2]; /* [0] = chunk id, [1] = chunk size */
  char comm[22];
  int offset;
  unsigned int skip;

  result = false;
  offset = 0xc;

  if (file_open(file, 1)) {
    if (file_read_from_position(file, 0xc, 8, chunk)) {
      for (;;) {
        FUN_00118be0((void *)0x32ebe4, chunk, 1);
        if (chunk[0] == 0x434f4d4d) /* 'COMM' */
          break;
        skip = chunk[1];
        if ((chunk[1] & 1) != 0)
          skip = chunk[1] + 1;
        offset = offset + 8 + (int)skip;
        if (!file_read_from_position(file, offset, 8, chunk)) {
          file_close(file);
          return false;
        }
      }

      if (file_read_from_position(file, offset + 8, 0x16, comm)) {
        /* 80-bit IEEE-754 extended encodings of 11025/22050/44100 Hz, stored
         * byte-by-byte at 0x1c69ae-0x1c6a19 (EBP-0x18/-0x24/-0x30). */
        unsigned char rate_11025[10];
        unsigned char rate_22050[10];
        unsigned char rate_44100[10];

        rate_11025[0] = 0x40; rate_11025[1] = 0x0c; rate_11025[2] = 0xac; rate_11025[3] = 0x44;
        rate_11025[4] = 0; rate_11025[5] = 0; rate_11025[6] = 0; rate_11025[7] = 0;
        rate_11025[8] = 0; rate_11025[9] = 0;

        rate_22050[0] = 0x40; rate_22050[1] = 0x0d; rate_22050[2] = 0xac; rate_22050[3] = 0x44;
        rate_22050[4] = 0; rate_22050[5] = 0; rate_22050[6] = 0; rate_22050[7] = 0;
        rate_22050[8] = 0; rate_22050[9] = 0;

        rate_44100[0] = 0x40; rate_44100[1] = 0x0e; rate_44100[2] = 0xac; rate_44100[3] = 0x44;
        rate_44100[4] = 0; rate_44100[5] = 0; rate_44100[6] = 0; rate_44100[7] = 0;
        rate_44100[8] = 0; rate_44100[9] = 0;

        FUN_00118be0((void *)0x32ec18, comm, 1);

        if (csmemcmp(rate_11025, &comm[8], 10) == 0) {
          *(uint32_t *)format_out = 0x2b11; /* 11025 */
        } else if (csmemcmp(rate_22050, &comm[8], 10) == 0) {
          *(uint32_t *)format_out = 0x5622; /* 22050 */
          *(uint16_t *)((char *)format_out + 8) = *(uint16_t *)&comm[6];
          *(uint16_t *)((char *)format_out + 4) = *(uint16_t *)&comm[0];
          if (chunk[1] == 0x12 || *(uint32_t *)&comm[18] == 0x4e4f4e45) {
            file_close(file);
            return true;
          }
          file_close(file);
          return false;
        } else if (csmemcmp(rate_44100, &comm[8], 10) == 0) {
          *(uint32_t *)format_out = 0xac44; /* 44100 */
        } else {
          *(uint32_t *)format_out = 0xffffffff;
          file_close(file);
          return false;
        }

        *(uint16_t *)((char *)format_out + 8) = *(uint16_t *)&comm[6];
        *(uint16_t *)((char *)format_out + 4) = *(uint16_t *)&comm[0];

        if (chunk[1] == 0x12 || *(uint32_t *)&comm[18] == 0x4e4f4e45)
          result = true;
      }
    }
    file_close(file);
  }
  return result;
}

/* Read the sample payload out of an AIFF/AIFC 'SSND' chunk (0x1c6b20).
 *
 * Same chunk walk as FUN_001c6900 but looking for 'SSND'.  The payload read
 * starts 0x10 bytes past the chunk header start (8-byte header plus SSND's
 * own offset/blockSize dwords) and is chunk_size - 8 bytes long; that length
 * is also stored through `size_out`.  On success the samples are byte-swapped
 * in place as 16-bit elements via FUN_00118620(buffer, *size_out >> 1, -2) —
 * SAR at 0x1c6bd6 makes the shift arithmetic, hence the signed `int *`.
 *
 * Mirrors the RIFF/WAVE 'data' reader FUN_001c6ed0 in game_sound.c. */
bool FUN_001c6b20(file_ref_t *file, int *size_out, void *buffer)
{
  bool result;
  uint32_t chunk[2]; /* [0] = chunk id, [1] = chunk size */
  int offset;
  int data_size;
  unsigned int skip;

  result = false;
  offset = 0xc;

  if (file_open(file, 1)) {
    if (file_read_from_position(file, offset, 8, chunk)) {
      for (;;) {
        FUN_00118be0((void *)0x32ebe4, chunk, 1);
        if (chunk[0] == 0x53534e44) { /* 'SSND' */
          data_size = (int)chunk[1] - 8;
          offset += 0x10;
          *size_out = data_size;
          if (file_read_from_position(file, offset, data_size, buffer))
            result = true;
          break;
        }
        skip = chunk[1];
        if ((skip & 1) != 0)
          skip = skip + 1;
        offset += (int)skip + 8;
        if (!file_read_from_position(file, offset, 8, chunk))
          break;
      }
    }
    file_close(file);
    if (result)
      FUN_00118620(buffer, *size_out >> 1, -2);
  }
  return result;
}

/* Post-parse hook for the AIFF branch of the sound-file import dispatch
 * (0x1c6bf0).  The whole function body is a single `RET` (byte C3 at
 * 0x1c6bf0, NOP padding to 0x1c6c00) — it is genuinely empty in the binary,
 * unlike its RIFF/WAVE counterpart FUN_001c6fb0 which is a real function.
 * FUN_001c6ca0 (game_sound.c) calls it with three stack arguments and cleans
 * up with ADD ESP,0xc at 0x1c6cd6, so the cdecl 3-arg shape is caller-side
 * evidence only; nothing here reads the arguments. */
void FUN_001c6bf0(void *param_1, void *param_2, void *param_3)
{
}

/* Sniff a sound file's container and fill in its format description
 * (0x1c6c00).  Tries AIFF/AIFC first (FUN_001c6880 sniff + FUN_001c6900 COMM
 * parse), then RIFF/WAVE (FUN_001c6d20 sniff + FUN_001c6d90 'fmt ' parse).
 *
 * The two asserts name the parameters verbatim — "info" (line 0x12) and
 * "file" (line 0x13) — and name the translation unit
 * "c:\halo\SOURCE\sound\sound_import\sound_import.c".  `info` is [EBP+0x8]
 * and is forwarded as the format-out argument; `file` is [EBP+0xc] and is
 * the file reference. */
bool FUN_001c6c00(void *info, file_ref_t *file)
{
  if (info == NULL) {
    display_assert(
      "info", "c:\\halo\\SOURCE\\sound\\sound_import\\sound_import.c", 0x12, 1);
    system_exit(-1);
  }
  if (file == NULL) {
    display_assert(
      "file", "c:\\halo\\SOURCE\\sound\\sound_import\\sound_import.c", 0x13, 1);
    system_exit(-1);
  }

  if (FUN_001c6880(file) && FUN_001c6900(file, info))
    goto valid;
  if (FUN_001c6d20(file) && FUN_001c6d90(file, info))
    goto valid;
  return false;

valid:
  return true;
}
