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

#include "x87_math.h"

/* Forward declarations for callbacks passed to lruv_cache_new.
 * FUN_0015afa0 is the eviction callback; FUN_0015b0c0 is the lock-query
 * callback (both ported at their original addresses). */
void FUN_0015afa0(int decal_index);
bool FUN_0015b0c0(int decal_index);

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

/* D3DVIEWPORT8 mirror (0x18 bytes; viewport buffer at EBP-0x18 in
 * FUN_00158140). */
typedef struct {
  unsigned int X; /* +0x00 */
  unsigned int Y; /* +0x04 */
  unsigned int Width; /* +0x08 */
  unsigned int Height; /* +0x0c */
  float MinZ; /* +0x10 */
  float MaxZ; /* +0x14 */
} d3d_viewport_t;

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

/* 0x158140
 *
 * FUN_00158140 — select a render-target surface, bind it (with optional
 * z-buffer), set the viewport to cover it, and optionally clear.
 *
 * Switch cases mirror FUN_001584f0's render-target table one dword later:
 * per-target D3D *surface* headers at 0x476a5c..0x476aac (vs the texture
 * headers at 0x476a54..0x476aa8).  Assert reasons: "mipmap_index==0" per
 * case, "d3d_surface" when the selected surface header is NULL,
 * "!zbuffer||d3d_surface_z" on the shared apply path, and "### ERROR
 * unsupported rasterizer target" for the default case.  __FILE__ is
 * rasterizer_xbox.c (original TU; linker grouped into rasterizer_decals.obj).
 *
 * Globals (used by address, not in kb.json):
 *   0x476a5c  void*    target 0 d3d_surface (backbuffer)
 *   0x476a60  void*    target 0 d3d_surface_z (shared z fallback)
 *   0x476a6c  void*    target 1 d3d_surface
 *   0x476a70  void*    target 1 d3d_surface_z (falls back to 0x476a60)
 *   0x476a78  void*    target 2 d3d_surface
 *   0x476a80  void*    target 3 d3d_surface
 *   0x476a88  void*    target 4 d3d_surface
 *   0x476a90  void*    target 5 d3d_surface
 *   0x476a98  void*[4] target 6 (water) per-mip d3d_surface array
 *   0x476aac  void*    target 7 d3d_surface
 *   0x5a5bf4  short    target-0 viewport top    (Y)
 *   0x5a5bf6  short    target-0 viewport left   (X)
 *   0x5a5bf8  short    target-0 viewport bottom (Y + Height)
 *   0x5a5bfa  short    target-0 viewport right  (X + Width)
 *
 * Call-site facts from the delinked reference (00158140.obj):
 *   - D3DDevice_SetRenderTarget(surface@ESI, depth@EBX) where EBX is the
 *     branchless select `neg bl; sbb ebx,ebx; and ebx,edi` =
 *     (zbuffer ? d3d_surface_z : 0) — written as a plain ternary here.
 *   - target != 0: D3DSurface_GetDesc(surface, &desc at EBP-0x34); viewport
 *     X/Y = 0, Width/Height = desc +0x14/+0x18.
 *   - target == 0: viewport X/Y/W/H from the four movsx'd shorts above
 *     (Ghidra resolved no args for either call; pushes are in the disasm).
 *   - D3DDevice_SetViewport takes ONE arg (LEA ECX,[EBP-0x18]; PUSH ECX) —
 *     the kb.json placeholder decl `void(void)` was corrected to match.
 *   - clear flags: 0xF3 for targets 0/1 (color+z+stencil), else 0xF0;
 *     D3DDevice_Clear(0, NULL, flags, color, 1.0f literal, 0).
 *
 *   target       - render-target index (switch on short)
 *   mipmap_index - water target mip level (short; must be 0 elsewhere)
 *   color        - D3D clear color (raw uint)
 *   do_clear     - char bool: issue D3DDevice_Clear
 *   zbuffer      - char bool: bind the target's z surface
 */
void FUN_00158140(int target, int mipmap_index, uint32_t color, int do_clear,
                  int zbuffer)
{
  void *d3d_surface;
  void *d3d_surface_z;
  d3d_surface_desc_t desc;
  d3d_viewport_t viewport;
  short water_mip;
  short target16;
  char zb;
  uint32_t flags;

  d3d_surface = 0;
  d3d_surface_z = 0;

  switch ((short)target) {
  case 0:
    if ((short)mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8e8, 1);
      system_exit(-1);
    }
    d3d_surface = *(void **)0x476a5c;
    d3d_surface_z = *(void **)0x476a60;
    if (d3d_surface == 0) {
      display_assert("d3d_surface",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8ec, 1);
      system_exit(-1);
    }
    break;
  case 1:
    if ((short)mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8ef, 1);
      system_exit(-1);
    }
    d3d_surface_z = *(void **)0x476a70;
    d3d_surface = *(void **)0x476a6c;
    if (d3d_surface_z == 0) {
      d3d_surface_z = *(void **)0x476a60;
    }
    if (d3d_surface == 0) {
      display_assert("d3d_surface",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8f2, 1);
      system_exit(-1);
    }
    break;
  case 2:
    if ((short)mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8f5, 1);
      system_exit(-1);
    }
    d3d_surface = *(void **)0x476a78;
    if (d3d_surface == 0) {
      display_assert("d3d_surface",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8f7, 1);
      system_exit(-1);
    }
    break;
  case 3:
    if ((short)mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8fa, 1);
      system_exit(-1);
    }
    d3d_surface = *(void **)0x476a80;
    if (d3d_surface == 0) {
      display_assert("d3d_surface",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8fc, 1);
      system_exit(-1);
    }
    break;
  case 4:
    if ((short)mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8ff, 1);
      system_exit(-1);
    }
    d3d_surface = *(void **)0x476a88;
    if (d3d_surface == 0) {
      display_assert("d3d_surface",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x901, 1);
      system_exit(-1);
    }
    break;
  case 5:
    if ((short)mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x904, 1);
      system_exit(-1);
    }
    d3d_surface = *(void **)0x476a90;
    if (d3d_surface == 0) {
      display_assert("d3d_surface",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x906, 1);
      system_exit(-1);
    }
    break;
  case 6:
    water_mip = (short)mipmap_index;
    if (water_mip < 0 || water_mip >= 4) {
      display_assert("mipmap_index>=0 && "
                     "mipmap_index<RASTERIZER_TARGET_WATER_MAX_MIPMAP_LEVELS",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x909, 1);
      system_exit(-1);
    }
    d3d_surface = ((void **)0x476a98)[water_mip];
    if (d3d_surface == 0) {
      display_assert("d3d_surface",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x90b, 1);
      system_exit(-1);
    }
    break;
  case 7:
    if ((short)mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x91b, 1);
      system_exit(-1);
    }
    d3d_surface = *(void **)0x476aac;
    if (d3d_surface == 0) {
      display_assert("d3d_surface",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x91d, 1);
      system_exit(-1);
    }
    break;
  default:
    display_assert("### ERROR unsupported rasterizer target",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x920, 1);
    system_exit(-1);
    break;
  }

  zb = (char)zbuffer;
  if (zb != 0 && d3d_surface_z == 0) {
    display_assert("!zbuffer||d3d_surface_z",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x924, 1);
    system_exit(-1);
  }

  D3DDevice_SetRenderTarget(d3d_surface, zb != 0 ? d3d_surface_z : 0);

  target16 = *(short *)&target;
  if (target16 == 0) {
    viewport.X = *(short *)0x5a5bf6;
    viewport.Y = *(short *)0x5a5bf4;
    viewport.Width = *(short *)0x5a5bfa - *(short *)0x5a5bf6;
    viewport.Height = *(short *)0x5a5bf8 - *(short *)0x5a5bf4;
  } else {
    D3DSurface_GetDesc(d3d_surface, &desc);
    viewport.X = 0;
    viewport.Y = 0;
    viewport.Width = desc.Width;
    viewport.Height = desc.Height;
  }
  viewport.MinZ = 0.0f;
  viewport.MaxZ = 1.0f;
  D3DDevice_SetViewport(&viewport);

  if ((char)do_clear != 0) {
    flags = 0xf0;
    if (target16 == 0 || target16 == 1) {
      flags = 0xf3;
    }
    D3DDevice_Clear(0, 0, flags, color, 1.0f, 0);
  }
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

/* 0x158800
 *
 * rasterizer_decal_fullscreen_pass  (composite decal buffer to the screen)
 *
 * Gated blit that draws one full-screen quad covering the screen-bounds rect
 * `bounds` (a uint16[4] = {x0, y0, x1, y1}) using a fixed vertex/pixel shader.
 * The pass only runs when the decal-composite feature byte 0x325703 is set AND
 * the busy word 0x5a5bc0 is zero; otherwise it early-outs.
 *
 * Uploads 5 vertex-shader constants (a 20-float contiguous block, ebp-0x50..
 * ebp-0x4). Slots 0/3 (x-transform) and 5/7 (y-transform) are computed from
 * the viewport dimensions:
 *   height = (short)(*0x5a5bfa - *0x5a5bf6)   [16-bit .hi fields]
 *   width  = (short)(*0x5a5bf8 - *0x5a5bf4)   [32-bit fields, low 16 used]
 *   hdiv   = *0x2533c8 / (float)height        (memory-form fdivrs)
 *   vs[0] = hdiv + hdiv;  vs[3] = *0x255e94 - hdiv;
 *   vs[7] = *0x2533c8 / (float)width;
 *   vs[5] = *0x25eeac * vs[7];  vs[7] = vs[7] + *0x2533c8;
 * The three numerator/offset constants are read as float globals (the original
 * emits fdivrs/flds/fadds with memory operands, NOT immediates), so they MUST
 * stay as *(float *)0xADDR reads to reproduce the codegen.
 *
 * Pixel-shader state block at 0x5a5ac0 is zeroed (0xf0 bytes) then seeded
 * (0x5a5b98=1, 0x5a5b94=1, 0x5a5ae0=8) and applied.
 *
 * The quad is emitted with D3DDevice_Begin(7 = D3DPT_QUADLIST): 4 vertices,
 * each preceded by a texcoord register write (reg 4 = D3DVSDE_TEXCOORD0) and a
 * position write (reg 0 = D3DVSDE_VERTEX). Position components are the uint16
 * bounds corners; texcoords are the 0/1 unit-square corners.
 *
 * __FILE__ for the asserts is rasterizer_xbox.c (the original TU; the linker
 * grouped this fn into rasterizer_decals.obj). The assert-fail path calls
 * display_assert then system_exit(-1) (delinked relocs at +0x27/+0x50 call
 * FUN_001029a0, each preceded by `push $-1`, so the source form is the
 * arg-passing system_exit(-1), NOT the arg-less halt_and_catch_fire()).
 *
 * Globals (used by address, not in kb.json):
 *   0x476ab0  void*   global_d3d_device (asserted non-NULL)
 *   0x325703  char    decal-composite feature gate (non-zero enables)
 *   0x5a5bc0  int16   busy/pending word (must be 0 to run)
 *   0x5a5bf4/f8  int32   viewport x extents (width)
 *   0x5a5bf6/fa  int16   viewport y extents (height)
 *   0x2533c8  float   viewport-scale numerator (used twice)
 *   0x255e94  float   x-transform offset
 *   0x25eeac  float   y-transform scale
 *   0x1fb7a4/784/788  uint32   shadow render-state cache
 *   0x5a5ac0  ...     pixel-shader state block (0xf0 bytes)
 *
 *   bounds - uint16[4] screen rectangle {x0, y0, x1, y1}
 */
