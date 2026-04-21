/* Compute the cross product of two 3D vectors.
 *
 * out = a × b
 *
 * Confirmed: pure x87 arithmetic, no assertions, no globals.
 * Confirmed: FLD [ECX] / FMUL [EAX+0x4] / FLD [ECX+0x4] / FMUL [EAX] / FSUBP
 *   computes out[2] = a[0]*b[1] - a[1]*b[0] (z component).
 * Confirmed: three-component store at [EAX], [EAX+0x4], [EAX+0x8].
 */
void cross_product3d(float *a, float *b, float *out)
{
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}
