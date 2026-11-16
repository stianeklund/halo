#include "x87_math.h"
#include "halo/objects/objects.h"
#ifdef HALO_RNG_TRACE
#include "halo/math/rng_trace.h"
/* Mirrors the host LOS-exit detour for calls returning into projectile_collision_test_line:
 * kind 33 with value bit 31 set (host sets it when the return address is
 * below 0x138900). */
static bool sweep_los_trace(bool result, const collision_test_result *collision)
{
  RNG_TRACE_EX(RNG_TRACE_KIND_LOS_RESULT,
               0x80000000u | ((unsigned int)(unsigned char)result << 16) |
                 (unsigned short)collision->field_00,
               RNG_TRACE_BITS(collision->t));
  return result;
}
#define SWEEP_LOS(call) \
  sweep_los_trace((call), (const collision_test_result *)collision_result)
#else
#define SWEEP_LOS(call) (call)
#endif

/* The empty hooks below are slots of the projectile object type definition
 * (0x324108); each compiles to a lone RET. */
void projectiles_initialize(void)
{
}

void projectiles_initialize_for_new_map(void)
{
}

void projectiles_dispose_from_old_map(void)
{
}

void projectile_kill_tracer(int projectile_handle)
{
  projectile_data_t *projectile = projectile_get(projectile_handle);
  projectile->flags &= ~FLAG(_projectile_tracer_bit);
}

void projectiles_delete_all(void)
{
  object_iter_t iter;

  object_iterator_new(&iter, _object_mask_projectile, 0);
  while (object_iterator_next(&iter) != NULL) {
    object_delete(iter.last_handle);
  }
}

void projectile_delete(int projectile_handle)
{
}

void projectile_set_target_object_index(int projectile_handle, int target)
{
  projectile_data_t *projectile = projectile_get(projectile_handle);
  projectile->target_object_index = target;
}

void projectile_make_tracer(int projectile_handle)
{
  projectile_data_t *projectile = projectile_get(projectile_handle);
  projectile->flags |= FLAG(_projectile_tracer_bit);
}

float projectile_get_ballistic_acceleration(int projectile_tag)
{
  projectile_definition_t *definition = (projectile_definition_t *)projectile_tag;
  return -(global_gravity * definition->air_gravity_scale);
}

float projectile_estimate_time_to_target(void *proj_tag, float target_distance)
{
  projectile_definition_t *definition = (projectile_definition_t *)proj_tag;
  float result;

  result = REAL_ZERO_POOL;
  if (definition->initial_velocity > REAL_ZERO_POOL) {
    result = target_distance / definition->initial_velocity;
  }

  return result;
}

/* Returns true while any projectile exists; the definition lookup of the
 * first one is kept although its result is unused. */
bool dangerous_projectiles_near_player(void)
{
  object_iter_t iter;
  projectile_data_t *projectile;

  object_iterator_new(&iter, _object_mask_projectile, 0);
  projectile = object_iterator_next(&iter);
  if (projectile != NULL) {
    projectile_definition_get(projectile->object.tag_index);
    return true;
  }
  return false;
}

void projectile_handle_deleted_object(int projectile_handle, int object_index)
{
  projectile_data_t *projectile = projectile_get(projectile_handle);
  if (projectile->target_object_index == object_index)
    projectile->target_object_index = NONE;
}

/* The action only ever escalates (0 none, 1 pending, 2 immediate). */
void projectile_set_action(int projectile_handle, int16_t action)
{
  projectile_data_t *projectile = projectile_get(projectile_handle);
  if (action > projectile->action)
    projectile->action = action;
}

/* Arguments arrive in registers: esi=effect_tag_index, edi=object_index,
 * eax=collision, edx=marker_points, ecx=marker_forwards. */
void projectile_effect_new(int effect_tag_index, int object_index, void *tag_def,
                  float *marker_points, float *marker_forwards, float scale,
                  float material_effect_scale)
{
  collision_test_result *collision = (collision_test_result *)tag_def;
  int object_hit;
  uint16_t node_index;

  if (collision->field_00 == _collision_result_object) {
    object_hit = collision->field_38;
    node_index = (uint16_t)collision->field_3e;
    effect_new_attached_from_markers(
      effect_tag_index, object_index, object_hit, node_index,
      NUMBER_OF_EFFECT_MARKERS, effect_marker_names, marker_points,
      marker_forwards, scale, material_effect_scale, 0.0f, 0.0f);
  } else {
    effect_new_unattached_from_markers(
      effect_tag_index, object_index, NULL, NUMBER_OF_EFFECT_MARKERS,
      effect_marker_names, marker_points, marker_forwards, scale,
      material_effect_scale, 0.0f, 0.0f, true);
  }
}

enum {
  _projectile_export_function_none = 0,
  _projectile_export_function_range_remaining,
  _projectile_export_function_time_remaining,
  _projectile_export_function_tracer
};

void projectile_export_function_values(int projectile_handle)
{
  projectile_data_t *projectile;
  projectile_definition_t *definition;
  int16_t *function_input;
  real *function_value;
  real value;
  int16_t input;
  int function_count;

  projectile = projectile_get(projectile_handle);
  definition = projectile_definition_get(projectile->object.tag_index);

  function_input = definition->function_inputs;
  function_value = object_incoming_function_values(&projectile->object);
  function_count = NUMBER_OF_INCOMING_OBJECT_FUNCTIONS;

  do {
    input = *function_input;
    if (input != _projectile_export_function_none) {
      switch (input) {
      case _projectile_export_function_range_remaining:
        if (definition->maximum_range != 0.0f) {
          value = projectile->odometer / definition->maximum_range;
        } else {
          value = 0.0f;
        }
        break;
      case _projectile_export_function_time_remaining:
        value = projectile->detonation_timer;
        break;
      case _projectile_export_function_tracer:
        if (projectile->flags & FLAG(_projectile_tracer_bit)) {
          value = 1.0f;
        } else {
          value = 0.0f;
        }
        break;
      default:
        display_assert(0, "c:\\halo\\SOURCE\\items\\projectiles.c", 0x622, 1);
        system_exit(-1);
        break;
      }
      *function_value = value;
    }
    function_input++;
    function_value++;
    function_count--;
  } while (function_count != 0);
}

/* Constant deceleration that slows initial_velocity to final_velocity over
 * the distance (range_end - range_begin): (v0^2 - v1^2) / (2 * distance). */
float projectile_calculate_deceleration_from_distances(void *tag, float range_begin, float range_end)
{
  projectile_definition_t *definition = (projectile_definition_t *)tag;

  range_end -= range_begin;
  /* 0xf7fa9: FSTP dword [ebp+0xc] -- the width is narrowed to float32
   * before the zero test and the 2*width divisor. */
  HALO_FLT_ROUNDTRIP(range_end);
  if (definition->initial_velocity == definition->final_velocity || range_end == REAL_ZERO_POOL)
    return REAL_ZERO_POOL;
  return (definition->initial_velocity * definition->initial_velocity -
          definition->final_velocity * definition->final_velocity) / (range_end + range_end);
}

char projectile_handle_parent_destroyed(int projectile_handle)
{
  projectile_data_t *projectile;

  projectile = projectile_get(projectile_handle);
  if (projectile->object.parent_object_index.value == NONE) {
    display_assert("projectile->object.parent_object_index != NONE",
                   "c:\\halo\\SOURCE\\items\\projectiles.c", 0x735, 1);
    system_exit(-1);
  }
  projectile->arming_time = 1.0f;
  projectile->detonation_timer = 1.0f;
  projectile->flags &= ~FLAG(_projectile_attached_bit);
  object_detach_from_parent(projectile_handle);
  return true;
}

void random_vector_in_cone3d(float *forward, float minimum_angle, float maximum_angle,
                             float *result)
{
  seed_random_vector_in_cone3d(get_global_random_seed_address(), forward, minimum_angle,
                               maximum_angle, result);
}

/* Solves a*t^4 + (acceleration_height - v^2)*t^2 + c = 0 (a = acceleration^2/4,
 * c = |delta|^2) for the flight time t, falling back to the minimum-speed arc
 * (t_max, v_min) when there is no solution.  Returns false on the fallback paths.
 * param_5 is never read. */
