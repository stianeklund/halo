#include "camera_internal.h"
#include "observer.h"
#include "x87_math.h"

/* Camera observer — tracks camera position/orientation per player. */

#line 177
/* Reset a flying camera (0x89330) to a level facing and the default 70-degree
 * field of view.  position.z is left untouched, as in the original. */
void flying_camera_new(void *flying_camera)
{
  flying_camera_t *camera = (flying_camera_t *)flying_camera;

  camera->position.y = 0.0f;
  camera->position.x = 0.0f;
  camera->facing.yaw = 0.0f;
  camera->facing.pitch = 0.0f;
  camera->roll = 0.0f;
  camera->field_of_view = 1.22173047f;
}

/* Initialize a flying camera at a position looking along forward (0x89350). */
void FUN_00089350(void *camera_data, float *position, float *forward)
{
  flying_camera_t *camera = (flying_camera_t *)camera_data;

  camera->position.y = 0.0f;
  camera->position.x = 0.0f;
  camera->facing.pitch = 0.0f;
  camera->roll = 0.0f;
  camera->facing.yaw = 0.0f;
  camera->field_of_view = 1.22173047f;
  camera->position = *(real_point3d *)position;
  vector_to_angles((float *)&camera->facing, forward);
}

/* flying_camera_update (0x893a0): the second entry of the gameplay cycle. */
void flying_camera_update(void *camera_data, void *control_data,
                          void *result_data)
{
  flying_camera_t *camera;
  camera_action_t *controls;
  camera_command_t *result;
  real cosine_yaw;
  real sine_yaw;
  real_vector3d translation;
  real_point3d position;

  camera = (flying_camera_t *)camera_data;
  controls = (camera_action_t *)control_data;
  result = (camera_command_t *)result_data;
  assert_halt_at("c:\\halo\\SOURCE\\camera\\flying_camera.c", 0x29, camera);
  assert_halt_at("c:\\halo\\SOURCE\\camera\\flying_camera.c", 0x2a, controls);
  assert_halt_at("c:\\halo\\SOURCE\\camera\\flying_camera.c", 0x2b, result);
  if (controls->active) {
    camera->facing.yaw += controls->facing_delta.yaw;
    camera->facing.pitch += controls->facing_delta.pitch;
    camera->facing.pitch =
      camera->facing.pitch < -1.56765485f ?
        -1.56765485f :
        (camera->facing.pitch > 1.56765485f ? 1.56765485f :
                                              camera->facing.pitch);
    camera->roll += controls->facing_delta.roll;
  }
  if (flying_camera_reset_ticks > 0) {
    camera->facing.yaw = 0.0f;
    camera->facing.pitch = 0.0f;
    camera->roll = 0.0f;
    --flying_camera_reset_ticks;
  }
  result->timer = 0.3f;
  angles_to_vector((float *)&result->forward, (float *)&camera->facing);
  observer_up_from_forward((float *)&result->forward, (float *)&result->up);
  rotate_vector3d_by_sincos((float *)&result->up, (float *)&result->forward,
                            x87_fsin(camera->roll), x87_fcos(camera->roll));
  if (controls->active) {
    cosine_yaw = x87_fcos(camera->facing.yaw);
    sine_yaw = x87_fsin(camera->facing.yaw);
    translation.i =
      cosine_yaw * controls->translation.i - sine_yaw * controls->translation.j;
    translation.j =
      cosine_yaw * controls->translation.j + sine_yaw * controls->translation.i;
    translation.k = controls->translation.k;
    position.x = camera->position.x + translation.i;
    position.y = camera->position.y + translation.j;
    position.z = camera->position.z + translation.k;
    camera->position = position;
  }
  result->position = camera->position;
  result->offset = *(real_vector3d *)global_zero_vector_ptr;
  result->depth = 0.0f;
  result->field_of_view = camera->field_of_view;
  result->flags = FLAG(_observer_command_valid_bit);
  if (!camera_command_valid(result)) {
    display_assert(csprintf(error_string_buffer, CAMERA_COMMAND_INVALID_FORMAT,
                            CAMERA_COMMAND_INVALID_ARGUMENTS(result)),
                   "c:\\halo\\SOURCE\\camera\\flying_camera.c", 0x95, 1);
    system_exit(-1);
  }
}

/* Initialize following-camera state (0x89850). */
void following_camera_new(void *camera_data)
{
  following_camera_t *camera;

  if (camera_data == NULL) {
    display_assert("camera", "c:\\halo\\SOURCE\\camera\\following_camera.c",
                   0x13, 1);
    system_exit(-1);
  }

  camera = (following_camera_t *)camera_data;
  camera->initialized = false;
  camera->confined = false;
  camera->crouched = false;
  camera->zoomed = false;
  camera->zoom_level = 0;
  camera->facing_offset.pitch = 0.0f;
  camera->facing_offset.yaw = 0.0f;
  camera->unit_index = NONE;
  camera->seat_index = NONE;
  camera->distance_scale = 1.0f;
}

/* Resolve the camera a unit looks through (0x898b0): the seat's camera when
 * the unit rides a vehicle in a seat with any of flags 0x15, otherwise the
 * unit definition's own camera.  The unit handle arrives in EAX. */
void *unit_camera_get(int unit_handle /* @<eax> */)
{
  uint8_t *seat_camera;
  unit_data_t *unit;
  object_data_t *vehicle;
  unit_definition_t *vehicle_definition;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  if (unit->object.parent_object_index.value != NONE) {
    vehicle = (object_data_t *)object_try_and_get_and_verify_type(
      unit->object.parent_object_index.value, 2);
    if (vehicle != NULL) {
      vehicle_definition =
        (unit_definition_t *)tag_get('vehi', vehicle->tag_index);
      seat_camera = (uint8_t *)tag_block_get_element(
        &vehicle_definition->seats, (int)(int16_t)unit->parent_seat_index,
        sizeof(unit_seat_t));
      if ((((unit_seat_t *)seat_camera)->flags & 0x15) != 0) {
        /* The seat's unit camera starts at seat +0x84 (inside pad_04). */
        seat_camera = (uint8_t *)((uintptr_t)seat_camera + 0x84);
        if (seat_camera != NULL) {
          return seat_camera;
        }
      }
    }
  }
  /* The unit definition's unit camera starts at +0x1a8. */
  return (char *)tag_get('unit', unit->object.tag_index) + 0x1a8;
}

/* arcsine (0x89930) — Single-precision arc sine.
 * The original is a five-instruction wrapper: PUSH EBP / MOV EBP,ESP /
 * FLD dword ptr [EBP+8] / POP EBP / JMP 0x1da0cc, i.e. it loads the single
 * float stack argument into ST(0) and tail-jumps to the MSVC CRT _CIasin
 * core (0x1da0cc), which returns its result in ST(0). Faithful equivalent is
 * asin(x) on the widened argument; the float param and float return are read
 * off the FLD/ST(0) pair above. */
float arcsine(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
  double asin(double x);
  return (float)asin((double)x);
#else
  return (float)asin((double)x);
#endif
}

/* uniform_cubic_spline (0x89940, ..\math\real_math.h inline emitted out of
 * line in this object) -- cubic through four samples f0..f3 at uniformly
 * spaced knots t0, t0+h, t0+2h, t0+3h, evaluated at t by Newton forward
 * differences.  Asserts are real_math.h lines 0x5e4/0x5e5 with the XBE's own
 * strings "h > 0.0f" and "t >= t0 && t <= t0 + 3.0f*h".  The differences are
 * written back into the f1/f3 parameter slots (FSTP [EBP+0xc] / [EBP+0x14] at
 * 0x899c8 / 0x899cf), i.e. in-place parameter updates.
 * 3.0f is the pool
 * constant at 0x254644. */
float uniform_cubic_spline(float f0, float f1, float f2, float f3, float t0,
                           float h, float t)
{
  assert_halt_msg_at("h > 0.0f", "..\\math\\real_math.h", 0x5e4,
                     h > 0.0f);
  assert_halt_msg_at("t >= t0 && t <= t0 + 3.0f*h", "..\\math\\real_math.h",
                     0x5e5, t >= t0 && t <= t0 + 3.0f * h);

  f3 -= f2;
  f2 -= f1;
  f1 -= f0;
  /* 0x899c8 / 0x899cf: f1 and the second f3 are FSTP'd to their dword
   * parameter slots (narrowed to float); f2 stays in ST(i). */
  HALO_FLT_ROUNDTRIP(f1);
  f3 -= f2;
  HALO_FLT_ROUNDTRIP(f3);
  f2 -= f1;

  return f0 + (t - t0) / h *
                (f1 + (t - (t0 + h)) *
                        (f2 + (t - (t0 + 2.0f * h)) * (f3 - f2) / (3.0f * h)) /
                        (2.0f * h));
}

/* Evaluate the scalar interpolator uniform_cubic_spline once per component of a
 * 3-vector (0x89a20). The four input pointers supply the four control values
 * y0..y3 for each component; t0/h/t are shared scalars forwarded unchanged to
 * every component. Results are stored into out[0..2] as each call returns. */
void FUN_00089a20(float *out, float *p0, float *p1, float *p2, float *p3,
                  float t0, float h, float t)
{
  out[0] = uniform_cubic_spline(p0[0], p1[0], p2[0], p3[0], t0, h, t);
  out[1] = uniform_cubic_spline(p0[1], p1[1], p2[1], p3[1], t0, h, t);
  out[2] = uniform_cubic_spline(p0[2], p1[2], p2[2], p3[2], t0, h, t);
}

