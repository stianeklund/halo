/* bored_camera.c — Idle/bored camera animation helpers.
 * Object: bored_camera.obj
 * Original source: c:\halo\SOURCE\camera\bored_camera.c */

/* FUN_000849f0 (0x849f0) — Random float in range using the local random seed.
 * Convenience wrapper that fetches the local seed and calls random_real_range. */
float FUN_000849f0(float min, float max)
{
  return random_real_range((int *)random_math_get_local_seed_address(), min, max);
}

/* FUN_00084fe0 (0x84fe0) — Set the bored-camera enable flag and mark the
 * camera state dirty so it will be re-evaluated this tick.
 * Object: objects.obj / source: bored_camera.c
 *
 * Confirmed: MOV AL,[param_1]; MOV [0x2ee5a0],AL; MOV byte ptr [0x2ee5a1],1.
 */
void FUN_00084fe0(unsigned char param_1)
{
  *(char *)0x2ee5a0 = param_1;
  *(char *)0x2ee5a1 = 1;
}

/* FUN_000850d0 (0x850d0) — Switch to first-person camera mode 2 for the
 * given unit handle, or report an error if the handle is -1.
 * Object: objects.obj / source: bored_camera.c
 *
 * Confirmed: CMP [param_1],-1; JE error_path; MOV [0x2ee5a2],2;
 * MOV [0x2ee5a1],1; MOV [0x2ee5d4],param_1; RET.
 */
void FUN_000850d0(int param_1)
{
  if (param_1 != -1) {
    *(char *)0x2ee5a2 = 2;
    *(char *)0x2ee5a1 = 1;
    *(int *)0x2ee5d4 = param_1;
    return;
  }
  error(2, "cannot set first person camera on a unit that doesn't exist.");
}

/* FUN_00085110 (0x85110) — Switch to first-person camera mode 3 for the
 * given unit handle, or report an error if the handle is -1.
 * Object: objects.obj / source: bored_camera.c
 *
 * Confirmed: identical to FUN_000850d0 but stores 3 in DAT_002ee5a2.
 */
void FUN_00085110(int param_1)
{
  if (param_1 != -1) {
    *(char *)0x2ee5a2 = 3;
    *(char *)0x2ee5a1 = 1;
    *(int *)0x2ee5d4 = param_1;
    return;
  }
  error(2, "cannot set first person camera on a unit that doesn't exist.");
}

/* FUN_00085150 (0x85150) — Check if first-person camera mode 2 is active for
 * the given unit handle. Returns 1 if the bored-camera is enabled, mode is 2,
 * and the stored unit handle matches param_1; otherwise 0. */
int FUN_00085150(int param_1)
{
  if (*(char *)0x2ee5a0 != '\0' &&
      *(short *)0x2ee5a2 == 2 &&
      *(int *)0x2ee5d4 == param_1) {
    return 1;
  }
  return 0;
}