char projectile_aim_ballistic(float speed, float gravity_scale, float *origin,
                              float *target, int param_5, float *ballistic_fraction,
                              float *forced_velocity, char lob, float *result_aim_vector,
                              float *result_velocity, float *result_ticks,
                              float *result_distance, float *result_vertical_velocity,
                              float *result_horizontal_velocity)
{
  real_vector3d aim_vector;
  /* separate scalars: a real_vector3d delta is laid out at [ebp-0x34] (-3 bytes) */
  float dx;
  float dy;
  float dz;
  float t_max;
  float two_a;
  float c;
  float b;
  float v_min;
  float four_a_c;
  float acceleration_height;
  float acceleration_height_wide; /* 0xf81ad: FST, the subtract uses the wide copy */
  float inverse_t; /* 0xf82ea: 1/t stays in ST(0), never stored */
  /* volatile: without it the byte flag at [ebp-1] is enregistered (-20 bytes) */
  volatile char result;
  bool solution;
  float a;
  float acceleration;
  /* shares t_squared_max's slot; a separate local costs 10 bytes */
  float v_desired;
  float distance;
  /* scratch shared by several roles; splitting them into named locals costs bytes */
  float temp;
  float temp2;
  float temp_wide; /* 0xf8272: FST, the square uses the wide copy */
  float horizontal_distance_squared;

  solution = false;
  dx = target[0] - origin[0];
  /* 0xf80b4: FSTP dword [ebp-0x28] -- narrowed before the reload at 0xf80cf. */
  HALO_FLT_ROUNDTRIP(dx);
  result = true;
  dy = target[1] - origin[1];
  /* 0xf80bd: FSTP dword [ebp-0x24] -- narrowed before the reload at 0xf80c9. */
  HALO_FLT_ROUNDTRIP(dy);
  dz = target[2] - origin[2];
  /* 0xf80c6: FSTP dword [ebp-0x20] -- narrowed before the reloads at
   * 0xf810c / 0xf81aa / 0xf8303. */
  HALO_FLT_ROUNDTRIP(dz);

  /* Stays on the FPU stack while acceleration and a are computed. */
  horizontal_distance_squared = dy * dy + dx * dx;

  acceleration = global_gravity * gravity_scale;
  if (acceleration < REAL_ZERO_POOL) {
    acceleration = 0.0f;
  }
  /* 0xf80fa: FSTP dword [ebp+0x10] -- the clamp compares the wide stack copy
   * (FCOMP at 0xf80e6), then stores the float32 the rest of the function
   * reloads (0xf80fd, 0xf8309).  The zero path stores an integer 0. */
  HALO_FLT_ROUNDTRIP(acceleration);

  temp = acceleration * acceleration;
  a = temp * REAL_0_25_POOL;
  /* 0xf8109: FSTP dword [ebp+0xc] -- narrowed before the reloads at 0xf815f
   * (two_a) and 0xf820a.  temp itself is never stored. */
  HALO_FLT_ROUNDTRIP(a);

  c = dz * dz + horizontal_distance_squared;
  /* 0xf8114: FSTP dword [ebp-0x14] -- narrowed before the reloads at 0xf8119
   * and 0xf8203. */
  HALO_FLT_ROUNDTRIP(c);

  temp = c * a;
  four_a_c = temp * REAL_4_0_POOL;
  if (!(four_a_c > REAL_ZERO_POOL)) {
    display_assert("4.0f * a * c > 0.0f",
                   "c:\\halo\\SOURCE\\items\\projectiles.c", 0x2f8, 1);
    system_exit(-1);
  }
  /* 0xf8125: FST dword [ebp-0xc] -- no pop, so the assert compare at 0xf8128
   * still sees the wide stack copy; the FSQRT at 0xf8155 and the subtract at
   * 0xf8278 reload the narrowed slot. */
  HALO_FLT_ROUNDTRIP(four_a_c);

  b = -sqrtf(four_a_c);
  /* 0xf815c: FSTP dword [ebp-0x10] -- narrowed before the divide at 0xf816a. */
  HALO_FLT_ROUNDTRIP(b);
  two_a = a + a;

  v_desired = -b / two_a;
  if (!(v_desired >= REAL_ZERO_POOL)) {
    display_assert("t_squared_max >= 0.0f",
                   "c:\\halo\\SOURCE\\items\\projectiles.c", 0x2fc, 1);
    system_exit(-1);
  }
  /* 0xf816f: FST dword [ebp+0x14] -- the compare above sees the wide copy,
   * the FSQRT at 0xf819f reloads the narrowed slot. */
  HALO_FLT_ROUNDTRIP(v_desired);
  t_max = sqrtf(v_desired); /* v_desired holds t_squared_max here */
  /* 0xf81a4: FSTP dword [ebp-0x1c] -- narrowed before the reload at 0xf81fa. */
  HALO_FLT_ROUNDTRIP(t_max);

  /* 0xf81ad: FST dword [ebp-8] -- no pop.  The subtract at 0xf81b0 uses the
   * wide copy; the reloads at 0xf8211 and 0xf826f use the narrowed slot. */
  acceleration_height_wide = acceleration * dz;
  acceleration_height = acceleration_height_wide;
  HALO_FLT_ROUNDTRIP(acceleration_height);
  if (acceleration_height_wide - b < REAL_ZERO_POOL) {
    v_min = 0.0f;
  } else {
    v_min = sqrtf(acceleration_height_wide - b);
  }

  if (forced_velocity != NULL) {
    v_desired = *forced_velocity;
  } else {
    v_desired = speed;
    if ((ballistic_fraction != NULL) && (*ballistic_fraction > REAL_ZERO_POOL)) {
      temp2 = t_max * *ballistic_fraction;
      temp2 = temp2 * temp2;
      temp = acceleration_height - -(temp2 * a + c / temp2);
      if (!(temp > REAL_ZERO_POOL)) {
        display_assert("v_desired_sq > 0.0f",
                       "c:\\halo\\SOURCE\\items\\projectiles.c", 0x326, 1);
        system_exit(-1);
      }
      temp = sqrtf(temp);
      if (speed > temp) {
        v_desired = temp;
      }
    }
  }

  if (v_desired >= v_min) {
    temp = acceleration_height - v_desired * v_desired;
    /* 0xf8272: FST dword [ebp+8] -- no pop.  The square at 0xf8275 multiplies
     * the wide stack copy by the narrowed slot, so keep one wide copy; every
     * later read (compare 0xf827e, subtract 0xf82b7) uses the narrowed one. */
    temp_wide = temp;
    HALO_FLT_ROUNDTRIP(temp);
    temp2 = temp_wide * temp - four_a_c;
    /* 0xf827b: FSTP dword [ebp+0xc] -- narrowed before the compare at 0xf828e
     * and the FSQRT at 0xf829e. */
    HALO_FLT_ROUNDTRIP(temp2);
    if ((temp < REAL_ZERO_POOL) && (temp2 >= REAL_ZERO_POOL)) {
      /* speed's slot holds t from here on; a separate local costs 6 bytes */
      speed = (sqrtf(temp2) * (lob ? 1 : -1) - temp) / two_a;
      if (REAL_ZERO_POOL < speed) {
        speed = sqrtf(speed);
        /* 0xf82cc: FSTP dword [ebp+8] -- the quotient itself is never stored
         * (the compare at 0xf82bd uses the wide stack copy); only t is
         * narrowed, and the output block reloads it at 0xf82ea/0xf8306/0xf832d. */
        HALO_FLT_ROUNDTRIP(speed);
        solution = true;
      }
    }
  }
  if (!solution) {
    result = false;
    speed = t_max;
    v_desired = v_min;
  }

  inverse_t = REAL_ONE_POOL / speed;
  aim_vector.i = dx * inverse_t;
  /* 0xf82f8: FSTP dword [ebp-0x34] -- narrowed before the reload at 0xf831d. */
  HALO_FLT_ROUNDTRIP(aim_vector.i);
  aim_vector.j = dy * inverse_t;
  /* 0xf8300: FSTP dword [ebp-0x30] -- narrowed before the reload at 0xf8317.
   * aim_vector.k is FST (0xf8314), so its wide stack copy is what
   * result_vertical_velocity receives at 0xf832a; only the slot copy is
   * narrowed and it is not round-tripped. */
  HALO_FLT_ROUNDTRIP(aim_vector.j);
  aim_vector.k = inverse_t * dz + speed * acceleration * REAL_0_5_POOL;

  temp2 = aim_vector.j * aim_vector.j + aim_vector.i * aim_vector.i;
  temp2 = sqrtf(temp2); /* horizontal velocity */
  temp = aim_vector.k; /* vertical velocity */
  distance = speed * v_desired;

  if (normalize3d((float *)&aim_vector) == REAL_ZERO_POOL) {
    aim_vector.i = dx;
    result = false;
    aim_vector.j = dy;
    aim_vector.k = dz;
    if (normalize3d((float *)&aim_vector) == REAL_ZERO_POOL) {
      aim_vector = *(real_vector3d *)global_up_vector_ptr;
    }
  }

  if (result_aim_vector == NULL) {
    display_assert("result_aim_vector",
                   "c:\\halo\\SOURCE\\items\\projectiles.c", 0x363, 1);
    system_exit(-1);
  }

  *(real_vector3d *)result_aim_vector = aim_vector;

  if (result_distance != NULL) {
    *result_distance = distance;
  }
  if (result_velocity != NULL) {
    *result_velocity = v_desired;
  }
  if (result_vertical_velocity != NULL) {
    *result_vertical_velocity = temp;
  }
  if (result_horizontal_velocity != NULL) {
    *result_horizontal_velocity = temp2;
  }
  if (result_ticks != NULL) {
    *result_ticks = speed;
  }
  return result;
}

bool projectile_aim_linear(float speed, float *origin, float *target,
                           float *result_aim_vector, float *result_velocity,
                           float *result_ticks, float *result_distance)
{
  real_vector3d aim_vector;
  float distance;
  float ticks;

  aim_vector.i = target[0] - origin[0];
  aim_vector.j = target[1] - origin[1];
  aim_vector.k = target[2] - origin[2];

  distance = normalize3d((float *)&aim_vector);
  /* 0xf843e: FSTP dword [ebp+0x10] -- the magnitude is narrowed before the
   * divide at 0xf8457 and before it is copied to result_distance at 0xf84a7. */
  HALO_FLT_ROUNDTRIP(distance);

  if (speed > REAL_ZERO_POOL) {
    ticks = distance / speed;
  } else {
    ticks = 0.0f;
  }

  if (result_aim_vector == NULL) {
    display_assert("result_aim_vector",
                   "c:\\halo\\SOURCE\\items\\projectiles.c", 0x399, 1);
    system_exit(-1);
  }

  *(real_vector3d *)result_aim_vector = aim_vector;

  if (result_distance != NULL) {
    *result_distance = distance;
  }
  if (result_velocity != NULL) {
    *result_velocity = speed;
  }
  if (result_ticks != NULL) {
    *result_ticks = ticks;
  }

  return true;
}

/* Declared void in the original: AL is whatever the solver left in EAX, and
 * callers such as actor_combat_compute_ballistic_solution test it. */
char projectile_aim(int projectile_tag, int origin, int target, void *velocity_override,
                    int param_5, int ballistic_fraction, int forced_velocity, int lob,
                    int result_aim_vector, int result_velocity, int result_ticks,
                    int result_distance, void *result_linear)
{
  projectile_definition_t *definition = (projectile_definition_t *)projectile_tag;
  float speed;
  char result;

  if (velocity_override == NULL) {
    speed = definition->initial_velocity;
  } else {
    speed = *(float *)velocity_override;
  }

  if ((definition->flags & FLAG(_projectile_definition_aim_ballistic_bit)) &&
      definition->air_gravity_scale > REAL_ZERO_POOL) {
    result = projectile_aim_ballistic(
      speed, definition->air_gravity_scale, (float *)origin, (float *)target, param_5,
      (float *)ballistic_fraction, (float *)forced_velocity, (char)lob,
      (float *)result_aim_vector, (float *)result_velocity, (float *)result_ticks,
      (float *)result_distance, NULL, NULL);
    if (result_linear != NULL) {
      *(char *)result_linear = false;
    }
  } else {
    result = (char)projectile_aim_linear(
      speed, (float *)origin, (float *)target, (float *)result_aim_vector,
      (float *)result_velocity, (float *)result_ticks, (float *)result_distance);
    if (result_linear != NULL) {
      *(char *)result_linear = true;
    }
  }

  return result;
}

void projectile_adjust_for_angular_velocity_change(int projectile_handle)
{
  projectile_data_t *projectile;
  float angular_speed;
  float inverse_angular_speed;

  projectile = projectile_get(projectile_handle);

  angular_speed = sqrtf(projectile->object.angular_velocity.x * projectile->object.angular_velocity.x +
                        projectile->object.angular_velocity.y * projectile->object.angular_velocity.y +
                        projectile->object.angular_velocity.z * projectile->object.angular_velocity.z);

  if (angular_speed != REAL_ZERO_POOL) {
    inverse_angular_speed = 1.0f / angular_speed;
    projectile->flags |= FLAG(_projectile_has_angular_velocity_bit);
    projectile->field_214.i = inverse_angular_speed * projectile->object.angular_velocity.x;
    projectile->field_214.j = inverse_angular_speed * projectile->object.angular_velocity.y;
    projectile->field_214.k = inverse_angular_speed * projectile->object.angular_velocity.z;
    projectile->field_220 = x87_fsin(angular_speed);
    projectile->field_224 = x87_fcos(angular_speed);
  } else {
    projectile->flags &= ~FLAG(_projectile_has_angular_velocity_bit);
    projectile->field_220 = REAL_ZERO_POOL;
    projectile->field_224 = 1.0f;
  }
}

/* The water flag selects the water damage range; both branches copy the water
 * upper bound into field_210. */