/* camera_track_splut (0x89ab0) — Sample the active camera track spline.
 * [TU: c:\halo\SOURCE\camera\following_camera.c — __FILE__ assert xref]
 * The definition pointer arrives in ECX; the tag_block at its +0x4c (0x1c-byte
 * elements, tag index at element +0xc) supplies the camera_track reference,
 * falling back to element 0 of the game globals block at +0x104 (0x10-byte
 * elements) when that block is empty, the element is missing, or the index is
 * NONE. The element index used for the first block is the branchless
 * MIN(count - 1, 0) the original emits (SETGE/DEC/AND at 0x89ac5).
 * The scalar t is remapped by (t + PI/2) * (1/PI), scaled by the control point
 * span and truncated to int16, then walked down until four consecutive control
 * points exist; the quadruple plus (t0, step, t) goes to the per-component
 * cubic spline evaluator at 0x89a20. */
void camera_track_splut(void *definition, float t, float *out)
{
  tag_block *tracks;
  tag_block *control_points;
  tag_reference *reference;
  camera_track_definition_t *camera_track;
  int tag_index;
  int index;
  int count;
  int count_minus_1;
  short start;
  short i;
  float step;

  tracks = &((unit_camera_t *)definition)->unit_camera_tracks;
  if (tracks->count != 0) {
    index = tracks->count - 1;
    index &= (index >= 0) - 1;
    reference =
      (tag_reference *)tag_block_get_element(tracks, index,
                                             sizeof(unit_camera_track_t));
    if (reference != NULL) {
      tag_index = reference->tag_index;
      if (tag_index != NONE) {
        goto have_track;
      }
    }
  }
  reference = (tag_reference *)tag_block_get_element(
    &((globals_definition_t *)game_globals_get())->camera_tracks, 0,
    sizeof(tag_reference));
  tag_index = reference->tag_index;

have_track:
  camera_track = (camera_track_definition_t *)tag_get('trak', tag_index);
  control_points = &camera_track->control_points;
  count = control_points->count;
  count_minus_1 = count - 1;
  t = (t + 1.5707964f) * 0.31830987f;
  start = (short)(int)(t * (float)count_minus_1);
  if (count < 4) {
    display_assert("camera_track->control_points.count >= 4",
                   "c:\\halo\\SOURCE\\camera\\following_camera.c", 0x56, 1);
    system_exit(-1);
  }
  step = 1.0f / (float)count_minus_1;

  i = start;
  if (start > 0) {
    do {
      if ((int)i + 4 <= control_points->count && (int)i <= (int)start - 1) {
        break;
      }
      i--;
    } while (i > 0);
  }

  index = (int)i;
  FUN_00089a20(
    out,
    (float *)tag_block_get_element(control_points, index,
                                   CAMERA_TRACK_CONTROL_POINT_SIZE),
    (float *)tag_block_get_element(control_points, index + 1,
                                   CAMERA_TRACK_CONTROL_POINT_SIZE),
    (float *)tag_block_get_element(control_points, index + 2,
                                   CAMERA_TRACK_CONTROL_POINT_SIZE),
    (float *)tag_block_get_element(&camera_track->control_points, index + 3,
                                   CAMERA_TRACK_CONTROL_POINT_SIZE),
    (float)index * step, step, t);
}

/* 0x89c00: unit camera position plus the camera-track offset rotated into the
 * unit's planar facing. The track parameter is asin(forward.k) (inline CRT
 * _CIasin at 0x1da0cc). */
void FUN_00089c00(int unit_handle, int out_position, int out_forward)
{
  float *position;
  real_vector3d *forward;
  unit_data_t *unit;
  void *camera_track;
  float offset[3];
  float i;
  float j;
  float length;
  double asin(double x);
  double fabs(double x);

  position = (float *)out_position;
  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  camera_track = unit_camera_get(unit_handle);
  unit_get_camera_position(unit_handle, position);
  forward = (real_vector3d *)out_forward;
  *forward = unit->aiming_vector;
  camera_track_splut(camera_track, (float)asin((double)forward->k), offset);
  i = forward->i;
  j = forward->j;
  length = x87_sqrt(i * i + j * j);
  if (!(fabs(length) < (double)0.0001f)) {
    length = 1.0f / length;
    i = length * i;
    j = length * j;
  }
  position[0] = offset[1] * j + offset[0] * i + position[0];
  position[1] = offset[0] * j - offset[1] * i + position[1];
  position[2] = offset[2] + position[2];
}

/* following_camera_update (0x89cd0): restore the vehicle/following command,
 * including unit/seat transitions and the crouch and look blend channels. */
void following_camera_update(void *camera_data, void *control_data,
                             void *result_data)
{
  following_camera_t *camera;
  camera_action_t *controls;
  camera_command_t *result;
  player_control_unit_camera_info_t info;
  real_euler_angles2d facing;
  real_vector3d track_offset;
  real depth;
  int crouched;
  unit_data_t *unit;

  camera = (following_camera_t *)camera_data;
  controls = (camera_action_t *)control_data;
  result = (camera_command_t *)result_data;
  assert_halt_at("c:\\halo\\SOURCE\\camera\\following_camera.c", 0x8a, camera);
  assert_halt_at("c:\\halo\\SOURCE\\camera\\following_camera.c", 0x8b, result);
  player_control_get_unit_camera_info(controls->local_player_index, &info);
  result->position = info.position;
  result->timer = 0.0f;
  result->flags = 0;
  result->field_of_view = 1.22173047f;
  if (camera->initialized && (info.unit_index != camera->unit_index ||
                              info.seat_index != camera->seat_index))
    result->timer = 1.0f;
  camera->unit_index = info.unit_index;
  camera->seat_index = info.seat_index;
  if (info.camera != NULL) {
    unit = (unit_data_t *)object_get_and_verify_type(info.unit_index, 3);
    /* 0x89d9d loads only the low byte of the unit control flags (+0x1b8). */
    crouched = (*(uint8_t *)&unit->unk_440 & 1) ||
               (*(uint8_t *)&unit->unk_440 & 2);
    if (crouched != camera->crouched) {
      result->field_4c[1] = 1;
      result->field_54[1] =
        0.5f > result->field_54[1] ? 0.5f : result->field_54[1];
      camera->crouched = crouched;
    }
    if (controls->active) {
      camera->facing_offset.yaw += controls->facing_delta.yaw;
      camera->facing_offset.pitch += controls->facing_delta.pitch;
      result->field_4c[4] = 1;
      result->field_54[4] =
        0.4f > result->field_54[4] ? 0.4f : result->field_54[4];
    } else if (camera->facing_offset.yaw != 0.0f ||
               camera->facing_offset.pitch != 0.0f) {
      camera->facing_offset.pitch = 0.0f;
      camera->facing_offset.yaw = 0.0f;
    }
    facing = *(real_euler_angles2d *)player_control_get_facing_angles(
      controls->local_player_index);
    facing.yaw += camera->facing_offset.yaw;
    facing.pitch = facing.pitch + camera->facing_offset.pitch < -1.57079637f ?
                     -1.57079637f :
                     (facing.pitch + camera->facing_offset.pitch > 1.57079637f ?
                        1.57079637f :
                        facing.pitch + camera->facing_offset.pitch);
    angles_to_vector((float *)&result->forward, (float *)&facing);
    assert_halt_msg_at(
      "magnitude3d(&result->forward) > 0.9999f && "
      "magnitude3d(&result->forward) < 1.0001f",
      "c:\\halo\\SOURCE\\camera\\following_camera.c", 0xd4,
      (real)x87_sqrtd(magnitude_squared3d(&result->forward)) > 0.999899983f &&
        (real)x87_sqrtd(magnitude_squared3d(&result->forward)) < 1.00010002f);
    camera_track_splut(info.camera, facing.pitch, (float *)&track_offset);
    depth = (real)x87_sqrtd(track_offset.i * track_offset.i +
                            track_offset.j * track_offset.j +
                            track_offset.k * track_offset.k);
    result->depth = depth;
    result->offset.i = (depth * x87_fcos(facing.pitch) + track_offset.i) *
                       camera->distance_scale;
    result->offset.j = -track_offset.j * camera->distance_scale;
    result->offset.k = (depth * x87_fsin(facing.pitch) + track_offset.k) *
                       camera->distance_scale;
    depth = (depth - 0.6f) * camera->distance_scale + 0.6f;
    result->depth = depth > 0.6f ? depth : 0.6f;
    object_get_root_location(info.unit_index, (float *)&result->velocity, NULL);
    result->flags |= FLAG(_observer_command_valid_bit);
  }
  observer_up_from_forward((float *)&result->forward, (float *)&result->up);
  if (!camera_command_valid(result)) {
    display_assert(csprintf(error_string_buffer, CAMERA_COMMAND_INVALID_FORMAT,
                            CAMERA_COMMAND_INVALID_ARGUMENTS(result)),
                   "c:\\halo\\SOURCE\\camera\\following_camera.c", 0xee, 1);
    system_exit(-1);
  }
  camera->initialized = true;
}

void observer_initialize(void)
{
}

/* Reset one observer to the default view (0x8a350): world origin, forward and
 * up axes and a 50-degree field of view, with an empty last command. */