void FUN_00158800(unsigned short *bounds)
{
  /* 20-float (5 vertex-shader constant) upload block; must stay one
   * contiguous array so the constants lay out ebp-0x50..ebp-0x4. */
  float vs[20];
  int width;
  int height;
  float hdiv;

  if (bounds == 0) {
    display_assert("bounds",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x9e4, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x9e5, 1);
    system_exit(-1);
  }

  if (*(char *)0x325703 != 0 && *(short *)0x5a5bc0 == 0) {
    FUN_001584f0(0, 1, 0);
    D3DDevice_SetTextureStageState(0, 0xa, 3);
    D3DDevice_SetTextureStageState(0, 0xb, 3);
    D3DDevice_SetTextureStageState(0, 0xd, 2);
    D3DDevice_SetTextureStageState(0, 0xe, 2);
    D3DDevice_SetTextureStageState(0, 0xf, 2);
    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD, NV097_COLOR_MASK_RGB);
    *(uint32_t *)0x1fb7a4 = 0x10101;
    D3DDevice_SetRenderState_Simple(0x40304, 0);
    *(uint32_t *)0x1fb784 = 0;
    D3DDevice_SetRenderState_Simple(0x40300, 0);
    *(uint32_t *)0x1fb788 = 0;
    D3DDevice_SetRenderState_ZEnable(0);
    D3DDevice_SetRenderState_ZBias(0);

    FUN_00178b40(4, 8, 0);

    /* viewport height from the 16-bit .hi (y) fields, sign-extended */
    height = (short)(*(short *)0x5a5bfa - *(short *)0x5a5bf6);
    /* viewport width: 32-bit fields, low 16 bits sign-extended */
    width = (short)(*(int *)0x5a5bf8 - *(int *)0x5a5bf4);

    vs[1] = 0.0f;
    vs[2] = 0.0f;
    vs[4] = 0.0f;
    vs[6] = 0.0f;
    vs[8] = 0.0f;
    vs[9] = 0.0f;
    vs[10] = 0.0f;
    vs[11] = 0.5f;

    /* memory-form FPU ops: numerator/offset constants are float globals */
    hdiv = *(float *)0x2533c8 / (float)height;
    vs[0] = hdiv + hdiv;
    vs[3] = *(float *)0x255e94 - hdiv;
    vs[7] = *(float *)0x2533c8 / (float)width;
    vs[5] = *(float *)0x25eeac * vs[7];
    vs[7] = vs[7] + *(float *)0x2533c8;

    vs[12] = 0.0f;
    vs[13] = 0.0f;
    vs[14] = 0.0f;
    vs[15] = 1.0f;
    vs[16] = 320.0f;
    vs[17] = 240.0f;
    vs[18] = 0.0f;
    vs[19] = 1.0f;

    D3DDevice_SetVertexShaderConstant(-0x44, vs, 5);

    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(uint32_t *)0x5a5b98 = 1;
    *(uint32_t *)0x5a5b94 = 1;
    *(uint32_t *)0x5a5ae0 = 8;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);

    D3DDevice_Begin(7);
    D3DDevice_SetVertexData2s(4, 0, 1);
    D3DDevice_SetVertexData2s(0, (int)(unsigned short)bounds[1],
                              (int)(unsigned short)bounds[0]);
    D3DDevice_SetVertexData2s(4, 1, 1);
    D3DDevice_SetVertexData2s(0, (int)(unsigned short)bounds[3],
                              (int)(unsigned short)bounds[0]);
    D3DDevice_SetVertexData2s(4, 1, 0);
    D3DDevice_SetVertexData2s(0, (int)(unsigned short)bounds[3],
                              (int)(unsigned short)bounds[2]);
    D3DDevice_SetVertexData2s(4, 0, 0);
    D3DDevice_SetVertexData2s(0, (int)(unsigned short)bounds[1],
                              (int)(unsigned short)bounds[2]);
    D3DDevice_End();
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
 * D3DDevice_SetRenderState_StencilEnable / _StencilFail are __stdcall @4
 * (both end RET 4, arg read at [ESP+8] after an internal PUSH ESI) even
 * though Ghidra decompiles them as void(void) cdecl. Original call sites:
 * case 0 pushes 0 (disable); cases 1-3 push ESI=1 for StencilEnable and
 * 0x1e00 (D3DSTENCILOP_KEEP) for StencilFail, with NO caller cleanup.
 * Calling them arg-less drifted ESP +4 per call; inlined into FUN_00158df0
 * this turned the epilogue RET into a jump onto the stack -> boot #PF at
 * CR2=0xfffffff5 (fixed 2026-07-11).
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
      D3DDevice_SetRenderState_StencilEnable(0);
      *(short *)0x325168 = sVar1;
      return;
    case 1:
      D3DDevice_SetRenderState_StencilEnable(1);
      D3DDevice_SetRenderState_StencilFail(0x1e00);
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
      D3DDevice_SetRenderState_StencilEnable(1);
      D3DDevice_SetRenderState_StencilFail(0x1e00);
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
      D3DDevice_SetRenderState_StencilEnable(1);
      D3DDevice_SetRenderState_StencilFail(0x1e00);
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
 * FUN_00159070  @ 0x159070  (rasterizer_decals.obj)
 * -----------------------------------------------------------------------------
 * real_alpha_to_pixel32-style inline from ..\bitmaps\bitmaps_inlines.h
 * (line 0x123): converts a [0,1] alpha float to a pixel32 with the alpha
 * in the top byte. Bounds assert falls through into the body (standard
 * display_assert + system_exit(-1) macro, combined ADD ESP,0x14 cleanup
 * at 0x1590ba — NOT an early return).
 *
 * Codegen notes (delinked/functions/00159070.obj):
 *  - 255.0f is materialized as an immediate store to a stack local
 *    (MOV [EBP-8],0x437F0000) then FLD/FMULP — volatile scale reproduces
 *    the store+load instead of a pooled-constant FMUL.
 *  - The int conversion is a bare FISTP (round-to-nearest, /QIfist
 *    codegen) — NOT C truncation. x87_round_to_int keeps runtime
 *    behavior faithful (a plain (int) cast diverged 7/100 equivalence
 *    seeds). Our VC71 harness lacks /QIfist, so the harness emits the
 *    helper's asm out-of-line-ish — a small structural gap, not a bug.
 *  - SHL happens in MEMORY ([EBP-4],0x18) then MOV EAX,[EBP-4].
 *  - Compares: FLD alpha; FCOMP 0.0f (test ah,1; jne assert) then
 *    FLD alpha; FCOMP 1.0f (test ah,0x41; jnp body) — i.e.
 *    !(alpha >= 0.0f && alpha <= 1.0f) with the assert as fall-through.
 */