void projectile_calculate_deceleration(int projectile_handle)
{
  projectile_data_t *projectile;
  projectile_definition_t *definition;

  projectile = projectile_get(projectile_handle);
  definition = projectile_definition_get(projectile->object.tag_index);

  if (projectile->object.flags & FLAG(_object_under_water_bit)) {
    projectile->deceleration = projectile_calculate_deceleration_from_distances(
      definition, definition->water_damage_range_lower_bound,
      definition->water_damage_range_upper_bound);
    projectile->field_210 = definition->water_damage_range_upper_bound;

    if (definition->water_damage_range_lower_bound > REAL_ZERO_POOL) {
      projectile->field_208 =
        definition->water_damage_range_lower_bound / definition->initial_velocity;
      return;
    }
  } else {
    projectile->deceleration = projectile_calculate_deceleration_from_distances(
      definition, definition->air_damage_range_lower_bound,
      definition->air_damage_range_upper_bound);
    projectile->field_210 = definition->water_damage_range_upper_bound;

    if (definition->air_damage_range_lower_bound > REAL_ZERO_POOL) {
      projectile->field_208 =
        definition->air_damage_range_lower_bound / definition->initial_velocity;
      return;
    }
  }

  projectile->field_204 = 1.0f;
  projectile->field_208 = 0.0f;
}

/* Tests the path from the projectile's position to new_pos: one ray along
 * the centre line, then (when the definition has a collision radius) two
 * parallel sweeps offset by +/-radius along cross(delta, up).
 * projectile_handle arrives in EAX and new_pos in EDI. */
bool projectile_collision_test_line(int projectile_handle, float *new_pos,
                  int16_t *collision_result)
{
  projectile_data_t *projectile;
  projectile_definition_t *definition;
  real_point3d *position;
  real_point3d *destination;
  float *up;
  float *left;
  float radius;
  real_vector3d delta;
  /* One aggregate keeps the five vectors in the original frame order. */
  struct {
    real_point3d negative_origin;
    real_point3d negative_end;
    real_point3d positive_end;
    union {
      real_vector3d ray;
      real_point3d positive_origin;
    };
    real_vector3d direction;
  } sweep;

#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_SWEEP_POS, RNG_TRACE_BITS(new_pos[0]),
               RNG_TRACE_BITS(new_pos[2]));
#endif
  projectile = projectile_get(projectile_handle);
  definition = projectile_definition_get(projectile->object.tag_index);
  position = &projectile->object.position;
  destination = (real_point3d *)new_pos;
#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_SWEEP_NEW_Y_HANDLE, RNG_TRACE_BITS(destination->y),
               (unsigned int)projectile_handle);
  RNG_TRACE_EX(RNG_TRACE_KIND_SWEEP_POS_XY, RNG_TRACE_BITS(position->x),
               RNG_TRACE_BITS(position->y));
  RNG_TRACE_EX(RNG_TRACE_KIND_SWEEP_POS_Z_VEL_X, RNG_TRACE_BITS(position->z),
               RNG_TRACE_BITS(projectile->object.translational_velocity.x));
  RNG_TRACE_EX(RNG_TRACE_KIND_SWEEP_VEL_YZ,
               RNG_TRACE_BITS(projectile->object.translational_velocity.y),
               RNG_TRACE_BITS(projectile->object.translational_velocity.z));
#endif

  sweep.ray.i = destination->x - position->x;
  sweep.ray.j = destination->y - position->y;
  sweep.ray.k = destination->z - position->z;
  if (SWEEP_LOS(FUN_0014df70(0x1000e9, (float *)position, (float *)&sweep.ray,
                             projectile->ignore_object_index, collision_result)) == 0) {
    if (definition->collision_radius < REAL_0_0001_POOL) {
      return false;
    }

    up = global_up_vector_ptr;
    delta.i = destination->x - position->x;
    delta.j = destination->y - position->y;
    delta.k = destination->z - position->z;

    sweep.direction.i = delta.j * up[2] - delta.k * up[1];
    sweep.direction.j = delta.k * up[0] - delta.i * up[2];
    sweep.direction.k = delta.i * up[1] - delta.j * up[0];

    if (normalize3d((float *)&sweep.direction) == REAL_ZERO_POOL) {
      /* Moving straight along up: offset sideways instead. */
      left = global_left_vector_ptr;
      sweep.direction.i = left[0];
      sweep.direction.j = left[1];
      sweep.direction.k = left[2];
    }

    radius = definition->collision_radius;

    sweep.positive_origin.x = sweep.direction.i * radius + position->x;
    sweep.positive_origin.y = sweep.direction.j * radius + position->y;
    sweep.positive_origin.z = sweep.direction.k * radius + position->z;

    sweep.positive_end.x = sweep.direction.i * radius + destination->x;
    sweep.positive_end.y = sweep.direction.j * radius + destination->y;
    sweep.positive_end.z = sweep.direction.k * radius + destination->z;

    sweep.negative_origin.x = sweep.direction.i * (-radius) + position->x;
    sweep.negative_origin.y = sweep.direction.j * (-radius) + position->y;
    sweep.negative_origin.z = sweep.direction.k * (-radius) + position->z;

    sweep.negative_end.x = sweep.direction.i * (-radius) + destination->x;
    sweep.negative_end.y = sweep.direction.j * (-radius) + destination->y;
    sweep.negative_end.z = sweep.direction.k * (-radius) + destination->z;

    sweep.direction.i = sweep.positive_end.x - sweep.positive_origin.x;
    sweep.direction.j = sweep.positive_end.y - sweep.positive_origin.y;
    sweep.direction.k = sweep.positive_end.z - sweep.positive_origin.z;

    if (SWEEP_LOS(FUN_0014df70(0x89, (float *)&sweep.positive_origin, (float *)&sweep.direction,
                               projectile->ignore_object_index, collision_result)) == 0 &&
        FUN_000130d0(0x89, (float *)&sweep.negative_origin, (float *)&sweep.negative_end,
                     projectile->ignore_object_index, collision_result) == 0) {
      return false;
    }
  }

  return true;
}

/* When more than six projectiles of one definition are stuck to a biped the
 * definition's super detonation replaces the normal effect. */
