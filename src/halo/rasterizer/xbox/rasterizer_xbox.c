/*
 * FUN_00155350 @ 0x155350 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::Present: pDummy2/pDummy1/pDestPointsArray arrive in
 * EAX/ECX/EDX, the device argument (s1) is ignored and pSourceRectsArray
 * (s2) is on the stack. Returns S_OK. No direct call sites; RET 0x8.
 */
/* 0x155350 */
int FUN_00155350(int r1, int r2, int r3, int s1, int s2)
{
  (void)s1;
  D3DDevice_Present((void *)s2, (void *)r3, (void *)r2, (void *)r1);
  return 0;
}

/*
 * FUN_00155380 @ 0x155380 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::CreateTexture: format/pool/ppTexture arrive in
 * EDX/ECX/EAX, the device argument (s1) is ignored, width/height/levels/
 * usage (s2-s5) are on the stack. EAX passes through from the callee (no
 * explicit return). No direct call sites; RET 0x14.
 */
/* 0x155380 */
void FUN_00155380(int r1, int r2, int r3, int s1, int s2, int s3, int s4,
                  int s5)
{
  (void)s1;
  D3DDevice_CreateTexture(s2, s3, s4, s5, r3, r2, (void *)r1);
}

/*
 * FUN_001553a0 @ 0x1553a0 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::CreateVolumeTexture: format/pool/ppVolumeTexture in
 * EDX/ECX/EAX, device (s1) ignored, width/height/depth/levels/usage
 * (s2-s6) on the stack. EAX passes through from the callee. No direct
 * call sites; RET 0x18.
 */
/* 0x1553a0 */
void FUN_001553a0(int r1, int r2, int r3, int s1, int s2, int s3, int s4,
                  int s5, int s6)
{
  (void)s1;
  D3DDevice_CreateVolumeTexture(s2, s3, s4, s5, s6, r3, r2, (void *)r1);
}

/*
 * FUN_001553d0 @ 0x1553d0 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::CreateCubeTexture: format/pool/ppCubeTexture in
 * EDX/ECX/EAX, device (s1) ignored, edge_length/levels/usage (s2-s4) on
 * the stack. EAX passes through from the callee. No direct call sites;
 * RET 0x10.
 */
/* 0x1553d0 */
void FUN_001553d0(int r1, int r2, int r3, int s1, int s2, int s3, int s4)
{
  (void)s1;
  D3DDevice_CreateCubeTexture(s2, s3, s4, r3, r2, (void *)r1);
}

/*
 * rasterizer_preinitialize (0x1553f0)
 *
 * Creates the IDirect3D8 object and performs a probe CreateDevice call to
 * verify that D3D hardware is usable.  The device is created with a fixed
 * 640x480 back-buffer, D3DSWAPEFFECT_DISCARD, fullscreen, auto depth/stencil
 * D3DFMT_D24S8 (0x2a), and BehaviorFlags 0x40
 * (D3DCREATE_HARDWARE_VERTEXPROCESSING on Xbox).  If CreateDevice succeeds,
 * device caps are fetched, Present is called, then the device and IDirect3D8
 * object are released.  On any failure an error is logged.
 *
 * Return value: `XOR BL,BL` / `MOV BL,1` accumulate a success flag and
 * `MOV AL,BL` precedes both RETs (0x15542b and 0x155506), so this returns a
 * char, not void (the old kb decl was a void-EAX misread).  The only caller,
 * shell_xbox.c, discards it.
 *
 * Globals (not in kb.json, hardcoded):
 *   0x476a50  void *  – IDirect3D8 object pointer
 *   0x476ab0  void *  – IDirect3DDevice8 pointer
 *   0x5a59e0  –       – D3DCAPS8 output buffer
 *
 * Present_parameters layout confirmed against D3DPRESENT_PARAMETERS from
 * third_party/xbox/d3d8types.h (0x34 bytes total).
 *
 * VC71 86.6% (95/92 insns) is a block-placement ceiling, not a logic gap.
 * The original keeps the success flag live in BL across every reporter call
 * (XOR BL,BL / MOV BL,1 ... MOV AL,BL) and so pushes EBX in the prologue;
 * VC71 /O2 constant-propagates the flag on the failure paths (XOR AL,AL /
 * MOV AL,1) and therefore sinks PUSH EBX into the success block.  It also
 * coalesces the two ADD ESP,8 error cleanups into one ADD ESP,0x10, because
 * the duplicated tail below is not a separate basic block the way the
 * original's LAB_0015541a is.  Two rewrites that reproduce the original's
 * shared-tail layout were measured and both lost: hoisting the success body
 * behind a forward goto scored 78.0%, and making the "preinitialize failed"
 * tail a shared backward-jump target scored 83.5%.  Duplicating that tail
 * into the create-object failure path (the current form) is the best
 * measured shape and the permuter found nothing better.  Accepted at
 * ceiling.
 */

/*
 * Minimal mirror of D3DPRESENT_PARAMETERS fields used here.
 * Layout matches d3d8types.h exactly (4-byte aligned, no pack).
 * Total: 0x34 bytes = 13 DWORD fields.
 */
#pragma pack(push, 4)
typedef struct {
  unsigned int BackBufferWidth; /* +0x00 */
  unsigned int BackBufferHeight; /* +0x04 */
  unsigned int BackBufferFormat; /* +0x08 (D3DFORMAT enum) */
  unsigned int BackBufferCount; /* +0x0c */
  unsigned int MultiSampleType; /* +0x10 (D3DMULTISAMPLE_TYPE) */
  unsigned int SwapEffect; /* +0x14 (D3DSWAPEFFECT enum) */
  void *hDeviceWindow; /* +0x18 (HWND) */
  unsigned int Windowed; /* +0x1c (BOOL) */
  unsigned int EnableAutoDepthStencil; /* +0x20 (BOOL) */
  unsigned int AutoDepthStencilFormat; /* +0x24 (D3DFORMAT enum) */
  unsigned int Flags; /* +0x28 (DWORD) */
  unsigned int FullScreen_RefreshRateInHz; /* +0x2c (UINT) */
  unsigned int FullScreen_PresentationInterval; /* +0x30 (UINT) */
} d3d_present_parameters_t;
#pragma pack(pop)

/* 0x1553f0 */
char rasterizer_preinitialize(void)
{
  d3d_present_parameters_t d3dpp;
  char success;
  int hr;

  *(void **)0x476a50 = Direct3DCreate8(0);
  if (*(void **)0x476a50 == 0) {
    error(2, "### ERROR failed to create D3D object");
    success = 0;
  } else {
    /* Zero-fill D3DPRESENT_PARAMETERS (0x34 bytes) then set used fields */
    csmemset(&d3dpp, 0, 0x34);
    d3dpp.BackBufferWidth = 0x280; /* 640 */
    d3dpp.BackBufferHeight = 0x1e0; /* 480 */
    d3dpp.BackBufferFormat = 6; /* D3DFMT_A8R8G8B8 */
    d3dpp.SwapEffect = 1; /* D3DSWAPEFFECT_DISCARD */
    d3dpp.Windowed = 0; /* fullscreen */
    d3dpp.EnableAutoDepthStencil = 1;
    d3dpp.AutoDepthStencilFormat = 0x2a; /* D3DFMT_D24S8 */
    d3dpp.Flags = 1;
    d3dpp.FullScreen_PresentationInterval = 0;

    hr = Direct3D_CreateDevice(0, 1, 0, 0x40, &d3dpp, (void **)0x476ab0);
    if (hr >= 0) {
      success = 1;
    } else {
      success = 0;
      FUN_00167ff0(hr, "IDirect3D8_CreateDevice(d3d, D3DADAPTER_DEFAULT, "
                       "D3DDEVTYPE_HAL, NULL, "
                       "RASTERIZER_DEVICE_CREATION_FLAGS, "
                       "&d3d_present_parameters, &global_d3d_device)");
    }

    if (*(void **)0x476ab0 == 0) {
      *(void **)0x476ab0 = 0;
      error(2, "### ERROR failed to create D3D device");
    } else if (success != 0) {
      D3DDevice_GetDeviceCaps((void *)0x5a59e0);
      D3DDevice_Present(0, 0, 0, 0);
      success = 1;
      /* Probe device torn down again — the real device is created later by
       * rasterizer_initialize (FUN_00157010). */
      if (*(void **)0x476ab0 != 0) {
        D3DDevice_Release();
        *(void **)0x476ab0 = 0;
      }
      if (*(void **)0x476a50 != 0) {
        *(void **)0x476a50 = 0;
      }
      return success;
    }
  }
  error(2, "### ERROR rasterizer_preinitialize failed");
  return success;
}

/*
 * FUN_00155560 @ 0x155560 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::Clear: stencil arrives in EAX, color in EDX (ECX is
 * clobbered before use), device (s1) ignored, count/rects/flags/z (s2-s5)
 * on the stack. s5 is typed float to match the callee's z parameter so the
 * passthrough compiles to a plain dword PUSH (not FLD/FSTP). Returns S_OK.
 * No direct call sites; RET 0x14.
 */
/* 0x155560 */
int FUN_00155560(int r1, int r3, int s1, int s2, int s3, int s4, float s5)
{
  (void)s1;
  D3DDevice_Clear(s2, (void *)s3, s4, r3, s5, r1);
  return 0;
}

/*
 * rasterizer_get_default_hardware_format (0x155580)
 *
 * Returns the default D3D texture pointer for a bitmap based on its type.
 * Bitmap types 0 (2D) and 1 (volume) map to the 2D default at 0x3256a4.
 * Bitmap type 2 (cubemap) maps to the cubemap default at 0x3256ac.
 * These globals are populated by rasterizer_filthy_bitmap_default_initialize
 * (FUN_00156e00).
 *
 * Globals:
 *   0x3256a4  void *  – default 2D hardware texture format
 *   0x3256a8  void *  – default volume texture format
 *   0x3256ac  void *  – default cubemap texture format
 */
/* 0x155580 */
void *rasterizer_get_default_hardware_format(void *bitmap_data)
{
  void *result;

  if (!bitmap_data) {
    display_assert("bitmap",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xd1, true);
    system_exit(-1);
  }

  /* MOVSWL + SUB $0 / DEC / DEC dispatch at 0x1555ab is a jump-free switch
   * lowering, not an if/else chain: the short type field is promoted to int
   * by the switch and compared against 0, 1, 2 in sequence. */
  switch (*(short *)((char *)bitmap_data + 0xa)) {
  case 0:
    result = *(void **)0x3256a4;
    break;
  case 1:
    result = *(void **)0x3256a4;
    break;
  case 2:
    result = *(void **)0x3256ac;
    break;
  default:
    display_assert("### ERROR unsupported bitmap type",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xdf, true);
    system_exit(-1);
    result = bitmap_data;
    break;
  }

  if (!result) {
    display_assert("hardware_format",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xe2, true);
    system_exit(-1);
  }

  return result;
}

/*
 * FUN_00155620 @ 0x155620 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::SetViewport: the viewport pointer arrives in EAX, the
 * device argument (s1) is ignored. Returns S_OK. No direct call sites;
 * frameless in the original; RET 0x4.
 *
 * VC71 72.7% is the @reg-DEFINED prologue ceiling: cl.exe cannot take a
 * parameter in EAX, so the candidate reads a phantom [ebp+8] slot which in
 * turn forces an EBP frame the frameless original never has. Body verified
 * insn-for-insn (PUSH EAX; CALL; XOR EAX,EAX; RET 4) after phantom-load
 * strip; accepted at ceiling.
 */
/* 0x155620 */
int FUN_00155620(int r1, int s1)
{
  (void)s1;
  D3DDevice_SetViewport((void *)r1);
  return 0;
}

/*
 * FUN_00155630 @ 0x155630 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::SetRenderState: state arrives in ESI, value in EDI,
 * the device argument (s1) is ignored. States < 0x52 map through the
 * D3D "simple" render-state register table at 0x282b90 and mirror the
 * value into the host-side shadow at 0x1fb698; states < 0x74 defer;
 * 0x74-0x8f dispatch to the per-state XDK setters in the exact original
 * compare order (0x7f is tested before 0x7e). Unknown states no-op.
 * Returns S_OK. No direct call sites; frameless in the original; RET 0x4.
 *
 * VC71 79.6% is the @reg-DEFINED ceiling: cl.exe cannot take state/value in
 * ESI/EDI, so it builds a frame, reloads value from [ebp+0xc] in every
 * dispatch arm, and restores ESI/EBP at each of the ~30 per-arm returns —
 * none of which exist in the frameless original. Compare chain, callee
 * order, table lookup and shadow write verified insn-for-insn; accepted at
 * ceiling.
 */