uint32_t FUN_00159070(float alpha)
{
  volatile float scale;
  int pixel;

  scale = 255.0f;
  if (!(alpha >= 0.0f && alpha <= 1.0f)) {
    display_assert("alpha>=0.0f && alpha<=1.0f",
                   "..\\bitmaps\\bitmaps_inlines.h", 0x123, true);
    system_exit(-1);
  }

  pixel = x87_round_to_int(scale * alpha);
  pixel <<= 0x18;
  return (uint32_t)pixel;
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

static const char kActiveCamoFile[] =
  "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_active_camouflage.c";

/* 0x1595c0
 *
 * Active-camouflage screen-space capture pass. When active camouflage is
 * enabled for this frame (byte gates 0x3256f9 debug-enable and 0x476ac0
 * per-frame flag), draws one full-screen 320x240 quad through the
 * active-camouflage pixel shader into the render target, then hands the
 * result to the camo texture manager (FUN_00158800) with a bounds rect
 * derived from the 16-bit frame counter at 0x476ac4.
 *
 * All facts below decoded from the delinked reference
 * delinked/functions/001595c0.obj and the pristine XBE:
 *   - Both assert tails are PUSH -1; CALL 0x8e2f0 = system_exit(-1)
 *     (XBE-verified at 0x1595e2/e4 and 0x159623/25; a parked lift twice
 *     claimed halt_and_catch_fire here — same anti-pattern as FUN_0015c680
 *     and FUN_00158df0, both review-gate REJECTs).
 *   - Guard bytes: MOV AL,[0x3256f9] / MOV AL,[0x476ac0], both JE end.
 *   - Render-target assert compare is 16-bit: CMP WORD PTR [0x5a5bc0],0.
 *   - SetRenderState_Simple(0x40358, 0x10101) — NV2A SET_COLOR_MASK,
 *     R+G+B write-enabled (MOV EDX,0x10101: BA 01 01 01 00 at 0x15968b);
 *     mirror store [0x1fb7a4]=0x10101. An earlier 0x101 transcription
 *     disabled red-channel writes during the capture, garbling the camo
 *     texture red (2026-07-12 regression).
 *   - Vertex-shader constant block: 5 vec4 at ebp-0x58, exact bit patterns
 *     0x3bcccccd / 0xbf806666 / 0xbc088889 / 0x3f808889 / 0x3f000000 /
 *     0x3f800000 (literals verified to round-trip to these encodings).
 *   - Quad texcoords are 16-bit globals: u from 0x5a5bf6/0x5a5bfa, v from
 *     0x5a5bf8/0x5a5bf4, all zero-extended (XOR reg,reg; MOV reg16).
 *   - Second FUN_00158140 first arg is the zero-extended WORD at 0x5a5bc0.
 *   - Bounds buffer at ebp-0x8, 4x uint16 passed to FUN_00158800:
 *     [0]=((f*3+3)<<5) low16, [1]=0x200, [2]=((f*3+6)<<5) low16, [3]=0x280,
 *     f = full 32-bit dword read of 0x476ac4 (LEA [EAX+EAX*2+n]; SHL 5).
 *   - Frame counter update is INC WORD PTR [0x476ac4] (low 16 bits only),
 *     and the gate byte 0x3256fa is loaded BEFORE the increment.
 *
 * Original TU:
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_active_camouflage.c
 * (asserts at source lines 0x29 and 0x2e).
 */
void FUN_001595c0(void)
{
  /* 20-float (5 vertex-shader constant) upload block; must stay one
   * contiguous array so the constants are laid out ebp-0x58..ebp-0x9. */
  float vs[20];
  /* 4x uint16 bounds rect at ebp-0x8, passed whole to FUN_00158800. */
  unsigned short bounds[4];

  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device", kActiveCamoFile, 0x29, 1);
    system_exit(-1);
  }

  if (*(char *)0x3256f9 != '\0' && *(char *)0x476ac0 != '\0') {
    /* 16-bit compare in the original (CMP WORD PTR [0x5a5bc0],0) */
    if (*(short *)0x5a5bc0 != 0) {
      display_assert("global_window_parameters.rasterizer_target==_"
                     "rasterizer_target_render_primary",
                     kActiveCamoFile, 0x2e, 1);
      system_exit(-1);
    }

    FUN_001584f0(0, 0, 0);

    D3DDevice_SetTextureStageState(0, 0xa, 3);
    D3DDevice_SetTextureStageState(0, 0xb, 3);
    D3DDevice_SetTextureStageState(0, 0xd, 2);
    D3DDevice_SetTextureStageState(0, 0xe, 2);
    D3DDevice_SetTextureStageState(0, 0xf, 1);

    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD, NV097_COLOR_MASK_RGB);
    *(uint32_t *)0x1fb7a4 = 0x10101;
    D3DDevice_SetRenderState_Simple(0x40304, 0);
    *(uint32_t *)0x1fb784 = 0;
    D3DDevice_SetRenderState_Simple(0x40300, 0);
    *(uint32_t *)0x1fb788 = 0;
    D3DDevice_SetRenderState_ZEnable(0);
    D3DDevice_SetRenderState_ZBias(0);

    FUN_00178b40(4, 8, 0);

    /* 5 vec4 vertex-shader constants (c[-0x44..-0x40]); literal bit
     * patterns per the reference: see block comment above. */
    vs[0] = 0.00625f; /* 0x3bcccccd */
    vs[1] = 0.0f;
    vs[2] = 0.0f;
    vs[3] = -1.003125f; /* 0xbf806666 */
    vs[4] = 0.0f;
    vs[5] = -0.008333334f; /* 0xbc088889 */
    vs[6] = 0.0f;
    vs[7] = 1.0041667f; /* 0x3f808889 */
    vs[8] = 0.0f;
    vs[9] = 0.0f;
    vs[10] = 0.0f;
    vs[11] = 0.5f; /* 0x3f000000 */
    vs[12] = 0.0f;
    vs[13] = 0.0f;
    vs[14] = 0.0f;
    vs[15] = 1.0f;
    vs[16] = 1.0f;
    vs[17] = 1.0f;
    vs[18] = 0.0f;
    vs[19] = 1.0f;
    D3DDevice_SetVertexShaderConstant(-0x44, vs, 5);

    /* pixel-shader state block (same 0x5a5ac0 block as the other passes in
     * this TU); combiner-count field 0x5a5ae0 = 8 for this shader. */
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(uint32_t *)0x5a5b98 = 1;
    *(uint32_t *)0x5a5b94 = 1;
    *(uint32_t *)0x5a5ae0 = 8;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);

    FUN_00158140(1, 0, 0, 0, 0);
    FUN_00158ae0(0);

    /* full-screen 320x240 quad; texcoords are the 16-bit viewport-rect
     * globals, zero-extended (order per reference relocations). */
    D3DDevice_Begin(7);
    D3DDevice_SetVertexData2s(4, *(unsigned short *)0x5a5bf6,
                              *(unsigned short *)0x5a5bf8);
    D3DDevice_SetVertexData2s(0, 0, 0);
    D3DDevice_SetVertexData2s(4, *(unsigned short *)0x5a5bfa,
                              *(unsigned short *)0x5a5bf8);
    D3DDevice_SetVertexData2s(0, 0x140, 0);
    D3DDevice_SetVertexData2s(4, *(unsigned short *)0x5a5bfa,
                              *(unsigned short *)0x5a5bf4);
    D3DDevice_SetVertexData2s(0, 0x140, 0xf0);
    D3DDevice_SetVertexData2s(4, *(unsigned short *)0x5a5bf6,
                              *(unsigned short *)0x5a5bf4);
    D3DDevice_SetVertexData2s(0, 0, 0xf0);
    D3DDevice_End();

    /* first arg is the zero-extended WORD at 0x5a5bc0 (XOR EAX,EAX;
     * MOV AX,[0x5a5bc0]) — asserted 0 above, but re-read here. */
    FUN_00158140(*(unsigned short *)0x5a5bc0, 0, 0, 0, 1);
    FUN_00158ae0(2);

    {
      /* bounds math reads the FULL 32-bit dword at 0x476ac4 */
      int frame = *(int *)0x476ac4;
      bounds[1] = 0x200;
      bounds[3] = 0x280;
      bounds[0] = (unsigned short)((frame * 3 + 3) << 5);
      bounds[2] = (unsigned short)((frame * 3 + 6) << 5);
      FUN_00158800(bounds);
    }

    {
      /* gate byte loaded BEFORE the 16-bit counter increment */
      char gate = *(char *)0x3256fa;
      /* INC WORD PTR [0x476ac4] — increment only the low 16 bits */
      *(unsigned short *)0x476ac4 += 1;
      if (gate == '\0') {
        *(char *)0x476ac0 = 0;
      }
      *(char *)0x476ac1 = 1;
    }
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
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD, NV097_COLOR_MASK_NONE);
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
  D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD, NV097_COLOR_MASK_RGB);
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

/*
 * FUN_0015a290 @ 0x15a290 — empty function in the binary (single RET;
 * three live call sites). The faithful lift is an empty body.
 */
/* 0x15a290 */
void FUN_0015a290(void)
{
}

/*
 * FUN_0015a4c0 @ 0x15a4c0 — empty function in the binary (single RET;
 * no direct call sites).
 */
/* 0x15a4c0 */
void FUN_0015a4c0(void)
{
}

/*
 * FUN_0015a4e0 @ 0x15a4e0 — empty function in the binary (single RET;
 * reached only via the tail-call thunk at 0x17ca70).
 */
/* 0x15a4e0 */
void FUN_0015a4e0(void)
{
}

/*
 * FUN_0015a4f0 @ 0x15a4f0 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::SetVertexData4f. __stdcall (RET 0x18 = 6 stack args),
 * ignores the device argument, forwards (reg, a, b, c, d) to
 * D3DDevice_SetVertexData4f, returns S_OK. No direct call sites.
 */
