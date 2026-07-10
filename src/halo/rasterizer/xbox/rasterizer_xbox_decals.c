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