/* 0x155630 */
int FUN_00155630(int state, int value, int s1)
{
  (void)s1;
  if (state < 0x52) {
    D3DDevice_SetRenderState_Simple(*(uint32_t *)(0x282b90 + state * 4), value);
    *(uint32_t *)(0x1fb698 + state * 4) = value;
    return 0;
  }
  if (state < 0x74) {
    D3DDevice_SetRenderState_Deferred(state, value);
    return 0;
  }
  if (state == 0x74) {
    D3DDevice_SetRenderState_PSTextureModes(value);
    return 0;
  }
  if (state == 0x75) {
    D3DDevice_SetRenderState_VertexBlend(value);
    return 0;
  }
  if (state == 0x76) {
    D3DDevice_SetRenderState_FogColor(value);
    return 0;
  }
  if (state == 0x77) {
    D3DDevice_SetRenderState_FillMode(value);
    return 0;
  }
  if (state == 0x78) {
    D3DDevice_SetRenderState_BackFillMode(value);
    return 0;
  }
  if (state == 0x79) {
    D3DDevice_SetRenderState_TwoSidedLighting(value);
    return 0;
  }
  if (state == 0x7a) {
    D3DDevice_SetRenderState_NormalizeNormals(value);
    return 0;
  }
  if (state == 0x7b) {
    D3DDevice_SetRenderState_ZEnable(value);
    return 0;
  }
  if (state == 0x7c) {
    D3DDevice_SetRenderState_StencilEnable(value);
    return 0;
  }
  if (state == 0x7d) {
    D3DDevice_SetRenderState_StencilFail(value);
    return 0;
  }
  if (state == 0x7f) {
    D3DDevice_SetRenderState_CullMode(value);
    return 0;
  }
  if (state == 0x7e) {
    D3DDevice_SetRenderState_FrontFace(value);
    return 0;
  }
  if (state == 0x80) {
    D3DDevice_SetRenderState_TextureFactor(value);
    return 0;
  }
  if (state == 0x81) {
    D3DDevice_SetRenderState_ZBias(value);
    return 0;
  }
  if (state == 0x82) {
    D3DDevice_SetRenderState_LogicOp(value);
    return 0;
  }
  if (state == 0x83) {
    D3DDevice_SetRenderState_EdgeAntiAlias(value);
    return 0;
  }
  if (state == 0x84) {
    D3DDevice_SetRenderState_MultiSampleAntiAlias(value);
    return 0;
  }
  if (state == 0x85) {
    D3DDevice_SetRenderState_MultiSampleMask(value);
    return 0;
  }
  if (state == 0x86) {
    D3DDevice_SetRenderState_MultiSampleType(value);
    return 0;
  }
  if (state == 0x87) {
    D3DDevice_SetRenderState_ShadowFunc(value);
    return 0;
  }
  if (state == 0x88) {
    D3DDevice_SetRenderState_LineWidth(value);
    return 0;
  }
  if (state == 0x89) {
    D3DDevice_SetRenderState_Dxt1NoiseEnable(value);
    return 0;
  }
  if (state == 0x8a) {
    D3DDevice_SetRenderState_YuvEnable(value);
    return 0;
  }
  if (state == 0x8b) {
    D3DDevice_SetRenderState_OcclusionCullEnable(value);
    return 0;
  }
  if (state == 0x8c) {
    D3DDevice_SetRenderState_StencilCullEnable(value);
    return 0;
  }
  if (state == 0x8d) {
    D3DDevice_SetRenderState_RopZCmpAlwaysRead(value);
    return 0;
  }
  if (state == 0x8e) {
    D3DDevice_SetRenderState_RopZRead(value);
    return 0;
  }
  if (state == 0x8f) {
    D3DDevice_SetRenderState_DoNotCullUncompressed(value);
  }
  return 0;
}

/*
 * FUN_00155850 @ 0x155850 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::GetRenderState: state arrives in EAX, the out-value
 * pointer in EDX, the device argument (s1) is ignored. Reads the host-side
 * render-state shadow table at 0x1fb698 (written by FUN_00155630). Returns
 * S_OK. No direct call sites; frameless in the original; RET 0x4.
 * VC71 72.7% = @reg-DEFINED prologue ceiling (same class as FUN_00155620);
 * 4-insn body verified; accepted at ceiling.
 */
/* 0x155850 */
int FUN_00155850(int state, int out_value, int s1)
{
  (void)s1;
  *(uint32_t *)out_value = *(uint32_t *)(0x1fb698 + state * 4);
  return 0;
}

/*
 * FUN_00155860 @ 0x155860 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::SetTexture: texture arrives in EAX, stage in ECX, the
 * device argument (s1) is ignored. Returns S_OK. No direct call sites;
 * frameless in the original; RET 0x4.
 */
/* 0x155860 */
int FUN_00155860(int texture, int stage, int s1)
{
  (void)s1;
  D3DDevice_SetTexture(stage, (void *)texture);
  return 0;
}

/*
 * FUN_00155870 @ 0x155870 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::SetPalette: palette arrives in EAX, stage in ECX, the
 * device argument (s1) is ignored. Returns S_OK. No direct call sites;
 * frameless in the original; RET 0x4.
 */
/* 0x155870 */
int FUN_00155870(int palette, int stage, int s1)
{
  (void)s1;
  D3DDevice_SetPalette(stage, (void *)palette);
  return 0;
}

/*
 * FUN_00155880 @ 0x155880 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::GetTextureStageState: stage arrives in EAX, state in
 * ECX, device (s1) ignored, out-value pointer (s2) on the stack. Reads the
 * host-side deferred texture-state shadow table at 0x1fb498
 * (D3D__DeferredTextureState, 0x20 dwords per stage). Returns S_OK. No
 * direct call sites; RET 0x8.
 *
 * VC71 85.7% is the @reg-DEFINED ceiling: the two phantom stack slots for
 * the EAX/ECX params shift the real s2 displacement (+8), which the scorer
 * counts as operand mismatches. Body verified against the 10-insn original;
 * accepted at ceiling.
 */
/* 0x155880 */
int FUN_00155880(int r1, int r2, int s1, int s2)
{
  (void)s1;
  *(uint32_t *)s2 = *(uint32_t *)(0x1fb498 + (r1 * 0x20 + r2) * 4);
  return 0;
}

/*
 * _rasterizer_reset_state @ 0x1559a0 — empty in the binary (single RET).
 * Reached via the tail-call thunk at 0x17c7d0.
 */
/* 0x1559a0 */
void _rasterizer_reset_state(void)
{
}

/*
 * rasterizer_spin_begin @ 0x1559b0 — empty in the binary (single RET);
 * debug-spin instrumentation compiled out of this build. Called from
 * 0x17adfa with the spin reason on the stack.
 */
/* 0x1559b0 */
void rasterizer_spin_begin(int spin_reason)
{
  (void)spin_reason;
}

/*
 * rasterizer_spin_end @ 0x1559c0 — empty in the binary (single RET).
 * Called from 0x17ae1a.
 */
/* 0x1559c0 */
void rasterizer_spin_end(void)
{
}

/*
 * _rasterizer_windows_begin @ 0x1559d0 — asserts that the global D3D
 * device exists (rasterizer_xbox.c line 0x533), otherwise no-op.
 * Reached via the tail-call thunk at 0x17c8c0. Frameless in the original.
 */
/* 0x1559d0 */
void _rasterizer_windows_begin(void)
{
  if (*(void **)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x533, true);
    system_exit(-1);
  }
}

/*
 * _rasterizer_window_get_fog @ 0x155a00 — copies the current window fog
 * parameter block (0x14 dwords / 0x50 bytes) from the global at 0x5a5da8
 * into the caller's buffer. Asserts fog != NULL (line 0x5a0). Called from
 * 0x17c8e4.
 */
/* 0x155a00 */
void _rasterizer_window_get_fog(void *fog)
{
  if (fog == 0) {
    display_assert("fog",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x5a0, true);
    system_exit(-1);
  }
  /* Copy 0x14 dwords; MSVC lowers the constant-size memcpy to `rep movsl`
   * (ECX=0x14, ESI=0x5a5da8) matching the original. */
  memcpy(fog, (void *)0x5a5da8, 0x50);
}

/*
 * _rasterizer_windows_end @ 0x155a40 — asserts that the global D3D device
 * exists (line 0x65d), otherwise no-op. Reached via the tail-call thunk at
 * 0x17c910. Frameless in the original.
 */
/* 0x155a40 */
void _rasterizer_windows_end(void)
{
  if (*(void **)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x65d, true);
    system_exit(-1);
  }
}

/*
 * _rasterizer_frame_end @ 0x155a70 — tears down the per-frame D3D binding
 * state: asserts the global device (line 0x670), flushes the two rasterizer
 * subsystems at 0x16fdd0 / 0x17ff50, then unbinds all 4 texture stages, all
 * 16 vertex streams and the index buffer.
 *
 * Each unbind is wrapped in the file's HRESULT-check idiom
 *   success = success && SUCCEEDED(hr = IDirect3DDevice8_Xxx(...));
 *   if (!success) rasterizer_report_hresult(hr, "<expression text>");
 * The D3D8 wrappers are inline and always yield S_OK, so `hr` folds to the
 * literal 0 the original pushes and the report path is unreachable at
 * runtime.  The `&&` still forces MSVC to re-materialise the flag in BL
 * (MOV BL,1 / XOR BL,BL) on both arms, which is why `success` is a plain
 * `char` here rather than a value the compiler can prove is 0/1.
 *
 * Reached via the tail-call thunk used by main.c. Plain cdecl, no args.
 */
/* 0x155a70 */
void _rasterizer_frame_end(void)
{
  char success;
  int hr;
  int index;
  int remaining;

  if (*(void **)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x670, true);
    system_exit(-1);
  }

  success = 1;
  hr = 0; /* inline D3D8 wrappers below always return S_OK */

  FUN_0016FDD0();
  FUN_0017ff50();

  /* Both loops are transcribed in the original's two-induction-variable
   * down-counter form (INC ESI / DEC EDI / JNZ). VC71 will not strength-reduce
   * a plain `for (index = 0; index < N; index++)` into that shape. */
  index = 0;
  remaining = 4;
  do {
    D3DDevice_SetTexture((uint32_t)index, (void *)0);
    success = success && hr >= 0;
    if (!success) {
      FUN_00167ff0(
        hr, "IDirect3DDevice8_SetTexture(global_d3d_device, index, NULL)");
    }
    index++;
    remaining--;
  } while (remaining != 0);

  index = 0;
  remaining = 16;
  do {
    D3DDevice_SetStreamSource((uint32_t)index, (void *)0, 0);
    success = success && hr >= 0;
    if (!success) {
      FUN_00167ff0(hr, "IDirect3DDevice8_SetStreamSource(global_d3d_device, "
                       "index, NULL, 0)");
    }
    index++;
    remaining--;
  } while (remaining != 0);

  D3DDevice_SetIndices((void *)0, 0);
  success = success && hr >= 0;
  if (!success) {
    FUN_00167ff0(hr, "IDirect3DDevice8_SetIndices(global_d3d_device, NULL, 0)");
    error(2, "### ERROR rasterizer_frame_end failed");
  }
}

/*
 * FUN_00155b60 @ 0x155b60 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DTexture8::LockRect: flags/pRect/pLockedRect arrive in
 * EAX/ECX/EDX, texture (s1) and level (s2) on the stack (the texture IS
 * forwarded — no ignored device argument here). Returns S_OK. No direct
 * call sites; RET 0x8.
 */
/* 0x155b60 */
int FUN_00155b60(int r1, int r2, int r3, int s1, int s2)
{
  D3DTexture_LockRect((void *)s1, s2, (void *)r3, (void *)r2, r1);
  return 0;
}

/*
 * _rasterizer_dispose @ 0x155b90 — rasterizer shutdown. Calls the twelve
 * per-subsystem dispose entry points in their original order, then releases
 * the D3D device and clears both global pointers.
 *
 * The IDirect3D8 teardown at the tail is `if (d3d) d3d = NULL;` in the
 * binary: on Xbox the IDirect3D8 Release() wrapper is inline and compiles
 * away entirely, leaving only the pointer clear. Transcribed as-is.
 *
 * Globals: 0x476ab0 = IDirect3DDevice8*, 0x476a50 = IDirect3D8*.
 * Plain cdecl, no args, no frame in the original.
 */
/* 0x155b90 */
void _rasterizer_dispose(void)
{
  rasterizer_memory_pool_delete();
  FUN_0015e9e0();
  FUN_00184690();
  rasterizer_vertex_shaders_dispose();
  FUN_0017e040();
  FUN_0017ff60();
  rasterizer_text_cache_dispose();
  FUN_0015c680();
  FUN_0016fec0();
  FUN_00165a10();
  FUN_0017d990();
  texture_cache_delete();

  if (*(void **)0x476ab0 != 0) {
    D3DDevice_Release();
    *(void **)0x476ab0 = 0;
  }

  if (*(void **)0x476a50 != 0) {
    *(void **)0x476a50 = 0;
  }
}

/*
 * FUN_00155c10 @ 0x155c10 — forwards a callback pointer to
 * D3DDevice_SetVerticalBlankCallback. Plain cdecl, one stack arg; EAX
 * passes through from the callee. Called from 0x17c954.
 */
/* 0x155c10 */
void FUN_00155c10(void *callback)
{
  D3DDevice_SetVerticalBlankCallback(callback);
}

/*
 * rasterizer_set_texture_bitmap_data @ 0x155c20 — binds a bitmap_data's
 * hardware texture to a texture stage.
 *
 * `stage` is a 16-bit parameter: the prologue loads it with
 * `MOV SI, word ptr [EBP+8]`, the range check uses `TEST SI,SI` / `CMP SI,4`
 * and the printf argument is `MOVSX EAX,SI` — all three prove int16.
 * The stage bound is RASTERIZER_MAXIMUM_TEXTURE_STAGES == 4 (assert line
 * 0x78f); the NULL-bitmap path asserts at line 0x797 without halting and
 * then logs, matching the original's fall-through to a common
 * `MOV AL,1 / RET` epilogue.
 *
 * Return value is confirmed: both exits execute `MOV AL,0x1`, so the
 * function returns `true` unconditionally (kb decl was `void`, corrected).
 *
 * Texture upload is bracketed by the texture profiler
 * (profile_texture_start / profile_texture_end) and goes through
 * xbox_texture_cache_get_hardware_format(bitmap, block=true, load=true) —
 * push order verified: PUSH 1; PUSH 1; PUSH EDI (cdecl, first PUSH is the
 * last argument).
 */
