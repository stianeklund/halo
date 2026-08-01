/*
 * rasterizer_xbox_widgets.c
 *
 * Xbox rasterizer widget helpers.
 *
 * Source path (from binary):
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_widgets.c
 * (proven by the __FILE__ string at 0x2ae7f0, referenced by the assert at
 *  0x17ae45)
 *
 * Globals (used by address, not in kb.json):
 *   0x3256fc  char  - occlusion-query feature flag (byte, !=0 gate)
 */

/* 0x17adc0 — rasterizer_widget_get_occlusion_test_result
 *
 * Blocking read of a D3D visibility (occlusion) query result.  Issues
 * GetVisibilityTestResult; while the query is still pending
 * (0x88760828 == D3DERR_ ...PENDING) it spins inside a
 * rasterizer_spin_begin(0x1a)/rasterizer_spin_end() bracket.
 *
 * Name evidence: the trailing error() string at 0x2ae788 is
 * "### ERROR rasterizer_widget_get_occlusion_test_result failed".
 *
 * Shape notes (all from disassembly, not the decompiler):
 *   - The 0xffffffff seed of the result local is stored at 0x17adcd, i.e.
 *     BETWEEN the flag TEST and the early-out JE — it is emitted before the
 *     branch even though it is dead on the early-out path.
 *   - The early-out at 0x17ae7b returns the literal 1 (MOV EAX,1), NOT the
 *     0xffffffff sentinel.  Do not merge the two exits.
 *   - Ghidra declared the assert/error blocks unreachable (it treats
 *     display_assert as noreturn) and dropped both the BL success flag and
 *     the trailing error() call.  Both are present in the binary
 *     (BL set at 0x17ae23 / cleared at 0x17ae2d, tested at 0x17ae60).
 *   - The assert tail at 0x17ae56 is system_exit(-1), not
 *     halt_and_catch_fire.
 */
unsigned int rasterizer_widget_get_occlusion_test_result(unsigned int index)
{
  unsigned int occlusion_test_result; /* [EBP-0x4] — also the return value */
  unsigned int timestamp; /* [EBP-0xc] — out param, unused after */
  int hr; /* ESI */
  char ok; /* BL — running success flag */

  occlusion_test_result = 0xffffffff;
  /* nested-if (not `if (!flag) return 1;`) so that the literal-1 exit is laid
   * out as a trailing block reached by a forward JE, matching the original's
   * `je LAB_0017ae7b` + separate `mov eax,1 / leave / ret` tail. */
  if (*(char *)0x3256fc != 0) {
    hr = D3DDevice_GetVisibilityTestResult(index, &occlusion_test_result,
                                           &timestamp);
    if (hr == (int)0x88760828) {
      rasterizer_spin_begin(0x1a);
      /* the two LEAs and the index push are re-done every iteration in the
       * original (the loop head at 0x17ae02 is the first LEA, not the
       * CALL). */
      do {
        hr = D3DDevice_GetVisibilityTestResult(index, &occlusion_test_result,
                                               &timestamp);
      } while (hr == (int)0x88760828);
      rasterizer_spin_end();
    }

    if (hr < 0) {
      ok = 0;
      FUN_00167ff0(hr, "hr");
    } else {
      ok = 1;
    }

    if ((int)occlusion_test_result < 0) {
      display_assert("(occlusion_test_result&0x80000000)==0",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "widgets.c",
                     0x23e, 1);
      system_exit(-1);
    }
    if (ok == 0) {
      error(2, "### ERROR rasterizer_widget_get_occlusion_test_result failed");
    }
    return occlusion_test_result;
  }
  return 1;
}

/* 0x17ae90 — FUN_0017ae90
 *
 * Builds a transparent-geometry group for a widget: allocates the next group
 * slot (rasterizer_transparent_geometry_group_new, 0x184330), stores the three
 * caller-supplied handles, zero-initialises the group record and computes the
 * signed plane distance of the supplied centroid against the view plane.
 *
 * Signature (from disassembly — the kb decl was the "void(void)" stub, so
 * Ghidra rendered the four cdecl slots as in_stack_XXXX):
 *   [EBP+0x08] arg_4c    -> group+0x4c
 *   [EBP+0x0c] arg_50    -> group+0x50
 *   [EBP+0x10] centroid  (float *, kept in EDI)
 *   [EBP+0x14] arg_48    -> group+0x48, and the non-zero guard at entry
 * Plain RET (no immediate) => __cdecl.  No register arguments.
 *
 * Globals (used by address, not in kb.json):
 *   0x5a5bc8/cc/d0  float  - view reference point (centroid is measured from
 * it) 0x5a5bd4/d8/dc  float  - view plane normal 0x47e4ca        char   -
 * one-shot "already reported" flag for the error()
 *
 * Group record: stride 0xa0 (see rasterizer_transparent_geometry_group_new).
 * Mixed store widths taken from the disassembly, NOT the decompiler:
 *   dword +0x00 +0x04 +0x08 +0x0c +0x44 +0x48 +0x4c +0x50 +0x54 +0x58 +0x5c
 *         +0x60 +0x68 +0x6c +0x80..+0x8c +0x98
 *   float +0x3c +0x40 (1.0f, materialised once in ECX as 0x3f800000)
 *         +0x70 (plane distance) +0x74 +0x78 +0x7c (centroid copy)
 *   word  +0x10 +0x14 +0x64 +0x94 +0x96      byte +0x9d
 * Do not widen the 16-bit fields to int (LOADW hazard).
 *
 * Shape notes:
 *   - group_new() is called BEFORE the centroid NULL check; the assert tail is
 *     display_assert(...) then system_exit(-1) (CALL at 0x17aec9 resolves to
 *     0x8e2f0 in the pristine XBE; Ghidra's delinked reloc mislabels it
 *     FUN_001029a0 / halt_and_catch_fire).
 *   - d0/d1/d2 stay live on the x87 stack across the whole integer-store block
 *     and are popped by three FSTP ST(0) at the end — they are float locals
 *     enregistered in the FPU, not expression temporaries.
 *   - The dot product is emitted right-associated: FLD[bdc]*d2, FLD[bd8]*d1,
 *     FADDP, FLD[bd4]*d0, FADDP, FCHS.
 */