void observer_result_initialize(void *observer_data)
{
  observer_data_t *observer = (observer_data_t *)observer_data;

  observer->forward = *(real_vector3d *)global_forward_vector_ptr;
  observer->up = *(real_vector3d *)global_up_vector_ptr;
  observer->field_of_view = 0.87266463f;

  observer->result.position = *global_origin3d_ptr;
  observer->result.location.cluster_index = -1;
  observer->result.location.leaf_index = -1;
  observer->result.velocity = *(real_vector3d *)global_zero_vector_ptr;
  observer->result.forward = *(real_vector3d *)global_forward_vector_ptr;
  observer->result.up = *(real_vector3d *)global_up_vector_ptr;
  observer->result.field_of_view = 0.87266463f;

  csmemset(&observer->last_command, 0, sizeof(observer->last_command));
  observer->last_command.forward = observer->forward;
  observer->last_command.up = observer->up;
  observer->last_command.field_of_view = observer->field_of_view;

  observer->trailer_signature = OBSERVER_SIGNATURE;
  observer->header_signature = OBSERVER_SIGNATURE;
  observer->updated_for_frame = 1;
  observer->first_command = 0;
}

/* Initialize observers for all 4 players. Calls observer_result_initialize
 * with ESI pointing to each player's observer data (base 0x33571c,
 * stride 0x29c). */
void observer_initialize_for_new_map(void)
{
  int16_t i;
  observer_t *observer = observer_globals.local_players;

  for (i = 0; i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; i++) {
    assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\camera\\observer.c", 0x72, i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
    observer_result_initialize(observer);
    observer++;
  }
}

void observer_dispose_from_old_map(void)
{
}

/* Return a pointer to the observer camera result for a local player.
 * Base at 0x33571c, stride 0x29c, camera result at offset +0x74.
 * Validates the cluster index against the current BSP. */
