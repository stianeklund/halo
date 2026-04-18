/* Normalize a 3D vector in-place.
 * Computes the magnitude (Euclidean length) of v, and if it exceeds a
 * small epsilon threshold (~0.0001), divides each component by the
 * magnitude so v becomes a unit vector. Returns the original magnitude,
 * or 0.0f if the vector was too small to normalize. */
float normalize3d(float *v)
{
  float mag;
  float scale;

  mag = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (fabsf(mag) >= *(double *)0x2533d0) {
    scale = 1.0f / mag;
    v[0] = v[0] * scale;
    v[1] = v[1] * scale;
    v[2] = v[2] * scale;
    return mag;
  }
  return 0.0f;
}
