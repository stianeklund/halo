/* structure_render.c
 * Halo CE Xbox (cachebeta.xbe) reverse-engineered reimplementation.
 * Original translation unit: c:\halo\SOURCE\structures\structure_render.c
 */

/* 0x1954e0 - set the current planar-fog eye offset for structure rendering.
 *
 * Double-set guard: if structure_render_globals.fog_offset_valid
 * (byte @ 0x4d8eb8) is already non-zero, display_assert() then system_exit(-1).
 * Otherwise mark the offset valid and copy the caller's vector3 fog offset into
 * the globals at 0x4d8ebc / 0x4d8ec0 / 0x4d8ec4.
 *
 * Copies are plain dword moves in the original (no FPU ops), so the parameter is
 * treated as raw dwords rather than float loads/stores. */
void FUN_001954e0(void *fog_offset)
{
  uint32_t *src = (uint32_t *)fog_offset;

  if (*(uint8_t *)0x4d8eb8 != 0) {
    display_assert("!structure_render_globals.fog_offset_valid",
                   "c:\\halo\\SOURCE\\structures\\structure_render.c", 0x67,
                   true);
    system_exit(-1);
  }

  *(uint8_t *)0x4d8eb8 = 1;
  *(uint32_t *)0x4d8ebc = src[0];
  *(uint32_t *)0x4d8ec0 = src[1];
  *(uint32_t *)0x4d8ec4 = src[2];
}