void projectile_detonate(int projectile_handle, char has_hit_count, float current_time)
{
  projectile_data_t *projectile;
  projectile_definition_t *definition;
  int effect_tag_index;
  void *marker_names[2];

  object_data_t *parent;
  int sibling_index;
  object_data_t *sibling;
  projectile_data_t *sibling_projectile;
  int count;

  real_point3d marker_points[2];
  real_vector3d marker_forwards[2];
  real_vector3d up;
  real_point3d parent_position;
  real_point3d original_position;

  damage_data_t damage;

  short material_type;
  projectile_material_response_definition_t *material_response;
  int detonation_effect_index;

  short ai_volume;

  projectile = projectile_get(projectile_handle);
  definition = projectile_definition_get(projectile->object.tag_index);
  effect_tag_index = definition->effect.tag_index;

  marker_names[0] = marker_name_none;
  marker_names[1] = marker_name_gravity;

  if (((definition->flags & 8u) != 0) &&
      ((projectile->flags & 0x40u) == 0) &&
      (projectile->object.parent_object_index.value != NONE)) {
    parent = object_get_and_verify_type(projectile->object.parent_object_index.value, _object_mask_all);
    sibling_index = parent->unk_200.value;
    count = 0;

    while (sibling_index != NONE) {
      sibling = object_get_and_verify_type(sibling_index, _object_mask_all);
      if (sibling->tag_index == projectile->object.tag_index) {
        sibling_projectile = projectile_get(sibling_index);
        if ((sibling_projectile->flags & 0x40u) == 0)
          count++;
      }
      sibling_index = sibling->next_object_index.value;
    }

    if ((parent->type == _object_type_biped) &&
        ((((unit_data_t *)object_get_and_verify_type(
             projectile->object.parent_object_index.value, _object_mask_biped))
            ->unk_456.value == NONE) ||
         game_engine_running()) &&
        ((short)count > 6)) {
      sibling_index = parent->unk_200.value;
      while (sibling_index != NONE) {
        sibling = object_get_and_verify_type(sibling_index, _object_mask_all);
        if (sibling->tag_index == projectile->object.tag_index) {
          sibling_projectile = projectile_get(sibling_index);
          if ((sibling_projectile->flags & 0x40u) == 0) {
            sibling_projectile = projectile_get(sibling_index);
            if ((short)count <= 6) {
              sibling_projectile->flags |= 0x40u;
              sibling_projectile->detonation_timer *= random_math_real(
                (unsigned int *)get_global_random_seed_address());
              sibling_projectile->arming_time *= random_math_real(
                (unsigned int *)get_global_random_seed_address());
            } else {
              sibling_projectile->detonation_timer = 0.0f;
              sibling_projectile->arming_time = 0.0f;
            }
            count--;
          }
        }
        sibling_index = sibling->next_object_index.value;
      }

      effect_tag_index = definition->super_detonation.tag_index;

      object_get_world_position(projectile->object.parent_object_index.value, &parent_position);
      object_detach_from_parent(projectile_handle);

      original_position.x = projectile->object.position.x;
      original_position.y = projectile->object.position.y;
      original_position.z = projectile->object.position.z;

      object_translate(projectile_handle, (float *)&parent_position, NULL);
      object_try_place(projectile_handle, (float *)&original_position);
      object_update_children_recursive(projectile_handle);
    }
  }

  if (has_hit_count && (projectile->tracer_attachment_index != NONE) &&
      (projectile->object.attachment_indices[projectile->tracer_attachment_index] != NONE)) {
    object_compute_node_matrices(projectile_handle);

    contrail_owner_collision(
      projectile->object.attachment_indices[projectile->tracer_attachment_index], 0,
      (REAL_ONE_POOL - current_time) * REAL_ONE_THIRTIETH_28AB38_POOL);
  }

  object_get_world_position(projectile_handle, &marker_points[0]);
  object_get_orientation(projectile_handle, (float *)&marker_forwards[0], (float *)&up);

  marker_points[1] = marker_points[0];
  up.i = global_down3d->i;
  up.j = global_down3d->j;
  up.k = global_down3d->k;
  marker_forwards[1].i = up.i;
  marker_forwards[1].j = up.j;
  marker_forwards[1].k = up.k;

  effect_new_unattached_from_markers(effect_tag_index, projectile->object.owner_object_index,
                                     NULL, 2, marker_names, (float *)marker_points, (float *)marker_forwards, 0.0f,
                                     0.0f, 0.0f, 0.0f, 1.0f);

  if ((projectile->object.parent_object_index.value != NONE) &&
      (definition->attached_detonation_damage.tag_index != NONE)) {
    damage_data_new(&damage, definition->attached_detonation_damage.tag_index);

    damage.flags |= 8u;

    object_get_orientation(projectile_handle, (float *)&damage.direction, NULL);

    object_get_world_position(projectile_handle, &damage.origin);

    damage.epicenter = damage.origin;

    damage.owner_object_index = projectile->object.owner_object_index;
    damage.owner_player_index = projectile->object.owner_player_index;
    damage.owner_team_index = projectile->object.owner_team_index;

    object_cause_damage(
      &damage, projectile->object.parent_object_index.value, NONE,
      NONE, /* dup-args-ok: verified 3x PUSH -1 at 0x000f8c5e/6c/7a */
      NONE, NULL);
  }

  material_type = projectile->hit_material_type;

  if (material_type != NONE) {
    if (material_type >= 0 && (int)material_type < definition->material_responses.count) {
      material_response = tag_block_get_element(&definition->material_responses, (int)material_type,
                                                sizeof(projectile_material_response_definition_t));
    } else {
      material_response = &default_projectile_material_response;
    }

    detonation_effect_index = material_response->detonation_effect.tag_index;
    effect_new_unattached_from_markers(detonation_effect_index, projectile->object.owner_object_index,
                                       NULL, 2, marker_names, (float *)marker_points, (float *)marker_forwards,
                                       0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  }

  ai_volume = definition->detonation_noise;
  ai_handle_spatial_effect(projectile_handle, (float *)&marker_points[0], (short)2, ai_volume, (short)1);
}

/* Arguments of projectile_adjust_for_angular_velocity_change and
 * projectile_calculate_deceleration go in EAX. */
bool projectile_new(int projectile_handle)
{
  projectile_data_t *projectile;
  projectile_definition_t *definition;
  float timer;
  float timer_ticks;
  float arming_ticks;
  float initial_velocity;
  projectile_attachment_definition_t *attachment;
  int attachment_count;
  int16_t attachment_index;
  int *seed;

  projectile = projectile_get(projectile_handle);
  definition = projectile_definition_get(projectile->object.tag_index);

  projectile->object.flags |= 0x2000u;
  projectile->flags = FLAG(_projectile_tracer_bit);
  projectile->target_object_index = NONE;
  projectile->action = 0;
  projectile->hit_material_type = NONE;

  projectile->ignore_object_index = object_get_root_parent(projectile->object.owner_object_index);

  if (definition->flags & 4) {
    timer = definition->timer_lower_bound;
  } else {
    seed = get_global_random_seed_address();
    timer = random_real_range(seed, definition->timer_lower_bound,
                              definition->timer_upper_bound);
  }

  timer_ticks = timer * TICKS_PER_SECOND;
  if (REAL_ONE_POOL <= timer_ticks) {
    projectile->detonation_timer_delta = REAL_ONE_POOL / timer_ticks;
  }
  arming_ticks = definition->arming_time * TICKS_PER_SECOND;
  if (REAL_ONE_POOL <= arming_ticks) {
    projectile->arming_time_delta = REAL_ONE_POOL / arming_ticks;
  }

  projectile->tracer_attachment_index = NONE;
  attachment_count = definition->attachments.count;
  attachment_index = 0;
  while (attachment_index < attachment_count) {
    attachment = tag_block_get_element(&definition->attachments, attachment_index,
                                       sizeof(projectile_attachment_definition_t));
    if (attachment->tag_group == TAG_GROUP_CONT) {
      projectile->tracer_attachment_index = attachment_index;
      break;
    }
    attachment_index++;
  }

  initial_velocity = definition->initial_velocity;
  projectile->object.translational_velocity.x += initial_velocity * projectile->object.forward.x;
  projectile->object.translational_velocity.y += initial_velocity * projectile->object.forward.y;
  projectile->object.translational_velocity.z += initial_velocity * projectile->object.forward.z;

  if (FUN_0018f3e0(&projectile->object.unk_72, &projectile->object.unk_80, 0)) {
    projectile->object.flags |= 0x10u;
  } else {
    projectile->object.flags &= ~0x10u;
  }

  projectile_adjust_for_angular_velocity_change(projectile_handle);
  projectile_export_function_values(projectile_handle);
  projectile_calculate_deceleration(projectile_handle);

  projectile->object.flags |= 0xc0000u;

  return true;
}

/* Adds acceleration to the translational velocity of an unattached projectile
 * and a random spin proportional to its magnitude.  projectile_handle goes to
 * projectile_adjust_for_angular_velocity_change in EAX. */
void projectile_accelerate(int projectile_handle, float *acceleration)
{
  projectile_data_t *projectile;
  vector3_t *velocity;
  real_vector3d direction;
  float magnitude_squared;
  float scale;

#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_PROJECTILE_ACCEL_X,
               (unsigned int)projectile_handle, RNG_TRACE_BITS(acceleration[0]));
  RNG_TRACE_EX(RNG_TRACE_KIND_PROJECTILE_ACCEL_YZ,
               RNG_TRACE_BITS(acceleration[1]), RNG_TRACE_BITS(acceleration[2]));
#endif
  projectile = projectile_get(projectile_handle);
  projectile_definition_get(projectile->object.tag_index);
  if (!real_vector3d_valid(acceleration)) {
    display_assert(
      csprintf(error_string_buffer, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "acceleration", (double)acceleration[0], (double)acceleration[1], (double)acceleration[2]),
      "c:\\halo\\SOURCE\\items\\projectiles.c", 0x3ef, 1);
    system_exit(-1);
  }

  if (projectile->object.parent_object_index.value == NONE) {
    velocity = &projectile->object.translational_velocity;

    if (!real_vector3d_valid((float *)velocity)) {
      display_assert(
        csprintf(error_string_buffer, "%s: assert_valid_real_vector2d(%f, %f, %f)",
                 "&projectile->object.translational_velocity",
                 (double)velocity->x, (double)velocity->y, (double)velocity->z),
        "c:\\halo\\SOURCE\\items\\projectiles.c", 0x3f3, 1);
      system_exit(-1);
    }

    velocity->x += acceleration[0];
    velocity->y += acceleration[1];
    velocity->z += acceleration[2];

    random_seed_get_direction3d((unsigned int *)get_global_random_seed_address(), (float *)&direction);

    magnitude_squared = acceleration[0] * acceleration[0] + acceleration[1] * acceleration[1] + acceleration[2] * acceleration[2];
    /* 0xf9001: the squared magnitude is narrowed to float32 before the FSQRT
     * (0xf9018); the sqrt and scale stay wide.  Without it a thrown grenade
     * lands one tick late (system-link run 3, tick 1009 vs 1010). */
    HALO_FLT_ROUNDTRIP(magnitude_squared);

    scale = random_math_real((unsigned int *)get_global_random_seed_address()) * sqrtf(magnitude_squared) * REAL_HALF_PI_POOL;

    /* 0xf9028..0xf9054: direction.i*scale stays on the x87 stack; j and k
     * are narrowed through their stack slots before the adds. */
    direction.j *= scale;
    HALO_FLT_ROUNDTRIP(direction.j);
    direction.k *= scale;
    HALO_FLT_ROUNDTRIP(direction.k);
    projectile->object.angular_velocity.x += direction.i * scale;
    projectile->object.angular_velocity.y += direction.j;
    projectile->object.angular_velocity.z += direction.k;

    projectile_adjust_for_angular_velocity_change(projectile_handle);

    projectile->object.flags &= ~0x20u;

    if (!real_vector3d_valid((float *)velocity)) {
      display_assert(
        csprintf(error_string_buffer, "%s: assert_valid_real_vector2d(%f, %f, %f)",
                 "&projectile->object.translational_velocity",
                 (double)velocity->x, (double)velocity->y, (double)velocity->z),
        "c:\\halo\\SOURCE\\items\\projectiles.c", 0x405, 1);
      system_exit(-1);
    }
  }
}

/* projectile_collision (0xf90d0): applies the material response for a hit
 * reported by projectile_collision_test_line.  Writes the hit point back to
 * hit_pos and the response velocity back to in_velocity.
 *
 * Register arguments: in_velocity arrives in EAX (moved to EDI at entry),
 * col_result (a collision_test_result) in ESI.  param_3 is never read
 * anywhere in 0xf90d0..0xf9c26. */
void projectile_collision(int projectile_handle, float *hit_pos, float param_3,
                  float *in_velocity /* @<eax> */,
                  int16_t *col_result /* @<esi> */)
{
  collision_test_result *collision = (collision_test_result *)col_result;
  projectile_data_t *projectile;
  projectile_definition_t *definition;
  projectile_material_response_definition_t *material_response;
  projectile_material_response_definition_t *surface_response;
  float damage_scale;
  int16_t response;
  float effect_scale, material_effect_scale;
  float speed_squared;
  /* Holds the material type until the response is chosen (one stack slot
   * in the reference). */
  int effect_definition_index;
  int16_t attached_count;
  float temp;
  /* Single assignment: the reference keeps it ST-resident across the == 0
   * test and the damage_scale fraction. */
  float speed;
  float impact_velocity;
  float impact_angle;
  float velocity_noise;
  int object_index;
  projectile_data_t *action_projectile;
  uint32_t object_flags;
  short material_type;
  int *seed;
  damage_data_t damage;
  real_point3d marker_points[NUMBER_OF_EFFECT_MARKERS];
  real_vector3d marker_forwards[NUMBER_OF_EFFECT_MARKERS];
  real_vector3d tangent_component;
  real_vector3d normal_component;
  real_vector3d direction;
  real_point3d *point;
  int marker_index;

#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_RESPONSE_HIT_XY,
               RNG_TRACE_BITS(hit_pos[0]), RNG_TRACE_BITS(hit_pos[1]));
  RNG_TRACE_EX(RNG_TRACE_KIND_RESPONSE_HIT_Z_HANDLE,
               RNG_TRACE_BITS(hit_pos[2]), (unsigned int)projectile_handle);
  RNG_TRACE_EX(RNG_TRACE_KIND_RESPONSE_VEL_XY,
               RNG_TRACE_BITS(in_velocity[0]), RNG_TRACE_BITS(in_velocity[1]));
  RNG_TRACE_EX(RNG_TRACE_KIND_RESPONSE_VEL_Z_TYPE,
               RNG_TRACE_BITS(in_velocity[2]),
               (unsigned int)(unsigned short)collision->field_00);
  RNG_TRACE_EX(RNG_TRACE_KIND_RESPONSE_NORMAL_XY,
               RNG_TRACE_BITS(collision->normal[0]),
               RNG_TRACE_BITS(collision->normal[1]));
  RNG_TRACE_EX(RNG_TRACE_KIND_RESPONSE_NORMAL_Z_T,
               RNG_TRACE_BITS(collision->normal[2]),
               RNG_TRACE_BITS(collision->t));
  RNG_TRACE_EX(RNG_TRACE_KIND_RESPONSE_OBJECT_META,
               (unsigned int)collision->field_38,
               *(unsigned int *)&collision->field_34);
