/*
 * interface/progress_bar.c — loading progress bar UI
 * XBE source: c:\halo\SOURCE\interface\progress_bar.c
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0xe1c70  progress_bar_initialize
 *   0xe1c80  progress_bar_dispose
 *   0xe1c90  progress_bar_begin
 *   0xe1cc0  progress_bar_end
 *   0xe1ce0  ui_automation_is_active
 *   0xe29a0  progress_bar_screen_initialize
 *   0xe2e50  progress_bar_render
 *   0xe3300  progress_bar_display
 */

#include "common.h"

/*
 * pow via x87 FYL2X + F2XM1 + FSCALE.
 * Used only for positive base values in [0,1] with small positive exponents.
 * Matches the binary's use of CRT pow through the thunk at 0x1d9e70.
 */
static double pow(double base, double exponent)
{
  double result;
  asm volatile(
    "fyl2x\n\t"          /* ST(0) = exponent * log2(base) */
    "fld %%st(0)\n\t"    /* duplicate */
    "frndint\n\t"         /* ST(0) = integer part */
    "fxch %%st(1)\n\t"   /* swap: ST(0) = full, ST(1) = int */
    "fsub %%st(1), %%st\n\t" /* ST(0) = fractional part */
    "f2xm1\n\t"          /* ST(0) = 2^frac - 1 */
    "fld1\n\t"            /* push 1.0 */
    "faddp\n\t"           /* ST(0) = 2^frac */
    "fscale\n\t"          /* ST(0) = 2^frac * 2^int = 2^(exp*log2(base)) */
    "fstp %%st(1)\n\t"   /* pop the integer part */
    : "=t"(result)
    : "0"(base), "u"(exponent)
    : "st(1)"
  );
  return result;
}

/* XDK SetThreadPriority thunk (stdcall) */
typedef int(__stdcall *SetThreadPriority_fn)(int handle, int priority);
#define XSetThreadPriority ((SetThreadPriority_fn)0x1cf999)

/* Progress bar globals */
static char *progress_bar_suppress(void)
{
  return (char *)0x46c3ec;
}

static char *progress_bar_active(void)
{
  return (char *)0x46c3f8;
}

static int *progress_bar_screen_initialized(void)
{
  return (int *)0x46c3f0;
}

static float *progress_bar_start_progress(void)
{
  return (float *)0x46c214;
}

static char *progress_bar_rendering_enabled(void)
{
  return (char *)0x30f030;
}

/* progress_bar_initialize — no-op stub. */
void progress_bar_initialize(void)
{
}

/* progress_bar_dispose — no-op stub. */
void progress_bar_dispose(void)
{
}

/*
 * progress_bar_begin — start a progress bar session.
 *
 * Sets the suppress flag to the inverse of the argument, marks the bar
 * as active, and raises the current thread to highest priority.
 *
 * Confirmed: SETZ AL stores !(param_1) to 0x46c3ec.
 * Confirmed: MOV byte ptr [0x46c3f8], 1.
 * Confirmed: SetThreadPriority(-2, 2) — current thread,
 * THREAD_PRIORITY_HIGHEST.
 */
void progress_bar_begin(bool suppress)
{
  *progress_bar_suppress() = (suppress == 0);
  *progress_bar_active() = 1;
  XSetThreadPriority(-2, 2);
}

/*
 * progress_bar_end — finish a progress bar session.
 *
 * Clears the active flag and restores the current thread to normal priority.
 *
 * Confirmed: MOV byte ptr [0x46c3f8], 0.
 * Confirmed: SetThreadPriority(-2, 0) — current thread, THREAD_PRIORITY_NORMAL.
 */
void progress_bar_end(void)
{
  *progress_bar_active() = 0;
  XSetThreadPriority(-2, 0);
}

/*
 * ui_automation_is_active — return whether a progress bar is currently active.
 *
 * Confirmed: MOV AL, [0x46c3f8] / RET — single byte load, returns bool.
 */
bool ui_automation_is_active(void)
{
  return *progress_bar_active();
}