/* 0x15a4f0 */
int __stdcall FUN_0015a4f0(void *device, uint32_t reg, float a, float b, float c, float d)
{
  (void)device;
  D3DDevice_SetVertexData4f(reg, a, b, c, d);
  return 0;
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
    system_exit(-1);
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
    system_exit(-1);
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

/* 0x15a7f0
 *
 * rasterizer_debug_draw_line  (debug 3D line drawer)
 *
 * Draws a single world-space debug line between two 3D points with optional
 * per-vertex color, using the D3D inline (Begin/SetVertexData4f/End) vertex
 * stream. The 3D counterpart to FUN_0015abe0 (which draws a 2D screen-space
 * line). Originally defined in rasterizer_xbox_debug.c (the assert __FILE__
 * string preserves that path); the linker grouped it into
 * rasterizer_decals.obj.
 *
 *   p0     - float[3] world position of the first endpoint  (x, y, z)
 *   p1     - float[3] world position of the second endpoint (x, y, z)
 *   color0 - float[3] RGB color for p0
 *   color1 - optional float[3] RGB color for p1; if NULL, p0's color is reused
 *
 * D3D primitive: Begin(2 = D3DPT_LINELIST). Vertex register 9 =
 * D3DVSDE_DIFFUSE (color), register 0 = D3DVSDE_VERTEX (position). Each
 * SetVertexData4f pushes the three components plus a hard-coded w = 1.0f.
 */
void FUN_0015a7f0(float *p0, float *p1, float *color0, float *color1)
{
  if (p0 == 0 || p1 == 0 || color0 == 0) {
    display_assert(
      "p0 && p1 && color0",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_debug.c", 0x74, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_debug.c", 0x75, 1);
    system_exit(-1);
  }

  D3DDevice_Begin(2);

  /* The volatile y/z reads are a VC71 codegen shape lever only (same as
   * FUN_0015a8f0): they reproduce the original's hybrid arg push (x via GPR
   * mov+push, y/z via FLD/FSTP [ESP], 1.0f as an immediate push) under /O2.
   * A single read either way - semantics are unchanged. */
  D3DDevice_SetVertexData4f(9, color0[0], *(volatile float *)(color0 + 1),
                            *(volatile float *)(color0 + 2), 1.0f);
  D3DDevice_SetVertexData4f(0, p0[0], *(volatile float *)(p0 + 1),
                            *(volatile float *)(p0 + 2), 1.0f);

  if (color1 != 0) {
    D3DDevice_SetVertexData4f(9, color1[0], *(volatile float *)(color1 + 1),
                              *(volatile float *)(color1 + 2), 1.0f);
  }
  D3DDevice_SetVertexData4f(0, p1[0], *(volatile float *)(p1 + 1),
                            *(volatile float *)(p1 + 2), 1.0f);

  D3DDevice_End();
}

/*
 * rasterizer_xbox_debug.c
 *
 * Xbox rasterizer debug-draw helpers (immediate-mode D3D primitives).
 * Original TU: c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_debug.c
 * (__FILE__ assert string xref, confirmed).
 *
 * Globals (used by address, not in kb.json):
 *   0x476ab0  void *  – global_d3d_device (IDirect3DDevice8 pointer)
 */

/* 0x15a8f0 — debug triangle draw (D3DPT_TRIANGLEFAN, 3 vertices).
 *
 * Ghidra's decl is void(void) but the binary reads 6 cdecl stack args at
 * [EBP+8..+0x1c] (verified in disassembly: ESI=[EBP+8]=p0, EBX=[EBP+0xc]=p1,
 * [EBP+0x10]=p2, EDI=[EBP+0x14]=color0, [EBP+0x18]=color1, [EBP+0x1c]=color2).
 * Each pointer is a 3-float vector; the 4th component is the literal 1.0f
 * (push 0x3f800000 in the binary).
 *
 * Vertex-register interleave (order verified against disassembly):
 *   SetVertexData4f(9, color0)   – reg 9 = diffuse
 *   SetVertexData4f(0, p0)       – reg 0 = position
 *   if (color1) SetVertexData4f(9, color1)
 *   SetVertexData4f(0, p1)
 *   if (color2) SetVertexData4f(9, color2)
 *   SetVertexData4f(0, p2)
 * then D3DDevice_End (tail jump in the original).
 *
 * Asserts: line 0x8b = "p0 && p1 && p2 && color0", line 0x8c =
 * "global_d3d_device"; each pairs display_assert with system_exit(-1)
 * (push -1; call FUN_001029a0) and falls through, matching the binary.
 */
void FUN_0015a8f0(float *p0, float *p1, float *p2, float *color0, float *color1,
                  float *color2)
{
  if (p0 == 0 || p1 == 0 || p2 == 0 || color0 == 0) {
    display_assert(
      "p0 && p1 && p2 && color0",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_debug.c", 0x8b, 1);
    system_exit(-1);
  }

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_debug.c", 0x8c, 1);
    system_exit(-1);
  }

  D3DDevice_Begin(5); /* D3DPT_TRIANGLEFAN */

  /* The volatile y/z reads are a VC71 codegen shape lever only: they
   * reproduce the original's hybrid arg push (x via GPR mov+push, y/z via
   * FLD/FSTP [ESP]) under /O2. A single read either way - semantics are
   * unchanged. */
  D3DDevice_SetVertexData4f(9, color0[0], *(volatile float *)(color0 + 1),
                            *(volatile float *)(color0 + 2), 1.0f);
  D3DDevice_SetVertexData4f(0, p0[0], *(volatile float *)(p0 + 1),
                            *(volatile float *)(p0 + 2), 1.0f);

  if (color1 != 0) {
    D3DDevice_SetVertexData4f(9, color1[0], *(volatile float *)(color1 + 1),
                              *(volatile float *)(color1 + 2), 1.0f);
  }
  D3DDevice_SetVertexData4f(0, p1[0], *(volatile float *)(p1 + 1),
                            *(volatile float *)(p1 + 2), 1.0f);

  if (color2 != 0) {
    D3DDevice_SetVertexData4f(9, color2[0], *(volatile float *)(color2 + 1),
                              *(volatile float *)(color2 + 2), 1.0f);
  }
  D3DDevice_SetVertexData4f(0, p2[0], *(volatile float *)(p2 + 1),
                            *(volatile float *)(p2 + 2), 1.0f);

  D3DDevice_End();
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

/* 0x15acc0
 *
 * rasterizer_debug_draw_polyline2d  (debug 2D multi-point line drawer)
 *
 * Draws a screen-space debug line strip through `point_count` 2D points, all
 * sharing one color, using the D3D inline (Begin/SetVertexData.../End) vertex
 * stream. Originally defined in rasterizer_xbox_debug.c (the assert __FILE__
 * string preserves that path); the linker grouped it into
 * rasterizer_decals.obj.
 *
 *   points      - array of short[2] screen coords (x, y), stride 4 bytes/point
 *   point_count - number of points; must be > 1 (asserted, line 0xf9)
 *   color       - real_rgb_color, packed to 0x00RRGGBB by FUN_000d1dd0
 *
 * D3D primitive: Begin(4 = D3DPT_LINELIST). Vertex register 9 = D3DVSDE_DIFFUSE
 * (color, set once), register 0 = D3DVSDE_VERTEX (position via SetVertexData2s,
 * per point). Position components are zero-extended from the 16-bit screen
 * coords before the call (decl.h SetVertexData2s takes int a/b).
 *
 * `success` mirrors the original D3D result-check macro: it stays true, so the
 * FUN_00167ff0 (report_d3d_call_failed) branches after each SetVertexData2s and
 * after End are unreachable in practice — preserved to match the binary's
 * basic-block layout. The per-vertex check emits the reassignment shape
 * (if(success) success=1; else { success=0; report; }) that the original
 * compiled to (0x15ad74: mov $1,bl / xor bl,bl); the post-End check is the
 * plain if(!success) report form (0x15ad8a). The two assert paths call
 * display_assert (arg check line 0xf9, device-null check line 0xfa, halt=1)
 * followed by system_exit(-1) — NOT halt_and_catch_fire; the disassembly at
 * 0x15acde/0x15acfe pushes -1 with a combined ADD ESP,0x14 cleanup, matching
 * the sibling assert macro. Laid out at the function tail as fall-through
 * (the original's inverted nesting).
 */
void FUN_0015acc0(short *points, int16_t point_count, float *color)
{
  unsigned int packed;
  unsigned int remaining;
  short *p;
  /* volatile: the D3D result-check macro's test/branch on the flag is live
   * in the binary (TEST BL,BL at 0x15ad64/0x15ad8a); a plain local folds to
   * constant 1 and VC71 dead-codes both report branches (-18 insns). */
  volatile int success;

  /* Fall-through assert blocks, matching the original layout (asserts first,
   * body unconditional after them). */
  if (!(points != 0 && color != 0 && 1 < point_count)) {
    display_assert("points && color && point_count>1",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_debug.c",
                   0xf9, 1);
    system_exit(-1);
  }
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_debug.c", 0xfa, 1);
    system_exit(-1);
  }

  packed = FUN_000d1dd0(color);
  D3DDevice_Begin(4);
  D3DDevice_SetVertexDataColor(9, packed);
  success = 1;
  if (0 < point_count) {
    p = points;
    remaining = (unsigned int)(unsigned short)point_count;
    do {
      D3DDevice_SetVertexData2s(0, (int)(unsigned short)p[0],
                                (int)(unsigned short)p[1]);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexData2s(global_d3d_device, "
                     "D3DVSDE_VERTEX, point->x, point->y)");
      }
      p = p + 2;
      remaining = remaining - 1;
    } while (remaining != 0);
  }
  D3DDevice_End();
  if (!success) {
    FUN_00167ff0(0, "IDirect3DDevice8_End(global_d3d_device)");
  }
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

/* 0x15b0c0
 *
 * rasterizer_decals_vertex_cache_query  (LRUV locked-probe callback)
 *
 * Called by the LRUV cache to check whether a cached decal may be evicted.
 * Returns true (locked / not evictable) if the decal datum has either the
 * locked (bit 0) or permanent (bit 1) flag set; false otherwise.
 * Also records decal_index into 0x32516c (last-queried decal index).
 *
 * Ghidra reports void(void) — WRONG on both counts (§16 void-EAX): one
 * stack arg [EBP+8] = decal_index, bool return in AL (XOR AL,AL / MOV AL,1).
 *
 * The goto shape mirrors the binary's block layout exactly and is
 * match-sensitive — do not fold back into sequential ifs:
 *   check==0 / assert-0x47 body / shared tail 0x15b0f6 (call display_assert;
 *   push -1; call system_exit) / main path 0x15b105 / check==-1 at 0x15b127
 *   with its pushes cross-jumping back into the shared tail / mov al,1 ret
 *   last. The original compiler knew system_exit is noreturn and let the
 *   assert body fall into the main path; our VC71 shim expands __noreturn
 *   to nothing under MSVC, so sequential ifs put the -1 check inline after
 *   the first assert (70.9%). Sequential-if + setne return also replaced
 *   the branchy XOR AL,AL / MOV AL,1 pair.
 * Flag load is a single byte: MOV CL, byte ptr [EAX+2] (§24 — do not widen).
 */
bool FUN_0015b0c0(int decal_index)
{
  char *decal;

  if (decal_index != 0)
    goto check_none;
  display_assert("decal_index",
                 "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c",
                 0x47, 1);
  system_exit(-1);
  /* noreturn fallthrough into the main path, as in the binary */

main_path:
  decal = (char *)datum_get(*(void **)0x5aa8b8, decal_index);
  *(int *)0x32516c = decal_index;
  if (*(unsigned char *)(decal + 2) & 3)
    goto ret_locked;
  return 0;

check_none:
  if (decal_index != -1)
    goto main_path;
  display_assert("decal_index!=NONE",
                 "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c",
                 0x48, 1);
  system_exit(-1);

ret_locked:
  return 1;
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

/*
 * FUN_0015b5a0 @ 0x15b5a0 — dead register-convention adapter for
 * IDirect3DDevice8::DrawPrimitive: primitive type arrives in EAX, the
 * device pointer (a1) is ignored, and the primitive count (a3) is
 * converted to a vertex count via the per-type {multiplier, addend}
 * table at 0x29f7e8 before calling D3DDevice_DrawVertices. Falls
 * through with 0 in EAX (S_OK). No direct call sites; RET 0xC.
 */
/* 0x15b5a0 */
int FUN_0015b5a0(int index, int a1, int a2, int a3)
{
  (void)a1;
  D3DDevice_DrawVertices(
      index,
      a2,
      *(uint32_t *)(0x29f7e8 + index * 8) * a3
        + *(uint32_t *)(0x29f7ec + index * 8));
  return 0;
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

/*
 * FUN_0015b650 @ 0x15b650 — dead register-convention adapter for
 * D3DDevice_SetVertexData4ub: g/b/a components arrive in EDX/ECX/EAX,
 * the register index (s2) and red component (s3) on the stack; s1 is
 * the ignored device pointer. Returns S_OK. No direct call sites;
 * RET 0xC.
 */
/* 0x15b650 */
int FUN_0015b650(int r1, int r2, int r3, int s1, int s2, int s3)
{
  (void)s1;
  D3DDevice_SetVertexData4ub(s2, s3, r3, r2, r1);
  return 0;
}

/*
 * FUN_0015b6a0 @ 0x15b6a0 — dead register-convention adapter for
 * D3DVertexBuffer_Lock (0x1ef100, kb.json stub decl is void(void) so the
 * call uses the raw __stdcall cast, matching FUN_0015abe0): vertex buffer
 * (s1) and offset (s2) on the stack, size/ppbData/flags in EDX/ECX/EAX.
 * Returns S_OK. No direct call sites; RET 8.
 */
/* 0x15b6a0 */
int FUN_0015b6a0(int r1, int r2, int r3, int s1, int s2)
{
  ((void(__stdcall *)(void *, uint32_t, uint32_t, void **, uint32_t))0x1ef100)(
      (void *)s1, s2, r3, (void **)r2, r1);
  return 0;
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
  *(void **)0x476adc = lruv_cache_new("decal vertex cache", 0xa00, 6, 0x800,
                                      FUN_0015afa0, (int (*)(int))FUN_0015b0c0);

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
 * FUN_0015b960 @ 0x15b960 — empty function in the binary (single RET;
 * no direct call sites).
 */
/* 0x15b960 */
void FUN_0015b960(void)
{
}

/*
 * FUN_0015b970 (0x15b970)  rasterizer_decals_begin / decal-layer render setup
 *
 * Sets up decal render state for a given decal layer (pass_index, 0..4).
 * Selects a rasterizer texture profile from a 5-entry local table via
 * FUN_0016f910, records the active layer in the 0x476ac8 shadow, and (when
 * the two early-out gates pass) resets the cached texture/blend state,
 * programs texture-stage + cull/z/alpha render state (each mirrored into the
 * 0x1fb7xx shadow copies), and rebuilds the 0xf0-byte pixel-shader/format
 * state block at 0x5a5ac0 before binding the decal vertex stream (stride
 * 0x10).
 *
 * pass_index==3 forces alpha-blend on with alpha-ref 0x7f and switches
 * texture mode via FUN_00158ae0(4). Other layers latch 0x476ae2 from an FPU
 * equality test (fld 0x5a5db8; fcomp 0x2533c8; test ah,0x44; jp) gated by
 * byte 0x325719; the latch selects stream format 3 (with extra combiner
 * dwords) vs 2 in the state block. Branch shape below mirrors the original
 * layout: latch-set falls to the enable block, the else-if jumps forward to
 * the disable block (delinked ref 0x1c8-0x22c).
 *
 * Both assert tails call halt_and_catch_fire (FUN_001029a0, relocs 0x2f and
 * 0xb9 in the per-fn delinked ref — NOT system_exit like most of this TU).
 * The original pushes -1 (EDI) to it; our shared void(void) decl cannot
 * express that push — fixed 1-insn diff per assert site.
 *
 * profile_table is short[6] with only [0..4] initialized: the original frame
 * is sub esp,0xc and the guard is pass_index<5; do not shrink.
 *
 * Globals (by address, widths from the reference disasm):
 *   0x476ab0  int    global_d3d_device (asserted non-NULL)
 *   0x476ac8  int16  active decal layer shadow
 *   0x3256bc  uint16 early-out gate A (!=0 => return)
 *   0x3256cd  uint8  early-out gate B (==0 => return)
 *   0x476ad4/0x476ad0 uint16, 0x476acc uint32: cached blend/frame/bitmap
 *   0x476ae2  char   stream-format latch flag
 *   0x32570c  uint32 z-bias value
 *   0x325719  char   FPU-compare enable gate
 *   0x5a5db8, 0x2533c8 float latch comparands (0x2533c8 is const 1.0f)
 *   0x1fb784/788/78c/798/77c uint32 render-state shadow copies
 *   0x5a5ac0  0xf0-byte pixel-shader state block; 0x476ad8 vertex buffer
 *
 * 0x15b970 / rasterizer_decals.obj
 */
void FUN_0015b970(short pass_index)
{
  short profile_table[6];

  if (*(int *)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x11b, 1);
    system_exit(-1);
  }

  profile_table[0] = 9;
  profile_table[1] = 10;
  profile_table[2] = 6;
  profile_table[3] = 7;
  profile_table[4] = 0x14;

  if ((pass_index >= 0) && (pass_index < 5)) {
    FUN_0016f910(profile_table[pass_index]);
  }

  *(short *)0x476ac8 = pass_index;

  if (*(uint16_t *)0x3256bc != 0) {
    return;
  }
  if (*(char *)0x3256cd == '\0') {
    return;
  }

  if ((pass_index < 0) || (pass_index >= 5)) {
    display_assert(
      "layer>=0 && layer<NUMBER_OF_DECAL_LAYERS",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_decals.c", 0x133, 1);
    system_exit(-1);
  }

  *(uint16_t *)0x476ad4 = 0xffff;
  *(uint16_t *)0x476ad0 = 0xffff;
  *(uint32_t *)0x476acc = 0xffffffff;
  *(char *)0x476ae2 = '\0';

  rasterizer_set_texture(0, 0, 1, -1, 0);
  D3DDevice_SetTextureStageState(0, 10, 3);
  D3DDevice_SetTextureStageState(0, 0xb, 3);
  D3DDevice_SetTextureStageState(0, 0xd, 2);
  D3DDevice_SetTextureStageState(0, 0xe, 2);
  D3DDevice_SetTextureStageState(0, 0xf, 2);
  D3DDevice_SetRenderState_CullMode(0x901);
  D3DDevice_SetRenderState_Simple(0x40304, 1);
  *(uint32_t *)0x1fb784 = 1;
  D3DDevice_SetRenderState_ZEnable(1);
  D3DDevice_SetRenderState_Simple(0x4035c, 0);
  *(uint32_t *)0x1fb798 = 0;
  D3DDevice_SetRenderState_Simple(0x40354, 0x203);
  *(uint32_t *)0x1fb77c = 0x203;
  D3DDevice_SetRenderState_ZBias(*(uint32_t *)0x32570c);

  if (pass_index == 3) {
    D3DDevice_SetRenderState_Simple(0x40300, 1);
    *(uint32_t *)0x1fb788 = 1;
    D3DDevice_SetRenderState_Simple(0x40340, 0x7f);
    *(uint32_t *)0x1fb78c = 0x7f;
    FUN_00158ae0(4);
  } else {
    /* fld [0x5a5db8]; fcomp [0x2533c8]; test ah,0x44; jp — equality test;
     * the equal path latches 0x476ae2 and falls into the enable block. */
    if ((*(char *)0x325719 != '\0') &&
        (*(float *)0x5a5db8 == *(float *)0x2533c8)) {
      *(char *)0x476ae2 = '\x01';
    } else if (*(char *)0x476ae2 == '\0') {
      D3DDevice_SetRenderState_Simple(0x40300, 0);
      *(uint32_t *)0x1fb788 = 0;
      goto LAB_0015bb9c;
    }
    D3DDevice_SetRenderState_Simple(0x40300, 1);
    *(uint32_t *)0x1fb788 = 1;
    D3DDevice_SetRenderState_Simple(0x40340, 0);
    *(uint32_t *)0x1fb78c = 0;
  }

LAB_0015bb9c:
  FUN_00178b40(1, 10, 0);
  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b98 = 1;
  *(uint32_t *)0x5a5b74 = 0xc00;
  *(uint32_t *)0x5a5b2c = 0xc00;
  *(uint32_t *)0x5a5b78 = 0xc00;
  if (*(char *)0x476ae2 != '\0') {
    *(uint32_t *)0x5a5b94 = 3;
    *(uint32_t *)0x5a5ae8 = 0x1000000;
    *(uint32_t *)0x5a5ac8 = 0x1c151115;
    *(uint32_t *)0x5a5b30 = 0xc00;
  } else {
    *(uint32_t *)0x5a5b94 = 2;
  }
  *(uint32_t *)0x5a5ae0 = 0xc;
  *(uint32_t *)0x5a5ae4 = 0x1c00;
  D3DDevice_SetStreamSource(0, *(void **)0x476ad8, 0x10);
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
          D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD, NV097_COLOR_MASK_RGBA);
          *(uint32_t *)0x1fb7a4 = 0x1010101;
        } else {
          D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD, NV097_COLOR_MASK_RGB);
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
/*
 * FUN_0015c190 @ 0x15c190 — detail-objects sprite vertex expansion.
 * Register args (kb.json f0235d66, caller evidence at 0x15cb3e in
 * FUN_0015c980): count@EAX, base@ECX, out@EDX; one stack arg = the
 * 6-byte-stride input entry array.
 *
 * For each input entry it stages an 8-byte packed vertex in a local
 * buffer: bytes 0-2 are the entry's position bytes, bytes 3-5 are
 * bit-mixed from the entry's 16-bit type word (byte 3 from the high
 * byte, bytes 4-5 from both halves), and the word at +6 is a packed
 * sprite reference: sprite slot = (entry[3] >> 4) % type-block count
 * (signed IDIV), sprite frame = type->byte[0x22] + (entry[3] & 0xF) %
 * type->byte[0x23], packed as (frame << 8) | (slot << 4). It then emits
 * FOUR 8-byte copies per entry (one per sprite corner), incrementing
 * the packed word by 1 for each corner, advancing out by 0x20 and the
 * entry pointer by 6. The type block is the tag_block at base+0x44
 * (element size 0x60); the slot index is passed to
 * tag_block_get_element as a sign-extended int16 (MOVSX BX).
 *
 * Store order within each pair follows the original (second dword of
 * pairs 1-2 written before the first; pairs 3-4 in ascending order).
 */
/* 0x15c190 */
void FUN_0015c190(int count, void *base, void *out, void *entries)
{
  uint8_t *src;
  uint32_t *dst;
  int remaining;
  uint16_t w;
  uint8_t lo;
  uint8_t hi;
  int slot;
  void *elem;
  uint16_t packed;
  uint8_t staging[8];

  if (count <= 0) {
    return;
  }

  /* The original biases the entry cursor by +2 (EDI = entries + 2) and
   * addresses fields at EDI-2..EDI+3; mirror that shape. */
  src = (uint8_t *)entries + 2;
  dst = (uint32_t *)out;
  remaining = count;
  do {
    /* The original zero-extends the type word via XOR EAX,EAX; MOV AX and
     * derives the mixed bytes with 32-bit shifts of that register. */
    w = 0;
    w = *(uint16_t *)(src + 2);
    hi = (uint8_t)(w >> 8);
    lo = (uint8_t)w;

    staging[0] = src[-2];
    staging[1] = src[-1];
    staging[2] = src[0];
    staging[3] = (uint8_t)((((uint8_t)((uint32_t)w >> 13) ^ hi) & 7) ^ hi);
    staging[4] = (uint8_t)(((((uint8_t)((uint32_t)w >> 9)) ^ (uint8_t)(lo >> 3)) & 3)
                           ^ (uint8_t)((uint32_t)w >> 3));
    staging[5] = (uint8_t)(((lo >> 2) & 7) | (uint8_t)(lo << 3));

    slot = (int)(uint32_t)(src[1] >> 4) % *(int *)((uint8_t *)base + 0x44);
    elem = tag_block_get_element((uint8_t *)base + 0x44, (int16_t)slot, 0x60);
    packed = (uint16_t)((((src[1] & 0xf)
                            % (int)*(uint8_t *)((uint8_t *)elem + 0x23)
                          + (int)*(uint8_t *)((uint8_t *)elem + 0x22)) << 8)
                        | (slot << 4));

    /* Packed index word runs INC AX-style between the four corners. */
    *(uint16_t *)(staging + 6) = packed;
    dst[1] = *(uint32_t *)(staging + 4);
    dst[0] = *(uint32_t *)(staging + 0);
    packed++;
    *(uint16_t *)(staging + 6) = packed;
    dst[3] = *(uint32_t *)(staging + 4);
    dst[2] = *(uint32_t *)(staging + 0);
    packed++;
    *(uint16_t *)(staging + 6) = packed;
    dst[4] = *(uint32_t *)(staging + 0);
    dst[5] = *(uint32_t *)(staging + 4);
    packed++;
    *(uint16_t *)(staging + 6) = packed;
    dst[6] = *(uint32_t *)(staging + 0);
    dst[7] = *(uint32_t *)(staging + 4);

    dst += 8;
    src += 6;
    remaining--;
  } while (remaining != 0);
}

/*
 * FUN_0015c2b0 @ 0x15c2b0 — dead register-convention adapter for
 * D3DDevice_CreateVertexBuffer: length (s2) and usage (s3) on the stack,
 * fvf/pool/ppVertexBuffer in EDX/ECX/EAX; s1 is the ignored device
 * pointer. No XOR EAX before RET — the callee's HRESULT is the implicit
 * return value. No direct call sites; RET 0xC.
 */
/* 0x15c2b0 */
int FUN_0015c2b0(int r1, int r2, int r3, int s1, int s2, int s3)
{
  (void)s1;
  return D3DDevice_CreateVertexBuffer(s2, s3, r3, r2, (void **)r1);
}

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
 * FUN_0015c5f0 @ 0x15c5f0 — dead wrapper: calls the rasterizer profile
 * function FUN_0016fa40 with profile id 0x15 and returns. PUSH 0x15;
 * CALL 0x16fa40; POP ECX; RET. No direct call sites.
 */
/* 0x15c5f0 */
void FUN_0015c5f0(void)
{
  FUN_0016fa40(0x15);
}

/*
 * FUN_0015c600 @ 0x15c600 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::SetVertexData4f, byte-identical in shape to
 * FUN_0015a4f0 (RET 0x18, device ignored, returns S_OK). No direct
 * call sites.
 */
/* 0x15c600 */
int __stdcall FUN_0015c600(void *device, uint32_t reg, float a, float b, float c, float d)
{
  (void)device;
  D3DDevice_SetVertexData4f(reg, a, b, c, d);
  return 0;
}

/*
 * FUN_0015c650 @ 0x15c650 — dead register-convention adapter for
 * D3DVertexBuffer_Lock, byte-identical in shape to FUN_0015b6a0 (raw
 * __stdcall cast for the same reason). Returns S_OK. No direct call
 * sites; RET 8.
 */
/* 0x15c650 */
int FUN_0015c650(int r1, int r2, int r3, int s1, int s2)
{
  ((void(__stdcall *)(void *, uint32_t, uint32_t, void **, uint32_t))0x1ef100)(
      (void *)s1, s2, r3, (void **)r2, r1);
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
  D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD, NV097_COLOR_MASK_RGB);
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
 * FUN_0015c980 @ 0x15c980 — rasterizer_detail_objects_begin: expands every
 * visible detail-object cell's sprites into the detail-objects dynamic
 * vertex buffer. Same TU as 0x15c2d0 (rasterizer_xbox_detail_objects.c,
 * __FILE__ assert xref, lines 0xd7-0xd9).
 *
 * Gated on the detail-objects enable byte (0x3256dc) and window count
 * (split-screen with more than one window skips detail objects). Locks the
 * detail-objects vertex buffer (0x476ae4, 0x20000 bytes, D3D busy flag
 * 0x325652 = 3 around the D3DVertexBuffer_Lock — raw __stdcall cast, kb.json
 * stub decl is void(void)). Assert terminals are PUSH -1; CALL 0x8e2f0 =
 * system_exit(-1) — the DECOMPILER shows thunk_FUN_001029a0 here and prunes
 * the whole loop body as unreachable; the disassembly is authoritative.
 *
 * The 6-byte detail-object entry pool is element 0 of the sub-block at +0xC
 * of cell block element 0 (scenario_get()+0x24C, element size 0x40); when
 * that block is empty the original still calls tag_block_get_element on
 * NULL+0xC — preserved. Per cell (8-byte stride, palette index at +6): looks
 * up the detail-object-collection palette entry (scenario+0x3C0, element
 * size 0x30, tag index at +0xC), resolves the 'dobc' tag, then per type
 * (24-byte stride off the cell's array): clamps the entry count to the
 * remaining frame budget (0x1000 vertices total), calls FUN_0015c190
 * (count@EAX, dobc@ECX, out@EDX) to emit 4 packed vertices per entry,
 * records the first-vertex index at +0x10, advances the write cursor by the
 * UNCLAMPED count*4 (original quirk — the cursor uses the stored count read
 * before the clamp is written back), and on overflow clamps the stored
 * count at +4 and errors once per call ("too many detail object submitted").
 */
/* 0x15c980 */
void FUN_0015c980(void *view_data)
{
  void *scenario;
  void *locked;
  void *cells0;
  void *entry_pool;
  void *palette_elem;
  void *dobc;
  uint8_t *cell;
  uint8_t *sub;
  int written;
  int used;
  int cell_index;
  int type_index;
  int count;
  int budget;
  int stored;
  char warned;

  if (*(char *)0x3256dc == 0) {
    return;
  }
  if (main_get_window_count() > 1) {
    return;
  }

  scenario = global_scenario_get();
  locked = (void *)0x0;
  if (view_data == (void *)0x0) {
    display_assert(
      "detail_object_view_data",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_detail_objects.c",
      0xd7, 1);
    system_exit(-1);
  }
  if (*(void **)0x476ae4 == (void *)0x0) {
    display_assert(
      "local_d3d_vertex_buffer",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_detail_objects.c",
      0xd8, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_detail_objects.c",
      0xd9, 1);
    system_exit(-1);
  }

  *(uint16_t *)0x325652 = 3;
  ((void(__stdcall *)(void *, uint32_t, uint32_t, void **, uint32_t))0x1ef100)(
      *(void **)0x476ae4, 0, 0x20000, &locked, 0);
  *(uint16_t *)0x325652 = 0;
  if (locked == (void *)0x0) {
    return;
  }

  if (*(int *)((uint8_t *)scenario_get() + 0x24c) != 0) {
    cells0 = tag_block_get_element((uint8_t *)scenario_get() + 0x24c, 0, 0x40);
  } else {
    cells0 = (void *)0x0;
  }
  entry_pool = tag_block_get_element((uint8_t *)cells0 + 0xc, 0, 6);

  written = 0;
  used = 0;
  warned = 0;
  cell_index = 0;
  if (*(int16_t *)((uint8_t *)view_data + 4) <= 0) {
    return;
  }

  do {
    cell = *(uint8_t **)view_data + (int16_t)cell_index * 8;
    palette_elem = tag_block_get_element(
        (uint8_t *)scenario + 0x3c0, *(int16_t *)(cell + 6), 0x30);
    dobc = tag_get(0x646f6263, *(int *)((uint8_t *)palette_elem + 0xc));

    type_index = 0;
    if (*(int16_t *)(cell + 4) > 0) {
      do {
        sub = *(uint8_t **)cell + (int16_t)type_index * 24;
        count = *(int *)(sub + 4);
        budget = 0x1000 - used;
        if (count > budget) {
          count = budget;
        }
        FUN_0015c190(
            count,
            dobc,
            (uint8_t *)locked + written * 8,
            (uint8_t *)entry_pool + *(int *)sub * 6);
        /* The cursor advances by the stored count read BEFORE the clamp
         * below writes back — original quirk, preserved. */
        stored = *(int *)(sub + 4);
        *(int *)(sub + 0x10) = written;
        written += stored * 4;
        if (stored > count) {
          *(int *)(sub + 4) = count;
          if (warned == 0) {
            warned = 1;
            error(2,
                  "### ERROR too many detail object submitted for frame "
                  "(max=#%d)",
                  0x1000);
          }
        }
        used += count;
        type_index++;
      } while ((int16_t)type_index < *(int16_t *)(cell + 4));
    }
    cell_index++;
  } while ((int16_t)cell_index < *(int16_t *)((uint8_t *)view_data + 4));
}

/* 0x15cbb0
 *
 * rasterizer_detail_objects_draw (assert strings). Same TU as 0x15c2d0
 * (rasterizer_xbox_detail_objects.c). Draws every visible detail-object cell
 * for each detail-object-collection referenced by the view data:
 *
 *   detail_object_view_data (param): { void *entries; short entry_count; }
 *   entry (stride 8): +0 cell array ptr, +4 short cell_count,
 *                     +6 short palette index (into scenario+0x3c0 block)
 *   cell (stride 0x18): +4 int detail_object_count, +8/+0xa int16
 *                     cell_x/cell_y, +0xc float cell_z, +0x10 int
 *                     internal__first_vertex_index, +0x14 float* z_ref_vector
 *
 * Per collection it binds the sprite bitmap, fills two contiguous vec4
 * constant blocks — type data (16 slots, VSH offset -0x4b) from the
 * collection's type_definitions (block at collection+0x44, stride 0x60) and
 * frame data (128 slots, VSH offset -0x24) from the bitmap's sequence/sprite
 * blocks — then issues one D3DPT_QUADLIST DrawVertices per cell
 * (detail_object_count * 4 vertices from the shared dynamic VB).
 *
 * Disasm-verified details (delinked/functions/0015cbb0.obj):
 *   - bitmap tag ref at collection+0x40 (Ghidra draft showed +0x20);
 *     type block at collection+0x44 (draft showed +0x22).
 *   - FPU subtraction directions: range = [type+0x34] - [type+0x30]
 *     (FLD 0x34; FSUB 0x30); frame spans = [+0xc]-[+0x8] and
 *     [+0x14]-[+0x10].
 *   - reciprocal only when range > 0.0f (FCOMP vs 0.0; TEST AH,0x41; JNE).
 *   - SetVertexData4f reg 2: int16 cell_x/cell_y <<3 then FILD
 *     (push-then-fstp floats); cell_z * 8.0f (DAT_00253f78).
 *   - DrawVertices(8 /-D3DPT_QUADLIST-/, cell+0x10, (cell+4) << 2) — Ghidra
 *     dropped all three args.
 *   - loop counters are (short)-truncated each iteration (MOVSX re-loads);
 *     block counts are re-read from memory every iteration.
 *   - `success` ([EBP-1]) is the alternating d3d debug-check latch, same
 *     idiom as rasterizer.c: on the false path FUN_00167ff0(0, call_text).
 *
 * Globals: 0x3256dc byte detail-objects-draw enable; 0x476ab0
 * global_d3d_device; scenario+0x3c0 detail_object_collection_palette block.
 */
void FUN_0015cbb0(void *detail_object_view_data)
{
  float frame_data[512]; /* 128 vec4 frame slots  (EBP-0x924)          */
  float type_data[64]; /* 16 vec4 type slots    (EBP-0x124)          */
  char *scenario;
  char *palette;
  char *entry;
  char *collection;
  char *bitmap;
  char *type_elem;
  char *seq_elem;
  char *sprite;
  char *sprite_block;
  char *cell;
  short *bitmap_data;
  float *vsh;
  float *z_ref;
  void *type_block;
  float range;
  float frame_scale;
  int i;
  int j; /* (short)-truncated view of counter (register-resident) */
  int m;
  int counter; /* full loop counter, memory-resident (EBP-0x14)      */
  int frame_count; /* reused as the cell index in the draw loop (EBP-0x8) */
  char success;

  success = 1;
  if (*(char *)0x3256dc == 0) {
    return;
  }
  if (main_get_window_count() > 1) {
    return;
  }
  scenario = (char *)global_scenario_get();
  if (detail_object_view_data == 0) {
    display_assert(
      "detail_object_view_data",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_detail_objects.c",
      0x132, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_detail_objects.c",
      0x133, 1);
    system_exit(-1);
  }
  i = 0;
  if (*(short *)((char *)detail_object_view_data + 4) > 0) {
    palette = scenario + 0x3c0;
    do {
      entry = *(char **)detail_object_view_data + (short)i * 8;
      type_elem =
        (char *)tag_block_get_element(palette, *(short *)(entry + 6), 0x30);
      collection = (char *)tag_get(0x646f6263, *(int *)(type_elem + 0xc));
      bitmap = (char *)tag_get(0x6269746d, *(int *)(collection + 0x40));
      rasterizer_set_texture(0, 0, 1, *(int *)(collection + 0x40), 0);
      D3DDevice_SetTextureStageState(0, 0xa, 3);
      D3DDevice_SetTextureStageState(0, 0xb, 3);
      D3DDevice_SetTextureStageState(0, 0xd, 2);
      D3DDevice_SetTextureStageState(0, 0xe, 2);
      D3DDevice_SetTextureStageState(0, 0xf, 2);
      if (*(unsigned short *)collection != 0xffff) {
        FUN_00178b40(0x21, 0xb, *(unsigned short *)collection);
      }

      /* Type data: one vec4 per type definition (block at collection+0x44,
       * stride 0x60). */
      frame_count = 0;
      counter = 0;
      j = 0;
      type_block = collection + 0x44;
      if (*(int *)type_block > 0) {
        do {
          type_elem = (char *)tag_block_get_element(type_block, j, 0x60);
          seq_elem = (char *)tag_block_get_element(
            bitmap + 0x54, *(unsigned char *)(type_elem + 0x20), 0x40);
          bitmap_data = (short *)tag_block_get_element(
            bitmap + 0x60, *(short *)(seq_elem + 0x20), 0x30);
          range = *(float *)(type_elem + 0x34) - *(float *)(type_elem + 0x30);
          frame_scale = *(float *)(type_elem + 0x38);
          if (range > 0.0f) {
            range = 1.0f / range;
          }
          vsh = &type_data[j * 4];
          vsh[0] = range * *(float *)(type_elem + 0x34);
          vsh[1] = -range;
          vsh[2] = (float)bitmap_data[2] * frame_scale;
          vsh[3] = (float)bitmap_data[3] * frame_scale;
          counter = counter + 1;
          j = (short)counter;
        } while (j < *(int *)type_block);
      }

      /* Frame data: one vec4 per sprite across every sequence of the sprite
       * bitmap (sequences block at bitmap+0x54, sprites at seq+0x34). */
      counter = 0;
      j = 0;
      if (*(int *)(bitmap + 0x54) > 0) {
        do {
          seq_elem = (char *)tag_block_get_element(bitmap + 0x54, j, 0x40);
          sprite_block = seq_elem + 0x34;
          m = 0;
          if (*(int *)sprite_block > 0) {
            do {
              sprite =
                (char *)tag_block_get_element(sprite_block, (short)m, 0x20);
              /* validation side effect only (bitmap index range check) */
              tag_block_get_element(bitmap + 0x60, *(short *)sprite, 0x30);
              vsh = &frame_data[(short)frame_count * 4];
              frame_count = frame_count + 1;
              vsh[0] = *(float *)(sprite + 8);
              vsh[1] = *(float *)(sprite + 0x10);
              vsh[2] = *(float *)(sprite + 0xc) - *(float *)(sprite + 8);
              vsh[3] = *(float *)(sprite + 0x14) - *(float *)(sprite + 0x10);
              m = m + 1;
            } while ((short)m < *(int *)sprite_block);
          }
          counter = counter + 1;
          j = (short)counter;
        } while (j < *(int *)(bitmap + 0x54));
      }

      D3DDevice_SetVertexShaderConstant(-0x4b, type_data, *(int *)type_block);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(
          0, "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, "
             "VSH_CONSTANTS__DETAILOBJ_TYPEDATA_OFFSET, "
             "vsh_constants__detailobj_typedata, "
             "collection_definition->type_definitions.count)");
      }
      D3DDevice_SetVertexShaderConstant(-0x24, frame_data, (short)frame_count);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(
          0, "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, "
             "VSH_CONSTANTS__DETAILOBJ_FRAMEDATA_OFFSET, "
             "vsh_constants__detailobj_framedata, frame_count)");
      }

      /* Draw every cell of this entry: one quad list per cell. */
      frame_count = 0;
      if (*(short *)(entry + 4) > 0) {
        do {
          cell = *(char **)entry + (short)frame_count * 0x18;
          if (*(int *)(cell + 0x14) == 0) {
            display_assert(
              "cell->z_reference_vector",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_detail_"
              "objects.c",
              0x18a, 1);
            system_exit(-1);
          }
          D3DDevice_SetVertexData4f(2, (float)(*(short *)(cell + 8) << 3),
                                    (float)(*(short *)(cell + 0xa) << 3),
                                    *(float *)(cell + 0xc) * 8.0f, 1.0f);
          if (success != 0) {
            success = 1;
          } else {
            success = 0;
            FUN_00167ff0(
              0, "IDirect3DDevice8_SetVertexData4f(global_d3d_device, 2, "
                 "(real)(cell->cell_x*DETAIL_OBJECT_CELL_SIZE), "
                 "(real)(cell->cell_y*DETAIL_OBJECT_CELL_SIZE), "
                 "(cell->cell_z*(real)DETAIL_OBJECT_CELL_SIZE), 1.0f)");
          }
          z_ref = *(float **)(cell + 0x14);
          D3DDevice_SetVertexData4f(3, z_ref[0], z_ref[1], z_ref[2], z_ref[3]);
          if (success != 0) {
            success = 1;
          } else {
            success = 0;
            FUN_00167ff0(
              0, "IDirect3DDevice8_SetVertexData4f(global_d3d_device, 3, "
                 "cell->z_reference_vector->i, cell->z_reference_vector->j, "
                 "cell->z_reference_vector->k, cell->z_reference_vector->l)");
          }
          D3DDevice_DrawVertices(8, *(uint32_t *)(cell + 0x10),
                                 *(uint32_t *)(cell + 4) << 2);
          if (success != 0) {
            success = 1;
          } else {
            success = 0;
            FUN_00167ff0(0,
                         "IDirect3DDevice8_DrawVertices(global_d3d_device, "
                         "D3DPT_QUADLIST, cell->internal__first_vertex_index, "
                         "cell->detail_object_count*"
                         "NUMBER_OF_VERTICES_PER_QUADRILATERAL)");
          }
          frame_count = frame_count + 1;
        } while ((short)frame_count < *(short *)(entry + 4));
      }
      i = i + 1;
    } while ((short)i < *(short *)((char *)detail_object_view_data + 4));
    if (success == 0) {
      error(2, "### ERROR rasterizer_detail_objects_draw failed");
    }
  }
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

/*
 * FUN_0015d020 @ 0x15d020 — dead register-convention adapter for
 * D3DDevice_CreateVertexBuffer, byte-identical in shape to FUN_0015c2b0:
 * length (s2) / usage (s3) on the stack, fvf/pool/ppVertexBuffer in
 * EDX/ECX/EAX, s1 ignored, callee HRESULT is the implicit return. No
 * direct call sites; RET 0xC.
 */
/* 0x15d020 */
int FUN_0015d020(int r1, int r2, int r3, int s1, int s2, int s3)
{
  (void)s1;
  return D3DDevice_CreateVertexBuffer(s2, s3, r3, r2, (void **)r1);
}

/*
 * FUN_0015d040 @ 0x15d040 — dead register-convention adapter for
 * D3DDevice_CreateIndexBuffer (0x1eef80, kb.json stub decl is void(void)
 * so the call uses a raw __stdcall cast): length (s2) / usage (s3) on the
 * stack, format/pool/ppIndexBuffer in EDX/ECX/EAX, s1 ignored, callee
 * HRESULT is the implicit return. No direct call sites; RET 0xC.
 */
/* 0x15d040 */
int FUN_0015d040(int r1, int r2, int r3, int s1, int s2, int s3)
{
  (void)s1;
  return ((int(__stdcall *)(uint32_t, uint32_t, uint32_t, uint32_t, void **))0x1eef80)(
      s2, s3, r3, r2, (void **)r1);
}

static const char kDrawPrimitivesFile[] =
  "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_draw_primitives.c";

/* 0x15d060
 *
 * Per-frame reset of the dynamic-primitive submission cursors.  Walks the 12
 * dynamic_vertices.groups[] records (base 0x476ae8, stride 0x14; +0
 * current_offset, +4 limit, +8 capacity, +0x10 byte dirty flag) and resets
 * them, then clears the dynamic-triangle cursor (0x47dbe4) / record count
 * (0x47dbe0) and sets the byte latch 0x47dbec.  The walk pointer starts at
 * entry[0]+0x10 (0x476af8, the byte field); offsets -0x10/-0xc/-0x8 reach
 * current_offset/limit/capacity.  End sentinel (exclusive): 0x476be8.
 *
 * Two modes on the flag byte 0x32571b:
 *  - 0 (cold reset): zero every group's current_offset, set its dirty byte,
 *    and clear 0x47abd8 (store order after the loop: 0x47dbe4=0, 0x47dbec=1,
 *    0x47abd8=0, 0x47dbe0=0 -- shared-tail block layout from the disasm).
 *  - nonzero (windowed): validate global_window_parameters.window_index
 *    (short @0x5a5bc2) in [0, main_get_window_count()).  window 0 resets
 *    offset+dirty like the cold path; later windows instead scale the
 *    group limit: limit = ((window_index+1) * capacity) / window_count,
 *    with main_get_window_count() re-called and window_index re-read from
 *    the global EVERY iteration (one load feeds both the multiply and the
 *    post-loop window_index==0 test -- CX in the disasm).  0x47abd8 is
 *    cleared only when window_index==0.
 *
 * Assert tails here are display_assert(...,1) then `push -1; CALL 0x1029a0`
 * (halt_and_catch_fire) per the delinked relocs at +0x32/+0x63 -- NOT
 * system_exit like the 0x15d170+ cluster; the shared void decl cannot emit
 * the -1 push (known fixed ~2-insn gap per site).  Execution falls through
 * after the call in the original codegen.  0x47dbec is a BYTE store
 * (movb $1), not an int.
 *
 * 0x15d060 / rasterizer_decals.obj
 */
void FUN_0015d060(void)
{
  short window_index; /* CX: re-read from 0x5a5bc2 each loop iteration */
  short window_count;
  char *entry; /* ESI/EAX: walks entry[i]+0x10 across the group table */

  if (*(char *)0x32571b != 0) {
    if (*(short *)0x5a5bc2 < 0) {
      display_assert("global_window_parameters.window_index>=0",
                     kDrawPrimitivesFile, 0xc6, 1);
      system_exit(-1);
    }
    window_count = main_get_window_count();
    if (window_count <= *(short *)0x5a5bc2) {
      display_assert(
        "global_window_parameters.window_index<main_get_window_count()",
        kDrawPrimitivesFile, 0xc7, 1);
      system_exit(-1);
    }
    window_index = *(short *)0x5a5bc2;
    entry = (char *)0x476af8;
    do {
      if (window_index == 0) {
        *(int *)(entry - 0x10) = 0;
        *entry = 1;
      } else {
        window_count = main_get_window_count();
        window_index = *(short *)0x5a5bc2;
        *(int *)(entry - 0xc) =
          ((window_index + 1) * *(int *)(entry - 8)) / (int)window_count;
      }
      entry = entry + 0x14;
    } while ((int)entry < 0x476be8);
    *(int *)0x47dbe4 = 0;
    *(char *)0x47dbec = 1;
    if (window_index == 0) {
      *(int *)0x47abd8 = 0;
      *(int *)0x47dbe0 = 0;
      return;
    }
  } else {
    entry = (char *)0x476af8;
    do {
      *(int *)(entry - 0x10) = 0;
      *entry = 1;
      entry = entry + 0x14;
    } while ((int)entry < 0x476be8);
    *(int *)0x47dbe4 = 0;
    *(char *)0x47dbec = 1;
    *(int *)0x47abd8 = 0;
  }
  *(int *)0x47dbe0 = 0;
}

/*
 * FUN_0015d160 @ 0x15d160 — empty function in the binary (single RET;
 * one live call site in this TU).
 */
/* 0x15d160 */
void FUN_0015d160(void)
{
}

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
 * Returns the record index (the widget HANDLE = pre-increment record count:
 * EAX is loaded from [0x47dbe0] at 0x15d20a and the success exit takes
 * `jne 0x15d28f`, SKIPPING the `mov eax,edi` at 0x15d28d), or -1 on the
 * count<=0 / overflow paths (edi is seeded -1 and reaches EAX only there).
 * Callers pass this handle to 0x15ea70 (rasterizer_widget_begin), which
 * asserts handle < record count — an earlier lift returned the advanced
 * cursor instead, firing that assert (draw_primitives.c:338) in-game
 * (fixed 2026-07-12).
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
  int handle;

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
      handle = *(int *)0x47dbe0;
      *(int *)(0x47abe0 + handle * 0xc) = *(int *)0x47dbe4;
      *(int *)(0x47abe4 + handle * 0xc) = count;
      *(int *)0x47dbe4 = *(int *)0x47dbe4 + count;
      *(int *)0x47dbe0 = handle + 1;
      if (*(short *)0x3256ba == 2) {
        *(int *)0x5a5538 = *(int *)0x5a5538 + count;
        *(int *)0x5a553c = *(int *)0x5a553c + 1;
      }
      return handle;
    } else if (*(char *)0x47dbf4 == 0) {
      error(2,
            "### ERROR too many dynamic triangles requested from rasterizer");
      *(char *)0x47dbf4 = 1;
    }
  }
  return result;
}

