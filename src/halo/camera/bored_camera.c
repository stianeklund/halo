/* bored_camera.c — Idle/bored camera animation helpers.
 * Object: bored_camera.obj
 * Original source: c:\halo\SOURCE\camera\bored_camera.c */

/* FUN_000849f0 (0x849f0) — Random float in range using the local random seed.
 * Convenience wrapper that fetches the local seed and calls random_real_range. */
float FUN_000849f0(float min, float max)
{
  return random_real_range((int *)random_math_get_local_seed_address(), min, max);
}