/*
 * progress_bar_screen_initialize — one-time setup for the loading screen.
 *
 * Initializes two 4x4 identity color matrices at 0x46c358 and 0x46c398,
 * builds an ortho projection matrix at 0x46c318 and a perspective projection
 * matrix at 0x46c2d8, decodes the progress bar texture from embedded RLE data
 * at 0x30f040 and generates a gradient texture. Configures D3D render state
 * for alpha blending and texture stage states for the loading bar overlay.
 * Zeroes and configures two screen primitive blocks at 0x5aa480 and 0x5aa580.
 * Creates 4 DirectSound buffers from embedded PCM data at 0x315828 (0x47dc
 * bytes) with different pitch offsets for ambient loading audio. Optionally
 * clears the back buffer if the suppress flag was set.
 *
 * Confirmed: Two 4x4 identity matrices written to 0x46c358 and 0x46c398.
 * Confirmed: ortho matrix at 0x46c318, perspective at 0x46c2d8.
 * Confirmed: CALL 0xe24e0 with EAX=height, ECX=width, EDX=0x46c3f0.
 * Confirmed: CALL 0xe2580 (gradient texture generation).
 * Confirmed: D3D render states via 0x1e9350 (register-arg, ECX=reg, EDX=val).
 * Confirmed: 5 SetTextureStageState calls via 0x1e9410.
 * Confirmed: Two 0xf0-byte memset + store blocks for screen primitives.
 * Confirmed: 4 DirectSound buffers at 0x46c3d8..0x46c3e4 with loop at ESI.
 * Confirmed: Pitch values 0xffffedb7, 0xffffe70e, 0xfffff061, 0xffffddbb.
 * Confirmed: Back-buffer clear block gated by byte at 0x46c3ec.
 */
