#include "x87_math.h"

/* FUN_0017ff50: stub (0x17ff50) */
void FUN_0017ff50(void)
{
}

/* rasterizer_frame_statistics.c */

/* rasterizer_frame_statistics_dispose: free frame statistics buffer if
 * allocated (0x17ff60) */
void FUN_0017ff60(void)
{
  void *ptr;
  ptr = *(void **)0x47ec40;
  if (ptr != 0) {
    debug_free(ptr,
               "c:\\halo\\SOURCE\\rasterizer\\rasterizer_frame_statistics.c",
               0x345);
  }
}

/* rasterizer_geometry.c */

/* scale byte 0-255 to float via constant at 0x261518 (0x17ff80) */
float FUN_0017ff80(unsigned char param_1)
{
  return (float)param_1 * *(float *)0x261518;
}

/* scale signed short to float: (2*param_1 + 1.0f) * scale (0x17ffa0) */
float FUN_0017ffa0(short param_1)
{
  return ((float)(int)param_1 + (float)(int)param_1 + *(float *)0x2533c8) *
         *(float *)0x2647f4;
}

/* decode packed 32-bit normal to float[3] output, returns param_1 (0x17ffc0) */
float *FUN_0017ffc0(float *param_1, unsigned int param_2)
{
  float fVar1;
  fVar1 = (float)(int)((param_2 >> 0xb) << 0x15) * *(float *)0x29ba04;
  *param_1 =
    ((float)(int)(param_2 << 0x15) * *(float *)0x29ba04 + *(float *)0x2533c8) *
    *(float *)0x2afe34;
  param_1[1] = (fVar1 + *(float *)0x2533c8) * *(float *)0x2afe34;
  param_1[2] = ((float)(int)(param_2 & 0xffc00000) * *(float *)0x2afe30 +
                *(float *)0x2533c8) *
               *(float *)0x28c8e0;
  return param_1;
}

/* rasterizer_geometry_vertex_type_to_stride: return vertex stride for type,
 * assert valid range (0x180050) */
int FUN_00180050(short param_1)
{
  if ((param_1 < 0) || (0xb < param_1)) {
    display_assert("type>=0 && type<NUMBER_OF_RASTERIZER_VERTEX_TYPES",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0xaa,
                   1);
    system_exit(-1);
  }
  return (int)*(short *)(0x2afe14 + param_1 * 2);
}

/*
 * rasterizer_geometry_vertex_decompress: decompress compressed vertex buffers
 * into their uncompressed forms (0x1800b0).
 *
 * param_1: vertex_type (1=env_vertex, 3=env_lightmap_vertex, 5=model_vertex)
 * param_2: count (number of vertices)
 * param_3: uncompressed_output (output buffer base, int/ptr)
 * param_4: uncompressed_size (expected output byte count for assert)
 * param_5: compressed_input (input buffer base, int/ptr)
 * param_6: compressed_size (expected input byte count for assert, INT not
 * float)
 *
 * Vertex struct sizes (uncompressed / compressed):
 *   env_vertex:           56 (0x38) / 32 (0x20)
 *   env_lightmap_vertex:  20 (0x14) /  8
 *   model_vertex:         68 (0x44) / 32 (0x20)
 *
 * Constants from binary:
 *   _DAT_002533c8 = 1.0f
 *   _DAT_002647f4 = 1.0f/65535.0f  (signed short texcoord scale)
 *   _DAT_00261518 = 1.0f/255.0f    (byte weight scale)
 *
 * Texcoord short->float formula: (s16 * 2 + 1) * (1.0f / 65535.0f)
 * Node weight byte->float:       byte * (1.0f / 255.0f)
 */
void FUN_001800b0(short param_1, int param_2, int param_3, int param_4,
                  int param_5, int param_6)
{
  /* Three 12-byte (3-float) scratch buffers for FUN_0017ffc0 output */
  float buf_c[3]; /* at EBP-0xc */
  float buf_18[3]; /* at EBP-0x18 */
  float buf_24[3]; /* at EBP-0x24 */

  /* temp for MOVSX + FILD idiom: param_1 slot reused as int temp */
  int temp_int;

  float weight0;
  float *result;
  int i;
  int count;
  unsigned int *in32;
  unsigned int *out32;
  unsigned char *in8;
  signed short *in16;
  unsigned char node0_byte;
  unsigned char node1_byte;

  if (param_3 == 0) {
    display_assert("uncompressed",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x118,
                   1);
    system_exit(-1);
  }
  if (param_5 == 0) {
    display_assert("compressed",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x119,
                   1);
    system_exit(-1);
  }

  if (param_1 == 1) {
    /* environment_vertex: uncompressed=56 bytes, compressed=32 bytes */
    count = param_2;
    if (param_2 * 0x38 != param_4) {
      display_assert("count*sizeof(struct "
                     "environment_vertex_uncompressed)==uncompressed_size",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c",
                     0x11f, 1);
      system_exit(-1);
    }
    if ((count << 5) != param_6) {
      display_assert(
        "count*sizeof(struct environment_vertex_compressed)==compressed_size",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x120, 1);
      system_exit(-1);
    }
    if (count > 0) {
      /* ESI = out+0x18, EDI = in+0x10 at loop entry */
      out32 = (unsigned int *)(param_3 + 0x18);
      in32 = (unsigned int *)(param_5 + 0x10);
      for (i = 0; i < count; i++) {
        /* copy position xyz (3 dwords) from in[0..8] to out[0..8] */
        out32[-6] = in32[-4];
        out32[-5] = in32[-3];
        out32[-4] = in32[-2];
        /* unpack normal from in[12] into buf_24 */
        result = FUN_0017ffc0(buf_24, in32[-1]);
        out32[-3] = ((unsigned int *)result)[0];
        out32[-2] = ((unsigned int *)result)[1];
        out32[-1] = ((unsigned int *)result)[2];
        /* unpack binormal from in[16] into buf_18 */
        result = FUN_0017ffc0(buf_18, in32[0]);
        out32[0] = ((unsigned int *)result)[0];
        out32[1] = ((unsigned int *)result)[1];
        out32[2] = ((unsigned int *)result)[2];
        /* unpack tangent from in[20] into buf_c */
        result = FUN_0017ffc0(buf_c, in32[1]);
        out32[3] = ((unsigned int *)result)[0];
        out32[4] = ((unsigned int *)result)[1];
        out32[5] = ((unsigned int *)result)[2];
        /* copy texcoords (2 raw dwords) from in[24..28] to out[48..52] */
        out32[6] = in32[2];
        out32[7] = in32[3];
        /* advance: out += 56 bytes = 14 dwords, in += 32 bytes = 8 dwords */
        out32 += 14;
        in32 += 8;
      }
    }
  } else if (param_1 == 3) {
    /* environment_lightmap_vertex: uncompressed=20 bytes, compressed=8 bytes */
    count = param_2;
    if (param_2 * 0x14 != param_4) {
      display_assert(
        "count*sizeof(struct "
        "environment_lightmap_vertex_uncompressed)==uncompressed_size",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x133, 1);
      system_exit(-1);
    }
    if (param_2 * 8 != param_6) {
      display_assert("count*sizeof(struct "
                     "environment_lightmap_vertex_compressed)==compressed_size",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c",
                     0x134, 1);
      system_exit(-1);
    }
    if (count > 0) {
      /* ESI = out+0x10, EDI = in+0x6 at loop entry */
      out32 = (unsigned int *)(param_3 + 0x10);
      in16 = (signed short *)(param_5 + 6);
      for (i = 0; i < count; i++) {
        /* unpack normal from in[0] into buf_24 */
        result = FUN_0017ffc0(buf_24, *(unsigned int *)(in16 - 3));
        out32[-4] = ((unsigned int *)result)[0];
        out32[-3] = ((unsigned int *)result)[1];
        out32[-2] = ((unsigned int *)result)[2];
        /* texcoord u: (s16 * 2 + 1) * (1/65535) -- in[4] */
        temp_int = (int)in16[-1];
        *(float *)(out32 - 1) =
          ((float)temp_int + (float)temp_int + 1.0f) * (1.0f / 65535.0f);
        /* texcoord v: (s16 * 2 + 1) * (1/65535) -- in[6] */
        temp_int = (int)in16[0];
        *(float *)out32 =
          ((float)temp_int + (float)temp_int + 1.0f) * (1.0f / 65535.0f);
        /* advance: out += 20 bytes = 5 dwords, in += 8 bytes = 4 shorts */
        out32 += 5;
        in16 += 4;
      }
    }
  } else {
    if (param_1 != 5) {
      error(2, "### ERROR can't uncompress this type of vertex buffer");
      return;
    }
    /* model_vertex: uncompressed=68 bytes, compressed=32 bytes */
    count = param_2;
    if (param_2 * 0x44 != param_4) {
      display_assert(
        "count*sizeof(struct model_vertex_uncompressed)==uncompressed_size",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x145, 1);
      system_exit(-1);
    }
    if ((count << 5) != param_6) {
      display_assert(
        "count*sizeof(struct model_vertex_compressed)==compressed_size",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x146, 1);
      system_exit(-1);
    }
    if (count > 0) {
      /* ESI = out+0x18, EDI = in+0x10 at loop entry */
      out32 = (unsigned int *)(param_3 + 0x18);
      in32 = (unsigned int *)(param_5 + 0x10);
      for (i = 0; i < count; i++) {
        /* copy position xyz from in[0..8] to out[0..8] */
        out32[-6] = in32[-4];
        out32[-5] = in32[-3];
        out32[-4] = in32[-2];
        /* unpack normal from in[12] into buf_c */
        result = FUN_0017ffc0(buf_c, in32[-1]);
        out32[-3] = ((unsigned int *)result)[0];
        out32[-2] = ((unsigned int *)result)[1];
        out32[-1] = ((unsigned int *)result)[2];
        /* unpack binormal from in[16] into buf_18 */
        result = FUN_0017ffc0(buf_18, in32[0]);
        out32[0] = ((unsigned int *)result)[0];
        out32[1] = ((unsigned int *)result)[1];
        out32[2] = ((unsigned int *)result)[2];
        /* unpack tangent from in[20] into buf_24 */
        result = FUN_0017ffc0(buf_24, in32[1]);
        out32[3] = ((unsigned int *)result)[0];
        out32[4] = ((unsigned int *)result)[1];
        out32[5] = ((unsigned int *)result)[2];
        /* texcoord u: (s16*2+1)*(1/65535) from in[24] */
        temp_int = (int)*(signed short *)((int)in32 + 8);
        *(float *)(out32 + 6) =
          ((float)temp_int + (float)temp_int + 1.0f) * (1.0f / 65535.0f);
        /* texcoord v: (s16*2+1)*(1/65535) from in[26] */
        temp_int = (int)*(signed short *)((int)in32 + 10);
        *(float *)(out32 + 7) =
          ((float)temp_int + (float)temp_int + 1.0f) * (1.0f / 65535.0f);
        /* node_index[0] = in[28] / 3 (assert in[28] % 3 == 0) */
        in8 = (unsigned char *)in32;
        node0_byte = in8[12];
        node1_byte = in8[13];
        if ((unsigned int)node0_byte % 3 != 0) {
          display_assert("src->nodes[0]%3==0",
                         "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c",
                         0x155, 1);
          system_exit(-1);
        }
        if ((unsigned int)node1_byte % 3 != 0) {
          display_assert("src->nodes[1]%3==0",
                         "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c",
                         0x156, 1);
          system_exit(-1);
        }
        /* node_index stored as short = byte / 3 */
        *(short *)(out32 + 8) = (short)((int)node0_byte / 3);
        *((short *)(out32 + 8) + 1) = (short)((int)node1_byte / 3);
        /* weight0 = in[30] * (1/255.0f); weight1 = 1.0f - weight0 */
        weight0 = (float)(int)in8[14] * (1.0f / 255.0f);
        *(float *)(out32 + 9) = weight0;
        *(float *)(out32 + 10) = 1.0f - weight0;
        /* advance: out += 68 bytes = 17 dwords, in += 32 bytes = 8 dwords */
        out32 += 17;
        in32 += 8;
      }
    }
  }
}

/* rasterizer_geometry_vertex_get_position: copy 3-float position from vertex
 * to output (0x180500) */
void FUN_00180500(float *param_1, float *param_2)
{
  if (param_1 == 0) {
    display_assert("vertex",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x1b6,
                   1);
    system_exit(-1);
  }
  if (param_2 == 0) {
    display_assert(
      "point", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x1b7, 1);
    system_exit(-1);
  }
  param_2[0] = param_1[0];
  param_2[1] = param_1[1];
  param_2[2] = param_1[2];
}

/* rasterizer_geometry_vertex_get_normal: unpack normal from compressed vertex
 * +0xc (0x180570) */
void FUN_00180570(int param_1, float *param_2)
{
  float local_out[3];
  float *result;
  if (param_1 == 0) {
    display_assert("vertex",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x1c2,
                   1);
    system_exit(-1);
  }
  if (param_2 == 0) {
    display_assert("normal",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x1c3,
                   1);
    system_exit(-1);
  }
  result = FUN_0017ffc0(local_out, *(unsigned int *)(param_1 + 0x0c));
  param_2[0] = result[0];
  param_2[1] = result[1];
  param_2[2] = result[2];
}

/* rasterizer_geometry_vertex_get_texcoord: copy 2-float texcoord from
 * compressed vertex to output (0x1805f0) */
void FUN_001805f0(int param_1, float *param_2)
{
  if (param_1 == 0) {
    display_assert("vertex",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x1ce,
                   1);
    system_exit(-1);
  }
  if (param_2 == 0) {
    display_assert("texcoord",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x1cf,
                   1);
    system_exit(-1);
  }
  param_2[0] = *(float *)(param_1 + 0x18);
  param_2[1] = *(float *)(param_1 + 0x1c);
}

/* rasterizer_geometry_vertex_get_normal_packed: unpack normal from packed value
 * ptr (0x180660) */
void FUN_00180660(unsigned int *param_1, float *param_2)
{
  float local_out[3];
  float *result;
  if (param_1 == 0) {
    display_assert("vertex",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x1da,
                   1);
    system_exit(-1);
  }
  if (param_2 == 0) {
    display_assert("normal",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x1db,
                   1);
    system_exit(-1);
  }
  result = FUN_0017ffc0(local_out, *param_1);
  param_2[0] = result[0];
  param_2[1] = result[1];
  param_2[2] = result[2];
}

/* rasterizer_geometry_vertex_get_texcoord_short: decode compressed short
 * texcoords from vertex to float[2] output (0x1806e0) */
void FUN_001806e0(int param_1, float *param_2)
{
  if (param_1 == 0) {
    display_assert("vertex",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x1e6,
                   1);
    system_exit(-1);
  }
  if (param_2 == 0) {
    display_assert("texcoord",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x1e7,
                   1);
    system_exit(-1);
  }
  *param_2 = ((float)(int)*(short *)(param_1 + 4) +
              (float)(int)*(short *)(param_1 + 4) + *(float *)0x2533c8) *
             *(float *)0x2647f4;
  param_2[1] = ((float)(int)*(short *)(param_1 + 6) +
                (float)(int)*(short *)(param_1 + 6) + *(float *)0x2533c8) *
               *(float *)0x2647f4;
}

/* rasterizer_geometry_z_to_uint8: assert z in [0.0, 1.0] then quantize to an
 * 8-bit channel via scale 255.0. Two separate x87 compares in the original
 * (FCOMP against 0.0 then 1.0, both reloading [EBP+8]) implementing the
 * literal source predicate "z>=0.0f && z<=1.0f". The multiply result is
 * stored back through the parameter slot (FSTP [EBP+8]) and reloaded before
 * FISTP, so the scaling is written onto the parameter itself. The original
 * was built with /QIfist, so the conversion is an inline FISTP using the
 * current rounding mode (round-to-nearest) — NOT C truncation, which is off
 * by one for every fractional part >= 0.5 (measured: 7/100 equivalence seeds
 * diverged with a plain (int) cast, e.g. alpha=0.5 -> 128 vs 127).
 * x87_round_to_int keeps the original rounding. Return is the low byte of the
 * 32-bit conversion result (MOV AL). (0x180770) */
unsigned char FUN_00180770(float alpha)
{
  int quantized;
  if (!(alpha >= 0.0f && alpha <= 1.0f)) {
    display_assert("z>=0.0f && z<=1.0f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x2a,
                   1);
    system_exit(-1);
  }
  alpha = alpha * 255.0f;
  quantized = x87_round_to_int(alpha);
  return (unsigned char)quantized;
}

extern double floor(double);

/* rasterizer_geometry_float_to_uint8: clamp float [0,1] to byte via scale
 * 255.0. FISTP round-to-nearest in original; C cast truncates — structural
 * rounding delta at midpoints. (0x1807d0) */
unsigned char FUN_001807d0(float param_1)
{
  float clamped;
  if (param_1 < 0.0f) {
    clamped = 0.0f;
  } else if (param_1 > 1.0f) {
    clamped = 1.0f;
  } else {
    clamped = param_1;
  }
  return (unsigned char)(int)(clamped * *(float *)0x2602c8);
}

/*
 * compress_real_to_int16: compress a normalized real in [-1.0, 1.0] to a
 * signed 16-bit fixed-point value (0x180820).
 *
 * Asserts the input range, then returns (short)floor(z * 32767.5f).
 *
 * Binary shape preserved:
 *   - Range guard is two FCOMP/FNSTSW tests: TEST AH,0x01/JNZ (z < -1.0f) and
 *     TEST AH,0x41/JNP (z <= 1.0f). Assert tail is display_assert + system_exit
 *     (-1), NOT halt_and_catch_fire.
 *   - The floor() double result is narrowed back to float through the incoming
 *     parameter slot [EBP+8] before the integer conversion (FSTP dword [EBP+8];
 *     FLD [EBP+8]; FISTP dword [EBP-4]). Assigning back to `z` reproduces that
 *     round-trip; folding it into one expression would skip the narrowing.
 *   - The int conversion is an inline FISTP, not a _ftol call, so
 *     x87_round_to_int is used rather than a C (int) cast. Only the low word of
 *     the 4-byte result is read (MOV AX, word [EBP-4]).
 *
 * Assert __FILE__ is rasterizer_geometry.c:55 — inlined from there, but the
 * delinked object is rasterizer_text.obj.
 */
