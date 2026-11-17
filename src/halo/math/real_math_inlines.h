/* Inline vector helpers in the style of real_math.h, used by the camera.
 * They live apart from real_math.h because adding unused inline functions
 * there shifts VC71 codegen in translation units that include it. */

#ifndef HALO_MATH_REAL_MATH_INLINES_H
#define HALO_MATH_REAL_MATH_INLINES_H

#include "real_math.h"

/* Statement macros rather than inline functions: the original stores each
 * component as it is computed. */
#define set_real_point3d_inline(point, x_value, y_value, z_value) \
  do {                                                            \
    (point)->x = (x_value);                                       \
    (point)->y = (y_value);                                       \
    (point)->z = (z_value);                                       \
  } while (0)

#define set_real_vector3d_inline(vector, i_value, j_value, k_value) \
  do {                                                              \
    (vector)->i = (i_value);                                        \
    (vector)->j = (j_value);                                        \
    (vector)->k = (k_value);                                        \
  } while (0)

/* Normalizes v in place and returns its former magnitude, or 0 when it is
 * too short to normalize.  The inlined copy of normalize3d. */
static __inline real normalize3d_inline(real_vector3d *v)
{
  real magnitude = sqrtf(magnitude_squared3d_inline(v));

  if (fabs(magnitude) >= 0.0001f) {
    scale_vector3d_inline(v, 1.0f / magnitude, v);
  } else {
    magnitude = 0.0f;
  }
  return magnitude;
}

/* result = p + t * v. */
static __inline real_point3d *point_from_line3d(real_point3d const *p,
                                                real_vector3d const *v, real t,
                                                real_point3d *result)
{
  result->x = v->i * t + p->x;
  result->y = v->j * t + p->y;
  result->z = v->k * t + p->z;
  return result;
}

/* result = p + v. */
static __inline real_point3d *point_plus_vector3d(real_point3d const *p,
                                                  real_vector3d const *v,
                                                  real_point3d *result)
{
  result->x = p->x + v->i;
  result->y = p->y + v->j;
  result->z = p->z + v->k;
  return result;
}

/* result = p - t * v.  A macro: the original re-reads t for each
 * component. */
#define point_from_line_back3d(p, v, t, result) \
  do {                                          \
    (result)->x = (p)->x - (t) * (v)->i;        \
    (result)->y = (p)->y - (t) * (v)->j;        \
    (result)->z = (p)->z - (t) * (v)->k;        \
  } while (0)

/* result = point - distance * vector.  A macro rather than an inline
 * function: the original re-evaluates the distance expression for each
 * component. */
#define point_behind3d(point, vector, distance, result) \
  do {                                                  \
    real_vector3d offset;                               \
                                                        \
    offset.i = -(vector)->i * (distance);               \
    offset.j = -(vector)->j * (distance);               \
    offset.k = -(vector)->k * (distance);               \
    (result)->x = offset.i + (point)->x;                \
    (result)->y = offset.j + (point)->y;                \
    (result)->z = offset.k + (point)->z;                \
  } while (0)

/* result = a x b.  All three components are computed before any is stored,
 * so result may alias a or b. */
static __inline real_vector3d *cross_product3d_inline(real_vector3d const *a,
                                                      real_vector3d const *b,
                                                      real_vector3d *result)
{
  real k = a->i * b->j - a->j * b->i;
  real j = a->k * b->i - a->i * b->k;
  real i = a->j * b->k - a->k * b->j;

  result->i = i;
  result->j = j;
  result->k = k;
  return result;
}

#endif /* HALO_MATH_REAL_MATH_INLINES_H */
