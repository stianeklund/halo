/* TU: c:\halo\SOURCE\objects\widgets\lightning.c
 *
 * Lightning widget marker helpers. */

/* lightning_offset_marker_position — perturb a marker position by a random
 * offset drawn within random_position_bounds, transform that offset into the
 * marker's local frame via matrix_scale_transform_vector, then add it to the
 * base position (in place).
 *
 * Register-arg ABI (verified against disasm prologue: TEST ESI @135426,
 * TEST EBX @135447, TEST EDI @135468):
 *   matrix_ptr    @ebx  (int handle; passed as float* to the transform)
 *   position_out  @esi  (float* in/out, xyz)
 *   random_bounds @edi  (float* xyz half-extents)
 *
 * FPU fidelity notes:
 *   - The three random draws feed the offset components in REVERSED order:
 *     3rd draw -> comp0, 2nd draw -> comp1, 1st draw -> comp2.  The 3rd draw
 *     is kept live in ST0 (not spilled) and consumed by comp0 first.
 *   - Each component is ((r + r) - 1.0f) * bound[i]; the (r + r) form matches
 *     FADD ST,ST (not a multiply) and 1.0f is _DAT_002533c8 (read_memory
 *     confirmed = 0x3f800000).
 *   - The transform is called in place (in == out == offset); do NOT split
 *     into a separate output buffer.
 * Assert-tail flavor: display_assert(...) then system_exit(-1). */
void lightning_offset_marker_position(int matrix_ptr /*@ebx*/,
                                      float *position_out /*@esi*/,
                                      float *random_bounds /*@edi*/)
{
  unsigned int *seed;
  float draw1;
  float draw2;
  float draw3;
  float offset[3];

  if (position_out == (float *)0) {
    display_assert("position",
                   "c:\\halo\\SOURCE\\objects\\widgets\\lightning.c", 0x74, 1);
    system_exit(-1);
  }
  if (matrix_ptr == 0) {
    display_assert("matrix",
                   "c:\\halo\\SOURCE\\objects\\widgets\\lightning.c", 0x75, 1);
    system_exit(-1);
  }
  if (random_bounds == (float *)0) {
    display_assert("random_position_bounds",
                   "c:\\halo\\SOURCE\\objects\\widgets\\lightning.c", 0x76, 1);
    system_exit(-1);
  }

  seed = random_math_get_local_seed_address();
  draw1 = random_math_real(seed);
  seed = random_math_get_local_seed_address();
  draw2 = random_math_real(seed);
  seed = random_math_get_local_seed_address();
  draw3 = random_math_real(seed);

  offset[0] = ((draw3 + draw3) - 1.0f) * random_bounds[0];
  offset[1] = ((draw2 + draw2) - 1.0f) * random_bounds[1];
  offset[2] = ((draw1 + draw1) - 1.0f) * random_bounds[2];

  matrix_scale_transform_vector((float *)matrix_ptr, offset, offset);

  position_out[0] = offset[0] + position_out[0];
  position_out[1] = offset[1] + position_out[1];
  position_out[2] = offset[2] + position_out[2];
}
