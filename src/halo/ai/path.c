/* path.c — AI path planning state builders.
 *
 * Corresponds to path.obj (XBE address range ~0x5dfc0–0x5ef13+).
 * __FILE__ = c:\halo\SOURCE\ai\path.c (confirmed via display_assert strings
 * in FUN_0005eae0 at 0x5eae0).
 *
 * Ported: path_state_init (0x5dfc0), path_state_set_focus (0x5e000),
 *         path_state_set_sphere (0x5e030), path_state_set_min_speed (0x5e070),
 *         path_state_commit (0x5e090), path_state_set_obstacle (0x5e0d0).
 * Deferred: FUN_0005eae0 (0x5eae0) — complex path evaluation, deferred.
 */

#include "../../common.h"

/* All callees (csmemset, csmemcpy, scenario_get) are declared via decl.h */

/* 0x005dfc0 — path_state_init
 * Zero-fills a 0x48-byte path_state record, then writes the initial fields.
 *
 * Disassembly-confirmed field layout (ESI = param_1):
 *   [ESI+0x00] = param_2  (uint32_t flags)
 *   [ESI+0x04] = param_3  (uint8_t — byte at +4 in the uint32_t slot, MSVC
 * packs) [ESI+0x08] = param_4  (int unit_handle) [ESI+0x0c] = 0xffffffff
 *
 * Note: Ghidra showed param_1+1 (dword slot) for param_3 storage, but the
 * disassembly has `MOV byte ptr [ESI+4], CL` — param_3 is stored at byte
 * offset +4, not at dword-slot +1 (+4). The decompiler rendered this as
 * `*(undefined1*)(param_1+1)` due to how it assigns dword-indexed fields.
 * The raw byte offset is +4.
 */
void FUN_0005dfc0(void *param_1, uint32_t param_2, uint8_t param_3, int param_4)
{
  csmemset(param_1, 0, 0x48);
  *(uint32_t *)param_1 = param_2;
  *(uint8_t *)((char *)param_1 + 4) = param_3;
  *(int *)((char *)param_1 + 8) = param_4;
  *(int *)((char *)param_1 + 0xc) = -1;
  return;
}

/* 0x005dff0 — path_state_set_ignore_object
 * Sets the ignore-object handle in a path_state record.
 *
 * Disassembly: MOV EAX,[EBP+0xc]; MOV ECX,[EBP+0x8]; MOV [ECX+0xc],EAX; RET
 */
void FUN_0005dff0(void *param_1, int param_2)
{
  *(int *)((char *)param_1 + 0xc) = param_2;
}

/* 0x005e000 — path_state_set_focus
 * Sets the focus-position fields in a path_state record.
 *
 * Disassembly-confirmed stores (EAX = param_1):
 *   [EAX+0x10] = 1  (focus_valid flag, uint8_t)
 *   [EAX+0x14] = param_2[0]  (focus_pos.x)
 *   [EAX+0x18] = param_2[1]  (focus_pos.y)
 *   [EAX+0x1c] = param_2[2]  (focus_pos.z)
 *   [EAX+0x20] = param_3  (bone index)
 */
void FUN_0005e000(void *param_1, float *param_2, int param_3)
{
  *(uint8_t *)((char *)param_1 + 0x10) = 1;
  *(float *)((char *)param_1 + 0x14) = param_2[0];
  *(float *)((char *)param_1 + 0x18) = param_2[1];
  *(float *)((char *)param_1 + 0x1c) = param_2[2];
  *(int *)((char *)param_1 + 0x20) = param_3;
  return;
}

/* 0x005e030 — path_state_set_sphere
 * Sets the sphere-obstacle fields in a path_state record.
 *
 * Disassembly-confirmed stores (EAX = param_1, ECX/EDX = params 3/4/5):
 *   [EAX+0x24] = 1       (sphere_valid flag, uint8_t)
 *   [EAX+0x28] = param_2[0]  (sphere_pos.x)
 *   [EAX+0x2c] = param_2[1]  (sphere_pos.y)
 *   [EAX+0x30] = param_2[2]  (sphere_pos.z)
 *   [EAX+0x34] = param_4  (flags — note: [EBP+0x14] stored at +0x34, NOT +0x38)
 *   [EAX+0x38] = param_3  (inner_r — note: [EBP+0x10] stored at +0x38, NOT
 * +0x34) [EAX+0x3c] = param_5  (outer_r)
 *
 * Store rotation confirmed: MSVC emitted param_3 → +0x38, param_4 → +0x34
 * (pipeline-scheduled out-of-order). See disassembly:
 *   MOV [EAX+0x38], ECX   ; ECX = [EBP+0x10] = param_3
 *   MOV [EAX+0x34], EDX   ; EDX = [EBP+0x14] = param_4
 */