void *observer_get_camera(unsigned __int16 local_player_index)
{
  int16_t idx = (int16_t)local_player_index;
  observer_data_t *observer;

  if (idx == NONE)
    return NULL;

  assert_halt(((idx >= 0) & 0xFFFFu) && idx < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = observer_data_get(idx);

  if (observer->result.location.cluster_index < NONE ||
      (int)observer->result.location.cluster_index >=
        ((structure_bsp_t *)scenario_get())->clusters.count) {
    display_assert("observer->result.location.cluster_index>=NONE && "
                   "observer->result.location.cluster_index<"
                   "global_structure_bsp_get()->clusters.count",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x12d, 1);
    system_exit(-1);
  }

  return &observer->result;
}

/* Return true when the observer has no active command timer. */
boolean observer_command_has_finished(int16_t local_player_index)
{
  observer_data_t *observer;
  int16_t parameter_index;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = observer_data_get(local_player_index);
  if (observer->last_command.timer != 0.0f)
    return false;

  for (parameter_index = 0;
       parameter_index < NUMBER_OF_OBSERVER_COMMAND_PARAMETERS;
       parameter_index++) {
    if (observer->last_command.field_54[parameter_index] != 0.0f)
      return false;
  }

  return true;
}

void observer_reconnect_to_structure_bsp(void)
{
  int16_t local_player_index;
  observer_data_t *observer;

  local_player_index = 0;
  observer = observer_data_get(0);
  do {
    if (local_player_get_player_index(local_player_index) != -1) {
      if (local_player_index < 0 ||
          local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
        display_assert("local_player_index>=0 && "
                       "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                       "c:\\halo\\SOURCE\\camera\\observer.c", 0x72, 1);
        system_exit(-1);
      }
      scenario_location_from_point(&observer->result.location,
                                   &observer->result.position);
    }
    local_player_index++;
    observer++;
  } while (local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
}

/* Evaluate each command parameter's acceleration (0x8a660): the second
 * derivative 20*c5*t^3 + 12*c4*t^2 + 6*c3*t + 2*c2 of its quintic, at the time
 * left on its timer after this tick.  An acceleration beyond the parameter's
 * limit ends that timer and every other timer holding the same value. */
void observer_apply_acceleration(int16_t local_player_index)
{
  observer_data_t *observer;
  real *coefficient5, *coefficient4, *coefficient3, *coefficient2;
  real *accelerations;
  real *timer, *timers;
  int16_t *sizes;
  real *limits;
  int16_t parameter_index;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = observer_data_get(local_player_index);
  coefficient5 = observer->coefficient5;
  coefficient4 = observer->coefficient4;
  coefficient3 = observer->coefficient3;
  coefficient2 = observer->coefficient2;
  accelerations = observer->accelerations.n;
  timers = observer->last_command.field_54;
  timer = timers;
  sizes = observer_velocity_sizes;
  limits = observer_acceleration_limits;

  for (parameter_index = 0;
       parameter_index < NUMBER_OF_OBSERVER_COMMAND_PARAMETERS;
       parameter_index++) {
    float t = *timer - observer_globals.dtime;

    if (t <= 0.0f) {
      csmemset(accelerations, 0, (int)*sizes << 2);
    } else {
      float t_sq = t * t;
      float t_cu = t_sq * t;
      int16_t j;

      for (j = 0; j < *sizes; j++) {
        float acceleration = t_cu * coefficient5[j] * 20.0f +
                             t_sq * coefficient4[j] * 12.0f +
                             t * coefficient3[j] * 6.0f +
                             coefficient2[j] + coefficient2[j];
        accelerations[j] = acceleration;

        if (acceleration > *limits || acceleration < -*limits) {
          int16_t k;
          float *other = timers;
          for (k = 0; k < NUMBER_OF_OBSERVER_COMMAND_PARAMETERS; k++) {
            if (k != parameter_index && *other == *timer)
              *other = 0.0f;
            other++;
          }
          *timer = 0.0f;
        }
      }
    }

    coefficient5 += *sizes;
    coefficient4 += *sizes;
    accelerations += *sizes;
    coefficient3 += *sizes;
    coefficient2 += *sizes;
    timer++;
    limits++;
    sizes++;
  }
}

/* Evaluate each command parameter's velocity (0x8a830): the first derivative
 * 5*c5*t^4 + 4*c4*t^3 + 3*c3*t^2 + 2*c2*t + c1 of its quintic while its timer
 * runs.  Once the timer has run out a valid command either stops the
 * parameter (field_4c bit 1 or the force-time flag) or backs out the remaining
 * displacement over one tick. */
void observer_integrate(int16_t local_player_index)
{
  observer_data_t *observer;
  real *displacements, *coefficient5, *coefficient4, *coefficient3;
  real *coefficient2, *coefficient1;
  real *velocities, *timer;
  uint8_t *parameter_flags;
  int16_t *sizes;
  float inverse_dtime;
  int count;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  inverse_dtime = (float)(1.0 / observer_globals.dtime);

  observer = observer_data_get(local_player_index);
  displacements = observer->displacements.n;
  coefficient5 = observer->coefficient5;
  coefficient4 = observer->coefficient4;
  coefficient3 = observer->coefficient3;
  coefficient2 = observer->coefficient2;
  coefficient1 = observer->coefficient1;
  timer = observer->last_command.field_54;
  parameter_flags = observer->last_command.field_4c;
  velocities = observer->velocities.n;
  sizes = observer_velocity_sizes;

  for (count = NUMBER_OF_OBSERVER_COMMAND_PARAMETERS; count != 0; count--) {
    float t = *timer - observer_globals.dtime;
    int16_t size = *sizes;

    if (t > 0.0f) {
      float t_sq = t * t;
      float t_cu = t_sq * t;
      float t_q4 = t_cu * t;
      int16_t i;

      for (i = 0; i < size; i++) {
        float v = t * coefficient2[i];
        velocities[i] = v + v + t_sq * coefficient3[i] * REAL_3_0_POOL +
                        t_cu * coefficient4[i] * REAL_4_0_POOL +
                        t_q4 * coefficient5[i] * REAL_5_0_POOL +
                        coefficient1[i];
      }
    } else {
      uint32_t flags = observer->last_command.flags;
      if ((flags & FLAG(_observer_command_valid_bit)) &&
          ((*parameter_flags & 2) ||
           (flags & FLAG(_observer_command_force_time_bit)))) {
        csmemset(velocities, 0, (int)size << 2);
      } else if ((flags & FLAG(_observer_command_valid_bit)) && size > 0) {
        int16_t i;
        for (i = 0; i < size; i++)
          velocities[i] = -(inverse_dtime * displacements[i]);
      }
    }

    displacements += size;
    coefficient5 += size;
    coefficient4 += size;
    coefficient3 += size;
    coefficient2 += size;
    velocities += size;
    coefficient1 += size;
    timer++;
    parameter_flags++;
    sizes++;
  }
}

/* Reset a local player's observer result to its default state (0x8aa30).
 * Observer array base 0x33571c, stride 0x29c. */
void observer_obsolete_position(int16_t local_player_index)
{
  if (local_player_index < 0 ||
      local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
    display_assert("local_player_index>=0 && "
                   "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x72, 1);
    system_exit(-1);
  }
  observer_result_initialize(&observer_globals.local_players[local_player_index]);
}

/* Derive a camera up vector from a forward vector (0x8aa80), cdecl, 2 args.
 * right = (forward.y, -forward.x, 0) is normalized with normalize3d; if its
 * length is 0.0f (forward is vertical) right falls back to (1, 0, 0).
 * up = right x forward. */
void observer_up_from_forward(float *forward, float *up)
{
  real_vector3d right;
  float i;
  float j;
  float k;

  /* The cross product is the inlined
   * cross_product3d(&right, forward, up), which computes k, j, i into
   * temporaries before storing i, j, k. */
  right.i = forward[1];
  right.j = -forward[0];
  right.k = 0.0f;
  if (normalize3d((float *)&right) == 0.0f) {
    right.i = 1.0f;
    right.j = right.k = 0.0f;
  }
  k = right.i * forward[1] - right.j * forward[0];
  j = right.k * forward[0] - right.i * forward[2];
  i = right.j * forward[2] - right.k * forward[1];
  up[0] = i;
  up[1] = j;
  up[2] = k;
}

/* Rotate two vectors about a rotational-displacement axis (0x8ab10).
 * The EAX argument is a rotation vector whose direction is the axis and whose
 * magnitude is the angle in radians. A local copy is normalized in place by
 * normalize3d, which returns the original magnitude; a zero magnitude means no
 * displacement and the function returns without touching either vector.
 * Otherwise sin and cos of the angle are computed once and both vectors are
 * rotated about the normalized axis. */
void observer_apply_rotational_displacement(float *rotation, float *vector_a,
                                            float *vector_b)
{
  real_vector3d axis;
  float angle;
  float sin_angle;
  float cos_angle;

  axis = *(real_vector3d *)rotation;

  angle = normalize3d((float *)&axis);
  if (angle != 0.0f) {
    sin_angle = x87_fsin(angle);
    cos_angle = x87_fcos(angle);
    rotate_vector3d_by_sincos(vector_a, (float *)&axis, sin_angle, cos_angle);
    rotate_vector3d_by_sincos(vector_b, (float *)&axis, sin_angle, cos_angle);
  }
}

/* Cast a collision ray for the observer camera (0x8ab90) from ray_origin to
 * ray_endpoint under collision user 0xc, storing the hit fraction in
 * *out_fraction when something is hit. */
bool FUN_0008ab90(float *out_fraction, bool indoor, float *ray_origin,
                  float *ray_endpoint)
{
  uint32_t flags;
  bool result;
  real_vector3d direction;
  struct collision_result collision;

  result = false;
  flags = 0x40e1;
  if (indoor)
    flags = 0x40a1;

  if (global_current_collision_user_depth >= MAXIMUM_COLLISION_USER_STACK_DEPTH) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x4b4, 1);
    system_exit(-1);
  }

  collision_user_stack[global_current_collision_user_depth++] = 0xc;

  direction.i = ray_endpoint[0] - ray_origin[0];
  direction.j = ray_endpoint[1] - ray_origin[1];
  direction.k = ray_endpoint[2] - ray_origin[2];

  if (FUN_0014df70(flags, ray_origin, (float *)&direction, -1,
                   (int16_t *)&collision)) {
    *out_fraction = collision.t;
    result = true;
  }

  if (global_current_collision_user_depth <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x4ba, 1);
    system_exit(-1);
  }
  --global_current_collision_user_depth;

  return result;
}

/* 0x8ac70: vector @<eax>, point @<ecx>, ray_origin @<esi>; the callee's AL
 * result is left in EAX. No direct callers in the binary. */
bool observer_collision_test_differential(float scale, float *out_fraction,
                                          bool indoor, float *ray_origin,
                                          float *point, float *vector)
{
  real_point3d endpoint;

  endpoint.x = scale * vector[0] + point[0];
  endpoint.y = scale * vector[1] + point[1];
  endpoint.z = scale * vector[2] + point[2];
  return FUN_0008ab90(out_fraction, indoor, ray_origin, (float *)&endpoint);
}

/* Hand a camera command block to a local player's observer (0x8acb0).
 * Sole caller: director_update (0x877a3, cdecl, `push eax; push esi`, ADD
 * ESP,8) passing the player index and the director's per-player command.
 * A NULL command, or one carrying _observer_command_valid_bit that fails
 * camera_command_valid, trips the "Invalid camera command." assert (line
 * 0xe9).  The command becomes the observer's pending_command and
 * updated_for_frame is cleared -- the only store that clears it.  On the
 * observer's first command its timer is zeroed, the force-time flag is
 * raised and the five field_54 timers are cleared. */
void observer_set_camera(int16_t local_player_index, void *camera_command)
{
  camera_command_t *command = (camera_command_t *)camera_command;
  observer_t *observer;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  observer = &observer_globals.local_players[local_player_index];

  if (!(command && camera_command_valid(command))) {
    display_assert(csprintf(error_string_buffer, CAMERA_COMMAND_INVALID_FORMAT,
                            CAMERA_COMMAND_INVALID_ARGUMENTS(command)),
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0xe9, 1);
    system_exit(-1);
  }

  observer->pending_command = command;
  observer->updated_for_frame = 0;
  if (!observer->first_command) {
    observer->first_command = 1;
    observer->pending_command->timer = 0.0f;
    observer->pending_command->flags |= FLAG(_observer_command_force_time_bit);
    csmemset(observer->pending_command->field_54, 0,
             sizeof(observer->pending_command->field_54));
  }
}

/* Stage the pending camera command (0x8b060).  The command is validated
 * ("Invalid camera command.", line 0x172); for a valid command each of the
 * five parameter timers is chosen against the observer's running timer
 * (capped at 2 seconds) before the command is copied into last_command. */
void observer_update_command(int16_t local_player_index)
{
  observer_data_t *observer;
  camera_command_t *command;
  real *timer;
  uint8_t *parameter_flags;
  real *last_timer;
  int i;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = observer_data_get(local_player_index);
  command = observer->pending_command;
  timer = command->field_54;
  last_timer = observer->last_command.field_54;
  parameter_flags = command->field_4c;

  if (!(command && camera_command_valid(command))) {
    display_assert(csprintf(error_string_buffer, CAMERA_COMMAND_INVALID_FORMAT,
                            CAMERA_COMMAND_INVALID_ARGUMENTS(command)),
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x172, 1);
    system_exit(-1);
  }

  if (observer->pending_command->flags & FLAG(_observer_command_valid_bit)) {
    for (i = NUMBER_OF_OBSERVER_COMMAND_PARAMETERS; i != 0; i--) {
      if ((*parameter_flags & 1) != 0) {
        if ((*parameter_flags & 2) == 0 && *timer < *last_timer)
          *timer = *last_timer > 2.0f ? 2.0f : *last_timer;
      } else {
        command = observer->pending_command;
        if (command->timer < *last_timer &&
            (command->flags & FLAG(_observer_command_force_time_bit)) == 0)
          *timer = *last_timer > 2.0f ? 2.0f : *last_timer;
        else
          *timer = command->timer;
      }

      timer++;
      last_timer++;
      parameter_flags++;
    }

    observer->last_command = *observer->pending_command;
  }
}

/* Compute quintic Hermite acceleration coefficients for observer interpolation
 * (0x8b470). Validates the observer command state (forward/up perpendicular,
 * position/orientation in range, velocity valid, distance/FOV/timer bounded).
 * When mode bit 0 is set and timer > delta_time, computes snap/jerk/accel/vel/
 * pos/extra polynomial coefficients for each of 5 observer components.
 * Component 0 receives additional velocity-dependent correction terms. */
void observer_compute_accelerations(int16_t local_player_index)
{
  char *observer;
  char *mode_ptr;
  float *snap_ptr, *jerk_ptr, *accel_ptr, *vel_ptr, *pos_ptr, *extra_ptr;
  float *accel_out_ptr, *vel_out_ptr, *result_ptr;
  float *timers;
  int16_t comp;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)observer_data_get(local_player_index);

  snap_ptr = (float *)(observer + 0x158);
  accel_ptr = (float *)(observer + 0x1b0);
  pos_ptr = (float *)(observer + 0x208);
  jerk_ptr = (float *)(observer + 0x184);
  timers = (float *)(observer + 0x5c);
  vel_ptr = (float *)(observer + 0x1dc);
  extra_ptr = (float *)(observer + 0x234);
  vel_out_ptr = (float *)(observer + 0xe8);
  accel_out_ptr = (float *)(observer + 0x120);
  result_ptr = (float *)(observer + 0x260);

  mode_ptr = observer + 0x8;

  /* Validate observer command state when mode bit 0 is set */
  if (mode_ptr == NULL ||
      ((*(uint8_t *)mode_ptr & 1) &&
       (!valid_real_normal3d_perpendicular((float *)(observer + 0x2c),
                                           (float *)(observer + 0x38)) ||
        (*(uint32_t *)(observer + 0xc) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0xc) >= -5000.0f) ||
        !(*(float *)(observer + 0xc) <= 5000.0f) ||
        (*(uint32_t *)(observer + 0x10) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x10) >= -5000.0f) ||
        !(*(float *)(observer + 0x10) <= 5000.0f) ||
        (*(uint32_t *)(observer + 0x14) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x14) >= -5000.0f) ||
        !(*(float *)(observer + 0x14) <= 5000.0f) ||
        (*(uint32_t *)(observer + 0x18) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x18) >= -5000.0f) ||
        !(*(float *)(observer + 0x18) <= 5000.0f) ||
        (*(uint32_t *)(observer + 0x1c) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x1c) >= -5000.0f) ||
        !(*(float *)(observer + 0x1c) <= 5000.0f) ||
        (*(uint32_t *)(observer + 0x20) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x20) >= -5000.0f) ||
        !(*(float *)(observer + 0x20) <= 5000.0f) ||
        !real_vector3d_valid((float *)(observer + 0x44)) ||
        (*(uint32_t *)(observer + 0x24) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x24) >= 0.0f) ||
        !(*(float *)(observer + 0x24) <= 5000.0f) ||
        (*(uint32_t *)(observer + 0x28) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x28) >= 0.001f) ||
        !(*(float *)(observer + 0x28) <= 1.57079637f) ||
        (*(uint32_t *)(observer + 0x50) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x50) >= 0.0f) ||
        !(*(float *)(observer + 0x50) <= 3600.0f)))) {
    char *msg = csprintf(
      error_string_buffer,
      "Invalid camera command.\n"
      "F: (%f, %f, %f) U: (%f, %f, %f)\n"
      "P: (%f, %f, %f) O: (%f, %f, %f)\n"
      "D: %f V: (%f, %f, %f), FOV: %f, T: %f, FL: %ld",
      (double)*(float *)(observer + 0x2c), (double)*(float *)(observer + 0x30),
      (double)*(float *)(observer + 0x34), (double)*(float *)(observer + 0x38),
      (double)*(float *)(observer + 0x3c), (double)*(float *)(observer + 0x40),
      (double)*(float *)(observer + 0x0c), (double)*(float *)(observer + 0x10),
      (double)*(float *)(observer + 0x14), (double)*(float *)(observer + 0x18),
      (double)*(float *)(observer + 0x1c), (double)*(float *)(observer + 0x20),
      (double)*(float *)(observer + 0x24), (double)*(float *)(observer + 0x44),
      (double)*(float *)(observer + 0x48), (double)*(float *)(observer + 0x4c),
      (double)*(float *)(observer + 0x28), (double)*(float *)(observer + 0x50),
      *(uint32_t *)(observer + 0x8));
    display_assert(msg, "c:\\halo\\SOURCE\\camera\\observer.c", 0x1f6, 1);
    system_exit(-1);
  }

  for (comp = 0; comp < 5; comp++) {
    if ((*(uint8_t *)(observer + 0x8) & 1) && *timers > observer_globals.dtime) {
      float f = 1.0f / *timers;
      float f2 = f * f;
      float f3 = f2 * f;
      float f4 = f3 * f;
      int16_t size = observer_velocity_sizes[comp];
      int16_t j;

      for (j = 0; j < size; j++) {
        snap_ptr[j] =
          f3 * accel_out_ptr[j] * 0.5f -
          (f4 * vel_out_ptr[j] * 3.0f +
           f4 * f * result_ptr[j] * 6.0f);

        jerk_ptr[j] = f3 * vel_out_ptr[j] * 7.0f +
                          f4 * result_ptr[j] * 15.0f -
                          f2 * accel_out_ptr[j];

        accel_ptr[j] =
          f * accel_out_ptr[j] * 0.5f -
          (f2 * vel_out_ptr[j] * 4.0f +
           f3 * result_ptr[j] * 10.0f);

        *(int *)&vel_ptr[j] = 0;
        *(int *)&pos_ptr[j] = 0;
        *(int *)&extra_ptr[j] = *(int *)&result_ptr[j];

        if (comp == 0) {
          float fv = ((float *)(observer + 0x44))[j] * TICKS_PER_SECOND;
          snap_ptr[j] -= f4 * fv * 3.0f;
          jerk_ptr[j] += f3 * fv * 8.0f;
          accel_ptr[j] -= f2 * fv * 6.0f;
          pos_ptr[j] += fv;
        }
      }
    }

    {
      int size = observer_velocity_sizes[comp];
      snap_ptr += size;
      jerk_ptr += size;
      accel_ptr += size;
      vel_ptr += size;
      pos_ptr += size;
      extra_ptr += size;
      vel_out_ptr += size;
      accel_out_ptr += size;
      result_ptr += size;
      timers++;
    }
  }
}

