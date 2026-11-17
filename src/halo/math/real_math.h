/* c:\halo\source\math\real_math.h
 *
 * Header path recovered from the XBE (the string "..\math\real_math.h" is the
 * __FILE__ of asserts in this header's inline functions).  Only the inline
 * helpers a lifted caller needs are recovered here; each is inlined by the
 * original at its call sites, so a lift that calls it reproduces the original
 * instruction schedule.  Names: T2. */

#ifndef HALO_MATH_REAL_MATH_H
#define HALO_MATH_REAL_MATH_H

#include "../../types.h"

/* Exponent-not-all-ones test on the float's bits.  Inlined in the camera
 * command validators (observer_set_camera, first_person_camera_for_unit_and_
 * vector): the by-value float is stored to a stack slot and reloaded as an
 * integer before the AND/CMP 0x7f800000. */
static __inline boolean valid_real(real n)
{
  return (*(long *)&n & 0x7f800000) != 0x7f800000;
}

/* The IEEE-754 bit view of a float, as assert_valid_real prints it. */
typedef union {
  real value;
  uint32_t bits;
} real_bits;

static __inline const real_bits *real_bits_of(const void *value)
{
  return value;
}

/* result = b - a.  Inlined in lightning_submit (0x135818, 0x135c6f). */
static __inline real_vector3d *vector_from_points3d(real_point3d const *a,
                                                    real_point3d const *b,
                                                    real_vector3d *result)
{
  result->i = b->x - a->x;
  result->j = b->y - a->y;
  result->k = b->z - a->z;
  return result;
}

/* result = c * a.  Inlined in lightning_submit (0x1359c4). */
static __inline real_vector3d *scale_vector3d(real_vector3d const *a, real c,
                                              real_vector3d *result)
{
  result->i = c * a->i;
  result->j = c * a->j;
  result->k = c * a->k;
  return result;
}

/* |v|^2.  Inlined in ai_communication_get_player_rating. */
static __inline real magnitude_squared3d(real_vector3d const *v)
{
  return v->i * v->i + v->j * v->j + v->k * v->k;
}

/* a . b.  Inlined in ai_communication_get_player_rating. */
static __inline real dot_product3d(real_vector3d const *a,
                                   real_vector3d const *b)
{
  return a->i * b->i + a->j * b->j + a->k * b->k;
}

/* The constant vectors the engine reaches through pointer globals. */
#define global_zero_vector3d ((const real_vector3d *)global_zero_vector_ptr)
#define global_forward_vector3d ((const real_vector3d *)global_forward_vector_ptr)
#define global_up_vector3d ((const real_vector3d *)global_up_vector_ptr)

#endif /* HALO_MATH_REAL_MATH_H */