#endif

  projectile = projectile_get(projectile_handle);
  definition = projectile_definition_get(projectile->object.tag_index);
  effect_definition_index = collision->field_34;

  effect_scale = 1.0f;
  material_effect_scale = 0.0f;
  direction = *(real_vector3d *)in_velocity;
  speed = normalize3d((float *)&direction);
  if (speed == REAL_ZERO_POOL) {
    direction = *global_up3d;
  }

  if (definition->final_velocity == definition->initial_velocity) {
    damage_scale = 1.0f;
  } else {
    damage_scale = (speed - definition->final_velocity) /
                   (definition->initial_velocity - definition->final_velocity);
    if (damage_scale < REAL_ZERO_POOL) {
      damage_scale = 0.0f;
    } else {
      /* 0xf9180 FST without pop: the < 0 compare sees the wide value, the
       * > 1 compare and every later use reload the narrowed slot. */
      HALO_FLT_ROUNDTRIP(damage_scale);
      if (damage_scale > REAL_ONE_POOL) {
        damage_scale = 1.0f;
      }
    }
  }

  if (collision->field_00 == _collision_result_object &&
      definition->impact_damage.tag_index != NONE) {
    if (collision->field_38 == NONE) {
      display_assert("collision->object_index!=NONE",
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x47c, 1);
      system_exit(-1);
    }
    damage_data_new(&damage, definition->impact_damage.tag_index);
    damage.flags |= 8u;
    damage.owner_player_index = projectile->object.owner_player_index;
    damage.owner_object_index = projectile->object.owner_object_index;
    damage.owner_team_index = projectile->object.owner_team_index;
    damage.scale = damage_scale;
    damage.origin = *(real_point3d *)collision->position;
    damage.epicenter = *(real_point3d *)collision->position;
    damage.direction = *(real_vector3d *)in_velocity;
    normalize3d((float *)&damage.direction);
    object_cause_damage(&damage, collision->field_38,
                        (short)collision->field_3e, (short)collision->field_3c,
                        (short)collision->field_4e, collision->normal);
    if (damage.material_type != NONE) {
      effect_definition_index = damage.material_type;
    }
    material_effect_scale = damage.field_48;
  }

  material_type = (int16_t)effect_definition_index;
  projectile->hit_material_type = material_type;
  if (material_type < 0 ||
      (int)material_type >= definition->material_responses.count) {
    material_response = &default_projectile_material_response;
  } else {
    material_response = tag_block_get_element(&definition->material_responses,
                                              (int)material_type,
                                              sizeof(*material_response));
  }

  temp = -material_response->velocity_noise;
  seed = get_global_random_seed_address();
  velocity_noise = random_real_range(seed, temp, 0.0f);
  impact_velocity = velocity_noise -
                    collision->normal[2] * in_velocity[2] -
                    collision->normal[1] * in_velocity[1] -
                    collision->normal[0] * in_velocity[0];
  /* 0xf932b FSTP: narrowed before the range test and 0xf9950 reload it. */
  HALO_FLT_ROUNDTRIP(impact_velocity);

  temp = material_response->angular_noise;
  seed = get_global_random_seed_address();
  impact_angle = random_real_range(seed, -temp, temp);
  {
    float angle_between;

    angle_between = FUN_0010c510(in_velocity, collision->normal);
    impact_angle = angle_between - REAL_HALF_PI_POOL + impact_angle;
    /* 0xf9365 FSTP: narrowed before the range test and 0xf981e reload it. */
    HALO_FLT_ROUNDTRIP(impact_angle);
  }

  response = material_response->default_response;
  {
    int use_default;

    use_default = 0;
    if (material_response->potential_response == 0) {
      use_default = 1;
    } else {
      if (material_response->potential_angle.upper != REAL_ZERO_POOL) {
        if (impact_angle < material_response->potential_angle.lower ||
            impact_angle > material_response->potential_angle.upper) {
          use_default = 1;
        }
      }
      if (!use_default &&
          material_response->potential_velocity.upper != REAL_ZERO_POOL) {
        if (impact_velocity < material_response->potential_velocity.lower ||
            impact_velocity > material_response->potential_velocity.upper) {
          use_default = 1;
        }
      }
      if (!use_default && (material_response->potential_flags & 1)) {
        if (collision->field_00 != _collision_result_object) {
          use_default = 1;
        } else if (!object_try_and_get_and_verify_type(collision->field_38,
                                                       _object_mask_unit)) {
          use_default = 1;
        }
      }
    }
    if (!use_default) {
      seed = get_global_random_seed_address();
      temp = random_math_real((unsigned int *)seed);
      if (temp >= material_response->potential_skip_fraction) {
        response = material_response->potential_response;
        effect_definition_index = material_response->potential_effect.tag_index;
      } else {
        effect_definition_index = material_response->default_effect.tag_index;
      }
    } else {
      effect_definition_index = material_response->default_effect.tag_index;
    }
  }

  /* Breakable structure surface. */
  if (collision->field_00 == 2 && (collision->field_4c & 0x8)) {
    damage_data_new(&damage, definition->impact_damage.tag_index);
    damage.flags |= 8;
    damage.origin = *(real_point3d *)collision->position;
    damage.epicenter = *(real_point3d *)collision->position;
    damage.direction = *(real_vector3d *)in_velocity;
    normalize3d((float *)&damage.direction);
    material_type = collision->field_34;
    if (material_type < 0 ||
        (int)material_type >= definition->material_responses.count) {
      surface_response = &default_projectile_material_response;
    } else {
      surface_response = tag_block_get_element(&definition->material_responses,
                                              (int)material_type,
                                              sizeof(*surface_response));
    }
    /* Divergence: the reference stores this to damage.material_response
     * (0xf94c1) before the call; this lift drops it. */
    (void)surface_response;
    damage.material_type = (int16_t)material_type;
    damage.field_14 = collision->field_0c;
    /* Dword copy of the int16 field_10 and its padding. */
    damage.field_18 = *(int32_t *)&collision->field_10;
    FUN_00146a90((uint16_t)(uint8_t)collision->field_4d, &damage,
                 collision->field_44);
  }

  *(real_point3d *)hit_pos = *(real_point3d *)collision->position;

  if (response == _projectile_material_response_overpenetrate) {
    if (collision->field_00 == 0) {
      object_flags = projectile->object.flags;
      if (object_flags & 0x10) {
        object_flags &= ~0x10u;
      } else {
        object_flags |= 0x10u;
      }
      projectile->object.flags = object_flags;
      projectile_calculate_deceleration(projectile_handle);
      hit_pos[0] -= collision->normal[0] * REAL_0_001_POOL;
      hit_pos[1] -= collision->normal[1] * REAL_0_001_POOL;
      /* Read through the parameter, not `collision`: the alias keeps the
       * reference's FLD normal.k / FMUL 0.001f operand order. */
      hit_pos[2] -= ((collision_test_result *)col_result)->normal[2] *
                    REAL_0_001_POOL;
    } else if (collision->field_00 == _collision_result_object) {
      temp = REAL_ONE_POOL - material_response->field_90;
      in_velocity[0] *= temp;
      in_velocity[1] *= temp;
      in_velocity[2] *= temp;
      projectile->ignore_object_index = collision->field_38;
    } else {
      if (definition->timer_upper_bound == REAL_ZERO_POOL) {
        response = _projectile_material_response_detonate;
      } else {
        projectile->flags |= 0x14;
        response = _projectile_material_response_attach;
      }
      goto clear_velocity;
    }
  } else if (response == _projectile_material_response_reflect) {
    FUN_0010b910(in_velocity, collision->normal,
                 (float *)&normal_component, (float *)&tangent_component);
    in_velocity[0] =
      (1.0f - material_response->perpendicular_friction) * tangent_component.i -
      (1.0f - material_response->parallel_friction) * normal_component.i;
    in_velocity[1] =
      (1.0f - material_response->perpendicular_friction) * tangent_component.j -
      (1.0f - material_response->parallel_friction) * normal_component.j;
    in_velocity[2] =
      (1.0f - material_response->perpendicular_friction) * tangent_component.k -
      (1.0f - material_response->parallel_friction) * normal_component.k;
    goto add_noise;
  } else {
  clear_velocity:
    *(real_vector3d *)in_velocity = *(real_vector3d *)halo_global_zero_vector_ptr;
  }

add_noise:
  if (material_response->angular_noise != REAL_ZERO_POOL) {
    seed = get_global_random_seed_address();
    seed_random_vector_in_cone3d(
      seed, in_velocity,
      0.0f, /* dup-args-ok: in-place, verified PUSH EDI x2 at 0x000f95f3/59 */
      material_response->angular_noise, in_velocity);
  }

  if (material_response->velocity_noise != REAL_ZERO_POOL) {
    float lower_bound, upper_bound;

    temp = normalize3d(in_velocity);
    if (temp != REAL_ZERO_POOL) {
      lower_bound = -material_response->velocity_noise;
      upper_bound = material_response->velocity_noise;
      seed = get_global_random_seed_address();
      temp += random_real_range(seed, lower_bound, upper_bound);
      in_velocity[0] = temp * in_velocity[0];
      in_velocity[1] = temp * in_velocity[1];
      in_velocity[2] = temp * in_velocity[2];
    }
  }

  speed_squared = in_velocity[0] * in_velocity[0] +
                  in_velocity[1] * in_velocity[1] +
                  in_velocity[2] * in_velocity[2];
  if (response != _projectile_material_response_attach) {
    if (speed_squared <
        definition->minimum_velocity * definition->minimum_velocity) {
      action_projectile = projectile_get(projectile_handle);
      if (action_projectile->action < _projectile_action_detonate) {
        action_projectile->action = _projectile_action_detonate;
      }
    }
  }
  /* 0xf9694 FST without pop: the minimum-velocity compare sees the wide
   * value, the epsilon test reloads the narrowed slot. */
  HALO_FLT_ROUNDTRIP(speed_squared);

  if (speed_squared < REAL_0_0001_POOL) {
    projectile->flags |= 0x10;
    if (collision->normal[2] > REAL_0_3_POOL) {
      projectile->object.flags |= 0x20;
      if (definition->timer_upper_bound == REAL_ZERO_POOL) {
        action_projectile = projectile_get(projectile_handle);
        if (action_projectile->action < _projectile_action_detonate) {
          action_projectile->action = _projectile_action_detonate;
        }
      }
    }
  }

  effect_scale = 0.0f;
  {
    int16_t scale_effects_by = material_response->scale_effects_by;

    if (scale_effects_by == 0) {
      effect_scale = damage_scale;
    } else if (scale_effects_by == 1) {
      effect_scale = impact_angle * REAL_TWO_OVER_PI_POOL;
      /* 0xf9827 FSTP: narrowed before both clamp compares reload it. */
      HALO_FLT_ROUNDTRIP(effect_scale);
    } else {
      goto clamp_material_effect_scale;
    }
  }
  if (effect_scale < REAL_ZERO_POOL) {
    effect_scale = 0.0f;
  } else if (effect_scale > REAL_ONE_POOL) {
    effect_scale = 1.0f;
  }