/* Advance each observer parameter by one tick (0x8ba10).  A parameter whose
 * timer has run out under a valid command snaps to the command's value;
 * otherwise its step is the quintic c5*t^5 + ... + c0 while the timer runs,
 * or -dtime * velocity once it has stopped.  Parameters 0-3 add the step;
 * the forward/up pair (parameter 4) rotates by it.  Forward and up are
 * re-orthonormalized when they drift. */
void observer_compute_update(int16_t local_player_index)
{
  observer_data_t *observer;
  real *parameters;
  real *command_parameters;
  real *coefficient5;
  real *coefficient4;
  real *coefficient3;
  real *coefficient2;
  real *velocities;
  real *coefficient1;
  real *coefficient0;
  real *timer;
  float steps[NUMBER_OF_OBSERVER_PARAMETERS];
  float *step;
  int16_t parameter_index;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = observer_data_get(local_player_index);

  parameters = (real *)&observer->focus_position;
  command_parameters = (real *)&observer->last_command.position;
  coefficient5 = observer->coefficient5;
  coefficient4 = observer->coefficient4;
  coefficient3 = observer->coefficient3;
  coefficient2 = observer->coefficient2;
  velocities = observer->velocities.n;
  coefficient1 = observer->coefficient1;
  coefficient0 = observer->coefficient0;
  timer = observer->last_command.field_54;
  step = steps;

  {
    float *velocity = velocities;
    int count = NUMBER_OF_OBSERVER_VELOCITIES;
    do {
      if ((*(uint32_t *)velocity & 0x7f800000u) == 0x7f800000u) {
        char *message =
          csprintf(error_string_buffer, "%s: assert_valid_real(0x%08X %f)",
                   "observer->velocities.n[parameter_index]",
                   *(uint32_t *)velocity, (double)*velocity);
        display_assert(message, "c:\\halo\\SOURCE\\camera\\observer.c", 0x2f4,
                       1);
        system_exit(-1);
      }
      velocity++;
      count--;
    } while (count != 0);
  }

  for (parameter_index = 0;
       parameter_index < NUMBER_OF_OBSERVER_COMMAND_PARAMETERS;
       parameter_index++) {
    float t = *timer - observer_globals.dtime;

    if (t <= 0.0f &&
        (*(uint8_t *)&observer->last_command.flags &
         FLAG(_observer_command_valid_bit))) {
      int16_t j = 0;
      if (j < observer_parameter_sizes[parameter_index]) {
        do {
          *(uint32_t *)&parameters[j] = *(uint32_t *)&command_parameters[j];
          j++;
        } while (j < observer_parameter_sizes[parameter_index]);
      }
    } else {
      if (t <= REAL_ZERO_POOL) {
        int16_t j = 0;
        if (j < observer_velocity_sizes[parameter_index]) {
          do {
            step[j] = -(observer_globals.dtime * velocities[j]);
            j++;
          } while (j < observer_velocity_sizes[parameter_index]);
        }
      } else {
        float t2 = t * t;
        float t3 = t2 * t;
        float t4 = t3 * t;
        float t5 = t4 * t;
        int16_t j = 0;

        if (j < observer_velocity_sizes[parameter_index]) {
          do {
            int index = (int)j;
            j++;
            step[index] = t5 * coefficient5[index] + t4 * coefficient4[index] +
                          t3 * coefficient3[index] + t2 * coefficient2[index] +
                          t * coefficient1[index] + coefficient0[index];
          } while (j < observer_velocity_sizes[parameter_index]);
        }
      }

      if (parameter_index < 4) {
        int16_t j = 0;
        if (j < observer_velocity_sizes[parameter_index]) {
          do {
            int index = (int)j;
            j++;
            parameters[index] = step[index] + parameters[index];
          } while (j < observer_velocity_sizes[parameter_index]);
        }
      } else {
        real_vector3d axis;
        float angle;

        axis.i = step[0];
        axis.j = step[1];
        axis.k = step[2];
        angle = sqrtf(axis.i * axis.i + axis.j * axis.j + axis.k * axis.k);

        if (fabsf(angle) >= (float)(double)0.0001f) {
          float inverse_angle = 1.0f / angle;
          axis.i = axis.i * inverse_angle;
          axis.j = axis.j * inverse_angle;
          axis.k = axis.k * inverse_angle;

          if (angle != 0.0f) {
            float sine = x87_fsin(angle);
            float cosine = x87_fcos(angle);
            rotate_vector3d_by_sincos(parameters, (float *)&axis, sine, cosine);
            rotate_vector3d_by_sincos(parameters + 3, (float *)&axis, sine,
                                      cosine);
          }
        }
      }
    }

    {
      int parameter_size = observer_parameter_sizes[parameter_index];
      int velocity_size = observer_velocity_sizes[parameter_index];
      command_parameters += parameter_size;
      parameters += parameter_size;
      velocities += observer_velocity_sizes[parameter_index];
      coefficient5 += velocity_size;
      coefficient4 += velocity_size;
      coefficient3 += velocity_size;
      coefficient2 += velocity_size;
      step += observer_velocity_sizes[parameter_index];
      coefficient1 += velocity_size;
      coefficient0 += velocity_size;
      timer++;
    }
  }

  {
    real_vector3d *forward = &observer->forward;
    real_vector3d *up = &observer->up;
    float error;

    error = (forward->i * forward->i + forward->j * forward->j +
             forward->k * forward->k) -
            1.0f;
    if ((*(uint32_t *)&error & 0x7f800000u) == 0x7f800000u ||
        fabs(error) >= 0.001f) {
      goto orthogonalize;
    }

    error = (up->i * up->i + up->j * up->j + up->k * up->k) - 1.0f;
    if ((*(uint32_t *)&error & 0x7f800000u) == 0x7f800000u ||
        fabs(error) >= 0.001f) {
      goto orthogonalize;
    }

    error = forward->k * up->k + forward->i * up->i + up->j * forward->j;
    if ((*(uint32_t *)&error & 0x7f800000u) == 0x7f800000u ||
        fabs(error) >= 0.001f) {
      goto orthogonalize;
    }

    return;

  orthogonalize: {
    real_vector3d left;
    float magnitude;

    /* left = forward x up */
    left.i = forward->k * up->j - up->k * forward->j;
    left.j = forward->i * up->k - forward->k * up->i;
    left.k = forward->j * up->i - forward->i * up->j;

    /* up = left x forward */
    up->i = left.k * forward->j - left.j * forward->k;
    up->j = left.i * forward->k - left.k * forward->i;
    up->k = left.j * forward->i - left.i * forward->j;

    magnitude = sqrtf(forward->i * forward->i + forward->j * forward->j +
                      forward->k * forward->k);
    if (fabsf(magnitude) >= (float)(double)0.0001f) {
      float inverse = 1.0f / magnitude;
      forward->i = inverse * forward->i;
      forward->j = inverse * forward->j;
      forward->k = inverse * forward->k;
    }

    magnitude = sqrtf(up->i * up->i + up->j * up->j + up->k * up->k);
    if (fabsf(magnitude) >= (float)(double)0.0001f) {
      float inverse = 1.0f / magnitude;
      up->i = inverse * up->i;
      up->j = inverse * up->j;
      up->k = inverse * up->k;
    }
  }
  }
}

