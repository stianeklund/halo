/*
 * rasterizer_xbox_decals.c
 *
 * Rasterizer decal subsystem: D3D vertex buffer allocation, LRUV vertex
 * cache lifecycle, and per-map init/dispose.
 *
 * Source path (from binary):
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_decals.c
 *
 * Globals (used by address, not in kb.json):
 *   0x476ab0  void *  – global_d3d_device (IDirect3DDevice8 pointer)
 *   0x476ad8  void *  – local_d3d_vertex_buffer (12-byte D3D VB struct)
 *   0x476adc  void *  – local_vertex_cache (lruv_cache handle)
 *   0x476ae0  bool    – locked-decal eviction warning flag (one-shot)
 *   0x476ae1  bool    – permanent-decal eviction warning flag (one-shot)
 *   0x5aa8b8  void *  – global_decal_data (decal data array pointer)
 *   0x32516c  int     – most-recently-queried decal index (debug display)
 */

/* Forward declarations for callbacks passed to lruv_cache_new.
 * FUN_0015afa0 is the eviction callback (ported at its original address);
 * the query callback remains a static helper. */
void FUN_0015afa0(int decal_index);
static int rasterizer_decals_vertex_cache_query(int decal_index);

/* D3DSURFACE_DESC mirror (Xbox D3D8, 0x1c bytes; desc buffer at EBP-0x30). */
typedef struct {
  unsigned int Format; /* +0x00 */
  unsigned int Type; /* +0x04 */
  unsigned int Usage; /* +0x08 */
  unsigned int Size; /* +0x0c  ([EBP-0x24]) */
  unsigned int MultiSampleType; /* +0x10 */
  unsigned int Width; /* +0x14  ([EBP-0x1c]) */
  unsigned int Height; /* +0x18  ([EBP-0x18]) */
} d3d_surface_desc_t;

/* D3DLOCKED_RECT mirror (Pitch first, then pBits — 8 bytes at EBP-0x14). */
typedef struct {
  int Pitch; /* +0x00  ([EBP-0x14]) */
  void *pBits; /* +0x04  ([EBP-0x10]) */
} d3d_locked_rect_t;

/* 0x157e40
 *
 * rasterizer_present
 *
 * Presents the current back buffer to the display.  When a non-NULL
 * screenshot_bitmap with pixel data (bitmap+0x2c != 0) is supplied, the
 * back-buffer surface is locked and blitted into the bitmap before Present.
 *
 * The capture rectangle comes from two packed-short DWORD globals
 * (left/top/right/bottom at 0x325654/56/58/5a); the optional 2-element
 * `point` (short[2]) rescales the base corner within the rect span.
 *
 * NOTE the blit is rotated: the copy loop iterates right-left times (BX)
 * while each row copies Pitch = bpp*(bottom-top)/8 bytes, and
 * bitmap_2d_address is called with x = top-chain value ([EBP-0xa]) and
 * y = row + left-chain value ([EBP-0xc]) — verified against disassembly
 * (0x157fea-0x158024); the Ghidra draft re-homes these slots.
 *
 * Globals (hardcoded, not in kb.json):
 *   0x476ab0  void *   – global_d3d_device
 *   0x325654  short[2] – capture rect left|top (packed dword)
 *   0x325658  short[2] – capture rect right|bottom (packed dword)
 *   0x505728  void *   – window_globals.hWndPresentTarget
 *   0x325668  uint64   – 64-bit frame/present counter (ADD/ADC pair)
 */
void rasterizer_present(void *screenshot_bitmap, short *point)
{
  const char *msg;
  bool ok;
  short rect_x0; /* lo(0x325654) = left chain   ([EBP-0xc], Ghidra local_10) */
  short rect_y0; /* hi(0x325654) = top chain    ([EBP-0xa], Ghidra sStack_e) */
  short rect_x1; /* lo(0x325658) = right chain  ([EBP-0x8], Ghidra local_c) */
  short rect_y1; /* hi(0x325658) = bottom chain ([EBP-0x6], Ghidra sStack_a) */

  if (*(void **)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x699, 1);
    system_exit(-1);
  }

  ok = true;

  if (screenshot_bitmap != 0 &&
      *(int *)((char *)screenshot_bitmap + 0x2c) != 0) {
    /* Seed rect from the two packed-short globals. */
    rect_x0 = *(short *)0x325654;
    rect_y0 = *(short *)0x325656;
    rect_x1 = *(short *)0x325658;
    rect_y1 = *(short *)0x32565a;

    if (point != 0) {
      rect_y1 = rect_y1 - rect_y0;
      rect_x1 = rect_x1 - rect_x0;
      rect_y0 = point[0] * rect_y1;
      rect_x0 = point[1] * rect_x1;
      rect_y1 = rect_y1 + rect_y0;
      rect_x1 = rect_x0 + rect_x1;
    }

    if ((*(short *)((char *)screenshot_bitmap + 0xc) == 0xb ||
         *(short *)((char *)screenshot_bitmap + 0xc) == 10) &&
        *(short *)((char *)screenshot_bitmap + 0x14) == 0 && rect_y0 >= 0 &&
        rect_x0 >= 0 && rect_y1 <= *(short *)((char *)screenshot_bitmap + 4) &&
        rect_x1 <= *(short *)((char *)screenshot_bitmap + 6)) {
      void *surface;
      d3d_surface_desc_t desc;

      surface = 0;
      D3DDevice_GetBackBuffer(0, 0, &surface);
      D3DSurface_GetDesc(surface, &desc);

      /* Size check loads +0x18 then +0x14 (disasm order). */
      if (desc.Size == desc.Height * desc.Width * 4) {
        d3d_locked_rect_t locked;

        D3DSurface_LockRect(surface, &locked, 0, 0xC0);

        if (locked.pBits != 0) {
          short row;
          short h; /* SI: bottom - top (per-row byte span source) */
          short w; /* BX: right - left (row count) */
          short bpp;
          int nb;

          h = (short)(*(short *)0x32565a - *(short *)0x325656);
          w = (short)(*(short *)0x325658 - *(short *)0x325654);
          bpp = bitmap_format_bits_per_pixel(
            *(short *)((char *)screenshot_bitmap + 0xc));
          nb = (int)bpp * (int)h;

          /* signed /8 — VC71 emits the CDQ/AND 7/ADD/SAR 3 idiom. */
          if (locked.Pitch != nb / 8) {
            display_assert(
              "d3d_locked_rect.Pitch==bitmap_format_get_bits_per_pixel("
              "screenshot_bitmap->format)*screen_width/CHAR_BITS",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0x6c4,
              1);
            system_exit(-1);
          }

          for (row = 0; row < w; row = (short)(row + 1)) {
            void *dst = bitmap_2d_address(screenshot_bitmap, rect_y0,
                                          (short)(row + rect_x0), 0);
            csmemcpy(dst, (char *)locked.pBits + (int)row * locked.Pitch,
                     locked.Pitch);
          }

          ok = true;
          goto present;
        } else {
          msg = "### ERROR rasterizer_present: failed to lock backbuffer "
                "surface";
        }
      } else {
        msg = "### ERROR rasterizer_present: failed to get backbuffer surface";
      }
    } else {
      msg = "### ERROR rasterizer_present: invalid bitmap";
    }

    error(2, msg);
    ok = false;
  }

present:
  /* PUSH 0; PUSH EAX=[0x505728]; PUSH 0; PUSH 0 →
   * Present(NULL, NULL, hWndPresentTarget, NULL). */
  D3DDevice_Present(0, 0, *(void **)0x505728, 0);

  if (!ok) {
    FUN_00167ff0(0, "IDirect3DDevice8_Present(global_d3d_device, NULL, NULL, "
                    "window_globals.hWndPresentTarget, NULL)");
  }

  /* 64-bit present counter {0x325668 lo, 0x32566c hi} += 1 (ADD/ADC). */
  *(unsigned __int64 *)0x325668 += 1;

  if (!ok) {
    error(2, "### ERROR rasterizer_present failed");
  }
}

/* 0x1580b0
 *
 * rasterizer_set_shader_framebuffer_blend_function
 *
 * Programs the three D3D alpha-blend render states for a shader framebuffer
 * blend function, selecting each state value from a parallel dword lookup
 * table indexed by the blend-function id, and caches the applied values in
 * the rasterizer shadow globals (read back by other draw paths, cf.
 * progress_bar.c). Originally defined in rasterizer_xbox.c (the assert
 * __FILE__ preserves that path); the linker grouped it into
 * rasterizer_decals.obj.
 *
 *   framebuffer_blend_function - shader framebuffer blend function id, [0, 7];
 *                                read at 16-bit width (mov si, word [ebp+8])
 *                                and bounds-checked as a signed short BEFORE
 *                                the movsx widen — keep the short local.
 *
 * Render state (reg@<ecx>) / source table / shadow global (value@<edx>):
 *   0x40344 <- table @0x29da7c, cached at 0x1fb790
 *   0x40348 <- table @0x29daa0, cached at 0x1fb794
 *   0x40350 <- table @0x29dac4, cached at 0x1fb7c0   (0x4034C is skipped)
 *
 * The assert path calls display_assert then system_exit(-1) and FALLS
 * THROUGH into the body (combined ADD ESP,0x14 cleanup) — it is the standard
 * assert macro, not an early return. Tables are indexed via an explicit
 * index*4 byte offset (movsx; shl esi,2; unscaled [esi+disp32] reused for
 * all three tables).
 */
void FUN_001580b0(int framebuffer_blend_function)
{
  short index;
  int offset;
  uint32_t value;
  uint32_t value2;

  index = (short)framebuffer_blend_function;
  if (index < 0 || index >= 8) {
    display_assert(
      "framebuffer_blend_function>=0 && "
      "framebuffer_blend_function<NUMBER_OF_SHADER_FRAMEBUFFER_BLEND_FUNCTIONS",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0x773, 1);
    system_exit(-1);
  }

  offset = index * 4;
  value = *(uint32_t *)(0x29da7c + offset);
  D3DDevice_SetRenderState_Simple(0x40344, value);
  *(uint32_t *)0x1fb790 = value;
  value = *(uint32_t *)(0x29daa0 + offset);
  D3DDevice_SetRenderState_Simple(0x40348, value);
  *(uint32_t *)0x1fb794 = value;
  value2 = *(uint32_t *)(0x29dac4 + offset);
  D3DDevice_SetRenderState_Simple(0x40350, value2);
  *(uint32_t *)0x1fb7c0 = value2;
}

/* 0x1584f0
 *
 * rasterizer_set_target_as_texture  (bind a render-target as a D3D texture)
 *
 * Selects one of the engine's render-target texture headers by `target`
 * index and binds it to D3D sampler `stage`. `max_mipmap` selects the top
 * mip level of the water target's format word.
 *
 * Name/param evidence: assert string "### ERROR
 * rasterizer_set_target_as_texture failed" and the per-case max_mipmap==0
 * asserts.  __FILE__ for the asserts is rasterizer_xbox.c (the original TU; the
 * linker grouped this fn into rasterizer_decals.obj).
 *
 * Globals (used by address, not in kb.json):
 *   0x476a54  void*[2]  render-target texture header pair (backbuffer),
 *                       indexed by frame parity (*0x325668 & 1)
 *   0x476a64  void*     target 1 texture header
 *   0x476a74  void*     target 2 texture header
 *   0x476a7c  void*     target 3 texture header
 *   0x476a84  void*     target 4 texture header
 *   0x476a8c  void*     target 5 texture header
 *   0x476a94  void*     target 6 (water) texture header; ALSO reused
 *                       unconditionally as the mip-field patch target
 *   0x476aa8  void*     target 7 texture header
 *   0x325668  int       backbuffer frame-parity selector
 *
 *   stage       - D3D sampler stage index (used as short)
 *   target      - render-target index (switch on short)
 *   max_mipmap  - top mip level (used as short)
 */