typedef struct {
  float x;
  float y;
  float z;
} rasterizer_widget_point3d;

/* the 16 bytes at group+0x80; zeroed through a stack copy (EBP-0x10..EBP-0x4)
 * in the original, so it is an aggregate assignment, not four direct stores */
typedef struct {
  unsigned int field_0;
  unsigned int field_4;
  unsigned int field_8;
  unsigned int field_c;
} rasterizer_widget_block16;

void FUN_0017ae90(unsigned int arg_4c, unsigned int arg_50, float *centroid,
                  unsigned int arg_48)
{
  char *group;
  /* volatile is required to reproduce the original frame: the original keeps
   * this 16-byte block in the stack slots EBP-0x10..EBP-0x4 (SUB ESP,0x10),
   * zeroes it with four MOVL $0 and only then copies it to group+0x80 through
   * ECX.  Without volatile VC71 constant-folds the copy into four direct zero
   * stores and drops the stack frame entirely (86.1% -> 91.7%). */
  volatile rasterizer_widget_block16 zeros; /* EBP-0x10 .. EBP-0x4 */
  float d0, d1, d2; /* held in ST(2)/ST(1)/ST(0) by the original */

  if (arg_48 != 0) {
    group = (char *)rasterizer_transparent_geometry_group_new();

    if (centroid == (float *)0) {
      display_assert("centroid",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "widgets.c",
                     0x58, 1);
      system_exit(-1);
    }

    if (group != (char *)0) {
      d0 = centroid[0] - *(const float *)0x5a5bc8;
      d1 = centroid[1] - *(const float *)0x5a5bcc;
      d2 = centroid[2] - *(const float *)0x5a5bd0;

      zeros.field_0 = 0;
      zeros.field_4 = 0;
      zeros.field_8 = 0;
      zeros.field_c = 0;

      *(unsigned int *)(group + 0x48) = arg_48;
      *(unsigned int *)(group + 0x4c) = arg_4c;
      *(unsigned int *)(group + 0x50) = arg_50;
      *(int *)(group + 0x00) = 0;
      *(int *)(group + 0x04) = 0;
      *(int *)(group + 0x08) = 0;
      *(int *)(group + 0x0c) = 0;
      *(short *)(group + 0x10) = 0;
      *(short *)(group + 0x14) = 0;
      *(int *)(group + 0x58) = 0;
      *(int *)(group + 0x5c) = 0;
      *(int *)(group + 0x44) = -1;
      *(int *)(group + 0x54) = -1;

      *(float *)(group + 0x70) =
        -(*(const float *)0x5a5bd4 * d0 +
          (*(const float *)0x5a5bd8 * d1 + *(const float *)0x5a5bdc * d2));

      *(rasterizer_widget_point3d *)(group + 0x74) =
        *(rasterizer_widget_point3d *)centroid;
      *(rasterizer_widget_block16 *)(group + 0x80) =
        *(rasterizer_widget_block16 *)&zeros;

      *(int *)(group + 0x98) = 0;
      *(char *)(group + 0x9d) = 0;
      *(int *)(group + 0x60) = 0;
      *(short *)(group + 0x64) = 0;
      *(int *)(group + 0x68) = 0;
      *(int *)(group + 0x6c) = 0;
      *(float *)(group + 0x40) = 1.0f;
      *(float *)(group + 0x3c) = 1.0f;
      *(short *)(group + 0x94) = -1;
      *(short *)(group + 0x96) = -1;
      return;
    }

    if (*(char *)0x47e4ca == 0) {
      error(2, "### ERROR too many transparent geometry groups");
      *(char *)0x47e4ca = 1;
    }
  }
}