/*
 * FUN_0015d2a0 @ 0x15d2a0 — dead register-convention adapter for
 * IDirect3DDevice8::DrawPrimitive, byte-identical in shape to
 * FUN_0015b5a0: primitive type in EAX, device (a1) ignored, primitive
 * count (a3) converted to a vertex count via the per-type table at
 * 0x29f7e8, forwarded to D3DDevice_DrawVertices. Returns S_OK. No
 * direct call sites; RET 0xC.
 */
/* 0x15d2a0 */
int FUN_0015d2a0(int index, int a1, int a2, int a3)
{
  (void)a1;
  D3DDevice_DrawVertices(
      index,
      a2,
      *(uint32_t *)(0x29f7e8 + index * 8) * a3
        + *(uint32_t *)(0x29f7ec + index * 8));
  return 0;
}

/*
 * FUN_0015d2d0 @ 0x15d2d0 — dead register-convention adapter for
 * IDirect3DDevice8::DrawIndexedPrimitive: primitive type in EAX, index
 * offset (in 16-bit indices) in ECX, primitive count (s3) converted to
 * an index count via the per-type table at 0x29f7e8, index data pointer
 * built from the global index-buffer base at 0x1fb494. Callee
 * D3DDevice_DrawIndexedVertices (0x1ecf90) has a void(void) kb.json stub
 * decl, so the call uses a raw __stdcall cast. Returns S_OK. No direct
 * call sites; RET 0x10.
 */