void FUN_001584f0(int stage, int target, int max_mipmap)
{
  void *d3d_texture;
  void *water_hdr;
  char success;
  int hr;

  d3d_texture = 0;
  success = 1;

  switch ((short)target) {
  case 0:
    d3d_texture = ((void **)0x476a54)[*(int *)0x325668 & 1];
    if ((short)max_mipmap != 0) {
      display_assert(
        "max_mipmap==0",
        "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c",
        0x96f, 1);
      system_exit(-1);
    }
    if (((uint32_t *)d3d_texture)[1] == 0) {
      /* Reuse target's stack slot for the backbuffer surface pointer. */
      D3DDevice_GetBackBuffer(0, 0, (void **)&target);
      ((uint32_t *)d3d_texture)[0] = 0x40001;
      ((uint32_t *)d3d_texture)[1] = *(uint32_t *)(target + 4);
      ((uint32_t *)d3d_texture)[2] = 0;
      ((uint32_t *)d3d_texture)[4] = 0x271df27f;
      ((uint32_t *)d3d_texture)[3] = 0x11229;
      hr = (int)D3DResource_Release((void *)target);
      if (hr < 0) {
        success = 0;
        FUN_00167ff0(hr, "IDirect3DSurface8_Release(d3d_backbuffer)");
      } else {
        success = 1;
      }
    }
    d3d_texture = ((void **)0x476a54)[*(int *)0x325668 & 1];
    break;
  case 1:
    if ((short)max_mipmap != 0) {
      display_assert(
        "max_mipmap==0",
        "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c",
        0x999, 1);
      system_exit(-1);
    }
    d3d_texture = *(void **)0x476a64;
    break;
  case 2:
    if ((short)max_mipmap != 0) {
      display_assert(
        "max_mipmap==0",
        "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c",
        0x99d, 1);
      system_exit(-1);
    }
    d3d_texture = *(void **)0x476a74;
    break;
  case 3:
    if ((short)max_mipmap != 0) {
      display_assert(
        "max_mipmap==0",
        "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c",
        0x9a1, 1);
      system_exit(-1);
    }
    d3d_texture = *(void **)0x476a7c;
    break;
  case 4:
    if ((short)max_mipmap != 0) {
      display_assert(
        "max_mipmap==0",
        "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c",
        0x9a5, 1);
      system_exit(-1);
    }
    d3d_texture = *(void **)0x476a84;
    break;
  case 5:
    if ((short)max_mipmap != 0) {
      display_assert(
        "max_mipmap==0",
        "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c",
        0x9a9, 1);
      system_exit(-1);
    }
    d3d_texture = *(void **)0x476a8c;
    break;
  case 6:
    if ((short)max_mipmap < 0 || 4 < (short)max_mipmap) {
      display_assert(
        "max_mipmap>=0 && "
        "max_mipmap<=RASTERIZER_TARGET_WATER_MAX_MIPMAP_LEVELS",
        "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c",
        0x9ad, 1);
      system_exit(-1);
    }
    d3d_texture = *(void **)0x476a94;
    break;
  case 7:
    if ((short)max_mipmap != 0) {
      display_assert(
        "max_mipmap==0",
        "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c",
        0x9bc, 1);
      system_exit(-1);
    }
    d3d_texture = *(void **)0x476aa8;
    break;
  default:
    display_assert(
      "### ERROR unsupported rasterizer target",
      "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c", 0x9c0,
      1);
    system_exit(-1);
  }

  if (d3d_texture == 0) {
    display_assert(
      "d3d_texture",
      "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c", 0x9c3,
      1);
    system_exit(-1);
  }

  water_hdr = *(void **)0x476a94;
  if ((short)max_mipmap != 0) {
    if ((short)max_mipmap < 0 || 4 < (short)max_mipmap) {
      display_assert(
        "max_mipmap>=0 && "
        "max_mipmap<=RASTERIZER_TARGET_WATER_MAX_MIPMAP_LEVELS",
        "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c",
        0x9cc, 1);
      system_exit(-1);
    }
  } else {
    max_mipmap = 4;
  }
  *(uint32_t *)((int)water_hdr + 0xc) =
    ((int)(short)max_mipmap << 0x10) |
    (*(uint32_t *)((int)water_hdr + 0xc) & 0xfff0ffff);

  D3DDevice_SetTexture((uint32_t)(short)stage, d3d_texture);
  if (success == 0) {
    FUN_00167ff0(0, "IDirect3DDevice8_SetTexture(global_d3d_device, stage, "
                    "(IDirect3DBaseTexture8*)d3d_texture)");
    error(2, "### ERROR rasterizer_set_target_as_texture failed");
  }
}

/* 0x158ae0
 *
 * rasterizer_set_stencil_mode  (select a stencil-buffer render mode)
 *
 * Sets the D3D stencil renderstate for one of 6 decal/shadow stencil modes.
 * Caches the currently-applied mode in DAT_00325168 (int16) and early-outs
 * when the requested mode is unchanged. DAT_003256c7 is a byte gate: when
 * clear, the requested mode is forced to 0 (stencil disabled path).
 *
 * Modes 1 and 2 apply the stencil renderstate via the fast D3D wrapper
 * (D3DDevice_SetRenderState_Simple, @ecx=renderstate reg, @edx=value) and
 * mirror each value into a 6-dword shadow cache at 0x1fb7a8..0x1fb7bc.
 * Modes 3/4/5 go through SetRenderStateSmart (deferred/tracked renderstate),
 * which maintains its own cache, so no shadow store is emitted.
 *
 * __FILE__ for the asserts is rasterizer_xbox.c (the original TU; the linker
 * grouped this fn into rasterizer_decals.obj).
 *
 * Globals (used by address, not in kb.json):
 *   0x476ab0  int       global_d3d_device presence (asserted non-NULL)
 *   0x3256c7  char      stencil-enable gate flag (0 => force mode 0)
 *   0x325168  int16     currently-applied stencil mode (cache)
 *   0x1fb7a8  int[6]    shadow renderstate cache (modes 1/2 only)
 *
 *   param_1 - requested stencil mode (used as short)
 */
void FUN_00158ae0(int param_1)
{
  short sVar1;

  if (*(int *)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c", 0xbda,
      1);
    system_exit(-1);
  }

  if (*(char *)0x3256c7 == '\0') {
    sVar1 = 0;
  } else {
    sVar1 = (short)param_1;
  }

  if (sVar1 != *(short *)0x325168) {
    switch (sVar1) {
    case 0:
      D3DDevice_SetRenderState_StencilEnable();
      *(short *)0x325168 = sVar1;
      return;
    case 1:
      D3DDevice_SetRenderState_StencilEnable();
      D3DDevice_SetRenderState_StencilFail();
      D3DDevice_SetRenderState_Simple(0x40374, 0x1e00);
      *(int *)0x1fb7a8 = 0x1e00;
      D3DDevice_SetRenderState_Simple(0x40378, 0x1e01);
      *(int *)0x1fb7ac = 0x1e01;
      D3DDevice_SetRenderState_Simple(0x40364, 0x207);
      *(int *)0x1fb7b0 = 0x207;
      D3DDevice_SetRenderState_Simple(0x40368, 1);
      *(int *)0x1fb7b4 = 1;
      D3DDevice_SetRenderState_Simple(0x4036c, 1);
      *(int *)0x1fb7b8 = 1;
      D3DDevice_SetRenderState_Simple(0x40360, 1);
      *(short *)0x325168 = sVar1;
      *(int *)0x1fb7bc = 1;
      return;
    case 2:
      D3DDevice_SetRenderState_StencilEnable();
      D3DDevice_SetRenderState_StencilFail();
      D3DDevice_SetRenderState_Simple(0x40374, 0x1e00);
      *(int *)0x1fb7a8 = 0x1e00;
      D3DDevice_SetRenderState_Simple(0x40378, 0x1e00);
      *(int *)0x1fb7ac = 0x1e00;
      D3DDevice_SetRenderState_Simple(0x40364, 0x202);
      *(int *)0x1fb7b0 = 0x202;
      D3DDevice_SetRenderState_Simple(0x40368, 0);
      *(int *)0x1fb7b4 = 0;
      D3DDevice_SetRenderState_Simple(0x4036c, 1);
      *(int *)0x1fb7b8 = 1;
      D3DDevice_SetRenderState_Simple(0x40360, 0);
      *(short *)0x325168 = sVar1;
      *(int *)0x1fb7bc = 0;
      return;
    case 3:
      D3DDevice_SetRenderState_StencilEnable();
      D3DDevice_SetRenderState_StencilFail();
      SetRenderStateSmart(0x44, 0x1e00);
      SetRenderStateSmart(0x45, 0x1e00);
      SetRenderStateSmart(0x46, 0x205);
      SetRenderStateSmart(0x47, 0);
      SetRenderStateSmart(0x48, 1);
      SetRenderStateSmart(0x49, 0);
      *(short *)0x325168 = sVar1;
      return;
    case 4:
      SetRenderStateSmart(0x7c, 1);
      SetRenderStateSmart(0x7d, 0x1e00);
      SetRenderStateSmart(0x44, 0x1e00);
      SetRenderStateSmart(0x45, 0x1e01);
      SetRenderStateSmart(0x46, 0x202);
      SetRenderStateSmart(0x47, 2);
      SetRenderStateSmart(0x48, 1);
      SetRenderStateSmart(0x49, 2);
      *(short *)0x325168 = sVar1;
      return;
    case 5:
      SetRenderStateSmart(0x7c, 1);
      SetRenderStateSmart(0x7d, 0x1e00);
      SetRenderStateSmart(0x44, 0x1e00);
      SetRenderStateSmart(0x45, 0x1e00);
      SetRenderStateSmart(0x46, 0x202);
      SetRenderStateSmart(0x47, 0);
      SetRenderStateSmart(0x48, 3);
      SetRenderStateSmart(0x49, 0);
      *(short *)0x325168 = sVar1;
      return;
    default:
      display_assert(
        "### ERROR unsupported stencil mode",
        "c:\\\\halo\\\\SOURCE\\\\rasterizer\\\\xbox\\\\rasterizer_xbox.c",
        0xc1b, 1);
      system_exit(-1);
      *(short *)0x325168 = sVar1;
    }
  }
  return;
}

/*
 * FUN_00158f90 (0x158f90)
 *
 * Per-frame rasterizer render-pass dispatcher.  Asserts the D3D device is
 * present (assert line 0x61f in rasterizer_xbox.c), optionally clears the
 * back buffer target (D3DCLEAR_TARGET = 0x80) in split-screen (>1 window)
 * when the split-screen gate is clear, then runs a fixed sequence of
 * sub-render passes, each gated by a global flag.
 *
 * The FUN_00158800 pass is handed a 4-element uint16 screen-bounds rect
 * (built on the stack) that FUN_00158800 reads as bounds[0..3] to emit a
 * screen-space quad.  Store order below matches the original (offsets 2, 6,
 * 0, 4) for codegen fidelity.
 *
 * Globals (hardcoded, not in kb.json):
 *   0x476ab0  void *  - global_d3d_device (IDirect3DDevice8 pointer)
 *   0x3256ea  short   - split-screen clear gate (0 => issue the Clear)
 *   0x5a5bc4  char    - gate for FUN_00158800
 *   0x5a5bc2  short   - mode sentinel; -1 (0xFFFF) => 0x17ebb0/0x17ef00 passes
 *   0x476ab8  char    - when non-zero, suppress the main pass sequence
 *   0x5a5400  void *  - argument passed to FUN_0017ebb0
 */
/* 0x158f90 */
void FUN_00158f90(void)
{
  short window_count;
  unsigned short bounds[4];

  if (*(void **)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x61f, true);
    system_exit(-1);
  }

  window_count = main_get_window_count();
  if (window_count > 1 && *(short *)0x3256ea == 0) {
    /* 0x1ea650: D3DDevice_Clear(count, rects, flags, color, float z, stencil).
     * Z arg is a genuine float (1.0f) pushed via FLD/FSTP, not an int. */
    D3DDevice_Clear(0, (void *)0, 0x80, 0, 1.0f, 0);
  }

  if (*(char *)0x5a5bc4 != 0) {
    bounds[1] = 0x200; /* 512 */
    bounds[3] = 0x280; /* 640 */
    bounds[0] = 0x0;
    bounds[2] = 0x60; /* 96 */
    FUN_00158800(bounds);
  }

  if (*(short *)0x5a5bc2 == -1) {
    FUN_0017ebb0((void *)0x5a5400);
    FUN_0017ef00();
  }

  if (*(char *)0x476ab8 == 0) {
    FUN_001825d0();
    FUN_0015d160();
    FUN_00184680();
    FUN_00165a00();
    FUN_00181410();
    FUN_0017e030();
    FUN_0017e010();
  }

  FUN_0016FEB0();
}