/* 0xe29a0 */
void progress_bar_screen_initialize(void)
{
  int i;
  int hr;
  void *depth_surface;
  void *back_buffer;
  void *front_buffer;

  /*
   * DSBUFFERDESC-compatible struct (0x18 bytes).
   * Layout confirmed from memset(desc, 0, 0x18) then field stores:
   *   +0x00: dwSize = 0x18
   *   +0x04: dwFlags = 0x40100
   *   +0x08: dwBufferBytes = 0
   *   +0x0c: lpwfxFormat = &wfx
   */
  unsigned char ds_desc[0x18];

  /*
   * WAVEFORMATEX + 2-byte ADPCM extension.
   * Layout confirmed from disassembly stores at EBP-0x20:
   *   +0x00 (word): wFormatTag = 0x69 (XBOX_ADPCM)
   *   +0x02 (word): nChannels = 1
   *   +0x04 (dword): nSamplesPerSec = 0x5622 (22050)
   *   +0x08 (dword): nAvgBytesPerSec = 0x3060 (12384)
   *   +0x0c (word): nBlockAlign = 0x24 (36)
   *   +0x0e (word): wBitsPerSample = 4
   *   +0x10 (word): cbSize = 2
   *   +0x12 (word): extra = 0x40 (64 samples per block)
   */
  unsigned char wfx[0x14];

  /* Initialize two 4x4 identity color matrices at 0x46c358 and 0x46c398.
   * Each is 64 bytes (16 floats). Diagonal elements set to 1.0f, rest 0. */
  /* Matrix 1 at 0x46c358 */
  *(uint32_t *)0x46c390 = 0;
  *(uint32_t *)0x46c38c = 0;
  *(uint32_t *)0x46c388 = 0;
  *(uint32_t *)0x46c384 = 0;
  *(uint32_t *)0x46c37c = 0;
  *(uint32_t *)0x46c378 = 0;
  *(uint32_t *)0x46c374 = 0;
  *(uint32_t *)0x46c370 = 0;
  *(uint32_t *)0x46c368 = 0;
  *(uint32_t *)0x46c364 = 0;
  *(uint32_t *)0x46c360 = 0;
  *(uint32_t *)0x46c35c = 0;
  *(uint32_t *)0x46c394 = 0x3f800000; /* 1.0f */
  *(uint32_t *)0x46c380 = 0x3f800000;
  *(uint32_t *)0x46c36c = 0x3f800000;
  *(uint32_t *)0x46c358 = 0x3f800000;

  /* Matrix 2 at 0x46c398 */
  *(uint32_t *)0x46c3d0 = 0;
  *(uint32_t *)0x46c3cc = 0;
  *(uint32_t *)0x46c3c8 = 0;
  *(uint32_t *)0x46c3c4 = 0;
  *(uint32_t *)0x46c3bc = 0;
  *(uint32_t *)0x46c3b8 = 0;
  *(uint32_t *)0x46c3b4 = 0;
  *(uint32_t *)0x46c3b0 = 0;
  *(uint32_t *)0x46c3a8 = 0;
  *(uint32_t *)0x46c3a4 = 0;
  *(uint32_t *)0x46c3a0 = 0;
  *(uint32_t *)0x46c39c = 0;
  *(uint32_t *)0x46c3d4 = 0x3f800000;
  *(uint32_t *)0x46c3c0 = 0x3f800000;
  *(uint32_t *)0x46c3ac = 0x3f800000;
  *(uint32_t *)0x46c398 = 0x3f800000;

  /* Build ortho projection matrix at 0x46c318 (scale_x=2.0, scale_y=2.0,
   * near=-1.0, far=2.0). stdcall, RET 0x14. */
  matrix_build_ortho_projection((void *)0x46c318, 2.0f, 2.0f, -1.0f, 2.0f);

  /* Build perspective projection matrix at 0x46c2d8 (fov=0.64, aspect=0.48,
   * near=1.0, far=1000.0). stdcall, RET 0x14. */
  matrix_build_perspective_projection((void *)0x46c2d8, 0.64f, 0.48f, 1.0f,
                                      1000.0f);

  /* Decode the progress bar texture from embedded RLE data.
   * EAX = MOVSX height from [0x30f038], ECX = MOVSX width from [0x30f034],
   * EDX = 0x46c3f0 (out texture ptr), stack: data=0x30f040, size=0x67e4. */
  progress_bar_decode_texture(
    (int)*(int16_t *)0x30f038,  /* height (sign-extended) */
    (int)*(int16_t *)0x30f034,  /* width (sign-extended) */
    (void *)0x46c3f0,           /* out_texture */
    (void *)0x30f040,           /* data */
    0x67e4                      /* data_size */
  );

  /* Generate gradient texture for the loading bar effect. */
  progress_bar_generate_gradient_texture();

  /* D3D render state setup via D3DDevice_SetRenderState_Simple (register-arg).
   * ECX = push buffer register offset, EDX = value. */
  /* SetRenderState_Simple(0x40304, 1) — alpha blend enable */
  D3DDevice_SetRenderState_Simple(0x40304, 1);
  *(uint32_t *)0x1fb784 = 1;

  /* SetRenderState_Simple(0x40344, 0x302) — src/dst blend */
  D3DDevice_SetRenderState_Simple(0x40344, 0x302);
  *(uint32_t *)0x1fb790 = 0x302;

  /* SetRenderState_Simple(0x40348, 1) — blend op */
  D3DDevice_SetRenderState_Simple(0x40348, 1);
  *(uint32_t *)0x1fb794 = 1;

  /* CullMode = 0 (D3DCULL_NONE) */
  D3DDevice_SetRenderState_CullMode(0);

  /* ZEnable = 0 (disable depth testing) */
  D3DDevice_SetRenderState_ZEnable(0);

  /* SetRenderState_Deferred(0x5c, 0) */
  D3DDevice_SetRenderState_Deferred(0x5c, 0);

  /* SetTexture stage 0 to the decoded progress bar texture */
  D3DDevice_SetTexture(0, *(void **)0x46c3f0);

  /* SetTextureStageState calls:
   * stage=0, state=0xe (COLOROP), value=2 (SELECTARG1)
   * stage=0, state=0xd (COLORARG1), value=2 (TEXTURE)
   * stage=0, state=0xf (COLORARG2), value=1 (CURRENT)
   * stage=0, state=0xa (ALPHAOP), value=3 (MODULATE)
   * stage=0, state=0xb (ALPHAARG1), value=3 */
  D3DDevice_SetTextureStageState(0, 0xe, 2);
  D3DDevice_SetTextureStageState(0, 0xd, 2);
  D3DDevice_SetTextureStageState(0, 0xf, 1);
  D3DDevice_SetTextureStageState(0, 0xa, 3);
  D3DDevice_SetTextureStageState(0, 0xb, 3);

  /* Initialize screen primitive 1 at 0x5aa480 (0xf0 bytes) */
  csmemset((void *)0x5aa480, 0, 0xf0);
  *(uint32_t *)0x5aa558 = 0x8421;
  *(uint32_t *)0x5aa554 = 3;
  *(uint32_t *)0x5aa508 = 0x8a009a0;
  *(uint32_t *)0x5aa534 = 0xc00;
  *(uint32_t *)0x5aa50c = 0xaa00ba0;
  *(uint32_t *)0x5aa538 = 0xd00;
  *(uint32_t *)0x5aa510 = 0xca00da0;
  *(uint32_t *)0x5aa53c = 0xc00;
  *(uint32_t *)0x5aa480 = 0x14200000;
  *(uint32_t *)0x5aa4e8 = 0xc0;
  *(uint32_t *)0x5aa4a0 = 0xc;
  *(uint32_t *)0x5aa4a4 = 0x1c00;

  /* Initialize screen primitive 2 at 0x5aa580 (0xf0 bytes) */
  csmemset((void *)0x5aa580, 0, 0xf0);
  *(uint32_t *)0x5aa658 = 0x21;
  *(uint32_t *)0x5aa654 = 1;
  *(uint32_t *)0x5aa608 = 0x8040000;
  *(uint32_t *)0x5aa634 = 0xc0;
  *(uint32_t *)0x5aa580 = 0x14190000;
  *(uint32_t *)0x5aa5e8 = 0xc0;
  *(uint32_t *)0x5aa5a0 = 0x200c0000;
  *(uint32_t *)0x5aa5a4 = 0x1c00;

  /* Build WAVEFORMATEX for Xbox ADPCM audio */
  *(uint16_t *)&wfx[0x00] = 0x69;    /* wFormatTag (XBOX_ADPCM) */
  *(uint16_t *)&wfx[0x02] = 1;       /* nChannels */
  *(uint32_t *)&wfx[0x04] = 0x5622;  /* nSamplesPerSec (22050) */
  *(uint32_t *)&wfx[0x08] = 0x3060;  /* nAvgBytesPerSec (12384) */
  *(uint16_t *)&wfx[0x0c] = 0x24;    /* nBlockAlign (36) */
  *(uint16_t *)&wfx[0x0e] = 4;       /* wBitsPerSample */
  *(uint16_t *)&wfx[0x10] = 2;       /* cbSize */
  *(uint16_t *)&wfx[0x12] = 0x40;    /* extra (64 samples per block) */

  /* Build DSBUFFERDESC */
  csmemset(ds_desc, 0, 0x18);
  *(uint32_t *)&ds_desc[0x00] = 0x18;     /* dwSize */
  *(uint32_t *)&ds_desc[0x04] = 0x40100;  /* dwFlags */
  *(uint32_t *)&ds_desc[0x08] = 0;        /* dwBufferBytes */
  *(uint32_t *)&ds_desc[0x0c] = (uint32_t)wfx;  /* lpwfxFormat */

  /* Create 4 sound buffers at 0x46c3d8..0x46c3e4 */
  for (i = 0; i < 4; i++) {
    hr = DirectSoundCreateBuffer(ds_desc, (void **)(0x46c3d8 + i * 4));
    if (hr < 0) {
      return;
    }
    IDirectSoundBuffer_SetBufferData(*(void **)(0x46c3d8 + i * 4),
                                     (void *)0x315828, 0x47dc);
    IDirectSoundBuffer_SetLoopRegion(*(void **)(0x46c3d8 + i * 4), 0, 0x47dc);
    IDirectSoundBuffer_SetCurrentPosition(*(void **)(0x46c3d8 + i * 4), 0);
    IDirectSoundBuffer_SetVolume(*(void **)(0x46c3d8 + i * 4),
                                 (int)0xffffec78);
    IDirectSoundBuffer_Play(*(void **)(0x46c3d8 + i * 4), 0, 0, 1);
  }

  /* Set distinct pitches for each of the 4 sound buffers */
  IDirectSoundBuffer_SetPitch(*(void **)0x46c3d8, (int)0xffffedb7);
  IDirectSoundBuffer_SetPitch(*(void **)0x46c3dc, (int)0xffffe70e);
  IDirectSoundBuffer_SetPitch(*(void **)0x46c3e0, (int)0xfffff061);
  IDirectSoundBuffer_SetPitch(*(void **)0x46c3e4, (int)0xffffddbb);

  /* If the suppress flag was set, clear the back buffer to black */
  if (*progress_bar_suppress() != 0) {
    *progress_bar_suppress() = 0;
    D3DDevice_GetDepthStencilSurface(&depth_surface);
    D3DDevice_GetBackBuffer(0, 0, &back_buffer);
    D3DDevice_GetBackBuffer(-1, 0, &front_buffer);
    D3DDevice_SetRenderTarget(front_buffer, depth_surface);
    D3DDevice_Clear(0, 0, 0xf0, 0, 0, 0);
    D3DDevice_SetRenderTarget(back_buffer, depth_surface);
  }
}