short compress_real_to_int16(float z)
{
  if (z < -1.0f || z > 1.0f) {
    display_assert("z>=-1.0f && z<=1.0f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x37,
                   1);
    system_exit(-1);
  }
  z = (float)floor((double)(z * 32767.5f));
  return (short)x87_round_to_int(z);
}

/*
 * FUN_00180890: clamping variant of compress_real_to_int16 (0x180890).
 *
 * Same compression as 0x180820 (scale by 32767.5f, floor, narrow to int16),
 * but the out-of-range guard is a silent clamp instead of an assert. The
 * relationship mirrors 0x180770 (assert) / 0x1807d0 (clamp) for the byte
 * quantizers.
 *
 * Binary shape preserved:
 *   - Clamp is two FCOMP/FNSTSW tests against the constant pool:
 *     FCOMP [0x255e94] (-1.0f); TEST AH,0x5; JP  -> below-min path loads -1.0f
 *     FCOMP [0x2533c8] (+1.0f); TEST AH,0x41; JNZ -> in-range path loads param,
 *     fall-through loads +1.0f. i.e. f < -1 -> -1, f > 1 -> +1, else f.
 *   - The floor() double result is narrowed back through a float store before
 *     the integer conversion (FSTP dword [EBP+8]; FLD [EBP+8]; FISTP dword
 *     [EBP-4]); the assignment back to a float reproduces that round-trip.
 *   - The int conversion is an inline FISTP (current rounding mode), not a
 *     _ftol call, so x87_round_to_int is used rather than a C (int) cast. The
 *     value is already integral after floor, so the two agree, but the emitted
 *     shape only matches with the FISTP helper.
 *   - Only the low word of the 4-byte conversion result is read (MOV AX).
 *
 * Constant pool values verified from the XBE: 0x255e94 = -1.0f,
 * 0x2533c8 = +1.0f, 0x2b00b4 = 32767.5f.
 */
short FUN_00180890(float f)
{
  float clamped;
  if (f < -1.0f) {
    clamped = -1.0f;
  } else if (f > 1.0f) {
    clamped = 1.0f;
  } else {
    clamped = f;
  }
  clamped = (float)floor((double)(clamped * 32767.5f));
  return (short)x87_round_to_int(clamped);
}

/* rasterizer_geometry_pack_normal_11_11_10_validated: pack float[3] normal
 * into 11-11-10 uint, asserting components in [-1.0, 1.0]. Encodes via
 * floor(component * scale) + FISTP. Verifies round-trip via FUN_0017ffc0.
 * Structural cap: FUCOMPP-based range asserts cannot be matched exactly.
 * layout: bits[10:0]=i, bits[21:11]=j, bits[31:22]=k (10-bit). (0x1808f0) */
unsigned int FUN_001808f0(float *param_1)
{
  float decoded_i;
  float decoded_j;
  float decoded_k;
  float *decoded;
  unsigned int i_11;
  unsigned int j_11;
  unsigned int packed;
  int tmp;
  float local_buf[3];

  if (param_1 == 0) {
    display_assert("parameters",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x45,
                   1);
    system_exit(-1);
  }
  if (*param_1 < -1.0f || *param_1 > 1.0f || param_1[1] < -1.0f ||
      param_1[1] > 1.0f || param_1[2] < -1.0f || param_1[2] > 1.0f) {
    display_assert("invalid vector",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x4e,
                   1);
    system_exit(-1);
  }
  tmp = (int)floor((double)(*param_1 * *(float *)0x2b0118));
  i_11 = (unsigned int)tmp & 0x7ff;
  tmp = (int)floor((double)(param_1[1] * *(float *)0x2b0118));
  j_11 = (unsigned int)tmp & 0x7ff;
  tmp = (int)floor((double)(param_1[2] * *(float *)0x2b0114));
  packed = (((unsigned int)tmp & 0x3ff) << 11 | j_11) << 11 | i_11;

  decoded = FUN_0017ffc0(local_buf, packed);
  decoded_i = decoded[0];
  decoded_j = decoded[1];
  decoded_k = decoded[2];

  if ((float)*(double *)0x28b800 <= fabsf(decoded_i - *param_1)) {
    display_assert("fabs(v2.i - v->i)<0.01f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x5c,
                   1);
    system_exit(-1);
  }
  if ((float)*(double *)0x28b800 <= fabsf(decoded_j - param_1[1])) {
    display_assert("fabs(v2.j - v->j)<0.01f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x5d,
                   1);
    system_exit(-1);
  }
  if ((float)*(double *)0x28b800 <= fabsf(decoded_k - param_1[2])) {
    display_assert("fabs(v2.k - v->k)<0.01f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x5e,
                   1);
    system_exit(-1);
  }
  return packed;
}

/* rasterizer_geometry_pack_normal_11_11_10_clamped: clamp float[3] normal to
 * [-1.0, 1.0] then pack to 11-11-10 uint. Same encoding as FUN_001808f0 but
 * silently clamps out-of-range values. Verifies round-trip via FUN_0017ffc0.
 * layout: bits[10:0]=i, bits[21:11]=j, bits[31:22]=k (10-bit). (0x180b10) */
unsigned int FUN_00180b10(float *param_1)
{
  float ci;
  float cj;
  float ck;
  float *decoded;
  float decoded_i;
  float decoded_j;
  float decoded_k;
  unsigned int i_11;
  unsigned int j_11;
  unsigned int packed;
  int tmp;
  float local_buf[3];

  if (param_1 == 0) {
    display_assert("parameters",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x68,
                   1);
    system_exit(-1);
  }
  if (*param_1 < -1.0f) {
    ci = -1.0f;
  } else if (*param_1 > 1.0f) {
    ci = 1.0f;
  } else {
    ci = *param_1;
  }
  tmp = (int)floor((double)(ci * *(float *)0x2b0118));
  i_11 = (unsigned int)tmp & 0x7ff;

  if (param_1[1] < -1.0f) {
    cj = -1.0f;
  } else if (param_1[1] > 1.0f) {
    cj = 1.0f;
  } else {
    cj = param_1[1];
  }
  tmp = (int)floor((double)(cj * *(float *)0x2b0118));
  j_11 = (unsigned int)tmp & 0x7ff;

  if (param_1[2] < -1.0f) {
    ck = -1.0f;
  } else if (param_1[2] > 1.0f) {
    ck = 1.0f;
  } else {
    ck = param_1[2];
  }
  tmp = (int)floor((double)(ck * *(float *)0x2b0114));
  packed = (((unsigned int)tmp & 0x3ff) << 11 | j_11) << 11 | i_11;

  decoded = FUN_0017ffc0(local_buf, packed);
  decoded_i = decoded[0];
  decoded_j = decoded[1];
  decoded_k = decoded[2];

  if ((float)*(double *)0x28b800 <= fabsf(decoded_i - *param_1)) {
    display_assert("fabs(v2.i - v->i)<0.01f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x76,
                   1);
    system_exit(-1);
  }
  if ((float)*(double *)0x28b800 <= fabsf(decoded_j - param_1[1])) {
    display_assert("fabs(v2.j - v->j)<0.01f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x77,
                   1);
    system_exit(-1);
  }
  if ((float)*(double *)0x28b800 <= fabsf(decoded_k - param_1[2])) {
    display_assert("fabs(v2.k - v->k)<0.01f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x78,
                   1);
    system_exit(-1);
  }
  return packed;
}

/* rasterizer_geometry_vertex_compress: compress vertex buffer (0x180d10)
 * ported=false: structural cap, too complex for reliable VC71 match */
void FUN_00180d10(short param_1, int param_2, int param_3, int param_4,
                  void *param_5, int param_6)
{
  (void)param_1;
  (void)param_2;
  (void)param_3;
  (void)param_4;
  (void)param_5;
  (void)param_6;
}

/* rasterizer_lights.c */

/* Address of a slot in this frame's lens flare queue (0x181020).
 *
 * The queue base is 0x4c6480 and the stride is 0x28 (40 bytes); 0x4d0480 is
 * local_lens_flare_count, the number of slots filled so far this frame. Both
 * globals and the stride are the same ones FUN_00181180 below walks.
 *
 * The index arrives in SI (kb.json: @<si>) — the original has no frame at all
 * (0x181020 opens with TEST SI,SI) and never reads the stack. It is
 * SIGN-extended (MOVSX EAX,SI at 0x18102b) before the count compare, and the
 * negative test is a separate TEST SI,SI / JL that runs BEFORE the count is
 * loaded, so the two bounds tests must stay short-circuited in that order.
 *
 * TU is rasterizer_lights.c (proven by the __FILE__ assert string at 0x2b01b4,
 * referenced at 0x181036); it lives here only because of the kb.json object
 * grouping, like FUN_00181150/FUN_00181180 below.
 *
 * kb.json declares the return as int *; callers cast it to the 0x28-byte
 * submission struct (see FUN_00181670 in rasterizer_lights.c). Assert tail is
 * display_assert + system_exit(-1) (PUSH -1; CALL 0x8e2f0 at 0x181047), NOT
 * halt_and_catch_fire as Ghidra renders it. */
int *FUN_00181020(short lens_flare_index)
{
  if (lens_flare_index < 0 || lens_flare_index >= *(int *)0x4d0480) {
    display_assert(
      "lens_flare_index>=0 && lens_flare_index<local_lens_flare_count",
      "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c", 0x43, 1);
    system_exit(-1);
  }
  return (int *)((char *)0x4c6480 + lens_flare_index * 0x28);
}

/* rasterizer_lights_initialize: clear lights buffers and counter (0x181150) */
void FUN_00181150(void)
{
  csmemset((void *)0x4bed80, 0, 0x7722);
  csmemset((void *)0x47ed60, 0, 0x40020);
  *(int *)0x4d0480 = 0;
}

/* rasterizer_lights_update_lens_flare_alphas: end-of-frame pass over the queued
 * lens flares. For each flare the occlusion query result (FUN_0017d040) is
 * converted to an 8-bit alpha ratio against the flare's sample count
 * (element+0x24), then blended into the persistent alpha byte returned by
 * FUN_00181060. The queue is emptied afterwards. (0x181180)
 *
 * TU is rasterizer_lights.c (proven by the __FILE__ assert string at 0x1811e9);
 * it lives in rasterizer_text.c only because of the kb.json object grouping.
 *
 * Globals (from disassembly):
 *   0x3256d7  char   lens flare subsystem enabled flag
 *   0x46e008  int16  render pass mode (CMP word ptr)
 *   0x31fa98  int16  secondary mode counter (CMP word ptr)
 *   0x4d0480  int    local_lens_flare_count
 *   0x4c6480  base of the lens flare queue, stride 0x28 (40 bytes)
 *
 * Assert tail is display_assert + system_exit(-1) (PUSH -1; CALL 0x8e2f0 at
 * 0x1811ee), NOT halt_and_catch_fire as Ghidra renders it.
 *
 * Blend direction derived from the disassembly (CL = new alpha, AL = old):
 *   new > old : *p = (old * 3 + new) / 4   (CDQ; AND EDX,3; ADD; SAR 2)
 *   new < old : *p = (old + new) / 2       (CDQ; SUB EAX,EDX; SAR 1)
 *   new == old: no store at all
 */
void FUN_00181180(void)
{
  short i; /* 16-bit loop counter (ESI after MOVSX) */
  int idx; /* [EBP-4] 32-bit widened counter */
  char *elem; /* &DAT_004c6480 + idx * 0x28 (EDI) */
  unsigned char *alpha_ptr; /* return of FUN_00181060 */
  /* elem[+0x24] sample count is re-read inline, never cached */
  int quotient; /* (occlusion * 255 + denom/2) / denom, signed IDIV */
  unsigned char old_alpha; /* *alpha_ptr before the blend (AL) */
  unsigned char new_alpha; /* clamped alpha ratio (CL) */

  FUN_0016f910(0x18);

  if (*(char *)0x3256d7 != 0 && *(short *)0x46e008 <= 1 &&
      (*(short *)0x46e008 != 1 || *(short *)0x31fa98 <= 1)) {
    idx = 0;
    i = 0;
    if (*(int *)0x4d0480 > 0) {
      do {
        if (i < 0 || idx >= *(int *)0x4d0480) {
          display_assert(
            "lens_flare_index>=0 && lens_flare_index<local_lens_flare_count",
            "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c", 0x43, 1);
          system_exit(-1);
        }

        elem = (char *)0x4c6480 + idx * 0x28;
        alpha_ptr = FUN_00181060((void *)elem);

        if (*(int *)(elem + 0x24) <= 0) {
          *alpha_ptr = 0;
        } else {
          /* signed rounding division: (occluded * 255 + count/2) / count.
           * The count field is re-read rather than cached, matching the two
           * separate `mov 0x24(%edi)` loads in the original. */
          quotient = (FUN_0017d040(idx) * 0xff + (*(int *)(elem + 0x24) >> 1)) /
                     *(int *)(elem + 0x24);
          if (quotient < 0xff) {
            new_alpha = (unsigned char)quotient;
          } else {
            new_alpha = 0xff;
          }

          if (new_alpha == 0) {
            *alpha_ptr = 0;
          } else {
            old_alpha = *alpha_ptr;
            /* CMP CL,AL with CL = new_alpha, AL = old_alpha */
            if (new_alpha > old_alpha) {
              *alpha_ptr = (unsigned char)((old_alpha * 3 + new_alpha) / 4);
            } else if (new_alpha < old_alpha) {
              *alpha_ptr = (unsigned char)((old_alpha + new_alpha) / 2);
            }
          }
        }

        i = (short)(i + 1);
        idx = (int)i;
      } while (idx < *(int *)0x4d0480);
    }
    *(int *)0x4d0480 = 0;
  }

  FUN_0016fa40(0x18);
}

/* rasterizer_lights_reset_stat: zero stat counter at 0x5a37e0 (0x1812b0) */
void FUN_001812b0(void)
{
  *(int *)0x5a37e0 = 0;
}

/* rasterizer_lights_submit: append one light to the per-window light array and
 * return its index, or -1 when the array is full (0x1812c0).
 *
 * TU is rasterizer_lights.c (proven by the __FILE__ assert string); it lives in
 * rasterizer_text.c only because that is the kb.json object grouping.
 *
 * Globals (from disassembly):
 *   0x5a37e0  int    light count (capacity 0x80)
 *   0x5a37e4  base of the light array, stride 0x38 (56 bytes)
 *   0x3256ba  int16  render mode flag; secondary counter ticks when == 2
 *   0x5a5548  int    secondary counter
 *   0x2533c0  0.0f   0x2533c8  1.0f
 *
 * The 56-byte element copy is a whole-struct assignment (REP MOVSD of 0xE
 * dwords in the original). Only color.red/green/blue at +0x28/+0x2c/+0x30 are
 * identified; the rest of the element is opaque here. */
int rasterizer_lights_submit(void *parameters)
{
  struct rasterizer_light_element {
    int data[14]; /* 0x38 bytes */
  };
  int light_index;
  int count;

  light_index = -1;
  if (parameters == 0) {
    display_assert("parameters",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c", 0xf0,
                   1);
    system_exit(-1);
  }
  if (!(*(float *)((char *)parameters + 0x28) >= *(float *)0x2533c0 &&
        *(float *)((char *)parameters + 0x28) <= *(float *)0x2533c8)) {
    display_assert("parameters->color.red >=0.0f && parameters->color.red "
                   "<=1.0f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c", 0xf1,
                   1);
    system_exit(-1);
  }
  if (!(*(float *)((char *)parameters + 0x2c) >= *(float *)0x2533c0 &&
        *(float *)((char *)parameters + 0x2c) <= *(float *)0x2533c8)) {
    display_assert("parameters->color.green>=0.0f && "
                   "parameters->color.green<=1.0f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c", 0xf2,
                   1);
    system_exit(-1);
  }
  if (!(*(float *)((char *)parameters + 0x30) >= *(float *)0x2533c0 &&
        *(float *)((char *)parameters + 0x30) <= *(float *)0x2533c8)) {
    display_assert("parameters->color.blue >=0.0f && "
                   "parameters->color.blue <=1.0f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c", 0xf3,
                   1);
    system_exit(-1);
  }

  count = *(int *)0x5a37e0;
  if (count < 0x80) {
    light_index = count;
    count = count + 1;
    *(int *)0x5a37e0 = count;
    *(struct rasterizer_light_element *)(0x5a37e4 + light_index * 0x38) =
      *(struct rasterizer_light_element *)parameters;
    if (*(short *)0x3256ba == 2) {
      *(int *)0x5a5548 = *(int *)0x5a5548 + 1;
    }
  } else {
    error(2, "### ERROR too many lights submitted to window");
  }
  return light_index;
}

/* FUN_00181410: stub (0x181410) */
void FUN_00181410(void)
{
}

/* lens_flare_scenery_queue: queue lens flares from scenario scenery lights for
 * rendering. Iterates light-marker block entries for param_1 scenery_light
 * index. Builds a 0x28-byte params struct and calls FUN_00181670 (the lens
 * flare queue submission function) for each entry. (0x181900) */