/* 0x155c20 */
char rasterizer_set_texture_bitmap_data(short stage, void *bitmap_data)
{
  void *hardware_format;

  if (stage < 0 || stage >= 4) {
    display_assert("stage>=0 && stage<RASTERIZER_MAXIMUM_TEXTURE_STAGES",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x78f, true);
    system_exit(-1);
  }

  /* Tested as `!= 0` (not `== 0`) so the bind is the fall-through and the
   * "not found" report is the out-of-line tail block, matching the original's
   * `TEST EDI,EDI / JZ LAB_00155c84` layout at 0x155c59. */
  if (bitmap_data != 0) {
    profile_texture_start();
    hardware_format =
      xbox_texture_cache_get_hardware_format(bitmap_data, true, true);
    profile_texture_end();

    D3DDevice_SetTexture((uint32_t)stage, hardware_format);
    return 1;
  }

  display_assert("### YOU GOT FUCKED in rasterizer_set_texture_bitmap_data",
                 "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0x797,
                 true);
  error(2, "### ERROR direct texture not found (stage=%d)", (int)stage);
  return 1;
}

/*
 * FUN_00155cc0 @ 0x155cc0 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DVolumeTexture8::LockBox: flags/pBox/pLockedBox arrive in
 * EAX/ECX/EDX, volume texture (s1) and level (s2) on the stack. Returns
 * S_OK. No direct call sites; RET 0x8.
 */
/* 0x155cc0 */
int FUN_00155cc0(int r1, int r2, int r3, int s1, int s2)
{
  D3DVolumeTexture_LockBox((void *)s1, s2, (void *)r3, (void *)r2, r1);
  return 0;
}

/*
 * rasterizer_set_texture_direct @ 0x155cf0 — binds a texture stage straight
 * from a bitmap tag index + frame index, blocking on the texture cache.
 *
 * Looks the tag up with tag_get('bitm', bitmap_tag_index), reads the bitmap
 * count at +0x60, wraps `frame_index` into that count with a signed IDIV
 * (`MOVSX EAX,word [EBP+0x10]; CDQ; IDIV ECX` — proving frame_index is
 * int16), then resolves the bitmap_data through FUN_00076ff0 and forwards to
 * rasterizer_set_texture_bitmap_data.
 *
 * Parameter widths: `stage` is compared with `TEST DI,DI` / `CMP DI,4` and
 * promoted with `MOVSX EAX,DI` for the printf — int16. `bitmap_tag_index` is
 * a full dword compared against -1 (NONE). Asserts: stage bound at line
 * 0x7a6, "not found" at line 0x7be (non-halting, followed by error()).
 *
 * Returns `true` on the bind path (`MOV AL,1`) and the zero-initialised
 * `result` flag on every failure path (`XOR BL,BL` in the prologue, `MOV
 * AL,BL` at the tail) — a sentinel result-local, not an early return.
 * The forwarded call's return value is discarded: the original re-materialises
 * the literal 1 rather than passing EAX through.
 */
/* 0x155cf0 */
char rasterizer_set_texture_direct(short stage, int bitmap_tag_index,
                                   short frame_index)
{
  char result;
  void *bitmap;
  void *bitmap_data;

  result = 0;

  if (stage < 0 || stage >= 4) {
    display_assert("stage>=0 && stage<RASTERIZER_MAXIMUM_TEXTURE_STAGES",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x7a6, true);
    system_exit(-1);
  }

  if (bitmap_tag_index != -1) {
    bitmap = tag_get(0x6269746d /* 'bitm' */, bitmap_tag_index);
    if (*(int *)((char *)bitmap + 0x60) > 0) {
      bitmap_data = FUN_00076ff0(bitmap_tag_index,
                                 frame_index % *(int *)((char *)bitmap + 0x60));
      if (bitmap_data != 0) {
        rasterizer_set_texture_bitmap_data(stage, bitmap_data);
        return 1;
      }
    }
  }

  display_assert("### YOU GOT FUCKED in rasterizer_set_texture_direct",
                 "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0x7be,
                 true);
  error(2, "### ERROR direct texture not found (stage=%d)", (int)stage);
  return result;
}

/*
 * rasterizer_set_texture_direct_non_blocking @ 0x155da0 — same lookup chain
 * as rasterizer_set_texture_direct, but asks the texture cache with
 * block=false so a not-yet-resident texture is skipped instead of stalling.
 *
 * Push order verified for the cache probe: PUSH 1; PUSH 0; PUSH ESI (cdecl)
 * = xbox_texture_cache_get_hardware_format(bitmap_data, block=false,
 * load=true). Note ESI is reloaded with the bitmap_data pointer at 0x155e01,
 * overwriting the tag index it held earlier — the pushed ESI is bitmap_data,
 * not bitmap_tag_index.
 *
 * Return polarity is inverted relative to the blocking variant and is
 * transcribed exactly as the binary has it: when the texture IS resident the
 * bind happens and the zero `result` flag is returned (`MOV AL,BL`), and when
 * it is NOT resident the function returns 1 (`MOV AL,0x1` at 0x155e2e). Both
 * hard-failure paths also return `result`. Asserts: stage bound at line
 * 0x7d3, "not found" at line 0x7f3.
 */
/* 0x155da0 */
char rasterizer_set_texture_direct_non_blocking(short stage,
                                                int bitmap_tag_index,
                                                short frame_index)
{
  char result;
  void *bitmap;
  void *bitmap_data;

  result = 0;

  if (stage < 0 || stage >= 4) {
    display_assert("stage>=0 && stage<RASTERIZER_MAXIMUM_TEXTURE_STAGES",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x7d3, true);
    system_exit(-1);
  }

  if (bitmap_tag_index != -1) {
    bitmap = tag_get(0x6269746d /* 'bitm' */, bitmap_tag_index);
    if (*(int *)((char *)bitmap + 0x60) > 0) {
      bitmap_data = FUN_00076ff0(bitmap_tag_index,
                                 frame_index % *(int *)((char *)bitmap + 0x60));
      if (bitmap_data != 0) {
        if (xbox_texture_cache_get_hardware_format(bitmap_data, false, true) !=
            0) {
          rasterizer_set_texture_bitmap_data(stage, bitmap_data);
          return result;
        }
        return 1;
      }
    }
  }

  display_assert(
    "### YOU GOT FUCKED in rasterizer_set_texture_direct_non_blocking",
    "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0x7f3, true);
  error(2, "### ERROR direct texture not found (stage=%d)", (int)stage);
  return result;
}

/*
 * rasterizer_set_texture @ 0x155e80 — bind a bitmap to a texture stage,
 * falling back to the rasterizer globals' default bitmap for the requested
 * type/usage pair when the caller's tag index is unusable or the bitmap has
 * the wrong type.
 *
 * The kb declaration was `void (int,int,int,int,int)`. The prologue proves
 * three int16 leading parameters and an int16 tail parameter:
 *   [EBP+0x08] MOV EAX,dword / TEST AX,AX / CMP AX,4 – stage  (int16)
 *   [EBP+0x0c] MOV EDI,dword / TEST DI,DI / CMP DI,3 – type   (int16)
 *   [EBP+0x10] MOV EBX,dword / TEST BX,BX / CMP BX,4 – usage  (int16)
 *   [EBP+0x14] MOV ESI,dword / CMP ESI,-1            – bitmap tag index
 *   [EBP+0x18] MOVSX EAX,word                        – frame index (int16)
 * The plain dword loads feeding 16-bit compares are the same shape the
 * already-lifted rasterizer_set_texture_direct @0x155cf0 emits for its
 * declared `short stage` (MOV EDI,dword[EBP+8] then PUSH EDI straight into
 * the `short` parameter of rasterizer_set_texture_bitmap_data).
 *
 * The function is NOT void. Every exit runs
 *   MOV AL,<flag> / NEG AL / SBB EAX,EAX / AND EAX,0x476a4c
 * which is MSVC's lowering of `return valid ? <constant> : NULL`, and the
 * caller at 0x00162a17 immediately does `MOV EDX,dword ptr [EAX]` — the
 * result is a live pointer to the width/height pair the function just wrote,
 * not a discarded bool. The flag itself lives at [EBP-1] (zeroed at 0x155e8a)
 * and is shared by both bind sites, so it is written as one `valid` local
 * with a common `done:` return; MSVC tail-duplicated that epilogue into each
 * exit (constant-folding the reload to `MOV AL,1` at 0x155f73).
 *
 * Globals:
 *   0x3256e1  char  – flag that lets usage 3 use the caller's own bitmap
 *   0x476204  void* – rasterizer globals block; default bitmap tag index for
 *                     `type` at +0xb8 + type*0x10
 *   0x476a4c  short – bound texture width  (start of the returned pointer)
 *   0x476a4e  short – bound texture height
 *
 * error() argument order verified from the pushes: the incompatible-type
 * report is error(2, fmt, bitmap_type_get_string(bitmap_data->type),
 * bitmap_type_get_string(type)) — the `type` string is computed first
 * (0x155f98) because cdecl arguments are evaluated right to left.
 *
 * Asserts: stage 0x80a, type 0x80b, usage 0x80c, "YOU GOT FUCKED" 0x845
 * (which does NOT call system_exit — the assert is followed directly by the
 * error() pushes, ADD ESP,0x24 covering both calls).
 */
/* 0x155e80 */
void *rasterizer_set_texture(short stage, short type, short usage,
                             int bitmap_tag_index, short frame_index)
{
  char valid;
  void *bitmap;
  void *bitmap_data;
  int default_tag_index;

  valid = 0;

  if (stage < 0 || stage >= 4) {
    display_assert("stage>=0 && stage<RASTERIZER_MAXIMUM_TEXTURE_STAGES",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x80a, true);
    system_exit(-1);
  }

  if (type < 0 || type >= 3) {
    display_assert("type>=0 && type<NUMBER_OF_BITMAP_TYPES",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x80b, true);
    system_exit(-1);
  }

  if (usage < 0 || usage >= 4) {
    display_assert("usage>=0 && usage<NUMBER_OF_BITMAP_USAGES",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x80c, true);
    system_exit(-1);
  }

  if ((*(char *)0x3256e1 != 0 || usage != 3) && bitmap_tag_index != -1) {
    bitmap = tag_get(0x6269746d /* 'bitm' */, bitmap_tag_index);
    if (*(int *)((char *)bitmap + 0x60) > 0) {
      bitmap_data = FUN_00076ff0(bitmap_tag_index,
                                 frame_index % *(int *)((char *)bitmap + 0x60));
      /* Type match is the fall-through here (JNZ to the error block at
       * 0x155f97), so the bind is written inline rather than as the goto the
       * non-blocking twin at 0x1560a0 uses. */
      if (*(short *)((char *)bitmap_data + 0xa) == type) {
        rasterizer_set_texture_bitmap_data(stage, bitmap_data);
        *(short *)0x476a4c = *(short *)((char *)bitmap_data + 4);
        *(short *)0x476a4e = *(short *)((char *)bitmap_data + 6);
        valid = 1;
        goto done;
      }
      error(2,
            "### ERROR incompatible bitmap type in shader got %s expected %s",
            bitmap_type_get_string(*(short *)((char *)bitmap_data + 0xa)),
            bitmap_type_get_string(type));
    }
  }

  default_tag_index = *(int *)(*(char **)0x476204 + type * 0x10 + 0xb8);
  if (default_tag_index != -1) {
    bitmap_data = FUN_00076ff0(default_tag_index, usage);
    if (bitmap_data != 0) {
      rasterizer_set_texture_bitmap_data(stage, bitmap_data);
      *(short *)0x476a4c = *(short *)((char *)bitmap_data + 4);
      *(short *)0x476a4e = *(short *)((char *)bitmap_data + 6);
      valid = 1;
      goto done;
    }
  }

  display_assert("### YOU GOT FUCKED in rasterizer_set_texture",
                 "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0x845,
                 true);
  error(2, "### ERROR default texture not found (stage=%d, type=%d, usage=%d)",
        (int)stage, (int)type, (int)usage);

done:
  return valid ? (void *)0x476a4c : (void *)0;
}

/*
 * FUN_00156070 @ 0x156070 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DCubeTexture8::LockRect: flags/pRect/pLockedRect arrive in
 * EAX/ECX/EDX, cube texture (s1), face (s2) and level (s3) on the stack.
 * Returns S_OK. No direct call sites; RET 0xc.
 */
/* 0x156070 */
int FUN_00156070(int r1, int r2, int r3, int s1, int s2, int s3)
{
  D3DCubeTexture_LockRect((void *)s1, s2, s3, (void *)r3, (void *)r2, r1);
  return 0;
}

/*
 * rasterizer_set_texture_non_blocking @ 0x1560a0 — same selection chain as
 * rasterizer_set_texture @0x155e80, but probes the texture cache with
 * block=false first and bails out instead of stalling when the bitmap is not
 * yet resident.
 *
 * Parameter widths from the prologue (kb had five `int`):
 *   [EBP+0x08] MOV EAX,dword / TEST AX,AX / CMP AX,4 – stage  (int16)
 *   [EBP+0x0c] MOV EDI,dword / TEST DI,DI / CMP DI,3 – type   (int16)
 *   [EBP+0x10] MOV EBX,dword / TEST BX,BX / CMP BX,4 – usage  (int16)
 *   [EBP+0x14] MOV ESI,dword / CMP ESI,-1            – bitmap tag index
 *   [EBP+0x18] MOVSX EAX,word                        – frame index (int16)
 *
 * Return polarity is the same inverted convention as
 * rasterizer_set_texture_direct_non_blocking: 1 (`MOV AL,0x1` at 0x156206)
 * only when the cache probe reports the texture is not resident, 0
 * (`XOR AL,AL`) both when the bind succeeds and on the hard-failure path.
 * There is no stack flag local here — the original has no `SUB ESP`.
 *
 * Cache probe push order: PUSH 1; PUSH 0; PUSH ESI (cdecl) =
 * xbox_texture_cache_get_hardware_format(bitmap_data, block=false,
 * load=true); the ADD ESP,0x14 at 0x156176 is the merged cleanup for that
 * call plus the preceding FUN_00076ff0.
 *
 * The type-match branch is `JZ LAB_001561da` into the shared bind block, so
 * the goto is transcribed rather than duplicating the bind (the blocking
 * twin at 0x155e80 falls through into its own copy instead and is written
 * that way there).
 *
 * Globals: 0x3256e1 char flag (allows usage 3 to use the caller's bitmap),
 * 0x476204 rasterizer globals block (default bitmap tag index for `type` at
 * +0xb8 + type*0x10), 0x476a48/0x476a4a shorts receiving the bound texture
 * width/height.
 *
 * Asserts: stage 0x85e, type 0x85f, usage 0x860, "YOU GOT FUCKED" 0x8a2.
 */
/* 0x1560a0 */
char rasterizer_set_texture_non_blocking(short stage, short type, short usage,
                                         int bitmap_tag_index,
                                         short frame_index)
{
  void *bitmap;
  void *bitmap_data;
  int default_tag_index;

  if (stage < 0 || stage >= 4) {
    display_assert("stage>=0 && stage<RASTERIZER_MAXIMUM_TEXTURE_STAGES",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x85e, true);
    system_exit(-1);
  }

  if (type < 0 || type >= 3) {
    display_assert("type>=0 && type<NUMBER_OF_BITMAP_TYPES",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x85f, true);
    system_exit(-1);
  }

  if (usage < 0 || usage >= 4) {
    display_assert("usage>=0 && usage<NUMBER_OF_BITMAP_USAGES",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x860, true);
    system_exit(-1);
  }

  if ((*(char *)0x3256e1 != 0 || usage != 3) && bitmap_tag_index != -1) {
    bitmap = tag_get(0x6269746d /* 'bitm' */, bitmap_tag_index);
    if (*(int *)((char *)bitmap + 0x60) > 0) {
      bitmap_data = FUN_00076ff0(bitmap_tag_index,
                                 frame_index % *(int *)((char *)bitmap + 0x60));
      /* Positive test with the bail-out in the `else` so VC71 exiles the
       * `return 1` block to the tail, matching the original's
       * `TEST EAX,EAX / JZ LAB_00156204` at 0x15617b. */
      if (xbox_texture_cache_get_hardware_format(bitmap_data, false, true) !=
          0) {
        if (*(short *)((char *)bitmap_data + 0xa) == type) {
          goto bind;
        }
        error(2,
              "### ERROR incompatible bitmap type in shader got %s expected %s",
              bitmap_type_get_string(*(short *)((char *)bitmap_data + 0xa)),
              bitmap_type_get_string(type));
      } else {
        return 1;
      }
    }
  }

  default_tag_index = *(int *)(*(char **)0x476204 + type * 0x10 + 0xb8);
  if (default_tag_index != -1) {
    bitmap_data = FUN_00076ff0(default_tag_index, usage);
    if (bitmap_data != 0) {
    bind:
      rasterizer_set_texture_bitmap_data(stage, bitmap_data);
      *(short *)0x476a48 = *(short *)((char *)bitmap_data + 4);
      *(short *)0x476a4a = *(short *)((char *)bitmap_data + 6);
      return 0;
    }
  }

  display_assert("### YOU GOT FUCKED in rasterizer_set_texture_non_blocking",
                 "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0x8a2,
                 true);
  error(2, "### ERROR default texture not found (stage=%d, type=%d, usage=%d)",
        (int)stage, (int)type, (int)usage);
  return 0;
}

/*
 * rasterizer_get_target @ 0x156250 — returns the D3D surface bound to a
 * render-target slot.
 *
 * The kb declaration was `void (void)`; the function takes two int16
 * arguments and returns a pointer in EAX:
 *   [EBP+0x08] MOVSX EAX,word / CMP EAX,6 / JA default – target
 *   [EBP+0x0c] CMP word,0 (cases 0..5), MOV SI,word (case 6) – mipmap_index
 * Every case ends with `MOV EAX,<surface global>` before the epilogue and the
 * default path ends with `MOV EAX,ESI` where ESI was zeroed by the prologue's
 * `XOR ESI,ESI` — i.e. a NULL-initialised result variable that each case
 * overwrites, tail-duplicated by MSVC into per-case epilogues.
 *
 * The surface globals match the table already documented for FUN_00158140 in
 * rasterizer_xbox_decals.c (which inlines the same selection):
 *   0x476a5c  target 0    0x476a80  target 3    0x476a98[4] target 6 (water)
 *   0x476a6c  target 1    0x476a88  target 4
 *   0x476a78  target 2    0x476a90  target 5
 *
 * `JA 0x1563c0` on the switch value is the unsigned jump-table range check
 * emitted for the 7-entry table at 0x1563e8; the default arm is the
 * "unsupported rasterizer target" assert.
 *
 * Asserts: "mipmap_index==0" at 0x8b8/0x8bc/0x8c0/0x8c4/0x8c8/0x8cc for
 * targets 0..5, the water mip bound at 0x8d0, and the unsupported-target
 * assert at 0x8d4.
 */
/* 0x156250 */
void *rasterizer_get_target(short target, short mipmap_index)
{
  void *d3d_surface;

  d3d_surface = 0;

  switch (target) {
  case 0:
    if (mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8b8, true);
      system_exit(-1);
    }
    d3d_surface = *(void **)0x476a5c;
    break;
  case 1:
    if (mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8bc, true);
      system_exit(-1);
    }
    d3d_surface = *(void **)0x476a6c;
    break;
  case 2:
    if (mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8c0, true);
      system_exit(-1);
    }
    d3d_surface = *(void **)0x476a78;
    break;
  case 3:
    if (mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8c4, true);
      system_exit(-1);
    }
    d3d_surface = *(void **)0x476a80;
    break;
  case 4:
    if (mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8c8, true);
      system_exit(-1);
    }
    d3d_surface = *(void **)0x476a88;
    break;
  case 5:
    if (mipmap_index != 0) {
      display_assert("mipmap_index==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0x8cc, true);
      system_exit(-1);
    }
    d3d_surface = *(void **)0x476a90;
    break;
  case 6:
    if (mipmap_index < 0 || mipmap_index >= 4) {
      display_assert(
        "mipmap_index>=0 && mipmap_index<RASTERIZER_TARGET_WATER_MAX_MIPMAP_"
        "LEVELS",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0x8d0, true);
      system_exit(-1);
    }
    d3d_surface = ((void **)0x476a98)[mipmap_index];
    break;
  default:
    display_assert("### ERROR unsupported rasterizer target",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0x8d4, true);
    system_exit(-1);
    break;
  }

  return d3d_surface;
}

