#include "x87_math.h"
#include "../math/real_math.h"
#include "unit_definitions.h"
#include "units_globals.h"
#include "vehicle_definitions.h"
#include "game_globals_definitions.h"

/* C fmod: VC71 emits the original CALL _CIfmod (0x1daf7e); clang uses the
 * equivalent FPREM loop. */
#if defined(_MSC_VER) && !defined(__clang__)
double __cdecl fmod(double, double);
#define VEHICLE_FMOD(val, div) ((real)fmod((double)(val), (double)(div)))
#else
#define VEHICLE_FMOD(val, div) x87_fmod((val), (double)(div))
#endif
/* vehicle_scripting_unload (0x1b5400)
 * Ejects every unit seated in vehicle_handle whose seat label (unit seat
 * element +0x4) contains name_filter; an empty filter matches every seat.
 * The rider's handle is iter.last_handle, written by object_iterator_next.
 */
uint16_t vehicle_scripting_unload(int vehicle_handle, const char *name_filter)
{
  uint32_t exit_count;
  object_iter_t iter;
  unit_definition_t *vehicle_definition;
  unit_data_t *rider;
  unit_seat_t *seat;
  char seat_label[256];
  int filter_is_empty;

  exit_count = 0;
  if (vehicle_handle != NONE) {
    vehicle_definition = tag_get(
      TAG_GROUP_UNIT,
      *(int *)object_get_and_verify_type(vehicle_handle, _object_mask_unit));
    filter_is_empty = name_filter == NULL || csstrlen(name_filter) == 0;
    object_iterator_new(&iter, _object_mask_unit, 0);
    rider = object_iterator_next(&iter);
    while (rider != NULL) {
      if (rider->object.parent_object_index.value == vehicle_handle) {
        seat = tag_block_get_element(&vehicle_definition->seats,
                                     rider->parent_seat_index,
                                     sizeof(unit_seat_t));
        csstrcpy(seat_label, (char *)seat + 4);
        csstr_tolower(seat_label);
        if ((filter_is_empty || crt_strstr(seat_label, name_filter) != NULL) &&
            unit_try_and_exit_seat(iter.last_handle) != 0) {
          exit_count++;
        }
      }
      rider = object_iterator_next(&iter);
    }
  }

  return (uint16_t)exit_count;
}

/* unit_scripting_exit_vehicle (0x1b5500) — eject unit_handle if it is seated in a vehicle. */
void unit_scripting_exit_vehicle(int unit_handle)
{
  unit_data_t *unit;

  if (unit_handle != NONE) {
    unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
    if (unit->object.parent_object_index.value != NONE &&
        unit->parent_seat_index != NONE) {
      unit_try_and_exit_seat(unit_handle);
    }
  }
}

/* vehicles_initialize_for_new_map (0x1b5550): empty in this build; vehicle
 * object_type_definition (0x323de8) slot +0x18. */
void vehicles_initialize_for_new_map(void)
{
}

/* vehicles_dispose_from_old_map (0x1b5560): empty in this build; vehicle
 * object_type_definition (0x323de8) slot +0x1c. */
void vehicles_dispose_from_old_map(void)
{
}

/* vehicle_place (0x1b5580) */
void vehicle_place(int vehicle_handle, void *placement)
{
  unit_place(vehicle_handle, (char *)placement + 0x48);
  object_add_scenario_permutation(vehicle_handle, (char *)placement + 0x28);
}

/* vehicle_causes_collision_damage (0x1b55c0): bit 7 of the 'vehi' definition flags. */
bool vehicle_causes_collision_damage(int vehicle_handle)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;

  vehicle = object_get_and_verify_type(vehicle_handle, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  return (bool)((definition->flags >> 7) & 1);
}

/* vehicle_hover (0x1b5610): latch the vehicle's world position into
 * hover_position and set flag bit 1, or clear the bit. */
void vehicle_hover(int vehicle_handle, uint8_t enable)
{
  vehicle_data_t *vehicle;

  if (vehicle_handle != NONE) {
    vehicle = object_get_and_verify_type(vehicle_handle, OBJECT_MASK_VEHICLE);
    if (enable != 0) {
      object_get_world_position(vehicle_handle, &vehicle->hover_position);
      vehicle->flags |= 2;
      return;
    }
    vehicle->flags &= ~2;
  }
}

/* vehicle_is_flipped (0x1b5680): up.k < 0.2f (pooled constant 0x2549d4).
 * FCOMP + TEST AH,0x5 + JP: NaN reads as not flipped. */
bool vehicle_is_flipped(int vehicle_handle)
{
  int result;
  vehicle_data_t *vehicle;

  vehicle = object_get_and_verify_type(vehicle_handle, OBJECT_MASK_VEHICLE);
  result = vehicle->unit.object.up.k < REAL_0_2_POOL;
  return (bool)result;
}

/* compute_airborne_ticks (0x1b56b0) — per-tick contact counters.
 * Register args: vehicle_handle@<eax>, mass_points@<edi>. airborne_ticks
 * saturates upward each tick and is cleared by any mass point touching
 * (flag 0x2) or flagged 0x10; the first touching point also bumps
 * on_ground_ticks and returns. Falling out of the loop clears
 * on_ground_ticks. The loop bound is re-read every iteration.
 */
void compute_airborne_ticks(int vehicle_handle, void *state_array)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;
  struct mass_point_datum *mass_points;
  uint32_t flags;
  int16_t i;

  vehicle = object_get_and_verify_type(vehicle_handle, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = tag_get(TAG_GROUP_PHYS, definition->physics.tag_index);
  mass_points = state_array;

  if (vehicle->airborne_ticks < 0xff) {
    vehicle->airborne_ticks++;
  }

  for (i = 0; i < physics->mass_points.count; i++) {
    flags = mass_points[i].flags;
    if ((flags & 2) != 0) {
      vehicle->airborne_ticks = 0;
      if (vehicle->on_ground_ticks < 0xff) {
        vehicle->on_ground_ticks++;
      }
      return;
    }
    if ((flags & 0x10) != 0) {
      vehicle->airborne_ticks = 0;
    }
  }

  vehicle->on_ground_ticks = 0;
}

/* set_real_quaternion (0x1b5750): no callers in this build. */
void set_real_quaternion(float *out, float i, float j, float k, float w)
{
  out[0] = i;
  out[1] = j;
  out[2] = k;
  out[3] = w;
}

/* vehicle_reset (0x1b5770) — zero the vehicle block. The stores are integer
 * zeros (XOR EBX,EBX), thrust (+0x448) is written before hover (+0x444), and
 * only the suspension bytes are memset, so hover_position survives.
 */
void vehicle_reset(int vehicle_handle)
{
  vehicle_data_t *vehicle;

  vehicle = object_get_and_verify_type(vehicle_handle, OBJECT_MASK_VEHICLE);

  vehicle->flags = 0;
  vehicle->stop_time = 0;
  vehicle->airborne_ticks = 0;
  vehicle->upending_type = 0;
  vehicle->upending_ticks = 0;
  vehicle->on_ground_ticks = 0;
  vehicle->speed = 0.0f;
  vehicle->slide = 0.0f;
  vehicle->turn = 0.0f;
  vehicle->wheel = 0.0f;
  vehicle->left_tread = 0.0f;
  vehicle->right_tread = 0.0f;
  vehicle->thrust = 0.0f;
  vehicle->hover = 0.0f;

  csmemset(vehicle->suspension, 0, sizeof(vehicle->suspension));

  vehicle->field_460[0] = 0.0f;
  vehicle->field_460[1] = 0.0f;
  vehicle->field_460[2] = 0.0f;
  vehicle->field_460[3] = 0.0f;
  vehicle->field_460[4] = 0.0f;
  vehicle->field_460[5] = 0.0f;
  vehicle->stuck_mass_point_flags = 0;
}

/* vehicle_new (0x1b5820) — returns true unconditionally. The physics test
 * is made twice in the original. Definition +0x4 is read as a float
 * (half of it raises position.z); 0x253398 is the pooled 0.5f.
 */
bool vehicle_new(int vehicle_handle)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;

  vehicle = object_get_and_verify_type(vehicle_handle, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  vehicle_reset(vehicle_handle);

  if (definition->physics.tag_index == NONE) {
    vehicle->unit.object.flags |= 0x20;
  } else {
    vehicle->unit.object.flags &= 0xffffffdf;
  }

  if (definition->physics.tag_index != NONE) {
    vehicle->unit.object.position.z =
      object_definition_field_04(definition) * REAL_0_5_POOL +
      vehicle->unit.object.position.z;
  }

  return true;
}

/* Animation graph vehicle_animations element (graph +0x24 block, stride
 * 0x74). Its leading bytes are the aiming screen read by
 * aiming_screen_apply. */
typedef struct vehicle_animation_t {
  uint8_t pad_00[0x5c];
  tag_block animations;                       /* +0x5c int16 indices */
  tag_block suspensions;                      /* +0x68 */
} vehicle_animation_t;
cs(vehicle_animation_t, 0x74);
co(vehicle_animation_t, animations, 0x5c);
co(vehicle_animation_t, suspensions, 0x68);

/* vehicle_animation_t::suspensions element (stride 0x14). */
typedef struct vehicle_suspension_t {
  int16_t mass_point_index;                   /* +0x00 */
  int16_t animation_index;                    /* +0x02 */
  real field_04;                              /* +0x04 */
  real field_08;                              /* +0x08 */
  uint8_t pad_0c[8];
} vehicle_suspension_t;
cs(vehicle_suspension_t, 0x14);
co(vehicle_suspension_t, animation_index, 0x02);
co(vehicle_suspension_t, field_08, 0x08);

/*
 * vehicle_preprocess_node_orientations (0x1b5890)
 *
 * Applies the vehicle's continuous overlay animations from the animation
 * graph's first vehicle_animations element. Index [4] is fetched and
 * discarded. 0x261518 is read from memory (byte -> fraction scale).
 */
void vehicle_preprocess_node_orientations(int vehicle_handle, void *node_data)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  animation_graph_t *animation_graph;
  vehicle_animation_t *vehicle_animation;
  animation_t *animation;
  vehicle_suspension_t *suspension;
  short i;
  unsigned char suspension_state;
  float t;
  float v;

  vehicle = object_get_and_verify_type(vehicle_handle, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  if (object_definition_animation_graph_index(definition) == NONE) {
    return;
  }
  animation_graph =
    tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(definition));
  if (animation_graph_definition_field_24(animation_graph).count == 0) {
    return;
  }
  vehicle_animation = tag_block_get_element(
    (char *)animation_graph + 0x24, 0, sizeof(vehicle_animation_t));
  if (vehicle_animation == NULL) {
    return;
  }

  if (vehicle_animation->animations.count > 0) {
    if (((short *)vehicle_animation->animations.address)[0] != NONE) {
      aiming_screen_apply(
        (int)tag_block_get_element(&animation_graph->animations,
                                   ((short *)vehicle_animation->animations.address)[0],
                                   sizeof(animation_t)),
        (float *)vehicle_animation, vehicle->turn, 0.0f, (int)node_data);
    }
  }

  if (vehicle_animation->animations.count > 1) {
    if (((short *)vehicle_animation->animations.address)[1] != NONE) {
      animation = tag_block_get_element(&animation_graph->animations,
                                        ((short *)vehicle_animation->animations.address)[1],
                                        sizeof(animation_t));
      t = (triple_product3d(&vehicle->unit.object.up,
                            &vehicle->unit.object.forward,
                            &vehicle->unit.object.translational_velocity) /
             definition->field_2f8[0] +
           1.0f) *
          0.5f;
      if (t < 0.0f) {
        t = 0.0f;
      } else if (t > 1.0f) {
        t = 1.0f;
      }
      overlay_animation_apply_continuous(
        animation, (float)(animation->frame_count - 1) * t, node_data);
    }
  }

  if (vehicle_animation->animations.count > 2) {
    if (((short *)vehicle_animation->animations.address)[2] != NONE) {
      animation = tag_block_get_element(&animation_graph->animations,
                                        ((short *)vehicle_animation->animations.address)[2],
                                        sizeof(animation_t));
      v = vehicle->speed;
      if (v < 0.0f) {
        t = 0.5f - v / definition->field_2f8[1] * 0.5f;
      } else {
        t = (v / definition->field_2f8[0] + 1.0f) * 0.5f;
      }
      overlay_animation_apply_continuous(
        animation, (float)(animation->frame_count - 1) * t, node_data);
    }
  }

  if (vehicle_animation->animations.count > 3) {
    if (((short *)vehicle_animation->animations.address)[3] != NONE) {
      animation = tag_block_get_element(&animation_graph->animations,
                                        ((short *)vehicle_animation->animations.address)[3],
                                        sizeof(animation_t));
      t = vehicle->unit.object.translational_velocity.k *
            vehicle->unit.object.forward.k +
          vehicle->unit.object.translational_velocity.j *
            vehicle->unit.object.forward.j +
          vehicle->unit.object.translational_velocity.i *
            vehicle->unit.object.forward.i;
      if (t < 0.0f) {
        t = 0.0f;
      } else if (t > 1.0f) {
        t = 1.0f;
      }
      t = t / (float)fabs(definition->field_2f8[0]);
      if (t < 0.0f) {
        t = 0.0f;
      } else if (t > 1.0f) {
        t = 1.0f;
      }
      overlay_animation_apply_continuous(
        animation, (float)(animation->frame_count - 1) * t, node_data);
    }
  }

  if (vehicle_animation->animations.count > 4) {
    if (((short *)vehicle_animation->animations.address)[4] != NONE) {
      tag_block_get_element(&animation_graph->animations,
                            ((short *)vehicle_animation->animations.address)[4],
                            sizeof(animation_t));
    }
  }

  if (vehicle_animation->animations.count > 5) {
    if (((short *)vehicle_animation->animations.address)[5] != NONE) {
      animation = tag_block_get_element(&animation_graph->animations,
                                        ((short *)vehicle_animation->animations.address)[5],
                                        sizeof(animation_t));
      if (definition->wheel_circumference > 0.0f) {
        t = vehicle->wheel / definition->wheel_circumference;
      } else {
        t = 0.0f;
      }
      overlay_animation_apply_continuous(
        animation, (float)(int)animation->frame_count * t, node_data);
    }
  }

  for (i = 0; i < vehicle_animation->suspensions.count; i++) {
    suspension = tag_block_get_element(&vehicle_animation->suspensions, i,
                                       sizeof(vehicle_suspension_t));
    if (suspension->animation_index != NONE) {
      animation = tag_block_get_element(&animation_graph->animations,
                                        suspension->animation_index,
                                        sizeof(animation_t));
      suspension_state = vehicle->suspension[i];
      if (suspension_state == 0xff) {
        t = 1.0f;
      } else {
        t = (float)suspension_state * REAL_ONE_OVER_255_POOL;
      }
      overlay_animation_apply_continuous(
        animation, (float)(animation->frame_count - 1) * t, node_data);
    }
  }
}