void FUN_00181900(short param_1)
{
  int scenario; /* scenario base ptr */
  int light_block; /* scenario->scenery_lights[param_1] element ptr */
  int lf_block_base; /* scenario->lens_flare_block ptr */
  int lf_mark_base; /* scenario->lens_flare_marker_block ptr */
  int entry; /* current light_marker_block entry ptr */
  int lf_instance; /* lens_flare_instance element ptr */
  int loop_end; /* count of light_marker entries */
  int i; /* loop counter */
  /* params struct for FUN_00181670: 0x28-byte contiguous buffer.
   * Layout (confirmed from disassembly at 0x181a2c..0x181a67):
   *   +0x00: tag_get('lens', def->tag_index) result
   *   +0x04: entry->xyz[0] (float, from puVar2[0..2])
   *   +0x08: entry->xyz[1]
   *   +0x0c: entry->xyz[2]
   *   +0x10: FUN_00180b10(&dir_vec) = compressed normal of direction
   *   +0x14: FUN_00180b10(&perp_vec) = compressed normal of perpendicular
   *   +0x18: 0xffffffff (color/alpha = -1)
   *   +0x1c: 0xffff word (light_index = -1 -> scenery path)
   *   +0x1e: entry_index >> 16 (hi word of scenery marker index)
   *   +0x20: entry_index & 0xffff (lo word)
   *   +0x22: DAT_0050654a (compressed window index byte)
   *   +0x23: 0
   */
  int params[10]; /* 0x28 = 40 bytes = 10 ints; accessed as byte/short/int */
  float dir[3]; /* direction vector (from entry bytes 0xc/0xd/0xe * scale) */
  float perp[3]; /* perpendicular vector (from perpendicular3d of dir) */
  int entry_idx; /* combined scenery marker index (hi<<16 | lo) */

  if (*(char *)0x3256d7 == 0) {
    return;
  }
  if (*(short *)0x46e008 > 1) {
    return;
  }
  if (*(short *)0x46e008 == 1 && *(short *)0x31fa98 > 1) {
    return;
  }

  scenario = (int)scenario_get();
  /* tag_block_get_element(scenario+0x134, param_1, 0x68) */
  light_block =
    (int)tag_block_get_element((void *)(scenario + 0x134), (int)param_1, 0x68);
  loop_end = (int)*(short *)(light_block + 0x42);
  if (loop_end <= 0) {
    return;
  }

  lf_mark_base = scenario + 0x128;
  lf_block_base = scenario + 0x11c;

  i = 0;
  do {
    /* light_marker_block entry */
    entry_idx = (int)*(unsigned short *)(light_block + 0x40) + i;
    entry = (int)tag_block_get_element((void *)lf_mark_base, entry_idx, 0x10);

    /* lens_flare_instance element */
    lf_instance = (int)tag_block_get_element(
      (void *)lf_block_base, (int)*(unsigned char *)(entry + 0xf), 0x10);

    /* Extract signed bytes from entry for direction vector */
    dir[0] = (float)(int)*(signed char *)(entry + 0xc) * *(float *)0x2820c0;
    dir[1] = (float)(int)*(signed char *)(entry + 0xd) * *(float *)0x2820c0;
    dir[2] = (float)(int)*(signed char *)(entry + 0xe) * *(float *)0x2820c0;

    /* Compute perpendicular and normalize both */
    perpendicular3d(dir, perp);
    normalize3d(dir);
    normalize3d(perp);

    /* Build params buffer (byte-level stores into int array). */
    *(int *)((char *)params + 0x00) =
      (int)tag_get(0x6c656e73, *(int *)(lf_instance + 0xc));
    *(int *)((char *)params + 0x04) = *(int *)(entry + 0x00);
    *(int *)((char *)params + 0x08) = *(int *)(entry + 0x04);
    *(int *)((char *)params + 0x0c) = *(int *)(entry + 0x08);
    *(unsigned int *)((char *)params + 0x10) = (unsigned int)FUN_00180b10(dir);
    *(unsigned int *)((char *)params + 0x14) = (unsigned int)FUN_00180b10(perp);
    *(int *)((char *)params + 0x18) = -1;
    *(short *)((char *)params + 0x1c) = -1;
    *(short *)((char *)params + 0x1e) = (short)(entry_idx >> 16);
    *(short *)((char *)params + 0x20) = (short)entry_idx;
    *(unsigned char *)((char *)params + 0x22) = *(unsigned char *)0x50654a;
    *(unsigned char *)((char *)params + 0x23) = 0;

    FUN_00181670(params);

    i++;
  } while (i < loop_end);
}

/* lens_flare_occlusion_submit: for each queued lens flare entry, compute the
 * occlusion test position and submit via FUN_0017d030. Wrapped by
 * FUN_0016f910/FUN_0016fa40 rasterizer widget begin/end. (0x181a90) */
void FUN_00181a90(void)
{
  int *entry; /* pointer to queued lens flare slot (from FUN_00181020) */
  int definition; /* *entry = definition tag ptr */
  float *dir_result; /* return of FUN_0017ffc0 (3-float direction vec) */
  short occlusion_dir; /* *(short *)(definition + 0x14) */
  int vis_param; /* *(int *)(definition + 0x10) as int (passes to thunk) */
  int lf_count; /* DAT_004d0480 */
  int i; /* loop index */
  float perp[3]; /* perpendicular output (12 bytes, EBP-0x2c) */
  float dir[3]; /* direction vec copied from FUN_0017ffc0 result */
  float pos[3]; /* output position vec for occlusion test (EBP-0x14) */

  FUN_0016f910(0x17);

  if (*(char *)0x3256d7 == 0) {
    FUN_0016fa40(0x17);
    return;
  }
  if (*(short *)0x46e008 > 1) {
    FUN_0016fa40(0x17);
    return;
  }
  if (*(short *)0x46e008 == 1 && *(short *)0x31fa98 > 1) {
    FUN_0016fa40(0x17);
    return;
  }

  if (*(short *)0x5a5bc0 != 0) {
    FUN_0016fa40(0x17);
    return;
  }

  lf_count = *(int *)0x4d0480;
  if (lf_count <= 0) {
    FUN_0016fa40(0x17);
    return;
  }

  FUN_0017cfc0(6, 1);

  lf_count = *(int *)0x4d0480;
  if (lf_count > 0) {
    i = 0;
    do {
      /* FUN_00181020 takes index via SI register; build system provides
       * a thunk that loads the arg into SI before the call. */
      entry = FUN_00181020((short)i);
      definition = *entry;

      /* FUN_0017ffc0(&perp, entry[4]) fills perp[] and returns a
       * pointer to a 3-float direction vec; copy it into dir[]. */
      dir_result = FUN_0017ffc0(perp, (unsigned int)entry[4]);
      dir[0] = dir_result[0];
      dir[1] = dir_result[1];
      dir[2] = dir_result[2];

      /* MOVZX byte [entry+0x22]; AND 0xffffff7f (clear bit 7) → compare
       * with window index */
      if ((*(unsigned char *)((char *)entry + 0x22) & 0x7f) ==
          *(unsigned short *)0x5a5bc2) {
        occlusion_dir = *(short *)(definition + 0x14);
        vis_param = *(int *)(definition + 0x10);

        if (occlusion_dir == 0) {
          /* Negate scale; use global forward direction (0x5a5bd4) */
          vector3d_scale_add((float *)(entry + 1), (float *)0x5a5bd4,
                             -*(float *)(definition + 0x10), pos);
        } else if (occlusion_dir == 1) {
          /* Scale along dir[] by definition field * constant */
          vector3d_scale_add((float *)(entry + 1), dir,
                             *(float *)(definition + 0x10) * *(float *)0x254e68,
                             pos);
        } else if (occlusion_dir == 2) {
          /* Use object/light position directly */
          pos[0] = *(float *)(entry + 1);
          pos[1] = *(float *)(entry + 2);
          pos[2] = *(float *)(entry + 3);
        } else {
          display_assert(
            "### ERROR unsupported lens flare occlusion offset direction",
            "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c", 0x1e2, 1);
          system_exit(-1);
        }

        entry[9] = FUN_0017d030(pos, vis_param, i);
      }

      i++;
    } while (i < *(int *)0x4d0480);
  }

  /* FUN_0017d020 (thunk → FUN_0017ad90) is called after the loop whenever
   * the first lf_count check passed (i.e. when lf_count > 0), matching
   * the original control-flow shape (0x181bfd falls through to 0x181c02
   * regardless of the inner lf_count re-check). */
  FUN_0017d020();

  FUN_0016fa40(0x17);
}

/* rasterizer_lights_draw_lens_flares: render all queued lens flare reflections
 * for the current frame. Iterates the lens flare queue (DAT_004c6480,
 * count = DAT_004d0480), computes per-flare brightness/rotation/position,
 * then for each reflection element calls the widget draw path
 * (FUN_0017d010 / FUN_0017b7d0). A second pass renders sun-glow overlays
 * (DAT_003256fe guard). (0x181c20) */