/*
 * rasterizer_set_vertex_shader @ 0x156440 — selects one of the
 * NUMBER_OF_VERTEX_SHADERS (0x43) compiled vertex shaders, skipping the work
 * when it is already current.
 *
 * The kb declaration was `void (void)`; Ghidra surfaced the argument as
 * `in_stack_00000004` because of that. The prologue's
 * `MOV DI, word ptr [EBP+8]` plus the 16-bit compares and the
 * `MOV word ptr [0x325164],DI` write-back prove a single int16 parameter.
 *
 * Globals:
 *   0x325164  short  – currently bound vertex shader index
 *   0x325208  []     – vertex shader table, stride 0x10:
 *                        +0x00 D3D shader handle (0xffffffff = not created)
 *                        +0x04 instruction/cost counter
 *   0x3256ba  short  – shader-cost accounting enabled flag
 *   0x5a5558  int    – accumulated shader cost for the frame
 *
 * Asserts at lines 0xa35 (index >= 0) and 0xa36 (index < 0x43). The bound is
 * written `>= 0x43` to match the original's `CMP DI,0x43 / JL`.
 */
/* 0x156440 */
void rasterizer_set_vertex_shader(short vertex_shader_index)
{
  char success;
  int index;

  if (vertex_shader_index < 0) {
    display_assert("vertex_shader_index>=0",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xa35, true);
    system_exit(-1);
  }

  if (vertex_shader_index >= 0x43) {
    display_assert("vertex_shader_index<NUMBER_OF_VERTEX_SHADERS",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xa36, true);
    system_exit(-1);
  }

  if (vertex_shader_index != *(short *)0x325164) {
    index = vertex_shader_index;

    /* Tested as `!= 0xffffffff` so the bind is the fall-through and the
     * "not valid" report is the out-of-line block, matching the original's
     * `CMP ECX,-1 / JZ LAB_001564d8` at 0x1564ab. */
    if (*(uint32_t *)(0x325208 + index * 0x10) != 0xffffffff) {
      D3DDevice_SetVertexShader(*(uint32_t *)(0x325208 + index * 0x10));
      success = 1;
      if (*(short *)0x3256ba != 0) {
        /* Written accumulator-first (not `+=`) to match the original's
         * MOV EDX,[0x5a5558] / MOV ECX,[ESI+0x32520c] / ADD EDX,ECX order. */
        *(int *)0x5a5558 = *(int *)0x5a5558 + *(int *)(0x32520c + index * 0x10);
      }
    } else {
      error(2, "### ERROR vertex shader not valid (#%d)", index);
      success = 0;
    }

    *(short *)0x325164 = vertex_shader_index;

    if (!success) {
      error(2, "### ERROR rasterizer_set_vertex_shader failed");
    }
  }
}

/*
 * rasterizer_set_pixel_shader @ 0x156510 — programs the NV2A register
 * combiners from a D3DPIXELSHADERDEF, either by pushing the individual
 * combiner registers inline (<= 5 combiner stages) or by handing the whole
 * definition to D3DDevice_SetPixelShaderProgram.
 *
 * Field offsets are the stock XDK D3DPIXELSHADERDEF layout, confirmed by the
 * loop's base pointer walk (ESI starts at pixel_shader+0x88 and steps 4 per
 * combiner stage, so the six loads at [ESI-0x88]/[ESI-0x60]/[ESI-0x40]/
 * [ESI-0x20]/[ESI]/[ESI+0x2c] are the dword arrays at +0x00/+0x28/+0x48/
 * +0x68/+0x88/+0xb4):
 *   +0x00  PSAlphaInputs[8]            +0xac  PSFinalCombinerConstant0
 *   +0x20  PSFinalCombinerInputsABCD   +0xb0  PSFinalCombinerConstant1
 *   +0x24  PSFinalCombinerInputsEFG    +0xb4  PSRGBOutputs[8]
 *   +0x28  PSConstant0[8]              +0xd4  PSCombinerCount
 *   +0x48  PSConstant1[8]              +0xd8  PSTextureModes
 *   +0x68  PSAlphaOutputs[8]           +0xdc  PSDotMapping
 *   +0x88  PSRGBInputs[8]              +0xe0  PSInputTexture
 *
 * PSCombinerCount packs three fields the original reads separately:
 *   bits 0..3   number of active combiner stages (byte load: XOR EAX,EAX /
 *               MOV AL,[ESI+0xd4] / AND EAX,0xf, then a 16-bit CMP AX,6)
 *   bit 12      per-stage PSConstant0 (mux enable) — otherwise only stage 0
 *   bit 16      per-stage PSConstant1 (mux enable) — otherwise only stage 0
 * Both mux bits are consumed with MOVZX at the accounting site, so they are
 * unsigned char here.
 *
 * Render-state indices passed to D3DDevice_SetRenderStateNotInline are the
 * induction variable EDI (= 0x22 + stage) biased per array:
 *   LEA [EDI-0x22] = 0x00 + stage  PSAlphaInputs
 *   LEA [EDI-0x18] = 0x0a + stage  PSConstant0
 *   LEA [EDI-0x10] = 0x12 + stage  PSConstant1
 *   LEA [EDI-0x08] = 0x1a + stage  PSAlphaOutputs
 *       EDI        = 0x22 + stage  PSRGBInputs
 *   LEA [EDI+0x0b] = 0x2d + stage  PSRGBOutputs
 * D3DDevice_SetRenderStateNotInline is stdcall (RET 8) taking
 * (state_index, value): PUSH <value>; PUSH <index>; CALL, no ADD ESP. Its
 * kb.json placeholder decl `void(void)` was corrected to match.
 *
 * D3DDevice_SetRenderState_Simple is the ECX/EDX fastcall form; each of the
 * seven simple sets is followed by a write of the same value into the D3D
 * host-side shadow table, so the shadow store is written after the call as
 * the disassembly has it (Ghidra reorders these).
 *
 * Globals:
 *   0x1fb6b8/0x1fb6bc  uint32  – shadow for NV2A regs 0x40288/0x4028c
 *   0x1fb744/0x1fb748  uint32  – shadow for regs 0x41e20/0x41e24
 *   0x1fb76c/0x1fb774/0x1fb778 uint32 – shadow for regs 0x41e60/0x41e74/0x41e78
 *   0x3256ba  short – shader-cost accounting mode (2 = pixel-shader costs)
 *   0x5a555c  int   – accumulated pixel-shader cost for the frame
 *
 * Assert "pixel_shader" at line 0xa5c.
 */