/* 0x15d2d0 */
int FUN_0015d2d0(int index, int a2, int s1, int s2, int s3, int s4)
{
  (void)s1;
  (void)s2;
  (void)s4;
  ((void(__stdcall *)(int, uint32_t, void *))0x1ecf90)(
      index,
      *(uint32_t *)(0x29f7e8 + index * 8) * s3
        + *(uint32_t *)(0x29f7ec + index * 8),
      (void *)(*(uint32_t *)0x1fb494 + a2 * 2));
  return 0;
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
 * cdecl stack params, NOT register args).  Returns the record index (the
 * handle = pre-increment record count: `mov ecx,[0x47abd8]; mov eax,ecx` at
 * 0x15d3f3), or -1 (`or eax,-1` at 0x15d472) on the count<=0 / overflow
 * paths.  Callers reach this via the 0x17c9b0 thunk (kb name
 * rasterizer_widget_set_zbuffer_enable, a misnomer) and pass the handle to
 * 0x15ec50 — a void lift here left garbage in EAX, firing the
 * dynamic_vertex_buffer_index assert (draw_primitives.c:536) in-game
 * (fixed 2026-07-12, same class as sibling FUN_0015d170).
 *
 * NB: like the sibling FUN_0015d170, every assert path is
 * display_assert(...); system_exit(-1); (the pristine XBE's combined
 * `add esp,0x14` after each site proves the second call takes one arg -- it is
 * system_exit(-1), NOT the arg-less halt_and_catch_fire the Ghidra draft
 * named). The 0x3256ba stat-mode gate is a 16-bit compare.
 *
 * 0x15d310 / rasterizer_decals.obj
 */
