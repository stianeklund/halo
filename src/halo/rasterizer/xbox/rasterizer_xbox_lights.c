#include "x87_math.h"

/*
 * FUN_00169650 @ 0x169650 — D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::SetVertexData2f. Disassembly (11 instructions):
 *   push ebp; mov ebp,esp; mov eax,[ebp+0x10]; mov ecx,[ebp+0xc];
 *   push eax; push ecx; push edx; call 0x1ed280 (D3DDevice_SetVertexData2f,
 *   __stdcall); xor eax,eax; pop ebp; ret 0xc
 * Byte-identical in shape to the already-ported FUN_000e1f00 (0xe1f00) in
 * progress_bar.c and its siblings in rasterizer.c.
 *
 * RET 0xc => __stdcall with three stack args at +8/+0xc/+0x10. The first
 * (+8, the device pointer of the inline member instantiation) is never read;
 * it stays in the signature so the callee-cleans immediate is correct.
 * PUSH EDX at 0x16965b with no prior write to EDX anywhere in the function
 * => the D3D register index is an implicit register input, @<edx> in
 * kb.json, not a stack slot. XOR EAX,EAX => returns S_OK.
 *
 * Both floats are forwarded as raw dwords through EAX/ECX with no FLD/FSTP,
 * a pure bit passthrough, so they are typed `float`, matching the callee's
 * kb.json declaration.
 *
 * Argument order into the callee is from the push sequence — last push is
 * the first argument — so SetVertexData2f(reg, a, b) with a=[EBP+0xc],
 * b=[EBP+0x10].
 *
 * The C impl is cdecl, not __stdcall, even though kb.json records the
 * original as __stdcall: knowledge.py strips the convention from any
 * @<reg> declaration when generating decl.h, and patch.py's reverse thunk
 * restores the original RET 0xc contract for the original callers.
 *
 * xrefs_to is empty in the fingerprinted Ghidra artifact — no callers found.
 */
int FUN_00169650(void *device, uint32_t reg, float a, float b)
{
  (void)device;
  D3DDevice_SetVertexData2f(reg, a, b);
  return 0;
}

/*
 * FUN_00169670 @ 0x169670 — D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::SetVertexData4f. Disassembly (16 instructions):
 *   push ebp; mov ebp,esp; mov eax,[ebp+0x1c]; mov ecx,[ebp+0x18];
 *   mov edx,[ebp+0x14]; push eax; mov eax,[ebp+0x10]; push ecx;
 *   mov ecx,[ebp+0xc]; push edx; push eax; push ecx;
 *   call 0x1ed2c0 (D3DDevice_SetVertexData4f, __stdcall);
 *   xor eax,eax; pop ebp; ret 0x18
 *
 * RET 0x18 => __stdcall with six stack args at +8..+0x1c. The first (+8,
 * the device pointer of the inline member instantiation) is never read; it
 * stays in the signature so the callee-cleans immediate is correct.
 *
 * Unlike the sibling FUN_00169650, the D3D register index here is NOT an
 * implicit register input: EDX is written by MOV EDX,[EBP+0x14] at 0x169679
 * before the PUSH EDX at 0x169684, and every one of the five forwarded
 * values comes from a stack slot. So there is no @<reg> annotation.
 *
 * Argument order into the callee is from the push sequence — last push is
 * the first argument — so SetVertexData4f(reg, a, b, c, d) with
 * reg=[EBP+0xc], a=[EBP+0x10], b=[EBP+0x14], c=[EBP+0x18], d=[EBP+0x1c].
 *
 * All four floats are forwarded as raw dwords through GPRs with no FLD/FSTP,
 * a pure bit passthrough, so they are typed `float`, matching the callee's
 * kb.json declaration. XOR EAX,EAX => returns S_OK.
 *
 * xrefs_to is empty in the fingerprinted Ghidra artifact — no callers found.
 */
int __stdcall FUN_00169670(void *device, uint32_t reg, float a, float b,
                           float c, float d)
{
  (void)device;
  D3DDevice_SetVertexData4f(reg, a, b, c, d);
  return 0;
}

static const char kLightsFile[] =
  "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_lights.c";