/*
 * FUN_001592e0  @ 0x1592e0  (rasterizer_decals.obj)
 * -----------------------------------------------------------------------------
 * Decal-state enable setter. Stores the incoming byte flag to the decal-state
 * enable global (0x476ac0). When the flag is cleared (enable == 0), it also
 * resets the paired decal-state word at 0x476ac4 (16-bit store) and the byte
 * flag at 0x476ac1 -- the "disabled" reset path.
 *
 * ABI: plain cdecl, single char stack parameter at [ESP+4] (Ghidra's
 * in_stack_00000004). No @<reg> args, no callees, no FPU. Global writes only.
 * Store order (enable byte, then 0x476ac4 word, then 0x476ac1 byte) preserved
 * from the decompile.
 */
void FUN_001592e0(char enable)
{
  *(char *)0x476ac0 = enable;
  if (enable == '\0') {
    *(short *)0x476ac4 = 0;
    *(char *)0x476ac1 = 0;
  }
}

/* rasterizer_xbox_active_camouflage_draw (FUN_00159900): emit the
 * active-camouflage transparent draw for one geometry group. The effect has
 * two regimes selected by the fade intensity (group->effect.intensity at
 * +0x18):
 *   - intensity == 1.0f (fully cloaked): a single distortion/noise pass that
 *     binds the shader distortion map and animates its scroll matrix
 *     (vs const block, register -0x54), then installs the active-camouflage
 *     pixel shader and draws.
 *   - intensity < 1.0f (partially fading in/out): a first pass that renders
 *     the model geometry (FUN_0017cbc0) so the silhouette shows through,
 *     followed by the common distortion pass.
 * Both regimes finish with the "common" second pass which uploads the
 * screen-space distortion matrix derived from the active camera/view-params
 * block (*(void **)0x476204) lerped by group->effect fade (+0x1c), then
 * installs the distortion pixel shader and draws.
 *
 * Original TU:
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_active_camouflage.c
 *
 * Lift provenance: verified instruction-by-instruction against the delinked
 * reference delinked/functions/00159900.obj (render-state values, stage-state
 * triples, vertex-constant formulas, and the slow-path contiguous descriptor
 * were all decoded from that disassembly, not from the decompiler). */

static const char kActiveCamoFile[] =
  "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_active_camouflage.c";

/* Slow-path (partial fade) geometry-pass descriptor. In the original this is
 * ONE contiguous stack block at ebp-0x108 passed whole to FUN_0017cbb0 —
 * separate locals would break the callee's indexed reads (stack-aliasing
 * hazard). Offsets derived from the reference disassembly MOV [ebp-N] stores:
 *   -0x108 flags, -0x100 g+0x60, -0xfc word g+0x64, -0xf8 anim[0x74],
 *   -0x84 tc pair, -0x7c zeroed 0x28, -0x54 g+0x74/78/7c, -0x44 g+0x3c/0x40 */
typedef struct {
  uint32_t flags; /* +0x00: group->geometry_flags & 0x80 */
  uint32_t field_4; /* +0x04: not written by the original */
  uint32_t field_8; /* +0x08: *(uint32_t *)(grp + 0x60) */
  uint16_t field_c; /* +0x0c: *(uint16_t *)(grp + 0x64) */
  uint16_t field_e; /* +0x0e: pad, not written */
  uint8_t anim[0x74]; /* +0x10: copy of *(void **)(grp + 0x68) or zeroed */
  uint32_t tc[2]; /* +0x84: pair from *(uint32_t **)(grp + 0x6c) or 0 */
  uint8_t zeroed[0x28]; /* +0x8c: csmemset 0 */
  uint32_t field_b4; /* +0xb4: *(uint32_t *)(grp + 0x74) */
  uint32_t field_b8; /* +0xb8: *(uint32_t *)(grp + 0x78) */
  uint32_t field_bc; /* +0xbc: *(uint32_t *)(grp + 0x7c) */
  uint32_t field_c0; /* +0xc0: not written by the original */
  uint32_t field_c4; /* +0xc4: *(uint32_t *)(grp + 0x3c) (u scale bits) */
  uint32_t field_c8; /* +0xc8: *(uint32_t *)(grp + 0x40) (v scale bits) */
} s_camo_geometry_pass; /* 0xcc bytes */

void FUN_00159900(void *group)
{
  char *grp = (char *)group;
  char *pv; /* FUN_001906b0(shader, 4): resolved shader params base (edi) */
  char *cam; /* *(char **)0x476204: active camera / view-parameters block */
  int cull; /* computed CullMode select */
  float fv; /* 1.0f - group->effect fade (+0x1c) */

  /* Slow-path contiguous descriptor (ebp-0x108 block in the original). */
  s_camo_geometry_pass desc;

  /* Contiguous 12-float (3 vec4) vertex-shader constant block uploaded via
   * D3DDevice_SetVertexShaderConstant(-0x54, &vs[0], 3). MUST stay a single
   * array so the 12 floats are laid out contiguously (ebp-0x30..-0x4 in the
   * original). */
  float vs[12];

  /* --- input validation asserts (source lines 0x98..0x9d) --- */
  if (group == 0) {
    display_assert("group", kActiveCamoFile, 0x98, 1);
    system_exit(-1);
  }
  if (*(int *)(grp + 0xc) == 0) {
    display_assert("group->shader", kActiveCamoFile, 0x99, 1);
    system_exit(-1);
  }
  if (*(short *)(grp + 0x14) != 1) {
    display_assert(
      "group->effect.type==_render_model_effect_type_active_camouflage",
      kActiveCamoFile, 0x9a, 1);
    system_exit(-1);
  }
  if (!(*(float *)(grp + 0x18) > 0.0f)) {
    display_assert("group->effect.intensity>0.0f", kActiveCamoFile, 0x9b, 1);
    system_exit(-1);
  }
  if (!(*(float *)(grp + 0x18) <= 1.0f)) {
    display_assert("group->effect.intensity<=1.0f", kActiveCamoFile, 0x9c, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device", kActiveCamoFile, 0x9d, 1);
    system_exit(-1);
  }

  /* Early-out gate: only draw when active-camouflage rendering is enabled
   * (char @0x3256f9) and the distortion accumulation buffer is idle
   * (short @0x5a5bc0 == 0). */
  if (*(char *)0x3256f9 == 0 || *(short *)0x5a5bc0 != 0) {
    return;
  }

  pv = (char *)FUN_001906b0(*(void **)(grp + 0xc), 4);

  /* --- debug asserts (source lines 0xa4..0xa5) --- */
  if (*(char *)0x476ac1 == 0) {
    display_assert("local_active_camouflage_debug", kActiveCamoFile, 0xa4, 1);
    system_exit(-1);
  }
  if ((*(int *)grp & 2) != 0) {
    display_assert("!(group->geometry_flags & _no_queue_bit)", kActiveCamoFile,
                   0xa5, 1);
    system_exit(-1);
  }

  /* Constrain the depth range for this group's queue bucket. */
  if (*(char *)grp < 0) {
    rasterizer_set_frustum_z(*(float *)0x32569c, *(float *)0x3256a0);
  }

  /* intensity == 1.0f tested as a bit-exact integer compare (0x3f800000),
   * NOT a float comparison, to match the original codegen. */
  if (*(int *)(grp + 0x18) == 0x3f800000) {
    /* ---- FAST PATH: fully cloaked, distortion pass only ---- */
    rasterizer_set_texture(0, 0, 1, *(int *)(pv + 0xb0),
                           *(unsigned short *)(grp + 0x10));
    D3DDevice_SetTextureStageState(0, 0xa, 1);
    D3DDevice_SetTextureStageState(0, 0xb, 1);
    D3DDevice_SetTextureStageState(0, 0xd, 2);
    D3DDevice_SetTextureStageState(0, 0xe, 2);
    D3DDevice_SetTextureStageState(0, 0xf, 2);

    /* CullMode select: 0x901 (CW) normally, 0x900&..=0x200-series when the
     * shader two-sided flag (pv+0x28 & 2) is set. */
    cull =
      (-(int)((*(unsigned char *)(pv + 0x28) & 2) != 0) & (int)0xfffff6ff) +
      0x901;
    D3DDevice_SetRenderState_CullMode(cull);

    /* Render-state block (values decoded from the delinked reference; each
     * SetRenderState_Simple is followed by its render-state cache mirror). */
    D3DDevice_SetRenderState_Simple(0x40358, 0);
    *(uint32_t *)0x1fb7a4 = 0;
    D3DDevice_SetRenderState_Simple(0x40304, 0);
    *(uint32_t *)0x1fb784 = 0;
    D3DDevice_SetRenderState_Simple(0x40300, 1);
    *(uint32_t *)0x1fb788 = 1;
    D3DDevice_SetRenderState_Simple(0x40340, 0x7f);
    *(uint32_t *)0x1fb78c = 0x7f;
    D3DDevice_SetRenderState_ZEnable(1);
    D3DDevice_SetRenderState_Simple(0x4035c, 1);
    *(uint32_t *)0x1fb798 = 1;
    D3DDevice_SetRenderState_Simple(0x40354, 0x203);
    *(uint32_t *)0x1fb77c = 0x203;
    D3DDevice_SetRenderState_ZBias(0);

    FUN_00178b40(0xd, FUN_00184610(group), 0);

    /* vs row 0: distortion scale (raw copy + product); rows 1-2 hold the
     * animated scroll matrix filled by FUN_00190e10 (out ptrs &vs[4]/&vs[8]),
     * seeded with identity. */
    vs[0] = *(float *)(pv + 0xd8);
    vs[1] = *(float *)(pv + 0xec) * *(float *)(pv + 0xd8);
    vs[2] = 1.0f;
    vs[3] = 1.0f;
    vs[4] = 1.0f;
    vs[5] = 0.0f;
    vs[6] = 0.0f;
    vs[7] = 0.0f;
    vs[8] = 0.0f;
    vs[9] = 1.0f;
    vs[10] = 0.0f;
    vs[11] = 0.0f;
    FUN_00190e10((void *)(pv + 0xfc), *(void **)(grp + 0x6c),
                 *(float *)(pv + 0x9c) * *(float *)(grp + 0x3c),
                 *(float *)(pv + 0xa0) * *(float *)(grp + 0x40), 0.0f, 0.0f,
                 0.0f, *(float *)0x5a5e18, &vs[4], &vs[8]);
    D3DDevice_SetVertexShaderConstant(-0x54, vs, 3);

    /* Install the active-camouflage pixel shader. */
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(uint32_t *)0x5a5b98 = 1;
    *(uint32_t *)0x5a5b94 = 1;
    *(uint32_t *)0x5a5ae4 = 0x1800;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    FUN_00174510(group, 0);
  } else {
    /* ---- SLOW PATH: partial fade, render silhouette geometry first ---- */
    desc.flags = *(int *)grp & 0x80;
    desc.field_8 = *(uint32_t *)(grp + 0x60);
    desc.field_c = *(uint16_t *)(grp + 0x64);
    desc.field_b4 = *(uint32_t *)(grp + 0x74);
    desc.field_b8 = *(uint32_t *)(grp + 0x78);
    desc.field_bc = *(uint32_t *)(grp + 0x7c);
    csmemset(desc.zeroed, 0, 0x28);
    desc.field_c4 = *(uint32_t *)(grp + 0x3c);
    desc.field_c8 = *(uint32_t *)(grp + 0x40);
    if (*(void **)(grp + 0x68) != 0) {
      csmemcpy(desc.anim, *(void **)(grp + 0x68), 0x74);
    } else {
      csmemset(desc.anim, 0, 0x74);
    }
    if (*(uint32_t **)(grp + 0x6c) != 0) {
      desc.tc[0] = (*(uint32_t **)(grp + 0x6c))[0];
      desc.tc[1] = (*(uint32_t **)(grp + 0x6c))[1];
    } else {
      csmemset(desc.tc, 0, 8);
    }
    rasterizer_psuedo_dynamic_screen_quad_draw(0);
    FUN_0017d1a0(0);
    FUN_0017cbb0(&desc, 1);
    FUN_0017cbc0(*(int *)(grp + 0xc), *(unsigned short *)(grp + 0x10),
                 *(int *)(grp + 0x48), *(int *)(grp + 0x44),
                 *(int *)(grp + 0x50), *(int *)(grp + 0x58),
                 *(int *)(grp + 0x54));
    FUN_0016b1c0();
    FUN_0016b240();
    rasterizer_psuedo_dynamic_screen_quad_draw(1);
  }

  /* ---- COMMON SECOND PASS: screen-space distortion ---- */
  rasterizer_set_texture_direct(0, *(int *)(*(char **)0x476204 + 0x5c), 0);
  /* Stage-state triples decoded from the reference (note (0,0xb,3) really is
   * issued twice in the original). */
  D3DDevice_SetTextureStageState(0, 0xa, 3);
  D3DDevice_SetTextureStageState(0, 0xb, 3);
  D3DDevice_SetTextureStageState(0, 0xb, 3);
  D3DDevice_SetTextureStageState(0, 0xd, 2);
  D3DDevice_SetTextureStageState(0, 0xe, 2);
  D3DDevice_SetTextureStageState(0, 0xf, 2);
  FUN_001584f0(2, 1, 0);
  D3DDevice_SetTextureStageState(2, 0xa, 3);
  D3DDevice_SetTextureStageState(2, 0xb, 3);
  D3DDevice_SetTextureStageState(2, 0xd, 2);
  D3DDevice_SetTextureStageState(2, 0xe, 2);
  D3DDevice_SetTextureStageState(2, 0xf, 1);

  cull = (-(int)((*(unsigned char *)(pv + 0x28) & 2) != 0) & (int)0xfffff6ff) +
         0x901;
  D3DDevice_SetRenderState_CullMode(cull);

  /* Render state for the distortion pass. */
  D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
  *(uint32_t *)0x1fb7a4 = 0x10101;
  D3DDevice_SetRenderState_Simple(0x40300, 0);
  *(uint32_t *)0x1fb788 = 0;
  D3DDevice_SetRenderState_ZEnable(1);
  D3DDevice_SetRenderState_Simple(0x4035c, 0);
  *(uint32_t *)0x1fb798 = 0;
  D3DDevice_SetRenderState_Simple(0x40354, 0x202);
  *(uint32_t *)0x1fb77c = 0x202;
  D3DDevice_SetRenderState_ZBias(0);

  /* Blend mode: alpha-blend while fading in, opaque when fully cloaked. */
  if (*(float *)(grp + 0x18) < 1.0f) {
    SetRenderStateSmart(0x3b, 1);
    SetRenderStateSmart(0x3e, 0x302);
    SetRenderStateSmart(0x3f, 0x303);
    SetRenderStateSmart(0x4a, 0x8006);
  } else {
    SetRenderStateSmart(0x3b, 0);
  }

  FUN_00178b40(0x40, FUN_00184610(group), 0);

  /* Screen-space distortion matrix: lerp camera-matrix rows (view-params
   * block at *(char **)0x476204, current row at +0x174.. vs target row at
   * +0x188..) by the effect fade (grp+0x1c). Only vs[0] is additionally
   * scaled by intensity. Constants 320.0f/240.0f are the half-screen
   * distortion extents. Formulas decoded from the reference FPU stream. */
  fv = 1.0f - *(float *)(grp + 0x1c);
  cam = *(char **)0x476204;
  vs[1] = fv * *(float *)(cam + 0x178) +
          *(float *)(cam + 0x18c) * *(float *)(grp + 0x1c);
  vs[8] = fv * *(float *)(cam + 0x17c) +
          *(float *)(cam + 0x190) * *(float *)(grp + 0x1c);
  vs[9] = fv * *(float *)(cam + 0x180) +
          *(float *)(cam + 0x194) * *(float *)(grp + 0x1c);
  vs[10] = fv * *(float *)(cam + 0x184) +
           *(float *)(cam + 0x198) * *(float *)(grp + 0x1c);
  vs[2] = 320.0f;
  vs[3] = 240.0f;
  vs[4] = 0.0f;
  vs[5] = 0.0f;
  vs[6] = 0.0f;
  vs[7] = 0.0f;
  vs[0] = (fv * *(float *)(cam + 0x174) +
           *(float *)(cam + 0x188) * *(float *)(grp + 0x1c)) *
          *(float *)(grp + 0x18);
  vs[11] = 0.0f;
  D3DDevice_SetVertexShaderConstant(-0x54, vs, 3);

  /* Install the distortion pixel shader and its combiner state. */
  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b98 = 0x2623;
  *(uint32_t *)0x5a5ba0 = 0;
  *(uint32_t *)0x5a5b9c = 0x11;
  *(uint32_t *)0x5a5b94 = 2;
  *(uint32_t *)0x5a5b74 = 0xc00;
  *(uint32_t *)0x5a5b4c = 0x3420140c;
  *(uint32_t *)0x5a5b78 = 0xc00;
  *(uint32_t *)0x5a5b48 =
    (-(int)((*(unsigned char *)(*(char **)0x476204 + 0x170) & 1) != 0) &
     (int)0x34001804) +
    0x4200000;
  *(uint32_t *)0x5a5b6c = FUN_00159070(*(float *)(grp + 0x18));
  *(uint32_t *)0x5a5ae0 = 0xa0c0000;
  *(uint32_t *)0x5a5ae4 = 0x1100;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);
  FUN_00174510(group, 0);

  /* Restore the default depth range. */
  if (*(char *)grp < 0) {
    rasterizer_set_frustum_z(0.0f, 0.0f);
  }
}