/*
 * progress_bar_render — render one frame of the loading progress bar.
 *
 * Updates a phase timer and computes a smoothed progress value from the
 * normalized input. Adjusts 4 DirectSound buffer volumes based on how far
 * progress falls within per-channel fade ranges. Sets up D3D transforms
 * (saving/restoring projection, world, and view matrices), render states
 * for alpha blending, and draws a full-screen tinted overlay plus the
 * textured loading bar. Waits for vertical blank before returning.
 *
 * Confirmed: phase timer at 0x46c400 incremented by 0.01f each frame.
 * Confirmed: smoothed progress at 0x46c3fc with 0.2 lerp factor, clamped to 1.0.
 * Confirmed: 4 sound buffers at 0x46c3d8..0x46c3e4 with volume ranges
 *   [3500.0, 4500.0, 3500.0, 4500.0] and fade ranges [(0,1),(0.4,1),(0.5,1),(0.55,1)].
 * Confirmed: pow(sin(factor*pi), 0.3) for volume envelope.
 * Confirmed: pow(sin(progress*pi), 0.9) for base color intensity.
 * Confirmed: D3D transforms saved to 0x46c258/0x46c298/0x46c218,
 *   loading bar transforms set from 0x46c358/0x46c398/0x46c318.
 * Confirmed: render states: alpha blend enable, src/dst blend 0x302, blend op 1.
 * Confirmed: FUN_000e2040 called with (0, 0, fade_alpha) cdecl.
 * Confirmed: FUN_000e26c0 called with EAX=rect, ECX=color, stack=(1.0f, progress) cdecl.
 * Confirmed: system_milliseconds stored to 0x5aa670 for timeout tracking.
 */