/*
 * FUN_001696d0 @ 0x1696d0 — full-screen light/lens-flare accumulation pass.
 *
 * ABI (from disassembly, not the decompiler):
 *   ESI  = `bounds`, a 4-float rect {x0, x1, y0, y1}. The name is binary
 *          evidence: TEST ESI,ESI / assert("bounds", ..., 0x143) at
 *          0x1696d9..0x1696ee. => @<esi> register parameter.
 *   [EBP+8] = one cdecl stack argument, forwarded verbatim as the first
 *          argument of FUN_00158140 at 0x1699a9 (MOV EDX,[EBP+8]; PUSH EDX
 *          last of five pushes).
 *
 * Frame: SUB ESP,0x80 = exactly one 32-float vertex-shader constant block
 * at EBP-0x80..EBP-0x4, uploaded whole by
 * D3DDevice_SetVertexShaderConstant(-0x51, block, 8) at 0x169937
 * (PUSH 8; LEA ECX,[EBP-0x80]; PUSH ECX; PUSH -0x51). It must stay a single
 * contiguous `float vs[32]` with no sibling locals.
 *
 * Constant block (8 vec4 registers, offsets read off the MOV [EBP-N] stores):
 *   r0 = { bounds[1]-bounds[0], 0, 0, bounds[0] }   (FLD [ESI+4]; FSUB [ESI])
 *   r1 = { 0, bounds[3]-bounds[2], 0, bounds[2] }   (FLD [ESI+c]; FSUB [ESI+8])
 *   r2..r7 alternate { 1,0,0,0 } and { 0,1,0,0 }.
 * MSVC interleaves those stores with the call setup; source order here is
 * natural index order, same as the sibling passes in this TU family.
 *
 * ADD ESP,0x24 at 0x1699ae is the cdecl mis-grouping trap: it cleans
 * FUN_00158140 (5) + csmemset (3) + rasterizer_set_pixel_shader (1) = 9
 * dwords, not a 9-argument call.
 *
 * Second FUN_00158140 takes the zero-extended WORD at 0x5a5bc0
 * (XOR EAX,EAX; MOV AX,[0x005a5bc0]) — a 16-bit read, not the dword the
 * decompiler's `(uint)DAT_005a5bc0` suggests.
 *
 * Globals used by address (not in kb.json):
 *   0x476ab0  void*    global_d3d_device (asserted non-NULL, line 0x144)
 *   0x476204  ptr      texture globals base; +0x6c is a bitmap tag index
 *   0x1fb7a4/0x1fb784/0x1fb788  uint32  shadow render-state cache
 *   0x5a5ac0  0xf0 bytes  pixel-shader state block
 *   0x5a5bc0  uint16   current rasterizer render target
 */
void FUN_001696d0(int param_1, float *bounds)
{
  float vs[32];

  if (bounds == 0) {
    display_assert("bounds", kLightsFile, 0x143, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device", kLightsFile, 0x144, 1);
    system_exit(-1);
  }

  FUN_001584f0(0, 0, 0);

  D3DDevice_SetTextureStageState(0, 0xa, 3);
  D3DDevice_SetTextureStageState(0, 0xb, 3);
  D3DDevice_SetTextureStageState(0, 0xd, 2);
  D3DDevice_SetTextureStageState(0, 0xe, 2);
  D3DDevice_SetTextureStageState(0, 0xf, 1);

  rasterizer_set_texture_direct(1, *(int *)(*(char **)0x476204 + 0x6c), 0);

  D3DDevice_SetTextureStageState(1, 0xa, 3);
  D3DDevice_SetTextureStageState(1, 0xb, 3);
  D3DDevice_SetTextureStageState(1, 0xd, 2);
  D3DDevice_SetTextureStageState(1, 0xe, 2);
  D3DDevice_SetTextureStageState(1, 0xf, 2);

  D3DDevice_SetRenderState_CullMode(0x901);
  D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                  NV097_COLOR_MASK_RGBA);
  *(uint32_t *)0x1fb7a4 = NV097_COLOR_MASK_RGBA;
  D3DDevice_SetRenderState_Simple(0x40304, 0);
  *(uint32_t *)0x1fb784 = 0;
  D3DDevice_SetRenderState_Simple(0x40300, 0);
  *(uint32_t *)0x1fb788 = 0;
  D3DDevice_SetRenderState_ZEnable(0);
  D3DDevice_SetRenderState_ZBias(0);

  FUN_00178b40(0x26, 8, 0);

  vs[0] = bounds[1] - bounds[0];
  vs[1] = 0.0f;
  vs[2] = 0.0f;
  vs[3] = bounds[0];
  vs[4] = 0.0f;
  vs[5] = bounds[3] - bounds[2];
  vs[6] = 0.0f;
  vs[7] = bounds[2];
  vs[8] = 1.0f;
  vs[9] = 0.0f;
  vs[10] = 0.0f;
  vs[11] = 0.0f;
  vs[12] = 0.0f;
  vs[13] = 1.0f;
  vs[14] = 0.0f;
  vs[15] = 0.0f;
  vs[16] = 1.0f;
  vs[17] = 0.0f;
  vs[18] = 0.0f;
  vs[19] = 0.0f;
  vs[20] = 0.0f;
  vs[21] = 1.0f;
  vs[22] = 0.0f;
  vs[23] = 0.0f;
  vs[24] = 1.0f;
  vs[25] = 0.0f;
  vs[26] = 0.0f;
  vs[27] = 0.0f;
  vs[28] = 0.0f;
  vs[29] = 1.0f;
  vs[30] = 0.0f;
  vs[31] = 0.0f;
  D3DDevice_SetVertexShaderConstant(-0x51, vs, 8);

  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b98 = 0x21;
  *(uint32_t *)0x5a5b94 = 1;
  *(uint32_t *)0x5a5ae8 = 0xc0000000;
  *(uint32_t *)0x5a5ac0 = 0x19110000;
  *(uint32_t *)0x5a5b28 = 0x100c0;
  *(uint32_t *)0x5a5ae0 = 0x18;
  *(uint32_t *)0x5a5ae4 = 0x1c00;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);

  FUN_00158140(param_1, 0, 0, 0, 0);

  D3DDevice_Begin(7);
  D3DDevice_SetVertexData2s(4, 0, 0);
  D3DDevice_SetVertexData2f(0, -1.015625f, 1.015625f);
  D3DDevice_SetVertexData2s(4, 1, 0);
  D3DDevice_SetVertexData2f(0, 0.984375f, 1.015625f);
  D3DDevice_SetVertexData2s(4, 1, 1);
  D3DDevice_SetVertexData2f(0, 0.984375f, -0.984375f);
  D3DDevice_SetVertexData2s(4, 0, 1);
  D3DDevice_SetVertexData2f(0, -1.015625f, -0.984375f);
  D3DDevice_End();

  FUN_00158140(*(unsigned short *)0x5a5bc0, 0, 0, 0, 1);
}