/* Angular displacement between two forward/up frames (0x8c030): both pairs
 * must be valid perpendicular unit axes; each becomes a rotation matrix and
 * the rotation taking frame 0 to frame 1 is written to result_angular as an
 * axis scaled by its angle. */
void FUN_0008c030(float *forward1, float *result_angular, float *forward0,
                  float *up0, float *up1)
{
  real_matrix4x3 matrix0;
  float up1_k;
  real_matrix4x3 matrix1;
  float forward1_k;

  if (!valid_real_normal3d_perpendicular(forward0, up0)) {
    csprintf(
      error_string_buffer,
      "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
      "forward0", "up0", (double)forward0[0], (double)forward0[1],
      (double)forward0[2], (double)up0[0], (double)up0[1], (double)up0[2]);
    display_assert(error_string_buffer, "c:\\halo\\SOURCE\\camera\\observer.c",
                   0x382, 1);
    system_exit(-1);
  }
  forward1_k = forward1[2];
  if (!valid_real_normal3d_perpendicular(forward1, up1)) {
    up1_k = up1[2];
    csprintf(
      error_string_buffer,
      "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
      "forward1", "up1", (double)forward1[0], (double)forward1[1],
      (double)forward1_k, (double)up1[0], (double)up1[1], (double)up1_k);
    display_assert(error_string_buffer, "c:\\halo\\SOURCE\\camera\\observer.c",
                   0x383, 1);
    system_exit(-1);
  }
  matrix_from_forward_and_up((float *)&matrix0, forward0, up0);
  matrix_from_forward_and_up((float *)&matrix1, forward1, up1);
  quaternion_to_angle_and_vector((float *)&matrix0, (float *)&matrix1,
                                 result_angular);
}

/* Near-plane collision fix for the camera focus distance (0x8c150).
 * Casts collision rays from the focus position along the up and right (cross
 * product of up and forward) directions, scaled by a near-plane factor
 * proportional to focus_distance. Finds the closest obstruction among 4
 * directions (+/- up_scaled, +/- right_scaled), then runs a binary-search
 * refinement loop (10 iterations) to converge on the exact obstruction
 * boundary. Adjusts *focus_distance by blending the initial collision
 * fraction with the refined fraction. If no obstruction is found,
 * *focus_distance is simply scaled by the initial collision fraction. */
void FUN_0008c150(float *up, float *focus_distance, float near_plane_dist,
                  float *forward, float *position)
{
  location_t location;
  bool indoor_fog;
  float initial_fraction;
  float best_t;
  float test_fraction;
  float best_sign;
  real_vector3d *best_plane;
  real_point3d adjusted_pos;
  real_point3d test_point;
  real_vector3d up_scaled;
  real_vector3d right_scaled;
  float near_plane_scale;
  float dist;
  int16_t i;
  int counter;

  initial_fraction = 1.0f;

  /* Determine location and indoor fog status at the focus position */
  scenario_location_from_point(&location, position);
  indoor_fog = FUN_0018f3e0(&location, position, 0);

  /* Compute adjusted position: position - (near_plane_dist + *focus_distance) *
   * forward */
  dist = near_plane_dist + *focus_distance;
  adjusted_pos.x = -(dist * forward[0]) + position[0];
  adjusted_pos.y = -(dist * forward[1]) + position[1];
  adjusted_pos.z = -(dist * forward[2]) + position[2];

  /* Cast initial ray from position to adjusted_pos */
  FUN_0008ab90(&initial_fraction, indoor_fog, position, (float *)&adjusted_pos);

  /* Scale factor for the near-plane probe vectors */
  near_plane_scale = observer_near_plane_scale * *focus_distance;

  /* Compute scaled up vector */
  up_scaled.i = up[0] * near_plane_scale;
  up_scaled.j = up[1] * near_plane_scale;
  up_scaled.k = up[2] * near_plane_scale;

  /* Compute scaled right vector = cross(up, forward) * near_plane_scale */
  right_scaled.i =
    (up[1] * forward[2] - up[2] * forward[1]) * near_plane_scale;
  right_scaled.j =
    (up[2] * forward[0] - up[0] * forward[2]) * near_plane_scale;
  right_scaled.k =
    (up[0] * forward[1] - up[1] * forward[0]) * near_plane_scale;

  best_t = initial_fraction;
  best_plane = NULL;
  counter = 0;

  /* Sweep 4 directions: -up, -right, +up, +right */
  i = 0;
  do {
    float sign;
    int plane_idx;
    real_vector3d *plane;

    /* sign: -1 for i=0,1; +1 for i=2,3 */
    sign = (float)(int)(((i & 2) ? 2 : 0) - 1);

    /* plane: up_scaled for even counter, right_scaled for odd */
    plane_idx = counter & 1;
    plane = (plane_idx == 0) ? &up_scaled : &right_scaled;

    test_point.x = sign * plane->i + adjusted_pos.x;
    test_point.y = sign * plane->j + adjusted_pos.y;
    test_point.z = sign * plane->k + adjusted_pos.z;

    if (FUN_0008ab90(&test_fraction, indoor_fog, position, (float *)&test_point)) {
      if (test_fraction < best_t) {
        best_t = test_fraction;
        best_plane = plane;
        best_sign = sign;
      }
    }

    i = (int16_t)(i + 1);
    counter = counter + 1;
  } while (i < 4);

  if (best_plane != NULL) {
    /* Refinement loop: binary search along the best plane direction */
    float refinement_scale;
    float offset;
    float best_frac;
    float fraction;
    int iterations;
    float final_scale;
    float value;

    refinement_scale = best_sign;
    offset = 0.0f;
    fraction = initial_fraction;
    best_frac = best_t;
    iterations = 10;

    do {
      float step;
      bool hit;

      step = (refinement_scale + offset) * REAL_0_5_POOL;

      test_point.x = step * best_plane->i + adjusted_pos.x;
      test_point.y = step * best_plane->j + adjusted_pos.y;
      test_point.z = step * best_plane->k + adjusted_pos.z;

      hit = FUN_0008ab90(&test_fraction, indoor_fog, position, (float *)&test_point);

      if (hit &&
          (float)(double)0.1f > fabsf(test_fraction - best_frac)) {
        /* Converged: update the refinement boundary */
        best_frac = test_fraction;
        refinement_scale = step;
      } else {
        /* Did not converge: record the step as offset */
        offset = step;
        if (!hit) {
          fraction = 1.0f;
        } else {
          fraction = test_fraction;
        }
      }

      iterations = iterations - 1;
    } while (iterations != 0);

    /* Determine final_scale from refinement results.
     * Reference (LAB_0008c3cf in delinked observer.obj): the "test $5,ah; jp"
     * takes the branch when fraction >= best_frac, selecting
     * value = (refinement_scale >= C0); the fall-through (fraction < best_frac)
     * selects value = offset. The comparison was previously inverted (> vs <),
     * which flipped the sign of the lerp scale below and collapsed the vehicle
     * chase distance onto the focus point. */
    if (fraction < best_frac) {
      value = offset;
    } else {
      value = (float)(int)(refinement_scale >= 0.0f);
    }

    if (value == 0.0f) {
      /* Negate path */
      if (fraction < best_frac) {
        final_scale = -offset;
      } else {
        final_scale = -refinement_scale;
      }
    } else {
      if (fraction < best_frac) {
        final_scale = offset;
      } else {
        final_scale = refinement_scale;
      }
    }

    *focus_distance = (final_scale * initial_fraction +
                       (1.0f - final_scale) * best_t) *
                      *focus_distance;
    return;
  }

  /* No obstruction found: scale focus_distance by initial fraction */
  *focus_distance = initial_fraction * *focus_distance;
}

/* Displacement from the current observer parameters to the command's
 * (0x8c440): the eight linear parameters subtract directly, then the
 * forward/up pair contributes an angular displacement via FUN_0008c030.
 * Register args: result @<eax>, state @<ecx>; stack arg: the command
 * parameters. */