/* Largest angular-velocity correction per tick: 0.8 degrees in radians. */
#define VEHICLE_ANGULAR_ACCELERATION 0.0139626348f

/* Inline vector helpers for the physics variants below. The original expands
 * these in place (no CALLs). */
static __inline real vehicle_dot_product2d(const real_vector2d *a,
                                           const real_vector2d *b)
{
  return a->i * b->i + a->j * b->j;
}

static __inline real vehicle_dot_product3d(const real_vector3d *a,
                                           const real_vector3d *b)
{
  return a->i * b->i + a->j * b->j + a->k * b->k;
}

static __inline real vehicle_magnitude_squared3d(const real_vector3d *v)
{
  return v->i * v->i + v->j * v->j + v->k * v->k;
}

/* *current moves toward desired by at most maximum_speed. */
static __inline void vehicle_interpolate_scalar(real *current, real desired,
                                              real maximum_speed)
{
  real difference = desired - *current;

  if (difference < -maximum_speed) {
    difference = -maximum_speed;
  } else if (difference > maximum_speed) {
    difference = maximum_speed;
  }
  *current += difference;
}

/* result = a x b, written component by component (no temporaries). */
static __inline void vehicle_cross_product3d_in_place(const real_vector3d *a,
                                                     const real_vector3d *b,
                                                     real_vector3d *result)
{
  result->i = a->j * b->k - b->j * a->k;
  result->j = a->k * b->i - a->i * b->k;
  result->k = b->j * a->i - a->j * b->i;
}

/* (b x a) . c, the cross product expanded in the original's operand order. */
static __inline real vehicle_triple_product3d(const real_vector3d *a,
                                              const real_vector3d *b,
                                              const real_vector3d *c)
{
  real_vector3d cross;

  cross.i = a->k * b->j - a->j * b->k;
  cross.j = b->k * a->i - a->k * b->i;
  cross.k = a->j * b->i - b->j * a->i;
  return vehicle_dot_product3d(&cross, c);
}

static __inline real_vector3d *vehicle_cross_product3d(const real_vector3d *a,
                                                       const real_vector3d *b,
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

/*
 * vehicle_accelerate (0x1b5c90)
 *
 * Adds an impulse to a vehicle's velocity and, when the vehicle has a
 * physics tag, spins it about (global_up x acceleration) by pi times the
 * horizontal impulse size; then clears object flag bit 5.
 */
void vehicle_accelerate(int handle, const real_vector3d *acceleration)
{
  vehicle_data_t *vehicle =
    object_get_and_verify_type(handle, OBJECT_MASK_VEHICLE);
  vehicle_definition_t *definition = tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);

  if (definition->physics.tag_index != -1) {
    real_vector3d *object_velocity =
      &vehicle->unit.object.translational_velocity;
    real_vector3d *object_angular_velocity =
      &vehicle->unit.object.angular_velocity;
    real_vector3d torque;
    real magnitude;

    tag_get(TAG_GROUP_PHYS, definition->physics.tag_index);

    object_velocity->i = object_velocity->i + acceleration->i;
    object_velocity->j = object_velocity->j + acceleration->j;
    object_velocity->k = object_velocity->k + acceleration->k;

    vehicle_cross_product3d_in_place(global_up_vector3d, acceleration, &torque);

    magnitude = normalize3d(&torque);
    if (magnitude > 0.0f) {
      magnitude = magnitude * _pi;
      torque.i = torque.i * magnitude;
      torque.j = torque.j * magnitude;
      torque.k = torque.k * magnitude;

      object_angular_velocity->i = torque.i + object_angular_velocity->i;
      object_angular_velocity->j = torque.j + object_angular_velocity->j;
      object_angular_velocity->k = torque.k + object_angular_velocity->k;
    }

    vehicle->unit.object.flags &= ~0x20;
  }
}

/*
 * vehicle_render_debug (0x1b5d90) — debug walk over the 'phys' mass points,
 * gated on the debug byte at 0x5054f4. The loop body is empty in this build.
 */
void vehicle_render_debug(int vehicle_handle)
{
  vehicle_definition_t *definition;
  struct physics_definition *physics;
  int physics_index;
  short i;

  definition = tag_get(
    TAG_GROUP_VEHI,
    *(int *)object_get_and_verify_type(vehicle_handle, OBJECT_MASK_VEHICLE));
  physics_index = definition->physics.tag_index;
  if (physics_index != NONE) {
    physics = tag_get(TAG_GROUP_PHYS, physics_index);
    if (DEBUG_OBJECTS_VEHICLE_POWERED_MASS_POINTS != 0) {
      for (i = 0; (int)i < physics->mass_points.count; i++) {
      }
    }
  }
}

/*
 * vehicle_get_estimated_position (0x1b5df0) — ground point under the vehicle.
 *
 * For vehicle types 0, 1, 4 and 6 casts a ray from 0.4 above the vehicle
 * along 2 * the vector at 0x31fc50 (0,0,-1) and writes the hit point to
 * out_position; returns the dword at result+0x8 on a hit, else NONE. The
 * 0x434-byte collision_bsp_test_vector result keeps t at +0x0.
 */