void FUN_0005e030(void *param_1, float *param_2, float param_3,
                  uint32_t param_4, float param_5)
{
  *(uint8_t *)((char *)param_1 + 0x24) = 1;
  *(float *)((char *)param_1 + 0x28) = param_2[0];
  *(float *)((char *)param_1 + 0x2c) = param_2[1];
  *(float *)((char *)param_1 + 0x30) = param_2[2];
  *(float *)((char *)param_1 + 0x38) = param_3;
  *(uint32_t *)((char *)param_1 + 0x34) = param_4;
  *(float *)((char *)param_1 + 0x3c) = param_5;
  return;
}

/* 0x005e070 — path_state_set_min_speed
 * Sets the minimum-speed constraint fields in a path_state record.
 *
 * Disassembly-confirmed stores (EAX = param_1):
 *   [EAX+0x40] = 1       (min_speed_valid flag, uint8_t)
 *   [EAX+0x44] = param_2 (min_speed, int)
 */
void FUN_0005e070(void *param_1, int param_2)
{
  *(uint8_t *)((char *)param_1 + 0x40) = 1;
  *(int *)((char *)param_1 + 0x44) = param_2;
  return;
}

/* 0x005e090 — path_state_commit
 * Zero-fills a 0x1408c-byte result buffer, then copies 0x48 bytes (0x12 dwords)
 * from the path_state (param_1) into it, stores the current scenario handle at
 * result+0x64, and writes the cam_ref handle at result+0x48.
 *
 * Disassembly-confirmed (EBX = param_2 = result buffer):
 *   csmemset(param_2, 0, 0x1408c)
 *   param_2[0x19] = scenario_get()   ; at byte offset 0x64 (0x19 * 4)
 *   MOVSD.REP ECX=0x12: copy param_1[0..0x47] → param_2[0..0x47]
 *   param_2[0x12] = param_3          ; at byte offset 0x48 (0x12 * 4)
 *
 * Note: scenario handle stored at +0x64, not +0x19 (raw dword index is 0x19).
 * The copy overwrites param_2[0..0x47], then param_3 is stored at
 * param_2[0x48].
 */
void FUN_0005e090(void *param_1, void *param_2, void *param_3)
{
  csmemset(param_2, 0, 0x1408c);
  *(void **)((char *)param_2 + 0x64) = scenario_get();
  /* Copy 0x48 bytes from param_1 into param_2 at offset 0 (MOVSD.REP ECX=0x12)
   */
  csmemcpy(param_2, param_1, 0x48);
  *(void **)((char *)param_2 + 0x48) = param_3;
  return;
}

/* 0x005e0d0 — path_state_set_obstacle
 * Sets an obstacle hit record in a path_state.
 *
 * Disassembly-confirmed stores (EAX = param_1):
 *   [EAX+0x4c] = 1       (obstacle_valid flag, uint8_t)
 *   [EAX+0x50] = param_2[0]  (hit_pos.x)
 *   [EAX+0x54] = param_2[1]  (hit_pos.y)
 *   [EAX+0x58] = param_2[2]  (hit_pos.z)
 *   [EAX+0x5c] = param_3  (hit_flags)
 *   [EAX+0x60] = param_4  (mask)
 */
void FUN_0005e0d0(void *param_1, float *param_2, int param_3, int param_4)
{
  *(uint8_t *)((char *)param_1 + 0x4c) = 1;
  *(float *)((char *)param_1 + 0x50) = param_2[0];
  *(float *)((char *)param_1 + 0x54) = param_2[1];
  *(float *)((char *)param_1 + 0x58) = param_2[2];
  *(int *)((char *)param_1 + 0x5c) = param_3;
  *(int *)((char *)param_1 + 0x60) = param_4;
  return;
}
