/* Render camera utilities. */

/* Compute the adjusted FOV tangent for the render camera.
 * Uses FPTAN: tan(fov * half_constant) * aspect_ratio */
double render_camera_get_adjusted_field_of_view_tangent(float fov)
{
  double result;
  asm volatile("flds %[f]\n\t"
               "fmuls 0x253398\n\t"
               "fptan\n\t"
               "fstp %%st(0)\n\t"
               "fmuls 0x2b1504"
               : "=t"(result)
               : [f] "m"(fov)
               : "memory");
  return result;
}