/* 0x15a560
 *
 * Set up D3D render state for one decal-render pass. `additive` selects the
 * blend mode: the additive path leaves SRCBLEND/DESTBLEND untouched and only
 * flips the blend enable/op shadows, while the alpha path programs the
 * SRC=SRCALPHA / DEST=INVSRCALPHA blend with BLENDOP=REVSUBTRACT (0x8006).
 * Both paths clear the 0xf0-byte decal render-state block at 0x5a5ac0, seed a
 * few of its fields, then install the decal pixel shader (FUN_00156510).
 *
 * Each *(uint32_t *)0x1fbXXX write is a host-side shadow copy of the D3D
 * renderstate just programmed (same globals used by the neighbouring
 * rasterizer setup functions); preserve each one.
 *
 * Original TU asserts against
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_debug.c (line 0x13); grouped
 * into rasterizer_decals.obj. cdecl, one byte-bool stack arg at [esp+4].
 */
void FUN_0015a560(char additive)
{
  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "debug.c",
                   0x13, 1);
    halt_and_catch_fire();
  }
  if (*(char *)0x3256dd == 0) {
    return;
  }

  FUN_00178b40(0, 9, 0);
  D3DDevice_SetRenderState_CullMode(0);
  D3DDevice_SetRenderState_Simple(0x40354, 0x203);
  *(uint32_t *)0x1fb77c = 0x203;
  D3DDevice_SetRenderState_ZEnable(1);
  D3DDevice_SetRenderState_ZBias(*(uint32_t *)0x32570c);
  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b98 = 0;
  *(uint32_t *)0x5a5b94 = 1;
  *(uint32_t *)0x5a5ae0 = 4;

  if (additive != 0) {
    D3DDevice_SetRenderState_Simple(0x40304, 0);
    *(uint32_t *)0x1fb784 = 0;
    D3DDevice_SetRenderState_Simple(0x40300, 0);
    *(uint32_t *)0x1fb788 = 0;
    D3DDevice_SetRenderState_Simple(0x4035c, 1);
    *(uint32_t *)0x1fb798 = 1;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    return;
  }

  D3DDevice_SetRenderState_Simple(0x40304, 1);
  *(uint32_t *)0x1fb784 = 1;
  D3DDevice_SetRenderState_Simple(0x40300, 0);
  *(uint32_t *)0x1fb788 = 0;
  D3DDevice_SetRenderState_Simple(0x40344, 0x302);
  *(uint32_t *)0x1fb790 = 0x302;
  D3DDevice_SetRenderState_Simple(0x40348, 0x303);
  *(uint32_t *)0x1fb794 = 0x303;
  D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
  *(uint32_t *)0x1fb7c0 = 0x8006;
  D3DDevice_SetRenderState_Simple(0x4035c, 0);
  *(uint32_t *)0x1fb798 = 0;
  *(uint32_t *)0x5a5ae4 = 0x1400;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);
}

/* 0x15a700
 *
 * Program the D3D render state for a decal-render pass and install the decal
 * pixel shader. Unlike the neighbouring FUN_0015a560 this variant takes no
 * arguments, has no per-frame gate byte, and uses a fixed (single) blend
 * configuration: CULL off, ALPHABLENDENABLE off (0x40304=0), no separate-alpha
 * blend (0x40300=0), STENCILFUNC=0x203 (D3DCMP_LESSEQUAL, 0x40354), ZENABLE on,
 * BLENDOP=1 (D3DBLENDOP_ADD, 0x4035c), then ZBias from the global at 0x32570c.
 *
 * Each *(uint32_t *)0x1fbXXX write is the host-side shadow copy of the D3D
 * renderstate just programmed (same shadow globals as FUN_0015a560); preserve
 * each store's value and its interleaved position relative to the D3D call.
 *
 * After the render-state block it clears the 0xf0-byte decal render-state
 * block at 0x5a5ac0, seeds three fields (0x5a5b98=0, 0x5a5b94=1, 0x5a5ae0=4),
 * and installs the decal pixel shader (FUN_00156510).
 *
 * Original TU asserts against
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_debug.c (line 0x51); the
 * linker grouped it into rasterizer_decals.obj. cdecl, no arguments.
 */
void FUN_0015a700(void)
{
  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "debug.c",
                   0x51, 1);
    halt_and_catch_fire();
  }

  D3DDevice_SetRenderState_CullMode(0);
  D3DDevice_SetRenderState_Simple(0x40304, 0);
  *(uint32_t *)0x1fb784 = 0;
  D3DDevice_SetRenderState_Simple(0x40300, 0);
  *(uint32_t *)0x1fb788 = 0;
  D3DDevice_SetRenderState_Simple(0x40354, 0x203);
  *(uint32_t *)0x1fb77c = 0x203;
  D3DDevice_SetRenderState_ZEnable(1);
  D3DDevice_SetRenderState_Simple(0x4035c, 1);
  *(uint32_t *)0x1fb798 = 1;
  D3DDevice_SetRenderState_ZBias(*(uint32_t *)0x32570c);
  FUN_00178b40(0, 9, 0);
  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b98 = 0;
  *(uint32_t *)0x5a5b94 = 1;
  *(uint32_t *)0x5a5ae0 = 4;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);
}

/* 0x15aa40
 *
 * rasterizer_debug_setup_screen_projection  (name inferred)
 *
 * Configures fixed-function-style render state and uploads a 5-constant
 * (20-float) vertex-shader constant block that maps 2D screen coordinates
 * into clip space for the debug-line / debug-geometry pass, then installs
 * the debug pixel shader state block. Called once per debug draw batch.
 *
 * __FILE__ for the device assert is rasterizer_xbox_debug.c (line 0xa7); the
 * linker grouped this fn into rasterizer_decals.obj.
 *
 * The constant block (vs[0..19], 5 vertex-shader constants c[-0x44..-0x40])
 * is a screen->clip transform. Slots vs[0],vs[3],vs[5],vs[7] are computed
 * from the current viewport rect; the remaining slots are literal 0.0/0.5/
 * 1.0 constants.
 *
 * Viewport rect globals (16-bit fields; .lo=x, .hi=y):
 *   0x5a5bf4  int  rect min (x0 at +0, y0 at +2)
 *   0x5a5bf8  int  rect max (x1 at +0, y1 at +2)
 *     height = (short)(y1 - y0)   [16-bit .hi fields, sign-extended]
 *     width  = (short)(x1 - x0)   [full 32-bit dword sub, low16 sign-extended]
 *
 * Float constants: [0x2533c8]=1.0f, [0x255e94]=-1.0f, [0x25eeac]=-2.0f.
 * Render-state cache mirrors: 0x1fb784, 0x1fb788. Pixel-shader state block:
 * 0x5a5ac0 (0xf0 bytes), fields 0x5a5b98=0, 0x5a5b94=1, 0x5a5ae0=4.
 */
