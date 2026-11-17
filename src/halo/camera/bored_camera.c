/* bored_camera.c
 * Original source: c:\halo\SOURCE\camera\bored_camera.c */

/* 0x849f0 */
float real_local_random_range(float lower_bound, float upper_bound)
{
  return random_real_range(random_math_get_local_seed_address(),
                           lower_bound, upper_bound);
}