int FUN_0015d310(short type, int count)
{
  int iType;
  int rec;
  int handle;

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
      handle = *(int *)0x47abd8;
      rec = handle * 0x10;
      *(short *)(0x476bd8 + rec) = type;
      *(unsigned int *)(0x476bdc + rec) = *(int *)(0x476ae8 + iType * 0x14);
      *(int *)(0x476be0 + rec) = count;
      *(int *)(0x476ae8 + iType * 0x14) =
        *(int *)(0x476ae8 + iType * 0x14) + count;
      *(int *)0x47abd8 = handle + 1;
      if (*(short *)0x3256ba == 2) {
        *(int *)0x5a5530 = *(int *)0x5a5530 + count;
        *(int *)0x5a5534 = *(int *)0x5a5534 + 1;
      }
      return handle;
    } else if (*(char *)0x47dbf5 == 0) {
      error(2, "### ERROR too many dynamic vertices requested from rasterizer");
      *(char *)0x47dbf5 = 1;
    }
  }
  return -1;
}

/* 0x15d480
 *
 * Query the short field at +0 of dynamic-vertices reservation record
 * `dynamic_vertex_buffer_index` (the 0x10-stride table at 0x476bd8 filled by
 * FUN_0015d310; +0 is the record's vertex-type short).  Validates the index:
 * NONE (-1) emits a non-fatal warning and returns NONE; any other
 * out-of-range value asserts.  Asserts cite
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_draw_primitives.c (grouped
 * into rasterizer_decals.obj).
 *
 * cdecl, one int32 stack arg at [ESP+4]; returns short in AX (kb.json decl
 * was void(void) - corrected from disasm: MOV ESI,[EBP+8]; SHL ESI,4;
 * MOV AX,[ESI+0x476bd8]).  The pristine XBE hoists -1 into EDI at the
 * prologue (OR EDI,0xFFFFFFFF) and returns it via MOV AX,DI on the NONE
 * path; `none_result` below exists to reproduce that callee-saved
 * materialization.
 *
 * Globals:
 *   0x476ab0  void *  - global_d3d_device (asserted non-NULL)
 *   0x47abd8  int     - dynamic_vertices reservation record_count (bound)
 *   0x476bd8  short   - reservation table base, stride 0x10, +0 short field
 */