void FUN_0015aa40(void)
{
  /* 20-float (5 vertex-shader constant) upload block; must stay one
   * contiguous array so the constants are laid out ebp-0x54..ebp-0x8. */
  float vs[20];
  int width;
  int height;
  float fVar1;
  float wdiv;

  if (*(int *)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_debug.c", 0xa7, 1);
    system_exit(-1);
  }

  D3DDevice_SetRenderState_CullMode(0);
  D3DDevice_SetRenderState_Simple(0x40304, 0);
  *(uint32_t *)0x1fb784 = 0;
  D3DDevice_SetRenderState_Simple(0x40300, 0);
  *(uint32_t *)0x1fb788 = 0;
  D3DDevice_SetRenderState_ZEnable(0);
  D3DDevice_SetRenderState_ZBias(0);

  FUN_00178b40(4, 8, 0);

  /* viewport height from the 16-bit .hi (y) fields, sign-extended */
  height = (short)(*(short *)0x5a5bfa - *(short *)0x5a5bf6);
  fVar1 = 1.0f / (float)height;
  /* viewport width: full 32-bit dword subtract, low 16 bits sign-extended */
  width = (short)(*(int *)0x5a5bf8 - *(int *)0x5a5bf4);

  /* constant slots (literal bit patterns) */
  vs[1] = 0.0f;
  vs[2] = 0.0f;
  vs[4] = 0.0f;
  vs[6] = 0.0f;
  vs[8] = 0.0f;
  vs[9] = 0.0f;
  vs[10] = 0.0f;
  vs[11] = 0.5f;
  vs[12] = 0.0f;
  vs[13] = 0.0f;
  vs[14] = 0.0f;
  vs[15] = 1.0f;
  vs[16] = 1.0f;
  vs[17] = 1.0f;
  vs[18] = 0.0f;
  vs[19] = 1.0f;

  /* computed slots (x87 order preserved: see disasm 0x15ab66-0x15ab94) */
  vs[0] = fVar1 + fVar1;
  vs[3] = -1.0f - fVar1;
  wdiv = 1.0f / (float)width;
  vs[5] = -2.0f * wdiv;
  vs[7] = wdiv + 1.0f;

  D3DDevice_SetVertexShaderConstant(-0x44, vs, 5);

  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b98 = 0;
  *(uint32_t *)0x5a5b94 = 1;
  *(uint32_t *)0x5a5ae0 = 4;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);
}

/* 0x15abe0
 *
 * rasterizer_debug_draw_line2d  (debug 2D line drawer)
 *
 * Draws a single screen-space debug line between two 2D points with optional
 * per-vertex color, using the D3D inline (Begin/SetVertexData.../End) vertex
 * stream. Originally defined in rasterizer_xbox_debug.c (the assert __FILE__
 * string preserves that path), but the linker grouped it into
 * rasterizer_decals.obj.
 *
 *   p0     - short[2] screen coords of the first endpoint  (x, y)
 *   p1     - short[2] screen coords of the second endpoint (x, y)
 *   color0 - real_rgb_color for p0 (packed to 0x00RRGGBB by FUN_000d1dd0)
 *   color1 - optional real_rgb_color for p1; if NULL, p0's color is reused
 *
 * D3D primitive: Begin(4 = D3DPT_LINELIST). Vertex register 9 =
 * D3DVSDE_DIFFUSE (color), register 0 = D3DVSDE_VERTEX (position via
 * SetVertexData2s). SetVertexData2s receives the position components
 * zero-extended from the 16-bit screen coords. The trailing D3DDevice_End
 * is emitted by the original as a tail-call (JMP 0x1ed490).
 */
void FUN_0015abe0(short *p0, short *p1, float *color0, float *color1)
{
  unsigned int packed0;
  unsigned int packed1;

  if (p0 == 0 || p1 == 0 || color0 == 0) {
    display_assert(
      "p0 && p1 && color0",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_debug.c", 0xdd, 1);
    system_exit(-1);
  }
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_debug.c", 0xde, 1);
    system_exit(-1);
  }

  packed0 = FUN_000d1dd0(color0);
  if (color1 != 0) {
    packed1 = FUN_000d1dd0(color1);
  }

  D3DDevice_Begin(4);

  D3DDevice_SetVertexDataColor(9, packed0);
  D3DDevice_SetVertexData2s(0, (int)(unsigned short)p0[0],
                            (int)(unsigned short)p0[1]);

  if (color1 != 0) {
    D3DDevice_SetVertexDataColor(9, packed1);
  }
  D3DDevice_SetVertexData2s(0, (int)(unsigned short)p1[0],
                            (int)(unsigned short)p1[1]);

  D3DDevice_End();
}

/* 0x15afa0
 *
 * rasterizer_decals_vertex_cache_delete  (LRUV eviction callback)
 *
 * Called by the LRUV cache when a cached decal vertex block is evicted.
 * Validates decal_index is neither 0 nor NONE (-1), then:
 *   - Warns once if the decal is still locked when evicted (flag bit 0).
 *   - Warns once if the decal is still permanent when evicted (flag bit 1).
 *   - Calls decal_delete (0x9a160) to free the underlying decal datum.
 *
 * datum_get(g_decals_data at 0x5aa8b8, decal_index) is re-read for each flag
 * check to match the original (0x15b020 and 0x15b05e); do not hoist.
 */
void FUN_0015afa0(int decal_index)
{
  char *decal;
  const char *reason;
  int line;

  /* lruv_has_locked_proc(local_vertex_cache): cache must have a lock query cb
   */
  if (!lruv_cache_has_query_cb(*(void **)0x476adc)) {
    display_assert(
      "lruv_has_locked_proc(local_vertex_cache)",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x1d, 1);
    system_exit(-1);
  }

  if (decal_index == 0) {
    display_assert(
      "decal_index",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x1e, 1);
    system_exit(-1);
    line = 0x20;
    reason = "decal_index!=0";
  } else {
    if (decal_index != -1)
      goto LAB_0015b018;
    line = 0x1f;
    reason = "decal_index!=NONE";
  }
  display_assert(reason,
                 "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c",
                 line, 1);
  system_exit(-1);

LAB_0015b018:
  decal = (char *)datum_get(*(void **)0x5aa8b8, decal_index);
  if (((*(unsigned char *)(decal + 2) & 1) != 0) &&
      (*(char *)0x476ae0 == '\0')) {
    error(2,
          "### ERROR decals: deleting locked decal (#%d, queried=#%d) in "
          "rasterizer -- tell Bernie!!",
          decal_index, *(int *)0x32516c);
    *(char *)0x476ae0 = '\x01';
  }

  decal = (char *)datum_get(*(void **)0x5aa8b8, decal_index);
  if (((*(unsigned char *)(decal + 2) & 2) != 0) &&
      (*(char *)0x476ae1 == '\0')) {
    error(2,
          "### ERROR decals: deleting permanent decal (#%d, queried=#%d) in "
          "rasterizer -- tell Bernie!!",
          decal_index, *(int *)0x32516c);
    *(char *)0x476ae1 = '\x01';
  }

  decal_delete(decal_index);
}

/* 0x15b150
 *
 * rasterizer_decals_register_callbacks
 *
 * Registers the LRUV local_vertex_cache delete/query callbacks. Asserts the
 * cache handle (initialized by rasterizer_decals_initialize) exists, then
 * installs FUN_0015afa0 as the eviction/delete callback and FUN_0015b0c0 as
 * the lock-query callback via lruv_cache_set_callbacks.
 *
 * The two callbacks are referenced by address only (not called here), so no
 * push-order or FPU concerns. FUN_0015b0c0 returns bool at its address; the
 * cast to int(*)(int) matches the lruv_cache_set_callbacks query_cb slot.
 */
void FUN_0015b150(void)
{
  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x74, 1);
    system_exit(-1);
  }
  lruv_cache_set_callbacks(*(void **)0x476adc, FUN_0015afa0,
                           (int (*)(int))FUN_0015b0c0);
}

/* 0x15b190
 *
 * rasterizer_decals_initialize_for_new_map
 *
 * No-op in this build: the vertex cache is persistent across maps and does
 * not need per-map reinitialization.
 */
void rasterizer_decals_initialize_for_new_map(void)
{
  return;
}

/* 0x15b1a0
 *
 * rasterizer_decals_dispose_from_old_map
 *
 * Asserts the LRUV vertex cache exists, then clears all per-map decal state:
 *   1. Calls decals_update_for_new_map(true) to strip lock/permanent flags
 *      from every live decal datum and reset the global lock/permanent counts.
 *   2. Calls lruv_cache_dispose_all() to evict all cached vertex entries.
 */
void rasterizer_decals_dispose_from_old_map(void)
{
  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x83, 1);
    system_exit(-1);
  }
  decals_update_for_new_map(1);
  lruv_cache_dispose_all(*(void **)0x476adc);
}

/* 0x15b1e0
 *
 * rasterizer_decals_dispose
 *
 * Asserts the LRUV vertex cache exists, resets decal state (non-full reset),
 * and evicts all cached vertex entries.
 */
void FUN_0015b1e0(void)
{
  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x8e, 1);
    system_exit(-1);
  }
  decals_update_for_new_map(0);
  lruv_cache_dispose_all(*(void **)0x476adc);
}

/* 0x15b220
 *
 * rasterizer_decals_idle
 *
 * Performs idle maintenance on the decal vertex LRUV cache (e.g. eviction
 * of stale entries).
 */
void FUN_0015b220(void)
{
  lruv_idle(*(void **)0x476adc);
}

/* 0x15b460
 *
 * Allocate vertex cache space for decal vertices.
 * Asserts that cache_size exceeds sizeof(struct decal_vertex) (0x10 bytes)
 * and is aligned to that size, then forwards to the LRUV cache allocator
 * (FUN_0011de10). Returns the allocated block index, or NONE on failure. */
int FUN_0015b460(uint32_t cache_size)
{
  if (cache_size <= 0x10) {
    display_assert(
      "cache_size>sizeof(struct decal_vertex)",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0xcc, 1);
    system_exit(-1);
  }

  if ((cache_size & 0xf) != 0) {
    display_assert(
      "cache_size%sizeof(struct decal_vertex)==0",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0xcd, 1);
    system_exit(-1);
  }

  {
    int (*lruv_cache_allocate)(void *, uint32_t) = (void *)0x11de10;
    return lruv_cache_allocate(*(void **)0x476adc, cache_size);
  }
}

/* 0x15b530
 *
 * Removes one decal vertex-cache entry from the LRUV cache.
 * Asserts valid decal index and cache handle before forwarding to the
 * lruv-cache block removal helper.
 */
void FUN_0015b530(int decal_index)
{
  if (decal_index == -1) {
    display_assert(
      "cache_index!=NONE",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x106, 1);
    system_exit(-1);
  }

  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x107, 1);
    system_exit(-1);
  }

  lruv_block_delete(*(void **)0x476adc, decal_index);
}

/* 0x15b0c0
 *
 * rasterizer_decals_vertex_cache_query  (LRUV locked-probe callback)
 *
 * Called by the LRUV cache to check whether a cached decal may be evicted.
 * Returns 1 (locked / not evictable) if the decal datum has either the
 * locked (bit 0) or permanent (bit 1) flag set; 0 otherwise.
 * Also records decal_index into 0x32516c for debug display.
 */
static int rasterizer_decals_vertex_cache_query(int decal_index)
{
  char *iVar1;
  char *pcVar2;
  int uVar3;

  if (decal_index == 0) {
    display_assert(
      "decal_index",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x47, 1);
    system_exit(-1);
    uVar3 = 0x49;
    pcVar2 = "decal_index!=0";
  } else {
    if (decal_index != -1)
      goto LAB_0015b105;
    uVar3 = 0x48;
    pcVar2 = "decal_index!=NONE";
  }
  display_assert(pcVar2,
                 "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c",
                 uVar3, 1);
  system_exit(-1);

LAB_0015b105:
  iVar1 = datum_get(*(void **)0x5aa8b8, decal_index);
  *(int *)0x32516c = decal_index;
  if ((*(unsigned char *)(iVar1 + 2) & 3) == 0) {
    return 0;
  }
  return 1;
}

/* 0x15b5e0
 *
 * Sets the appropriate rasterizer blend/render state for decals based on
 * the current decal rendering mode (global at 0x476ac8). If mode is 3
 * (shadow), calls FUN_00158ae0 to set up shadow state first. Then selects
 * a texture stage configuration from a local lookup table indexed by mode.
 */
void FUN_0015b5e0(void)
{
  short mode;
  short stage_table[5];

  mode = *(short *)0x476ac8;
  if (mode == 3) {
    FUN_00158ae0(2);
    mode = *(short *)0x476ac8;
  }
  stage_table[0] = 9;
  stage_table[1] = 10;
  stage_table[2] = 6;
  stage_table[3] = 7;
  stage_table[4] = 0x14;
  if (mode >= 0 && mode < 5) {
    FUN_0016fa40((int)stage_table[mode]);
  }
}

