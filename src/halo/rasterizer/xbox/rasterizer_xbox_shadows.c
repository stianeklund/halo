/*
 * rasterizer_xbox_shadows.c
 *
 * Rasterizer Xbox shadow rendering support.
 *
 * Source path (from binary):
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_shadows.c
 *
 * Globals (used by address, not in kb.json):
 *   0x476ab0  void *  – global_d3d_device (IDirect3DDevice8 pointer)
 *   0x5a5bc0  short   – render-disable / suppression flag (16-bit, ==0 gate)
 *   0x3256ca  char    – shadow feature enable flag (byte, !=0 gate)
 *   0x3256ba  short   – render mode/pass selector (16-bit, ==2 -> bump counter)
 *   0x47e4b0  void *  – stashed shadow parameters pointer
 *   0x47e4b5  bool    – shadow-parameters-active flag (one-shot set)
 *   0x5a54f4  int     – per-frame counter incremented when mode word == 2
 */

/* 0x172590
 *
 * FUN_00172590
 *
 * Begins/sets the per-frame shadow rendering parameters.
 *
 * Asserts the D3D device exists. When rendering is enabled
 * (*(short *)0x5a5bc0 == 0) and the shadow feature flag is set
 * (*(char *)0x3256ca != 0):
 *   1. Asserts the supplied parameters pointer is non-null.
 *   2. Programs model skinning from the parameter block at param+8.
 *   3. Stashes the parameters pointer and marks shadow params active.
 *   4. If the render-mode word (*(short *)0x3256ba) == 2, increments the
 *      per-frame counter at 0x5a54f4.
 *
 * param_1: pointer to the shadow parameter block.
 */
void FUN_00172590(int param_1)
{
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0xef, 1);
    system_exit(-1);
  }
  if (*(short *)0x5a5bc0 == 0 && *(char *)0x3256ca != 0) {
    if (param_1 == 0) {
      display_assert(
        "parameters",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0xf5,
        1);
      system_exit(-1);
    }
    rasterizer_set_model_skinning((void *)(param_1 + 8));
    *(int *)0x47e4b0 = param_1;
    *(char *)0x47e4b5 = 1;
    if (*(short *)0x3256ba == 2) {
      *(int *)0x5a54f4 = *(int *)0x5a54f4 + 1;
    }
  }
}