/*
 * FUN_00169a50 @ 0x169a50 — sun-glow convolve pass: ping-pongs a 4-tap
 * separable blur between two rasterizer render targets for `iterations`
 * passes and returns the target index holding the final result.
 *
 * Name evidence: the failure path at 0x169faa passes
 * "### ERROR rasterizer_sun_glow_convolve failed" to error(). The kb.json
 * name is deliberately left as FUN_00169a50 — a name-only change desyncs
 * tools/verify/function_bounds.json and blocks the commit gate.
 *
 * ABI (from disassembly, not the decompiler): plain RET (no immediate) =>
 * cdecl. Three 16-bit stack params, each occupying a dword slot:
 *   [EBP+0x8]  primary_target    MOV EDI,[EBP+8]; TEST DI,DI; CMP DI,0x8
 * The two target params are typed int, not short, because the reference
 * loads the whole dword slot (MOV EDI,[EBP+8]) and only the compares are
 * 16-bit; a short param makes clang emit a 16-bit load plus a zero-reg
 * compare instead. The (short) casts reproduce the 16-bit compares and
 * the MOVSX at the return.
 *   [EBP+0xc]  secondary_target  MOV ESI,[EBP+0xc]; TEST SI,SI; CMP SI,0x8
 *   [EBP+0x10] iterations        MOV BX,word ptr [EBP+0x10]  (word read)
 * Return is MOVSX EAX,SI / MOVSX EAX,DI at 0x169fb6..0x169fbb — a
 * sign-extended short in full EAX, so the C return type is int:
 *   (iterations & 1) ? secondary_target : primary_target
 * That is the destination of the last executed pass, since pass i writes
 * (i & 1) == 0 ? secondary : primary. The iterations <= 0 path at 0x169ae8
 * jumps straight to this same epilogue, so it is one shared return.
 *
 * Frame: SUB ESP,0x8c = 128 bytes of contiguous vertex-shader constant
 * block at EBP-0x8c..EBP-0x10 (uploaded whole by
 * D3DDevice_SetVertexShaderConstant(-0x51, block, 8) at 0x169c98), then
 * dest at -0xc, i at -0x8, success at -0x1. The float vs[32] must stay one
 * array with no locals interleaved.
 *
 * cdecl ADD ESP mis-grouping (the check_lift_hazards ARG_COUNT warnings on
 * rasterizer_set_pixel_shader and error are this, not real arg counts):
 *   ADD ESP,0x10 @0x169d16 = csmemset(3) + rasterizer_set_pixel_shader(1)
 *   ADD ESP,0x18 @0x169de3 = FUN_00158140(5) + rasterizer_set_pixel_shader(1)
 *   ADD ESP,0x14 @0x169f9c = FUN_00158140(5)
 * error() is variadic and is called here with exactly two arguments.
 *
 * The final FUN_00158140 target is a zero-extended WORD read
 * (XOR EAX,EAX; MOV AX,[0x005a5bc0] at 0x169f85), not a dword — same trap
 * the sibling FUN_001696d0 documents.
 *
 * Ten CALL 0x00167ff0 sites (0x169e09, e2d, e57, e7b, ea5, ec9, ef3, f17,
 * f41, f63), one after each of the ten D3D drawing calls. Each tests the
 * carried success flag and, when it is already clear, re-reports the call
 * text; the flag is never cleared by anything else in this build, so the
 * reports are unreachable on the normal path. Reproduced literally.
 *
 * Per-pass alpha at 0x169dbe..0x169dd8:
 *   XOR EDX,EDX; TEST SI,SI; SETLE DL; DEC EDX; AND EDX,0xffffff80;
 *   ADD EDX,0xff; SHL EDX,0x18  =>  (i <= 0 ? 0xff : 0x7f) << 24
 *
 * Globals used by address (not in kb.json):
 *   0x476ab0  void*    global_d3d_device (asserted non-NULL, line 0x1ad)
 *   0x1fb784/0x1fb788/0x1fb790/0x1fb794/0x1fb7a4/0x1fb7c0  shadow
 *            render-state cache, written after each SetRenderState_Simple
 *   0x5a5ac0  0xf0 bytes  pixel-shader state block
 *   0x5a5ae8  uint32   pixel-shader constant, per-pass alpha
 *   0x5a5bc0  uint16   current rasterizer render target
 */