/* 0x156510 */
void rasterizer_set_pixel_shader(void *pixel_shader)
{
  short combiner_count;
  short stage;
  unsigned char constant0_per_stage;
  unsigned char constant1_per_stage;
  uint32_t combiner_flags;
  uint32_t value;

  if (pixel_shader == 0) {
    display_assert("pixel_shader",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xa5c, true);
    system_exit(-1);
  }

  combiner_count = (short)(*(uint32_t *)((char *)pixel_shader + 0xd4) & 0xf);

  if (combiner_count < 6) {
    combiner_flags = *(uint32_t *)((char *)pixel_shader + 0xd4);
    constant1_per_stage = (unsigned char)((combiner_flags >> 0x10) & 1);
    constant0_per_stage = (unsigned char)((combiner_flags >> 0xc) & 1);

    D3DDevice_SetRenderState_PSTextureModes(
      *(uint32_t *)((char *)pixel_shader + 0xd8));

    value = *(uint32_t *)((char *)pixel_shader + 0xd4);
    D3DDevice_SetRenderState_Simple(0x41e60, value);
    *(uint32_t *)0x1fb76c = value;

    value = *(uint32_t *)((char *)pixel_shader + 0xe0);
    D3DDevice_SetRenderState_Simple(0x41e78, value);
    *(uint32_t *)0x1fb778 = value;

    value = *(uint32_t *)((char *)pixel_shader + 0xdc);
    D3DDevice_SetRenderState_Simple(0x41e74, value);
    *(uint32_t *)0x1fb774 = value;

    value = *(uint32_t *)((char *)pixel_shader + 0x20);
    D3DDevice_SetRenderState_Simple(0x40288, value);
    *(uint32_t *)0x1fb6b8 = value;

    value = *(uint32_t *)((char *)pixel_shader + 0x24);
    D3DDevice_SetRenderState_Simple(0x4028c, value);
    *(uint32_t *)0x1fb6bc = value;

    value = *(uint32_t *)((char *)pixel_shader + 0xac);
    D3DDevice_SetRenderState_Simple(0x41e20, value);
    *(uint32_t *)0x1fb744 = value;

    value = *(uint32_t *)((char *)pixel_shader + 0xb0);
    D3DDevice_SetRenderState_Simple(0x41e24, value);
    *(uint32_t *)0x1fb748 = value;

    /* Index loop: the original's latch is CMP AX,word[EBP-8] (stage vs
     * combiner_count), not a byte-offset compare, so the induction variable
     * is written as the stage index and VC71 strength-reduces it into the
     * ESI pointer walk plus the EDI = 0x22 + stage register-index. */
    for (stage = 0; stage < combiner_count; stage++) {
      D3DDevice_SetRenderStateNotInline(
        0x00 + stage, ((uint32_t *)((char *)pixel_shader + 0x00))[stage]);
      D3DDevice_SetRenderStateNotInline(
        0x1a + stage, ((uint32_t *)((char *)pixel_shader + 0x68))[stage]);
      D3DDevice_SetRenderStateNotInline(
        0x22 + stage, ((uint32_t *)((char *)pixel_shader + 0x88))[stage]);
      D3DDevice_SetRenderStateNotInline(
        0x2d + stage, ((uint32_t *)((char *)pixel_shader + 0xb4))[stage]);

      if (constant0_per_stage != 0 || stage == 0) {
        D3DDevice_SetRenderStateNotInline(
          0x0a + stage, ((uint32_t *)((char *)pixel_shader + 0x28))[stage]);
      }

      if (constant1_per_stage != 0 || stage == 0) {
        D3DDevice_SetRenderStateNotInline(
          0x12 + stage, ((uint32_t *)((char *)pixel_shader + 0x48))[stage]);
      }
    }

    if (*(short *)0x3256ba == 2) {
      /* Accumulator-first to match MOV ECX,[0x5a555c] / IMUL EDX,EAX /
       * LEA EDX,[ECX+EDX*4+0x20]. */
      *(int *)0x5a555c =
        *(int *)0x5a555c +
        (constant1_per_stage + constant0_per_stage + 4) * combiner_count * 4 +
        0x20;
    }
  } else {
    D3DDevice_SetPixelShaderProgram(pixel_shader);
    if (*(short *)0x3256ba == 2) {
      *(int *)0x5a555c = *(int *)0x5a555c + 0xe4;
    }
  }
}

/*
 * rasterizer_set_model_skinning @ 0x156710 — uploads a model's node matrices
 * into the vertex-shader constant staging buffer at 0x476208 and pushes them
 * to constant register -0x24 (c-36), three float4 registers per node.
 *
 * The node matrix is a real_matrix4x3 (0x34 bytes):
 *   +0x00 scale, +0x04/+0x10/+0x1c the three basis vectors, +0x28 position.
 *
 * Store offsets taken from the raw disassembly, not the decompiler.  Each node
 * emits 0x30 bytes at 0x476208 + node_index*0x30:
 *
 *   dest offset  source                    note
 *   -----------  ------------------------  -----------------------------
 *   +0x00        scale * node[+0x04]       vectors[0].i
 *   +0x04        scale * node[+0x10]       vectors[1].i
 *   +0x08        scale * node[+0x1c]       vectors[2].i
 *   +0x0c        node[+0x28]               position.i, raw dword MOV
 *   +0x10        scale * node[+0x08]       vectors[0].j
 *   +0x14        scale * node[+0x14]       vectors[1].j
 *   +0x18        scale * node[+0x20]       vectors[2].j
 *   +0x1c        node[+0x2c]               position.j, raw dword MOV
 *   +0x20        scale * node[+0x0c]       vectors[0].k
 *   +0x24        scale * node[+0x18]       vectors[1].k
 *   +0x28        scale * node[+0x24]       vectors[2].k
 *   +0x2c        node[+0x30]               position.k, raw dword MOV
 *
 * i.e. the rotation part is transposed and pre-scaled on upload while the
 * translation column is copied unscaled as a raw dword (MOV, never FLD/FSTP,
 * in the original).  `scale` stays FPU-resident across the whole body: the
 * original loads it once per node and duplicates it with FLD ST0 before each
 * FMUL, consuming the base copy on the last multiply.
 *
 * `skinning->node_matrices` is re-read from memory on every iteration
 * (MOV EDI,[ESI] at 0x156798 is inside the loop body), so the load is written
 * inside the loop here rather than hoisted.
 *
 * Globals:
 *   0x476208  float[]  – vertex-shader constant staging buffer (0x30/node)
 *   0x3256ba  short    – shader-cost accounting enabled flag
 *   0x5a5550  int      – accumulated vertex-shader constant bytes
 */
/* 0x156710 */
void rasterizer_set_model_skinning(void *skinning)
{
  short node_index;
  const char *node;
  float *dst;
  float scale;

  if (skinning == 0) {
    display_assert("skinning",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xabd, true);
    system_exit(-1);
  }

  if (*(int *)skinning == 0) {
    display_assert("skinning->node_matrices",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xabe, true);
    system_exit(-1);
  }

  if (!(*(short *)((char *)skinning + 4) > 0 &&
        *(short *)((char *)skinning + 4) < 0x2c)) {
    display_assert("skinning->node_matrix_count>0 && "
                   "skinning->node_matrix_count<"
                   "RASTERIZER_MAXIMUM_NODES_PER_MODEL",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xabf, true);
    system_exit(-1);
  }

  for (node_index = 0; node_index < *(short *)((char *)skinning + 4);
       node_index++) {
    node = *(const char **)skinning + node_index * 0x34;
    dst = (float *)(0x476208 + node_index * 0x30);
    scale = *(const float *)node;

    dst[0] = scale * *(const float *)(node + 0x04);
    dst[1] = scale * *(const float *)(node + 0x10);
    dst[2] = scale * *(const float *)(node + 0x1c);
    *(uint32_t *)(dst + 3) = *(const uint32_t *)(node + 0x28);
    dst[4] = scale * *(const float *)(node + 0x08);
    dst[5] = scale * *(const float *)(node + 0x14);
    dst[6] = scale * *(const float *)(node + 0x20);
    *(uint32_t *)(dst + 7) = *(const uint32_t *)(node + 0x2c);
    dst[8] = scale * *(const float *)(node + 0x0c);
    dst[9] = scale * *(const float *)(node + 0x18);
    dst[10] = scale * *(const float *)(node + 0x24);
    *(uint32_t *)(dst + 11) = *(const uint32_t *)(node + 0x30);
  }

  D3DDevice_SetVertexShaderConstant(-0x24, (const void *)0x476208,
                                    *(short *)((char *)skinning + 4) * 3);

  if (*(short *)0x3256ba != 0) {
    *(int *)0x5a5550 = *(short *)((char *)skinning + 4) * 0x30 + *(int *)0x5a5550;
  }
}

/*
 * rasterizer_set_model_lighting_point_light @ 0x156850 — fills one 0x30-byte
 * point-light record (three float4 vertex-shader constants) inside the
 * caller's lighting-constant block.
 *
 * The Ghidra decl is `void FUN_00156850(void)`; the body reads
 * in_stack_00000004 / in_stack_00000008 / in_stack_0000000c, which are the
 * three cdecl stack slots.  Widths come from the prologue loads:
 *   [EBP+0x08]  MOV ESI,dword  -> int    light_index (compared against -1)
 *   [EBP+0x0c]  MOVSX EAX,word -> short  light_slot
 *   [EBP+0x10]  MOV EDI,dword  -> void * lighting_constants
 * The single call site (0x156b96, inside rasterizer_set_model_lighting) pushes
 * constants / slot / index and cleans 0xc, confirming three cdecl args.
 *
 * Source light record: 0x5a37e4 + light_index*0x38, count at 0x5a37e0 (int).
 *   +0x00 pointer to the owning light definition
 *   +0x04..+0x0c  vector copied to record +0x00..+0x08
 *   +0x10..+0x18  vector copied to record +0x10..+0x18
 *   +0x28..+0x30  vector copied to record +0x20..+0x28
 *   +0x34         radius
 *
 * Store offsets taken from the raw disassembly:
 *
 *   dest offset  source
 *   -----------  ------------------------------------------------
 *   +0x00/04/08  light[+0x04]/[+0x08]/[+0x0c]   (raw dword MOVs)
 *   +0x0c        1.0f / (radius * radius)
 *   +0x10/14/18  light[+0x10]/[+0x14]/[+0x18]   (raw dword MOVs)
 *   +0x1c        1.0f / (owner[+0x1c] - owner[+0x20])
 *   +0x20/24/28  light[+0x28]/[+0x2c]/[+0x30]   (raw dword MOVs)
 *   +0x2c        -(that * owner[+0x20])
 *
 * `owner` is re-read from light[+0x00] for the second use (MOV EDX,[ESI] at
 * 0x156978), so the reload is written out here.  The -1.0f test at 0x15695f
 * is an integer compare against the bit pattern 0xbf800000 in the original,
 * not an FCOM, so it is transcribed as an integer compare.
 */
/* 0x156850 */
void rasterizer_set_model_lighting_point_light(int light_index,
                                               short light_slot,
                                               void *lighting_constants)
{
  struct point_light_t {
    const char *owner;
    float position[3];
    float color[3];
    float unk[3];
    float radius;
  };
  const struct point_light_t *light;
  const char *owner;
  char *dst;
  float attenuation;
  int light_count;

  if (lighting_constants == 0) {
    display_assert("lighting_constants",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xb0a, true);
    system_exit(-1);
  }

  /* Written as an if/else rather than an early return so the NONE case lands
   * out of line at the tail (the original's `JZ LAB_00156997`, past the main
   * path's RET) instead of as the fall-through. */
  if (light_index != -1) {
    light_count = *(int *)0x5a37e0;
    if (light_index < 0 || light_index >= light_count) {
      display_assert(csprintf((char *)0x5ab100,
                              "### ERROR invalid light index #%d (count=#%d)",
                              light_index, light_count),
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0xb13, true);
      system_exit(-1);
    }

    light = (const struct point_light_t *)0x5a37e4 + light_index;

    if (!(light->radius > *(const float *)0x2533c0)) {
      display_assert("light->radius>0.0f",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                     0xb15, true);
      system_exit(-1);
    }

    dst = (char *)lighting_constants + light_slot * 0x30;

    {
      struct vec3 { float x, y, z; };
      *(struct vec3 *)dst = *(const struct vec3 *)light->position;
      *(float *)(dst + 0x0c) = *(const float *)0x2533c8 /
        (light->radius * light->radius);
      *(struct vec3 *)(dst + 0x10) = *(const struct vec3 *)light->color;
      *(struct vec3 *)(dst + 0x20) = *(const struct vec3 *)light->unk;
    }

    owner = light->owner;

    if (*(const uint32_t *)(owner + 0x1c) != 0xbf800000) {
      attenuation = 1.0f / (*(const float *)(owner + 0x1c) -
                            *(const float *)(owner + 0x20));
      *(float *)(dst + 0x1c) = attenuation;
      *(float *)(dst + 0x2c) =
        -(attenuation * *(const float *)(*(const char *const *)light + 0x20));
    } else {
      *(uint32_t *)(dst + 0x1c) = 0;
      *(float *)(dst + 0x2c) = 1.0f;
    }
  } else {
    csmemset((char *)lighting_constants + light_slot * 0x30, 0, 0x30);
    *(uint32_t *)((char *)lighting_constants + light_slot * 0x30 + 0x1c) = 0;
    *(float *)((char *)lighting_constants + light_slot * 0x30 + 0x2c) = 1.0f;
  }
}

/*
 * FUN_001569f0 @ 0x1569f0 — writes one light into the vertex-shader lighting
 * constant block.
 *
 * The kb declaration was `void (void)`; Ghidra surfaced all three arguments
 * as `in_stack_*`. Widths are proven by the prologue:
 *   [EBP+0x08] MOV ECX,dword  – source light record (may be NULL)
 *   [EBP+0x0c] MOVSX word     – light index (int16)
 *   [EBP+0x10] MOV ESI,dword  – lighting_constants block (asserted non-NULL,
 *                               line 0xb3a)
 *
 * Store offsets taken from the raw disassembly, not the decompiler:
 *
 *   dest offset                       source
 *   --------------------------------  --------------------
 *   (index+3)*0x20 + 0x00             light[+0x0c]
 *   (index+3)*0x20 + 0x04             light[+0x10]
 *   (index+3)*0x20 + 0x08             light[+0x14]
 *   index*0x20 + 0x70 + 0x00          light[+0x00]
 *   index*0x20 + 0x70 + 0x04          light[+0x04]
 *   index*0x20 + 0x70 + 0x08          light[+0x08]
 *
 * i.e. two 12-byte vectors landing 0x10 apart inside the same 0x20-byte
 * per-light record based at lighting_constants+0x60. A NULL light zeroes only
 * the first vector's record (csmemset 0x20 bytes at (index+3)*0x20); the
 * original places that block out of line after the copy path, so the copy is
 * written as the fall-through here.
 *
 * The 0x20/0x70 constants are kept in the original's algebraic form rather
 * than folded, since the record base is unproven.
 *
 * VC71 86.8% (51/55 insns) is a register-allocation ceiling, not a logic
 * gap: the original re-reads `MOVSX EDX, word [EBP+0xc]` inside the
 * out-of-line csmemset block, so the sign-extension is not shared with the
 * copy path and the copy path needs two extra callee-saved registers
 * (PUSH EBX / PUSH EDI plus the matching POPs — exactly the 4 missing
 * instructions). VC71 CSEs the sign-extension above the branch instead.
 * Rewriting the NULL case as an early return to break the CSE was measured
 * and made it worse (63.6%), so the fall-through form is kept.
 */