clamp_material_effect_scale:
  if (material_effect_scale < REAL_ZERO_POOL) {
    material_effect_scale = 0.0f;
  } else if (material_effect_scale > REAL_ONE_POOL) {
    material_effect_scale = 1.0f;
  }

  marker_forwards[_effect_marker_incident].i =
    direction.i * REAL_NEGATIVE_ONE_POOL;
  marker_forwards[_effect_marker_incident].j =
    direction.j * REAL_NEGATIVE_ONE_POOL;
  marker_forwards[_effect_marker_incident].k =
    direction.k * REAL_NEGATIVE_ONE_POOL;
  marker_forwards[_effect_marker_negative_incident] = direction;
  marker_forwards[_effect_marker_gravity] = *global_down3d;
  marker_forwards[_effect_marker_normal] = *(real_vector3d *)collision->normal;
  FUN_0010c8e0((float *)&direction, collision->normal,
               (float *)&marker_forwards[_effect_marker_reflection]);

  point = marker_points;
  for (marker_index = 0; marker_index < NUMBER_OF_EFFECT_MARKERS;
       marker_index++) {
    point->x = collision->position[0];
    point->y = collision->position[1];
    point->z = collision->position[2];
    point++;
  }

  if (impact_velocity > REAL_ONE_OVER_120_POOL) {
    if (collision->field_00 == _collision_result_object) {
      effect_new_attached_from_markers(
        effect_definition_index, projectile_handle, collision->field_38,
        (uint16_t)collision->field_3e, NUMBER_OF_EFFECT_MARKERS,
        effect_marker_names, (float *)marker_points, (float *)marker_forwards,
        effect_scale, material_effect_scale, 0.0f, 0.0f);
    } else {
      effect_new_unattached_from_markers(
        effect_definition_index, projectile_handle, 0, NUMBER_OF_EFFECT_MARKERS,
        effect_marker_names, (float *)marker_points, (float *)marker_forwards,
        effect_scale, material_effect_scale, 0.0f, 0.0f, 1);
    }
  }

  if (!(projectile->flags & 0x20) &&
      ((projectile->flags & 0x10) ||
       response == _projectile_material_response_attach)) {
    if (collision->field_00 == _collision_result_object) {
      effect_new_attached_from_markers(
        definition->detonation_started.tag_index, projectile_handle,
        collision->field_38, (uint16_t)collision->field_3e,
        NUMBER_OF_EFFECT_MARKERS, effect_marker_names,
        (float *)marker_points, (float *)marker_forwards, effect_scale,
        material_effect_scale, 0.0f, 0.0f);
    } else {
      effect_new_unattached_from_markers(
        definition->detonation_started.tag_index, projectile_handle, 0,
        NUMBER_OF_EFFECT_MARKERS, effect_marker_names, (float *)marker_points,
        (float *)marker_forwards, effect_scale, material_effect_scale, 0.0f,
        0.0f, 1);
    }
  }

  switch (response) {
  case _projectile_material_response_disappear:
    action_projectile = projectile_get(projectile_handle);
    if (action_projectile->action < _projectile_action_delete) {
      action_projectile->action = _projectile_action_delete;
      return;
    }
    break;

  case _projectile_material_response_detonate:
    action_projectile = projectile_get(projectile_handle);
    if (action_projectile->action < _projectile_action_detonate) {
      action_projectile->action = _projectile_action_detonate;
      return;
    }
    break;

  case _projectile_material_response_reflect:
  case _projectile_material_response_overpenetrate:
    break;

  case _projectile_material_response_attach: {
    if (collision->field_00 == _collision_result_object &&
        (definition->flags & 0x8)) {
      /* Zero the timers of this definition's projectiles already attached
       * to the hit object; flag this one once more than five are found. */
      object_data_t *parent_object;
      object_data_t *child_object;
      projectile_data_t *child_projectile;

      parent_object = object_get_and_verify_type(collision->field_38, -1);
      object_index = parent_object->unk_200.value;
      attached_count = 0;
      while (object_index != NONE) {
        child_object = object_get_and_verify_type(object_index, -1);
        if (child_object->tag_index == projectile->object.tag_index) {
          child_projectile = projectile_get(object_index);
          if (!(child_projectile->flags & 0x40)) {
            projectile_data_t *attached_projectile;

            attached_projectile = projectile_get(object_index);
            attached_projectile->arming_time = 0.0f;
            attached_projectile->detonation_timer = 0.0f;
            attached_count++;
          }
        }
        if (attached_count > 5) {
          projectile->flags |= 0x80;
          break;
        }
        object_index = child_object->next_object_index.value;
      }
    }
    projectile->object.translational_velocity =
      *(real_point3d *)halo_global_zero_vector_ptr;
    projectile->object.angular_velocity =
      *(real_point3d *)halo_global_zero_vector_ptr;
    projectile->object.flags |= 0x20;
    projectile->flags |= FLAG(_projectile_attached_bit);
    object_translate(projectile_handle, hit_pos, &collision->field_0c);
    if (collision->field_00 == _collision_result_object) {
      object_attach_to_parent(collision->field_38, projectile_handle,
                              (int16_t)collision->field_3e);
    }
    if (definition->flags & 0x4) {
      temp = definition->timer_upper_bound * TICKS_PER_SECOND;
      if (temp >= REAL_ONE_POOL) {
        projectile->detonation_timer_delta = REAL_ONE_POOL / temp;
      }
    }
    break;
  }

  default:
    display_assert(0, "c:\\halo\\SOURCE\\items\\projectiles.c", 0x5cf, 1);
    system_exit(-1);
    return;
  }
}

/* Advances one projectile by one tick: detonation timer, guidance,
 * deceleration, gravity, maximum range, then up to
 * MAXIMUM_PROJECTILE_BOUNCES_PER_TICK collision steps. */