/* 0xe2e50 */
void progress_bar_render(float normalized_progress)
{
  int i;
  float phase_timer;
  float smoothed;
  float factor;
  float pow_result;
  float base_color[3];
  float fade_alpha;
  void *back_buffer_surface;

  /* Per-channel fade ranges: (start, end) pairs for 4 sound channels */
  float ranges[8];
  ranges[0] = 0.0f;
  ranges[1] = 1.0f;
  ranges[2] = 0.4f;
  ranges[3] = 1.0f;
  ranges[4] = 0.5f;
  ranges[5] = 1.0f;
  ranges[6] = 0.55f;
  ranges[7] = 1.0f;

  /* Max volume levels per sound channel */
  float volumes[4];
  volumes[0] = 3500.0f;
  volumes[1] = 4500.0f;
  volumes[2] = 3500.0f;
  volumes[3] = 4500.0f;

  /* Advance phase timer and compute sin of accumulated phase */
  phase_timer = *(float *)0x46c400;
  phase_timer = phase_timer + 0.01f;
  *(float *)0x46c400 = phase_timer;

  /* Smooth the progress value: lerp toward input with oscillation */
  smoothed = (sinf(phase_timer) * 0.05 + normalized_progress
              - *(float *)0x46c3fc) * 0.2
             + *(float *)0x46c3fc;
  if (smoothed > 1.0) {
    smoothed = 1.0f;
  }
  *(float *)0x46c3fc = smoothed;

  /* Compute base color intensity from smoothed progress */
  pow_result = (float)pow(sinf(smoothed * 3.14159), 0.9);
  base_color[0] = pow_result;
  base_color[1] = pow_result;
  base_color[2] = pow_result;

  /* Update volume for each of the 4 sound buffers */
  for (i = 0; i < 4; i++) {
    /* Compute fade factor: how far smoothed progress is within this channel's range */
    if (smoothed < ranges[i * 2]) {
      factor = 0.0f;
    } else if (smoothed > ranges[i * 2 + 1]) {
      factor = 0.0f;
    } else {
      factor = (smoothed - ranges[i * 2]) / (ranges[i * 2 + 1] - ranges[i * 2]);
    }

    /* Set volume if this sound buffer is valid */
    if (*(int *)(0x46c3d8 + i * 4) != 0) {
      int volume = (int)(
        (float)pow(sinf(factor * 3.1415), 0.3) * volumes[i] - 5000.0f
      );
      IDirectSoundBuffer_SetVolume(*(void **)(0x46c3d8 + i * 4), volume);
    }
  }

  /* Store current timestamp for timeout tracking */
  *(int *)0x5aa670 = system_milliseconds();

  /* Save current D3D transforms */
  D3DDevice_GetTransform(6, (void *)0x46c258);
  D3DDevice_GetTransform(0, (void *)0x46c298);
  D3DDevice_GetTransform(1, (void *)0x46c218);

  /* Set loading bar transforms */
  D3DDevice_SetTransform(6, (void *)0x46c358);
  D3DDevice_SetTransform(0, (void *)0x46c398);
  D3DDevice_SetTransform(1, (void *)0x46c318);

  /* Configure render states for alpha-blended overlay */
  D3DDevice_SetRenderState_Simple(0x40304, 1);
  *(uint32_t *)0x1fb784 = 1;

  D3DDevice_SetRenderState_Simple(0x40344, 0x302);
  *(uint32_t *)0x1fb790 = 0x302;

  D3DDevice_SetRenderState_Simple(0x40348, 1);
  *(uint32_t *)0x1fb794 = 1;

  D3DDevice_SetRenderState_CullMode(0);
  D3DDevice_SetRenderState_ZEnable(0);
  D3DDevice_SetRenderState_Deferred(0x5c, 0);

  /* Texture stage states for stage 0 (3 stages, 5 state sets each) */
  /* Stage 0 */
  D3DDevice_SetTextureStageState(0, 0xe, 2);
  D3DDevice_SetTextureStageState(0, 0xd, 2);
  D3DDevice_SetTextureStageState(0, 0xf, 1);
  D3DDevice_SetTextureStageState(0, 0xa, 3);
  D3DDevice_SetTextureStageState(0, 0xb, 3);
  /* Stage 1 */
  D3DDevice_SetTextureStageState(1, 0xe, 2);
  D3DDevice_SetTextureStageState(1, 0xd, 2);
  D3DDevice_SetTextureStageState(1, 0xf, 1);
  D3DDevice_SetTextureStageState(1, 0xa, 1);
  D3DDevice_SetTextureStageState(1, 0xb, 1);
  /* Stage 2 */
  D3DDevice_SetTextureStageState(2, 0xe, 2);
  D3DDevice_SetTextureStageState(2, 0xd, 2);
  D3DDevice_SetTextureStageState(2, 0xf, 1);
  D3DDevice_SetTextureStageState(2, 0xa, 1);
  D3DDevice_SetTextureStageState(2, 0xb, 1);

  /* Set texture, clear screen, set vertex shader and pixel shader */
  D3DDevice_SetTexture(0, *(void **)0x46c3f0);
  D3DDevice_Clear(0, 0, 0xf0, 0, 1.0f, 0);
  D3DDevice_SetVertexShader(0);
  D3DDevice_SetPixelShaderProgram((void *)0x5aa480);

  /* Get back buffer to build texture descriptor */
  D3DDevice_GetBackBuffer(-1, 0, &back_buffer_surface);

  {
    /* Build a texture descriptor on the stack (5 dwords at EBP-0x44) */
    uint32_t tex_desc[5];
    tex_desc[0] = 0x40005;
    tex_desc[1] = *(uint32_t *)((char *)back_buffer_surface + 4);
    tex_desc[2] = 0;
    tex_desc[3] = 0x11229;
    tex_desc[4] = 0x271df27f;

    /* Set texture stage states and texture for 4 stages */
    for (i = 0; i < 4; i++) {
      D3DDevice_SetTextureStageState((uint32_t)i, 0xe, 2);
      D3DDevice_SetTextureStageState((uint32_t)i, 0xd, 2);
      D3DDevice_SetTextureStageState((uint32_t)i, 0xf, 1);
      D3DDevice_SetTextureStageState((uint32_t)i, 0xa, 3);
      D3DDevice_SetTextureStageState((uint32_t)i, 0xb, 3);
      D3DDevice_SetTexture((uint32_t)i, (void *)tex_desc);
    }
  }

  /* Compute and draw the overlay fade quad */
  if (smoothed >= 0.9f) {
    fade_alpha = 0.9f - (smoothed - 0.9f) * 9.0f;
  } else {
    fade_alpha = 0.9f;
  }
  progress_bar_draw_fullscreen_overlay(0, 0, fade_alpha);

  /* Set textures for the loading bar */
  D3DDevice_SetTexture(0, *(void **)0x46c3f0);
  D3DDevice_SetTexture(1, *(void **)0x46c3f4);
  D3DDevice_SetTextureState_BorderColor(1, 0x5050505);

  /* Texture stage states for the bar rendering */
  D3DDevice_SetTextureStageState(1, 0xa, 4);
  D3DDevice_SetTextureStageState(1, 0xb, 3);

  /* Set pixel shader for loading bar */
  D3DDevice_SetPixelShaderProgram((void *)0x5aa580);

  /* Build screen rect struct (7 floats) and draw the loading bar.
   * Layout at EBP-0x30: [screen_w*2, screen_h*2, x_offset, y_offset,
   *                      half_w, half_h, depth] */
  {
    float screen_rect[7];

    screen_rect[4] = fabsf((float)320.0);
    screen_rect[5] = fabsf((float)240.0);

    screen_rect[0] = 640.0f;
    screen_rect[1] = 480.0f;
    screen_rect[2] = 0.0f;
    screen_rect[3] = 0.0f;
    screen_rect[6] = 0.0f;

    base_color[2] = 0.0f;

    progress_bar_draw_loading_bar(screen_rect, base_color, 1.0f, smoothed);
  }

  /* Clear all 4 texture stages */
  for (i = 0; i < 4; i++) {
    D3DDevice_SetTexture((uint32_t)i, 0);
  }

  /* Restore saved D3D transforms */
  D3DDevice_SetTransform(6, (void *)0x46c258);
  D3DDevice_SetTransform(0, (void *)0x46c298);
  D3DDevice_SetTransform(1, (void *)0x46c218);

  /* Wait for vertical blank */
  D3DDevice_BlockUntilVerticalBlank();
}