int FUN_00169a50(int primary_target, int secondary_target, short iterations)
{
  float vs[32];
  int dest;
  int src;
  short i;
  int j;
  char success;

  if ((short)primary_target < 0 || (short)primary_target >= 8) {
    display_assert(
      "primary_target>=0 && primary_target<NUMBER_OF_RASTERIZER_TARGETS",
      kLightsFile, 0x1ab, 1);
    system_exit(-1);
  }
  if ((short)secondary_target < 0 || (short)secondary_target >= 8) {
    display_assert(
      "secondary_target>=0 && secondary_target<NUMBER_OF_RASTERIZER_TARGETS",
      kLightsFile, 0x1ac, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device", kLightsFile, 0x1ad, 1);
    system_exit(-1);
  }

  if (iterations > 0) {
    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                    NV097_COLOR_MASK_RGB);
    *(uint32_t *)0x1fb7a4 = NV097_COLOR_MASK_RGB;
    D3DDevice_SetRenderState_Simple(0x40304, 1);
    *(uint32_t *)0x1fb784 = 1;
    D3DDevice_SetRenderState_Simple(0x40344, 0x304);
    *(uint32_t *)0x1fb790 = 0x304;
    D3DDevice_SetRenderState_Simple(0x40348, 0);
    *(uint32_t *)0x1fb794 = 0;
    D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
    *(uint32_t *)0x1fb7c0 = 0x8006;
    D3DDevice_SetRenderState_Simple(0x40300, 0);
    *(uint32_t *)0x1fb788 = 0;
    D3DDevice_SetRenderState_ZEnable(0);
    D3DDevice_SetRenderState_ZBias(0);

    FUN_00178b40(0x26, 8, 0);

    vs[0] = 1.0f;
    vs[1] = 0.0f;
    vs[2] = 0.0f;
    vs[3] = -0.0078125f;
    vs[4] = 0.0f;
    vs[5] = 1.0f;
    vs[6] = 0.0f;
    vs[7] = -0.0078125f;
    vs[8] = 1.0f;
    vs[9] = 0.0f;
    vs[10] = 0.0f;
    vs[11] = 0.0078125f;
    vs[12] = 0.0f;
    vs[13] = 1.0f;
    vs[14] = 0.0f;
    vs[15] = 0.0078125f;
    vs[16] = 1.0f;
    vs[17] = 0.0f;
    vs[18] = 0.0f;
    vs[19] = -0.0078125f;
    vs[20] = 0.0f;
    vs[21] = 1.0f;
    vs[22] = 0.0f;
    vs[23] = 0.0078125f;
    vs[24] = 1.0f;
    vs[25] = 0.0f;
    vs[26] = 0.0f;
    vs[27] = 0.0078125f;
    vs[28] = 0.0f;
    vs[29] = 1.0f;
    vs[30] = 0.0f;
    vs[31] = -0.0078125f;
    D3DDevice_SetVertexShaderConstant(-0x51, vs, 8);

    success = 1;

    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(uint32_t *)0x5a5b98 = 0x8421;
    *(uint32_t *)0x5a5b94 = 2;
    *(uint32_t *)0x5a5ae8 = 0xff000000;
    *(uint32_t *)0x5a5ac0 = 0x08a009a0;
    *(uint32_t *)0x5a5b28 = 0xc00;
    *(uint32_t *)0x5a5b48 = 0x0aa00ba0;
    *(uint32_t *)0x5a5b74 = 0xc00;
    *(uint32_t *)0x5a5b4c = 0x1c110c11;
    *(uint32_t *)0x5a5b78 = 0xc00;
    *(uint32_t *)0x5a5ae0 = 0xc;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);

    for (i = 0; i < iterations; i++) {
      if ((i & 1) == 0) {
        src = primary_target;
        dest = secondary_target;
      } else {
        src = secondary_target;
        dest = primary_target;
      }

      for (j = 0; j < 4; j++) {
        FUN_001584f0(j, src, 0);
        D3DDevice_SetTextureStageState(j, 0xa, 4);
        D3DDevice_SetTextureStageState(j, 0xb, 4);
        D3DDevice_SetTextureStageState(j, 0xd, 2);
        D3DDevice_SetTextureStageState(j, 0xe, 2);
        D3DDevice_SetTextureStageState(j, 0xf, 1);
      }

      FUN_00158140(dest, 0, 0, 0, 0);

      *(uint32_t *)0x5a5ae8 = (uint32_t)(i <= 0 ? 0xff : 0x7f) << 24;
      rasterizer_set_pixel_shader((void *)0x5a5ac0);

      D3DDevice_Begin(7);
      if (!success) {
        success = 0;
        rasterizer_error(
          0, "IDirect3DDevice8_Begin(global_d3d_device, D3DPT_TRIANGLEFAN)");
      }
      D3DDevice_SetVertexData2s(4, 0, 0);
      if (!success) {
        success = 0;
        rasterizer_error(
          0, "IDirect3DDevice8_SetVertexData2s(global_d3d_device, 4, 0, 0)");
      }
      D3DDevice_SetVertexData2f(0, -1.015625f, 1.015625f);
      if (!success) {
        success = 0;
        rasterizer_error(
          0, "IDirect3DDevice8_SetVertexData2f(global_d3d_device, VSDE_VERTEX, "
             "scale - 1.0f + mysterious_horizontal_offset, scale + 1.0f)");
      }
      D3DDevice_SetVertexData2s(4, 1, 0);
      if (!success) {
        success = 0;
        rasterizer_error(
          0, "IDirect3DDevice8_SetVertexData2s(global_d3d_device, 4, 1, 0)");
      }
      D3DDevice_SetVertexData2f(0, 0.984375f, 1.015625f);
      if (!success) {
        success = 0;
        rasterizer_error(
          0, "IDirect3DDevice8_SetVertexData2f(global_d3d_device, VSDE_VERTEX, "
             "scale + 1.0f + mysterious_horizontal_offset, scale + 1.0f)");
      }
      D3DDevice_SetVertexData2s(4, 1, 1);
      if (!success) {
        success = 0;
        rasterizer_error(
          0, "IDirect3DDevice8_SetVertexData2s(global_d3d_device, 4, 1, 1)");
      }
      D3DDevice_SetVertexData2f(0, 0.984375f, -0.984375f);
      if (!success) {
        success = 0;
        rasterizer_error(
          0, "IDirect3DDevice8_SetVertexData2f(global_d3d_device, VSDE_VERTEX, "
             "scale + 1.0f + mysterious_horizontal_offset, scale - 1.0f)");
      }
      D3DDevice_SetVertexData2s(4, 0, 1);
      if (!success) {
        success = 0;
        rasterizer_error(
          0, "IDirect3DDevice8_SetVertexData2s(global_d3d_device, 4, 0, 1)");
      }
      D3DDevice_SetVertexData2f(0, -1.015625f, -0.984375f);
      if (!success) {
        success = 0;
        rasterizer_error(
          0, "IDirect3DDevice8_SetVertexData2f(global_d3d_device, VSDE_VERTEX, "
             "scale - 1.0f + mysterious_horizontal_offset, scale - 1.0f)");
      }
      D3DDevice_End();
      if (!success) {
        success = 0;
        rasterizer_error(0, "IDirect3DDevice8_End(global_d3d_device)");
      }
    }

    FUN_00158140(*(unsigned short *)0x5a5bc0, 0, 0, 0, 1);

    if (!success) {
      error(2, "### ERROR rasterizer_sun_glow_convolve failed");
    }
  }

  return (iterations & 1) ? (short)secondary_target : (short)primary_target;
}