/* 0x15b6d0
 *
 * rasterizer_decals_initialize
 *
 * Allocates and registers the D3D decal vertex buffer, then creates the
 * LRUV vertex cache that maps decal indices to vertex-buffer ranges.
 *
 * The vertex buffer struct (12 bytes, from debug_malloc) layout:
 *   +0x0  uint32_t  D3D resource header word (ref-count/type), set to 1
 *   +0x4  void *    GPU memory pointer (game_state_gpu_alloc, 0x28000 bytes)
 *   +0x8  uint32_t  lock flags, set to 0
 *
 * Derived from disassembly:
 *   MOV dword ptr [EAX],     0x1   -> [+0x0]
 *   LEA ESI, [EAX+4];  MOV [ESI], EAX_gpu -> [+0x4]
 *   MOV dword ptr [EAX+0x8], 0x0  -> [+0x8]
 *
 * lruv_cache_new args: (name, capacity=0xa00, max_locked=6,
 *                       entry_size=0x800, delete_cb, query_cb)
 */
void rasterizer_decals_initialize(void)
{
  void *puVar1;
  void *iVar2;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x59, 1);
    system_exit(-1);
  }

  /* Allocate 12-byte D3D vertex buffer descriptor */
  puVar1 = debug_malloc(
    0xc, 0, "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c",
    0x5b);
  *(void **)0x476ad8 = puVar1;

  if (puVar1 == 0) {
    display_assert(
      "local_d3d_vertex_buffer",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x5c, 1);
    system_exit(-1);
    /* NOTE: original continues below after assert (EAX re-loaded at 0x15b72c)
     * to set the first field in the struct - preserved as-is */
    puVar1 = *(void **)0x476ad8;
  }

  /* Set D3D resource header word to 1 at +0x0 */
  *(unsigned int *)puVar1 = 1;

  /* Allocate 0x28000 bytes of GPU memory for "decal vertices" */
  iVar2 = game_state_gpu_alloc("decal vertices", 0, 0x28000);

  /* Store GPU pointer at +0x4 in the vertex buffer struct */
  *(void **)((char *)puVar1 + 0x4) = iVar2;

  /* Clear lock flags at +0x8 */
  *(unsigned int *)((char *)*(void **)0x476ad8 + 0x8) = 0;

  if (iVar2 == 0) {
    display_assert(
      "local_d3d_vertex_buffer->Data",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x60, 1);
    system_exit(-1);
  }

  /* Register the vertex buffer with D3D (stdcall, 2 args) */
  D3DResource_Register(*(void **)0x476ad8, 0);

  /* Create the LRUV vertex cache */
  *(void **)0x476adc =
    lruv_cache_new("decal vertex cache", 0xa00, 6, 0x800, FUN_0015afa0,
                   rasterizer_decals_vertex_cache_query);

  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x6a, 1);
    system_exit(-1);
  }
}

/* 0x15b7e0
 *
 * rasterizer_decals_dispose
 *
 * Tears down the decal rasterizer subsystem:
 *   1. Asserts vertex cache, vertex buffer, and D3D device are all non-null.
 *   2. Releases the D3D vertex buffer if non-null, then clears the pointer.
 *   3. Disposes the LRUV vertex cache (frees all entries and cache struct).
 */
void rasterizer_decals_dispose(void)
{
  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x99, 1);
    system_exit(-1);
  }
  if (*(void **)0x476ad8 == 0) {
    display_assert(
      "local_d3d_vertex_buffer",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x9a, 1);
    system_exit(-1);
  }
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x9b, 1);
    system_exit(-1);
  }
  if (*(void **)0x476ad8 != 0) {
    D3DResource_Release(*(void **)0x476ad8);
    *(void **)0x476ad8 = 0;
  }
  lruv_cache_dispose(*(void **)0x476adc);
}

/* 0x15b890
 *
 * Lock a region of the decal vertex buffer for writing.
 * Asserts that cache_index, vertex cache, and D3D device are all valid.
 * Translates the LRUV cache block index into a byte offset via
 * lruv_block_get_address, then locks that region of the D3D vertex buffer. Sets
 * the GPU lock flag (0x325652) to 5 during the lock operation, clears it
 * afterward. Returns a pointer to the locked vertex buffer memory. */
void *FUN_0015b890(int cache_index, uint32_t cache_size)
{
  uint32_t offset;
  void *locked_data;

  locked_data = 0;

  if (cache_index == -1) {
    display_assert(
      "cache_index!=NONE",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0xd9, 1);
    system_exit(-1);
  }

  if (*(void **)0x476adc == 0) {
    display_assert(
      "local_vertex_cache",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0xda, 1);
    system_exit(-1);
  }

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0xdb, 1);
    system_exit(-1);
  }

  {
    uint32_t (*lruv_cache_block_offset)(void *, int) = (void *)0x11da00;
    offset = lruv_cache_block_offset(*(void **)0x476adc, cache_index);
  }

  *(uint16_t *)0x325652 = 5;
  ((void(__stdcall *)(void *, uint32_t, uint32_t, void **, uint32_t))0x1ef100)(
    *(void **)0x476ad8, offset, cache_size, &locked_data, 0x80);
  *(uint16_t *)0x325652 = 0;

  return locked_data;
}

/*
 * FUN_0015bc40 — render every decal in one cluster/layer chain.
 *
 * Fetches the decal list head via FUN_00098fe0(cluster_index, current_layer),
 * then walks the singly-linked list (link at decal+0x34, -1 terminates). For
 * each decal it re-programs the framebuffer blend function + texture-combiner
 * state and rebinds the decal bitmap only when they change from the cached
 * values, then emits the decal's quad batch via D3D.
 *
 * Register roles in the original: EBX = current decal datum index, EDI = decal
 * datum pointer, ESI = 'deca' tag pointer (ESI += 0xbc after reading the blend
 * function at +0xc0; C keeps tag-base-relative offsets +0xc0/+0xe4).
 *
 * Render-state globals (raw addresses):
 *   0x476ab0 global_d3d_device; 0x3256bc word skip-flag; 0x3256cd byte enable;
 *   0x476ac8 word current layer; 0x476ad4 word cached blend function;
 *   0x1fb7a4 alpha-test render-state cache; 0x5a5b48/0x5a5b4c/0x5a5ac4
 *   texture-combiner dwords; 0x5a5ac0 pixel-shader state; 0x3256ba word stats
 *   mode (==2 accumulates); 0x5a5458/0x5a545c/0x5a5454/0x5a5450/0x5a544c stat
 *   counters; 0x476acc int cached bitmap; 0x476ad0 word cached frame index
 *   (sign-extended byte); 0x476adc lruv vertex-cache handle; 0x5aa8b8 decal
 *   data array pointer.
 *
 * Decal datum offsets: +0x1b s8 frame index; +0x24 u32 ARGB color; +0x28 u8
 *   intensity/alpha; +0x2a s16 vertex count; +0x2c s32 'deca' tag index;
 *   +0x34 s32 next-in-cluster link.
 * Decal tag offsets: +0xc0 u16 framebuffer blend function; +0xe4 s32 bitmap.
 *
 * Every assert path is display_assert(...); system_exit(-1); (confirmed from
 * the pristine XBE: each site calls 0x8d9f0 then 0x8e2f0 — there is NO
 * halt_and_catch_fire call, contrary to the Ghidra draft).
 *
 * 0x15bc40 / rasterizer_decals.obj
 */
void FUN_0015bc40(int rendered_cluster_data)
{
  int decal_index;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x198, 1);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256cd != 0) {
    decal_index =
      FUN_00098fe0((int16_t)rendered_cluster_data, *(int16_t *)0x476ac8);
    while (decal_index != -1) {
      char *decal;
      char *tag;
      uint16_t blend;
      int bitmap_index;
      uint32_t vertex_data_offset;
      uint32_t color;
      uint32_t intensity;

      decal = (char *)datum_get(*(void **)0x5aa8b8, decal_index);
      tag = (char *)tag_get(0x64656361, *(int *)(decal + 0x2c));

      blend = *(uint16_t *)(tag + 0xc0);
      if (*(uint16_t *)0x476ad4 != blend) {
        *(uint16_t *)0x476ad4 = blend;
        if (blend == 1 || blend == 2) {
          D3DDevice_SetRenderState_Simple(0x40358, 0x1010101);
          *(uint32_t *)0x1fb7a4 = 0x1010101;
        } else {
          D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
          *(uint32_t *)0x1fb7a4 = 0x10101;
        }
        switch (*(uint16_t *)0x476ad4) {
        case 0:
          *(uint32_t *)0x5a5b48 = 0x8040000;
          *(uint32_t *)0x5a5b4c = 0x200c0000;
          *(uint32_t *)0x5a5ac4 = 0x34180000;
          break;
        case 1:
        case 5:
          *(uint32_t *)0x5a5b48 = 0x28240820;
          *(uint32_t *)0x5a5b4c = 0x340c1420;
          *(uint32_t *)0x5a5ac4 = 0x341c1420;
          break;
        case 2:
          *(uint32_t *)0x5a5b48 = 0xa8240820;
          *(uint32_t *)0x5a5b4c = 0x340c14a0;
          *(uint32_t *)0x5a5ac4 = 0x341c14a0;
          break;
        case 3:
        case 4:
        case 6:
          /* NOTE: cases 3/4/6 deliberately leave 0x5a5ac4 unchanged. */
          *(uint32_t *)0x5a5b48 = 0x8040000;
          *(uint32_t *)0x5a5b4c = 0x340c0000;
          break;
        case 7:
          *(uint32_t *)0x5a5b48 = 0x8040000;
          *(uint32_t *)0x5a5b4c = 0x340c0000;
          *(uint32_t *)0x5a5ac4 = 0x34180000;
          break;
        default:
          display_assert(
            "### ERROR unsupported framebuffer blend function",
            "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c",
            0x1d6, 1);
          system_exit(-1);
        }
        FUN_001580b0(*(uint16_t *)0x476ad4);
        rasterizer_set_pixel_shader((void *)0x5a5ac0);
        if (*(uint16_t *)0x3256ba == 2) {
          *(int *)0x5a5458 += 1;
        }
      }

      bitmap_index = *(int *)(tag + 0xe4);
      if (*(int *)0x476acc != bitmap_index ||
          *(int16_t *)0x476ad0 != *(int8_t *)(decal + 0x1b)) {
        *(int16_t *)0x476ad0 = (int16_t) * (int8_t *)(decal + 0x1b);
        *(int *)0x476acc = bitmap_index;
        rasterizer_set_texture(0, 0, 1, bitmap_index, *(int16_t *)0x476ad0);
        if (*(uint16_t *)0x3256ba == 2) {
          *(int *)0x5a545c += 1;
        }
      }

      vertex_data_offset =
        lruv_block_get_address(*(void **)0x476adc, decal_index);
      /* Load the ARGB color dword once; the original reuses it for both the
       * intensity scale (color>>24 = alpha) and the vertex color argument. */
      color = *(uint32_t *)(decal + 0x24);
      intensity =
        ((uint32_t) * (uint8_t *)(decal + 0x28) * (color >> 0x18) + 0x7f) >> 8;
      if (intensity > 0xff) {
        display_assert(
          "intensity<=PIXEL32_COMPONENT_MASK",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x1fe,
          1);
        system_exit(-1);
      }
      if ((vertex_data_offset & 0xf) != 0) {
        display_assert(
          "vertex_data_offset%sizeof(struct decal_vertex)==0",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x1ff,
          1);
        system_exit(-1);
      }

      D3DDevice_SetVertexData4ub(9, color >> 16, color >> 8, color,
                                 0xff - intensity);
      D3DDevice_DrawVertices(8, vertex_data_offset >> 4,
                             (uint32_t)((int)*(int16_t *)(decal + 0x2a) << 2));

      if (*(uint16_t *)0x3256ba == 2) {
        *(int *)0x5a5454 += 1;
        *(int *)0x5a5450 += *(int16_t *)(decal + 0x2a) * 2;
        *(int *)0x5a544c += *(int16_t *)(decal + 0x2a) * 4;
      }

      decal_index = *(int *)(decal + 0x34);
    }
  }
}