void FUN_00181c20(void)
{
  /* atan2 from libm (used for flare screen-angle computation) */
  extern double atan2(double, double);

  /* Outer loop state */
  int lf_count; /* DAT_004d0480 */
  int i; /* outer loop counter (ESI, sign-extended as CX) */
  int outer_ctr; /* [EBP-0x78] inner loop index within outer */
  int *entry; /* lens flare queue entry: &DAT_004c6480 + i*0x28 (EBX) */
  unsigned char *light_data; /* return of FUN_00181060 (ESI after call) */
  float *dir_ptr; /* FUN_0017ffc0 return (3-float decoded direction) */
  int definition; /* entry[0] = tag definition ptr (EDI) */

  /* Relative position of flare to camera */
  float delta[3]; /* [EBP-0x18/-0x14/-0x10] entry - camera (contiguous array for
                     normalize3d) */
  float view_dot; /* [EBP-0x1c] dot(fwd, delta) */

  /* Reflection billboard offset */
  float refl_off_x; /* [EBP-0x48] */
  float refl_off_y; /* [EBP-0x44] */
  float refl_off_z; /* [EBP-0x40] */

  /* Decoded perpendicular direction of the flare (from FUN_0017ffc0) */
  float dir_local[3]; /* [EBP-0xb4] buffer passed to FUN_0017ffc0 (12 bytes) */
  float dir_x; /* [EBP-0x68] copy of dir_ptr[0] = local_6c */
  float dir_y; /* [EBP-0x64] copy of dir_ptr[1] = local_68 */
  float dir_z; /* [EBP-0x60] copy of dir_ptr[2] = local_64 */

  /* Per-flare scalar values */
  float brightness_byte; /* [EBP-0x28] *light_data * scale = local_2c */
  float entry_x; /* [EBP-0x74] entry[1] (float) */
  float entry_y; /* [EBP-0x70] entry[2] */
  float entry_z; /* [EBP-0x6c] entry[3] */

  /* Occlusion/brightness accumulator */
  float brightness; /* [EBP-0xc] accumulated brightness = local_10 */
  float depth_scale; /* [EBP-0x4] = local_8 */
  float depth_bias; /* [EBP-0x8] = local_c */

  /* Corona rotation */
  float
    corona_rot; /* [EBP-0x9c] = local_9c, output of FUN_00181420 * def[0x84] */
  float flare_angle; /* [EBP-0xa0] = local_a0, fpatan result * scale */

  /* Visibility array [5]: [0]=1.0, [1]=near_clip, [2]=direction, [3]=backward,
   * [4]=rotation_fn */
  float vis[5]; /* [EBP-0x8c] = local_90 */

  /* Second loop (sun glow) */
  short sun_i; /* [sVar13] second loop counter */
  int sun_entry; /* pointer into lens flare queue for sun glow */

  /* Inner reflection loop */
  int refl_idx; /* [EBP-0x2c] reflection loop counter = local_30 */
  int refl_count; /* *(int *)(definition + 0xc4) */
  void *refl; /* tag_block_get_element result = puVar8 */
  int refl_ivar; /* iVar7 inner loop counter */

  /* Reflection color */
  unsigned int color; /* packed ARGB for FUN_0017d010 (uVar11) */
  unsigned int tex_flags; /* [EBP-0x5c] local_60 */
  float anim_alpha; /* [EBP-0x58] local_5c */
  float anim_r; /* [EBP-0x54] local_58 */
  float anim_g; /* [EBP-0x50] local_54 */
  float anim_b; /* [EBP-0x4c] local_50 */

  /* Animation color: alpha from scalars_interpolate, RGB[3] from FUN_0007c270.
   * In MSVC layout: anim_alpha_out at EBP-0x3c (local_40),
   * anim_rgb[0..2] at EBP-0x38/0x34/0x30 (local_3c/38/34). */
  float anim_alpha_out; /* [EBP-0x3c] = local_40, from scalars_interpolate */
  float anim_rgb[3]; /* [EBP-0x38..0x30] = local_3c/38/34, from FUN_0007c270 */

  /* Reflection size and position output */
  float refl_size; /* current reflection size (local_8 reused = local_c in
                      Ghidra) */
  float refl_anim; /* animation period result (local_8 reused again) */
  float flare_size_angle; /* [EBP-0x24] = local_28, rotation + offset */
  float pos[3]; /* [EBP-0xa8..0xa0]: billboard position x/y/z */
  float scale2d[2]; /* [EBP-0x94] = local_98/local_94 */
  unsigned short refl_flags; /* *puVar8 */
  unsigned int stencil_mode; /* uVar15 */
  char occlusion_result; /* cVar4 return of FUN_0017cfd0 */
  int iVar1; /* iVar1 = i * 0x28 */

  FUN_0016f910(0x19);

  if (*(char *)0x3256d7 == '\0' || *(short *)0x5a5bc0 != 0 ||
      *(int *)0x4d0480 <= 0) {
    FUN_0016fa40(0x19);
    return;
  }

  FUN_0017cfc0(5, 0);

  lf_count = *(int *)0x4d0480;
  outer_ctr = 0;
  if (lf_count > 0) {
    i = 0;
    do {
      /* Bounds check */
      if ((short)outer_ctr < 0 || i >= lf_count) {
        display_assert(
          "lens_flare_index>=0 && lens_flare_index<local_lens_flare_count",
          "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c", 0x43, 1);
        system_exit(-1);
      }

      iVar1 = i * 0x28;
      /* entry = &DAT_004c6480 + i*0x28 (5*8 = 0x28 bytes per entry) */
      entry = (int *)((char *)0x4c6480 + iVar1);

      /* FUN_00181060 takes @eax = entry (lens_flare_params ptr).
       * Returns pointer to light color/alpha byte in the light table. */
      light_data = FUN_00181060((void *)entry);

      /* FUN_0017ffc0 decodes packed normal entry[4] into dir_local[3] */
      dir_ptr = FUN_0017ffc0(dir_local, (unsigned int)entry[4]);
      dir_x = dir_ptr[0];
      dir_y = dir_ptr[1];
      dir_z = dir_ptr[2];

      /* Filter by window index: byte[entry+0x22] & 0x7f == DAT_005a5bc2 */
      if (((*(unsigned char *)((char *)entry + 0x22) & 0x7f) ==
           *(unsigned short *)0x5a5bc2) &&
          (*(int *)((char *)entry + 0x24) > 0) &&
          (*(unsigned char *)((char *)entry + 0x1b) != 0) &&
          (*(int *)(entry[0] + 0xc4) > 0)) {
        definition = entry[0];
        entry_x = *(float *)((char *)entry + 4);
        entry_y = *(float *)((char *)entry + 8);
        entry_z = *(float *)((char *)entry + 0xc);

        /* Compute relative position to camera origin */
        delta[0] = entry_x - *(float *)0x5a5bc8;
        delta[1] = entry_y - *(float *)0x5a5bcc;
        delta[2] = entry_z - *(float *)0x5a5bd0;

        /* view_dot = dot(camera_fwd, delta) */
        view_dot = *(float *)0x5a5bd4 * delta[0] +
                   *(float *)0x5a5bd8 * delta[1] +
                   *(float *)0x5a5bdc * delta[2];

        /* Reflection offset: 2*(view_fwd * dot - delta) */
        refl_off_x = *(float *)0x5a5bd4 * view_dot - delta[0];
        refl_off_y = *(float *)0x5a5bd8 * view_dot - delta[1];
        refl_off_z = *(float *)0x5a5bdc * view_dot - delta[2];
        refl_off_x = refl_off_x + refl_off_x;
        refl_off_y = refl_off_y + refl_off_y;
        refl_off_z = refl_off_z + refl_off_z;

        brightness_byte = (float)*light_data * *(float *)0x261518;

        /* Near-clip brightness: clamp (view_dot - near_end) / (near_start -
         * near_end) */
        if (*(float *)(definition + 0x1c) <= *(float *)0x2533c0) {
          brightness = 1.0f;
        } else {
          brightness =
            (view_dot - *(float *)(definition + 0x1c)) /
            (*(float *)(definition + 0x18) - *(float *)(definition + 0x1c));
          if (brightness < *(float *)0x2533c0) {
            brightness = 0.0f;
          } else if (*(float *)0x2533c8 < brightness) {
            brightness = 1.0f;
          }
        }

        /* FUN_0017ff80: scale byte to float */
        brightness = brightness_byte * brightness *
                     FUN_0017ff80(*(unsigned char *)((char *)entry + 0x1b));

        /* FUN_00181420: corona rotation size.
         * Takes @esi = entry (lens_flare_params), @di =
         * *(short*)(definition+0x80). Returns float (ST0) = corona rotation
         * size. */
        corona_rot =
          FUN_00181420((void *)entry, *(short *)(definition + 0x80)) *
          *(float *)(definition + 0x84);

        /* fpatan of screen-space projection */
        flare_angle = (float)atan2(*(float *)0x5a5c6c * delta[2] +
                                     *(float *)0x5a5c68 * delta[1] +
                                     delta[0] * *(float *)0x5a5c64,
                                   *(float *)0x5a5c78 * delta[2] +
                                     *(float *)0x5a5c74 * delta[1] +
                                     delta[0] * *(float *)0x5a5c70) *
                      *(float *)0x2b073c;

        /* depth scale: 1.0 / (far - near) */
        depth_scale = *(float *)0x2533c8 / (*(float *)(definition + 8) -
                                            *(float *)(definition + 0xc));
        depth_bias = -(depth_scale * *(float *)(definition + 0xc));

        /* Normalize delta (in-place, modifies delta[0]/y/z via &delta[0]) */
        normalize3d(&delta[0]);

        /* Visibility array:
         * [0] = 1.0 (always)
         * [1] = camera-facing: clamp(depth_bias - dot(dir, camera_pos) *
         * depth_scale) [2] = light-facing:  clamp(depth_bias - dot(dir, delta)
         * * depth_scale) [3] = backward:      clamp(dot(fwd, delta) *
         * depth_scale + depth_bias) [4] = rotation fn output (filled later if
         * brightness > 0) */
        vis[0] = 1.0f;

        {
          float v;
          v = depth_bias -
              (dir_x * *(float *)0x5a5bd4 + *(float *)0x5a5bd8 * dir_y +
               *(float *)0x5a5bdc * dir_z) *
                depth_scale;
          if (v < *(float *)0x2533c0) {
            vis[1] = 0.0f;
          } else if (*(float *)0x2533c8 < v) {
            vis[1] = 1.0f;
          } else {
            vis[1] = v;
          }
        }

        {
          float v;
          v = depth_bias -
              (dir_x * delta[0] + dir_y * delta[1] + dir_z * delta[2]) *
                depth_scale;
          if (v < *(float *)0x2533c0) {
            vis[2] = 0.0f;
          } else if (*(float *)0x2533c8 < v) {
            vis[2] = 1.0f;
          } else {
            vis[2] = v;
          }
        }

        {
          float v;
          v = (*(float *)0x5a5bdc * delta[2] + *(float *)0x5a5bd8 * delta[1] +
               delta[0] * *(float *)0x5a5bd4) *
                depth_scale +
              depth_bias;
          if (v < *(float *)0x2533c0) {
            vis[3] = 0.0f;
          } else if (*(float *)0x2533c8 < v) {
            vis[3] = 1.0f;
          } else {
            vis[3] = v;
          }
        }

        if (*(float *)0x2533c0 < brightness) {
          /* vis[4] = rotation function output for animation */
          vis[4] = FUN_0017ff80(*(unsigned char *)((char *)entry + 0x23));

          refl_idx = 0;
          refl_count = *(int *)(definition + 0xc4);
          if (refl_count > 0) {
            refl_ivar = 0;
            do {
              int saved_refl_idx;
              saved_refl_idx = refl_idx;

              /* tag_block_get_element(definition+0xc4, refl_ivar, 0x80) */
              refl = tag_block_get_element((void *)(definition + 0xc4),
                                           refl_ivar, 0x80);

              /* Compute reflection scale from animation:
               * ((max - min) * vis[4] + min) * vis[flags] * brightness */
              {
                float anim_val;
                anim_val = (*(float *)((char *)refl + 0x38) -
                            *(float *)((char *)refl + 0x34)) *
                             vis[4] +
                           *(float *)((char *)refl + 0x34);
                /* refl[0x3c] = vis-factor index (short, sign-extended) */
                anim_alpha = anim_val *
                             vis[(int)(*(short *)((char *)refl + 0x3c))] *
                             brightness;
              }

              if (refl_idx == 0) {
                brightness = anim_alpha;
              }

              if (*(float *)0x2533c0 < anim_alpha) {
                refl_size = (*(float *)((char *)refl + 0x2c) -
                             *(float *)((char *)refl + 0x28)) *
                              vis[4] +
                            *(float *)((char *)refl + 0x28);

                /* Check if all tint color components are zero */
                if (*(float *)((char *)refl + 0x40) == *(float *)0x2533c0 &&
                    *(float *)((char *)refl + 0x44) == *(float *)0x2533c0 &&
                    *(float *)((char *)refl + 0x48) == *(float *)0x2533c0 &&
                    *(float *)((char *)refl + 0x4c) == *(float *)0x2533c0) {
                  /* No tint: use alpha from light_data byte, color from entry
                   */
                  color = (unsigned int)FUN_00180770(anim_alpha) << 0x18 |
                          (*(unsigned int *)((char *)entry + 0x18) & 0xffffff);
                  tex_flags = 0x3f800000;
                } else {
                  /* Has tint: build ARGB from animation color */
                  anim_r = *(float *)((char *)refl + 0x44);
                  anim_g = *(float *)((char *)refl + 0x48);
                  anim_b = *(float *)((char *)refl + 0x4c);
                  /* anim_alpha already set above */

                  if (*(short *)((char *)refl + 0x72) > 1) {
                    /* Has animation: compute animated color */
                    if (*(float *)((char *)refl + 0x74) == *(float *)0x2533c0) {
                      display_assert(
                        "reflection->animation_period!=0.0f",
                        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c",
                        0x28b, 1);
                      system_exit(-1);
                    }
                    refl_anim = FUN_0010a5e0(
                      *(short *)((char *)refl + 0x72),
                      (*(float *)0x5a5e18 + *(float *)((char *)refl + 0x78)) /
                        *(float *)((char *)refl + 0x74));

                    /* Interpolate RGB into anim_rgb[3] (EBP-0x38..EBP-0x30):
                     * output, mode, lower, upper, t */
                    FUN_0007c270(
                      anim_rgb,
                      (unsigned int)(*(unsigned char *)((char *)refl + 0x70) &
                                     3),
                      (float *)((char *)refl + 0x54),
                      (float *)((char *)refl + 0x64), refl_anim);

                    /* Interpolate alpha into anim_alpha_out (EBP-0x3c):
                     * lower=entry[0x50], upper=entry[0x60], t, output */
                    scalars_interpolate(*(float *)((char *)refl + 0x50),
                                        *(float *)((char *)refl + 0x60),
                                        refl_anim, &anim_alpha_out);

                    /* Validate animation_color.alpha */
                    if (anim_alpha_out < *(float *)0x2533c0 ||
                        (anim_alpha_out < *(float *)0x2533c8 ==
                         (anim_alpha_out == *(float *)0x2533c8))) {
                      display_assert(
                        "animation_color.alpha>=0.0f && "
                        "animation_color.alpha<=1.0f",
                        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c",
                        0x29b, 1);
                      system_exit(-1);
                    }
                    /* Validate animation_color.red */
                    if (anim_rgb[0] < *(float *)0x2533c0 ||
                        (anim_rgb[0] < *(float *)0x2533c8 ==
                         (anim_rgb[0] == *(float *)0x2533c8))) {
                      display_assert(
                        "animation_color.red >=0.0f && animation_color.red "
                        "<=1.0f",
                        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c",
                        0x29c, 1);
                      system_exit(-1);
                    }
                    /* Validate animation_color.green */
                    if (anim_rgb[1] < *(float *)0x2533c0 ||
                        (anim_rgb[1] < *(float *)0x2533c8 ==
                         (anim_rgb[1] == *(float *)0x2533c8))) {
                      display_assert(
                        "animation_color.green>=0.0f && "
                        "animation_color.green<=1.0f",
                        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c",
                        0x29d, 1);
                      system_exit(-1);
                    }
                    /* Validate animation_color.blue */
                    if (anim_rgb[2] < *(float *)0x2533c0 ||
                        (anim_rgb[2] < *(float *)0x2533c8 ==
                         (anim_rgb[2] == *(float *)0x2533c8))) {
                      display_assert(
                        "animation_color.blue >=0.0f && animation_color.blue "
                        "<=1.0f",
                        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c",
                        0x29e, 1);
                      system_exit(-1);
                    }

                    anim_alpha = anim_alpha_out * anim_alpha;
                    anim_r = anim_r * anim_rgb[0];
                    anim_g = anim_g * anim_rgb[1];
                    anim_b = anim_b * anim_rgb[2];
                  }

                  {
                    /* Pack ARGB: FUN_000d1c90 takes float[4] = {alpha,r,g,b}
                     * at [EBP-0x58] = {anim_alpha, anim_r, anim_g, anim_b}.
                     * These four contiguous slots map to local_5c/58/54/50. */
                    float argb4[4];
                    argb4[0] = anim_alpha;
                    argb4[1] = anim_r;
                    argb4[2] = anim_g;
                    argb4[3] = anim_b;
                    color = FUN_000d1c90(argb4);
                  }
                  tex_flags = *(unsigned int *)((char *)refl + 0x40);
                }

                /* Reflection position */
                if (refl_idx == 0) {
                  flare_size_angle =
                    corona_rot + *(float *)((char *)refl + 0x20);
                  scale2d[0] = *(float *)(definition + 0xa0);
                  scale2d[1] = *(float *)(definition + 0xa4);
                } else {
                  flare_size_angle = *(float *)((char *)refl + 0x20);
                  scale2d[0] = 1.0f;
                  scale2d[1] = 1.0f;
                }

                refl_flags = *(unsigned short *)refl;

                /* Optional rotation offset */
                if (refl_flags & 1) {
                  flare_size_angle = flare_size_angle + flare_angle;
                }

                /* Optional scale by brightness */
                if (refl_flags & 4) {
                  refl_size = (brightness_byte + *(float *)0x2533c8) *
                              refl_size * *(float *)0x253398;
                }

                /* Optional scale by view dot */
                if (refl_flags & 2) {
                  refl_size = refl_size * view_dot;
                }

                /* Billboard position: entry_xyz + offset * refl_size */
                {
                  float fVar2;
                  fVar2 = *(float *)((char *)refl + 0x1c);
                  pos[0] = refl_off_x * fVar2 + entry_x;
                  pos[1] = refl_off_y * fVar2 + entry_y;
                  pos[2] = refl_off_z * fVar2 + entry_z;
                }

                /* FUN_0017cfd0: check occlusion / stencil */
                occlusion_result =
                  FUN_0017cfd0(0, *(unsigned int *)(definition + 0x2c),
                               *(unsigned short *)((char *)refl + 4));
                if (occlusion_result != '\0') {
                  break; /* exit inner loop */
                }

                FUN_0017cfe0(tex_flags);

                /* stencil mode */
                if ((refl_flags & 8) != 0 &&
                    (char)(*(unsigned char *)((char *)entry + 0x22)) < 0) {
                  stencil_mode = 2;
                } else {
                  stencil_mode = 0;
                }
                FUN_00158ae0((short)stencil_mode);

                /* Draw the lens flare reflection:
                 * FUN_0017d010(&pos, refl_size, &scale2d,
                 *              flare_size_angle * deg2rad_scale, color) */
                FUN_0017d010(pos, refl_size, scale2d,
                             flare_size_angle * *(float *)0x253d4c, color);

                saved_refl_idx = refl_idx;
              }

              refl_idx = saved_refl_idx + 1;
              refl_ivar = (int)(short)refl_idx;
              refl_count = *(int *)(definition + 0xc4);
            } while (refl_ivar < refl_count);
          }
        }
      }

      outer_ctr = outer_ctr + 1;
      i = (int)(short)outer_ctr;
      lf_count = *(int *)0x4d0480;
    } while (i < lf_count);
  }

  FUN_00158ae0(0);
  FUN_0017ad90();

  /* Second pass: render sun glow overlays (DAT_003256fe guard) */
  if (*(char *)0x3256fe != '\0') {
    lf_count = *(int *)0x4d0480;
    if (lf_count > 0) {
      sun_i = 0;
      i = 0;
      do {
        if (sun_i < 0 || i >= lf_count) {
          display_assert(
            "lens_flare_index>=0 && lens_flare_index<local_lens_flare_count",
            "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c", 0x43, 1);
          system_exit(-1);
        }

        sun_entry = (int)((char *)0x4c6480 + (int)(short)sun_i * 0x28);
        if (*(int *)((char *)sun_entry + 0x24) > 0 &&
            ((*(unsigned char *)((char *)sun_entry + 0x22) & 0x7f) ==
             *(unsigned short *)0x5a5bc2)) {
          int sun_def;
          sun_def = *(int *)sun_entry;
          if (*(int *)(sun_def + 0x10) == 0x42480000 ||
              (*(unsigned char *)(sun_def + 0x30) & 1) != 0) {
            FUN_00169fd0((int *)sun_entry);
          }
        }

        sun_i = sun_i + 1;
        i = (int)sun_i;
        lf_count = *(int *)0x4d0480;
      } while (i < lf_count);
    }
  }

  FUN_0016fa40(0x19);
}

/* rasterizer_memory_pool.c */

/* rasterizer_memory_pool_new: allocate global rasterizer memory pool (0x1824e0)
 */
int rasterizer_memory_pool_new(void)
{
  void *pool;
  char result;
  result = 1;
  pool = (void *)debug_malloc(
    0x18000, 0, "c:\\halo\\SOURCE\\rasterizer\\rasterizer_memory_pool.c", 0x13);
  *(void **)0x4d0488 = pool;
  if (pool == 0) {
    error(2, "### ERROR rasterizer failed to allocate global memory pool");
    result = 0;
  }
  return result;
}

/* rasterizer_memory_pool_reset: reset pool allocation cursor to zero (0x182520)
 */
void rasterizer_memory_pool_reset(void)
{
  *(int *)0x4d048c = 0;
}

/* rasterizer_memory_pool_alloc: allocate from memory pool, optionally copying
 * data (0x182530) */
int rasterizer_memory_pool_alloc(int data, int size)
{
  unsigned int new_offset;
  int result;

  new_offset = *(unsigned int *)0x4d048c + size;
  result = 0;
  if (new_offset < 0x18001) {
    result = *(int *)0x4d0488 + *(int *)0x4d048c;
    *(unsigned int *)0x4d048c = new_offset;
    if (data != 0) {
      csmemcpy((void *)result, (void *)data, size);
      return result;
    }
  } else {
    error(2, "### ERROR rasterizer memory pool exceeded");
  }
  return result;
}

/* rasterizer_memory_pool_copy: assert data non-null then copy into pool
 * (0x182590) */
void rasterizer_memory_pool_copy(int data, int size)
{
  if (data == 0) {
    display_assert("data",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_memory_pool.c",
                   0x42, 1);
    system_exit(-1);
  }
  rasterizer_memory_pool_alloc(data, size);
}

/* FUN_001825d0: stub (0x1825d0) */
void FUN_001825d0(void)
{
}

/* rasterizer_memory_pool_delete: free the global rasterizer memory pool
 * (0x1825e0) */
void rasterizer_memory_pool_delete(void)
{
  if (*(void **)0x4d0488 != 0) {
    debug_free(*(void **)0x4d0488,
               "c:\\halo\\SOURCE\\rasterizer\\rasterizer_memory_pool.c", 0x50);
  }
  *(void **)0x4d0488 = 0;
  *(int *)0x4d048c = 0;
}

/* rasterizer_swizzle.c */

/* FUN_00182610: build three disjoint bit-interleave masks into the globals at
 * 0x4d0498 / 0x4d0494 / 0x4d0490 (0x182610).
 *
 * A single shifting bit (EAX) is shared across all three accumulators and only
 * advances when a dimension still has resolution left at the current probe
 * level, so the three masks partition disjoint bit ranges of the interleaved
 * address.  param_1 arrives in SI (sign-extended once, before the loop);
 * param_2/param_3 are re-sign-extended each iteration.  Comparisons are
 * unsigned against the shifting probe.
 *
 * The third accumulator lives in EDI for the whole loop and is stored to
 * 0x4d0490 once, after the loop. */
void FUN_00182610(short param_1, short param_2, short param_3)
{
  unsigned int acc_2; /* EDI: stored to 0x4d0490 after the loop */
  unsigned int probe; /* EDX */
  unsigned int bit; /* EAX: shared shifting bit */
  unsigned int taken; /* ECX: per-iteration "a bit was consumed" flag */
  int param_1_ext; /* ESI: sign-extended once, hoisted out of the loop */

  acc_2 = 0;
  probe = 1;
  bit = 1;
  *(unsigned int *)0x4d0494 = 0;
  *(unsigned int *)0x4d0498 = 0;
  param_1_ext = param_1;

  do {
    taken = 0;
    if (probe < (unsigned int)param_1_ext) {
      *(unsigned int *)0x4d0498 |= bit;
      bit <<= 1;
      taken = bit;
    }
    if (probe < (unsigned int)(int)param_2) {
      *(unsigned int *)0x4d0494 |= bit;
      bit <<= 1;
      taken = bit;
    }
    if (probe < (unsigned int)(int)param_3) {
      acc_2 |= bit;
      bit <<= 1;
      taken = bit;
    }
    probe <<= 1;
  } while (taken != 0);

  *(unsigned int *)0x4d0490 = acc_2;
}

/* rasterizer_swizzle_compute_masks: compute swizzle bit-interleave masks for a
 * texture surface (0x182690).
 * param_1/param_2: log2 of width/height; param_3/param_4: u/v tile indices;
 * param_5[0] = u mask, param_5[1] = v mask. */
void rasterizer_swizzle_compute_masks(short param_1, short param_2,
                                      unsigned short param_3,
                                      unsigned short param_4,
                                      unsigned int *param_5)
{
  int16_t sVar1;
  int16_t sVar2;
  int16_t param_1_min;
  unsigned char bVar5;
  unsigned short uVar3;
  unsigned int uVar6;
  unsigned int uVar4;
  int upper;

  sVar1 = FUN_00108db0((unsigned int)(int)param_1);
  sVar2 = FUN_00108db0((unsigned int)(int)param_2);

  /* param_1_min = min(sVar1, sVar2) */
  param_1_min = sVar2;
  if (sVar1 <= sVar2) {
    param_1_min = sVar1;
  }
  bVar5 = (unsigned char)param_1_min;
  uVar3 = (unsigned short)((1 << (bVar5 & 0x1f)) - 1);

  if ((short)uVar3 < 0x40) {
    uVar6 = (unsigned int)*(
      unsigned short *)((int)0x2b07e0 + (int)(short)(param_3 & uVar3) * 2);
    uVar4 = (unsigned int)*(
      unsigned short *)((int)0x2b07e0 + (int)(short)(param_4 & uVar3) * 2);
  } else {
    upper = (int)(short)uVar3 >> 6;
    if (upper > 0x3f) {
      display_assert("upper_mask<=63",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x56,
                     1);
      system_exit(-1);
    }
    uVar6 = (unsigned int)*(
              unsigned short *)((int)0x2b07e0 +
                                (((int)(short)param_3 >> 6) & upper) * 2)
              << 0xc |
            (unsigned int)*(unsigned short *)((int)0x2b07e0 +
                                              ((int)(short)param_3 & 0x3f) * 2);
    uVar4 = (unsigned int)*(
              unsigned short *)((int)0x2b07e0 +
                                (((int)(short)param_4 >> 6) & upper) * 2)
              << 0xc |
            (unsigned int)*(unsigned short *)((int)0x2b07e0 +
                                              ((int)(short)param_4 & 0x3f) * 2);
  }
  uVar4 = uVar4 << 1;
  if (param_1_min < sVar1) {
    param_5[1] = uVar4;
    *param_5 = uVar6 | ((int)(short)param_3 >> (bVar5 & 0x1f))
                         << (bVar5 * 2 & 0x1f);
    return;
  }
  if (param_1_min < sVar2) {
    uVar4 = uVar4 | ((int)(short)param_4 >> (bVar5 & 0x1f))
                      << (bVar5 * 2 & 0x1f);
  }
  *param_5 = uVar6;
  param_5[1] = uVar4;
}