/* 0x1569f0 */
void FUN_001569f0(void *light, short light_index, void *lighting_constants)
{
  char *dst_a;
  char *dst_b;
  const char *src;

  if (lighting_constants == 0) {
    display_assert("lighting_constants",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xb3a, true);
    system_exit(-1);
  }

  if (light != 0) {
    /* The +0xc source vector goes through its own base pointer to reproduce
     * the original's `LEA EDX,[ECX+0xc]` followed by (%edx)/4(%edx)/8(%edx);
     * the +0x00 vector is addressed straight off `light`, as in the binary. */
    src = (const char *)light + 0xc;
    dst_a = (char *)lighting_constants + (light_index + 3) * 0x20;
    *(uint32_t *)dst_a = *(const uint32_t *)src;
    *(uint32_t *)(dst_a + 4) = *(const uint32_t *)(src + 4);
    *(uint32_t *)(dst_a + 8) = *(const uint32_t *)(src + 8);

    dst_b = (char *)lighting_constants + light_index * 0x20 + 0x70;
    *(uint32_t *)dst_b = *(uint32_t *)light;
    *(uint32_t *)(dst_b + 4) = *(uint32_t *)((char *)light + 4);
    *(uint32_t *)(dst_b + 8) = *(uint32_t *)((char *)light + 8);
    return;
  }

  csmemset((char *)lighting_constants + (light_index + 3) * 0x20, 0, 0x20);
}

/*
 * FUN_00156a90 @ 0x156a90 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DSurface8::LockRect: flags/pRect/pLockedRect arrive in
 * EAX/ECX/EDX, surface (s1) on the stack. Returns S_OK. No direct call
 * sites; RET 0x4.
 */
/* 0x156a90 */
int FUN_00156a90(int r1, int r2, int r3, int s1)
{
  D3DSurface_LockRect((void *)s1, (void *)r3, (void *)r2, r1);
  return 0;
}

/*
 * rasterizer_set_model_lighting @ 0x156ab0 — builds the 0xb0-byte model
 * lighting constant block (11 float4 vertex-shader constants starting at
 * register -0x4f) and uploads it.
 *
 * `lighting` layout as used here:
 *   +0x00..+0x08  ambient colour (3 floats)
 *   +0x0c         short distant_light_count
 *   +0x10         distant lights, stride 0x18
 *   +0x40         short point_light_count
 *   +0x44         point-light indices, stride 4 (int)
 *
 * Constant block layout:
 *   0x00..0x5f  two 0x30-byte point-light records
 * (set_model_lighting_point_light) 0x60..0x9f  two distant-light records
 * (FUN_001569f0) 0xa0..0xab  ambient colour 0xac..0xaf is left uninitialised on
 * the lit path in the original (nothing writes it and there is no memset), so
 * it is left uninitialised here too.
 *
 * The debug override at 0x3256f0 is tested with FLD/FCOMP/TEST AH,0x41/JNZ,
 * i.e. `> 0.0f` with the flat-fill as the fall-through (then) block and the
 * per-light path out of line, so it is written as
 * `if (override > 0.0f) { flat } else { lights }`.
 *
 * Both light loops re-read their count from memory each iteration
 * (CMP word[EBX+0x40],SI at the loop head).  The distant-light selector is
 * branchless in the original (XOR EAX,EAX / SETLE AL / DEC EAX / AND EAX,EDI),
 * which is what a `cond ? pointer : NULL` ternary lowers to; the point-light
 * selector uses a real branch because its false value is -1, not 0.
 *
 * Globals:
 *   0x3256f0  float  – flat-ambient debug override (<= 0 disables)
 *   0x3256ba  short  – shader-cost accounting enabled flag
 *   0x5a5554  int    – accumulated model-lighting constant bytes
 */
/* 0x156ab0 */
void rasterizer_set_model_lighting(void *lighting)
{
  char lighting_constants[0xb0];
  short light_index;

  if (lighting == 0) {
    display_assert("lighting",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xb50, true);
    system_exit(-1);
  }

  if (!(*(short *)((char *)lighting + 0x40) >= 0 &&
        *(short *)((char *)lighting + 0x40) <= 2)) {
    display_assert("lighting->point_light_count>=0 && "
                   "lighting->point_light_count<="
                   "MAXIMUM_RENDERED_POINT_LIGHTS",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xb51, true);
    system_exit(-1);
  }

  if (!(*(short *)((char *)lighting + 0xc) >= 0 &&
        *(short *)((char *)lighting + 0xc) <= 2)) {
    display_assert("lighting->distant_light_count>=0 && "
                   "lighting->distant_light_count<="
                   "MAXIMUM_RENDERED_DISTANT_LIGHTS",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xb52, true);
    system_exit(-1);
  }

  if (*(const float *)0x3256f0 > 0.0f) {
    csmemset(lighting_constants, 0, 0xb0);
    *(float *)(lighting_constants + 0xa0) = *(const float *)0x3256f0;
    *(float *)(lighting_constants + 0xa4) = *(const float *)0x3256f0;
    *(float *)(lighting_constants + 0xa8) = *(const float *)0x3256f0;
  } else {
    for (light_index = 0; light_index < 2; light_index++) {
      rasterizer_set_model_lighting_point_light(
        *(short *)((char *)lighting + 0x40) > light_index ?
          *(int *)((char *)lighting + 0x44 + light_index * 4) :
          -1,
        light_index, lighting_constants);
    }

    for (light_index = 0; light_index < 2; light_index++) {
      FUN_001569f0(*(short *)((char *)lighting + 0xc) > light_index ?
                     (char *)lighting + 0x10 + light_index * 0x18 :
                     (char *)0,
                   light_index, lighting_constants);
    }

    *(uint32_t *)(lighting_constants + 0xa0) = *(uint32_t *)lighting;
    *(uint32_t *)(lighting_constants + 0xa4) =
      *(uint32_t *)((char *)lighting + 4);
    *(uint32_t *)(lighting_constants + 0xa8) =
      *(uint32_t *)((char *)lighting + 8);
  }

  D3DDevice_SetVertexShaderConstant(-0x4f, lighting_constants, 0xb);

  if (*(short *)0x3256ba != 0) {
    *(int *)0x5a5554 = *(int *)0x5a5554 + 0xb0;
  }
}

/*
 * rasterizer_set_frustum_z @ 0x156c30 — rebuilds the camera's view-projection
 * vertex-shader constants (c-96..c-89) after overriding the frustum's near/far
 * plane pair.
 *
 * The global frustum lives at 0x5a5c1c:
 *   +0x10  real_matrix4x3 (scale at +0x10, rows at +0x14/+0x20/+0x2c, position
 *          at +0x38) — the world-to-view transform, four rows of three floats
 *          at 0x5a5c30 stride 0xc
 *   +0x144 4x4 projection matrix — rows at 0x5a5d60/70/80/90
 *
 * Constant block (8 float4 registers, 0x80 bytes on the stack):
 *   [0..15]  transposed product of the 4x3 view rows with the projection
 *            columns; the fourth column of each result row also picks up
 *            projection row 3 (the translation row) because the 4x3 rows have
 *            an implicit w of 0 except the position row
 *   [16..18] 0x5a5bc8/cc/d0    [19] 2.0f
 *   [20..22] 0x5a5bd4/d8/dc    [23] 0.5f
 *   [24..26] 0x5a5c64/68/6c    [27] 1.0f
 *   [28..30] 0x5a5c70/74/78    [31] 255.9375f (0x437ff000)
 *
 * FPU operand order verified against the disassembly at 0x156ca0:
 *   FLD [EAX-4] / FMUL [ECX-0x10]   -> view[j][0] * proj[0][i]
 *   FLD [EAX+4] / FMUL [ECX+0x10]   -> view[j][2] * proj[2][i]  (FADDP)
 *   FLD [ECX]   / FMUL [EAX]        -> proj[1][i] * view[j][1]  (FADDP)
 * The third term has its factors the other way round in the original, so it is
 * written that way here.  Ghidra prints the three-term sum in reverse.
 *
 * The trailing add is FLD [ECX+0x20] / FADD [EDI], i.e. `proj[3][i] + acc`,
 * so it is written source-first rather than as `+=`.
 *
 * render_camera_hack_frustum_z is declared `(void)` by Ghidra but the call
 * site pushes three args and cleans 0xc: (frustum, near_z, far_z).
 */
/* 0x156c30 */
void rasterizer_set_frustum_z(float near_z, float far_z)
{
  float constants[32];
  const float *view_row;
  const float *proj_col;
  float *dst;
  float *acc;
  int i;
  int j;

  if (*(void **)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xb97, true);
    system_exit(-1);
  }

  render_camera_hack_frustum_z((void *)0x5a5c1c, near_z, far_z);

  /* Both loops are transcribed as MSVC's pointer-walk plus down-counter form
   * (LEA EDI,[EBP-0x74] / MOV EBX,4 ... DEC EBX / JNZ, and LEA EDX,[EDI-0xc] /
   * MOV ESI,4 ... DEC ESI / JNZ).  Written as plain indexed `for` loops VC71
   * constant-folds every source address and fully unrolls the inner loop. */
  acc = constants + 3;
  proj_col = (const float *)0x5a5d60;
  i = 4;
  do {
    dst = acc - 3;
    view_row = (const float *)0x5a5c30;
    j = 4;
    do {
      *dst = (view_row[0] * proj_col[0] + view_row[2] * proj_col[8]) +
             proj_col[4] * view_row[1];
      view_row = view_row + 3;
      dst = dst + 1;
      j = j - 1;
    } while (j != 0);
    *acc = proj_col[12] + *acc;
    proj_col = proj_col + 1;
    acc = acc + 4;
    i = i - 1;
  } while (i != 0);

  constants[16] = *(const float *)0x5a5bc8;
  constants[17] = *(const float *)0x5a5bcc;
  constants[18] = *(const float *)0x5a5bd0;
  constants[20] = *(const float *)0x5a5bd4;
  constants[21] = *(const float *)0x5a5bd8;
  constants[22] = *(const float *)0x5a5bdc;
  constants[24] = *(const float *)0x5a5c64;
  constants[25] = *(const float *)0x5a5c68;
  constants[26] = *(const float *)0x5a5c6c;
  constants[28] = *(const float *)0x5a5c70;
  constants[29] = *(const float *)0x5a5c74;
  constants[30] = *(const float *)0x5a5c78;
  constants[19] = 2.0f;
  constants[23] = 0.5f;
  constants[27] = 1.0f;
  constants[31] = 255.9375f;

  D3DDevice_SetVertexShaderConstant(-0x60, constants, 8);
}

/*
 * SetupSmartStates @ 0x156d80 — snapshots the D3D driver's live state tables
 * into the rasterizer's "smart state" mirrors so subsequent state sets can be
 * filtered against a known baseline.
 *
 * Copies, verbatim from the disassembly:
 *   0x1fb698 -> 0x5a57a0, 0x90 dwords (D3D__RenderState, REP MOVSD)
 *   0x1fb498 -> 0x5a55a0, 0x80 bytes  (D3D__DeferredTextureState stage 0)
 *   0x1fb518 -> 0x5a5620, 0x80 bytes  (stage 1)
 *   0x1fb598 -> 0x5a56a0, 0x80 bytes  (stage 2)
 *   0x1fb618 -> 0x5a5720, 0x80 bytes  (stage 3)
 * then zeroes the four dwords at 0x5a5580..0x5a558c.
 *
 * The four texture-stage copies share one byte-stepped loop counter in the
 * original (ADD EAX,4 / CMP EAX,0x80 / JL), so they are written as a single
 * loop rather than four memcpys. The 0x90-dword block is a bare memcpy, which
 * VC71 lowers to the same MOV ECX,0x90 / REP MOVSD.
 *
 * Base addresses cross-checked against FUN_00155630 / FUN_00155880 in this
 * file, which use 0x1fb698 as the render-state shadow and 0x1fb498 as the
 * deferred texture-state shadow (0x20 dwords per stage).
 *
 * Plain cdecl, no args, no frame.
 */
/* 0x156d80 */
void SetupSmartStates(void)
{
  int index;

  memcpy((void *)0x5a57a0, (void *)0x1fb698, 0x90 * 4);

  /* Written as a dword-index loop over the four 0x20-entry stage tables.
   * VC71 strength-reduces the index into the byte offset the original uses
   * (XOR EAX,EAX / ADD EAX,4 / CMP EAX,0x80 / JL); writing the byte offset
   * directly instead makes it substitute a pointer plus a down-counter. */
  for (index = 0; index < 0x20; index++) {
    ((uint32_t *)0x5a55a0)[index] = ((uint32_t *)0x1fb498)[index];
    ((uint32_t *)0x5a5620)[index] = ((uint32_t *)0x1fb518)[index];
    ((uint32_t *)0x5a56a0)[index] = ((uint32_t *)0x1fb598)[index];
    ((uint32_t *)0x5a5720)[index] = ((uint32_t *)0x1fb618)[index];
  }

  *(uint32_t *)0x5a5580 = 0;
  *(uint32_t *)0x5a5584 = 0;
  *(uint32_t *)0x5a5588 = 0;
  *(uint32_t *)0x5a558c = 0;
}