/*
 * rasterizer_detail_objects_initialize @ 0x15c2d0
 *
 * Real TU: c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_detail_objects.c
 * (__FILE__ assert xref, confirmed). kb.json groups it under
 * rasterizer_decals.obj, so it lives here for compile/verify locality.
 *
 * Allocates the shared dynamic vertex buffer used to draw detail objects.
 * Asserts the global D3D device exists, then creates a 0x20000-byte vertex
 * buffer (= RASTERIZER_MAXIMUM_DETAIL_OBJECTS_PER_FRAME *
 * NUMBER_OF_VERTICES_PER_QUADRILATERAL * sizeof(struct detail_object_vertex))
 * with usage D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY (0x208), FVF 0, pool
 * D3DPOOL_DEFAULT (0), storing the pointer at the module-static
 * local_d3d_vertex_buffer (0x476ae4). Returns true on success, false on
 * failure (HRESULT test `hr >= 0` == SUCCEEDED(hr), original JL at 0x15c310).
 *
 * Globals (hardcoded, not in kb.json):
 *   0x476ab0  void *  - global_d3d_device (IDirect3DDevice8 pointer)
 *   0x476ae4  void *  - local_d3d_vertex_buffer (detail-object dynamic VB)
 *
 * 0x1ef0a0 D3DDevice_CreateVertexBuffer is a __stdcall D3D8 import: 5 stack
 * args, HRESULT in EAX, callee-cleans (no ADD ESP after the CALL).
 */
/* 0x15c2d0 */
char FUN_0015c2d0(void)
{
  int hr;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_detail_objects.c",
      0x62, 1);
    system_exit(-1);
  }

  hr = D3DDevice_CreateVertexBuffer(0x20000, 0x208, 0, 0, (void **)0x476ae4);
  if (hr >= 0) {
    return 1;
  }

  FUN_00167ff0(
    hr,
    "IDirect3DDevice8_CreateVertexBuffer(global_d3d_device, "
    "RASTERIZER_MAXIMUM_DETAIL_OBJECTS_PER_FRAME*NUMBER_OF_VERTICES_PER_"
    "QUADRILATERAL"
    "*sizeof(struct detail_object_vertex), RASTERIZER_DYNAMIC_BUFFER_USAGE, 0, "
    "RASTERIZER_DYNAMIC_BUFFER_POOL, &local_d3d_vertex_buffer)");
  error(2, "### ERROR rasterizer_detail_objects_initialize failed");
  return 0;
}

/*
 * rasterizer_detail_objects_dispose @ 0x15c680
 *
 * Real TU: c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_detail_objects.c
 * (__FILE__ assert xref). Disposes the detail-objects dynamic vertex buffer:
 * asserts both the vertex buffer (0x476ae4) and the global D3D device
 * (0x476ab0) are non-NULL, then releases the vertex buffer and clears its
 * pointer. The device assert is purely an invariant check; the device is not
 * otherwise touched here.
 *
 * The trailing `!= NULL` guard on the vertex buffer is effectively always
 * true on the non-assert path (the first assert already proved it non-NULL)
 * but is preserved to match the original control-flow shape.
 *
 * Assert-terminal is PUSH -1; CALL 0x8e2f0 = system_exit(-1) at BOTH sites
 * (0x15c69e, 0x15c6c4) — NOT halt_and_catch_fire (review-gate REJECT on the
 * first lift attempt was exactly this substitution).
 *
 * Globals (hardcoded, not in kb.json):
 *   0x476ae4  void *  - local_d3d_vertex_buffer (detail-object dynamic VB)
 *   0x476ab0  void *  - global_d3d_device (IDirect3DDevice8 pointer)
 */
/* 0x15c680 */
void FUN_0015c680(void)
{
  if (*(void **)0x476ae4 == (void *)0x0) {
    display_assert(
      "local_d3d_vertex_buffer",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_detail_objects.c",
      0x77, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_detail_objects.c",
      0x78, 1);
    system_exit(-1);
  }
  if (*(void **)0x476ae4 != (void *)0x0) {
    D3DResource_Release(*(void **)0x476ae4);
    *(void **)0x476ae4 = (void *)0x0;
  }
}

/* 0x15c6f0
 *
 * rasterizer_detail_objects_begin  (set up D3D render state for the
 * detail-objects draw pass, then bind the vertex-shader constants, the
 * detail-object pixel-shader command buffer, and the dynamic vertex buffer).
 *
 * Cdecl void(void).  Real TU (from the assert __FILE__):
 *   c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_detail_objects.c
 * (linker grouped this fn into rasterizer_decals.obj alongside
 * rasterizer_detail_objects_initialize @ 0x15c2d0).
 *
 * Gated on the detail-objects enable byte (@0x3256dc) and single-window mode
 * (main_get_window_count() <= 1).  Two guard asserts confirm the D3D device
 * and dynamic vertex buffer are live, then the fixed render-state block is
 * issued.  Each D3DDevice_SetRenderState_Simple(token,value) is mirrored by a
 * write to its render-state cache global (DAT_001fb7xx = value); preserve both.
 *
 * Call-site notes (verified against disassembly, NOT the decompiler):
 *  - The vertex-shader-constant upload is
 *      D3DDevice_SetVertexShaderConstant(-0x51, &vs[0], 6)
 *    -> Register=-0x51, ConstantCount=6 (six vec4 = 24 dwords).  The
 *    decompiler mis-reported the count as 0x3b800000 (that value is actually
 *    vs[0], the first dword stored into the constant block).  vs[7] is a raw
 *    integer copy of *(int*)0x325708 (MOV EAX,[0x325708]; MOV [ebp-0x44],EAX),
 *    so the block is typed as unsigned int to keep every store a plain MOV imm
 *    / MOV reg and avoid FLD/FSTP float codegen.
 *  - The assert failure path is display_assert(...,1) then system_exit(-1)
 *    (CALL 0x8e2f0), NOT halt_and_catch_fire.
 *
 * Globals (used by address, not in kb.json):
 *   0x3256dc  bool      detail-objects enable flag
 *   0x476ae4  void *    local_d3d_vertex_buffer (dynamic detail-object VB)
 *   0x476ab0  void *    global_d3d_device
 *   0x325708  int       constant-block dword (copied into vs[7])
 *   0x1fb7xx  uint32_t  render-state cache mirrors
 *   0x5a5ac0  0xf0 byte detail-object pixel-shader command buffer
 */
void FUN_0015c6f0(void)
{
  unsigned int vs[24]; /* ebp-0x60..ebp-0x4: 6 vec4 vertex-shader constants */

  FUN_0016f910(0x15); /* profile begin, id 0x15 */

  if (*(char *)0x3256dc == 0) {
    return;
  }
  if (main_get_window_count() > 1) {
    return;
  }

  if (*(void **)0x476ae4 == 0) {
    display_assert(
      "local_d3d_vertex_buffer",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_detail_objects.c",
      0x88, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_detail_objects.c",
      0x89, 1);
    system_exit(-1);
  }

  /* Fixed render-state block (each Simple call mirrored by its cache global).
   */
  D3DDevice_SetRenderState_CullMode(0);
  D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
  *(uint32_t *)0x1fb7a4 = 0x10101;
  D3DDevice_SetRenderState_Simple(0x40304, 1);
  *(uint32_t *)0x1fb784 = 1;
  D3DDevice_SetRenderState_Simple(0x40344, 0x302);
  *(uint32_t *)0x1fb790 = 0x302;
  D3DDevice_SetRenderState_Simple(0x40348, 0x303);
  *(uint32_t *)0x1fb794 = 0x303;
  D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
  *(uint32_t *)0x1fb7c0 = 0x8006;
  D3DDevice_SetRenderState_Simple(0x40300, 0);
  *(uint32_t *)0x1fb788 = 0;
  D3DDevice_SetRenderState_ZEnable(1);
  D3DDevice_SetRenderState_Simple(0x40354, 0x203);
  *(uint32_t *)0x1fb77c = 0x203;
  D3DDevice_SetRenderState_Simple(0x4035c, 0);
  *(uint32_t *)0x1fb798 = 0;
  D3DDevice_SetRenderState_ZBias(0);

  /* Six vec4 vertex-shader constants (raw dword bit patterns; vs[7] is a plain
   * integer copy of *(int*)0x325708). */
  vs[0] = 0x3b800000;
  vs[1] = 0x41800000;
  vs[2] = 0x44074000;
  vs[3] = 0x4424c000;
  vs[4] = 0x41000000;
  vs[5] = 0x41000000;
  vs[6] = 0x41000000;
  vs[7] = *(unsigned int *)0x325708;
  vs[8] = 0x3f800000;
  vs[9] = 0x3f800000;
  vs[10] = 0x3f000000;
  vs[11] = 0;
  vs[12] = 0;
  vs[13] = 0x3f800000;
  vs[14] = 0xbf000000;
  vs[15] = 0;
  vs[16] = 0;
  vs[17] = 0;
  vs[18] = 0xbf000000;
  vs[19] = 0x3f800000;
  vs[20] = 0x3f800000;
  vs[21] = 0;
  vs[22] = 0x3f000000;
  vs[23] = 0x3f800000;
  D3DDevice_SetVertexShaderConstant(-0x51, vs, 6);

  /* Detail-object pixel-shader command buffer (0xf0 bytes @ 0x5a5ac0). */
  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b98 = 1;
  *(uint32_t *)0x5a5b94 = 1;
  *(uint32_t *)0x5a5b48 = 0x8040000;
  *(uint32_t *)0x5a5b74 = 0xc0;
  *(uint32_t *)0x5a5ac0 = 0x18140000;
  *(uint32_t *)0x5a5b28 = 0xc0;
  *(uint32_t *)0x5a5ae0 = 0xc;
  *(uint32_t *)0x5a5ae4 = 0x1c00;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);

  D3DDevice_SetStreamSource(0, *(void **)0x476ae4, 8);
}
/*
 * rasterizer_xbox_draw_primitives.c
 *
 * Xbox rasterizer dynamic-primitive submission.  The linker groups this TU
 * into rasterizer_decals.obj; the __FILE__ used by its asserts is
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_draw_primitives.c
 * (confirmed from the pristine XBE assert xref).
 *
 * Globals (used by address, not in kb.json):
 *   0x476ab0  void *  - global_d3d_device (asserted non-NULL)
 *   0x47dbe8  void *  - dynamic_triangles.d3d_index_buffer (asserted non-NULL)
 *   0x47dbe0  int     - dynamic_triangles sorted-record count (capped < 0x3ff)
 *   0x47dbe4  int     - dynamic_triangles running index/vertex cursor (<
 * 0x8000) 0x47abe0  int[]   - sorted-record array, stride 0xc bytes: +0
 * start_offset (index cursor snapshot), +4 count 0x47dbf4  char    - one-shot
 * "too many dynamic triangles" warning latch 0x3256ba  char    - render-stat
 * mode (==2 -> accumulate submission stats) 0x5a5538  int     - stat:
 * accumulated dynamic-triangle count 0x5a553c  int     - stat: accumulated
 * dynamic-triangle batch count
 */

static const char kDrawPrimitivesFile[] =
  "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_draw_primitives.c";

/* 0x15d170
 *
 * Append a run of `count` dynamic triangles to the sorted-submission table.
 * Records {start_offset = current vertex/index cursor, count} at
 * dynamic_triangles.sorted[record_count], then advances the vertex cursor by
 * `count` and the record count by 1.  Guards: the cursor must stay below
 * 0x8000 and the record count below 0x3ff; on overflow a one-shot error is
 * emitted via the 0x47dbf4 latch.  When render-stat mode (0x3256ba) is 2 the
 * submission is also accumulated into the stat counters.
 *
 * cdecl, one int stack arg at [esp+4] (Ghidra draft mis-typed this void(void)
 * with in_stack_00000004; it is a plain cdecl stack param, NOT a register arg).
 * Returns the advanced vertex cursor, or -1 on the count<=0 / overflow paths
 * (edi is seeded to -1 in the prologue and reused as the cursor accumulator;
 * the pristine XBE returns it via `mov eax,edi` at the early-exit merge).
 *
 * NB: every assert path is display_assert(...); system_exit(-1); (confirmed
 * from the pristine XBE: each site pushes -1 and calls 0x8e2f0 with a combined
 * `add esp,0x14` — there is NO halt_and_catch_fire call, contrary to the
 * Ghidra draft).  The 0x3256ba stat-mode gate is a 16-bit compare
 * (`cmp word ptr [0x3256ba],2`).
 *
 * 0x15d170 / rasterizer_decals.obj
 */