int vehicle_get_estimated_position(int vehicle_handle, vector3_t *out_position)
{
  void *bsp;
  real_point3d start;
  real_vector3d vector;
  float result[0x10d];
  int default_ret;
  int16_t vehicle_type;
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;

  vehicle = object_get_and_verify_type(vehicle_handle, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  default_ret = NONE;

  object_get_world_position(vehicle_handle, out_position);

  vehicle_type = definition->vehicle_type;
  switch (vehicle_type) {
  case 0:
  case 1:
  case 4:
  case 6:
    break;
  default:
    return default_ret;
  }

  bsp = global_collision_bsp_get();

  object_get_world_position(vehicle_handle, &start);

  start.x = global_up_vector3d->i * 0.4f + start.x;
  start.y = global_up_vector3d->j * 0.4f + start.y;
  start.z = global_up_vector3d->k * 0.4f + start.z;

  {
    real_vector3d *down = GLOBAL_DOWN3D;
    vector.i = down->i + down->i;
    vector.j = down->j + down->j;
    vector.k = down->k + down->k;
  }

  if (!collision_bsp_test_vector(1, (int)bsp, 0, 0, (int)&start, (int)&vector,
                                 3.4028235e+38f, result)) {
    return default_ret;
  }

  out_position->x = vector.i * result[0] + start.x;
  out_position->y = vector.j * result[0] + start.y;
  out_position->z = vector.k * result[0] + start.z;

  return *(int *)&result[2];
}

/*
 * compute_acceleration (0x1b5f20)
 *
 * acceleration = desired_velocity - velocity, plus global_gravity on k, then
 * clamped by limit3d to a length that blends from minimum (moving away from
 * the desired velocity) to maximum (moving along it) by the squared cosine
 * between desired_velocity and the acceleration. Returns acceleration.
 */
real_vector3d *compute_acceleration(const real_vector3d *desired_velocity,
                                    const real_vector3d *velocity,
                                    real_vector3d *acceleration, real maximum,
                                    real minimum)
{
  real dot;

  acceleration->i = desired_velocity->i - velocity->i;
  acceleration->j = desired_velocity->j - velocity->j;
  acceleration->k = desired_velocity->k - velocity->k;
  acceleration->k += global_gravity;

  dot = vehicle_dot_product3d(desired_velocity, acceleration);
  if (dot > 0.0001f) {
    maximum = (maximum - minimum) *
                ((dot * dot / vehicle_magnitude_squared3d(acceleration)) /
                 vehicle_magnitude_squared3d(desired_velocity)) +
              minimum;
  } else {
    maximum = minimum;
  }

  limit3d(acceleration, maximum);

  return acceleration;
}

/*
 * update_human_tank_physics (0x1b5ff0)
 *
 * Per-tick tank physics: advances the left/right tread accumulators by
 * speed -/+ turn, wraps each into [0, wheel_circumference) via fmod, then —
 * only when the 'phys' tag has exactly 2 powered mass points — gives each
 * tread its ground friction velocity with an identity rotation and hands off
 * to physics_update. Otherwise powered_mass_points is passed as NULL.
 */
void update_human_tank_physics(int vehicle_index, void *mass_points,
                               void *powered_mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;
  struct powered_mass_point_datum *state;
  real left;
  real right;

  vehicle = object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = tag_get(
    TAG_GROUP_PHYS, definition->physics.tag_index);

  left = vehicle->speed - vehicle->turn;
  right = vehicle->turn + vehicle->speed;

  vehicle->left_tread = left + vehicle->left_tread;
  vehicle->left_tread =
    VEHICLE_FMOD(vehicle->left_tread, definition->wheel_circumference);
  if (vehicle->left_tread < 0.0f) {
    vehicle->left_tread += definition->wheel_circumference;
  }

  vehicle->right_tread = right + vehicle->right_tread;
  vehicle->right_tread =
    VEHICLE_FMOD(vehicle->right_tread, definition->wheel_circumference);
  if (vehicle->right_tread < 0.0f) {
    vehicle->right_tread += definition->wheel_circumference;
  }

  if (physics->powered_mass_points.count == 2) {
    state = (struct powered_mass_point_datum *)powered_mass_points;

    state[0].ground_friction_velocity = left;
    state[0].rotation[0] = 0.0f;
    state[0].rotation[1] = 0.0f;
    state[0].rotation[2] = 0.0f;
    state[0].rotation[3] = 1.0f;

    state[1].ground_friction_velocity = right;
    state[1].rotation[0] = 0.0f;
    state[1].rotation[1] = 0.0f;
    state[1].rotation[2] = 0.0f;
    state[1].rotation[3] = 1.0f;

    physics_update(vehicle_index, state, mass_points, NULL, NULL);
  } else {
    physics_update(vehicle_index, NULL, mass_points, NULL, NULL);
  }
}

/*
 * update_human_jeep_physics (0x1b6140)
 *
 * Advances the wheel spin by speed, wrapped into [0, wheel_circumference)
 * via fmod; with exactly two powered mass points both get ground friction
 * velocity = speed and a steering rotation of +/- turn/2 about k, then
 * physics_update. Register arg: powered_mass_points@<edi>. FCOS is
 * evaluated before FSIN.
 */
void update_human_jeep_physics(int vehicle_index, void *mass_points,
                                void *powered_mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;
  float wheel;
  float half_turn;
  float sin_half_turn;
  float cos_half_turn;
  float speed;
  struct powered_mass_point_datum *left;
  struct powered_mass_point_datum *right;

  vehicle = object_get_and_verify_type(vehicle_index, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = tag_get(TAG_GROUP_PHYS, definition->physics.tag_index);

  wheel = vehicle->speed + vehicle->wheel;
  wheel = x87_fmod(wheel, definition->wheel_circumference);
  if (wheel < 0.0f) {
    wheel += definition->wheel_circumference;
  }
  vehicle->wheel = wheel;

  if (physics->powered_mass_points.count != 2) {
    physics_update(vehicle_index, NULL, mass_points, NULL, NULL);
    return;
  }

  speed = vehicle->speed;
  half_turn = vehicle->turn * 0.5f;
  cos_half_turn = x87_fcos(half_turn);
  sin_half_turn = x87_fsin(half_turn);

  left = powered_mass_points;
  right = left + 1;

  left->ground_friction_velocity = speed;
  left->rotation[0] = 0.0f;
  left->rotation[1] = 0.0f;
  left->rotation[2] = sin_half_turn;
  left->rotation[3] = cos_half_turn;

  right->ground_friction_velocity = speed;
  right->rotation[0] = 0.0f;
  right->rotation[1] = 0.0f;
  right->rotation[2] = -sin_half_turn;
  right->rotation[3] = cos_half_turn;

  physics_update(vehicle_index, powered_mass_points, mass_points, NULL, NULL);
}

/*
 * update_human_boat_physics (0x1b6250)
 *
 * Boat physics. With exactly three powered mass points: point 0 carries the
 * throttle as water friction velocity and a rudder rotation that shrinks with
 * speed; points 1 and 2 get fixed lift ratios and identity rotations. A
 * roll torque about the forward axis pulls the hull toward an up vector that
 * is banked by the sideways velocity. Otherwise physics_update runs with no
 * powered state and no magic terms.
 */
void update_human_boat_physics(int vehicle_index, void *mass_points,
                               void *powered_mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;

  vehicle = object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = tag_get(
    TAG_GROUP_PHYS, definition->physics.tag_index);

  if (physics->powered_mass_points.count == 3) {
    struct powered_mass_point_datum *state =
      (struct powered_mass_point_datum *)powered_mass_points;
    object_data_t *object = &vehicle->unit.object;
    real_vector3d zero;
    real_vector3d cross;
    real_vector3d thrust;
    real_vector3d up_relative;
    real dot;
    real angle;
    int sign;
    real speed;
    real negative_k;
    real spin;
    real maximum_angle;
    real *rotation;
    const real_vector3d *forward;

    speed = x87_fabs(x87_sqrt(vehicle_magnitude_squared3d(
                       &object->translational_velocity)) *
                     2.5f);
    maximum_angle = vehicle->turn * 0.5f;
    angle = 1.0f - (speed > 1.0f ? 1.0f : speed);

    state[0].water_friction_velocity = vehicle->speed;
    state[0].water_lift_ratio = 0.003f;
    angle *= maximum_angle;
    rotation = state[0].rotation;
    rotation[0] = 0.0f;
    rotation[2] = x87_fsin(angle);
    rotation[1] = 0.0f;
    rotation[3] = x87_fcos(angle);
    zero.i = 0.0f;
    zero.j = 0.0f;
    zero.k = 0.0f;

    forward = &object->forward;

    state[1].water_lift_ratio = 0.003f;
    state[1].rotation[0] = 0.0f;
    state[1].rotation[1] = 0.0f;
    state[1].rotation[2] = 0.0f;
    state[1].rotation[3] = 1.0f;

    state[2].water_lift_ratio = 0.005f;
    state[2].rotation[0] = 0.0f;
    state[2].rotation[1] = 0.0f;
    state[2].rotation[2] = 0.0f;
    state[2].rotation[3] = 1.0f;

    negative_k = -object->forward.k;

    up_relative.i = forward->i * negative_k + global_up_vector3d->i;
    up_relative.j = forward->j * negative_k + global_up_vector3d->j;
    up_relative.k = forward->k * negative_k + global_up_vector3d->k;

    if (normalize3d(&up_relative) != 0.0f) {
      const real_vector3d *up = &object->up;
      const real_vector3d *velocity = &object->translational_velocity;
      const real_vector3d *forward3d = forward;

      vehicle_cross_product3d_in_place(up, forward3d, &cross);

      spin = vehicle_triple_product3d(velocity, forward3d, global_up_vector3d) *
             (2.0f * _pi);

      rotate_vector3d_by_sincos(&up_relative, forward,
                                x87_fsin(spin), x87_fcos(spin));

      angle = angle_between_vectors3d(&up_relative, &object->up);

      if (vehicle_dot_product3d(&cross, &up_relative) > 0.0f) {
        angle = -angle;
      }

      dot = vehicle_dot_product3d(forward, &object->angular_velocity);

      sign = angle != 0.0f ? (angle < 0.0f ? -1 : 1) : 0;

      {
        real correction =
          (real)(x87_sqrtd(x87_fabs(angle) * 2.0 * VEHICLE_ANGULAR_ACCELERATION) *
                   sign -
                 dot);

        if (correction < -VEHICLE_ANGULAR_ACCELERATION) {
          correction = -VEHICLE_ANGULAR_ACCELERATION;
        } else if (correction > VEHICLE_ANGULAR_ACCELERATION) {
          correction = VEHICLE_ANGULAR_ACCELERATION;
        }
        scale_vector3d_inline(forward,
                       correction * physics->xx_moment, &thrust);
      }
    } else {
      thrust.i = 0.0f;
      thrust.j = 0.0f;
      thrust.k = 0.0f;
    }

    physics_update(vehicle_index, state, mass_points, &zero, &thrust);
  } else {
    physics_update(vehicle_index, NULL, mass_points, NULL, NULL);
  }
}

/*
 * update_alien_fighter_physics_new (0x1b6560)
 *
 * Banshee physics (the variant used when the 'phys' tag's first float is not
 * positive). With exactly two powered mass points: a magic force that seeks
 * forward * speed through compute_acceleration, a magic torque that rotates
 * the vehicle toward its desired facing (pitched by definition +0x364 unless
 * an AI drives it, then yawed by sideways velocity), a thrust fraction that
 * follows the spin rate, and identity rotations for both powered points.
 * Otherwise physics_update runs with no powered state and no magic terms.
 */
void update_alien_fighter_physics_new(int vehicle_index,
                                      void *powered_mass_points,
                                      void *mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;

  vehicle = object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = tag_get(
    TAG_GROUP_PHYS, definition->physics.tag_index);

  if (physics->powered_mass_points.count == 2) {
    struct powered_mass_point_datum *state =
      (struct powered_mass_point_datum *)powered_mass_points;
    const real_vector3d *object_forward =
      &vehicle->unit.object.forward;
    const real_vector3d *object_angular_velocity =
      &vehicle->unit.object.angular_velocity;
    real_vector3d magic_force;
    real_vector3d magic_torque;
    real_vector3d axis;
    real_vector3d desired_angular_velocity;
    real_vector3d angular_acceleration;
    real spin;
    real thrust_delta;

    {
      real_vector3d desired_velocity;
      real_vector3d acceleration;
      real throttle;

      desired_velocity.i = object_forward->i * vehicle->speed;
      desired_velocity.j = object_forward->j * vehicle->speed;
      desired_velocity.k = object_forward->k * vehicle->speed;

      if (vehicle->speed > 0.0f) {
        throttle = vehicle->speed / definition->field_2f8[0];
      } else {
        throttle = -(vehicle->speed / definition->field_2f8[1]);
      }

      compute_acceleration(
        &desired_velocity,
        &vehicle->unit.object.translational_velocity,
        &acceleration, throttle * definition->field_2f8[2],
        throttle * definition->field_2f8[3]);

      magic_force.i = acceleration.i * physics->mass;
      magic_force.j = acceleration.j * physics->mass;
      magic_force.k = acceleration.k * physics->mass;
      magic_force.i = magic_force.i * vehicle->unit.seat_power[0];
      magic_force.j = magic_force.j * vehicle->unit.seat_power[0];
      magic_force.k = magic_force.k * vehicle->unit.seat_power[0];
    }

    {
      real_matrix3x3 current_rotation;
      real_matrix3x3 desired_rotation;
      real_matrix3x3 rotation;
      real_quaternion rotation_quaternion;
      real yaw;
      real angle;
      real t;
      real scale;

      matrix3x3_from_forward_and_up((float *)&current_rotation,
                   (float *)&vehicle->unit.object.forward,
                   (float *)&vehicle->unit.object.up);

      desired_rotation.forward =
        *(real_vector3d *)&vehicle->unit.desired_facing_vector;

      /* point on the line through global_up along forward, at -forward.k */
      t = -desired_rotation.forward.k;
      desired_rotation.up.i =
        desired_rotation.forward.i * t + global_up_vector3d->i;
      desired_rotation.up.j =
        desired_rotation.forward.j * t + global_up_vector3d->j;
      desired_rotation.up.k =
        desired_rotation.forward.k * t + global_up_vector3d->k;

      if (normalize3d(&desired_rotation.up) == 0.0f) {
        desired_rotation.up = *global_forward_vector3d;
      }

      if (!unit_driven_by_ai(vehicle_index)) {
        pitch_vectors(&desired_rotation.forward, &desired_rotation.up,
                     x87_fsin(definition->field_364),
                     x87_fcos(definition->field_364));
      }

      yaw = (desired_rotation.forward.i *
               vehicle->unit.object.translational_velocity.j -
             desired_rotation.forward.j *
               vehicle->unit.object.translational_velocity.i) /
            definition->field_2f8[0] * definition->field_308;

      yaw_vectors(&desired_rotation.up, &desired_rotation.forward,
                  x87_fsin(yaw), x87_fcos(yaw));

      desired_rotation.left.i =
        desired_rotation.up.j * desired_rotation.forward.k -
        desired_rotation.up.k * desired_rotation.forward.j;
      desired_rotation.left.j =
        desired_rotation.up.k * desired_rotation.forward.i -
        desired_rotation.up.i * desired_rotation.forward.k;
      desired_rotation.left.k =
        desired_rotation.forward.j * desired_rotation.up.i -
        desired_rotation.up.j * desired_rotation.forward.i;

      matrix3x3_transpose((float *)&current_rotation, (float *)&current_rotation);
      matrix3x3_multiply((float *)&desired_rotation, (float *)&current_rotation,
                   (float *)&rotation);
      matrix3x3_rotation_to_quaternion((float *)&rotation, (float *)&rotation_quaternion);
      quaternion_to_angle_and_vector(&rotation_quaternion, &angle, &axis);

      scale = -angle * definition->field_314 * (1.0f / _pi);
      desired_angular_velocity.i = axis.i * scale;
      desired_angular_velocity.j = axis.j * scale;
      desired_angular_velocity.k = axis.k * scale;
    }

    angular_acceleration.i =
      desired_angular_velocity.i - object_angular_velocity->i;
    angular_acceleration.j =
      desired_angular_velocity.j - object_angular_velocity->j;
    angular_acceleration.k =
      desired_angular_velocity.k - object_angular_velocity->k;

    {
      real moment =
        (physics->zz_moment + physics->yy_moment + physics->xx_moment) *
        (1.0f / 3);

      magic_torque.i = angular_acceleration.i * moment;
      magic_torque.j = angular_acceleration.j * moment;
      magic_torque.k = angular_acceleration.k * moment;
    }
    magic_torque.i = magic_torque.i * vehicle->unit.seat_power[0];
    magic_torque.j = magic_torque.j * vehicle->unit.seat_power[0];
    magic_torque.k = magic_torque.k * vehicle->unit.seat_power[0];

    spin = (real)(x87_sqrtd(vehicle_magnitude_squared3d(
                    object_angular_velocity)) /
                  definition->field_314);

    if (spin > vehicle->thrust) {
      thrust_delta = (1.0f - vehicle->thrust) * (1.0f - vehicle->thrust) * 0.2f;
      if (thrust_delta < 0.01f) {
        thrust_delta = 0.01f;
      } else if (thrust_delta > 0.05f) {
        thrust_delta = 0.05f;
      }
      if (!(spin - vehicle->thrust > thrust_delta)) {
        thrust_delta = spin - vehicle->thrust;
      }
    } else {
      thrust_delta = vehicle->thrust * vehicle->thrust * 0.05f;
      if (!(thrust_delta > 0.005f)) {
        thrust_delta = 0.005f;
      }
      thrust_delta = -thrust_delta;
      if (spin - vehicle->thrust > thrust_delta) {
        thrust_delta = spin - vehicle->thrust;
      }
    }

    vehicle->thrust += thrust_delta;

    state[0].antigrav_fraction = vehicle->unit.seat_power[0];
    *(real_quaternion *)state[0].rotation =
      *(real_quaternion *)global_identity_quaternion_ptr;
    state[1].antigrav_fraction = vehicle->unit.seat_power[0];
    *(real_quaternion *)state[1].rotation =
      *(real_quaternion *)global_identity_quaternion_ptr;

    physics_update(vehicle_index, state, mass_points, &magic_force,
                   &magic_torque);
  } else {
    physics_update(vehicle_index, NULL, mass_points, NULL, NULL);
  }
}

/*
 * update_alien_fighter_physics_old (0x1b69a0)
 *
 * Banshee physics (the variant used when the 'phys' tag's radius is positive).
 * With exactly two powered mass points: a magic force that lifts against
 * gravity in proportion to forward speed and thrusts toward vehicle.speed, a
 * magic torque that turns the vehicle toward its desired facing (yawed by
 * sideways velocity), and identity rotations for both powered points.
 * Otherwise physics_update runs with no powered state and no magic terms.
 */
void update_alien_fighter_physics_old(int vehicle_index,
                                      void *mass_points,
                                      void *powered_mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;

  vehicle = object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = tag_get(
    TAG_GROUP_PHYS, definition->physics.tag_index);

  if (physics->powered_mass_points.count == 2) {
    struct powered_mass_point_datum *state =
      (struct powered_mass_point_datum *)powered_mass_points;
    object_data_t *object = &vehicle->unit.object;
    real_vector3d facing = *(real_vector3d *)&vehicle->unit.desired_facing_vector;
    real_vector3d perpendicular = *global_up_vector3d;
    real_matrix4x3 actual;
    real_matrix4x3 desired;
    real_matrix4x3 difference;
    real_vector3d axis;
    real_vector3d force;
    real_vector3d torque;
    real_vector3d scaled;
    real_vector2d velocity;
    real yaw;
    real speed;
    real thrust;
    real lift;
    real scale;

    perpendicular.i = -facing.k * facing.i;
    perpendicular.j = -(facing.k * facing.j);
    perpendicular.k = 1.0f - facing.k * facing.k;

    if (normalize3d(&perpendicular) == 0.0f) {
      perpendicular.i = 1.0f;
      perpendicular.j = 0.0f;
      perpendicular.k = 0.0f;
    }

    speed = vehicle_dot_product3d(
      &object->translational_velocity,
      &object->forward);
    thrust = (vehicle->speed - speed) * physics->mass * 0.05f;
    lift = (x87_fabs(speed / definition->field_2f8[0]) * physics->mass) *
           global_gravity * 1.05f;

    force.i = lift * object->up.i + thrust * object->forward.i;
    force.j = lift * object->up.j + thrust * object->forward.j;
    force.k = lift * object->up.k + thrust * object->forward.k;

    {
      real *destination = (real *)&velocity;
      const real *source = (const real *)&object->translational_velocity;
      short component_index;

      for (component_index = 0; component_index < 2; component_index++) {
        destination[component_index] = source[component_index];
      }
    }

    yaw = (facing.i * velocity.j - facing.j * velocity.i) *
          (_pi * 0.5f) / x87_fabs(definition->field_2f8[0]);

    yaw_vectors(&perpendicular, &facing, x87_fsin(yaw), x87_fcos(yaw));

    {
      real_quaternion rotation;
      real angle;

      matrix_from_forward_and_up(&actual, &object->forward,
                                 &object->up);
      matrix_from_forward_and_up(&desired, &facing,
                                 &perpendicular);
      matrix_inverse((float *)&desired, (float *)&desired);
      matrix4x3_multiply((float *)&actual, (float *)&desired,
                         (float *)&difference);
      matrix4x3_rotation_to_quaternion((float *)&difference, (float *)&rotation);
      quaternion_to_angle_and_vector(&rotation, &angle, &axis);
      scale_vector3d_inline(&axis, angle * (4.0f / 30.0f), &scaled);
    }

    scale = physics->radius * physics->radius * physics->mass * 0.05f;

    torque.i = (scaled.i - object->angular_velocity.i) * scale;
    torque.j = (scaled.j - object->angular_velocity.j) * scale;
    torque.k = (scaled.k - object->angular_velocity.k) * scale;

    state[0].antigrav_fraction = vehicle->unit.seat_power[0];
    state[0].rotation[3] = 1.0f;
    state[0].rotation[0] = 0.0f;
    state[0].rotation[1] = 0.0f;
    state[0].rotation[2] = 0.0f;

    state[1].antigrav_fraction = vehicle->unit.seat_power[0];
    state[1].rotation[3] = 1.0f;
    state[1].rotation[0] = 0.0f;
    state[1].rotation[1] = 0.0f;
    state[1].rotation[2] = 0.0f;

    scale_vector3d_inline(&force, vehicle->unit.seat_power[0], &force);
    scale_vector3d_inline(&torque, vehicle->unit.seat_power[0], &torque);

    physics_update(vehicle_index, state, mass_points, &force, &torque);
  } else {
    physics_update(vehicle_index, NULL, mass_points, NULL, NULL);
  }
}

/*
 * slowly_stop_vehicle (0x1b6ca0)
 *
 * Coasts a vehicle whose stop timer (+0x426) is running: counts the timer
 * down, damps linear and angular velocity by 0.835, advances the position by
 * one tick of velocity and the basis by one tick of angular velocity, zeroes
 * both velocities when the timer reaches zero, then object_set_position.
 */
void slowly_stop_vehicle(int vehicle_index)
{
  vehicle_data_t *vehicle = object_get_and_verify_type(
    vehicle_index, OBJECT_MASK_VEHICLE);
  real_point3d *object_position = &vehicle->unit.object.position;
  real_vector3d *velocity =
    &vehicle->unit.object.translational_velocity;
  real_vector3d *angular_velocity =
    &vehicle->unit.object.angular_velocity;
  real_matrix4x3 rotation;
  real_point3d position;
  real_vector3d forward;
  real_vector3d up;
  real magnitude;

  vehicle->stop_time--;

  velocity->i *= 0.835f;
  velocity->j *= 0.835f;
  velocity->k *= 0.835f;
  angular_velocity->i *= 0.835f;
  angular_velocity->j *= 0.835f;
  angular_velocity->k *= 0.835f;

  position.x = velocity->i + object_position->x;
  position.y = object_position->y + velocity->j;
  position.z = object_position->z + velocity->k;

  {
    real_vector3d axis;

    axis = *angular_velocity;

    magnitude = normalize3d(&axis);
    if (magnitude != 0.0f) {
      matrix4x3_rotation_from_axis_and_angle((float *)&rotation, &axis, x87_fsin(magnitude),
                   x87_fcos(magnitude));
      matrix_scale_transform_vector(&rotation,
                                    &vehicle->unit.object.forward,
                                    &forward);
      matrix_scale_transform_vector(&rotation,
                                    &vehicle->unit.object.up, &up);
    } else {
      forward = vehicle->unit.object.forward;
      up = vehicle->unit.object.up;
    }
  }

  if (!vehicle->stop_time) {
    *velocity = *(real_vector3d *)global_zero_vector_ptr;
    *angular_velocity = *(real_vector3d *)global_zero_vector_ptr;
  }

  object_set_position(vehicle_index, &position, &forward, &up);
}

/*
 * create_pelican_effect (0x1b6e20)
 *
 * Spawns the definition's thruster-wash effect where a jittered ray from
 * each "hover thrusters" / "jet thrusters" marker hits the world. The ray
 * length is hover (hover markers) or thrust (jet markers) * 6 + 2; the
 * effect is scaled by 1 - collision.t. 0x254640 (6.0f) and 0x2533c8 (1.0f)
 * are pooled constants read from memory.
 */
void create_pelican_effect(int vehicle_handle)
{
  object_marker markers[16];
  struct collision_result collision;
  real_vector3d marker_forwards[3];
  real_point3d marker_points[3];
  const char *marker_names[3];
  real_vector3d velocity;
  float fade;
  real_vector3d direction;
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  object_marker *marker;
  int16_t hover_count;
  int16_t jet_count;
  int marker_count;
  int16_t i;
  float speed;

  vehicle = object_get_and_verify_type(vehicle_handle, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  if (definition->effect.tag_index == NONE) {
    return;
  }

  hover_count = object_get_marker_by_name(
    vehicle_handle, (void *)"hover thrusters", markers, 0xf);
  jet_count = object_get_marker_by_name(
    vehicle_handle, (void *)"jet thrusters", &markers[hover_count],
    0x10 - (int)hover_count);
  marker_count = (int)hover_count + (int)jet_count;

  i = 0;
  if (marker_count <= 0) {
    return;
  }

  do {
    marker = &markers[i];
    seed_random_vector_in_cone3d(random_math_get_local_seed_address(),
                                 &marker->matrix.forward, 0.0f, 0.2617994f,
                                 &direction);

    if (i < hover_count) {
      speed = vehicle->hover;
    } else {
      speed = vehicle->thrust;
    }
    speed = speed * REAL_6_0_POOL + REAL_2_0_POOL;
    velocity.i = direction.i * speed;
    velocity.j = direction.j * speed;
    velocity.k = direction.k * speed;

    if (collision_test_vector(0x61, &marker->matrix.position, &velocity,
                              vehicle_handle, (collision_test_result *)&collision)) {
      marker_forwards[0].i = -direction.i;
      marker_forwards[0].j = -direction.j;
      marker_forwards[0].k = -direction.k;
      marker_forwards[1] = collision.plane.normal;

      marker_points[0] = collision.point;
      marker_points[1] = marker_points[0];
      marker_points[2] = marker_points[0];

      marker_names[0] = "incident";
      marker_names[1] = "normal";
      marker_names[2] = "reflected";

      reflect_vector3d(&direction, &collision.plane.normal,
                   &marker_forwards[2]);

      fade = REAL_ONE_POOL - collision.t;
      effect_new_unattached_from_markers(
        definition->effect.tag_index, NONE, NULL, 3, marker_names,
        &marker_points[0].x, &marker_forwards[0].i, fade, fade, 0.0f, 0.0f, 1);
    }
    i = i + 1;
  } while ((int)i < marker_count);
}

/*
 * create_ghost_effect (0x1b7020)
 *
 * Spawns the vehicle's thruster-wash effect (definition->effect) where each
 * "hover thrusters" marker's jittered forward ray hits the world, while seat
 * power 0 is positive.
 */
void create_ghost_effect(int vehicle_index)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  object_marker markers[16];
  struct collision_result collision;
  real_vector3d marker_forwards[4];
  real_point3d marker_points[4];
  const char *marker_names[4];
  real_vector3d vector;
  real_point3d midpoint;
  real_vector3d direction;
  real scale;
  object_marker *marker;
  int16_t hover_marker_count;
  int16_t jet_marker_count;
  int16_t marker_index;

  vehicle = object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);

  if (definition->effect.tag_index != NONE &&
      vehicle->unit.seat_power[0] > 0.0f) {
    hover_marker_count = object_get_marker_by_name(
      vehicle_index, (void *)"hover thrusters", markers, 15);
    jet_marker_count = 0;

    for (marker_index = 0;
         marker_index < hover_marker_count + jet_marker_count;
         marker_index++) {
      marker = &markers[marker_index];

      seed_random_vector_in_cone3d(
        random_math_get_local_seed_address(),
        &marker->matrix.forward, 0.0f, 15.0f, &direction);

      vector = direction;

      if (collision_test_vector(0x61, &marker->matrix.position, &vector,
                       vehicle_index, (collision_test_result *)&collision)) {
#define GHOST_WASH_SCALE \
  (-marker->matrix.forward.k * (1.0f - collision.t) * vehicle->unit.seat_power[0])
        scale = GHOST_WASH_SCALE < 0.0f   ? 0.0f
                : GHOST_WASH_SCALE > 1.0f ? 1.0f
                                          : GHOST_WASH_SCALE;
#undef GHOST_WASH_SCALE

        if (scale > 0.0f) {
          midpoint.x = (collision.point.x + marker->matrix.position.x) * 0.5f;
          midpoint.y = (collision.point.y + marker->matrix.position.y) * 0.5f;
          midpoint.z = (collision.point.z + marker->matrix.position.z) * 0.5f;
          marker_points[0] = collision.point;
          marker_points[1] = collision.point;
          marker_forwards[0].i = -direction.i;
          marker_names[0] = "incident";
          marker_forwards[0].j = -direction.j;
          marker_names[1] = "normal";
          marker_forwards[1] = collision.plane.normal;
          marker_points[2] = collision.point;
          marker_forwards[0].k = -direction.k;
          marker_names[2] = "reflected";

          reflect_vector3d(&direction, &collision.plane.normal,
                       &marker_forwards[2]);

          marker_points[3] = midpoint;
          marker_names[3] = "midpoint";

          reflect_vector3d(&direction, &collision.plane.normal,
                       &marker_forwards[3]);

          effect_new_unattached_from_markers(
            definition->effect.tag_index, NONE, NULL, 4,
            (void *)marker_names, &marker_points[0].x, &marker_forwards[0].i,
            scale, scale, 0.0f, 0.0f, 1);
        }
      }
    }
  }
}

/* Game globals falling_damage block element (globals +0x188, element size
 * 0x98). */
typedef struct game_globals_falling_damage_t {
  uint8_t pad_00[0x2c];
  tag_reference maximum_distance_damage;      /* +0x2c */
  tag_reference vehicle_hit_environment_damage_effect; /* +0x3c */
  uint8_t pad_4c[0x40];
  real runtime_maximum_falling_velocity;      /* +0x8c */
  uint8_t pad_90[8];
} game_globals_falling_damage_t;
cs(game_globals_falling_damage_t, 0x98);
co(game_globals_falling_damage_t, maximum_distance_damage, 0x2c);
co(game_globals_falling_damage_t, vehicle_hit_environment_damage_effect, 0x3c);
co(game_globals_falling_damage_t, runtime_maximum_falling_velocity, 0x8c);

/* create_crashing_effects (0x1b72b0)
 *
 * When the velocity change since previous_velocity exceeds 0.02 and some
 * mass point is touching (flag 0x2), applies the vehicle hit environment
 * damage effect to the vehicle and plays the definition's crash sound
 * (+0x3cc), both scaled by clamp((|dv| - 0.02) * 45.454544, 0, 1).
 */
void create_crashing_effects(int vehicle_index, const real_vector3d *previous_velocity,
                             void *mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;
  game_globals_falling_damage_t *falling_damage;
  struct damage_data damage;
  real_vector3d delta;
  float magnitude;
  float scale;
  int16_t i;

  vehicle = object_get_and_verify_type(vehicle_index, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = tag_get(TAG_GROUP_PHYS, definition->physics.tag_index);
  falling_damage = tag_block_get_element(
    &game_globals_falling_damage(game_globals_get()), 0,
    sizeof(game_globals_falling_damage_t));
  if (falling_damage->vehicle_hit_environment_damage_effect.tag_index == NONE &&
      vehicle_definition_crash_sound_index(definition) == NONE)
    return;
  delta.i = vehicle->unit.object.translational_velocity.i - previous_velocity->i;
  delta.j = vehicle->unit.object.translational_velocity.j - previous_velocity->j;
  delta.k = vehicle->unit.object.translational_velocity.k - previous_velocity->k;
  magnitude =
    x87_sqrt(delta.k * delta.k + delta.j * delta.j + delta.i * delta.i);
  if (!(magnitude > 0.02f))
    return;
  for (i = 0; i < physics->mass_points.count; i++) {
    tag_block_get_element(&physics->mass_points, i,
                          sizeof(struct mass_point_definition));
    if (((struct mass_point_datum *)mass_points)[i].flags & 0x2)
      break;
  }
  if (!(i < physics->mass_points.count))
    return;

  scale = (magnitude - 0.02f) * 45.454544f;
  if (falling_damage->vehicle_hit_environment_damage_effect.tag_index != NONE) {
    damage_data_new(&damage, falling_damage->vehicle_hit_environment_damage_effect.tag_index);
    if (scale < 0.0f)
      damage.scale = 0.0f;
    else if (scale > 1.0f)
      damage.scale = 1.0f;
    else
      damage.scale = scale;
    damage.origin.x = vehicle->unit.object.bounding_sphere_center.x;
    damage.origin.y = vehicle->unit.object.bounding_sphere_center.y;
    damage.origin.z = vehicle->unit.object.bounding_sphere_center.z;
    damage.direction.i = delta.i;
    damage.direction.j = delta.j;
    damage.direction.k = delta.k;
    object_cause_damage(&damage, vehicle_index, NONE, NONE, NONE, NULL);
  }
  if (vehicle_definition_crash_sound_index(definition) != NONE) {
    if (scale < 0.0f)
      scale = 0.0f;
    else if (scale > 1.0f)
      scale = 1.0f;
    object_impulse_sound_new(vehicle_index,
                             vehicle_definition_crash_sound_index(definition), NONE,
                             global_origin3d_ptr,
                             (real_vector3d *)global_forward_vector_ptr,
                             scale);
  }
}

/* update_suspension (0x1b74d0)
 *
 * Needs the definition's animation graph (+0x44) and its first 0x74-byte
 * element (antr +0x24). For each 0x14-byte suspension entry (element +0x68:
 * int16 mass point, int16 animation, float +0x4 extension, float +0x8
 * compression) with a valid mass point and animation, casts a 0xc0a0 ray
 * along the mass point's up vector and replaces suspension[i] with the
 * average of the new state clamp((1 - t) * 2, 0, 1) and the previous one
 * (0xff = 1.0). Returns 1 only after playing the definition's suspension
 * sound (+0x3bc) for a largest single-tick rise above 0.3.
 */
bool update_suspension(int vehicle_index)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  animation_graph_t *animation_graph;
  vehicle_animation_t *vehicle_animation;
  struct physics_definition *physics;
  tag_block *suspensions;
  vehicle_suspension_t *suspension;
  struct mass_point_definition *mass_point;
  struct collision_result collision;
  real_matrix4x3 matrix;
  real_point3d point;
  real_point3d start;
  real_vector3d vector;
  real_vector3d mass_point_up;
  float previous;
  float offset;
  float length;
  float state;
  float change;
  float largest;
  float scale;
  int16_t i;
  int sound;

  vehicle = object_get_and_verify_type(vehicle_index, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  if (object_definition_animation_graph_index(definition) == NONE)
    return 0;
  animation_graph =
    tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(definition));
  if (animation_graph_definition_field_24(animation_graph).count == 0)
    return 0;
  vehicle_animation = tag_block_get_element(
    (char *)animation_graph + 0x24, 0, sizeof(vehicle_animation_t));
  if (vehicle_animation == NULL)
    return 0;
  physics = tag_get(TAG_GROUP_PHYS, definition->physics.tag_index);
  largest = 0.0f;
  matrix4x3_from_forward_up_position(&matrix, &vehicle->unit.object.position,
                                     &vehicle->unit.object.forward,
                                     &vehicle->unit.object.up);
  suspensions = &vehicle_animation->suspensions;
  for (i = 0; i < suspensions->count; i++) {
    suspension =
      tag_block_get_element(suspensions, i, sizeof(vehicle_suspension_t));
    if (suspension->mass_point_index < 0 ||
        suspension->mass_point_index >= physics->mass_points.count ||
        suspension->animation_index == NONE)
      continue;
    tag_block_get_element(&animation_graph->animations,
                          suspension->animation_index, sizeof(animation_t));
    mass_point = tag_block_get_element(&physics->mass_points,
                                       suspension->mass_point_index,
                                       sizeof(struct mass_point_definition));
    if (vehicle->suspension[i] == 0xff)
      previous = 1.0f;
    else
      previous = (float)vehicle->suspension[i] * 0.003921569f;
    matrix_transform_point(&matrix, &mass_point->position,
                           &point);
    matrix_transform_vector((float *)&matrix, &mass_point->up, &mass_point_up);
    length = suspension->field_04 - suspension->field_08;
    offset =
      suspension->field_08 - physics->center_of_mass.z - length;
    start.x = mass_point_up.i * offset + point.x;
    start.y = mass_point_up.j * offset + point.y;
    start.z = mass_point_up.k * offset + point.z;
    length = length + length;
    vector.i = mass_point_up.i * length;
    vector.j = mass_point_up.j * length;
    vector.k = mass_point_up.k * length;
    collision_test_vector(0xc0a0, &start, &vector, vehicle_index,
                          (collision_test_result *)&collision);
    state = (1.0f - collision.t) * 2.0f;
    if (state < 0.0f)
      state = 0.0f;
    else if (state > 1.0f)
      state = 1.0f;
    change = state - previous;
    if (change > largest)
      largest = change;
    vehicle->suspension[i] =
      quantize_real_to_byte_lower_bound(0.0f, 1.0f, (state + previous) * 0.5f);
  }
  sound = vehicle_definition_suspension_sound_index(definition);
  if (sound == NONE)
    return 0;
  if (!(largest > 0.3f))
    return 0;
  scale = (largest - 0.3f) * 1.6666667f;
  if (scale < 0.0f)
    scale = 0.0f;
  else if (scale > 1.0f)
    scale = 1.0f;
  object_impulse_sound_new(vehicle_index, sound, NONE,
                           global_origin3d_ptr,
                           (real_vector3d *)global_forward_vector_ptr,
                           scale);
  return 1;
}

/* create_slipping_effects (0x1b77f0)
 *
 * Register arg: vehicle_index@<eax>; powered_mass_points is never read.
 * For each touching mass point (flag 0x2) sliding faster than 0.03, spawns
 * the definition's slipping material effect (+0x3dc), type 10 when the phys
 * mass point flags (+0x24) bit 0 is set else 9, lifted off the ground plane
 * by ground_depth - radius + 0.003, with scale
 * clamp((|slip| - 0.03) * 4.5454545, 0, 1).
 */
void create_slipping_effects(int vehicle_index, void *powered_mass_points,
                             void *mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;
  struct mass_point_definition *mass_point;
  struct mass_point_datum *state;
  real_point3d position;
  real_vector3d velocity;
  float speed;
  float scale;
  float lift;
  float inverse;
  int16_t i;

  (void)powered_mass_points;
  vehicle = object_get_and_verify_type(vehicle_index, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = tag_get(TAG_GROUP_PHYS, definition->physics.tag_index);
  if (vehicle_definition_material_effects_index(definition) == NONE)
    return;
  for (i = 0; i < physics->mass_points.count; i++) {
    state = (struct mass_point_datum *)mass_points + i;
    mass_point = tag_block_get_element(&physics->mass_points, i,
                                       sizeof(struct mass_point_definition));
    if (!(state->flags & 0x2))
      continue;
    speed = x87_sqrt(state->velocity_relative_to_ground.i *
                       state->velocity_relative_to_ground.i +
                     state->velocity_relative_to_ground.j *
                       state->velocity_relative_to_ground.j +
                     state->velocity_relative_to_ground.k *
                       state->velocity_relative_to_ground.k);
    if (!(speed > 0.03f))
      continue;
    scale = (speed - 0.03f) * 4.5454545f;
    lift = state->ground_depth - mass_point->radius + 0.003f;
    position.x = lift * state->ground_plane.normal.i + state->position.x;
    position.y = lift * state->ground_plane.normal.j + state->position.y;
    position.z = lift * state->ground_plane.normal.k + state->position.z;
    inverse = 0.8660254f / speed;
    velocity.i = state->ground_plane.normal.i * 0.5f +
                 inverse * state->velocity_relative_to_ground.i;
    velocity.j = state->ground_plane.normal.j * 0.5f +
                 inverse * state->velocity_relative_to_ground.j;
    velocity.k = state->ground_plane.normal.k * 0.5f +
                 inverse * state->velocity_relative_to_ground.k;
    if (scale < 0.0f)
      scale = 0.0f;
    else if (scale > 1.0f)
      scale = 1.0f;
    material_effect_new(vehicle_definition_material_effects_index(definition),
                        (mass_point_definition_field_24(mass_point) & 0x1) ? 10
                                                                        : 9,
                        state->ground_material_type, &position, &velocity,
                        &vehicle->unit.object.location, scale);
  }
}

/* vehicle_export_function_values (0x1b79c0)
 *
 * Fills the four exported function values (object +0xd4) from the four
 * int16 export sources at definition +0x31c. Source 0 leaves the slot
 * alone; every other source starts at 0.0f, is computed by the switch
 * (jump table at 0x1b7e48) and clamped to [0, 1]. Definition +0x330/+0x334
 * are the slide limits; *0x2549d4, *0x254e6c, *0x2533e8 and *0x2b7d40 are
 * pooled constants read from memory. Case 7 spills its second operand as a
 * double.
 */
void vehicle_export_function_values(int vehicle_handle)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  float speed_max;
  float speed_forward;
  float speed_reverse;
  float slide_left;
  float slide_right;
  float slide_max;
  float turn_left;
  float turn_right;
  float turn_max;
  float *value_out;
  int16_t *source;
  int count;
  float value;
  float interpolation;
  float velocity_value;
  float speed_value;
  real_vector3d parallel;
  real_vector3d perpendicular;

  vehicle = object_get_and_verify_type(vehicle_handle, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);

  speed_forward = (float)fabs(definition->field_2f8[0]);
  speed_reverse = (float)fabs(definition->field_2f8[1]);
  speed_max = speed_forward > speed_reverse ? speed_forward : speed_reverse;
  slide_left = (float)fabs(vehicle_definition_field_330(definition));
  slide_right = (float)fabs(vehicle_definition_field_334(definition));
  slide_max = slide_left > slide_right ? slide_left : slide_right;
  turn_left = (float)fabs(definition->field_308);
  turn_right = (float)fabs(definition->field_30c);
  turn_max = turn_left > turn_right ? turn_left : turn_right;

  value_out = &vehicle->unit.object.field_d0[1];
  source = &vehicle_definition_field_31c(definition);
  count = 4;
  do {
    if (*source != 0) {
      value = 0.0f;
      switch (*source) {
      case 1:
      case 28:
      case 29:
      case 30:
      case 31:
        value = (float)(fabs(vehicle->speed) / speed_max);
        break;
      case 2:
        value = (vehicle->speed < 0.0f ? 0.0f
                                                   : vehicle->speed) /
                speed_forward;
        break;
      case 3:
        value = (float)(fabs(vehicle->speed > 0.0f
                               ? 0.0f
                               : vehicle->speed) /
                        speed_reverse);
        break;
      case 4:
        value = (float)(fabs(vehicle->slide) / slide_max);
        break;
      case 5:
        value = (float)(fabs(vehicle->slide) / slide_left);
        break;
      case 6:
        value = (float)(fabs(vehicle->slide) / slide_right);
        break;
      case 7:
        value = (float)(fabs(vehicle->speed) / speed_max >
                            fabs(vehicle->slide) / slide_max
                          ? fabs(vehicle->speed) / speed_max
                          : fabs(vehicle->slide) / slide_max);
        break;
      case 8:
        value = (float)(fabs(vehicle->turn) / turn_max);
        break;
      case 9:
        value = (float)(fabs(vehicle->turn) / turn_left);
        break;
      case 10:
        value = (float)(fabs(vehicle->turn) / turn_right);
        break;
      case 11:
        value = (vehicle->flags & 4) ? 1.0f : 0.0f;
        break;
      case 12:
        value = (vehicle->flags & 8) ? 1.0f : 0.0f;
        break;
      case 14:
        value = magnitude3d(&vehicle->unit.object.translational_velocity) / speed_max;
        break;
      case 15:
        if (vehicle->unit.object.flags & 0x1c) {
          value = magnitude3d(&vehicle->unit.object.translational_velocity) / speed_max;
        }
        break;
      case 16:
        if (vehicle->unit.object.flags & 2) {
          value = magnitude3d(&vehicle->unit.object.translational_velocity) / speed_max;
        }
        break;
      case 17:
        value = (float)(fabs(vehicle_dot_product3d(
                               &vehicle->unit.object.translational_velocity,
                               &vehicle->unit.object.forward)) /
                        speed_max);
        break;
      case 18:
      case 19:
        value = (float)(fabs(vehicle_dot_product3d(
                               &vehicle->unit.object.translational_velocity,
                               &vehicle->unit.object.up)) /
                        speed_max);
        break;
      case 20:
        value = vehicle->left_tread / definition->wheel_circumference;
        break;
      case 21:
        value = vehicle->right_tread / definition->wheel_circumference;
        break;
      case 22:
        value = (float)(fabs(vehicle->speed -
                             vehicle->turn) /
                        speed_max);
        break;
      case 23:
        value = (float)(fabs(vehicle->turn +
                             vehicle->speed) /
                        speed_max);
        break;
      case 24:
      case 25:
      case 26:
      case 27:
        value = vehicle->wheel / definition->wheel_circumference;
        break;
      case 32:
        component_vectors_from_normal3d(&vehicle->unit.object.translational_velocity, &vehicle->unit.object.forward,
                     &parallel, &perpendicular);
        value = magnitude3d(&perpendicular) * REAL_TEN_THIRDS_POOL;
        value = value * value;
        break;
      case 34:
        value = vehicle->thrust;
        break;
      case 33:
        value = vehicle->hover;
        break;
      case 35:
        velocity_value = (float)(fabs(vehicle_dot_product3d(
                                        &vehicle->unit.object.translational_velocity,
                                        &vehicle->unit.object.forward)) /
                                 speed_max);
        speed_value =
          (float)(fabs(vehicle->speed) / speed_forward);
        interpolation =
          ((float)vehicle->airborne_ticks * REAL_0_2_POOL + 1.0f) *
          0.5f;
        if (interpolation < 0.0f) {
          interpolation = 0.0f;
        } else if (interpolation > 1.0f) {
          interpolation = 1.0f;
        }
        value = interpolation * speed_value +
                (1.0f - interpolation) * velocity_value;
        break;
      case 36:
        value = (magnitude3d(&vehicle->unit.object.translational_velocity) /
                   definition->field_2f8[0] *
                   vehicle->thrust -
                 REAL_0_05_POOL) *
                REAL_20_OVER_17_POOL;
        break;
      }
      if (value < 0.0f) {
        value = 0.0f;
      } else if (value > 1.0f) {
        value = 1.0f;
      }
      *value_out = value;
    }
    value_out = value_out + 1;
    source = source + 1;
    count = count - 1;
  } while (count != 0);
}

/*
 * vehicle_moving_near_any_player (0x1b7ee0)
 *
 * True when some vehicle within 10 world units of an on-foot local player's
 * unit (and not carrying that unit) moves faster than 1/30 per tick. Called
 * from game_safe_to_save (0xa7530). The player handle is fetched twice, as
 * in the original.
 */


#include "../../common.h"

/* Squared proximity threshold: 10.0^2. */
#define VEHICLE_NEAR_PLAYER_DIST_SQ 100.0f
/* Squared speed threshold (1/30)^2, the value at 0x25620c. */
#define VEHICLE_MIN_SPEED_SQ 0.001111111138f

bool vehicle_moving_near_any_player(void)
{
  object_iter_t iter;
  int unit_handles[4];
  real_point3d unit_centers[4];
  float radius;
  int16_t local_player_index;
  int16_t unit_count;
  int16_t i;
  int player_index;
  player_data_t *player;
  int unit_handle;
  unit_data_t *unit;
  vehicle_data_t *vehicle;
  int vehicle_handle;
  float dx, dy, dz;
  float vx, vy, vz;
  char found; /* 1 until a vehicle qualifies */

  unit_count = 0;
  found = 1;

  local_player_index = local_player_get_next(NONE);
  if (local_player_index == NONE)
    goto done;

  do {
    player_index = local_player_get_player_index(local_player_index);
    if (player_index != NONE) {
      player_index = local_player_get_player_index(local_player_index);
      player = datum_get(player_data, player_index);
      unit_handle = player->unit_handle;
      if (unit_handle != NONE) {
        unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
        if (unit->object.parent_object_index.value == NONE) {
          unit_handles[unit_count] = unit_handle;
          object_get_bounding_sphere(unit_handle, &unit_centers[unit_count].x,
                                     &radius);
          unit_count++;
        }
      }
    }
    local_player_index = local_player_get_next(local_player_index);
  } while (local_player_index != NONE);

  if (unit_count == 0)
    goto done;

  object_iterator_new(&iter, OBJECT_MASK_VEHICLE, 0);

  while ((vehicle = object_iterator_next(&iter)) != NULL) {
    vehicle_handle = iter.last_handle;

    i = 0;
    if (unit_count <= 0)
      continue;

    do {
      unit = object_get_and_verify_type(unit_handles[i], _object_mask_unit);
      if (unit->object.parent_object_index.value == vehicle_handle) {
        i++;
        continue;
      }

      dx = vehicle->unit.object.bounding_sphere_center.x - unit_centers[i].x;
      dy = vehicle->unit.object.bounding_sphere_center.y - unit_centers[i].y;
      dz = vehicle->unit.object.bounding_sphere_center.z - unit_centers[i].z;
      if (dx * dx + dy * dy + dz * dz >= VEHICLE_NEAR_PLAYER_DIST_SQ) {
        i++;
        continue;
      }

      vx = vehicle->unit.object.translational_velocity.i;
      vy = vehicle->unit.object.translational_velocity.j;
      vz = vehicle->unit.object.translational_velocity.k;
      if (vx * vx + vy * vy + vz * vz >= VEHICLE_MIN_SPEED_SQ) {
        found = 0;
        goto done;
      }
      i++;
    } while (i < unit_count);
  }

done:
  return found == 0;
}

/*
 * vehicle_stuck (0x1b8060)
 *
 * For a vehicle with stuck mass points (+0x478 bitmask), averages the
 * flagged mass points' positions from the 'phys' tag, transforms the
 * average to world space and writes the normalized direction from the
 * vehicle's origin to it. Returns TRUE only when that direction is nonzero.
 */
char vehicle_stuck(int unit_handle, float *vec)
{
  vehicle_data_t *vehicle = object_get_and_verify_type(
    unit_handle, OBJECT_MASK_VEHICLE);
  char stuck = 0;

  if (vehicle->stuck_mass_point_flags) {
    struct physics_instance instance;

    if (physics_instance_new(&instance, unit_handle)) {
      real_point3d center = *global_origin3d_ptr;
      short mass_point_count = 0;
      short mass_point_index;

      for (mass_point_index = 0;
           mass_point_index < instance.physics->mass_points.count;
           mass_point_index++) {
        if (vehicle->stuck_mass_point_flags & (1 << mass_point_index)) {
          struct mass_point_definition *mass_point =
            (struct mass_point_definition *)tag_block_get_element(
              (void *)&instance.physics->mass_points, mass_point_index,
              sizeof(struct mass_point_definition));

          center.x = center.x + mass_point->position.x;
          center.y = center.y + mass_point->position.y;
          center.z = center.z + mass_point->position.z;
          mass_point_count++;
        }
      }

      if (mass_point_count > 0) {
        real scale = 1.0f / mass_point_count;
        real_point3d center_in_world;
        real_point3d origin;

        center.x *= scale;
        center.y *= scale;
        center.z *= scale;

        matrix_transform_point(&instance.world_matrix, &center,
                               &center_in_world);
        object_get_world_position(unit_handle, (vector3_t *)&origin);
        vec[0] = center_in_world.x - origin.x;
        vec[1] = center_in_world.y - origin.y;
        vec[2] = center_in_world.z - origin.z;

        if (normalize3d((real_vector3d *)vec) != 0.0f) {
          stuck = 1;
        }
      }
    }
  }

  return stuck;
}

/*
 * update_human_plane_physics (0x1b81d0)
 *
 * Pelican physics. With vehicle flag bit 1 set the mass points are cleared
 * and no force is applied. Otherwise hover eases toward a flag-selected
 * factor (0.25 / 1.0 / 0.75) scaled by the unused throttle, thrust follows
 * the squared throttle, a magic force drives toward vehicle.speed and lifts
 * against gravity, and a magic torque turns the hull toward its desired
 * facing (yawed by sideways velocity). Both paths finish with
 * create_pelican_effect.
 */
void update_human_plane_physics(int vehicle_index, void *powered_mass_points,
                                void *mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;

  vehicle = object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = tag_get(
    TAG_GROUP_PHYS, definition->physics.tag_index);

  if (vehicle->flags & 0x2) {
    csmemset(mass_points, 0,
             physics->mass_points.count * sizeof(struct mass_point_datum));
  } else {
    object_data_t *object = &vehicle->unit.object;
    real_vector3d desired_forward;
    real_vector3d desired_up;
    real_vector3d force;
    real_vector3d torque;
    real throttle;
    real thrust;
    real factor;

    if (vehicle->speed < 0.0f) {
      throttle = 0.0f;
    } else if (vehicle->speed > definition->field_2f8[0]) {
      throttle = definition->field_2f8[0];
    } else {
      throttle = vehicle->speed;
    }
    throttle = throttle / definition->field_2f8[0];
    throttle = throttle * throttle;

    factor = !(vehicle->flags & 0x4) ? ((vehicle->flags & 0x8) ? 1.0f : 0.75f)
                                     : 0.25f;
    factor *= 1.0f - throttle;
    vehicle_interpolate_scalar(&vehicle->hover,
                               factor * vehicle->unit.seat_power[0], 0.05f);

    vehicle->thrust = thrust = throttle * vehicle->unit.seat_power[0];

    desired_forward = *(real_vector3d *)&vehicle->unit.desired_facing_vector;

    desired_up.i = -(desired_forward.k * desired_forward.i);
    desired_up.j = -(desired_forward.k * desired_forward.j);
    desired_up.k = 1.0f - desired_forward.k * desired_forward.k;

    if (normalize3d(&desired_up) == 0.0f) {
      desired_up.i = 1.0f;
      desired_up.j = 0.0f;
      desired_up.k = 0.0f;
    }

    {
      real dot;
      real drive;
      real lift;

      dot = vehicle_dot_product3d(
        &object->translational_velocity,
        &object->forward);

      drive = (vehicle->speed - dot) * thrust * physics->mass * 0.05f;
      lift = (x87_fabs(dot / definition->field_2f8[0]) * 1.05f +
              vehicle->hover * 1.3f) *
             global_gravity;
      lift *= physics->mass;

      force.i = lift * object->up.i + drive * object->forward.i;
      force.j = lift * object->up.j + drive * object->forward.j;
      force.k = lift * object->up.k + drive * object->forward.k;
    }

    {
      real_vector2d velocity;
      real yaw;

      {
        real *destination = (real *)&velocity;
        const real *source = (const real *)&object->translational_velocity;
        short component_index;

        for (component_index = 0; component_index < 2; component_index++) {
          destination[component_index] = source[component_index];
        }
      }

      yaw = (velocity.j * desired_forward.i - velocity.i * desired_forward.j) *
            (_pi / 2) / (real)fabs(definition->field_2f8[0]);

      yaw_vectors(&desired_up, &desired_forward, x87_fsin(yaw),
                  x87_fcos(yaw));
    }

    {
      real_matrix4x3 vehicle_rotation;
      real_matrix4x3 desired_rotation;
      real_matrix4x3 rotation;
      real_quaternion quaternion;
      real_vector3d axis;
      real_vector3d scaled;
      real angle;
      real scale;

      matrix_from_forward_and_up(&vehicle_rotation,
                                 &object->forward, &object->up);
      matrix_from_forward_and_up(&desired_rotation,
                                 &desired_forward, &desired_up);
      matrix_inverse((float *)&desired_rotation, (float *)&desired_rotation);
      matrix4x3_multiply((float *)&vehicle_rotation,
                         (float *)&desired_rotation, (float *)&rotation);
      matrix4x3_rotation_to_quaternion((float *)&rotation, (float *)&quaternion);
      quaternion_to_angle_and_vector(&quaternion, &angle, &axis);

      scale_vector3d_inline(&axis, angle * (1.0f / 30), &scaled);

      scale = physics->radius * physics->radius * physics->mass * 0.05f;

      torque.i = (scaled.i - object->angular_velocity.i) * scale;
      torque.j = (scaled.j - object->angular_velocity.j) * scale;
      torque.k = (scaled.k - object->angular_velocity.k) * scale;
    }

    scale_vector3d_inline(&force, vehicle->unit.seat_power[0], &force);
    scale_vector3d_inline(&torque, vehicle->unit.seat_power[0], &torque);

    physics_update(vehicle_index, NULL, mass_points, &force, &torque);
  }

  create_pelican_effect(vehicle_index);
}

/*
 * update_alien_scout_physics (0x1b8570)
 *
 * Ghost physics. Every powered mass point gets seat power 0 as its antigrav
 * fraction and an identity rotation. Out of deep water and not upside down,
 * the hover fraction drives three magic-force/torque terms (throttle
 * acceleration, steering yaw torque, self-levelling torque) plus a boost
 * term, all scaled by antigrav; then physics_update runs, the hover fraction
 * seeks the grounded-powered-mass-point ratio by at most 0.1 per tick, and
 * create_ghost_effect spawns the thruster wash.
 */
void update_alien_scout_physics(int vehicle_index, float steering,
                                void *powered_mass_points, void *mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;
  struct powered_mass_point_datum *state;
  real_matrix4x3 matrix;
  vector3_t local_velocity;
  real_vector3d magic_force;
  real_vector3d magic_torque;
  real antigrav;
  real water_depth;
  int16_t mass_point_index;

  vehicle = object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = tag_get(
    TAG_GROUP_PHYS, definition->physics.tag_index);
  state = (struct powered_mass_point_datum *)powered_mass_points;

  water_depth = scenario_location_water_depth(&vehicle->unit.object.location,
                             &vehicle->unit.object.position);

  magic_force = *(real_vector3d *)global_zero_vector_ptr;
  magic_torque = *(real_vector3d *)global_zero_vector_ptr;
  antigrav = vehicle->unit.seat_power[0];

  for (mass_point_index = 0;
       mass_point_index < physics->powered_mass_points.count;
       mass_point_index++) {
    state[mass_point_index].antigrav_fraction = antigrav;
    state[mass_point_index].rotation[0] = 0.0f;
    state[mass_point_index].rotation[1] = 0.0f;
    state[mass_point_index].rotation[2] = 0.0f;
    state[mass_point_index].rotation[3] = 1.0f;
  }

  if (water_depth < 0.5f && vehicle->unit.object.up.k > -0.2f) {
    const real_vector3d *object_forward =
      &vehicle->unit.object.forward;
    const real_vector3d *object_up =
      &vehicle->unit.object.up;
    const real_vector3d *object_angular_velocity =
      &vehicle->unit.object.angular_velocity;

    matrix4x3_from_forward_up_position(
      &matrix, &vehicle->unit.object.position, object_forward,
      object_up);
    real_matrix3x3_transform_vector(
      &matrix, (vector3_t *)&vehicle->unit.object.translational_velocity, &local_velocity);

    if (vehicle->hover > 0.0f) {
      real maximum_speed = definition->field_2f8[0];
      real maximum_acceleration;
      real_vector2d target_velocity;
      real_vector3d acceleration;
      real scale;

      if (vehicle->flags & FLAG(3)) {
        maximum_speed *= 0.8f;
      }

      maximum_acceleration = definition->field_2f8[2];

      target_velocity.i = maximum_speed * vehicle->unit.throttle.x;
      target_velocity.j = maximum_speed * vehicle->unit.throttle.y;
      acceleration.i = target_velocity.i - local_velocity.x;
      acceleration.j = target_velocity.j - local_velocity.y;
      acceleration.k = 0.0f;

      if (vehicle->on_ground_ticks > 0 && fabs(steering) > 0.785398185f) {
        real reduction = vehicle->on_ground_ticks * 0.05f;

        if (reduction > 0.98f) {
          reduction = 0.98f;
        }
        maximum_acceleration *= 1.0f - reduction;
      }

      limit3d(&acceleration, maximum_acceleration);
      matrix_scale_transform_vector(&matrix, &acceleration,
                                    &acceleration);

      scale = physics->mass * vehicle->hover;
      magic_force.i += acceleration.i * scale;
      magic_force.j += acceleration.j * scale;
      magic_force.k += acceleration.k * scale;
    }

    if (vehicle->hover > 0.0f) {
      real current = vehicle_dot_product3d(object_up, object_angular_velocity);
      int32_t sign = steering != 0.0f ? (steering < 0.0f ? -1 : 1) : 0;
      real desired = (real)x87_sqrtd(fabs(steering) * 0.0069813174f) * sign;
      real error;
      real torque;

      if (fabs(desired) > 0.0001f && steering / desired < 2.0f) {
        desired = steering * 0.5f;
      }

      error = desired - current;
      if (error < -0.0034906587f) {
        error = -0.0034906587f;
      } else if (error > 0.0034906587f) {
        error = 0.0034906587f;
      }
      torque = error * physics->zz_moment;
      torque *= vehicle->hover;

      magic_torque.i += object_up->i * torque;
      magic_torque.j += object_up->j * torque;
      magic_torque.k += object_up->k * torque;
    }

    if (vehicle->hover < 1.0f) {
      real_vector3d left;
      real_vector2d forward2d;
      real_vector2d left2d;
      real_vector2d control_torque = *(real_vector2d *)global_zero_vector2d_ptr;
      real torque_a;
      real torque_b;

      left.i = object_forward->k * object_up->j -
               object_up->k * object_forward->j;
      left.j = object_up->k * object_forward->i -
               object_up->i * object_forward->k;
      left.k = object_forward->j * object_up->i -
               object_up->j * object_forward->i;
      forward2d.i = object_forward->i;
      forward2d.j = object_forward->j;
      left2d.i = left.i;
      left2d.j = left.j;
      normalize2d(&forward2d);
      normalize2d(&left2d);

      if (object_up->k > 0.0f) {
        real_vector2d level_torque;
        real_vector2d alignment;
        real_vector2d rate;
        int32_t sign_a;
        int32_t sign_b;
        real weight_a;
        real weight_b;

        /* The 2d up/angular-velocity views read the object fields in place. */
        alignment.i =
          vehicle_dot_product2d((const real_vector2d *)object_up, &forward2d);
        alignment.j =
          vehicle_dot_product2d((const real_vector2d *)object_up, &left2d);
        rate.i = vehicle_dot_product2d(
          (const real_vector2d *)object_angular_velocity, &left2d);
        rate.j = -vehicle_dot_product2d(
          (const real_vector2d *)object_angular_velocity, &forward2d);
        level_torque = *(real_vector2d *)global_zero_vector2d_ptr;
        level_torque.i -= alignment.i;
        level_torque.j -= alignment.j;
        level_torque.i -= 15.0f * rate.i;
        level_torque.j -= 15.0f * rate.j;
        sign_a = vehicle->unit.throttle.x * level_torque.i != 0.0f
                   ? (vehicle->unit.throttle.x * level_torque.i < 0.0f ? -1 : 1)
                   : 0;
        weight_a = (real)fabs(level_torque.i) * sign_a;
        sign_b = vehicle->unit.throttle.y * level_torque.j != 0.0f
                   ? (vehicle->unit.throttle.y * level_torque.j < 0.0f ? -1 : 1)
                   : 0;
        weight_b = (real)fabs(level_torque.j) * sign_b;
        weight_a += 1.0f;
        if (weight_a < 0.3f) {
          weight_a = 0.3f;
        } else if (weight_a > 2.5f) {
          weight_a = 2.5f;
        }
        control_torque.i +=
          vehicle->unit.throttle.x * weight_a * 0.0015514038f;
        weight_b += 1.0f;
        if (weight_b < 0.3f) {
          weight_b = 0.3f;
        } else if (weight_b > 2.5f) {
          weight_b = 2.5f;
        }
        control_torque.j +=
          vehicle->unit.throttle.y * weight_b * 0.0015514038f;
        {
          real level_scale = (1.0f - object_up->k) * 0.0038785094f;

          torque_a = level_scale * level_torque.i;
          torque_b = level_scale * level_torque.j;
          torque_a += control_torque.i;
        }
      } else {
        torque_a = vehicle->unit.throttle.x * 0.0015514038f;
        torque_b = vehicle->unit.throttle.y * 0.0015514038f;
        torque_a += control_torque.i;
      }

      torque_b += control_torque.j;

      {
        real_vector3d torque = *(real_vector3d *)global_zero_vector_ptr;
        real left_scale = physics->yy_moment * torque_a;
        real forward_scale = -(physics->xx_moment * torque_b);
        real scale;

        torque.i += left.i * left_scale;
        torque.j += left.j * left_scale;
        torque.k += left.k * left_scale;
        torque.i += object_forward->i * forward_scale;
        torque.j += object_forward->j * forward_scale;
        torque.k += object_forward->k * forward_scale;

        scale = 1.0f - vehicle->hover;
        magic_torque.i += torque.i * scale;
        magic_torque.j += torque.j * scale;
        magic_torque.k += torque.k * scale;
      }
    }

    if (vehicle->flags & FLAG(3)) {
      real speed =
        vehicle_dot_product3d(object_forward,
                              &vehicle->unit.object.translational_velocity) /
        definition->field_2f8[0];
      real_vector3d left;

      if (speed < 0.0f) {
        speed = 0.0f;
      } else if (speed > 1.0f) {
        speed = 1.0f;
      }

      vehicle_cross_product3d(object_up, object_forward, &left);

      if (speed > 0.0f) {
        real torque =
          physics->yy_moment * vehicle->hover * speed * -0.005817764f;
        real lift;

        magic_torque.i += left.i * torque;
        magic_torque.j += left.j * torque;
        magic_torque.k += left.k * torque;

        lift = physics->mass * vehicle->hover * speed * 0.004f;
        magic_force.i += global_up_vector3d->i * lift;
        magic_force.j += global_up_vector3d->j * lift;
        magic_force.k += global_up_vector3d->k * lift;
      }

      if (vehicle->airborne_ticks > 0) {
        real_vector3d axis;

        vehicle_cross_product3d(&left, global_up_vector3d, &axis);
        if (normalize3d(&axis) > 0.0f) {
          real fade = 1.0f - vehicle->airborne_ticks * (1.0f / 30);
          real axis_scale;
          real up_scale;

          if (fade < 0.0f) {
            fade = 0.0f;
          } else if (fade > 1.0f) {
            fade = 1.0f;
          }
          axis_scale = (1.0f - vehicle->hover) * physics->mass * fade * 0.002f;

          magic_force.i += axis.i * axis_scale;
          magic_force.j += axis.j * axis_scale;
          magic_force.k += axis.k * axis_scale;

          up_scale = (1.0f - vehicle->hover) * physics->mass * fade * 0.001f;
          magic_force.i += global_up_vector3d->i * up_scale;
          magic_force.j += global_up_vector3d->j * up_scale;
          magic_force.k += global_up_vector3d->k * up_scale;
        }
      }
    }

    magic_force.i *= antigrav;
    magic_force.j *= antigrav;
    magic_force.k *= antigrav;
    magic_torque.i *= antigrav;
    magic_torque.j *= antigrav;
    magic_torque.k *= antigrav;
  }

  physics_update(vehicle_index, state, mass_points, &magic_force,
                 &magic_torque);

  {
    real maximum = 0.4f > vehicle->unit.object.up.k
                     ? 0.4f
                     : vehicle->unit.object.up.k;
    int32_t powered_count = 0;
    int32_t grounded_count = 0;
    real ratio = 0.0f;
    real target;

    for (mass_point_index = 0;
         mass_point_index < physics->mass_points.count;
         mass_point_index++) {
      struct mass_point_definition *mass_point_definition =
        (struct mass_point_definition *)tag_block_get_element(
          &physics->mass_points, mass_point_index,
          sizeof(struct mass_point_definition));

      if (mass_point_definition->powered_mass_point_index != NONE) {
        powered_count++;
        if (((struct mass_point_datum *)mass_points)[mass_point_index].flags &
            FLAG(4)) {
          grounded_count++;
        }
      }
    }

    if ((int16_t)powered_count > 0) {
      ratio = (real)(int16_t)grounded_count / (real)(int16_t)powered_count;
    }

    target = ratio * maximum;
    if (target < 0.0f) {
      target = 0.0f;
    } else if (target > 1.0f) {
      target = 1.0f;
    }
    if (target - vehicle->hover > 0.1f) {
      target = vehicle->hover + 0.1f;
    } else if (target - vehicle->hover < -0.1f) {
      target = vehicle->hover - 0.1f;
    }

    vehicle->hover = target;
  }

  create_ghost_effect(vehicle_index);
}

/*
 * update_alien_fighter_physics (0x1b8f10) — run the old Banshee physics when
 * the 'phys' radius is positive, else the new one, then create_ghost_effect.
 * Register args: vehicle_handle@<esi>, powered_mass_points@<eax>,
 * mass_points@<ebx>; both are only forwarded.
 */
void update_alien_fighter_physics(int vehicle_handle, int powered_mass_points,
                                   int mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;

  vehicle = object_get_and_verify_type(vehicle_handle, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = tag_get(TAG_GROUP_PHYS, definition->physics.tag_index);

  if (physics->radius > REAL_ZERO_POOL) {
    update_alien_fighter_physics_old(vehicle_handle, (void *)mass_points,
                                     (void *)powered_mass_points);
    create_ghost_effect(vehicle_handle);
    return;
  }

  update_alien_fighter_physics_new(vehicle_handle, (void *)powered_mass_points,
                                   (void *)mass_points);
  create_ghost_effect(vehicle_handle);
}

/* vehicle_update (0x1b8f80) — vehicle object_type_definition (0x323de8)
 * datum update slot +0x30.
 *
 * Returns 1. Frame 0x327c via _chkstk. Physics dispatches on the
 * definition's vehicle_type through the jump table at 0x1b9870 (6 = turret,
 * inline physics_update). Register args: tank/jeep powered_mass_points@<edi>,
 * boat powered_mass_points@<esi>, create_slipping_effects index@<eax>,
 * compute_airborne_ticks index@<eax> + mass_points@<edi> (its extra pushed
 * powered_mass_points is never read), update_alien_fighter_physics
 * index@<esi>/powered@<eax>/mass@<ebx>. Both steering dot products are
 * accumulated (k + j) + i. Definition +0x330 is the slide speed block,
 * +0x318 the blur speed, +0x340/+0x344 the roll clamp; profile section
 * 0x32e4a0 (enable byte 0x32e4a8). Scenario +0x10/+0x14 are the floor and
 * ceiling heights for vehicle types 3 and 5.
 */
#define COLLISION_USER_VEHICLES 0x11

bool vehicle_update(int vehicle_index)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  game_globals_falling_damage_t *falling_damage;
  object_data_t *child;
  real_vector3d *forward;
  real_vector3d *up;
  real_vector3d *desired_facing;
  real_vector3d left;
  float steering_angle;
  float torque;
  float roll;
  float desired_position;
  float target;
  float floor_z;
  float ceiling_z;
  int child_index;
  uint8_t upending_type;
  uint16_t vehicle_flags;
  char blur;
  real_vector3d torque_axis;
  real_vector3d cross;
  real_vector3d previous_velocity;
  char animation_update[2];
  char damage_data[0x54];
  char powered_mass_points[0xc00];
  char mass_points[0x2600];

  vehicle = object_get_and_verify_type(vehicle_index, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);

  if (PROFILE_GLOBAL_ENABLE != '\0' && VEHICLE_UPDATE_SECTION_ENABLED != '\0') {
    profile_enter_private(VEHICLE_UPDATE_SECTION);
  }

  if (vehicle->unit.object.parent_object_index.value != NONE) {
    vehicle->unit.object.angular_velocity.i = 0.0f;
    vehicle->unit.object.angular_velocity.j = 0.0f;
    vehicle->unit.object.angular_velocity.k = 0.0f;
    vehicle->unit.object.translational_velocity.i = 0.0f;
    vehicle->unit.object.translational_velocity.j = 0.0f;
    vehicle->unit.object.translational_velocity.k = 0.0f;
    vehicle->unit.object.flags &= ~0x20u;
    goto animate;
  }

  if ((vehicle->unit.control_flags & 1) != 0) {
    *(uint8_t *)&vehicle->flags |= 4;
  } else {
    *(uint8_t *)&vehicle->flags &= ~4;
  }
  if ((vehicle->unit.control_flags & 2) != 0 ||
      ((*(uint8_t *)&definition->flags & 0x10) != 0 &&
       ((vehicle->unit.throttle.x > 0.0f &&
         vehicle->speed < 0.0f) ||
        (vehicle->unit.throttle.x < 0.0f &&
         vehicle->speed > 0.0f)))) {
    *(uint8_t *)&vehicle->flags |= 8;
  } else {
    *(uint8_t *)&vehicle->flags &= ~8;
  }

  /* left = cross_product3d(up, forward); steering = atan2(left . facing,
   * facing . forward). */
  forward = &vehicle->unit.object.forward;
  up = &vehicle->unit.object.up;
  desired_facing = &vehicle->unit.desired_facing_vector;
  left.i = up->j * forward->k - forward->j * up->k;
  left.j = forward->i * up->k - up->i * forward->k;
  left.k = forward->j * up->i - up->j * forward->i;
  vehicle_flags = vehicle->flags;
  steering_angle = (float)atan2(
      left.k * desired_facing->k + left.j * desired_facing->j +
          left.i * desired_facing->i,
      desired_facing->k * forward->k + desired_facing->j * forward->j +
          desired_facing->i * forward->i);

  upending_type = vehicle->upending_type;
  if ((vehicle_flags & 0x10) != 0 && upending_type != 0 &&
      vehicle->upending_ticks < 30 && vehicle->unit.object.up.k <= 0.9f) {
    torque = (upending_type == 2 || upending_type == 4) ? 0.3 : -0.3;
    if (upending_type == 4 || upending_type == 3) {
      cross_product3d(forward, up, &torque_axis);
    } else {
      torque_axis = *forward;
    }
    roll = vehicle->unit.object.up.k * -2.0f;
    if (roll < vehicle_definition_field_340(definition)) {
      roll = vehicle_definition_field_340(definition);
    } else if (roll > vehicle_definition_field_344(definition)) {
      roll = vehicle_definition_field_344(definition);
    }
    torque = roll * torque;
    vehicle->unit.object.flags &= ~0x20u;
    if (upending_type == 2 || upending_type == 1) {
      cross_product3d(forward, up, &cross);
      /* in place: 0x1b91d7/0x1b91e3 push [ebp-0x1c] as both base and out */
      vector3d_scale_add((real_point3d *)&torque_axis, &cross, -forward->k,
                         (real_point3d *)&torque_axis); /* dup-args-ok */
    }
    scale_vector3d(&torque_axis, torque, &vehicle->unit.object.angular_velocity);
    switch (definition->vehicle_type) {
    case 0:
      scale_vector3d(forward,
                   dot_product3d(forward, &vehicle->unit.object.translational_velocity),
                   &vehicle->unit.object.translational_velocity);
      break;
    case 5:
      vehicle->unit.object.translational_velocity.k = (-0.01f > vehicle->unit.object.translational_velocity.k)
                                       ? vehicle->unit.object.translational_velocity.k
                                       : -0.01f;
      break;
    }
    vehicle->upending_ticks = vehicle->upending_ticks + 1;
  } else {
    vehicle->upending_ticks = 0;
    vehicle->upending_type = 0;
    vehicle->flags = vehicle_flags & 0xffef;
  }

  if ((*(uint8_t *)&vehicle->flags & 8) != 0) {
    physics_variable_speed_update_seek(&vehicle->speed,
                                       definition->field_2f8, 0.0f, 1.0f);
  } else {
    physics_variable_speed_update_seek(&vehicle->speed,
                                       definition->field_2f8,
                                       vehicle->unit.throttle.x, 1.0f);
    physics_variable_speed_update_seek(&vehicle->slide,
                                       &vehicle_definition_field_330(definition),
                                       vehicle->unit.throttle.y, 1.0f);
  }

  if (definition->vehicle_type != 0) {
    if (vehicle->speed < 0.0f) {
      desired_position = -steering_angle;
    } else {
      desired_position = steering_angle;
    }
    if (desired_position < definition->field_30c * (_pi / 180.0f)) {
      desired_position = definition->field_30c * (_pi / 180.0f);
    } else if (desired_position >
               definition->field_308 * (_pi / 180.0f)) {
      desired_position = definition->field_308 * (_pi / 180.0f);
    }
    physics_variable_position_update_seek(&vehicle->turn, &definition->field_308, 0,
                 desired_position,
                 definition->field_314 * (_pi / 180.0f) *
                     0.033333335f);
  } else if (vehicle->speed == 0.0f) {
    physics_variable_speed_update_seek(&vehicle->turn,
                                       definition->field_2f8, 0.0f, 1.0f);
  } else {
    target = steering_angle * 0.63661975f;
    if (target < -1.0f) {
      target = -1.0f;
    } else if (target > 1.0f) {
      target = 1.0f;
    }
    physics_variable_speed_update_seek(
        &vehicle->turn, definition->field_2f8,
        target * definition->field_2f8[0], 2.0f);
  }

  if (definition->physics.tag_index != NONE) {
    if (((definition->flags & 1) != 0 &&
         vehicle->speed != 0.0f) ||
        ((definition->flags & 2) != 0 &&
         vehicle->turn != 0.0f) ||
        ((definition->flags & 4) != 0 &&
         vehicle->unit.seat_power[0] != 0.0f) ||
        ((definition->flags & 8) != 0 &&
         vehicle->unit.seat_power[1] != 0.0f) ||
        ((definition->flags & 0x20) != 0 &&
         vehicle->slide != 0.0f)) {
      vehicle->unit.object.flags &= ~0x20u;
    }
  }

  assert_halt_msg_at("global_current_collision_user_depth < "
                     "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                     "c:\\halo\\SOURCE\\units\\vehicles.c", 0x134,
                     global_current_collision_user_depth < 0x20);
  /* global_current_collision_users[depth++] = 0x11 (vehicles). */
  COLLISION_USER_STACK[(COLLISION_USER_STACK_DEPTH)++] = 0x11;

  if (definition->physics.tag_index != NONE &&
      (*(uint8_t *)&vehicle->unit.object.flags & 0x20) == 0) {
    previous_velocity = vehicle->unit.object.translational_velocity;
    switch (definition->vehicle_type) {
    case 0:
      update_human_tank_physics(vehicle_index, mass_points,
                                powered_mass_points);
      break;
    case 1:
      update_human_jeep_physics(vehicle_index, mass_points,
                                powered_mass_points);
      break;
    case 2:
      update_human_boat_physics(vehicle_index, mass_points,
                                powered_mass_points);
      break;
    case 3:
      update_human_plane_physics(vehicle_index, powered_mass_points,
                                 mass_points);
      break;
    case 4:
      update_alien_scout_physics(vehicle_index, steering_angle,
                                 powered_mass_points, mass_points);
      break;
    case 5:
      update_alien_fighter_physics(vehicle_index, (int)powered_mass_points,
                                   (int)mass_points);
      break;
    case 6:
      physics_update(vehicle_index, NULL, mass_points, NULL, NULL);
      break;
    }
    create_slipping_effects(vehicle_index, powered_mass_points, mass_points);
    if (!update_suspension(vehicle_index)) {
      create_crashing_effects(vehicle_index, &previous_velocity,
                              mass_points);
    }
    compute_airborne_ticks(vehicle_index, mass_points);
    if ((vehicle->unit.object.flags & 0x20) != 0) {
      vehicle->stop_time = 15;
    }
    if ((vehicle->unit.object.flags & 0x1000000) == 0 &&
        ((1 << *(uint8_t *)&definition->vehicle_type) & 0x28) != 0) {
      floor_z = scenario_field_10(scenario_get());
      ceiling_z = scenario_field_14(scenario_get());
      if (floor_z != 0.0f && vehicle->unit.object.position.z < floor_z) {
        vehicle->unit.object.translational_velocity.k =
            ((floor_z - vehicle->unit.object.position.z) * 0.015625f -
             vehicle->unit.object.translational_velocity.k * 0.0625f) *
                vehicle->unit.seat_power[0] +
            vehicle->unit.object.translational_velocity.k;
      }
      if (ceiling_z != 0.0f && vehicle->unit.object.position.z > ceiling_z) {
        vehicle->unit.object.translational_velocity.k =
            vehicle->unit.object.translational_velocity.k -
            ((vehicle->unit.object.position.z - ceiling_z) * 0.015625f +
             vehicle->unit.object.translational_velocity.k * 0.0625f) *
                vehicle->unit.seat_power[0];
      }
    }
  } else if (vehicle->stop_time > 0) {
    slowly_stop_vehicle(vehicle_index);
    update_suspension(vehicle_index);
  }

  assert_halt_msg_at("global_current_collision_user_depth > 1",
                     "c:\\halo\\SOURCE\\units\\vehicles.c", 0x174,
                     COLLISION_USER_STACK_DEPTH > 1);
  --COLLISION_USER_STACK_DEPTH;

  if ((*(uint8_t *)&definition->flags & 0x40) != 0) {
    falling_damage = tag_block_get_element(
      &game_globals_falling_damage(game_globals_get()), 0,
      sizeof(game_globals_falling_damage_t));
    if (-falling_damage->runtime_maximum_falling_velocity >
        vehicle->unit.object.translational_velocity.k) {
      child_index = vehicle->unit.object.first_child_object_index.value;
      while (child_index != NONE) {
        child = object_get_and_verify_type(child_index, _object_mask_all);
        damage_data_new(damage_data, falling_damage->maximum_distance_damage.tag_index);
        object_cause_damage(damage_data, child_index, NONE, NONE, NONE, NULL);
        child_index = child->next_object_index.value;
      }
    }
  }

animate:
  if (object_definition_animation_graph_index(definition) != NONE) {
    animation_update[0] = 0;
    animation_update[1] = 0;
    unit_update_animation(vehicle_index, animation_update);
  }

  blur = 1;
  if (!(vehicle_definition_blur_speed(definition) <=
        (float)fabs(vehicle->speed))) {
    blur = 0;
  }
  if (blur != (*(uint8_t *)&vehicle->flags & 1)) {
    object_permute_region(vehicle_index, "~blur", NONE, blur);
    if (blur) {
      *(uint8_t *)&vehicle->flags |= 1;
    } else {
      *(uint8_t *)&vehicle->flags &= ~1;
    }
  }

  if (PROFILE_GLOBAL_ENABLE != '\0' && VEHICLE_UPDATE_SECTION_ENABLED != '\0') {
    profile_exit_private(VEHICLE_UPDATE_SECTION);
  }

  return 1;
}
