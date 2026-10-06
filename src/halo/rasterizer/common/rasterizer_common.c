/* Rasterizer common init/dispose and per-frame update. */

/* Initialize rasterizer data for a new map. Loads the rasterizer data
 * tag block from game globals and initializes rendering subsystems. */
void rasterizer_initialize_for_new_map(void)
{
  char *globals;
  int rasterizer_data;

  globals = (char *)game_globals_get();
  assert_halt_msg_at("game_globals", "c:\\halo\\SOURCE\\rasterizer\\common\\rasterizer_common.c", 0x18, globals);
  if (*(int *)(globals + 0x134) != 0) {
    rasterizer_data = (int)tag_block_get_element(globals + 0x134, 0, 0x1ac);
  } else {
    rasterizer_data = 0;
  }
  *(int *)0x476204 = rasterizer_data;
  assert_halt_msg_at("global_rasterizer_data", "c:\\halo\\SOURCE\\rasterizer\\common\\rasterizer_common.c", 0x1a, rasterizer_data);

  FUN_00181150();
  rasterizer_text_cache_flush();
  FUN_0017d950();

  if (*(void **)0x47e4d0 != 0)
    csmemset(*(void **)0x47e4d0, 0, 0x10);

  FUN_0017dec0(0);
}

/* Dispose rasterizer state from old map. */
void rasterizer_dispose_from_old_map(void)
{
  FUN_0017d980();
  rasterizer_text_cache_flush();
  *(int *)0x476204 = 0;
}

/* Per-frame rasterizer update — stores the delta time. */
void rasterizer_frame_update(float delta_time)
{
  *(float *)0x5a5e1c = delta_time;
}

/* D3D8 CreateDevice register-shim. Receives three register args
 * (EAX/ECX/EDX) and four stack args (the first stack arg is unused;
 * RET 0x10 cleans all four), reorders them into the standard 6-arg
 * stdcall to IDirect3D8_CreateDevice. No callers observed in the
 * shipped XBE — dead code from debug builds. */
void FUN_001550c0(unsigned int arg_eax /* @<eax> */,
                  unsigned int arg_ecx /* @<ecx> */,
                  unsigned int arg_edx /* @<edx> */, unsigned int unused_stack0,
                  unsigned int stack_arg1, unsigned int stack_arg2,
                  unsigned int stack_arg3)
{
  (void)unused_stack0;
  Direct3D_CreateDevice(stack_arg1, stack_arg2, (void *)stack_arg3, arg_edx,
                        (void *)arg_ecx, (void **)arg_eax);
}

/* Read one entry from D3D_g_DeferredTextureState[stage][sub_index] into
 * *result. The array is a flat DWORD array at 0x1fb498 with 32 entries per
 * stage row. Register args: EAX=stage, ECX=sub_index; stack arg: result
 * pointer. */
void FUN_00155110(int stage /* @<eax> */, int sub_index /* @<ecx> */,
                  unsigned int *result)
{
  *result = ((unsigned int *)0x1fb498)[stage * 32 + sub_index];
}