/* 0x169fd0 — project one sun / lens-flare entry into screen space, punch its
 * alpha silhouette into the occlusion targets, convolve it, then splat 16
 * additive halo quads of growing radius.
 *
 * Name evidence: the tail failure string at 0x2a30d4 reads
 * "### ERROR rasterizer_sun_glow_draw failed", and the per-call debug strings
 * name the original locals verbatim (bounds.x0/x1/y0/y1, r, pass, brightness).
 * The symbol is deliberately kept as FUN_00169fd0 to stay consistent with its
 * sibling FUN_00169a50, whose string likewise says
 * rasterizer_sun_glow_convolve.
 *
 * sun_entry layout actually touched here (0x28-stride array based at 0x4c6480):
 *   +0x00  int     pointer to the sun *definition* tag block
 *   +0x04  float   position x
 *   +0x08  float   position y
 *   +0x0c  float   position z
 *   +0x10  uint32  packed direction, unpacked by uncompress_int32_to_real_vector3d
 * On the definition:
 *   +0x10  float   glow radius — used both as the FMUL scale at 0x16a1b0 and
 *                  as the raw `radius` dword pushed to FUN_00169200 @0x16a1e1
 * No other offset of either structure is read or written by this function; the
 * +0x22/+0x24/+0x30 fields the caller filters on are never touched here.
 *
 * Stack-slot reuse in the original (MSVC coalescing).  Where the lifetimes are
 * provably disjoint the roles are split into separate C locals; that grows the
 * frame past the original 0x98 and is a deliberate, known cost:
 *   [EBP+0x8]   param_1 -> viewport width -> viewport height -> left_f ->
 *               pass -> r -> (bounds.y1 + r) -> pass
 *   [EBP-0x10]  to_sun -> unpacked sun direction -> FUN_00169200 out_screen.
 *               Kept as ONE float[3] (`scratch`): all three roles are 3-float
 *               vectors, and role (b)'s last read (0x16a1d6) precedes the call
 *               at 0x16a1eb that writes role (c).  scratch[2] then survives as
 *               the projected depth handed to SetVertexData4f.
 *   [EBP-0x34]  uncompress_int32_to_real_vector3d out buffer -> screen_bounds[1..3]
 *   [EBP-0x30]  FUN_00169200 out_extent -> screen_bounds[2]
 *               Split (different sizes; merging would need pointer arithmetic
 *               into the middle of an array).  out_extent is written by the
 *               callee and never read back here.
 *   [EBP-0x14]  top_f -> (bounds.x0 - r)
 *   [EBP-0x4]   MOVSX scratch (top / right / bottom) -> loop down-counter
 *
 * FPU association verified against the raw x87 stream, not the decompiler:
 *   dot   = ((v0*f0 + v1*f1) + v2*f2)   FLD/FMUL, FLD/FMUL, FADDP, FLD/FMUL,
 *                                       FADDP  @0x16a039-0x16a056
 *   raw   = (dot - cone) / (1.0f - cone)  FXCH; FSUB ST0,ST1; FLD 1.0;
 *                                       FSUB ST0,ST2; FDIVP; FSTP ST1
 *   Neither `dot` nor `cone` is ever spilled to memory in the original — both
 *   live on the x87 stack, so no narrowing happens between them.
 *   Clamp polarity from FCOM + FNSTSW: TEST AH,0x5 / JP  => `raw < 0.0f`
 *   (NaN falls through to the upper test); TEST AH,0x41 / JNZ => `raw > 1.0f`
 *   (NaN stores `raw`).  Both NaN paths are reproduced by the if/else below.
 *   `r` is TWO separate FMULs (0x16a6f5, 0x16a6fb) — do not fold 0.0625f*80.0f.
 *
 * The per-call `if (success) success = 1; else { success = 0; report(); }`
 * shape is what the reference literally does (TEST BL,BL; JZ; MOV BL,1; JMP /
 * XOR BL,BL; CALL 0x167ff0).  FUN_00169a50 above spells the same construct with
 * a single arm; the two-arm form is kept here because it matches this
 * function's own codegen. */