void FUN_0008c440(void *velocities, void *result, void *state)
{
  real *target = (real *)velocities;
  real *displacement = (real *)result;
  real *current = (real *)state;
  volatile int i;

  i = 8;
  do {
    *displacement = *target - *current;
    target++;
    current++;
    displacement++;
    i--;
  } while (i != 0);

  i = 1;
  do {
    FUN_0008c030(target, displacement, current, current + 3, target + 3);
    target += 6;
    current += 6;
    displacement += 3;
    i--;
  } while (i != 0);
}

/* Derive the final observer camera result from staged and integrated state
 * (0x8c4b0). Reads the observer's computed focus position, focus offset,
 * focus distance, forward/up vectors, and field of view. Applies focus offset
 * rotation using the XY-normalized forward vector, optionally runs near-plane
 * collision fix, computes camera position = focus - distance*forward, queries
 * the BSP for cluster location, adjusts Z for ground penetration, validates
 * all results, clamps to world bounds, and copies the final camera state
 * (position, forward, up, velocity, FOV) into the observer result area. */
void observer_update_result(int16_t local_player_index)
{
  observer_data_t *observer;
  real_vector3d *forward;
  real_vector3d *up;
  real_point3d focus_position;
  float focus_distance;
  float fov;
  float mag_xy, inv_mag;
  float fwd_n_x, fwd_n_y;
  float height_diff;
  location_t location;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = observer_data_get(local_player_index);

  focus_position.x = observer->focus_position.x;
  focus_position.y = observer->focus_position.y;
  focus_position.z = observer->focus_position.z;

  /* Clamp focus distance to [0, max_distance], or FLT_MAX if over */
  {
    float fd = observer->focus_distance;
    if (fd < REAL_ZERO_POOL) {
      focus_distance = 0.0f;
    } else if (fd > 3.4028235e38f) {
      focus_distance = 3.4028235e+38f;
    } else {
      focus_distance = fd;
    }
  }

  /* assert: valid_world_real_point3d(&focus_position) */
  if ((*(uint32_t *)&focus_position.x & 0x7f800000u) == 0x7f800000u ||
      focus_position.x < -5000.0f ||
      focus_position.x > 5000.0f ||
      (*(uint32_t *)&focus_position.y & 0x7f800000u) == 0x7f800000u ||
      focus_position.y < -5000.0f ||
      focus_position.y > 5000.0f ||
      (*(uint32_t *)&focus_position.z & 0x7f800000u) == 0x7f800000u ||
      focus_position.z < -5000.0f ||
      focus_position.z > 5000.0f) {
    display_assert("valid_world_real_point3d(&focus_position)",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x3af, 1);
    system_exit(-1);
  }

  /* Validate forward/up axes */
  forward = &observer->forward;
  up = &observer->up;
  if (!valid_real_normal3d_perpendicular((float *)forward, (float *)up)) {
    csprintf(
      error_string_buffer,
      "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
      "&observer->forward", "&observer->up", (double)forward->i,
      (double)forward->j, (double)forward->k, (double)up->i, (double)up->j,
      (double)up->k);
    display_assert(error_string_buffer, "c:\\halo\\SOURCE\\camera\\observer.c",
                   0x3b0, 1);
    system_exit(-1);
  }

  /* assert: valid_world_real_point3d(&observer->focus_offset) */
  if ((*(uint32_t *)&observer->focus_offset.i & 0x7f800000u) == 0x7f800000u ||
      observer->focus_offset.i < -5000.0f ||
      observer->focus_offset.i > REAL_5000_POOL ||
      (*(uint32_t *)&observer->focus_offset.j & 0x7f800000u) == 0x7f800000u ||
      observer->focus_offset.j < -5000.0f ||
      observer->focus_offset.j > 5000.0f ||
      (*(uint32_t *)&observer->focus_offset.k & 0x7f800000u) == 0x7f800000u ||
      observer->focus_offset.k < -5000.0f ||
      observer->focus_offset.k > 5000.0f) {
    display_assert(
      "valid_world_real_point3d((real_point3d *) &observer->focus_offset)",
      "c:\\halo\\SOURCE\\camera\\observer.c", 0x3b1, 1);
    system_exit(-1);
  }

  /* assert: valid_focus_distance(focus_distance) */
  if ((*(uint32_t *)&focus_distance & 0x7f800000u) == 0x7f800000u ||
      focus_distance < 0.0f || focus_distance > 5000.0f) {
    display_assert("valid_focus_distance(focus_distance)",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x3b2, 1);
    system_exit(-1);
  }

  /* Clamp field_of_view to [fov_min, fov_max] */
  {
    float f = observer->field_of_view;
    if (f < 0.001f) {
      fov = 0.001f;
    } else if (f > 1.57079637f) {
      fov = REAL_HALF_PI_POOL;
    } else {
      fov = f;
    }
    observer->field_of_view = fov;
  }

  /* Clamp focus_position components to [-5000, 5000] */
  if (focus_position.x < -5000.0f) {
    focus_position.x = -5000.0f;
  } else if (focus_position.x > 5000.0f) {
    focus_position.x = 5000.0f;
  }

  if (focus_position.y < -5000.0f) {
    focus_position.y = -5000.0f;
  } else if (focus_position.y > 5000.0f) {
    focus_position.y = 5000.0f;
  }

  if (focus_position.z < -5000.0f) {
    focus_position.z = -5000.0f;
  } else if (focus_position.z > 5000.0f) {
    focus_position.z = 5000.0f;
  }

  /* Clamp focus_distance to [0, 5000] */
  if (focus_distance < 0.0f) {
    focus_distance = 0.0f;
  } else if (focus_distance > 5000.0f) {
    focus_distance = 5000.0f;
  }

  /* Normalize forward vector in XY plane */
  fwd_n_x = forward->i;
  fwd_n_y = forward->j;
  mag_xy = sqrtf(fwd_n_x * fwd_n_x + fwd_n_y * fwd_n_y);
  if (fabsf(mag_xy) >= (float)(double)0.0001f) {
    inv_mag = 1.0f / mag_xy;
    fwd_n_x = inv_mag * fwd_n_x;
    fwd_n_y = inv_mag * fwd_n_y;
  }

  /* Apply rotated focus_offset to focus_position using normalized forward */
  focus_position.x = fwd_n_x * observer->focus_offset.i +
                      fwd_n_y * observer->focus_offset.j + focus_position.x;
  focus_position.y = (fwd_n_y * observer->focus_offset.i -
                       fwd_n_x * observer->focus_offset.j) +
                      focus_position.y;
  focus_position.z = focus_position.z + observer->focus_offset.k;

  /* Near-plane collision fix (skip if mode bit 0x10 set or focus_distance == 0)
   */
  if ((observer->last_command.flags &
       FLAG(_observer_command_ignore_obstructions_bit)) == 0 &&
      focus_distance != 0.0f) {
    FUN_0008c150((float *)up, &focus_distance, 0.02f, (float *)forward,
                 (float *)&focus_position);
  }

  /* Compute result.position = focus_position - focus_distance * forward */
  observer->result.position.x = focus_position.x - focus_distance * forward->i;
  observer->result.position.y = focus_position.y - focus_distance * forward->j;
  observer->result.position.z = focus_position.z - focus_distance * forward->k;

  /* Determine BSP cluster location for the camera position */
  scenario_location_from_point(&location, &observer->result.position);

  /* If cluster changed, precache resources for the new cluster */
  {
    int16_t cluster = location.cluster_index;
    if (cluster != -1) {
      if (cluster != *(int16_t *)&observer->result.location.cluster_index) {
        structure_bsp_cluster_t *element =
          (structure_bsp_cluster_t *)tag_block_get_element(
            &((structure_bsp_t *)scenario_get())->clusters, (int)cluster,
            sizeof(structure_bsp_cluster_t));
        /* +0x28: the cluster's predicted-resources tag block (types.h keeps
         * it inside pad_10). */
        predicted_resources_precache((char *)element + 0x28);
      }
      observer->result.location = location;
    }
  }

  /* Ground height adjustment */
  {
    float h = FUN_0018f510(&observer->result.location, &observer->result.position);
    height_diff = h;
    if (fabsf(height_diff) < (float)(double)0.05f) {
      if (height_diff <= 0.0f) {
        observer->result.position.z =
          height_diff + observer->result.position.z + 0.05f;
      } else {
        observer->result.position.z =
          observer->result.position.z - (0.05f - height_diff);
      }
    }
  }

  /* assert: valid_world_real_point3d(&observer->result.position) */
  if ((*(uint32_t *)&observer->result.position.x & 0x7f800000u) == 0x7f800000u ||
      observer->result.position.x < -5000.0f ||
      observer->result.position.x > 5000.0f ||
      (*(uint32_t *)&observer->result.position.y & 0x7f800000u) == 0x7f800000u ||
      observer->result.position.y < -5000.0f ||
      observer->result.position.y > 5000.0f ||
      (*(uint32_t *)&observer->result.position.z & 0x7f800000u) == 0x7f800000u ||
      observer->result.position.z < -5000.0f ||
      observer->result.position.z > 5000.0f) {
    display_assert("valid_world_real_point3d(&observer->result.position)",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x41f, 1);
    system_exit(-1);
  }

  /* Validate forward/up axes again */
  if (!valid_real_normal3d_perpendicular((float *)forward, (float *)up)) {
    csprintf(
      error_string_buffer,
      "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
      "&observer->forward", "&observer->up", (double)forward->i,
      (double)forward->j, (double)forward->k, (double)up->i, (double)up->j,
      (double)up->k);
    display_assert(error_string_buffer, "c:\\halo\\SOURCE\\camera\\observer.c",
                   0x420, 1);
    system_exit(-1);
  }

  /* assert: valid_field_of_view(observer->field_of_view) */
  if ((*(uint32_t *)&observer->field_of_view & 0x7f800000u) == 0x7f800000u ||
      observer->field_of_view < 0.001f ||
      observer->field_of_view > 1.57079637f) {
    display_assert("valid_field_of_view(observer->field_of_view)",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x421, 1);
    system_exit(-1);
  }

  /* Clamp result.position to world bounds */
  {
    float v;
    v = observer->result.position.x;
    if (v < -5000.0f) {
      v = -5000.0f;
    } else if (v > 5000.0f) {
      v = REAL_5000_POOL;
    }
    observer->result.position.x = v;

    v = observer->result.position.y;
    if (v < -5000.0f) {
      v = -5000.0f;
    } else if (v > 5000.0f) {
      v = REAL_5000_POOL;
    }
    observer->result.position.y = v;

    v = observer->result.position.z;
    if (v < -5000.0f) {
      v = -5000.0f;
    } else if (v > 5000.0f) {
      v = 5000.0f;
    }
    observer->result.position.z = v;
  }

  /* Copy forward -> result.forward, negate velocities -> result.velocity,
   * copy up -> result.up, copy field_of_view -> result.field_of_view */
  observer->result.forward.i = forward->i;
  observer->result.velocity.i = -observer->velocities.n[0];
  observer->result.forward.j = forward->j;
  observer->result.forward.k = forward->k;
  observer->result.velocity.j = -observer->velocities.n[1];
  observer->result.velocity.k = -observer->velocities.n[2];
  observer->result.up.i = up->i;
  observer->result.up.j = up->j;
  observer->result.field_of_view = observer->field_of_view;
  observer->result.up.k = up->k;
}