/*
 * rasterizer_filthy_bitmap_default_initialize @ 0x156e00 — creates the three
 * 4x4 A4R4G4B4 "missing texture" checkerboards used as the default hardware
 * formats and publishes them to 0x3256a4/a8/ac.
 *
 * The function name comes from the failure assert string at 0x29e258
 * ("### ERROR rasterizer_filthy_bitmap_default_initialize failed", line 0x137)
 * and the global roles from the argument-echo strings the error reporter is
 * handed, which name default_2d/3d/cm_hardware_format explicitly.  Those match
 * rasterizer_get_default_hardware_format in this file (0x3256a4 = 2D,
 * 0x3256a8 = volume, 0x3256ac = cubemap).
 *
 * Argument values verified against the disassembly (stdcall, right-to-left):
 *   CreateTexture(4, 4, 1, 0, D3DFMT_A4R4G4B4=4, D3DPOOL_MANAGED=1, &2d)
 *   CreateVolumeTexture(4, 4, 4, 1, 0, 4, 1, &3d)
 *   CreateCubeTexture(4, 1, 0, 4, 1, &cm)
 *
 * The `success = cond ? 1 : (report(...), 0)` shape is literal: the original
 * emits TEST BL,BL / JZ err / MOV BL,1 / JMP over / err: PUSH msg / PUSH hr /
 * XOR BL,BL / CALL 0x167ff0 at each check.  The cube-face LockRect and
 * UnlockRect checks push a literal 0 as the HRESULT and test only the running
 * flag, because those two XDK entry points are void inline wrappers on Xbox —
 * there is no UnlockRect CALL in the binary at all, only its error check.
 *
 * D3DLOCKED_RECT is modelled as int[2] (Pitch, pBits) and D3DLOCKED_BOX as
 * int[3] (RowPitch, SlicePitch, pBits), matching the EBP-0x1c / EBP-0x28 stack
 * slots.  pBits is re-read from the stack on every fill iteration in the
 * original (MOV ESI,[EBP-0x18] is inside the loop), which is what writing the
 * store through the array element reproduces.
 *
 * Failure bodies are written as the fall-through with the success bodies
 * jumped to, which is the shape the original emits (JGE over the reporter /
 * JMP LAB_00156fc5, and JNE LAB_00156fe5 with the assert falling straight
 * into the publish stores) — worth +5.0pp over the equivalent if/else form.
 * VC71 95.7% (188/186 insns): the residual is two alignment fillers VC71
 * inserts ahead of the fill loops (LEA ESP,[ESP] and NOP) that the original
 * does not have, plus the CreateCubeTexture reporter being placed beside the
 * assert tail instead of inline at its test.
 */
/* 0x156e00 */
void rasterizer_filthy_bitmap_default_initialize(void)
{
  void *default_2d;
  void *default_3d;
  void *default_cm;
  int locked_rect[2];
  int locked_box[3];
  int face_index;
  int face_count;
  uint16_t pattern[2];
  char success;
  int hr;
  int i;
  int n;

  if (*(void **)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
                   0xef, true);
    system_exit(-1);
  }

  hr = D3DDevice_CreateTexture(4, 4, 1, 0, 4, 1, &default_2d);
  if (hr >= 0) {
    success = 1;
  } else {
    success = 0;
    FUN_00167ff0(hr,
                 "IDirect3DDevice8_CreateTexture(global_d3d_device, 4, 4,"
                 " 1, 0, D3DFMT_A4R4G4B4, D3DPOOL_MANAGED, "
                 "&(IDirect3DTexture8*)default_2d_hardware_format)");
  }

  hr = D3DDevice_CreateVolumeTexture(4, 4, 4, 1, 0, 4, 1, &default_3d);
  if (success != 0 && hr >= 0) {
    success = 1;
  } else {
    success = 0;
    FUN_00167ff0(hr,
                 "IDirect3DDevice8_CreateVolumeTexture(global_d3d_device,"
                 " 4, 4, 4, 1, 0, D3DFMT_A4R4G4B4, D3DPOOL_MANAGED, "
                 "&(IDirect3DVolumeTexture8*)default_3d_hardware_format)");
  }

  hr = D3DDevice_CreateCubeTexture(4, 1, 0, 4, 1, &default_cm);
  /* Failure body is the fall-through and jumps to the shared assert tail
   * (JGE over / PUSH msg / PUSH hr / CALL / JMP LAB_00156fc5); the success
   * body is the jumped-to block. */
  if (success == 0 || hr < 0) {
    FUN_00167ff0(hr,
                 "IDirect3DDevice8_CreateCubeTexture(global_d3d_device, 4, 1, "
                 "0, D3DFMT_A4R4G4B4, D3DPOOL_MANAGED, "
                 "&(IDirect3DCubeTexture8*)default_cm_hardware_format)");
    goto failed;
  }
  {
    if (default_2d != 0 && default_3d != 0 && default_cm != 0) {
      pattern[0] = 0x0f00;
      pattern[1] = 0xf0f0;

      /* The fills are transcribed as MSVC's two-induction-variable down-count
       * loops (XOR EAX,EAX / MOV ECX,count / ... / INC EAX / DEC ECX / JNZ);
       * a plain indexed `for` makes VC71 keep a CMP against the bound. */
      D3DTexture_LockRect(default_2d, 0, locked_rect, 0, 0);
      i = 0;
      n = 0x10;
      do {
        ((uint16_t *)locked_rect[1])[i] = pattern[i & 1];
        i = i + 1;
        n = n - 1;
      } while (n != 0);

      D3DVolumeTexture_LockBox(default_3d, 0, locked_box, 0, 0);
      i = 0;
      n = 0x40;
      do {
        ((uint16_t *)locked_box[2])[i] = pattern[i & 1];
        i = i + 1;
        n = n - 1;
      } while (n != 0);

      success = 1;
      face_index = 0;
      face_count = 6;
      do {
        D3DCubeTexture_LockRect(default_cm, face_index, 0, locked_rect, 0, 0);
        if (success != 0) {
          success = 1;
        } else {
          success = 0;
          FUN_00167ff0(
            0, "IDirect3DCubeTexture8_LockRect((IDirect3DCubeTexture8*)"
               "default_cm_hardware_format, face_index, 0, "
               "&d3d_locked_rect, NULL, 0)");
        }

        i = 0;
        n = 0x10;
        do {
          ((uint16_t *)locked_rect[1])[i] = pattern[i & 1];
          i = i + 1;
          n = n - 1;
        } while (n != 0);

        if (success != 0) {
          success = 1;
        } else {
          success = 0;
          FUN_00167ff0(
            0, "IDirect3DCubeTexture8_UnlockRect((IDirect3DCubeTexture8*)"
               "default_cm_hardware_format, face_index, 0)");
        }

        face_index = face_index + 1;
        face_count = face_count - 1;
      } while (face_count != 0);

      if (success != 0) {
        goto publish;
      }
    }
  }

  /* The assert body is the fall-through of the final flag test (JNE over it)
   * and the publish stores are the jumped-to block placed last -- the
   * original falls out of the noreturn system_exit straight into them. */
failed:
  display_assert("### ERROR rasterizer_filthy_bitmap_default_initialize "
                 "failed",
                 "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0x137,
                 true);
  system_exit(-1);

publish:
  *(void **)0x3256a4 = default_2d;
  *(void **)0x3256a8 = default_3d;
  *(void **)0x3256ac = default_cm;
}
/*
 * FUN_00157010 @ 0x157010 — rasterizer initialize.
 *
 * Creates the IDirect3D8 object and the real device, publishes the screen /
 * title-safe bounds, allocates the D3D resource headers that stand in for the
 * back buffer and depth/stencil surfaces, creates every offscreen render
 * target (secondary, water mip chain, shadow primary/secondary, sun glow
 * primary/secondary), programs the initial render/texture state, then runs the
 * rasterizer sub-system initializers.
 *
 * Return value: `MOV AL,BL` precedes both RETs (0x15790c and 0x15792b), so the
 * accumulated success flag is returned even though the only in-binary
 * reference is the incremental-link thunk at 0x17c7ca (the kb decl said
 * `void`, which is a void-EAX misread).
 *
 * Globals (hardcoded, not in kb.json):
 *   0x325650  char       "rasterizer initialized"
 *   0x325654  short[4]   screen bounds     {top,left,bottom,right}
 *   0x32565c  short[4]   title-safe bounds {top,left,bottom,right}
 *   0x325668  int        render-target index (set to 1)
 *   0x32566c  int        cleared to 0
 *   0x325688  short      push-buffer size in KiB (default 0x200)
 *   0x32568a  short      kick-off size in KiB    (default 0x20)
 *   0x32568c  char       float-depth flag: selects D3DFMT 0x2a vs 0x2b
 *   0x325690  short      requested refresh rate (-1 / 30 / 60 / 0)
 *   0x31fa96  char       vsync enabled
 *   0x476a50  void *     IDirect3D8            0x476ab0  IDirect3DDevice8
 *   0x476ab4  void *     IDirect3DPalette8     0x2ee0a0  0x400-byte palette
 *   0x476a54  void *[2]  render-target texture headers (back buffer pair)
 *   0x476a5c  void *     primary render surface
 *   0x476a60  void *     primary depth/stencil surface
 *   0x476a64/6c  secondary render texture / surface   0x476a68, 0x476a70 zeroed
 *   0x476a74/78  shadow primary texture / surface
 *   0x476a7c/80  shadow secondary texture / surface
 *   0x476a84/88  sun glow primary texture / surface
 *   0x476a8c/90  sun glow secondary texture / surface
 *   0x476a94     water texture, 0x476a98[4] water mip surfaces
 *   0x476aa8     depth/stencil texture header (0x14)
 *   0x476aac     back-buffer surface header (0x18, copied from *0x476a5c)
 *   0x1fb77c..0x1fb7dc  render-state shadow slots written after each
 *                       D3DDevice_SetRenderState_Simple
 */