/*
 * progress_bar_display — update the progress bar with the current progress.
 *
 * Asserts progress is in [0.0, 1.0]. If the bar is active and progress > 0,
 * initializes the progress screen on first call (0xe29a0), stores the start
 * progress, and if rendering is enabled, calls the render function (0xe2e50)
 * with the normalized progress fraction.
 *
 * Confirmed: FCOMP against 0.0f at 0x2533c0 and 1.0f at 0x2533c8.
 * Confirmed: assert "(progress>=0.f) && (progress<=1.f)" at line 0x410.
 * Confirmed: CALL 0xe29a0 (progress screen init), CALL 0xe2e50 (render).
 * Confirmed: normalized = (progress - start) / (1.0f - start).
 */
void progress_bar_display(float progress)
{
  if (progress < 0.0f || progress > 1.0f) {
    display_assert("(progress>=0.f) && (progress<=1.f)",
                   "c:\\halo\\SOURCE\\interface\\progress_bar.c", 0x410, 1);
    system_exit(-1);
  }

  if (*progress_bar_active() != 0 && progress > 0.0f) {
    if (*progress_bar_screen_initialized() == 0) {
      progress_bar_screen_initialize();
      *progress_bar_start_progress() = progress;
    }
    if (*progress_bar_rendering_enabled() != 0) {
      float normalized = (progress - *progress_bar_start_progress()) /
                         (1.0f - *progress_bar_start_progress());
      progress_bar_render(normalized);
    }
  }
}