/* Refresh the observer's displacements from its parameters to the last
 * command's (0x8ccf0). */
void observer_compute_velocities(int16_t local_player_index)
{
  observer_data_t *observer;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = observer_data_get(local_player_index);
  FUN_0008c440(&observer->last_command.position, observer->displacements.n,
               &observer->focus_position);
}

/* Run one tick of the observer's parameter integration (0x8cd40) unless the
 * pending command freezes the camera, then count the five parameter timers
 * down by dtime, stopping at zero. */
void observer_update_positions(int16_t local_player_index)
{
  int i;
  observer_data_t *observer;
  real *timer;
  float remaining;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = observer_data_get(local_player_index);
  timer = observer->last_command.field_54;

  if ((observer->pending_command->flags &
       FLAG(_observer_command_freeze_camera_bit)) == 0) {
    observer_compute_velocities(local_player_index);
    observer_compute_accelerations(local_player_index);
    observer_apply_acceleration(local_player_index);
    observer_integrate(local_player_index);
    observer_compute_update(local_player_index);

    for (i = NUMBER_OF_OBSERVER_COMMAND_PARAMETERS; i != 0; i--) {
      remaining = *timer - observer_globals.dtime;
      if (!(remaining > 0.0f)) {
        remaining = 0.0f;
      }
      *timer = remaining;
      timer++;
    }
  }
}

/* Per-tick observer update for all local players (0x8cde0).
 * Saves the frame's delta time, then for each active local player's observer
 * checks both signatures and that updated_for_frame is clear, marks it set,
 * and dispatches three observer sub-updates:
 *   - observer_update_command (0x8b060): copies/stages camera block from
 *     director into observer state
 *   - observer_update_positions (0x8cd40): time-dependent integration,
 *     skipped when delta_time is zero
 *   - observer_update_result (0x8c4b0): derives the final observer camera
 *     result from staged and integrated state */
void observer_update(float delta_time)
{
  int16_t i;
  observer_t *observer = observer_globals.local_players;

  observer_globals.dtime = delta_time;

  for (i = 0; i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; i++, observer++) {
    if (local_player_get_player_index(i) == -1)
      continue;

    if (i < 0 || i >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
      display_assert("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72, 1);
      system_exit(-1);
    }

    if (observer->header_signature != OBSERVER_SIGNATURE ||
        observer->trailer_signature != OBSERVER_SIGNATURE) {
      display_assert("observer->header_signature==OBSERVER_SIGNATURE && "
                     "observer->trailer_signature==OBSERVER_SIGNATURE",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x108, 1);
      system_exit(-1);
    }

    if (observer->updated_for_frame) {
      display_assert("!observer->updated_for_frame",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x109, 1);
      system_exit(-1);
    }

    observer->updated_for_frame = 1;

    observer_update_command(i);

    if (observer_globals.dtime != 0.0f) {
      observer_update_positions(i);
    }

    observer_update_result(i);

    if (observer->header_signature != OBSERVER_SIGNATURE ||
        observer->trailer_signature != OBSERVER_SIGNATURE) {
      display_assert("observer->header_signature==OBSERVER_SIGNATURE && "
                     "observer->trailer_signature==OBSERVER_SIGNATURE",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x117, 1);
      system_exit(-1);
    }
  }
}

/* Initialize an orbiting camera (0x8cf10): the distance is stored before
 * vector_to_angles fills the facing pair. */
void orbiting_camera_new(float *orbiting_camera, float distance, float *vector)
{
  orbiting_camera_t *camera = (orbiting_camera_t *)orbiting_camera;

  camera->distance = distance;
  vector_to_angles((float *)&camera->facing, vector);
}

/* orbiting_camera_update (0x8cf30): replay mode's fourth cycle entry. */
void orbiting_camera_update(void *camera_data, void *control_data,
                            void *result_data)
{
  orbiting_camera_t *camera;
  camera_action_t *controls;
  camera_command_t *result;
  player_control_unit_camera_info_t info;

  camera = (orbiting_camera_t *)camera_data;
  controls = (camera_action_t *)control_data;
  result = (camera_command_t *)result_data;
  player_control_get_unit_camera_info(controls->local_player_index, &info);
  result->position = info.position;
  if (controls->active) {
    camera->facing.yaw -= controls->facing_delta.yaw;
    camera->facing.pitch =
      camera->facing.pitch - controls->facing_delta.pitch < -1.2566371f ?
        -1.2566371f :
        (camera->facing.pitch - controls->facing_delta.pitch > 1.2566371f ?
           1.2566371f :
           camera->facing.pitch - controls->facing_delta.pitch);
    director_set_local_player_context(controls->local_player_index);
  }
  camera->distance =
    camera->distance - controls->wheel_delta * 0.333333343f > 0.6f ?
      camera->distance - controls->wheel_delta * 0.333333343f :
      0.6f;
  if (info.unit_index != NONE) {
    angles_to_vector((float *)&result->forward, (float *)&camera->facing);
    observer_up_from_forward((float *)&result->forward, (float *)&result->up);
    object_get_root_location(info.unit_index, (float *)&result->velocity, NULL);
    result->position.z += orbiting_camera_height_offset;
    result->flags = FLAG(_observer_command_valid_bit);
  }
  result->offset = *(real_vector3d *)global_zero_vector_ptr;
  result->depth = camera->distance;
  result->field_of_view = orbiting_camera_field_of_view;
  result->timer = orbiting_camera_timer;
  if (!(!(result->flags & FLAG(_observer_command_valid_bit)) ||
        (valid_real_normal3d_perpendicular((float *)&result->forward,
                                           (float *)&result->up) &&
         valid_world_real(result->position.x) &&
         valid_world_real(result->position.y) &&
         valid_world_real(result->position.z) &&
         valid_world_real(result->offset.i) &&
         valid_world_real(result->offset.j) &&
         valid_world_real(result->offset.k) &&
         real_vector3d_valid((float *)&result->velocity) &&
         valid_real(result->depth) && result->depth >= 0.0f &&
         result->depth <= 5000.0f && valid_real(orbiting_camera_field_of_view) &&
         orbiting_camera_field_of_view >= 0.001f &&
         orbiting_camera_field_of_view <= 1.57079637f /* pi/2 */ &&
         valid_real(orbiting_camera_timer) && orbiting_camera_timer >= 0.0f &&
         orbiting_camera_timer <= 3600.0f))) {
    display_assert(
      csprintf(error_string_buffer, CAMERA_COMMAND_INVALID_FORMAT,
               result->forward.i, result->forward.j, result->forward.k,
               result->up.i, result->up.j, result->up.k, result->position.x,
               result->position.y, result->position.z, result->offset.i,
               result->offset.j, result->offset.k, result->depth,
               result->velocity.i, result->velocity.j, result->velocity.k,
               orbiting_camera_field_of_view, orbiting_camera_timer, result->flags),
      "c:\\halo\\SOURCE\\camera\\orbiting_camera.c", 0x47, 1);
    system_exit(-1);
  }
}

/* Fill in a scripted fixed-position camera (0x8d3a0); static_camera_update
 * (0x8d410) turns it into a camera command once and then sets initialized.
 * location is stored but never read by the update. */
void static_camera_new(void *camera, const vector3_t *position,
                       uint32_t location, const vector3_t *forward,
                       const vector3_t *up, float field_of_view,
                       int32_t duration_ticks, uint32_t flags)
{
  static_camera_t *static_camera = (static_camera_t *)camera;

  static_camera->position = *(const real_point3d *)position;
  static_camera->location = location;
  static_camera->forward = *(const real_vector3d *)forward;
  static_camera->up = *(const real_vector3d *)up;
  static_camera->field_of_view = field_of_view;
  static_camera->timer = duration_ticks;
  static_camera->initialized = false;
  static_camera->flags = flags;
}