/* rasterizer_swizzle_interleave_bits: interleave bits from up to 3 channels
 * into a Morton (Z-order) swizzle address (0x1827c0).
 * param_1/param_2/param_3: bit counts for each channel;
 * param_4/param_5/param_6: channel values (x/y/z);
 * param_7[0]=x bits, param_7[1]=y bits, param_7[2]=z bits. */
void rasterizer_swizzle_interleave_bits(short param_1, short param_2,
                                        short param_3, unsigned int param_4,
                                        unsigned int param_5,
                                        unsigned int param_6,
                                        unsigned int *param_7)
{
  unsigned int local_c;
  unsigned int local_8;
  unsigned int uVar7;
  short sVar6;
  short sVar4;
  short sVar5;
  short bVar8;
  unsigned int uVar1;
  unsigned int uVar2;
  unsigned int uVar3;

  local_c = 0;
  local_8 = 0;
  uVar7 = 0;
  sVar6 = 1;
  sVar4 = 0;
  do {
    uVar3 = param_6;
    uVar2 = param_5;
    uVar1 = param_4;
    sVar5 = sVar4;
    if (sVar6 < param_1) {
      param_4 = (unsigned int)(unsigned short)((short)param_4 >> 1);
      local_8 = local_8 | (uVar1 & 1) << ((unsigned char)sVar4 & 0x1f);
      sVar5 = sVar4 + 1;
    }
    if (sVar6 < param_2) {
      param_5 = (unsigned int)(unsigned short)((short)param_5 >> 1);
      local_c = local_c | (uVar2 & 1) << ((unsigned char)sVar5 & 0x1f);
      sVar5 = sVar5 + 1;
    }
    if (sVar6 < param_3) {
      param_6 = (unsigned int)(unsigned short)((short)param_6 >> 1);
      uVar7 = uVar7 | (uVar3 & 1) << ((unsigned char)sVar5 & 0x1f);
      sVar5 = sVar5 + 1;
    }
    sVar6 = sVar6 << 1;
    bVar8 = (short)(sVar4 != sVar5);
    sVar4 = sVar5;
  } while (bVar8);
  param_7[2] = uVar7;
  *param_7 = local_8;
  param_7[1] = local_c;
}

/* rasterizer_xbox_bitmap_swizzle2d_byte (0x182840): swizzle a 2D 8-bit
 * (one byte per pixel) bitmap from linear order into Xbox swizzled order.
 *
 * TU is c:\halo\SOURCE\rasterizer\rasterizer_swizzle.c (proven by the
 * __FILE__ assert string at 0x2b087c; the two asserts are lines 0x93/0x94).
 *
 * FUN_00182610 builds the disjoint bit-interleave masks into the globals
 * 0x4d0498 (u / column) and 0x4d0494 (v / row); the third mask (0x4d0490)
 * is unused here because the third dimension passed in is 1.
 *
 * The destination offset is `v | u` -- an OR of two disjoint accumulators,
 * NOT an add.  Each accumulator advances with the masked-subfield increment
 * `acc = (acc - mask) & mask`.  The source index is a single linear counter
 * that runs across all rows and is never reset per row.
 *
 * The v-mask global is re-read at the bottom of every outer iteration in the
 * original (ECX is clobbered by the inner body), so the read stays inside the
 * loop here.  The u-mask is loaded once, before the outer loop.
 *
 * Both loop guards are signed 16-bit `> 0` tests (TEST AX,AX / TEST SI,SI)
 * while the counters themselves are zero-extended (MOVZX). */
void rasterizer_xbox_bitmap_swizzle2d_byte(void *dst, const void *src,
                                           short width, short height)
{
  unsigned int u; /* EDI: column accumulator */
  unsigned int v; /* [EBP-4]: row accumulator */
  unsigned int umask; /* EAX: *0x4d0498, loaded once */
  unsigned int vmask; /* ECX: *0x4d0494, re-read every outer iteration */
  unsigned int rows; /* [EBP-8] */
  unsigned int cols; /* [EBP+0x14]: the dead height slot, reused */
  int linear; /* EBX: running source index */

  u = 0;
  linear = 0;
  v = 0;

  if (dst == (void *)0) {
    display_assert("dst", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                   0x93, 1);
    system_exit(-1);
  }
  if (src == (const void *)0) {
    display_assert("src", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                   0x94, 1);
    system_exit(-1);
  }

  FUN_00182610(width, height, 1);

  if (height > 0) {
    rows = (unsigned short)height;
    umask = *(unsigned int *)0x4d0498;
    do {
      if (width > 0) {
        cols = (unsigned short)width;
        do {
          ((unsigned char *)dst)[v | u] = ((const unsigned char *)src)[linear];
          linear++;
          u = (u - umask) & umask;
          cols--;
        } while (cols != 0);
      }
      vmask = *(unsigned int *)0x4d0494;
      v = (v - vmask) & vmask;
      rows--;
    } while (rows != 0);
  }
}

/* rasterizer_xbox_bitmap_swizzle2d_word (0x182910): swizzle a 2D 16-bit
 * (one word per pixel) bitmap from linear order into Xbox swizzled order.
 *
 * Identical in shape to rasterizer_xbox_bitmap_swizzle2d_byte (0x182840); the
 * only differences are the element width (word, so both indices scale by 2 in
 * the disassembly) and the assert line numbers (0xb0 / 0xb1 instead of
 * 0x93 / 0x94).  Same TU:
 * c:\halo\SOURCE\rasterizer\rasterizer_swizzle.c (__FILE__ string at 0x2b087c).
 *
 * FUN_00182610 builds the disjoint bit-interleave masks into the globals
 * 0x4d0498 (u / column) and 0x4d0494 (v / row); the third mask (0x4d0490) is
 * unused here because the third dimension passed in is 1.  Its first argument
 * (width) is a register argument in ESI -- see kb.json.
 *
 * The destination offset is `v | u` -- an OR of two disjoint accumulators, NOT
 * an add.  Each accumulator advances with the masked-subfield increment
 * `acc = (acc - mask) & mask`.  The source index is a single linear counter
 * that runs across all rows and is never reset per row; likewise the u
 * accumulator is initialized ONCE before the outer loop (it wraps modulo its
 * own mask after exactly `width` steps, so a per-row reset is unnecessary and
 * would not match).
 *
 * The u-mask is loaded once, before the height test.  The v-mask is re-read at
 * the bottom of every outer iteration in the original (ECX is clobbered by the
 * inner body at 0x1829c2), so that read stays inside the loop.
 *
 * Both loop guards are signed 16-bit `> 0` tests (TEST AX,AX / TEST SI,SI)
 * while the counters themselves are zero-extended (MOVZX). */
void rasterizer_xbox_bitmap_swizzle2d_word(void *dst, const void *src,
                                           short width, short height)
{
  unsigned int u; /* EDI: column accumulator */
  unsigned int v; /* [EBP-4]: row accumulator */
  unsigned int umask; /* EAX: *0x4d0498, loaded once */
  unsigned int vmask; /* ECX: *0x4d0494, re-read every outer iteration */
  unsigned int rows; /* [EBP-8] */
  unsigned int cols; /* [EBP+0x14]: the dead height slot, reused */
  int linear; /* EBX: running source index */

  u = 0;
  linear = 0;
  v = 0;

  if (dst == (void *)0) {
    display_assert("dst", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                   0xb0, 1);
    system_exit(-1);
  }
  if (src == (const void *)0) {
    display_assert("src", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                   0xb1, 1);
    system_exit(-1);
  }

  FUN_00182610(width, height, 1);
  umask = *(unsigned int *)0x4d0498;

  if (height > 0) {
    rows = (unsigned short)height;
    do {
      if (width > 0) {
        cols = (unsigned short)width;
        do {
          ((unsigned short *)dst)[v | u] =
            ((const unsigned short *)src)[linear];
          linear++;
          u = (u - umask) & umask;
          cols--;
        } while (cols != 0);
      }
      vmask = *(unsigned int *)0x4d0494;
      v = (v - vmask) & vmask;
      rows--;
    } while (rows != 0);
  }
}

/* rasterizer_xbox_bitmap_swizzle2d_long (0x1829f0): 32-bit-per-pixel variant of
 * the 2D Morton (Z-order) swizzle.  Identical in shape to the _byte (0x182840)
 * and _word (0x182910) siblings; only the element width and the assert line
 * numbers differ.
 *
 * The destination index is `v | u` -- an OR of two DISJOINT accumulators (the
 * u-mask and v-mask partition the address bits), NOT an add.  Confirmed at
 * 0x182a8a (OR ECX,EDI) and by the x4 scale on the store (MOV [ESI+ECX*4],EDX).
 *
 * `linear` is a single running source index across all rows and is never reset
 * per row; `u` is initialized ONCE before the outer loop.
 *
 * The u-mask is loaded once; the v-mask is re-read at the bottom of every outer
 * iteration in the original (ECX is clobbered by the inner body -- verified at
 * 0x182aa0), so that read stays inside the loop.
 *
 * Both loop guards are signed 16-bit `> 0` tests (TEST AX,AX / TEST SI,SI)
 * while the counters themselves are zero-extended (MOVZX).  The [EBP+0x14]
 * height slot is reused as the inner column counter at 0x182a7e. */
void rasterizer_xbox_bitmap_swizzle2d_long(void *dst, const void *src,
                                           short width, short height)
{
  unsigned int u; /* EDI: column accumulator */
  unsigned int v; /* [EBP-4]: row accumulator */
  unsigned int umask; /* EAX: *0x4d0498, loaded once */
  unsigned int vmask; /* ECX: *0x4d0494, re-read every outer iteration */
  unsigned int rows; /* [EBP-8] */
  unsigned int cols; /* [EBP+0x14]: the dead height slot, reused */
  int linear; /* EBX: running source index */

  u = 0;
  linear = 0;
  v = 0;

  if (dst == (void *)0) {
    display_assert("dst", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                   0xcd, 1);
    system_exit(-1);
  }
  if (src == (const void *)0) {
    display_assert("src", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                   0xce, 1);
    system_exit(-1);
  }

  FUN_00182610(width, height, 1);
  umask = *(unsigned int *)0x4d0498;

  if (height > 0) {
    rows = (unsigned short)height;
    do {
      if (width > 0) {
        cols = (unsigned short)width;
        do {
          ((unsigned int *)dst)[v | u] = ((const unsigned int *)src)[linear];
          linear++;
          u = (u - umask) & umask;
          cols--;
        } while (cols != 0);
      }
      vmask = *(unsigned int *)0x4d0494;
      v = (v - vmask) & vmask;
      rows--;
    } while (rows != 0);
  }
}

/* rasterizer_xbox_bitmap_swizzle3d_byte (0x182ac0): swizzle a 3D (volume)
 * 8-bit bitmap from linear order into Xbox swizzled (Morton / Z-order) order.
 *
 * Same TU as the 2D siblings:
 * c:\halo\SOURCE\rasterizer\rasterizer_swizzle.c (__FILE__ string at 0x2b087c).
 * Assert line numbers here are 0xeb / 0xec.
 *
 * FUN_00182610 builds the three disjoint bit-interleave masks into the globals
 * 0x4d0498 (u / column), 0x4d0494 (v / row) and 0x4d0490 (w / slice).  Unlike
 * the 2D siblings, which pass a literal 1 as the third dimension, this variant
 * passes the real `depth` (verified at 0x182b1f-0x182b2a: EAX=[EBP+0x18]=depth
 * and ECX=[EBP+0x14]=height are pushed, ESI=[EBP+0x10]=width is the @<esi>
 * register argument).
 *
 * The destination offset is `w | v | u` -- an OR of three DISJOINT
 * accumulators (the three masks partition the address bits), NOT an add.
 * Confirmed at 0x182b6b (OR EAX,ECX hoisting `w | v` out of the inner loop)
 * and 0x182b7e (OR EDX,EDI folding in `u`).  Each accumulator advances with
 * the masked-subfield increment `acc = (acc - mask) & mask` (SUB then AND).
 *
 * `linear` (EBX) is a single running source index across the whole volume and
 * is never reset; `u` (EDI), `v` ([EBP-4]) and `w` ([EBP-8]) are each
 * initialized once before the outer loop (0x182acc-0x182ad5).
 *
 * Mask re-read cadence is per-mask and is match-relevant:
 *   - u-mask 0x4d0498 is re-read on EVERY inner iteration (0x182b83), unlike
 *     the 2D siblings where it is hoisted out.
 *   - v-mask 0x4d0494 is pre-loaded at 0x182b3e and re-read at 0x182b9a,
 *     which the `jle 0x182ba0` at 0x182b63 skips -- so the re-read lives
 *     INSIDE the `width > 0` block, after the inner loop.
 *   - w-mask 0x4d0490 is pre-loaded at 0x182b4a and re-read at 0x182bb3,
 *     which the `jle 0x182bb9` at 0x182b56 skips -- so that re-read lives
 *     INSIDE the `height > 0` block, after the middle loop.
 * The `dst` and `src` pointers are likewise re-read from their stack slots on
 * every inner iteration (0x182b73, 0x182b79) because ESI/ECX are clobbered by
 * the loop body, and `width` is reloaded into ESI at 0x182b97.
 *
 * All three loop guards are signed 16-bit `> 0` tests (TEST AX,AX at 0x182b35
 * and 0x182b53, TEST SI,SI at 0x182b60) while the counters themselves are
 * zero-extended (MOVZX).  MSVC reuses the now-dead `depth` parameter slot
 * [EBP+0x18] as the inner column counter (0x182b70 / 0x182b8d / 0x182b92);
 * that is modelled here with a separate `cols` local rather than by writing
 * through the parameter. */
void rasterizer_xbox_bitmap_swizzle3d_byte(void *dst, const void *src,
                                           short width, short height,
                                           short depth)
{
  unsigned int u; /* EDI:      column accumulator */
  unsigned int v; /* [EBP-4]:  row accumulator */
  unsigned int w; /* [EBP-8]:  slice accumulator */
  unsigned int umask; /* ECX:      *0x4d0498, re-read every inner iteration */
  unsigned int vmask; /* ECX:      *0x4d0494 */
  unsigned int wmask; /* EDX:      *0x4d0490 */
  unsigned int slices; /* [EBP-0x10] */
  unsigned int rows; /* [EBP-0x0c] */
  unsigned int cols; /* [EBP+0x18]: the dead depth slot, reused */
  unsigned int wv; /* EAX: `w | v`, hoisted out of the inner loop */
  int linear; /* EBX: running source index */

  u = 0;
  linear = 0;
  v = 0;
  w = 0;

  if (dst == (void *)0) {
    display_assert("dst", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                   0xeb, 1);
    system_exit(-1);
  }
  if (src == (const void *)0) {
    display_assert("src", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                   0xec, 1);
    system_exit(-1);
  }

  FUN_00182610(width, height, depth);

  if (depth > 0) {
    vmask = *(unsigned int *)0x4d0494;
    slices = (unsigned short)depth;
    wmask = *(unsigned int *)0x4d0490;
    do {
      if (height > 0) {
        rows = (unsigned short)height;
        do {
          if (width > 0) {
            wv = w | v;
            cols = (unsigned short)width;
            do {
              ((unsigned char *)dst)[wv | u] =
                ((const unsigned char *)src)[linear];
              umask = *(unsigned int *)0x4d0498;
              u = (u - umask) & umask;
              linear++;
              cols--;
            } while (cols != 0);
            vmask = *(unsigned int *)0x4d0494;
          }
          v = (v - vmask) & vmask;
          rows--;
        } while (rows != 0);
        wmask = *(unsigned int *)0x4d0490;
      }
      w = (w - wmask) & wmask;
      slices--;
    } while (slices != 0);
  }
}

/* rasterizer_xbox_bitmap_swizzle3d_word (0x182bd0): swizzle a 3D (volume)
 * 16-bit bitmap from linear order into Xbox swizzled (Morton / Z-order) order.
 *
 * Line-for-line the same shape as the 8-bit sibling at 0x182ac0; the only
 * differences are the element width (word moves, index scaled *2 -- see
 * `MOV CX,word ptr [ECX+EBX*2]` at 0x182c86 and `MOV word ptr [ESI+EDX*2],CX`
 * at 0x182c9a) and the assert line numbers 0x10e / 0x10f.
 *
 * Same TU: c:\halo\SOURCE\rasterizer\rasterizer_swizzle.c (__FILE__ string at
 * 0x2b087c).
 *
 * FUN_00182610 builds the three disjoint bit-interleave masks into the globals
 * 0x4d0498 (u / column), 0x4d0494 (v / row) and 0x4d0490 (w / slice).  Like
 * the 8-bit volume sibling -- and unlike the 2D variants, which pass a literal
 * 1 as the third dimension -- this one passes the real `depth`
 * (0x182c2f-0x182c3a: EAX=[EBP+0x18]=depth and ECX=[EBP+0x14]=height are
 * pushed, ESI=[EBP+0x10]=width is the @<esi> register argument).
 *
 * The destination offset is `w | v | u` -- an OR of three DISJOINT
 * accumulators (the three masks partition the address bits), NOT an add.
 * Confirmed at 0x182c7b (OR EAX,ECX hoisting `w | v` out of the inner loop)
 * and 0x182c92 (OR EDX,EDI folding in `u`).  Each accumulator advances with
 * the masked-subfield increment `acc = (acc - mask) & mask` (SUB then AND).
 *
 * `linear` (EBX) is a single running source index across the whole volume and
 * is never reset; `u` (EDI), `v` ([EBP-4]) and `w` ([EBP-8]) are each
 * initialized once before the outer loop.
 *
 * Mask re-read cadence is per-mask and is match-relevant:
 *   - u-mask 0x4d0498 is re-read on EVERY inner iteration (0x182c9d).
 *   - v-mask 0x4d0494 is pre-loaded at 0x182c4e and re-read at 0x182cac,
 *     which the `jle 0x182cb2` at 0x182c73 skips -- so the re-read lives
 *     INSIDE the `width > 0` block, after the inner loop.
 *   - w-mask 0x4d0490 is pre-loaded at 0x182c5a and re-read at 0x182cc5,
 *     which the `jle 0x182ccb` at 0x182c66 skips -- so that re-read lives
 *     INSIDE the `height > 0` block, after the middle loop.
 * The `dst` and `src` pointers are likewise re-read from their stack slots on
 * every inner iteration (0x182c83, 0x182c8a) because ESI/ECX are clobbered by
 * the loop body, and `width` is reloaded into ESI at 0x182ca9.
 *
 * All three loop guards are signed 16-bit `> 0` tests (TEST AX,AX at 0x182c45
 * and 0x182c63, TEST SI,SI at 0x182c70) while the counters themselves are
 * zero-extended (MOVZX).  MSVC reuses the now-dead `depth` parameter slot
 * [EBP+0x18] as the inner column counter; that is modelled here with a
 * separate `cols` local rather than by writing through the parameter. */