void FUN_00169fd0(int *sun_entry)
{
  float vs[20]; /* EBP-0x98, uploaded whole by constant -0x44 */
  float world_point[3]; /* EBP-0x48 */
  float screen_bounds[4]; /* EBP-0x38: {left+x0, left+x1, top+y0, top+y1} */
  float dir[3]; /* EBP-0x34: uncompress_int32_to_real_vector3d output buffer */
  float glow_extent[2]; /* EBP-0x30: FUN_00169200 out_extent, never read */
  float bounds[4]; /* EBP-0x24: {x0, x1, y0, y1} — names from strings */
  float scratch[3]; /* EBP-0x10: see slot-reuse note above */
  const float *sun_pos;
  const char *sun_definition;
  float *unpacked;
  double cone;
  float dot;
  float raw;
  float brightness; /* EBP-0x28 */
  float inv_width;
  float inv_height;
  float glow_radius;
  float left_f; /* EBP+0x8  */
  float top_f; /* EBP-0x14 */
  float sy;
  float r;
  float x0r;
  float y0r;
  float x1r;
  float y1r;
  volatile long viewport_width;
  int viewport_height;
  int pass;
  int target;
  char success;
  uint32_t *new_var;

  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device", kLightsFile, 0x247, 1);
    system_exit(-1);
  }

  sun_pos = (const float *)sun_entry;
  scratch[0] = sun_pos[1] - *(float *)0x5a5bc8;
  scratch[1] = sun_pos[2] - *(float *)0x5a5bcc;
  scratch[2] = sun_pos[3] - *(float *)0x5a5bd0;
  normalize3d(scratch);

  dot = scratch[0] * *(float *)0x5a5bd4 + scratch[1] * *(float *)0x5a5bd8 +
        scratch[2] * *(float *)0x5a5bdc;
  /* FLD double [0x25b3f0]; FCOS — the constant is exactly
   * (double)(float)(pi/4). */
  cone = x87_fcos_d(0.78539816f);
  raw = (float)((dot - cone) / (1.0f - cone));
  if (raw < 0.0f) {
    brightness = 0.0f;
  } else if (raw > 1.0f) {
    brightness = 1.0f;
  } else {
    brightness = raw;
  }

  /* MOV AX,[0x5a5bfa]; SUB AX,word[0x5a5bf6]      (16-bit sub)
   * MOV ECX,[0x5a5bf8]; SUB ECX,[0x5a5bf4]; MOVSX ..,CX  (32-bit sub, low word)
   * Viewport rect at 0x5a5bf4 is rectangle2d order: top, left, bottom, right.
   */
  viewport_width = (short)(*(short *)0x5a5bfa - *(short *)0x5a5bf6);
  viewport_height = (short)(*(int *)0x5a5bf8 - *(int *)0x5a5bf4);

  inv_width = 1.0f / (float)viewport_width;
  vs[0] = inv_width + inv_width;
  vs[1] = 0.0f;
  vs[2] = 0.0f;
  vs[3] = -1.0f - inv_width;
  vs[4] = 0.0f;
  inv_height = 1.0f / (float)viewport_height;
  vs[5] = -2.0f * inv_height;
  vs[6] = 0.0f;
  vs[7] = inv_height + 1.0f;
  vs[8] = 0.0f;
  vs[9] = 0.0f;
  vs[10] = 1.0f;
  vs[11] = 0.0f;
  vs[12] = 0.0f;
  vs[13] = 0.0f;
  vs[14] = 0.0f;
  vs[15] = 1.0f;
  vs[16] = 0.0f;
  vs[17] = 0.0f;
  vs[18] = 0.0f;
  vs[19] = 1.0f;
  D3DDevice_SetVertexShaderConstant(-0x44, vs, 5);

  unpacked = uncompress_int32_to_real_vector3d(dir, (unsigned int)sun_entry[4]);
  scratch[0] = unpacked[0];
  scratch[1] = unpacked[1];
  scratch[2] = unpacked[2];
  sun_definition = (const char *)sun_entry[0];
  glow_radius = *(const float *)(sun_definition + 0x10);
  world_point[0] = scratch[0] * glow_radius + sun_pos[1];
  world_point[1] = scratch[1] * glow_radius + sun_pos[2];
  world_point[2] = scratch[2] * glow_radius + sun_pos[3];
  if (!FUN_00169200(world_point, glow_radius, glow_extent, scratch)) {
    return;
  }

  scratch[0] = (float)floor((double)(scratch[0] + 0.5f));
  sy = (float)floor((double)(scratch[1] + 0.5f));
  bounds[0] = scratch[0] - 32.0f;
  bounds[2] = sy - 32.0f;
  bounds[1] = scratch[0] + 32.0f;
  bounds[3] = sy + 32.0f;

  left_f = (float)*(short *)0x5a5bf6;
  screen_bounds[0] = left_f + bounds[0];
  top_f = (float)*(short *)0x5a5bf4;
  screen_bounds[2] = top_f + bounds[2];
  screen_bounds[1] = left_f + bounds[1];
  screen_bounds[3] = top_f + bounds[3];

  /* Four separate FCOM/FNSTSW guards, in this order; every one of them exits
   * the function on failure (all four JMP to the shared epilogue 0x16a8ec). */
  if (!(screen_bounds[0] < (float)*(short *)0x5a5bfa)) {
    return;
  }
  if (!((float)*(short *)0x5a5bf8 > screen_bounds[2])) {
    return;
  }
  if (!(screen_bounds[1] > left_f)) {
    return;
  }
  if (!(screen_bounds[3] > top_f)) {
    return;
  }

  /* Pass 1: stamp the flat 64x64 alpha silhouette into render target 4. */
  FUN_00178b40(0x38, 6, 0);
  D3DDevice_SetRenderState_CullMode(0x901);
  D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                  NV097_COLOR_MASK_ALPHA);
  *(uint32_t *)0x1fb7a4 = NV097_COLOR_MASK_ALPHA;
  D3DDevice_SetRenderState_Simple(0x40304, 0);
  *(uint32_t *)0x1fb784 = 0;
  D3DDevice_SetRenderState_Simple(0x40300, 0);
  new_var = (uint32_t *)0x1fb788;
  *new_var = 0;
  D3DDevice_SetRenderState_ZEnable(0);
  D3DDevice_SetRenderState_ZBias(0);
  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b94 = 1;
  *(uint32_t *)0x5a5b6c = 0;
  *(uint32_t *)0x5a5ae4 = 0x1100;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);
  D3DDevice_Begin(7);
  D3DDevice_SetVertexData2f(0, bounds[0], bounds[2]);
  D3DDevice_SetVertexData2f(0, bounds[1], bounds[2]);
  D3DDevice_SetVertexData2f(0, bounds[1], bounds[3]);
  D3DDevice_SetVertexData2f(0, bounds[0], bounds[3]);
  D3DDevice_End();

  /* Pass 2: depth-tested textured quad that keeps only the unoccluded part. */
  FUN_00178b40(0x38, 6, 0);
  rasterizer_set_texture_direct(0, *(int *)(*(int *)0x476204 + 0x6c), 0);
  D3DDevice_SetTextureStageState(0, 10, 3);
  D3DDevice_SetTextureStageState(0, 0xb, 3);
  D3DDevice_SetTextureStageState(0, 0xd, 2);
  D3DDevice_SetTextureStageState(0, 0xe, 2);
  D3DDevice_SetTextureStageState(0, 0xf, 2);
  D3DDevice_SetRenderState_CullMode(0x901);
  D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                  NV097_COLOR_MASK_ALPHA);
  *(uint32_t *)0x1fb7a4 = NV097_COLOR_MASK_ALPHA;
  D3DDevice_SetRenderState_Simple(0x40304, 0);
  *(uint32_t *)0x1fb784 = 0;
  D3DDevice_SetRenderState_Simple(0x40300, 0);
  *(uint32_t *)0x1fb788 = 0;
  D3DDevice_SetRenderState_ZEnable(1);
  D3DDevice_SetRenderState_Simple(0x40354, 0x203);
  *(uint32_t *)0x1fb77c = 0x203;
  D3DDevice_SetRenderState_Simple(0x4035c, 0);
  *(uint32_t *)0x1fb798 = 0;
  D3DDevice_SetRenderState_ZBias(0);
  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b98 = 1;
  *(uint32_t *)0x5a5b94 = 1;
  *(uint32_t *)0x5a5ac0 = 0x18200000;
  *(uint32_t *)0x5a5b28 = 0x200c0;
  *(uint32_t *)0x5a5ae4 = 0x1c00;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);
  D3DDevice_Begin(7);
  D3DDevice_SetVertexData2s(4, 0, 0);
  D3DDevice_SetVertexData4f(0, bounds[0], bounds[2], scratch[2], 1.0f);
  D3DDevice_SetVertexData2s(4, 1, 0);
  D3DDevice_SetVertexData4f(0, bounds[1], bounds[2], scratch[2], 1.0f);
  D3DDevice_SetVertexData2s(4, 1, 1);
  D3DDevice_SetVertexData4f(0, bounds[1], bounds[3], scratch[2], 1.0f);
  D3DDevice_SetVertexData2s(4, 0, 1);
  D3DDevice_SetVertexData4f(0, bounds[0], bounds[3], scratch[2], 1.0f);
  D3DDevice_End();

  /* MOV BL,0x1 sits at 0x16a587, immediately before the first downsample. */
  success = 1;
  FUN_001696d0(4, screen_bounds);
  FUN_001696d0(5, screen_bounds);
  target = FUN_00169a50(4, 5, 4);

  /* Pass 3: 16 additive halo quads, radius r growing, brightness falling. */
  FUN_00178b40(0x38, 6, 0);
  FUN_001584f0(0, target, 0);
  D3DDevice_SetTextureStageState(0, 10, 3);
  D3DDevice_SetTextureStageState(0, 0xb, 3);
  D3DDevice_SetTextureStageState(0, 0xd, 2);
  D3DDevice_SetTextureStageState(0, 0xe, 2);
  D3DDevice_SetTextureStageState(0, 0xf, 2);
  D3DDevice_SetRenderState_CullMode(0x901);
  SetRenderStateSmart(0x43, 0x10101);
  SetRenderStateSmart(0x3b, 1);
  SetRenderStateSmart(0x3e, 0x302);
  SetRenderStateSmart(0x3f, 1);
  SetRenderStateSmart(0x4a, 0x8006);
  SetRenderStateSmart(0x3c, 0);
  SetRenderStateSmart(0x7b, 0);
  D3DDevice_SetRenderState_ZBias(0);
  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b98 = 1;
  *(uint32_t *)0x5a5b94 = 2;
  *(uint32_t *)0x5a5ae8 = 0xb0b080;
  *(uint32_t *)0x5a5b08 = 0xffffff;
  *(uint32_t *)0x5a5ac0 = 0x48200000;
  *(uint32_t *)0x5a5b28 = 0xc0;
  *(uint32_t *)0x5a5b4c = 0x3c011c02;
  *(uint32_t *)0x5a5b78 = 0xc00;
  *(uint32_t *)0x5a5ae0 = 0xc080000;
  *(uint32_t *)0x5a5ae4 = 0x1400;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);

  for (pass = 0; pass < 16; pass++) {
    /* Two separate FMULs in the original — do not fold to * 5.0f. */
    r = (float)pass * 0.0625f * 80.0f - 4.0f;

    D3DDevice_Begin(7);
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_Begin(global_d3d_device, D3DPT_TRIANGLEFAN)");
    }

    D3DDevice_SetVertexData4f(9, 0.0f, 0.0f, 0.0f,
                              brightness / (float)(pass + 1));
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_SetVertexData4f(global_d3d_device, 9, 0.0f, 0.0f, "
           "0.0f, brightness/(real)(pass + 1))");
    }

    D3DDevice_SetVertexData2s(4, 0, 0);
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_SetVertexData2s(global_d3d_device, 4, 0, 0)");
    }

    y0r = bounds[2] - r;
    x0r = bounds[0] - r;
    D3DDevice_SetVertexData2f(0, x0r, y0r);
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_SetVertexData2f(global_d3d_device, VSDE_VERTEX, "
           "bounds.x0 - r, bounds.y0 - r)");
    }

    D3DDevice_SetVertexData2s(4, 1, 0);
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_SetVertexData2s(global_d3d_device, 4, 1, 0)");
    }

    x1r = r + bounds[1];
    D3DDevice_SetVertexData2f(0, x1r, y0r);
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_SetVertexData2f(global_d3d_device, VSDE_VERTEX, "
           "bounds.x1 + r, bounds.y0 - r)");
    }

    D3DDevice_SetVertexData2s(4, 1, 1);
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_SetVertexData2s(global_d3d_device, 4, 1, 1)");
    }

    y1r = r + bounds[3];
    D3DDevice_SetVertexData2f(0, x1r, y1r);
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_SetVertexData2f(global_d3d_device, VSDE_VERTEX, "
           "bounds.x1 + r, bounds.y1 + r)");
    }

    D3DDevice_SetVertexData2s(4, 0, 1);
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_SetVertexData2s(global_d3d_device, 4, 0, 1)");
    }

    D3DDevice_SetVertexData2f(0, x0r, y1r);
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(
        0, "IDirect3DDevice8_SetVertexData2f(global_d3d_device, VSDE_VERTEX, "
           "bounds.x0 - r, bounds.y1 + r)");
    }

    D3DDevice_End();
    if (success) {
      success = 1;
    } else {
      success = 0;
      rasterizer_error(0, "IDirect3DDevice8_End(global_d3d_device)");
    }
  }

  if (!success) {
    error(2, "### ERROR rasterizer_sun_glow_draw failed");
  }
}