bool projectile_update(int projectile_handle)
{
  projectile_data_t *projectile;
  projectile_definition_t *definition;
  int contrail_handle;
  int game_time;
  int local_player_index;
  int player_index;
  object_data_t *player_unit;
  int player_unit_index;
  float dot;
  float distance_squared;
  float sound_range;
  real_vector3d velocity;
  real_vector3d average_velocity;
  real_point3d new_position;
  real_point3d target_position;
  real_vector3d perpendicular;
  float angles[3];
  real_vector3d target_delta;
  real_vector3d velocity_direction;
  real_vector3d steer_axis;
  /* unattached_impulse_sound_new location (0x2c bytes); also the
   * cross-product scratch of the orientation update */
  impulse_sound_location_t sound_location;
  float speed;
  float initial_speed;
  float range_fraction;
  float new_speed;
  float average_speed;
  float gravity;
  float scale;
  float remaining_fraction;
  /* the original keeps this live in ST(2) across the whole split-tick arm
   * (0xfa1bb..0xfa24a) */
  float split_fraction;
  float target_distance;
  float wander_fraction;
  int bounce_count;
  float time_remaining;
  char flyby_played;
  char moved;
  char collided;
  int ignore_object_index;
  float turn_angle;
  collision_test_result collision;
  float normal_k;
  object_data_t *target;
  int scratch;

  projectile = projectile_get(projectile_handle);
  definition = projectile_definition_get(projectile->object.tag_index);

  time_remaining = 1.0f;
  bounce_count = 0;
  flyby_played = '\0';

  if (profile_global_enable_byte && projectile_update_profile_section_active) {
    profile_enter_private(projectile_update_profile_section);
  }

  /* byte-width flag read: the dword form here costs raw bytes */
  if ((((uint8_t)projectile->flags & FLAG(_projectile_tracer_bit)) == 0) &&
      (projectile->tracer_attachment_index != NONE)) {
    contrail_handle =
      projectile->object.attachment_indices[projectile->tracer_attachment_index];
    if (contrail_handle != NONE) {
      contrail_delete(contrail_handle);
    }
    projectile->object.attachment_indices[projectile->tracer_attachment_index] = NONE;
    projectile->tracer_attachment_index = NONE;
  }

  projectile->arming_time =
    projectile->arming_time_delta + projectile->arming_time;
  projectile->field_204 =
    projectile->field_208 + projectile->field_204;

  {
    uint8_t start_timer;
    start_timer = 0;
    {
      int timer_starts = definition->detonation_timer_starts;
      if (timer_starts == 1 || timer_starts == 2) {
        start_timer = (uint8_t)((projectile->flags >> _projectile_unknown_bit4) & 1);
      } else {
        start_timer = 1;
      }
    }
    {
      uint32_t flags = projectile->flags;
      if (((flags & FLAG(_projectile_detonation_timer_running_bit)) != 0) ||
          ((flags & FLAG(_projectile_attached_bit)) != 0) || (start_timer != 0)) {
        if ((flags & FLAG(_projectile_detonation_timer_running_bit)) == 0) {
          projectile->flags = flags | FLAG(_projectile_detonation_timer_running_bit);
        }
        {
          float timer =
            projectile->detonation_timer_delta + projectile->detonation_timer;
          projectile->detonation_timer = timer;
          if (timer >= 1.0f) {
            projectile_data_t *detonating = projectile_get(projectile_handle);
            if (detonating->action < _projectile_action_detonate) {
              detonating->action = _projectile_action_detonate;
            }
          }
        }
      }
    }
  }

  projectile_export_function_values(projectile_handle);

  do {
    if ((projectile->action != _projectile_action_none) &&
        (((projectile->action != _projectile_action_detonate) ||
          (projectile->arming_time_delta == 0.0f) ||
          (!(projectile->arming_time < 1.0f))))) {
      break;
    }
    if (((projectile->flags & FLAG(_projectile_attached_bit)) != 0) ||
        ((projectile->object.flags & FLAG(_object_unknown_bit5)) != 0) ||
        (projectile->object.parent_object_index.value != NONE)) {
      break;
    }

    velocity.i = projectile->object.translational_velocity.x;
    velocity.j = projectile->object.translational_velocity.y;
    velocity.k = projectile->object.translational_velocity.z;
    average_velocity.i = projectile->object.translational_velocity.x;
    average_velocity.j = projectile->object.translational_velocity.y;
    average_velocity.k = projectile->object.translational_velocity.z;
    ignore_object_index = projectile->ignore_object_index;
    moved = '\0';

    /* 0xf9e07-0xf9e37: (x*x + y*y) + z*z */
    speed = sqrtf(projectile->object.translational_velocity.x *
                    projectile->object.translational_velocity.x +
                  projectile->object.translational_velocity.y *
                    projectile->object.translational_velocity.y +
                  projectile->object.translational_velocity.z *
                    projectile->object.translational_velocity.z);
    /* 0xf9e4c: FST dword [ebp-0x1c] -- the FSQRT result is narrowed before it
     * is copied on to [ebp-0x2c] (new_speed) and [ebp-0x38] (average_speed) at
     * 0xf9e55/0xf9e58, so all four locals hold the same float32. */
    HALO_FLT_ROUNDTRIP(speed);
    new_speed = speed;
    initial_speed = speed;
    average_speed = speed;

    if (!(char)real_vector3d_valid(
          (float *)&projectile->object.translational_velocity)) {
      display_assert(csprintf(error_string_buffer,
                              "%s: assert_valid_real_vector2d(%f, %f, %f)",
                              "&projectile->object.translational_velocity",
                              (double)projectile->object.translational_velocity.x,
                              (double)projectile->object.translational_velocity.y,
                              (double)projectile->object.translational_velocity.z),
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x146, 1);
      system_exit(-1);
    }

    if ((projectile->target_object_index != NONE) &&
        (definition->guided_angular_velocity > 0.0f)) {
      target = object_get_and_verify_type(projectile->target_object_index, NONE);
      turn_angle = definition->guided_angular_velocity * REAL_ONE_THIRTIETH_POOL;
      /* 0xf9efa: FSTP dword [ebp-0x30] -- narrowed before the reload at
       * 0xf9f28. */
      HALO_FLT_ROUNDTRIP(turn_angle);
      if ((FLAG(target->type) & _object_mask_unit) != 0) {
        /* the unit pointer goes through the shared int scratch slot; a
         * separate pointer local moves bytes */
        scratch = (int)object_get_and_verify_type(projectile->target_object_index,
                                                  _object_mask_unit);
        if (((unit_data_t *)scratch)->unk_456.value != NONE) {
          turn_angle *= (float)FUN_000b5590(0x13);
          /* 0xf9f2e: FSTP dword [ebp-0x30] -- narrowed again after the
           * scale. */
          HALO_FLT_ROUNDTRIP(turn_angle);
        }
      }
      target_distance = (float)FUN_0001ad60(&target->unk_80,
                                        &projectile->object.unk_80);
      if (!(target_distance > REAL_10_POOL)) {
        if ((target_distance > REAL_2_0_POOL) &&
            (!((wander_fraction = (target_distance - REAL_2_0_POOL) *
                            REAL_0_125_POOL) < 0.0f))) {
          /* 0xf9f7c: FST dword [ebp-0x28] -- no pop, so the "< 0" test at
           * 0xf9f7f uses the wide stack copy; the "> 1.0" test reloads the
           * narrowed slot at 0xf9f8c. */
          HALO_FLT_ROUNDTRIP(wander_fraction);
          if (wander_fraction > 1.0f) {
            wander_fraction = 1.0f;
          }
        } else {
          wander_fraction = 0.0f;
        }
      } else {
        wander_fraction = 1.0f;
      }
      unit_get_center_of_mass(projectile->target_object_index,
                              (float *)&target_position);
      game_time = game_time_get();
      scratch = (game_time + (projectile_handle >> 16) * 7) & 0xffff;
      {
        float yaw_noise =
          FUN_0010a5e0(10, (float)scratch * REAL_ONE_NINETIETH_POOL);
        angles[0] = yaw_noise * REAL_TWO_PI_POOL;
        game_time = game_time_get();
        scratch = (game_time + (projectile_handle >> 16) * 3) & 0xffff;
        {
          float pitch_noise =
            FUN_0010a5e0(10, (float)scratch * REAL_ONE_NINETIETH_POOL);
          angles[2] =
            REAL_PI_POOL - pitch_noise * REAL_HALF_PI_POOL;
        }
      }
      angles[1] = angles[0];
      angles_to_vector((float *)&perpendicular, angles);
      target_position.x = perpendicular.i * wander_fraction + target_position.x;
      /* 0xfa06c / 0xfa07b / 0xfa08a: FSTP dword [ebp-0x6c] / [ebp-0x68] /
       * [ebp-0x64] -- each component is narrowed before the target_delta
       * subtractions reload it at 0xfa08d / 0xfa099 / 0xfa0a2. */
      HALO_FLT_ROUNDTRIP(target_position.x);
      target_position.y = perpendicular.j * wander_fraction + target_position.y;
      HALO_FLT_ROUNDTRIP(target_position.y);
      target_position.z = perpendicular.k * wander_fraction + target_position.z;
      HALO_FLT_ROUNDTRIP(target_position.z);
      target_delta.i = target_position.x - projectile->object.position.x;
      target_delta.j = target_position.y - projectile->object.position.y;
      target_delta.k = target_position.z - projectile->object.position.z;
      cross_product3d((float *)&projectile->object.translational_velocity,
                      (float *)&target_delta, (float *)&steer_axis);
      dot = target_delta.i * projectile->object.translational_velocity.x +
            target_delta.j * projectile->object.translational_velocity.y +
            target_delta.k * projectile->object.translational_velocity.z;
      if ((dot > 0.0f) &&
          ((float)normalize3d((float *)&steer_axis) > 0.0f)) {
        float turn_cos, turn_sin;
#ifdef XDK_BUILD
        __asm {
          fld turn_angle
          fcos
          fstp turn_cos
          fld turn_angle
          fsin
          fstp turn_sin
        }
#else
        turn_cos = x87_fcos(turn_angle);
        turn_sin = x87_fsin(turn_angle);
#endif
        rotate_vector3d_by_sincos((float *)&projectile->object.translational_velocity,
                                  (float *)&steer_axis, turn_sin, turn_cos);
      }
    }

    if (!(char)real_vector3d_valid((float *)&velocity)) {
      display_assert("projectile velocity is bad after steering.",
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x19d, 1);
      system_exit(-1);
    }

    if (projectile->field_204 >= 1.0f) {
      if ((!(initial_speed > definition->final_velocity)) ||
          (projectile->deceleration == 0.0f)) {
        if (((definition->maximum_range != 0.0f) ||
             ((definition->timer_upper_bound != 0.0f) ||
              !(definition->minimum_velocity <=
                definition->final_velocity))) ||
            ((projectile->deceleration == 0.0f) &&
             (!(projectile->odometer >= projectile->field_210)))) {
          if ((initial_speed < definition->final_velocity) &&
              (initial_speed > 0.0f)) {
            scale =
              (definition->final_velocity / initial_speed) * REAL_0_99_POOL;
            velocity.i *= scale;
            velocity.j *= scale;
            velocity.k *= scale;
          }
        } else {
          projectile_set_action(projectile_handle, _projectile_action_delete);
        }
      } else {
        scale = time_remaining * projectile->deceleration;
        new_speed = initial_speed - scale;
        if (new_speed <= definition->final_velocity) {
          /* the speed reaches final_velocity partway through the tick */
          split_fraction = (initial_speed - definition->final_velocity) / scale;
          new_speed = definition->final_velocity * REAL_0_99_POOL;
          /* 0xfa1c9: FSTP dword [ebp-0x2c] -- narrowed before the reload at
           * 0xfa1d4. */
          HALO_FLT_ROUNDTRIP(new_speed);
          remaining_fraction = 1.0f - split_fraction;
          average_speed =
            remaining_fraction * definition->final_velocity +
            (new_speed + initial_speed) * split_fraction * REAL_0_5_POOL;
          /* 0xfa1ec: FSTP dword [ebp-0x38] -- narrowed before the reloads at
           * 0xfa413 / 0xfa42c. */
          HALO_FLT_ROUNDTRIP(average_speed);
          scale = new_speed / initial_speed;
          velocity.i *= scale;
          velocity.j *= scale;
          velocity.k *= scale;
          /* 0xfa1fa/0xfa202/0xfa208: FSTP dword narrows each scaled component;
           * 0xfa20b/0xfa21a (and y/z) reload the narrowed slots for both
           * terms of the average. */
          HALO_FLT_ROUNDTRIP(velocity.i);
          HALO_FLT_ROUNDTRIP(velocity.j);
          HALO_FLT_ROUNDTRIP(velocity.k);
          /* 0xfa210: FMUL ST(2) is split_fraction (live since 0xfa1bb), not the
           * new_speed/initial_speed ratio that ST(0) holds. */
          average_velocity.i =
            remaining_fraction * velocity.i +
            (velocity.i + projectile->object.translational_velocity.x) *
              split_fraction * REAL_0_5_POOL;
          average_velocity.j =
            remaining_fraction * velocity.j +
            (velocity.j + projectile->object.translational_velocity.y) *
              split_fraction * REAL_0_5_POOL;
          average_velocity.k =
            remaining_fraction * velocity.k +
            (velocity.k + projectile->object.translational_velocity.z) *
              split_fraction * REAL_0_5_POOL;
        } else {
          average_speed = initial_speed - scale * REAL_0_5_POOL;
          /* 0xfa19e: FST dword [ebp-0x2c] -- no pop, so the split-tick test at
           * 0xfa1a1 saw the wide stack copy; this arm reloads the narrowed
           * slot at 0xfa265. */
          HALO_FLT_ROUNDTRIP(new_speed);
          scale = new_speed / initial_speed;
          velocity.i *= scale;
          velocity.j *= scale;
          velocity.k *= scale;
          /* 0xfa270/0xfa278/0xfa27e: FSTP dword [ebp-0x10]/[ebp-0xc]/[ebp-8]
           * narrow each scaled component; 0xfa281/0xfa28f/0xfa29e reload the
           * narrowed slots for the average.  clang otherwise keeps the
           * product in ST(i) (FST no-pop) and averages the wide value. */
          HALO_FLT_ROUNDTRIP(velocity.i);
          HALO_FLT_ROUNDTRIP(velocity.j);
          HALO_FLT_ROUNDTRIP(velocity.k);
          average_velocity.i =
            (velocity.i + projectile->object.translational_velocity.x) * REAL_0_5_POOL;
          average_velocity.j =
            (velocity.j + projectile->object.translational_velocity.y) * REAL_0_5_POOL;
          average_velocity.k =
            (velocity.k + projectile->object.translational_velocity.z) * REAL_0_5_POOL;
        }
      }
    }

    if (!(char)real_vector3d_valid((float *)&velocity)) {
      display_assert("projectile velocity is bad after deceleration.",
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x1cd, 1);
      system_exit(-1);
    }

    if ((projectile->object.flags & FLAG(_object_under_water_bit)) == 0) {
      gravity = definition->air_gravity_scale;
    } else {
      gravity = definition->water_gravity_scale;
    }
    gravity = global_gravity_by_address * gravity;
    /* 0xfa3b4: FSTP dword [ebp-4] -- narrowed before the reloads at 0xfa3ba
     * and 0xfa58c. */
    HALO_FLT_ROUNDTRIP(gravity);
    velocity.k -= gravity * time_remaining;
    average_velocity.k -= gravity * time_remaining * REAL_0_5_POOL;

    if (!(char)real_vector3d_valid((float *)&velocity)) {
      display_assert("projectile velocity is bad after gravity.",
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x1dc, 1);
      system_exit(-1);
    }

    if ((definition->maximum_range == 0.0f) ||
        (!(average_speed * time_remaining + projectile->odometer >
           definition->maximum_range))) {
      range_fraction = 1.0f;
    } else {
      if (average_speed != 0.0f) {
        range_fraction =
          ((definition->maximum_range - projectile->odometer) /
           average_speed) *
          time_remaining;
      } else {
        range_fraction = 0.0f;
      }
      {
        projectile_data_t *detonating = projectile_get(projectile_handle);
        if (detonating->action < _projectile_action_detonate) {
          detonating->action = _projectile_action_detonate;
        }
      }
    }

    scale = range_fraction * time_remaining;
    new_position.x = average_velocity.i * scale + projectile->object.position.x;
    new_position.y = average_velocity.j * scale + projectile->object.position.y;
    new_position.z = average_velocity.k * scale + projectile->object.position.z;

    if (!valid_real_point3d((float *)&new_position)) {
      display_assert(csprintf(error_string_buffer,
                              "%s: assert_valid_real_point3d(%f, %f, %f)",
                              "&new_position", (double)new_position.x,
                              (double)new_position.y, (double)new_position.z),
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x1f9, 1);
      system_exit(-1);
    }

    if (global_current_collision_user_depth >=
        MAXIMUM_COLLISION_USER_STACK_DEPTH) {
      display_assert("global_current_collision_user_depth < "
                     "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x1fb, 1);
      system_exit(-1);
    }
    collision_user_stack[global_current_collision_user_depth++] =
      COLLISION_USER_PROJECTILE_UPDATE;

    if ((short)bounce_count == MAXIMUM_PROJECTILE_BOUNCES_PER_TICK) {
      {
        projectile_data_t *detonating = projectile_get(projectile_handle);
        if (detonating->action < _projectile_action_detonate) {
          detonating->action = _projectile_action_detonate;
        }
      }
      time_remaining = 0.0f;
    } else if (projectile->action == _projectile_action_delete) {
      time_remaining = 0.0f;
    } else {
      moved = '\x01';
      collided = (char)projectile_collision_test_line(
        projectile_handle, (float *)&new_position, (int16_t *)&collision);
      if (collided == '\0') {
        time_remaining = 0.0f;
      } else {
        time_remaining = 1.0f - collision.t;
        /* 0xfa589: FSTP dword [ebp-0x18] -- narrowed before the reloads at
         * 0xfa58f and 0xfa5a8. */
        HALO_FLT_ROUNDTRIP(time_remaining);
        velocity.k += gravity * time_remaining;
        /* 0xfa595: FSTP dword [ebp-8] -- narrowed before the reload at
         * 0xfa5d6. */
        HALO_FLT_ROUNDTRIP(velocity.k);
        if (new_speed != 0.0f) {
          scale = time_remaining * projectile->deceleration + new_speed;
          if (scale > initial_speed) {
            scale = initial_speed;
          }
          scale /= new_speed;
          velocity.i *= scale;
          velocity.j *= scale;
          velocity.k *= scale;
        }
        normal_k = collision.normal[2];
        if (normal_k > REAL_0_3_POOL) {
          projectile->flags |= FLAG(_projectile_unknown_bit2);
        }
        projectile->ignore_object_index = NONE;
        projectile_collision(projectile_handle, (float *)&new_position,
                             time_remaining, (float *)&velocity,
                             (int16_t *)&collision);
        bounce_count++;
        ai_handle_spatial_effect(projectile_handle, collision.position, 1,
                                 definition->impact_noise, 1);
        if ((projectile->flags & FLAG(_projectile_attached_bit)) != 0) {
          moved = '\0';
        }
      }
    }

    if (collision_user_depth_word <= 1) {
      display_assert("global_current_collision_user_depth > 1",
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x230, 1);
      system_exit(-1);
    }
    --collision_user_depth_word;

    if (moved != '\0') {
      {
        volatile float dx, dy;
        /* 0xfa6d2: FST dword [ebp-0x3c] -- no pop, so the square at 0xfa6d5
         * multiplies the wide ST(0) by the narrowed slot; dx/dy are FSTP'd
         * (0xfa6c0/0xfa6c9) and squared narrow-by-narrow.  Addend order is
         * dz, dy, dx (0xfa6d5/0xfa6d8/0xfa6e0). */
        float dz, dz_wide;
        dx = new_position.x - projectile->object.position.x;
        dy = new_position.y - projectile->object.position.y;
        dz = new_position.z - projectile->object.position.z;
        dz_wide = dz;
        HALO_FLT_ROUNDTRIP(dz);
        projectile->odometer += sqrtf(dz_wide * dz + dy * dy + dx * dx);
      }

      /* flyby sound for the first local player the projectile passes */
      if ((flyby_played == '\0') && (definition->flyby_sound.tag_index != NONE)) {
        for (local_player_index = 0;
             (short)local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS;
             local_player_index++) {
          player_index = local_player_get_player_index(local_player_index);
          if (player_index != NONE) {
            scratch = local_player_get_player_index(local_player_index);
            player_unit_index =
              ((player_data_t *)datum_get(player_data, scratch))->unit_handle;
            if ((player_unit_index != NONE) &&
                (player_unit_index != ignore_object_index)) {
              player_unit = object_get_and_verify_type(player_unit_index, NONE);
              sound_range =
                (float)sound_get_default_priority(definition->flyby_sound.tag_index);
              target_delta.i = player_unit->unk_80 - projectile->object.position.x;
              target_delta.j =
                player_unit->unk_84 - projectile->object.position.y;
              target_delta.k =
                player_unit->unk_88 - projectile->object.position.z;
              FUN_0010b910((float *)&target_delta, (float *)&new_position,
                           (float *)&perpendicular, (float *)&sound_location.position);
              dot = perpendicular.i * (new_position.x - projectile->object.position.x) +
                    perpendicular.j * (new_position.y - projectile->object.position.y) +
                    perpendicular.k * (new_position.z - projectile->object.position.z);
              if ((dot >= 0.0f) &&
                  ((float)FUN_00012170((float *)&new_position) > dot)) {
                distance_squared = (float)FUN_00012170((float *)&sound_location.position);
                if (distance_squared < sound_range * sound_range) {
                  vector3d_scale_add(&player_unit->unk_80,
                                     (float *)&sound_location.position, /* dup-args-ok: in-place */
                                     -1.0f, (float *)&sound_location.position);
                  sound_location.field_0c = *(real_vector3d *)&projectile->object.up;
                  sound_location.field_18 = *global_down3d;
                  sound_location.field_24 = 0.0f;
                  sound_location.field_28 = 0.0f;
                  unattached_impulse_sound_new(definition->flyby_sound.tag_index,
                                               &sound_location, 1.0f);
                  flyby_played = '\x01';
                }
              }
            }
          }
        }
      }

      if (((definition->flags &
            FLAG(_projectile_definition_oriented_along_velocity_bit)) == 0) ||
          ((projectile->object.translational_velocity.x == 0.0f) &&
           (projectile->object.translational_velocity.y == 0.0f) &&
           (projectile->object.translational_velocity.z == 0.0f))) {
        if ((projectile->flags & FLAG(_projectile_has_angular_velocity_bit)) != 0) {
          float *forward = (float *)&projectile->object.forward;
          float *up = (float *)&projectile->object.up;
          float *axis = (float *)&projectile->field_214;
          float sine = projectile->field_220;
          float cosine = projectile->field_224;
          rotate_vector3d_by_sincos(forward, axis, sine, cosine);
          rotate_vector3d_by_sincos(up, axis, sine, cosine);
          normalize3d(forward);
          cross_product3d(up, forward, (float *)&sound_location.position);
          cross_product3d(forward, (float *)&sound_location.position, up);
          normalize3d(up);
        }
      } else {
        {
          float *forward = (float *)&projectile->object.forward;
          float *up = (float *)&projectile->object.up;
          velocity_direction.i = projectile->object.translational_velocity.x;
          velocity_direction.j = projectile->object.translational_velocity.y;
          velocity_direction.k = projectile->object.translational_velocity.z;
          if ((float)normalize3d((float *)&velocity_direction) > 0.0f) {
            forward[0] = velocity_direction.i;
            forward[1] = velocity_direction.j;
            forward[2] = velocity_direction.k;
            cross_product3d(up, forward, (float *)&sound_location.position);
            cross_product3d(forward, (float *)&sound_location.position, up);
            if ((float)normalize3d(up) == 0.0f) {
              perpendicular3d(forward, up);
              normalize3d(up);
            }
          }
          rotate_vector3d_by_sincos(up, forward, projectile->field_220,
                                    projectile->field_224);
        }
      }

      object_translate(projectile_handle, (float *)&new_position,
                       &collision.field_0c);
      {
        float tick_remaining = time_remaining;
        projectile->object.translational_velocity.x = velocity.i;
        projectile->object.translational_velocity.y = velocity.j;
        projectile->object.translational_velocity.z = velocity.k;
        if ((tick_remaining != 0.0f) && ((short)bounce_count != 0) &&
            (projectile->tracer_attachment_index != NONE) &&
            (projectile->object.attachment_indices
               [projectile->tracer_attachment_index] != NONE)) {
          object_compute_node_matrices(projectile_handle);
          contrail_owner_collision(
            projectile->object.attachment_indices[projectile->tracer_attachment_index], 0,
            (1.0f - time_remaining) * REAL_ONE_THIRTIETH_28AB38_POOL);
        }
      }
    }

    if (!(char)real_vector3d_valid(
          (float *)&projectile->object.translational_velocity)) {
      display_assert(csprintf(error_string_buffer,
                              "%s: assert_valid_real_vector2d(%f, %f, %f)",
                              "&projectile->object.translational_velocity",
                              (double)projectile->object.translational_velocity.x,
                              (double)projectile->object.translational_velocity.y,
                              (double)projectile->object.translational_velocity.z),
                     "c:\\halo\\SOURCE\\items\\projectiles.c", 0x2a0, 1);
      system_exit(-1);
    }
  } while (time_remaining > 0.0f);

  switch (projectile->action) {
    case _projectile_action_detonate: {
      float arming_time_delta = projectile->arming_time_delta;
      if (arming_time_delta == 0.0f ||
          projectile->arming_time >= 1.0f) {
        projectile_detonate(projectile_handle, (char)(bounce_count == 0),
                     time_remaining);
        object_delete(projectile_handle);
      }
      break;
    }
    case _projectile_action_delete:
      object_delete(projectile_handle);
      break;
  }

  if (!valid_real_normal3d_perpendicular((float *)&projectile->object.forward,
                                         (float *)&projectile->object.up)) {
    display_assert(
      csprintf(error_string_buffer,
               "%s, %s: assert_valid_real_vector3d_axes2"
               "(%f, %f, %f / %f, %f, %f)",
               "&projectile->object.forward", "&projectile->object.up",
               (double)projectile->object.forward.x,
               (double)projectile->object.forward.y,
               (double)projectile->object.forward.z,
               (double)projectile->object.up.x, (double)projectile->object.up.y,
               (double)projectile->object.up.z),
      "c:\\halo\\SOURCE\\items\\projectiles.c", 0x2b0, 1);
    system_exit(-1);
  }
  if (profile_global_enable_byte && projectile_update_profile_section_active) {
    profile_exit_private(projectile_update_profile_section);
  }
  return 1;
}

/* Sum of the damage-effect midpoints of the first trigger's projectile
 * (impact damage, then attached-detonation damage); also returns the
 * trigger's field_08 through out_field8 when non-NULL. */
float weapon_definition_get_damage_potential(int weapon_tag_index, float *out_field8)
{
  weapon_definition_t *weapon;
  weapon_trigger_definition_t *trigger;
  projectile_definition_t *projectile;
  damage_effect_definition_t *damage_effect;
  int projectile_tag_index;
  int damage_effect_tag_index;
  float damage_potential;

  weapon = tag_get(TAG_GROUP_WEAP, weapon_tag_index);
  trigger = tag_block_get_element(&weapon->triggers, 0,
                                  sizeof(weapon_trigger_definition_t));
  damage_potential = 0.0f;
  if (out_field8 != NULL) {
    *out_field8 = trigger->field_08;
  }
  projectile_tag_index = trigger->projectile.tag_index;
  if (projectile_tag_index != NONE) {
    projectile = projectile_definition_get(projectile_tag_index);
    damage_effect_tag_index = projectile->impact_damage.tag_index;
    if (damage_effect_tag_index != NONE) {
      damage_effect = tag_get(TAG_GROUP_JPT, damage_effect_tag_index);
      damage_potential = (damage_effect->field_1d8 + damage_effect->field_1d4) *
                         REAL_0_5_POOL;
    }
    damage_effect_tag_index = projectile->attached_detonation_damage.tag_index;
    if (damage_effect_tag_index != NONE) {
      damage_effect = tag_get(TAG_GROUP_JPT, damage_effect_tag_index);
      return (damage_effect->field_1d8 + damage_effect->field_1d4) *
               REAL_0_5_POOL +
             damage_potential;
    }
  }

  return damage_potential;
}

void animation_update(int animation_graph_tag_index, short *state, int *out_sound)
{
  animation_update_internal(1, animation_graph_tag_index, state, out_sound);
}

int16_t animation_choose_random_permutation(int animation_graph_tag_index, int16_t animation_index)
{
  return model_animation_choose_random(1, animation_graph_tag_index,
                                       animation_index);
}