void rasterizer_xbox_bitmap_swizzle3d_word(void *dst, const void *src,
                                           short width, short height,
                                           short depth)
{
  unsigned int u; /* EDI:      column accumulator */
  unsigned int v; /* [EBP-4]:  row accumulator */
  unsigned int w; /* [EBP-8]:  slice accumulator */
  unsigned int umask; /* ECX:      *0x4d0498, re-read every inner iteration */
  unsigned int vmask; /* ECX:      *0x4d0494 */
  unsigned int wmask; /* EDX:      *0x4d0490 */
  unsigned int slices; /* [EBP-0x10] */
  unsigned int rows; /* [EBP-0x0c] */
  unsigned int cols; /* [EBP+0x18]: the dead depth slot, reused */
  unsigned int wv; /* EAX: `w | v`, hoisted out of the inner loop */
  int linear; /* EBX: running source index */

  u = 0;
  linear = 0;
  v = 0;
  w = 0;

  if (dst == (void *)0) {
    display_assert("dst", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                   0x10e, 1);
    system_exit(-1);
  }
  if (src == (const void *)0) {
    display_assert("src", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                   0x10f, 1);
    system_exit(-1);
  }

  FUN_00182610(width, height, depth);

  if (depth > 0) {
    vmask = *(unsigned int *)0x4d0494;
    slices = (unsigned short)depth;
    wmask = *(unsigned int *)0x4d0490;
    do {
      if (height > 0) {
        rows = (unsigned short)height;
        do {
          if (width > 0) {
            wv = w | v;
            cols = (unsigned short)width;
            do {
              ((unsigned short *)dst)[wv | u] =
                ((const unsigned short *)src)[linear];
              umask = *(unsigned int *)0x4d0498;
              u = (u - umask) & umask;
              linear++;
              cols--;
            } while (cols != 0);
            vmask = *(unsigned int *)0x4d0494;
          }
          v = (v - vmask) & vmask;
          rows--;
        } while (rows != 0);
        wmask = *(unsigned int *)0x4d0490;
      }
      w = (w - wmask) & wmask;
      slices--;
    } while (slices != 0);
  }
}

/* rasterizer_xbox_bitmap_swizzle3d_long (0x182cf0): swizzle a 3D (volume)
 * 32-bit bitmap from linear order into Xbox swizzled (Morton / Z-order) order.
 *
 * Line-for-line the same shape as the 8-bit sibling at 0x182ac0 and the 16-bit
 * sibling at 0x182bd0; the only differences are the element width (dword moves,
 * index scaled *4 -- see `MOV ECX,dword ptr [ECX+EBX*4]` at 0x182da6 and
 * `MOV dword ptr [ESI+EDX*4],ECX` at 0x182db0) and the assert line numbers
 * 0x131 / 0x132.
 *
 * Same TU: c:\halo\SOURCE\rasterizer\rasterizer_swizzle.c (__FILE__ string at
 * 0x2b087c; the two assert reason strings are "dst" at 0x2b08ac and "src" at
 * 0x2b07dc).  Both assert tails are `PUSH -1; CALL 0x8e2f0` = system_exit(-1).
 *
 * FUN_00182610 builds the three disjoint bit-interleave masks into the globals
 * 0x4d0498 (u / column), 0x4d0494 (v / row) and 0x4d0490 (w / slice).  Like
 * the other two volume siblings -- and unlike the 2D variants, which pass a
 * literal 1 as the third dimension -- this one passes the real `depth`
 * (0x182d4f-0x182d5a: EAX=[EBP+0x18]=depth and ECX=[EBP+0x14]=height are
 * pushed, ESI=[EBP+0x10]=width is the @<esi> register argument).
 *
 * The destination offset is `w | v | u` -- an OR of three DISJOINT
 * accumulators (the three masks partition the address bits), NOT an add.
 * Confirmed at 0x182d9b (OR EAX,ECX hoisting `w | v` out of the inner loop)
 * and 0x182dae (OR EDX,EDI folding in `u`).  Each accumulator advances with
 * the masked-subfield increment `acc = (acc - mask) & mask` (SUB then AND).
 *
 * `linear` (EBX) is a single running source index across the whole volume and
 * is never reset; `u` (EDI), `v` ([EBP-4]) and `w` ([EBP-8]) are each
 * initialized once before the outer loop (0x182cfc-0x182d05).
 *
 * Mask re-read cadence is per-mask and is match-relevant:
 *   - u-mask 0x4d0498 is re-read on EVERY inner iteration (0x182db3).
 *   - v-mask 0x4d0494 is pre-loaded at 0x182d6e and re-read at 0x182dca,
 *     which the `jle 0x182dd0` at 0x182d93 skips -- so the re-read lives
 *     INSIDE the `width > 0` block, after the inner loop.
 *   - w-mask 0x4d0490 is pre-loaded at 0x182d7a and re-read at 0x182de3,
 *     which the `jle 0x182de9` at 0x182d86 skips -- so that re-read lives
 *     INSIDE the `height > 0` block, after the middle loop.
 * The `dst` and `src` pointers are likewise re-read from their stack slots on
 * every inner iteration (0x182da3, 0x182da9) because ESI/ECX are clobbered by
 * the loop body, and `width` is reloaded into ESI at 0x182dc7.
 *
 * All three loop guards are signed 16-bit `> 0` tests (TEST AX,AX at 0x182d65
 * and 0x182d83, TEST SI,SI at 0x182d90) while the counters themselves are
 * zero-extended (MOVZX).  MSVC reuses the now-dead `depth` parameter slot
 * [EBP+0x18] as the inner column counter (0x182da0 / 0x182dbd / 0x182dc2);
 * that is modelled here with a separate `cols` local rather than by writing
 * through the parameter. */
void rasterizer_xbox_bitmap_swizzle3d_long(void *dst, const void *src,
                                           short width, short height,
                                           short depth)
{
  unsigned int u; /* EDI:      column accumulator */
  unsigned int v; /* [EBP-4]:  row accumulator */
  unsigned int w; /* [EBP-8]:  slice accumulator */
  unsigned int umask; /* ECX:      *0x4d0498, re-read every inner iteration */
  unsigned int vmask; /* ECX:      *0x4d0494 */
  unsigned int wmask; /* EDX:      *0x4d0490 */
  unsigned int slices; /* [EBP-0x10] */
  unsigned int rows; /* [EBP-0x0c] */
  unsigned int cols; /* [EBP+0x18]: the dead depth slot, reused */
  unsigned int wv; /* EAX: `w | v`, hoisted out of the inner loop */
  int linear; /* EBX: running source index */

  u = 0;
  linear = 0;
  v = 0;
  w = 0;

  if (dst == (void *)0) {
    display_assert("dst", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                   0x131, 1);
    system_exit(-1);
  }
  if (src == (const void *)0) {
    display_assert("src", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                   0x132, 1);
    system_exit(-1);
  }

  FUN_00182610(width, height, depth);

  if (depth > 0) {
    vmask = *(unsigned int *)0x4d0494;
    slices = (unsigned short)depth;
    wmask = *(unsigned int *)0x4d0490;
    do {
      if (height > 0) {
        rows = (unsigned short)height;
        do {
          if (width > 0) {
            wv = w | v;
            cols = (unsigned short)width;
            do {
              ((unsigned int *)dst)[wv | u] =
                ((const unsigned int *)src)[linear];
              umask = *(unsigned int *)0x4d0498;
              u = (u - umask) & umask;
              linear++;
              cols--;
            } while (cols != 0);
            vmask = *(unsigned int *)0x4d0494;
          }
          v = (v - vmask) & vmask;
          rows--;
        } while (rows != 0);
        wmask = *(unsigned int *)0x4d0490;
      }
      w = (w - wmask) & wmask;
      slices--;
    } while (slices != 0);
  }
}

/* rasterizer_xbox_bitmap_swizzle (0x182e00): swizzle every mipmap level of a
 * bitmap in place.
 *
 * For each mipmap level a temporary buffer the size of that level's pixel data
 * is allocated, the level is swizzled from the bitmap into the temporary, and
 * the temporary is copied back over the level (csmemcpy(mip_addr, temp, size)).
 * Once all levels are done the _bitmap_is_swizzled bit (0x8) is set in
 * bitmap->flags (a BYTE-width OR on [+0xe] in the original, even though the
 * field is read as a ushort).
 *
 * Dispatch is on bitmap->type at [+0xa]: 0 = 2D, 1 = 3D (volume), 2 = cubemap;
 * anything else asserts.  Within each type the swizzler is picked by bytes per
 * pixel (1/2/4); anything else asserts.  A cubemap is six square 2D faces
 * packed back to back, so the level size is divided by 6 and the 2D swizzler is
 * run once per face -- the original computes `temp - mip_addr` once and walks a
 * single source pointer (EDI), deriving each destination as `src + delta`,
 * which is reproduced here rather than indexing both buffers.
 *
 * Skipped entirely when either flag bit 0x2 (compressed) or 0x10 is set.
 *
 * Register/frame notes (from disassembly, Ghidra dropped ALL of the swizzler
 * arguments and the width/height/depth results): width lives in ESI, height in
 * EBX, depth in [EBP-0xc].  bitmap_mipmap_width's result is moved with a plain
 * MOV ESI,EAX (no MOVSX), so width/height/depth are int locals here, while
 * bytes_per_pixel at [EBP-0x14] is stored word-wide (a later MOVSX reads it
 * back) and so stays short.  MSVC reuses [EBP-0x14] for the six-face counter
 * and [EBP-0xc] for the destination delta; those are separate locals here.
 * Assert __LINE__ values (0x14e..0x1b8) belong to rasterizer_swizzle.c. */
