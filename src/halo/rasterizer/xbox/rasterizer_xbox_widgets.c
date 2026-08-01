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
  unsigned int timestamp;             /* [EBP-0xc] — out param, unused after */
  int hr;                             /* ESI */
  char ok;                            /* BL — running success flag */

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
      error(2,
            "### ERROR rasterizer_widget_get_occlusion_test_result failed");
    }
    return occlusion_test_result;
  }
  return 1;
}