short FUN_0015d480(int dynamic_vertex_buffer_index)
{
  short none_result = -1;

  if (*(void **)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "draw_primitives.c",
                   0x1de, 1);
    system_exit(-1);
  }
  if (dynamic_vertex_buffer_index != -1) {
    if (dynamic_vertex_buffer_index < 0) {
      display_assert("dynamic_vertex_buffer_index>=0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "draw_primitives.c",
                     0x1e4, 1);
      system_exit(-1);
    }
    if (dynamic_vertex_buffer_index >= *(int *)0x47abd8) {
      display_assert(
        "dynamic_vertex_buffer_index<dynamic_vertices.buffer_count",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
        "draw_primitives.c",
        0x1e5, 1);
      system_exit(-1);
    }
    return *(short *)((dynamic_vertex_buffer_index << 4) + 0x476bd8);
  }
  error(2, "### WARNING tried to query dynamic vertices with index=NONE");
  return none_result;
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

/*
 * FUN_00158df0 (0x158df0) — rasterizer scene render begin
 *
 * Ghidra mis-declares this as void(void); the real ABI is a single cdecl
 * stack pointer parameter (`parameters`, a ushort/struct pointer). Copies the
 * 600-byte (0x96-dword) parameters block into the global mirror at 0x5a5bc0,
 * updates the "same render target" flag, resolves the clear color, then drives
 * the per-frame render-begin call chain and installs the initial frustum-z /
 * fill-mode state.
 *
 * Globals (hardcoded, not in kb.json; widths taken from disasm store/compare
 * operand sizes, NOT the decompiler):
 *   0x476ab0  device pointer, global_d3d_device (asserted non-NULL)
 *   0x476ab8  BYTE same-target flag (mov [..],al — skips heavy setup when 1)
 *   0x476abc  WORD previous parameters[1] (cmp/mov word; sentinel 0xffff)
 *   0x3256bc  WORD mode flag (cmp word ptr,1 forces the clear color to 0)
 *   0x3256be  BYTE wireframe flag (neg/sbb ternary picks D3DFILL_WIREFRAME)
 *   0x5a5bc0  600-byte mirror of the parameters block
 *   0x5a5dac  float color at offset 0x1ec inside the mirror, converted to pixel32
 *
 * Field offsets (parameters is a ushort pointer):
 *   parameters[0]  byte +0x00  render target index (only 0 or 1 supported)
 *   parameters[1]  byte +0x02  target id (0xffff = special/main target)
 *   byte +0x05                 bool selector for FUN_00158140 arg4
 *   float +0x44                camera.z_near
 *
 * All four assert terminals are PUSH -1 (or PUSH EDI with EDI still -1 from
 * the OR EDI,-1 at 0x158e54) then CALL 0x8e2f0 = system_exit(-1) — NOT
 * halt_and_catch_fire (the first parked lift substituted hcf at all four
 * sites; review-gate REJECT, same anti-pattern as FUN_0015c680).
 */
/* 0x158df0 */
void FUN_00158df0(unsigned short *parameters)
{
  unsigned int color_pixel;
  char same_target;

  if (parameters == 0) {
    display_assert("parameters",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x547, true);
    system_exit(-1);
  }
  if (*(void **)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x548, true);
    system_exit(-1);
  }

  /* Copy 0x96 dwords (600 bytes) from parameters into the global mirror.
   * MSVC lowers the constant-size memcpy to `rep movsl` (ECX=0x96). */
  memcpy((void *)0x5a5bc0, parameters, 0x258);

  /* Same-target detection: flag set only when we have rendered before
   * (prev word != 0xffff sentinel) and the current target id IS 0xffff.
   * The flag is computed in AL and branched on directly (test al,al) —
   * the branch below uses the local, not a re-read of the byte global. */
  if (*(unsigned short *)0x476abc != 0xffff && parameters[1] == 0xffff) {
    same_target = 1;
  } else {
    same_target = 0;
  }
  *(char *)0x476ab8 = same_target;
  *(unsigned short *)0x476abc = parameters[1];

  if (same_target == 0) {
    rasterizer_memory_pool_reset();
    FUN_0015d060();
    rasterizer_transparent_geometry_begin();
    FUN_001659f0();
    FUN_001812b0();
  }
  /* Disasm: push 0 before 0x1792c0 and 0x1592e0, no push before 0x16f880,
   * push 0 before 0x158ae0, push (parameters+0x1e8) before 0x17c8f0;
   * one deferred ADD ESP,0x10 cleans all four dword args. The decompiler
   * dropped the first, second and fifth arguments. */
  FUN_001792C0(0);
  FUN_001592e0(0);
  FUN_0016f880();
  FUN_00158ae0(0);
  rasterizer_environment_fog_screen_end((char *)parameters + 0x1e8);

  if (*(short *)0x3256bc == 1) {
    color_pixel = 0;
  } else {
    color_pixel = FUN_000d1dd0((float *)0x5a5dac);
  }

  if (*parameters == 0 || *parameters == 1) {
    FUN_0016f910(0);
    /* arg4 is a byte-wide bool; Ghidra's CONCAT31(extraout_EAX>>8,...) is an
     * artifact of the bool being built in EAX — the upper bytes are garbage. */
    FUN_00158140((unsigned int)*parameters, 0, color_pixel,
                 (*((char *)parameters + 5) == 0), 1);
    FUN_0016fa40(0);
    if (*parameters == 0 && *(float *)((char *)parameters + 0x44) == 0.0f) {
      display_assert("parameters->camera.z_near!=0.0f",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x587, true);
      system_exit(-1);
    }
  } else {
    display_assert(
      "### ERROR unsupported rasterizer target for scene rendering",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0x58c, true);
    system_exit(-1);
  }

  rasterizer_set_frustum_z(-1.0f, -1.0f);
  /* Fill mode: byte 0x3256be selects D3DFILL_WIREFRAME (0x1b01) over
   * D3DFILL_SOLID (0x1b02); original lowers this ternary to neg/sbb/add. */
  D3DDevice_SetRenderState_FillMode(
    (*(unsigned char *)0x3256be != 0) ? 0x1b01 : 0x1b02);
}