void FUN_00182e00(int param_1)
{
  int mip_index; /* [EBP-0x10] */
  int mip_size; /* [EBP-0x18] */
  void *mip_addr; /* [EBP-0x08] */
  void *temp; /* [EBP-0x04] */
  int width; /* ESI */
  int height; /* EBX */
  int depth; /* [EBP-0x0c] */
  short bytes_per_pixel; /* [EBP-0x14] */
  int face; /* [EBP-0x14], reused as the six-face counter */
  int face_stride; /* [EBP-0x20] = mip_size / 6 */
  int dst_delta; /* [EBP-0x0c], reused as temp - mip_addr */
  char *src_face; /* EDI */

  if (param_1 == 0) {
    display_assert(
      "bitmap", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x14e, 1);
    system_exit(-1);
  }
  if (*(int *)(param_1 + 0x2c) == 0) {
    display_assert("bitmap->base_address",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x14f,
                   1);
    system_exit(-1);
  }

  if ((*(unsigned short *)(param_1 + 0xe) & 0x12) == 0) {
    if ((*(unsigned short *)(param_1 + 0xe) & 1) == 0) {
      display_assert("TEST_FLAG(bitmap->flags, "
                     "_bitmap_has_power_of_two_dimensions_bit)",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                     0x159, 1);
      system_exit(-1);
    }

    mip_index = 0;
    if (*(short *)(param_1 + 0x14) >= 0) {
      do {
        mip_size =
          bitmap_mipmap_get_pixel_data_size((void *)param_1, mip_index);
        mip_addr = bitmap_mipmap_address((void *)param_1, (short)mip_index);
        temp = debug_malloc(
          (unsigned int)mip_size, 0,
          "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x15f);
        width = bitmap_mipmap_width((void *)param_1, mip_index);
        height = bitmap_mipmap_get_height((void *)param_1, (short)mip_index);
        depth = bitmap_mipmap_get_depth((void *)param_1, (short)mip_index);
        if (temp != (void *)0) {
          bytes_per_pixel = (short)(bitmap_format_bits_per_pixel(
                                      *(short *)(param_1 + 0xc)) /
                                    8);
          FUN_00182610(width, height, depth);
          switch (*(short *)(param_1 + 10)) {
            case 0: /* 2D */
              switch (bytes_per_pixel) {
                case 1:
                  rasterizer_xbox_bitmap_swizzle2d_byte(temp, mip_addr, width,
                                                        height);
                  break;
                case 2:
                  rasterizer_xbox_bitmap_swizzle2d_word(temp, mip_addr, width,
                                                        height);
                  break;
                case 4:
                  rasterizer_xbox_bitmap_swizzle2d_long(temp, mip_addr, width,
                                                        height);
                  break;
                default:
                  display_assert(
                    "### ERROR unsupported bitmap format (bytes per pixel)",
                    "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x17b,
                    1);
                  system_exit(-1);
                  break;
              }
              break;
            case 1: /* 3D (volume) */
              switch (bytes_per_pixel) {
                case 1:
                  rasterizer_xbox_bitmap_swizzle3d_byte(temp, mip_addr, width,
                                                        height, depth);
                  break;
                case 2:
                  rasterizer_xbox_bitmap_swizzle3d_word(temp, mip_addr, width,
                                                        height, depth);
                  break;
                case 4:
                  rasterizer_xbox_bitmap_swizzle3d_long(temp, mip_addr, width,
                                                        height, depth);
                  break;
                default:
                  display_assert(
                    "### ERROR unsupported bitmap format (bytes per pixel)",
                    "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x18f,
                    1);
                  system_exit(-1);
                  break;
              }
              break;
            case 2: /* cubemap: six faces packed back to back in the level */
              face_stride = mip_size / 6;
              dst_delta = (int)((char *)temp - (char *)mip_addr);
              src_face = (char *)mip_addr;
              face = 6;
              do {
                switch (bytes_per_pixel) {
                  case 1:
                    rasterizer_xbox_bitmap_swizzle2d_byte(
                      (void *)(src_face + dst_delta), src_face, width, height);
                    break;
                  case 2:
                    rasterizer_xbox_bitmap_swizzle2d_word(
                      (void *)(src_face + dst_delta), src_face, width, height);
                    break;
                  case 4:
                    rasterizer_xbox_bitmap_swizzle2d_long(
                      (void *)(src_face + dst_delta), src_face, width, height);
                    break;
                  default:
                    display_assert(
                      "### ERROR unsupported bitmap format (bytes per pixel)",
                      "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                      0x1a9, 1);
                    system_exit(-1);
                    break;
                }
                src_face += face_stride;
                face--;
              } while (face != 0);
              break;
            default:
              display_assert(
                "### ERROR unsupported bitmap type",
                "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x1b4, 1);
              system_exit(-1);
              break;
          }
          csmemcpy(mip_addr, temp, (unsigned int)mip_size);
          debug_free(temp, "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                     0x1b8);
          *(unsigned char *)(param_1 + 0xe) =
            *(unsigned char *)(param_1 + 0xe) | 8;
        } else {
          error(2, "### ERROR failed to allocate temporary buffer for "
                   "swizzling");
        }
        mip_index++;
      } while ((short)mip_index <= *(short *)(param_1 + 0x14));
    }
  }
}

/* rasterizer_swizzle_bitmap_mipmap_count (0x183120): number of mipmap levels
 * that will actually be swizzled for this bitmap.
 *
 * Returns 0 unless the bitmap is flagged swizzled (0x1) and not flagged 0x10.
 * Otherwise the level count is MIN(bitmap->mipmap_count,
 * FUN_00108db0(MAX(width, MAX(height, depth)))), where FUN_00108db0 maps a
 * dimension to a level index. For the compressed case (flag 0x2) width and
 * height are divided by 4 (the DXT block size) but depth is NOT -- that
 * asymmetry is real in both copies of the block in the original.
 *
 * Field widths are all int16 (+0x4 width, +0x6 height, +0x8 depth,
 * +0x14 mipmap_count, +0xe flags/ushort).
 *
 * NOTE: the MAX chain is evaluated twice per branch (once for the compare,
 * once for the returned value) -- that is the original macro expansion; do
 * not hoist it into a temporary. */
short FUN_00183120(void *param_1)
{
  unsigned short flags;
  short result;

  result = 0;

  if (!bitmap_verify(param_1, 0)) {
    display_assert("bitmap_verify(bitmap, FALSE)",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x1cb,
                   1);
    system_exit(-1);
  }

  flags = *(unsigned short *)((char *)param_1 + 0xe);
  if ((flags & 1) != 0 && (flags & 0x10) == 0) {
    if ((flags & 2) != 0) {
      /* compressed: width and height in 4x4 blocks, depth unscaled */
      result = *(short *)((char *)param_1 + 0x14);
      if (FUN_00108db0(
            (unsigned int)(*(short *)((char *)param_1 + 4) / 4 >
                               (*(short *)((char *)param_1 + 6) / 4 >
                                    *(short *)((char *)param_1 + 8) ?
                                  *(short *)((char *)param_1 + 6) / 4 :
                                  *(short *)((char *)param_1 + 8)) ?
                             *(short *)((char *)param_1 + 4) / 4 :
                             (*(short *)((char *)param_1 + 6) / 4 >
                                  *(short *)((char *)param_1 + 8) ?
                                *(short *)((char *)param_1 + 6) / 4 :
                                *(short *)((char *)param_1 + 8)))) < result) {
        return FUN_00108db0(
          (unsigned int)(*(short *)((char *)param_1 + 4) / 4 >
                             (*(short *)((char *)param_1 + 6) / 4 >
                                  *(short *)((char *)param_1 + 8) ?
                                *(short *)((char *)param_1 + 6) / 4 :
                                *(short *)((char *)param_1 + 8)) ?
                           *(short *)((char *)param_1 + 4) / 4 :
                           (*(short *)((char *)param_1 + 6) / 4 >
                                *(short *)((char *)param_1 + 8) ?
                              *(short *)((char *)param_1 + 6) / 4 :
                              *(short *)((char *)param_1 + 8))));
      }
    } else {
      result = *(short *)((char *)param_1 + 0x14);
      if (FUN_00108db0((unsigned int)(*(short *)((char *)param_1 + 4) >
                                          (*(short *)((char *)param_1 + 6) >
                                               *(short *)((char *)param_1 + 8) ?
                                             *(short *)((char *)param_1 + 6) :
                                             *(short *)((char *)param_1 + 8)) ?
                                        *(short *)((char *)param_1 + 4) :
                                        (*(short *)((char *)param_1 + 6) >
                                             *(short *)((char *)param_1 + 8) ?
                                           *(short *)((char *)param_1 + 6) :
                                           *(short *)((char *)param_1 + 8)))) <
          result) {
        return FUN_00108db0(
          (unsigned int)(*(short *)((char *)param_1 + 4) >
                             (*(short *)((char *)param_1 + 6) >
                                  *(short *)((char *)param_1 + 8) ?
                                *(short *)((char *)param_1 + 6) :
                                *(short *)((char *)param_1 + 8)) ?
                           *(short *)((char *)param_1 + 4) :
                           (*(short *)((char *)param_1 + 6) >
                                *(short *)((char *)param_1 + 8) ?
                              *(short *)((char *)param_1 + 6) :
                              *(short *)((char *)param_1 + 8))));
      }
    }
  }

  return result;
}

/* rasterizer_swizzle_bitmap_mipmaps: compute total swizzle buffer size
 * needed for all mipmaps of a bitmap (0x183290).
 * Returns total byte count, aligned to 128 bytes (or x6 for cubemaps). */
int FUN_00183290(void *param_1)
{
  int bitmap;
  short sVar2;
  short height;
  int iVar4;
  int mip_size;
  int mip_index;
  int row_pitch;
  int local_8;

  bitmap = (int)param_1;
  iVar4 = 0;
  local_8 = 0;
  sVar2 = FUN_00183120((void *)param_1);
  mip_index = 0;
  if (-1 < (int)sVar2) {
    do {
      mip_size = bitmap_mipmap_get_pixel_data_size((void *)bitmap, mip_index);
      if ((*(unsigned char *)(bitmap + 0xe) & 0x10) != 0) {
        /* swizzled/tiled: add per-row padding */
        if ((short)mip_index != 0) {
          display_assert("mipmap_index==0",
                         "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                         0x1fa, 1);
          system_exit(-1);
        }
        if ((*(unsigned char *)(bitmap + 0xe) & 2) != 0) {
          display_assert("!TEST_FLAG(bitmap->flags, _bitmap_compressed_bit)",
                         "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c",
                         0x1fb, 1);
          system_exit(-1);
        }
        row_pitch = bitmap_mipmap_get_row_pitch((void *)bitmap, mip_index);
        height = (short)bitmap_mipmap_get_height((void *)bitmap, mip_index);
        mip_size = mip_size + (int)height * (-row_pitch & 0x3f);
      }
      if (*(short *)(bitmap + 10) == 2) {
        /* cubemap: divide per-face */
        mip_size = mip_size / 6;
      }
      iVar4 = local_8 + mip_size;
      mip_index = mip_index + 1;
      local_8 = iVar4;
    } while ((short)mip_index <= sVar2);
  }
  /* align total to 128 bytes */
  iVar4 = iVar4 + (-iVar4 & 0x7f);
  if (*(short *)(bitmap + 10) == 2) {
    /* cubemap: multiply back by 6 */
    return iVar4 * 6;
  }
  return iVar4;
}

/* rasterizer_swizzle_bitmap_all: rebuild hardware format for a bitmap by
 * allocating a swizzle buffer and copying/padding all face mipmaps
 * (0x183390). Returns 1 on success, 0 on out-of-memory. */
int FUN_00183390(int param_1)
{
  int total_size;
  int iVar8;
  unsigned short face_count;
  short sVar2;
  short sVar3;
  int local_c;
  short face_index;
  int swizzle_buf;
  int mip_src;
  int mip_size;
  int row_pitch;
  short adjusted_face_index;
  short local_1c;
  int local_20;

  total_size = FUN_00183290((void *)param_1);
  iVar8 = 0;
  /* face_count: 1 for 2D textures, 6 for cubemaps */
  face_count = (unsigned short)(*(short *)(param_1 + 10) != 2) - 1 & 5;
  local_1c = (short)(face_count + 1);
  if (*(int *)(param_1 + 0x2c) == 0) {
    display_assert("bitmap->base_address",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x225,
                   1);
    system_exit(-1);
  }
  swizzle_buf = (int)debug_malloc(
    total_size, 0, "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x228);
  if (swizzle_buf == 0) {
    error(2, "### ERROR rasterizer_xbox_bitmap_rebuild_hardware_format "
             "failed (out of memory)");
    return 0;
  }
  FUN_00182e00(param_1);
  face_index = 0;
  if (local_1c > 0) {
    do {
      sVar2 = FUN_00183120((void *)param_1);
      if (-1 < (int)sVar2) {
        local_c = 0;
        local_20 = (int)sVar2;
        do {
          mip_src = (int)bitmap_mipmap_address((void *)param_1, local_c);
          mip_size =
            bitmap_mipmap_get_pixel_data_size((void *)param_1, local_c);
          if (*(short *)(param_1 + 10) == 2) {
            mip_size = mip_size / 6;
          }
          adjusted_face_index = *(short *)((int)0x2b0860 + (int)face_index * 2);
          if ((*(unsigned char *)(param_1 + 0xe) & 0x10) == 0) {
            /* non-swizzled: copy face mipmap data */
            csmemcpy((void *)(swizzle_buf + iVar8),
                     (void *)((int)adjusted_face_index * mip_size + mip_src),
                     (unsigned int)mip_size);
            iVar8 = iVar8 + mip_size;
          } else {
            /* swizzled/tiled: must be face 0, mip 0 */
            if ((face_index != 0) || (adjusted_face_index != 0)) {
              display_assert(
                "face_index==0 && adjusted_face_index==0",
                "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x24c, 1);
              system_exit(-1);
            }
            if ((short)local_c != 0) {
              display_assert(
                "mipmap_index==0",
                "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x24d, 1);
              system_exit(-1);
            }
            if ((*(unsigned char *)(param_1 + 0xe) & 2) != 0) {
              display_assert(
                "!TEST_FLAG(bitmap->flags, _bitmap_compressed_bit)",
                "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x24e, 1);
              system_exit(-1);
            }
            row_pitch = bitmap_mipmap_get_row_pitch((void *)param_1, local_c);
            sVar3 = 0;
            if (0 < *(short *)(param_1 + 6)) {
              do {
                csmemcpy((void *)(swizzle_buf + iVar8), (void *)mip_src,
                         (unsigned int)row_pitch);
                csmemset((void *)(swizzle_buf + iVar8 + row_pitch), 0,
                         (unsigned int)(-row_pitch & 0x3f));
                mip_src = mip_src + row_pitch;
                iVar8 = iVar8 + row_pitch + (-row_pitch & 0x3f);
                sVar3 = sVar3 + 1;
              } while (sVar3 < *(short *)(param_1 + 6));
            }
          }
          local_c = local_c + 1;
        } while ((short)local_c <= (short)local_20);
      }
      /* align offset to 128 bytes at end of each face */
      csmemset((void *)(swizzle_buf + iVar8), 0, (unsigned int)(-iVar8 & 0x7f));
      iVar8 = iVar8 + (-iVar8 & 0x7f);
      face_index = face_index + 1;
    } while (face_index < local_1c);
  }
  if (iVar8 != total_size) {
    display_assert("offset==size",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x271,
                   1);
    system_exit(-1);
  }
  csmemcpy(*(void **)(param_1 + 0x2c), (void *)swizzle_buf,
           (unsigned int)total_size);
  debug_free((void *)swizzle_buf,
             "c:\\halo\\SOURCE\\rasterizer\\rasterizer_swizzle.c", 0x275);
  return 1;
}

/* rasterizer_text_cache_initialize: init hardware text cache (0x183650) */
int rasterizer_text_cache_initialize(void)
{
  int texture_handle;
  char success;

  if (*(char *)0x4d04a0 != 0) {
    display_assert("!hardware_character_cache.initialized",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x76, 1);
    system_exit(-1);
  }

  texture_handle = (int)bitmap_2d_new(128, 128, 0, 9);
  if (texture_handle != 0) {
    csmemset((void *)0x4d04a0, 0, 0x810);
    success = FUN_00168370((void *)texture_handle);
    if (success != 0) {
      *(int *)0x4d04ac = texture_handle;
      *(char *)0x4d04a0 = 1;
      return 1;
    }
  }

  error(2, "### ERROR failed to initialize hardware text cache");
  return 0;
}

/* rasterizer_text_set_shadow_color: set text shadow color (0x1836e0) */
void rasterizer_text_set_shadow_color(const void *color)
{
  *(int *)0x4d0cb0 = (int)color;
}

/* rasterizer_text_cache_flush: invalidate all cached characters (0x1836f0) */
void rasterizer_text_cache_flush(void)
{
  int *slot;
  int i;

  if (*(char *)0x4d04a0 != 0) {
    slot = (int *)0x4d04b0;
    for (i = 0; i < 256; i++) {
      if (*slot != 0) {
        *(short *)(*slot + 0xc) = -1;
      }
      *slot = 0;
      slot += 2;
    }
  }
}

/* rasterizer_text_cache_dispose: dispose hardware character cache (0x183720) */
void rasterizer_text_cache_dispose(void)
{
  if (*(char *)0x4d04a0 != 0) {
    rasterizer_text_cache_flush();
    bitmap_delete(*(void **)0x4d04ac);
    *(char *)0x4d04a0 = 0;
  }
}

/* rasterizer_text.c — hardware character cache and text rendering.
 *
 * Address range: 0x183650 - 0x184060
 */

#define HARDWARE_CHARACTER_CACHE_BITMAP_WIDTH 128
#define HARDWARE_CHARACTER_CACHE_BITMAP_HEIGHT 128
#define MAXIMUM_HARDWARE_CHARACTERS 256

/* Hardware character cache (0x4d04a0, 0x810 bytes):
 *   +0x00 (byte):   initialized
 *   +0x02 (ushort): read_index   (wraps at 256)
 *   +0x04 (ushort): write_index  (wraps at 256)
 *   +0x06 (short):  cursor_x
 *   +0x08 (short):  cursor_y
 *   +0x0a (short):  max_char_height
 *   +0x0c (int):    texture_handle
 *   +0x10-0x810:    character_table[256] entries (8 bytes each):
 *       +0x0 (int*):   character pointer
 *       +0x4 (short):  screen_x
 *       +0x6 (short):  screen_y
 */

/* rasterizer_text_get_character_position: get hardware character screen
 * position. Original ABI: AX=index, EBX=*out_y, stack=*out_x
 */
void rasterizer_text_get_character_position(short index, short *out_y,
                                            short *out_x)
{
  if (*(char *)0x4d04a0 == 0) {
    display_assert("hardware_character_cache.initialized",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x255, 1);
    system_exit(-1);
  }
  if (index < 0 || index >= 256) {
    display_assert("hardware_character_index>=0 && "
                   "hardware_character_index<MAXIMUM_HARDWARE_CHARACTERS",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x256, 1);
    system_exit(-1);
  }
  if (out_x == (short *)0 || out_y == (short *)0) {
    display_assert("x0 && y0",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 599, 1);
    system_exit(-1);
  }
  *out_x = *(short *)(0x4d04b4 + index * 8);
  *out_y = *(short *)(0x4d04b6 + index * 8);
}

/* rasterizer_text_evict_character: evict a hardware character from the cache.
 * Original ABI: ESI=slot (pointer to character pointer in cache)
 */
void rasterizer_text_evict_character(int **slot)
{
  int *character;

  if (slot == (int **)0) {
    display_assert("hardware_character",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x262, 1);
    system_exit(-1);
  }

  character = *slot;
  if (character != (int *)0) {
    *(short *)((char *)character + 0xc) = -1;
    if (*(short *)((char *)character + 0xe) == *(short *)0x325748) {
      error(3, "font cache overwrote character in use");
    }
    *slot = (int *)0;
  }
}

/* rasterizer_text_cache_character: cache a hardware character into the texture
 * cache. Original ABI: EDI=character pointer, stack=font pointer
 */
void rasterizer_text_cache_character(void *font_character, void *font)
{
  int character = (int)font_character;
  int **character_slot;
  short hw_index;
  short y;
  short x;
  short *pixel_out;
  unsigned char *pixel_data;
  int i;
  int cache_top;
  int cache_bottom;
  unsigned short read_index;
  unsigned short write_index;

  if (*(char *)0x4d04a0 == 0) {
    display_assert("hardware_character_cache.initialized",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x279, 1);
    system_exit(-1);
  }

  hw_index = *(short *)(character + 0xc);

  if (hw_index == -1) {
    if (*(short *)(character + 4) > 128) {
      display_assert(
        "font_character->bitmap_width<=HARDWARE_CHARACTER_CACHE_BITMAP_WIDTH",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x285, 1);
      system_exit(-1);
    }
    if (*(short *)(character + 6) > 128) {
      display_assert(
        "font_character->bitmap_height<=HARDWARE_CHARACTER_CACHE_BITMAP_HEIGHT",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x286, 1);
      system_exit(-1);
    }

    *(short *)(character + 0xe) = *(short *)0x325748;

    /* Advance to next row if needed. Original writes _DAT_004d04a8 =
       (uint)cursor_y as a single 32-bit store, which zero-extends cursor_y
       into the high half — i.e. max_char_height (0x4d04aa) is reset to 0. */
    if (128 < (int)*(short *)0x4d04a6 + (int)*(short *)(character + 4)) {
      *(short *)0x4d04a8 += *(short *)0x4d04aa;
      *(short *)0x4d04a6 = 0;
      *(short *)0x4d04aa = 0;
    }

    /* Wrap back to top if needed, evicting characters. Original writes
       _DAT_004d04a8 = 0 as a single 32-bit store, clearing both cursor_y
       (0x4d04a8) and max_char_height (0x4d04aa). */
    if (128 < (int)*(short *)0x4d04a8 + (int)*(short *)(character + 6)) {
      *(short *)0x4d04a6 = 0;
      *(short *)0x4d04a8 = 0;
      *(short *)0x4d04aa = 0;

      read_index = *(unsigned short *)0x4d04a2;
      write_index = *(unsigned short *)0x4d04a4;

      if (read_index != write_index) {
        i = read_index & 0xFF;
        while (i != (write_index & 0xFF)) {
          if (*(short *)(0x4d04b6 + i * 8) <= 0) {
            break;
          }
          rasterizer_text_evict_character((int **)(0x4d04b0 + i * 8));
          i = (i + 1) & 0xFF;
        }
        *(unsigned short *)0x4d04a2 = (unsigned short)i;
      }
    }

    /* Evict characters that overlap */
    if (*(short *)0x4d04aa < *(short *)(character + 6)) {
      cache_top = *(short *)0x4d04a8 + *(short *)0x4d04aa;
      cache_bottom = *(short *)(character + 6) + (int)*(short *)0x4d04a8;

      read_index = *(unsigned short *)0x4d04a2;
      write_index = *(unsigned short *)0x4d04a4;

      if (read_index != write_index) {
        i = read_index & 0xFF;
        /* Original is a FIFO drain: break at the first slot whose y is
           outside [cache_top, cache_bottom); only the contiguous front
           entries are evicted and read_index advances past them. cache_bottom
           is exclusive. The prior lift instead scanned the whole queue and
           then set read_index = write_index, draining the entire character
           cache whenever a taller glyph arrived, which dropped already-cached
           menu text. */
        do {
          if (*(short *)(0x4d04b6 + i * 8) < (short)cache_top ||
              (short)cache_bottom <= *(short *)(0x4d04b6 + i * 8)) {
            break;
          }
          rasterizer_text_evict_character((int **)(0x4d04b0 + i * 8));
          i = (i + 1) & 0xFF;
        } while (i != (write_index & 0xFF));
        *(unsigned short *)0x4d04a2 = (unsigned short)i;
      }
      /* Original writes _DAT_004d04a8 = CONCAT22(char_height, cursor_y):
         a 32-bit store that sets max_char_height (0x4d04aa, high half) to
         char_height while leaving cursor_y (0x4d04a8, low half) UNCHANGED.
         The prior lift mistranslated this as `cursor_y += char_height`,
         which advanced the pen down a full row each character until a
         glyph was placed at cursor_y=128, overflowing the 128-tall cache
         texture (bitmaps.c:421 "y>=0 && y<bitmap->height"). */
      *(short *)0x4d04aa = *(short *)(character + 6);
    }

    /* Handle full cache: evict oldest character. Original compares
       (byte)(write_index + 1) against read_index, so the +1 wraps at 256;
       truncate to unsigned char before comparing or the 255->0 wrap is
       missed and the cache-full case is never detected. */
    if ((unsigned char)(*(unsigned char *)0x4d04a4 + 1) ==
        *(unsigned char *)0x4d04a2) {
      character_slot = (int **)(0x4d04b0 + *(short *)0x4d04a2 * 8);
      rasterizer_text_evict_character(character_slot);
      *(unsigned short *)0x4d04a2 =
        (unsigned short)(unsigned char)(*(unsigned char *)0x4d04a2 + 1);
    }

    /* Allocate slot and copy bitmap to texture */
    i = *(short *)0x4d04a4;
    *(short *)(character + 0xc) = (short)i;
    *(int *)(0x4d04b0 + i * 8) = character;
    *(short *)(0x4d04b4 + i * 8) = *(short *)0x4d04a6;
    *(short *)(0x4d04b6 + i * 8) = *(short *)0x4d04a8;

    pixel_data = (unsigned char *)(*(int *)((int)font + 0x94) +
                                   *(int *)(character + 0x10));

    for (y = 0; y < *(short *)(character + 6); y++) {
      pixel_out = (short *)bitmap_2d_address(
        *(void **)0x4d04ac, *(short *)(0x4d04b4 + i * 8),
        *(short *)(0x4d04b6 + i * 8) + y, 0);
      for (x = 0; x < *(short *)(character + 4); x++) {
        *pixel_out = (short)((*pixel_data << 8) | 0xfff);
        pixel_data++;
        pixel_out++;
      }
    }

    FUN_00168b10(*(void **)0x4d04ac);

    *(short *)0x4d04a6 += *(short *)(character + 4);
    *(unsigned short *)0x4d04a4 =
      (unsigned short)(unsigned char)(*(unsigned char *)0x4d04a4 + 1);
  } else {
    if (hw_index < 0 || hw_index >= 256) {
      display_assert(
        "font_character->hardware_character_index>=0 && "
        "font_character->hardware_character_index<MAXIMUM_HARDWARE_CHARACTERS",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x27d, 1);
      system_exit(-1);
    }
    if (character != *(int *)(0x4d04b0 + hw_index * 8)) {
      display_assert("font_character==hardware_character_cache.characters[font_"
                     "character->hardware_character_index].character",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c", 0x27e,
                     1);
      system_exit(-1);
    }
  }
}

/* rasterizer_text_draw_cached_char: draw a single cached character quad.
 * Vertex format is 5 floats each (screen x, screen y, texel u, texel v,
 * packed color) — 4 verts = 20 floats — in winding order TL, TR, BR, BL.
 * cache_offset_x/y (param 7/8) are added to the TEXEL coords (the atlas
 * position), not the screen position. */
void rasterizer_text_draw_cached_char(void *arg0, void *font,
                                      void *font_character, unsigned int color,
                                      short x, short y, int cache_offset_x,
                                      int cache_offset_y, short width,
                                      short height)
{
  float quad_verts[20];
  short cache_x;
  short cache_y;
  short tx;
  short ty;

  rasterizer_text_cache_character(font_character, font);

  if (*(short *)((int)font_character + 0xc) != -1) {
    rasterizer_text_get_character_position(
      *(short *)((int)font_character + 0xc), &cache_y, &cache_x);
    tx = (short)(cache_x + (short)cache_offset_x);
    ty = (short)(cache_y + (short)cache_offset_y);

    /* vert0 TL */
    quad_verts[0] = (float)x;
    quad_verts[1] = (float)y;
    quad_verts[2] = (float)tx;
    quad_verts[3] = (float)ty;
    *(unsigned int *)&quad_verts[4] = color;
    /* vert1 TR */
    quad_verts[5] = (float)(x + width);
    quad_verts[6] = (float)y;
    quad_verts[7] = (float)(tx + width);
    quad_verts[8] = (float)ty;
    *(unsigned int *)&quad_verts[9] = color;
    /* vert2 BR */
    quad_verts[10] = (float)(x + width);
    quad_verts[11] = (float)(y + height);
    quad_verts[12] = (float)(tx + width);
    quad_verts[13] = (float)(ty + height);
    *(unsigned int *)&quad_verts[14] = color;
    /* vert3 BL */
    quad_verts[15] = (float)x;
    quad_verts[16] = (float)(y + height);
    quad_verts[17] = (float)tx;
    quad_verts[18] = (float)(ty + height);
    *(unsigned int *)&quad_verts[19] = color;

    FUN_001741d0(quad_verts);
  }
}

/* rasterizer_text_draw_cached_chars: draw character string via hardware cache.
 * This is the callback used by the text drawing system. It draws the glyph
 * twice: pass 1 is the drop shadow (offset +1.0 in x/y, shadow color), pass 2
 * is the glyph itself (no offset, actual color). Vertex format is 5 floats
 * (screen x, screen y, texel u, texel v, packed color) — 4 verts = 20 floats —
 * in winding order TL, TR, BR, BL. cache_offset_x/y (param 7/8) are added to
 * the TEXEL coords, not the screen position; the shadow offset is what moves
 * the screen position. */
void rasterizer_text_draw_cached_chars(void *arg0, void *font,
                                       void *font_character, unsigned int color,
                                       short x, short y, int cache_offset_x,
                                       int cache_offset_y, short width,
                                       short height)
{
  float quad_verts[20];
  short cache_x;
  short cache_y;
  short tx;
  short ty;
  unsigned int draw_color;
  unsigned int shadow_color;
  float x_base;
  float x_right;
  float y_base;
  float y_bottom;
  float shadow_off_x;
  float shadow_off_y;
  int first_pass;
  int was_first;

  rasterizer_text_cache_character(font_character, font);

  if (*(short *)((int)font_character + 0xc) != -1) {
    shadow_off_x = 1.0f;
    shadow_off_y = 1.0f;
    shadow_color = *(unsigned int *)0x4d0cb0;
    if (*(unsigned int *)0x4d0cb0 == 0) {
      shadow_color = color & 0xff000000;
    }
    x_base = (float)x;
    x_right = (float)(width + x);
    y_base = (float)y;
    y_bottom = (float)(height + y);
    first_pass = 1;

    while (1) {
      rasterizer_text_get_character_position(
        *(short *)((int)font_character + 0xc), &cache_y, &cache_x);
      was_first = first_pass;
      tx = (short)(cache_x + (short)cache_offset_x);
      ty = (short)(cache_y + (short)cache_offset_y);
      draw_color = shadow_color;
      if (first_pass == 0) {
        draw_color = color;
      }

      /* vert0 TL */
      quad_verts[0] = x_base + shadow_off_x;
      quad_verts[1] = y_base + shadow_off_y;
      quad_verts[2] = (float)tx;
      quad_verts[3] = (float)ty;
      *(unsigned int *)&quad_verts[4] = draw_color;
      /* vert1 TR */
      quad_verts[5] = x_right + shadow_off_x;
      quad_verts[6] = y_base + shadow_off_y;
      quad_verts[7] = (float)(tx + width);
      quad_verts[8] = (float)ty;
      *(unsigned int *)&quad_verts[9] = draw_color;
      /* vert2 BR */
      quad_verts[10] = x_right + shadow_off_x;
      quad_verts[11] = y_bottom + shadow_off_y;
      quad_verts[12] = (float)(tx + width);
      quad_verts[13] = (float)(ty + height);
      *(unsigned int *)&quad_verts[14] = draw_color;
      /* vert3 BL */
      quad_verts[15] = x_base + shadow_off_x;
      quad_verts[16] = y_bottom + shadow_off_y;
      quad_verts[17] = (float)tx;
      quad_verts[18] = (float)(ty + height);
      *(unsigned int *)&quad_verts[19] = draw_color;

      FUN_001741d0(quad_verts);

      if (was_first == 0) {
        break;
      }
      first_pass = 0;
      shadow_off_x = 0.0f;
      shadow_off_y = 0.0f;
    }
  }
}

/* rasterizer_text_draw: draw ASCII string (0x183e60) */
void rasterizer_text_draw(void *screen_pos, short *bounds, const void *color,
                          int flags, const char *text)
{
  int draw_bounds[4];
  int clip_bounds[4];
  float texel_width;
  float texel_height;
  float widget_params[35];
  void *texture;
  int font_width;
  int font_height;
  int max_width;
  int max_height;
  int clamp_x;
  int clamp_y;

  if (*(char *)0x3256da == 0 || *(short *)0x5a5bc0 != 0) {
    return;
  }

  *(short *)0x325748 += 1;

  if (text == (const char *)0) {
    display_assert("string", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c",
                   0xb4, 1);
    system_exit(-1);
  }

  texture = (void *)(*(int *)0x4d04ac);
  if ((*(char *)0x4d04a0 != 0) && texture != (void *)0 && *text != 0) {
    csstrlen(text);

    if (screen_pos == (void *)0) {
      draw_bounds[0] = *(int *)0x506584;
      draw_bounds[1] = *(int *)0x506588;
      rect2d_offset((short *)draw_bounds, (short)(-*(short *)0x50657e),
                    (short)(-*(short *)0x50657c));
    } else {
      draw_bounds[0] = *(int *)screen_pos;
      draw_bounds[1] = *(int *)((char *)screen_pos + 4);
    }

    if (bounds == (short *)0) {
      clip_bounds[0] = *(int *)0x50657c;
      clip_bounds[1] = *(int *)0x506580;
      rect2d_offset((short *)clip_bounds, (short)(-*(short *)0x50657e),
                    (short)(-*(short *)0x50657c));
    } else {
      max_width = (int)bounds[2];
      if ((int)(*(short *)0x506580 - *(short *)0x50657c) <= (int)bounds[2]) {
        max_width = (int)(*(short *)0x506580 - *(short *)0x50657c);
      }
      max_height = (int)(*(short *)0x506582 - *(short *)0x50657e);
      if ((int)bounds[3] < max_height) {
        max_height = (int)bounds[3];
      }
      clamp_x = (int)bounds[0];
      if (bounds[0] < 0) {
        clamp_x = 0;
      }
      clamp_y = (int)bounds[1];
      if (bounds[1] < 0) {
        clamp_y = 0;
      }
      FUN_001089a0(clip_bounds, clamp_y, clamp_x, max_height, max_width);
      texture = (void *)(*(int *)0x4d04ac);
    }

    csmemset(widget_params, 0, 0x8c);
    font_width = (int)*(short *)((int)texture + 4);
    font_height = (int)*(short *)((int)texture + 6);
    texel_width = *(float *)0x2533c8 / (float)font_width;
    texel_height = *(float *)0x2533c8 / (float)font_height;

    *(unsigned int *)&widget_params[3] = (unsigned int)texture;
    widget_params[10] = 1.0f;
    widget_params[11] = 1.0f;
    widget_params[16] = texel_width;
    widget_params[17] = texel_height;

    FUN_00173b40(widget_params);
    FUN_0019c5d0(rasterizer_text_draw_cached_chars, draw_bounds, color,
                 clip_bounds, flags, (char *)text);
    FUN_00173ae0();
  }
}

/* rasterizer_draw_string: draw wide-character string (0x184060) */
void rasterizer_draw_string(void *screen_pos, short *bounds, const void *color,
                            int flags, unsigned short *text)
{
  int draw_bounds[4];
  int clip_bounds[4];
  float texel_width;
  float texel_height;
  float widget_params[35];
  void *texture;
  int font_width;
  int font_height;
  int max_width;
  int max_height;
  int clamp_x;
  int clamp_y;

  if (*(char *)0x3256da == 0 || *(short *)0x5a5bc0 != 0) {
    return;
  }

  *(short *)0x325748 += 1;

  if (text == (unsigned short *)0) {
    display_assert("string", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_text.c",
                   0x136, 1);
    system_exit(-1);
  }

  texture = (void *)(*(int *)0x4d04ac);
  if ((*(char *)0x4d04a0 != 0) && texture != (void *)0 && *text != 0) {
    ustrlen(text);

    if (screen_pos == (void *)0) {
      draw_bounds[0] = *(int *)0x506584;
      draw_bounds[1] = *(int *)0x506588;
      rect2d_offset((short *)draw_bounds, (short)(-*(short *)0x50657e),
                    (short)(-*(short *)0x50657c));
    } else {
      draw_bounds[0] = *(int *)screen_pos;
      draw_bounds[1] = *(int *)((char *)screen_pos + 4);
    }

    if (bounds == (short *)0) {
      clip_bounds[0] = *(int *)0x50657c;
      clip_bounds[1] = *(int *)0x506580;
      rect2d_offset((short *)clip_bounds, (short)(-*(short *)0x50657e),
                    (short)(-*(short *)0x50657c));
    } else {
      max_width = (int)bounds[2];
      if ((int)(*(short *)0x506580 - *(short *)0x50657c) <= (int)bounds[2]) {
        max_width = (int)(*(short *)0x506580 - *(short *)0x50657c);
      }
      max_height = (int)(*(short *)0x506582 - *(short *)0x50657e);
      if ((int)bounds[3] < max_height) {
        max_height = (int)bounds[3];
      }
      clamp_x = (int)bounds[0];
      if (bounds[0] < 0) {
        clamp_x = 0;
      }
      clamp_y = (int)bounds[1];
      if (bounds[1] < 0) {
        clamp_y = 0;
      }
      FUN_001089a0(clip_bounds, clamp_y, clamp_x, max_height, max_width);
      texture = (void *)(*(int *)0x4d04ac);
    }

    csmemset(widget_params, 0, 0x8c);
    font_width = (int)*(short *)((int)texture + 4);
    font_height = (int)*(short *)((int)texture + 6);
    texel_width = *(float *)0x2533c8 / (float)font_width;
    texel_height = *(float *)0x2533c8 / (float)font_height;

    *(unsigned int *)&widget_params[3] = (unsigned int)texture;
    widget_params[10] = 1.0f;
    widget_params[11] = 1.0f;
    widget_params[16] = texel_width;
    widget_params[17] = texel_height;

    FUN_00173b40(widget_params);
    FUN_0019c960(rasterizer_text_draw_cached_chars, draw_bounds, color,
                 clip_bounds, flags, text);
    FUN_00173ae0();
  }
}

/* rasterizer_transparent_geometry.c */

/* rasterizer_transparent_geometry_new: allocate transparent geometry buffers
 * and init vertex cache (0x184260) */
int rasterizer_transparent_geometry_new(void)
{
  int success;

  *(void **)0x4d0cec = debug_malloc(
    0xf000, 0,
    "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0x29);
  *(void **)0x4d0cfc = debug_malloc(
    0x300, 0, "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c",
    0x2b);
  *(void **)0x4d0cf0 = debug_malloc(
    0x1400, 0,
    "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0x2e);
  *(int *)0x4d0cf8 = 0;
  *(int *)0x4d0cf4 = 0;
  if (*(int *)0x4d0cec == 0 || *(int *)0x4d0cfc == 0 || *(int *)0x4d0cf0 == 0) {
    error(2, "### ERROR failed to allocate transparent geometry buffer");
  } else {
    success = FUN_00174bd0();
    if (success != 0) {
      return 1;
    }
  }
  return 0;
}

/* rasterizer_transparent_geometry_begin: reset group counts and stats for new
 * frame (0x184300) */
void rasterizer_transparent_geometry_begin(void)
{
  *(int *)0x4d0cf4 = 0;
  *(short *)0x4d0d00 = 0;
  csmemset((void *)0x4d0cbc, 0, 0x30);
  *(int *)0x4d0cf8 = 0;
}

/* rasterizer_transparent_geometry_group_new: allocate next transparent geometry
 * group slot (0x184330) */
void *rasterizer_transparent_geometry_group_new(void)
{
  void *group;

  group = (void *)0;
  if (*(int *)0x4d0cf4 < 0x180) {
    group = (void *)(*(int *)0x4d0cf4 * 0xa0 + *(int *)0x4d0cec);
    *(int *)((char *)group + 0x90) = *(int *)0x4d0cf4;
    *(int *)0x4d0cf4 = *(int *)0x4d0cf4 + 1;
  }
  return group;
}

/* rasterizer_secondary_geometry_group_new: allocate next secondary geometry
 * group slot (0x184360) */
void *rasterizer_secondary_geometry_group_new(void)
{
  void *group;

  group = (void *)0;
  if (*(int *)0x4d0cf8 < 0x20) {
    group = (void *)(*(int *)0x4d0cf8 * 0xa0 + *(int *)0x4d0cf0);
    *(int *)((char *)group + 0x90) = *(int *)0x4d0cf8;
    *(int *)0x4d0cf8 = *(int *)0x4d0cf8 + 1;
  }
  return group;
}

/* rasterizer_secondary_geometry_groups_get: return secondary groups buffer;
 * optionally write count (0x184390) */
void *rasterizer_secondary_geometry_groups_get(short *out_count)
{
  if (out_count != (short *)0) {
    *out_count = (short)*(int *)0x4d0cf8;
  }
  return *(void **)0x4d0cf0;
}

/* rasterizer_transparent_geometry_next_group: return next sorted group after
 * given group (0x1843b0) */
void *rasterizer_transparent_geometry_next_group(void *group)
{
  short next_index;
  short sorted_index;

  if (group != (void *)0) {
    sorted_index = *(short *)((char *)group + 0x90);
    next_index = (short)(sorted_index + 1);
    if (*(int *)((char *)group + 0x90) < 0 ||
        *(int *)0x4d0cf4 <= *(int *)((char *)group + 0x90)) {
      display_assert(
        "group->sorted_index>=0 && "
        "group->sorted_index<transparent_geometry_group_count",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0x89,
        1);
      system_exit(-1);
    }
    if (next_index < *(int *)0x4d0cf4) {
      if (next_index < 0) {
        display_assert(
          "next_group_sorted_index>=0",
          "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c",
          0x8d, 1);
        system_exit(-1);
      }
      return (void *)(*(short *)(*(int *)0x4d0cfc + next_index * 2) * 0xa0 +
                      *(int *)0x4d0cec);
    }
  }
  return (void *)0;
}

/* rasterizer_transparent_geometry_group_get: return group by presorted index
 * (0x184460) */
void *rasterizer_transparent_geometry_group_get(short group_presorted_index)
{
  if (group_presorted_index < 0 || *(int *)0x4d0cf4 <= group_presorted_index) {
    display_assert(
      "group_presorted_index>=0 && "
      "group_presorted_index<transparent_geometry_group_count",
      "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0xbc,
      1);
    system_exit(-1);
  }
  return (void *)(group_presorted_index * 0xa0 + *(int *)0x4d0cec);
}

/* rasterizer_transparent_geometry_group_to_presorted_index: convert group
 * pointer to presorted index (0x1844b0) */
short rasterizer_transparent_geometry_group_to_presorted_index(
  unsigned int group)
{
  unsigned int base;
  int count;
  short index;
  unsigned int offset;
  unsigned int remainder;

  base = *(unsigned int *)0x4d0cec;
  count = *(int *)0x4d0cf4;
  index = -1;
  if (group >= base && group < base + (unsigned int)(count * 0xa0)) {
    index = (short)((int)(group - base) / 0xa0);
    if (index < 0 || count <= index) {
      display_assert(
        "group_presorted_index>=0 && "
        "group_presorted_index<transparent_geometry_group_count",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0xcb,
        1);
      system_exit(-1);
    }
    offset = group - base;
    remainder = offset % 0xa0;
    if (remainder != 0) {
      display_assert(
        "((unsigned long)group-(unsigned "
        "long)transparent_geometry_groups)%sizeof(struct "
        "transparent_geometry_group)==0",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0xcc,
        1);
      system_exit(-1);
    }
  }
  return index;
}