int FUN_0015d170(int count)
{
  int result;

  result = -1;
  if (count < 0) {
    display_assert("count>=0", kDrawPrimitivesFile, 0x11c, 1);
    system_exit(-1);
  }
  if (*(int *)0x47dbe8 == 0) {
    display_assert("dynamic_triangles.d3d_index_buffer", kDrawPrimitivesFile,
                   0x11d, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x11e, 1);
    system_exit(-1);
  }
  if (0 < count) {
    if ((*(int *)0x47dbe4 < 0x8000 - count) && (*(int *)0x47dbe0 < 0x3ff)) {
      *(int *)(0x47abe0 + *(int *)0x47dbe0 * 0xc) = *(int *)0x47dbe4;
      *(int *)(0x47abe4 + *(int *)0x47dbe0 * 0xc) = count;
      result = *(int *)0x47dbe4 + count;
      *(int *)0x47dbe4 = result;
      *(int *)0x47dbe0 = *(int *)0x47dbe0 + 1;
      if (*(short *)0x3256ba == 2) {
        *(int *)0x5a5538 = *(int *)0x5a5538 + count;
        *(int *)0x5a553c = *(int *)0x5a553c + 1;
      }
      return result;
    } else if (*(char *)0x47dbf4 == 0) {
      error(2,
            "### ERROR too many dynamic triangles requested from rasterizer");
      *(char *)0x47dbf4 = 1;
    }
  }
  return result;
}

/* 0x15d310
 *
 * Reserve a run of `count` dynamic vertices in group `type` of the
 * dynamic-vertex buffer.  Appends a {type, current_offset, count} record to
 * the reservation table at dynamic_vertices.records[record_count] (16-byte
 * stride), then advances that group's running vertex offset by `count` and the
 * record count by 1.  Guards: `count >= 0`, `0 <= type < 12`
 * (NUMBER_OF_RASTERIZER_VERTEX_TYPES), the group's D3D vertex buffer and the
 * global D3D device must both be non-NULL.  The reservation is skipped (with a
 * one-shot 0x47dbf5 error latch) when advancing would exceed the group's limit
 * or the record count would reach 0x3ff.  When render-stat mode (0x3256ba) is
 * 2 the reservation is also accumulated into the vertex stat counters.
 *
 * dynamic_vertices.groups[] is an array of 0x14-byte (5-int) records indexed by
 * type: +0 current_offset (0x476ae8), +4 limit (0x476aec), +0xc
 * d3d_vertex_buffer (0x476af4).  The reservation table is an array of 0x10-byte
 * records indexed by record_count: +0 short type (0x476bd8), +4 u32 offset
 * (0x476bdc), +8 int count (0x476be0).
 *
 * cdecl, two stack args: short type @[esp+4], int count @[esp+8] (Ghidra draft
 * mis-typed this void(void) with in_stack_00000004/00000008 -- they are plain
 * cdecl stack params, NOT register args).  void return.
 *
 * NB: like the sibling FUN_0015d170, every assert path is
 * display_assert(...); system_exit(-1); (the pristine XBE's combined
 * `add esp,0x14` after each site proves the second call takes one arg -- it is
 * system_exit(-1), NOT the arg-less halt_and_catch_fire the Ghidra draft
 * named). The 0x3256ba stat-mode gate is a 16-bit compare.
 *
 * 0x15d310 / rasterizer_decals.obj
 */
void FUN_0015d310(short type, int count)
{
  int iType;
  int rec;

  if (count < 0) {
    display_assert("count>=0", kDrawPrimitivesFile, 0x1aa, 1);
    system_exit(-1);
  }
  if (type < 0 || type > 0xb) {
    display_assert("type>=0 && type<NUMBER_OF_RASTERIZER_VERTEX_TYPES",
                   kDrawPrimitivesFile, 0x1ab, 1);
    system_exit(-1);
  }
  iType = type;
  if (*(int *)(0x476af4 + iType * 0x14) == 0) {
    display_assert("dynamic_vertices.groups[type].d3d_vertex_buffer",
                   kDrawPrimitivesFile, 0x1ad, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x1ae, 1);
    system_exit(-1);
  }
  if (0 < count) {
    if ((*(int *)(0x476ae8 + iType * 0x14) <
         *(int *)(0x476aec + iType * 0x14) - count) &&
        (*(int *)0x47abd8 < 0x3ff)) {
      FUN_00180050(type);
      rec = *(int *)0x47abd8 * 0x10;
      *(short *)(0x476bd8 + rec) = type;
      *(unsigned int *)(0x476bdc + rec) = *(int *)(0x476ae8 + iType * 0x14);
      *(int *)(0x476be0 + rec) = count;
      *(int *)(0x476ae8 + iType * 0x14) =
        *(int *)(0x476ae8 + iType * 0x14) + count;
      *(int *)0x47abd8 = *(int *)0x47abd8 + 1;
      if (*(short *)0x3256ba == 2) {
        *(int *)0x5a5530 = *(int *)0x5a5530 + count;
        *(int *)0x5a5534 = *(int *)0x5a5534 + 1;
        return;
      }
    } else if (*(char *)0x47dbf5 == 0) {
      error(2, "### ERROR too many dynamic vertices requested from rasterizer");
      *(char *)0x47dbf5 = 1;
    }
  }
  return;
}

/* 0x15d5b0
 *
 * Draw a batch of `primitive_count` dynamic primitives that were previously
 * reserved into group-buffer slot `dynamic_vertex_buffer_index` (an index into
 * the reservation table filled by FUN_0015d310).  Each primitive consumes
 * `vertices_per_primitive` vertices.  The D3D primitive type is selected from
 * vertices_per_primitive: 2->LINELIST(2), 3->TRIANGLELIST(5), 4->QUADLIST(8);
 * any other value is treated as a single fan/strip run (TRIANGLESTRIP, 6) with
 * primitive_count re-derived as vertices_per_primitive-2 (this branch asserts
 * primitive_count==1 and first_primitive_index==0).  The batch is emitted in
 * chunks of at most RASTERIZER_MAXIMUM_PRIMITIVES_PER_DRAW_COMMAND (0x2710)
 * primitives per DrawVertices call; first_primitive_index / primitive_count are
 * advanced/decremented across chunks (the pristine XBE mutates its own [EBP+8]
 * / [EBP+0xc] param slots -- modelled here as the mutable params themselves).
 *
 * The stream vertex size comes from FUN_00180050(group_index).  Group 6's
 * vertex buffer is overridden to the scratch buffer at 0x47dbf0 when
 * (*(int *)0x325668 & 1) != 0 (verified against disasm 0x15d755-0x15d771 --
 * this is the OPPOSITE polarity to a naive reading; the scratch buffer is used
 * only when the low bit is SET).  DrawVertices' vertex count is
 * primtype_table[type].mul * clamped_primitive_count + primtype_table[type].add
 * and its start vertex is vertices_per_primitive*first_primitive_index +
 * dynamic_vertex_buffer->vertex_start_index.
 *
 * local_5 / bl are a two-phase debug HRESULT-check toggle (both initialised to
 * 1 and mirrored around the two D3D calls -- FUN_00167ff0 only fires when the
 * toggle is 0, which never happens in the retail path).  Preserved verbatim for
 * codegen fidelity; do not fold to a plain bool.  On a failed toggle the tail
 * calls error(2,"### ERROR rasterizer_draw_dynamic_vertices failed").
 *
 * cdecl, four stack args (Ghidra draft mis-typed this void(void) with
 * in_stack_* reassignments -- they are plain cdecl params, NOT register args).
 * Every assert path is display_assert(...); system_exit(-1); (the combined
 * `add esp,0x14` after each site proves the second call takes one arg).
 *
 * 0x15d5b0 / rasterizer_decals.obj
 */
void rasterizer_draw_dynamic_vertices(int first_primitive_index,
                                      int primitive_count,
                                      int dynamic_vertex_buffer_index,
                                      short vertices_per_primitive)
{
  int d3d_primitive_type;
  int vertex_size;
  int vpp_int;
  int local_primitive_count;
  int record;
  int group;
  void *d3d_vertex_buffer;
  char local_5;
  char bl;

  local_5 = 1;
  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device", kDrawPrimitivesFile, 0x28b, 1);
    system_exit(-1);
  }
  if (0 < primitive_count) {
    do {
      if (dynamic_vertex_buffer_index == -1) {
        break;
      }
      if (dynamic_vertex_buffer_index < 0) {
        display_assert("dynamic_vertex_buffer_index>=0", kDrawPrimitivesFile,
                       0x29a, 1);
        system_exit(-1);
      }
      if (dynamic_vertex_buffer_index >= *(int *)0x47abd8) {
        display_assert(
          "dynamic_vertex_buffer_index<dynamic_vertices.buffer_count",
          kDrawPrimitivesFile, 0x29b, 1);
        system_exit(-1);
      }

      vpp_int = vertices_per_primitive;
      switch (vertices_per_primitive) {
      case 2:
        d3d_primitive_type = 2; /* D3DPT_LINELIST */
        break;
      case 3:
        d3d_primitive_type = 5; /* D3DPT_TRIANGLELIST */
        break;
      case 4:
        d3d_primitive_type = 8; /* D3DPT_QUADLIST */
        break;
      default:
        if (primitive_count != 1) {
          display_assert("primitive_count==1", kDrawPrimitivesFile, 0x2aa, 1);
          system_exit(-1);
        }
        primitive_count = vertices_per_primitive - 2;
        if (first_primitive_index != 0) {
          display_assert("first_primitive_index==0", kDrawPrimitivesFile, 0x2ae,
                         1);
          system_exit(-1);
        }
        if (vertices_per_primitive > 0x2710) {
          display_assert("vertices_per_primitive<=RASTERIZER_MAXIMUM_PRIMITIVES"
                         "_PER_DRAW_COMMAND",
                         kDrawPrimitivesFile, 0x2af, 1);
          system_exit(-1);
        }
        d3d_primitive_type = 6; /* D3DPT_TRIANGLESTRIP */
        break;
      }

      record = 0x476bd8 + dynamic_vertex_buffer_index * 0x10;
      vertex_size = FUN_00180050(*(short *)record);
      group = 0x476ae8 + *(short *)record * 0x14;
      if (group == 0) {
        display_assert("group", kDrawPrimitivesFile, 0x1f8, 1);
        system_exit(-1);
      }

      if (group == 0x476b60) {
        d3d_vertex_buffer = *(void **)0x47dbf0;
        if ((*(int *)0x325668 & 1) == 0) {
          d3d_vertex_buffer = *(void **)(group + 0xc);
        }
      } else {
        d3d_vertex_buffer = *(void **)(group + 0xc);
      }
      if (d3d_vertex_buffer == 0) {
        display_assert("d3d_vertex_buffer", kDrawPrimitivesFile, 0x2bd, 1);
        system_exit(-1);
      }

      if (*(int *)(record + 4) < 0) {
        display_assert("dynamic_vertex_buffer->vertex_start_index>=0",
                       kDrawPrimitivesFile, 0x2c0, 1);
        system_exit(-1);
      }
      if (*(int *)(record + 4) > *(int *)group - *(int *)(record + 8)) {
        display_assert("dynamic_vertex_buffer->vertex_start_index<=group->"
                       "vertex_count - dynamic_vertex_buffer->vertex_count",
                       kDrawPrimitivesFile, 0x2c1, 1);
        system_exit(-1);
      }

      local_primitive_count = primitive_count;
      if (local_primitive_count > 0x2710) {
        local_primitive_count = 0x2710;
      }

      D3DDevice_SetStreamSource(0, d3d_vertex_buffer, (uint32_t)vertex_size);
      if (local_5 != 0) {
        bl = 1;
      } else {
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetStreamSource(global_d3d_device, 0, "
                     "d3d_vertex_buffer, vertex_size)");
        bl = 0;
      }

      D3DDevice_DrawVertices(
        (uint32_t)d3d_primitive_type,
        (uint32_t)(vpp_int * first_primitive_index + *(int *)(record + 4)),
        (uint32_t)(*(int *)(0x29f7e8 + d3d_primitive_type * 8) *
                     local_primitive_count +
                   *(int *)(0x29f7ec + d3d_primitive_type * 8)));
      if (bl != 0) {
        local_5 = 1;
      } else {
        FUN_00167ff0(0,
                     "IDirect3DDevice8_DrawPrimitive(global_d3d_device, "
                     "d3d_primitive_type, first_primitive_index*vertices_per_"
                     "primitive + dynamic_vertex_buffer->vertex_start_index, "
                     "local_primitive_count)");
        local_5 = 0;
      }

      first_primitive_index = first_primitive_index + local_primitive_count;
      primitive_count = primitive_count - local_primitive_count;
    } while (0 < primitive_count);

    if (local_5 == 0) {
      error(2, "### ERROR rasterizer_draw_dynamic_vertices failed");
    }
  }
}