/* 0x157010 */
char FUN_00157010(void)
{
  d3d_present_parameters_t d3dpp;
  void *palette_data;
  char success;
  short refresh_rate;
  short i;
  int hr;
  uint32_t *header;

  if (*(short *)0x325688 == 0) {
    *(short *)0x325688 = 0x200;
  }
  if (*(short *)0x32568a == 0) {
    *(short *)0x32568a = 0x20;
  }
  D3D_SetPushBufferSize(*(short *)0x325688 << 10, *(short *)0x32568a << 10);

  *(void **)0x476a50 = Direct3DCreate8(0);
  if (*(void **)0x476a50 == 0) {
    error(2, "### ERROR failed to create D3D object");
    success = 0;
  } else {
    *(short *)0x325654 = 0; /* screen bounds: top */
    *(short *)0x325656 = 0; /* left */
    *(short *)0x325658 = 0x1e0; /* bottom (480) */
    *(short *)0x32565a = 0x280; /* right  (640) */
    *(short *)0x32565c = 0x24; /* title-safe bounds: top */
    *(short *)0x32565e = 0x30; /* left */
    *(short *)0x325660 = 0x1bc; /* bottom */
    *(short *)0x325662 = 0x250; /* right */
    *(int *)0x325668 = 1;
    *(int *)0x32566c = 0;

    csmemset(&d3dpp, 0, 0x34);
    /* Back buffer is derived from the screen rectangle, not the constants:
     * width = right - left, height = bottom - top. */
    d3dpp.BackBufferWidth =
      (unsigned int)(*(short *)0x32565a - *(short *)0x325656);
    d3dpp.BackBufferHeight =
      (unsigned int)(*(short *)0x325658 - *(short *)0x325654);
    d3dpp.BackBufferFormat = 6; /* D3DFMT_A8R8G8B8 */
    d3dpp.SwapEffect = 1; /* D3DSWAPEFFECT_DISCARD */
    d3dpp.Windowed = 0;
    d3dpp.EnableAutoDepthStencil = 1;
    /* 0x2a = D3DFMT_D24S8, 0x2b = float depth variant */
    d3dpp.AutoDepthStencilFormat =
      (unsigned int)(0x2a + (*(char *)0x32568c != 0));
    d3dpp.Flags = 1;

    refresh_rate = *(short *)0x325690;
    switch (refresh_rate) {
    case -1:
      d3dpp.FullScreen_PresentationInterval = 0x80000000;
      *(char *)0x31fa96 = 0;
      break;
    case 30:
      d3dpp.FullScreen_PresentationInterval = 2;
      *(char *)0x31fa96 = 1;
      break;
    case 60:
      d3dpp.FullScreen_PresentationInterval = 1;
      *(char *)0x31fa96 = 1;
      break;
    default:
      if (refresh_rate != 0) {
        error(2,
              "### ERROR unsupported refresh rate (%dHz), switching to "
              "default",
              refresh_rate);
        *(short *)0x325690 = 0;
      }
      d3dpp.FullScreen_PresentationInterval = 0;
      *(char *)0x31fa96 = 1;
      break;
    }

    hr = Direct3D_CreateDevice(0, 1, 0, 0x40, &d3dpp, (void **)0x476ab0);
    success =
      (hr >= 0) ?
        1 :
        (FUN_00167ff0(hr, "IDirect3D8_CreateDevice(d3d, D3DADAPTER_DEFAULT, "
                          "D3DDEVTYPE_HAL, NULL, "
                          "RASTERIZER_DEVICE_CREATION_FLAGS, "
                          "&d3d_present_parameters, &global_d3d_device)"),
         0);

    if (*(void **)0x476ab0 == 0) {
      success = 0;
    }
    if (success == 0) {
      *(void **)0x476ab0 = 0;
      error(2, "### ERROR failed to create D3D device");
    } else {
      D3DDevice_GetDeviceCaps((void *)0x5a59e0);
      SetupSmartStates();

      palette_data = 0;
      hr = D3DDevice_CreatePalette(0, (void **)0x476ab4);
      success = (hr >= 0) ?
                  1 :
                  (FUN_00167ff0(hr, "IDirect3DDevice8_CreatePalette("
                                    "global_d3d_device, D3DPALETTE_256, "
                                    "&d3d_palette)"),
                   0);

      D3DPalette_Lock(*(void **)0x476ab4, &palette_data, 0);
      success = (success != 0) ?
                  1 :
                  (FUN_00167ff0(0, "IDirect3DPalette8_Lock(d3d_palette, "
                                   "&palette_data, 0)"),
                   0);

      csmemcpy(palette_data, (void *)0x2ee0a0, 0x400);
      /* IDirect3DPalette8_Unlock is a no-op inline on Xbox — only the
       * success test survives. */
      success = (success != 0) ?
                  1 :
                  (FUN_00167ff0(0, "IDirect3DPalette8_Unlock(d3d_palette)"), 0);

      D3DDevice_SetPalette(0, *(void **)0x476ab4);
      success = (success != 0) ?
                  1 :
                  (FUN_00167ff0(0, "IDirect3DDevice8_SetPalette("
                                   "global_d3d_device, 0, d3d_palette)"),
                   0);

      D3DDevice_SetPalette(1, *(void **)0x476ab4);
      success = (success != 0) ?
                  1 :
                  (FUN_00167ff0(0, "IDirect3DDevice8_SetPalette("
                                   "global_d3d_device, 1, d3d_palette)"),
                   0);

      D3DDevice_SetPalette(2, *(void **)0x476ab4);
      success = (success != 0) ?
                  1 :
                  (FUN_00167ff0(0, "IDirect3DDevice8_SetPalette("
                                   "global_d3d_device, 2, d3d_palette)"),
                   0);

      D3DDevice_SetPalette(3, *(void **)0x476ab4);
      if (success == 0) {
        FUN_00167ff0(0, "IDirect3DDevice8_SetPalette(global_d3d_device, 3, "
                        "d3d_palette)");
      } else {
        D3DDevice_GetBackBuffer(0, 0, (void **)0x476a5c);

        hr = D3DDevice_GetDepthStencilSurface((void **)0x476a60);
        success = (hr >= 0) ?
                    1 :
                    (FUN_00167ff0(hr, "IDirect3DDevice8_GetDepthStencilSurface("
                                      "global_d3d_device, "
                                      "&global_d3d_surface_render_primary_z)"),
                     0);

        *(void **)0x476a54 = debug_malloc(
          0x14, false, "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
          0x232);
        *(void **)0x476a58 = debug_malloc(
          0x14, false, "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
          0x233);
        if (*(void **)0x476a54 == 0 || *(void **)0x476a58 == 0) {
          success = 0;
        } else {
          for (i = 0; i < 2; i++) {
            header = (uint32_t *)((void **)0x476a54)[i];
            header[0] = 0x40001;
            header[1] =
              (i == 1) ? ((uint32_t *)*(void **)0x476a5c)[1] : (uint32_t)0;
            header[2] = 0;
            header[3] = 0x11229;
            header[4] = 0x271df27f;
          }
        }

        header = (uint32_t *)debug_malloc(
          0x18, false, "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
          0x262);
        *(void **)0x476aac = header;
        if (header != 0) {
          /* 0x18 bytes = the six-dword surface header, copied wholesale from
           * the primary render surface (REP MOVSD, ECX=6). */
          memcpy(header, *(void **)0x476a5c, 0x18);
          header[1] = ((uint32_t *)*(void **)0x476a60)[1];
        } else {
          success = 0;
        }

        header = (uint32_t *)debug_malloc(
          0x14, false, "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c",
          0x26f);
        *(void **)0x476aa8 = header;
        if (header != 0) {
          header[0] = 0x40001;
          header[1] = ((uint32_t *)*(void **)0x476a60)[1];
          header[2] = 0;
          header[3] = 0x11229;
          header[4] = 0x271df27f;
        } else {
          success = 0;
        }

        hr =
          D3DDevice_CreateTexture(0x140, 0xf0, 1, 1, 0x12, 0, (void *)0x476a64);
        success =
          (success != 0 && hr >= 0) ?
            1 :
            (FUN_00167ff0(
               hr,
               "IDirect3DDevice8_CreateTexture(global_d3d_device, "
               "RASTERIZER_TARGET_RENDER_SECONDARY_WIDTH, "
               "RASTERIZER_TARGET_RENDER_SECONDARY_HEIGHT, 1, "
               "D3DUSAGE_RENDERTARGET, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, "
               "&global_d3d_texture_render_secondary)"),
             0);

        hr =
          D3DTexture_GetSurfaceLevel(*(void **)0x476a64, 0, (void **)0x476a6c);
        success = (success != 0 && hr >= 0) ?
                    1 :
                    (FUN_00167ff0(hr, "IDirect3DTexture8_GetSurfaceLevel("
                                      "global_d3d_texture_render_secondary, 0, "
                                      "&global_d3d_surface_render_secondary)"),
                     0);
        if (*(void **)0x476a64 == 0 || *(void **)0x476a6c == 0) {
          success = 0;
        }

        *(int *)0x476a68 = 0;
        *(int *)0x476a70 = 0;
        hr = D3DDevice_CreateTexture(0x80, 0x80, 4, 1, 6, 0, (void *)0x476a94);
        success =
          (success != 0 && hr >= 0) ?
            1 :
            (FUN_00167ff0(
               hr, "IDirect3DDevice8_CreateTexture(global_d3d_device, "
                   "RASTERIZER_TARGET_WATER_SIZE, "
                   "RASTERIZER_TARGET_WATER_SIZE, "
                   "RASTERIZER_TARGET_WATER_MAX_MIPMAP_LEVELS, "
                   "D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, "
                   "&global_d3d_texture_water)"),
             0);
        if (*(void **)0x476a94 == 0) {
          success = 0;
        }

        for (i = 0; success != 0 && i < 4; i++) {
          hr = D3DTexture_GetSurfaceLevel(*(void **)0x476a94, i,
                                          &((void **)0x476a98)[i]);
          success =
            (success != 0 && hr >= 0) ?
              1 :
              (FUN_00167ff0(hr, "IDirect3DTexture8_GetSurfaceLevel("
                                "global_d3d_texture_water, mipmap_index, "
                                "&global_d3d_surface_water[mipmap_index])"),
               0);
          if (((void **)0x476a98)[i] == 0) {
            success = 0;
          }
        }

        hr = D3DDevice_CreateTexture(0x80, 0x80, 1, 1, 5, 0, (void *)0x476a74);
        success =
          (success != 0 && hr >= 0) ?
            1 :
            (FUN_00167ff0(
               hr, "IDirect3DDevice8_CreateTexture(global_d3d_device, "
                   "RASTERIZER_TARGET_SHADOW_PRIMARY_SIZE, "
                   "RASTERIZER_TARGET_SHADOW_PRIMARY_SIZE, 1, "
                   "D3DUSAGE_RENDERTARGET, D3DFMT_R5G6B5, D3DPOOL_DEFAULT, "
                   "&global_d3d_texture_shadow_primary)"),
             0);

        hr =
          D3DTexture_GetSurfaceLevel(*(void **)0x476a74, 0, (void **)0x476a78);
        success = (success != 0 && hr >= 0) ?
                    1 :
                    (FUN_00167ff0(hr, "IDirect3DTexture8_GetSurfaceLevel("
                                      "global_d3d_texture_shadow_primary, 0, "
                                      "&global_d3d_surface_shadow_primary)"),
                     0);
        if (*(void **)0x476a74 == 0 || *(void **)0x476a78 == 0) {
          success = 0;
        }

        hr = D3DDevice_CreateTexture(0x80, 0x80, 1, 1, 5, 0, (void *)0x476a7c);
        success =
          (success != 0 && hr >= 0) ?
            1 :
            (FUN_00167ff0(
               hr, "IDirect3DDevice8_CreateTexture(global_d3d_device, "
                   "RASTERIZER_TARGET_SHADOW_SECONDARY_SIZE, "
                   "RASTERIZER_TARGET_SHADOW_SECONDARY_SIZE, 1, "
                   "D3DUSAGE_RENDERTARGET, D3DFMT_R5G6B5, D3DPOOL_DEFAULT, "
                   "&global_d3d_texture_shadow_secondary)"),
             0);

        hr =
          D3DTexture_GetSurfaceLevel(*(void **)0x476a7c, 0, (void **)0x476a80);
        success = (success != 0 && hr >= 0) ?
                    1 :
                    (FUN_00167ff0(hr, "IDirect3DTexture8_GetSurfaceLevel("
                                      "global_d3d_texture_shadow_secondary, 0, "
                                      "&global_d3d_surface_shadow_secondary)"),
                     0);
        if (*(void **)0x476a7c == 0 || *(void **)0x476a80 == 0) {
          success = 0;
        }

        hr = D3DDevice_CreateTexture(0x40, 0x40, 1, 1, 6, 0, (void *)0x476a84);
        success =
          (success != 0 && hr >= 0) ?
            1 :
            (FUN_00167ff0(
               hr, "IDirect3DDevice8_CreateTexture(global_d3d_device, "
                   "RASTERIZER_TARGET_SUN_GLOW_SIZE, "
                   "RASTERIZER_TARGET_SUN_GLOW_SIZE, 1, "
                   "D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, "
                   "&global_d3d_texture_sun_glow_primary)"),
             0);

        hr =
          D3DTexture_GetSurfaceLevel(*(void **)0x476a84, 0, (void **)0x476a88);
        success = (success != 0 && hr >= 0) ?
                    1 :
                    (FUN_00167ff0(hr, "IDirect3DTexture8_GetSurfaceLevel("
                                      "global_d3d_texture_sun_glow_primary, 0, "
                                      "&global_d3d_surface_sun_glow_primary)"),
                     0);
        if (*(void **)0x476a84 == 0 || *(void **)0x476a88 == 0) {
          success = 0;
        }

        hr = D3DDevice_CreateTexture(0x40, 0x40, 1, 1, 6, 0, (void *)0x476a8c);
        success =
          (success != 0 && hr >= 0) ?
            1 :
            (FUN_00167ff0(
               hr, "IDirect3DDevice8_CreateTexture(global_d3d_device, "
                   "RASTERIZER_TARGET_SUN_GLOW_SIZE, "
                   "RASTERIZER_TARGET_SUN_GLOW_SIZE, 1, "
                   "D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, "
                   "&global_d3d_texture_sun_glow_secondary)"),
             0);

        hr =
          D3DTexture_GetSurfaceLevel(*(void **)0x476a8c, 0, (void **)0x476a90);
        success =
          (success != 0 && hr >= 0) ?
            1 :
            (FUN_00167ff0(hr, "IDirect3DTexture8_GetSurfaceLevel("
                              "global_d3d_texture_sun_glow_secondary, 0, "
                              "&global_d3d_surface_sun_glow_secondary)"),
             0);

        if (*(void **)0x476a8c == 0 || *(void **)0x476a90 == 0) {
          /* success = 0 is written after the error() call so VC71 cannot
           * tail-merge this arm with the identical `else` arm below (the
           * reference keeps both blocks separate, with XOR BL,BL scheduled
           * into the push sequence at 0x157739). */
          error(2, "### ERROR failed to create offscreen surface(s)");
          success = 0;
        } else if (success != 0) {
          D3DDevice_SetShaderConstantMode(1);
          success = 1;
          D3DDevice_SetRenderState_ZEnable(1);
          D3DDevice_SetRenderState_Simple(0x4035c, 1);
          *(int *)0x1fb798 = 1;
          D3DDevice_SetRenderState_Simple(0x40354, 0x203);
          *(int *)0x1fb77c = 0x203;
          D3DDevice_SetRenderState_ZBias(0);
          D3DDevice_SetRenderState_Simple(0x40338, 1);
          *(int *)0x1fb7dc = 1;
          D3DDevice_SetRenderState_Simple(0x40300, 0);
          *(int *)0x1fb788 = 0;
          D3DDevice_SetRenderState_Simple(0x4033c, 0x204);
          *(int *)0x1fb780 = 0x204;
          D3DDevice_SetRenderState_Simple(0x40340, 0);
          *(int *)0x1fb78c = 0;
          D3DDevice_SetRenderState_Simple(0x40304, 0);
          *(int *)0x1fb784 = 0;
          D3DDevice_SetRenderState_Simple(0x40344, 1);
          *(int *)0x1fb790 = 1;
          D3DDevice_SetRenderState_Simple(0x40348, 0);
          *(int *)0x1fb794 = 0;
          D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
          *(int *)0x1fb7c0 = 0x8006;
          D3DDevice_SetRenderState_Deferred(0x52, 1);
          D3DDevice_SetRenderState_Deferred(0x5c, 0);
          D3DDevice_SetRenderState_Deferred(0x5d, 1);
          D3DDevice_SetTextureState_TexCoordIndex(0, 0);
          D3DDevice_SetTextureState_TexCoordIndex(1, 1);
          D3DDevice_SetTextureState_TexCoordIndex(2, 2);
          D3DDevice_SetTextureState_TexCoordIndex(3, 3);
          D3DDevice_SetFlickerFilter(5);
          D3DDevice_SetSoftDisplayFilter(0);
        } else {
          error(2, "### ERROR failed to create offscreen surface(s)");
        }
      }
    }
  }

  rasterizer_filthy_bitmap_default_initialize();

  success =
    (success != 0 && (char)rasterizer_memory_pool_new() != 0 &&
     FUN_0015e800() != 0 && (char)rasterizer_transparent_geometry_new() != 0 &&
     rasterizer_vertex_shaders_initialize() != 0 && FUN_0017df80() != 0 &&
     FUN_0017eb50() != 0 && (char)rasterizer_text_cache_initialize() != 0 &&
     FUN_0015c2d0() != 0 && FUN_0016f6c0() != 0 && FUN_001659a0() != 0) ?
      1 :
      0;

  rasterizer_screen_effects_initialize();
  texture_cache_new();
  FUN_0017e010();

  if (success != 0) {
    *(char *)0x325650 = 1;
  } else {
    error(2, "### ERROR failed to initialize rasterizer");
  }
  return success;
}
