#include "camera_internal.h"
#include "x87_math.h"

/* Camera observer — tracks camera position/orientation per player. */

#line 177
/* Reset a flying-camera state block to defaults (0x89330). Zeroes the two
 * leading dwords and the three dwords at +0xc..+0x14, then stores the default
 * field of view (0x3f9c61aa == 1.22173047f, 70 degrees) at +0x18. The dword at
 * +0x8 is deliberately left untouched by the original. */
void flying_camera_new(void *flying_camera)
{
  char *camera = (char *)flying_camera;

  *(int *)(camera + 0x4) = 0;
  *(int *)(camera + 0x0) = 0;
  *(int *)(camera + 0xc) = 0;
  *(int *)(camera + 0x10) = 0;
  *(int *)(camera + 0x14) = 0;
  *(float *)(camera + 0x18) = 1.22173047f;
}

/* Initialize flying camera data from a position and forward vector (0x89350).
 * The position occupies +0x0..+0x8; vector_to_angles writes the angle pair at
 * +0xc. The zeros at +0x10..+0x14 and default field of view at +0x18 are
 * preserved from the binary store order. */
void FUN_00089350(void *camera_data, float *position, float *forward)
{
  char *camera = (char *)camera_data;

  *(int *)(camera + 0x4) = 0;
  *(int *)(camera + 0x0) = 0;
  *(int *)(camera + 0x10) = 0;
  *(int *)(camera + 0x14) = 0;
  *(int *)(camera + 0xc) = 0;
  *(float *)(camera + 0x18) = 1.22173047f;
  *(int *)(camera + 0x0) = *(int *)(position + 0);
  *(int *)(camera + 0x4) = *(int *)(position + 1);
  *(int *)(camera + 0x8) = *(int *)(position + 2);
  vector_to_angles((float *)(camera + 0xc), forward);
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
  if (*(int16_t *)0x325716 > 0) {
    camera->facing.yaw = 0.0f;
    camera->facing.pitch = 0.0f;
    camera->roll = 0.0f;
    --*(int16_t *)0x325716;
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
  result->offset = **(real_vector3d **)0x31fc38;
  result->depth = 0.0f;
  result->field_of_view = camera->field_of_view;
  result->flags = 1;
  if ((result->flags & 1) &&
      (!valid_real_normal3d_perpendicular((float *)&result->forward,
                                          (float *)&result->up) ||
       (*(uint32_t *)&result->position.x & 0x7f800000) == 0x7f800000 ||
       !(result->position.x >= *(float *)0x266e98) ||
       !(result->position.x <= *(float *)0x266e94) ||
       (*(uint32_t *)&result->position.y & 0x7f800000) == 0x7f800000 ||
       !(result->position.y >= *(float *)0x266e98) ||
       !(result->position.y <= *(float *)0x266e94) ||
       (*(uint32_t *)&result->position.z & 0x7f800000) == 0x7f800000 ||
       !(result->position.z >= *(float *)0x266e98) ||
       !(result->position.z <= *(float *)0x266e94) ||
       (*(uint32_t *)&result->offset.i & 0x7f800000) == 0x7f800000 ||
       !(result->offset.i >= *(float *)0x266e98) ||
       !(result->offset.i <= *(float *)0x266e94) ||
       (*(uint32_t *)&result->offset.j & 0x7f800000) == 0x7f800000 ||
       !(result->offset.j >= *(float *)0x266e98) ||
       !(result->offset.j <= *(float *)0x266e94) ||
       (*(uint32_t *)&result->offset.k & 0x7f800000) == 0x7f800000 ||
       !(result->offset.k >= *(float *)0x266e98) ||
       !(result->offset.k <= *(float *)0x266e94) ||
       !real_vector3d_valid((float *)&result->velocity) ||
       (*(uint32_t *)&result->depth & 0x7f800000) == 0x7f800000 ||
       !(result->depth >= *(float *)0x2533c0) ||
       !(result->depth <= *(float *)0x266e94) ||
       (*(uint32_t *)&result->field_of_view & 0x7f800000) == 0x7f800000 ||
       !(result->field_of_view >= *(float *)0x255ef8) ||
       !(result->field_of_view <= *(float *)0x2568bc) ||
       (*(uint32_t *)&result->timer & 0x7f800000) == 0x7f800000 ||
       !(result->timer >= *(float *)0x2533c0) ||
       !(result->timer <= *(float *)0x266e90))) {
    char *msg = csprintf((char *)0x5ab100,
                         "Invalid camera command.\n"
                         "F: (%f, %f, %f) U: (%f, %f, %f)\n"
                         "P: (%f, %f, %f) O: (%f, %f, %f)\n"
                         "D: %f V: (%f, %f, %f), FOV: %f, T: %f, FL: %ld",
                         (double)result->forward.i, (double)result->forward.j,
                         (double)result->forward.k, (double)result->up.i,
                         (double)result->up.j, (double)result->up.k,
                         (double)result->position.x, (double)result->position.y,
                         (double)result->position.z, (double)result->offset.i,
                         (double)result->offset.j, (double)result->offset.k,
                         (double)result->depth, (double)result->velocity.i,
                         (double)result->velocity.j, (double)result->velocity.k,
                         (double)result->field_of_view, (double)result->timer,
                         result->flags);
    display_assert(msg, "c:\\halo\\SOURCE\\camera\\flying_camera.c", 0x95, 1);
    system_exit(-1);
  }
}

/* Initialize following-camera state (0x89850). The independently accessed
 * fields remain mechanical because the state layout has not been recovered. */
void following_camera_new(void *camera_data)
{
  char *camera;

  if (camera_data == NULL) {
    display_assert("camera", "c:\\halo\\SOURCE\\camera\\following_camera.c",
                   0x13, 1);
    system_exit(-1);
  }

  camera = (char *)camera_data;
  camera[0] = 0;
  camera[1] = 0;
  camera[2] = 0;
  camera[3] = 0;
  *(int16_t *)(camera + 0x4) = 0;
  *(int *)(camera + 0x14) = 0;
  *(int *)(camera + 0x10) = 0;
  *(int *)(camera + 0x8) = -1;
  *(int16_t *)(camera + 0xc) = -1;
  *(float *)(camera + 0x18) = 1.0f;
}

/* Resolve the active camera tag data for a unit (0x898b0). The unit handle
 * arrives in EAX and the result is returned in EAX. A vehicle-associated unit
 * returns seat element + 0x84 when the selected seat flags include any of
 * 0x15; otherwise it returns unit definition + 0x1a8. The +0xcc and +0x2a0
 * unit offsets remain mechanical. */
void *unit_camera_get(int unit_handle /* @<eax> */)
{
  uint8_t *tag_element;
  void *unit;
  void *vehicle;
  void *tag_data;

  unit = object_get_and_verify_type(unit_handle, 3);
  if (*(int *)((char *)unit + 0xcc) != -1) {
    vehicle =
      object_try_and_get_and_verify_type(*(int *)((char *)unit + 0xcc), 2);
    if (vehicle != NULL) {
      tag_data = tag_get(0x76656869, *(int *)vehicle);
      tag_element = (uint8_t *)tag_block_get_element(
        (char *)tag_data + 0x2e4, (int)*(int16_t *)((char *)unit + 0x2a0),
        0x11c);
      if ((tag_element[0] & 0x15) != 0) {
        tag_element = (uint8_t *)((uintptr_t)tag_element + 0x84);
        if (tag_element != NULL) {
          return tag_element;
        }
      }
    }
  }
  return (char *)tag_get(0x756e6974, *(int *)unit) + 0x1a8;
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
                     h > *(float *)0x2533c0);
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
  char *block;
  char *points;
  void *element;
  void *tag_data;
  int tag_index;
  int index;
  int count;
  int count_minus_1;
  short start;
  short i;
  float step;

  block = (char *)definition + 0x4c;
  if (*(int *)block != 0) {
    index = *(int *)block - 1;
    index &= (index >= 0) - 1;
    element = tag_block_get_element(block, index, 0x1c);
    if (element != NULL) {
      tag_index = *(int *)((char *)element + 0xc);
      if (tag_index != -1) {
        goto have_track;
      }
    }
  }
  element = tag_block_get_element((char *)game_globals_get() + 0x104, 0, 0x10);
  tag_index = *(int *)((char *)element + 0xc);

have_track:
  tag_data = tag_get(0x7472616b, tag_index);
  points = (char *)tag_data + 4;
  count = *(int *)points;
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
      if ((int)i + 4 <= *(int *)points && (int)i <= (int)start - 1) {
        break;
      }
      i--;
    } while (i > 0);
  }

  index = (int)i;
  FUN_00089a20(
    out, (float *)tag_block_get_element(points, index, 0x3c),
    (float *)tag_block_get_element(points, index + 1, 0x3c),
    (float *)tag_block_get_element(points, index + 2, 0x3c),
    (float *)tag_block_get_element((char *)tag_data + 4, index + 3, 0x3c),
    (float)index * step, step, t);
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
  char *unit;

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
    unit = (char *)object_get_and_verify_type(info.unit_index, 3);
    /* 0x89d9d loads only the low byte of unit control flags at +0x1b8. */
    crouched =
      (*(uint8_t *)(unit + 0x1b8) & 1) || (*(uint8_t *)(unit + 0x1b8) & 2);
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
    result->flags |= 1;
  }
  observer_up_from_forward((float *)&result->forward, (float *)&result->up);
  if (!camera_command_valid(result)) {
    display_assert(csprintf(error_string_buffer, CAMERA_COMMAND_INVALID_FORMAT,
                            CAMERA_COMMAND_INVALID_ARGUMENTS(result)),
                   "c:\\halo\\SOURCE\\camera\\following_camera.c", 0xee, 1);
    system_exit(-1);
  }
  camera->initialized = 1;
}

void observer_initialize(void)
{
}

/* Initialize an observer result struct with default camera orientation,
 * zero velocities, and signature markers (0x8a350). Sets the camera up/forward
 * vectors from globals, zeros the integration working area, then copies the
 * template vectors into the active camera state. */
void observer_result_initialize(void *observer)
{
  char *obs = (char *)observer;
  float *up = *(float **)0x31fc3c;
  float *fwd = *(float **)0x31fc44;
  float *pos;

  *(float *)(obs + 0xd0) = up[0];
  *(float *)(obs + 0xd4) = up[1];
  *(float *)(obs + 0xd8) = up[2];
  *(float *)(obs + 0xdc) = fwd[0];
  *(float *)(obs + 0xe0) = fwd[1];
  *(float *)(obs + 0xe4) = fwd[2];
  *(int *)(obs + 0xcc) = 0x3f5f66f3;

  pos = *(float **)0x31fc1c;
  *(float *)(obs + 0x74) = pos[0];
  *(float *)(obs + 0x78) = pos[1];
  *(float *)(obs + 0x7c) = pos[2];

  *(int16_t *)(obs + 0x84) = -1;
  *(int *)(obs + 0x80) = -1;

  {
    float *zero = *(float **)0x31fc38;
    *(float *)(obs + 0x88) = zero[0];
    *(float *)(obs + 0x8c) = zero[1];
    *(float *)(obs + 0x90) = zero[2];
  }

  up = *(float **)0x31fc3c;
  *(float *)(obs + 0x94) = up[0];
  *(float *)(obs + 0x98) = up[1];
  *(float *)(obs + 0x9c) = up[2];

  fwd = *(float **)0x31fc44;
  *(float *)(obs + 0xa0) = fwd[0];
  *(float *)(obs + 0xa4) = fwd[1];
  *(float *)(obs + 0xa8) = fwd[2];
  *(int *)(obs + 0xac) = 0x3f5f66f3;

  csmemset(obs + 0x8, 0, 0x68);

  *(float *)(obs + 0x2c) = *(float *)(obs + 0xd0);
  *(float *)(obs + 0x30) = *(float *)(obs + 0xd4);
  *(float *)(obs + 0x34) = *(float *)(obs + 0xd8);
  *(float *)(obs + 0x38) = *(float *)(obs + 0xdc);
  *(float *)(obs + 0x3c) = *(float *)(obs + 0xe0);
  *(float *)(obs + 0x40) = *(float *)(obs + 0xe4);
  *(int *)(obs + 0x28) = *(int *)(obs + 0xcc);

  *(int *)(obs + 0x298) = 0x72616421;
  *(int *)(obs + 0x0) = 0x72616421;
  *(uint8_t *)(obs + 0x70) = 1;
  *(uint8_t *)(obs + 0x71) = 0;
}

/* Initialize observers for all 4 players. Calls observer_result_initialize
 * with ESI pointing to each player's observer data (base 0x33571c,
 * stride 0x29c). */
void observer_initialize_for_new_map(void)
{
  int16_t i;
  char *entry = (char *)0x33571c;

  for (i = 0; i < 4; i++) {
    assert_halt(i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
    observer_result_initialize(entry);
    entry += 0x29c;
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
  char *entry;

  if (idx == -1)
    return 0;

  assert_halt(((idx >= 0) & 0xFFFFu) && idx < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  entry = (char *)0x33571c + (int)idx * 0x29c;

  if (*(int16_t *)(entry + 0x84) < -1 ||
      (int)*(int16_t *)(entry + 0x84) >=
        *(int *)((char *)scenario_get() + 0x134)) {
    display_assert("observer->result.location.cluster_index>=NONE && "
                   "observer->result.location.cluster_index<"
                   "global_structure_bsp_get()->clusters.count",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x12d, 1);
    system_exit(-1);
  }

  return (void *)(entry + 0x74);
}

/* Return true when the observer has no active command timer. */
boolean observer_command_has_finished(int16_t local_player_index)
{
  char *observer;
  int16_t component_index;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;
  if (*(float *)(observer + 0x50) != 0.0f)
    return 0;

  for (component_index = 0; component_index < 5; component_index++) {
    if (*(float *)(observer + 0x5c + (int)component_index * 4) != 0.0f)
      return 0;
  }

  return 1;
}

void observer_reconnect_to_structure_bsp(void)
{
  int16_t local_player_index;
  char *observer;

  local_player_index = 0;
  observer = (char *)0x33579c;
  do {
    if (local_player_get_player_index(local_player_index) != -1) {
      if (local_player_index < 0 ||
          local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
        display_assert("local_player_index>=0 && "
                       "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                       "c:\\halo\\SOURCE\\camera\\observer.c", 0x72, 1);
        system_exit(-1);
      }
      scenario_location_from_point(observer, observer - 0xc);
    }
    local_player_index++;
    observer += 0x29c;
  } while (local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
}

/* Apply spring acceleration to observer state (0x8a660).
 * For each of 5 observer components, evaluates a cubic polynomial
 * (2*vel + t*accel*K1 + t^2*jerk*K2 + t^3*snap*K3) using the component's
 * timer. If any element exceeds its threshold, resets the timer (and any
 * other timers sharing the same value) to zero. */
void observer_apply_acceleration(int16_t local_player_index)
{
  char *observer;
  float *snap_ptr, *jerk_ptr, *accel_ptr, *vel_ptr, *output;
  float *timers, *timers_base;
  int16_t *sizes;
  float *thresholds;
  int16_t comp;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;
  snap_ptr = (float *)(observer + 0x158);
  jerk_ptr = (float *)(observer + 0x184);
  accel_ptr = (float *)(observer + 0x1b0);
  vel_ptr = (float *)(observer + 0x1dc);
  output = (float *)(observer + 0x120);
  timers_base = (float *)(observer + 0x5c);
  timers = timers_base;
  sizes = (int16_t *)0x2ee6b8;
  thresholds = (float *)0x26738c;

  for (comp = 0; comp < 5; comp++) {
    float t = *timers - *(float *)0x335718;

    if (t <= 0.0f) {
      csmemset(output, 0, (int)*sizes << 2);
    } else {
      float t_sq = t * t;
      float t_cu = t_sq * t;
      int16_t j;

      for (j = 0; j < *sizes; j++) {
        float result = t_cu * snap_ptr[j] * *(float *)0x254cd0 +
                       t_sq * jerk_ptr[j] * *(float *)0x254cc8 +
                       t * accel_ptr[j] * *(float *)0x254640 + vel_ptr[j] +
                       vel_ptr[j];
        output[j] = result;

        if (result > *thresholds || result < -*thresholds) {
          int16_t k;
          float *tp = timers_base;
          for (k = 0; k < 5; k++) {
            if (k != comp && *tp == *timers)
              *tp = 0.0f;
            tp++;
          }
          *timers = 0.0f;
        }
      }
    }

    snap_ptr += *sizes;
    jerk_ptr += *sizes;
    output += *sizes;
    accel_ptr += *sizes;
    vel_ptr += *sizes;
    timers++;
    thresholds++;
    sizes++;
  }
}

/* Integrate observer spring state (0x8a830). For each of 5 components,
 * evaluates a quartic polynomial (pos + 2*t*vel + t^2*accel*K1 + t^3*jerk*K2
 * + t^4*snap*K3) when the timer is active. When expired, either zeros the
 * output or applies a negated ratio correction from the result buffer. */
void observer_integrate(int16_t local_player_index)
{
  char *observer;
  float *result_ptr, *snap_ptr, *jerk_ptr, *accel_ptr, *vel_ptr, *pos_ptr;
  float *output, *timers;
  uint8_t *byte_flags;
  int16_t *sizes;
  float ratio;
  int count;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  ratio = (float)(*(double *)0x2573d8 / *(float *)0x335718);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;
  result_ptr = (float *)(observer + 0x260);
  snap_ptr = (float *)(observer + 0x158);
  jerk_ptr = (float *)(observer + 0x184);
  accel_ptr = (float *)(observer + 0x1b0);
  vel_ptr = (float *)(observer + 0x1dc);
  pos_ptr = (float *)(observer + 0x208);
  timers = (float *)(observer + 0x5c);
  byte_flags = (uint8_t *)(observer + 0x54);
  output = (float *)(observer + 0xe8);
  sizes = (int16_t *)0x2ee6b8;

  for (count = 5; count != 0; count--) {
    float t = *timers - *(float *)0x335718;
    int16_t size = *sizes;

    if (t > 0.0f) {
      float t_sq = t * t;
      float t_cu = t_sq * t;
      float t_q4 = t_cu * t;
      int16_t i;

      for (i = 0; i < size; i++) {
        float v = t * vel_ptr[i];
        output[i] = v + v + t_sq * accel_ptr[i] * *(float *)0x254644 +
                    t_cu * jerk_ptr[i] * *(float *)0x2533d8 +
                    t_q4 * snap_ptr[i] * *(float *)0x254cc4 + pos_ptr[i];
      }
    } else {
      uint32_t mode = *(uint32_t *)(observer + 0x8);
      if ((mode & 1) && ((*byte_flags & 2) || (mode & 8))) {
        csmemset(output, 0, (int)size << 2);
      } else if ((mode & 1) && size > 0) {
        int16_t i;
        for (i = 0; i < size; i++)
          output[i] = -(ratio * result_ptr[i]);
      }
    }

    result_ptr += size;
    snap_ptr += size;
    jerk_ptr += size;
    accel_ptr += size;
    vel_ptr += size;
    output += size;
    pos_ptr += size;
    timers++;
    byte_flags++;
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
  observer_result_initialize((char *)0x33571c + local_player_index * 0x29c);
}

/* Derive a camera up vector from a forward vector (0x8aa80), cdecl, 2 args.
 * right = (forward.y, -forward.x, 0) is normalized with normalize3d; if its
 * length is 0.0f (forward is vertical) right falls back to (1, 0, 0).
 * up = right x forward. */
void observer_up_from_forward(float *forward, float *up)
{
  float right[3];
  float i;
  float j;
  float k;

  /* The cross product is the inlined
   * cross_product3d(&right, forward, up), which computes k, j, i into
   * temporaries before storing i, j, k. */
  right[0] = forward[1];
  right[1] = -forward[0];
  right[2] = 0.0f;
  if (normalize3d(right) == 0.0f) {
    right[0] = 1.0f;
    right[1] = right[2] = 0.0f;
  }
  k = right[0] * forward[1] - right[1] * forward[0];
  j = right[2] * forward[0] - right[0] * forward[2];
  i = right[1] * forward[2] - right[2] * forward[1];
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
  float axis[3];
  float angle;
  float sin_angle;
  float cos_angle;

  axis[0] = rotation[0];
  axis[1] = rotation[1];
  axis[2] = rotation[2];

  angle = normalize3d(axis);
  if (angle != REAL_ZERO_POOL) {
    sin_angle = x87_fsin(angle);
    cos_angle = x87_fcos(angle);
    rotate_vector3d_by_sincos(vector_a, axis, sin_angle, cos_angle);
    rotate_vector3d_by_sincos(vector_b, axis, sin_angle, cos_angle);
  }
}

/* Cast a collision ray for the observer camera (0x8ab90).
 * Pushes a collision user tag (0xc = observer), computes direction from
 * ray_origin to ray_endpoint, fires FUN_0014df70, and writes the hit
 * fraction to *out_fraction if a collision is found. */
bool FUN_0008ab90(float *out_fraction, bool indoor, float *ray_origin,
                  float *ray_endpoint)
{
  uint32_t flags;
  bool result;
  float direction[3];
  char collision_result[0x60];

  result = false;
  flags = 0x40e1;
  if (indoor)
    flags = 0x40a1;

  if (*(int16_t *)0x4761d8 >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x4b4, 1);
    system_exit(-1);
  }

  {
    int depth = (int)*(int16_t *)0x4761d8;
    *(int16_t *)(0x5a8c80 + depth * 2) = 0xc;
    *(int16_t *)0x4761d8 += 1;
  }

  direction[0] = ray_endpoint[0] - ray_origin[0];
  direction[1] = ray_endpoint[1] - ray_origin[1];
  direction[2] = ray_endpoint[2] - ray_origin[2];

  if (FUN_0014df70(flags, ray_origin, direction, -1,
                   (int16_t *)collision_result)) {
    *(int *)out_fraction = *(int *)(collision_result + 0x14);
    result = true;
  }

  if (*(int16_t *)0x4761d8 <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x4ba, 1);
    system_exit(-1);
  }
  *(int16_t *)0x4761d8 -= 1;

  return result;
}

/* 0x8ac70: vector @<eax>, point @<ecx>, ray_origin @<esi>; the callee's AL
 * result is left in EAX. No direct callers in the binary. */
bool observer_collision_test_differential(float scale, float *out_fraction,
                                          bool indoor, float *ray_origin,
                                          float *point, float *vector)
{
  float endpoint[3];

  endpoint[0] = scale * vector[0] + point[0];
  endpoint[1] = scale * vector[1] + point[1];
  endpoint[2] = scale * vector[2] + point[2];
  return FUN_0008ab90(out_fraction, indoor, ray_origin, endpoint);
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

/* Copy/stage camera command block from director into observer state (0x8b060).
 * Validates the command struct (pointed to by observer+0x4): checks forward/up
 * perpendicular, position/orientation in range, velocity valid, distance/FOV/
 * timer bounded. Then adjusts 5 component timers in the command based on the
 * observer's current timers and mode bytes, and finally copies the command
 * struct (0x68 bytes) into the observer at offset +0x8. */
void observer_update_command(int16_t local_player_index)
{
  char *observer;
  char *command;
  float *timer_out;
  uint8_t *mode_bytes;
  float *obs_timers;
  int i;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;
  command = *(char **)(observer + 0x4);
  timer_out = (float *)(command + 0x54);
  mode_bytes = (uint8_t *)(command + 0x4c);
  obs_timers = (float *)(observer + 0x5c);

  if (command == NULL ||
      ((*(uint8_t *)command & 1) &&
       (!valid_real_normal3d_perpendicular((float *)(command + 0x24),
                                           (float *)(command + 0x30)) ||
        (*(uint32_t *)(command + 0x4) & 0x7f800000) == 0x7f800000 ||
        !(((camera_command_t *)command)->position.x >= REAL_NEG_5000_POOL) ||
        !(((camera_command_t *)command)->position.x <= REAL_5000_POOL) ||
        (*(uint32_t *)(command + 0x8) & 0x7f800000) == 0x7f800000 ||
        !(((camera_command_t *)command)->position.y >= REAL_NEG_5000_POOL) ||
        !(((camera_command_t *)command)->position.y <= REAL_5000_POOL) ||
        (*(uint32_t *)(command + 0xc) & 0x7f800000) == 0x7f800000 ||
        !(((camera_command_t *)command)->position.z >= REAL_NEG_5000_POOL) ||
        !(((camera_command_t *)command)->position.z <= REAL_5000_POOL) ||
        (*(uint32_t *)(command + 0x10) & 0x7f800000) == 0x7f800000 ||
        !(((camera_command_t *)command)->offset.i >= REAL_NEG_5000_POOL) ||
        !(((camera_command_t *)command)->offset.i <= REAL_5000_POOL) ||
        (*(uint32_t *)(command + 0x14) & 0x7f800000) == 0x7f800000 ||
        !(((camera_command_t *)command)->offset.j >= REAL_NEG_5000_POOL) ||
        !(((camera_command_t *)command)->offset.j <= REAL_5000_POOL) ||
        (*(uint32_t *)(command + 0x18) & 0x7f800000) == 0x7f800000 ||
        !(((camera_command_t *)command)->offset.k >= REAL_NEG_5000_POOL) ||
        !(((camera_command_t *)command)->offset.k <= REAL_5000_POOL) ||
        !real_vector3d_valid((float *)(command + 0x3c)) ||
        (*(uint32_t *)(command + 0x1c) & 0x7f800000) == 0x7f800000 ||
        !(((camera_command_t *)command)->depth >= REAL_ZERO_POOL) ||
        !(((camera_command_t *)command)->depth <= REAL_5000_POOL) ||
        (*(uint32_t *)(command + 0x20) & 0x7f800000) == 0x7f800000 ||
        !(((camera_command_t *)command)->field_of_view >= REAL_0_001_POOL) ||
        !(((camera_command_t *)command)->field_of_view <= REAL_HALF_PI_POOL) ||
        (*(uint32_t *)(command + 0x48) & 0x7f800000) == 0x7f800000 ||
        !(((camera_command_t *)command)->timer >= REAL_ZERO_POOL) ||
        !(((camera_command_t *)command)->timer <= REAL_3600_POOL)))) {
    char *msg = csprintf((char *)0x5ab100,
                         "Invalid camera command.\n"
                         "F: (%f, %f, %f) U: (%f, %f, %f)\n"
                         "P: (%f, %f, %f) O: (%f, %f, %f)\n"
                         "D: %f V: (%f, %f, %f), FOV: %f, T: %f, FL: %ld",
                         (double)((camera_command_t *)command)->forward.i,
                         (double)((camera_command_t *)command)->forward.j,
                         (double)((camera_command_t *)command)->forward.k,
                         (double)((camera_command_t *)command)->up.i,
                         (double)((camera_command_t *)command)->up.j,
                         (double)((camera_command_t *)command)->up.k,
                         (double)((camera_command_t *)command)->position.x,
                         (double)((camera_command_t *)command)->position.y,
                         (double)((camera_command_t *)command)->position.z,
                         (double)((camera_command_t *)command)->offset.i,
                         (double)((camera_command_t *)command)->offset.j,
                         (double)((camera_command_t *)command)->offset.k,
                         (double)((camera_command_t *)command)->depth,
                         (double)((camera_command_t *)command)->velocity.i,
                         (double)((camera_command_t *)command)->velocity.j,
                         (double)((camera_command_t *)command)->velocity.k,
                         (double)((camera_command_t *)command)->field_of_view,
                         (double)((camera_command_t *)command)->timer,
                         *(uint32_t *)command);
    display_assert(msg, "c:\\halo\\SOURCE\\camera\\observer.c", 0x172, 1);
    system_exit(-1);
  }

  if (*(uint8_t *)(*(char **)(observer + 0x4)) & 1) {
    for (i = 5; i != 0; i--) {
      if ((*mode_bytes & 1) == 0) {
        command = *(char **)(observer + 0x4);
        if (((camera_command_t *)command)->timer < *obs_timers &&
            (*(uint8_t *)command & 8) == 0)
          goto clamp_timer;
        *timer_out = ((camera_command_t *)command)->timer;
      } else if ((*mode_bytes & 2) == 0 && *timer_out < *obs_timers) {
      clamp_timer:
        if (*obs_timers <= REAL_2_0_POOL)
          *timer_out = *obs_timers;
        else
          *timer_out = REAL_2_0_POOL;
      }

      timer_out++;
      obs_timers++;
      mode_bytes++;
    }

    {
      uint32_t *src = (uint32_t *)*(char **)(observer + 0x4);
      uint32_t *dst = (uint32_t *)(observer + 0x8);
      for (i = 0x1a; i != 0; i--)
        *dst++ = *src++;
    }
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

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;

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
        !(*(float *)(observer + 0xc) >= REAL_NEG_5000_POOL) ||
        !(*(float *)(observer + 0xc) <= REAL_5000_POOL) ||
        (*(uint32_t *)(observer + 0x10) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x10) >= REAL_NEG_5000_POOL) ||
        !(*(float *)(observer + 0x10) <= REAL_5000_POOL) ||
        (*(uint32_t *)(observer + 0x14) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x14) >= REAL_NEG_5000_POOL) ||
        !(*(float *)(observer + 0x14) <= REAL_5000_POOL) ||
        (*(uint32_t *)(observer + 0x18) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x18) >= REAL_NEG_5000_POOL) ||
        !(*(float *)(observer + 0x18) <= REAL_5000_POOL) ||
        (*(uint32_t *)(observer + 0x1c) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x1c) >= REAL_NEG_5000_POOL) ||
        !(*(float *)(observer + 0x1c) <= REAL_5000_POOL) ||
        (*(uint32_t *)(observer + 0x20) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x20) >= REAL_NEG_5000_POOL) ||
        !(*(float *)(observer + 0x20) <= REAL_5000_POOL) ||
        !real_vector3d_valid((float *)(observer + 0x44)) ||
        (*(uint32_t *)(observer + 0x24) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x24) >= REAL_ZERO_POOL) ||
        !(*(float *)(observer + 0x24) <= REAL_5000_POOL) ||
        (*(uint32_t *)(observer + 0x28) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x28) >= REAL_0_001_POOL) ||
        !(*(float *)(observer + 0x28) <= REAL_HALF_PI_POOL) ||
        (*(uint32_t *)(observer + 0x50) & 0x7f800000) == 0x7f800000 ||
        !(*(float *)(observer + 0x50) >= REAL_ZERO_POOL) ||
        !(*(float *)(observer + 0x50) <= REAL_3600_POOL)))) {
    char *msg = csprintf(
      (char *)0x5ab100,
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

  /* Compute polynomial coefficients for each of 5 components */
  for (comp = 0; comp < 5; comp++) {
    if ((*(uint8_t *)(observer + 0x8) & 1) && *timers > *(float *)0x335718) {
      float f = 1.0f / *timers;
      float f2 = f * f;
      float f3 = f2 * f;
      float f4 = f3 * f;
      int16_t size = ((int16_t *)0x2ee6b8)[comp];
      int16_t j;

      for (j = 0; j < size; j++) {
        int idx = (int)j;
        int off = idx * 4;

        *(float *)((char *)snap_ptr + off) =
          f3 * *(float *)((char *)accel_out_ptr + off) * *(float *)0x253398 -
          (f4 * *(float *)((char *)vel_out_ptr + off) * *(float *)0x254644 +
           f4 * f * *(float *)((char *)result_ptr + off) * *(float *)0x254640);

        *(float *)((char *)jerk_ptr + off) =
          f3 * *(float *)((char *)vel_out_ptr + off) * *(float *)0x2548f4 +
          f4 * *(float *)((char *)result_ptr + off) * *(float *)0x254cc0 -
          f2 * *(float *)((char *)accel_out_ptr + off);

        *(float *)((char *)accel_ptr + off) =
          f * *(float *)((char *)accel_out_ptr + off) * *(float *)0x253398 -
          (f2 * *(float *)((char *)vel_out_ptr + off) * *(float *)0x2533d8 +
           f3 * *(float *)((char *)result_ptr + off) * *(float *)0x253f34);

        *(int *)((char *)vel_ptr + off) = 0;
        *(int *)((char *)pos_ptr + off) = 0;
        *(int *)((char *)extra_ptr + off) = *(int *)((char *)result_ptr + off);

        if (comp == 0) {
          float fv = *(float *)(observer + 0x44 + off) * TICKS_PER_SECOND;
          *(float *)((char *)snap_ptr + off) -= f4 * fv * *(float *)0x254644;
          *(float *)((char *)jerk_ptr + off) += f3 * fv * *(float *)0x253f78;
          *(float *)((char *)accel_ptr + off) -= f2 * fv * *(float *)0x254640;
          *(float *)((char *)pos_ptr + off) += fv;
        }
      }
    }

    {
      int size = (int)((int16_t *)0x2ee6b8)[comp] * 4;
      snap_ptr = (float *)((char *)snap_ptr + size);
      jerk_ptr = (float *)((char *)jerk_ptr + size);
      accel_ptr = (float *)((char *)accel_ptr + size);
      vel_ptr = (float *)((char *)vel_ptr + size);
      pos_ptr = (float *)((char *)pos_ptr + size);
      extra_ptr = (float *)((char *)extra_ptr + size);
      vel_out_ptr = (float *)((char *)vel_out_ptr + size);
      accel_out_ptr = (float *)((char *)accel_out_ptr + size);
      result_ptr = (float *)((char *)result_ptr + size);
      timers++;
    }
  }
}

/* Apply observer polynomial update and orthogonalize result vectors (0x8ba10).
 * For each of 5 observer components, validates velocities (assert_valid_real),
 * then either copies defaults (when timer expired and mode active), evaluates
 * a quintic polynomial (when timer active), or negates velocity*delta_time
 * (when timer expired but mode not active). For the last component (index 4,
 * forward/up vectors), applies axis-angle rotation via
 * rotate_vector3d_by_sincos instead of simple addition. After the loop,
 * orthogonalizes the forward/up vectors via Gram-Schmidt if they are no longer
 * orthonormal. */
void observer_compute_update(int16_t local_player_index)
{
  char *observer;
  char *result_ptr;
  char *default_ptr;
  char *snap_ptr;
  char *jerk_ptr;
  char *accel_ptr;
  char *vel_ptr;
  float *velocities;
  char *pos_ptr;
  char *extra_ptr;
  float *timers;
  float scratch[14];
  float *scratch_ptr;
  int16_t comp;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;

  result_ptr = observer + 0xb0;
  default_ptr = observer + 0x0c;
  snap_ptr = observer + 0x158;
  jerk_ptr = observer + 0x184;
  accel_ptr = observer + 0x1b0;
  vel_ptr = observer + 0x1dc;
  velocities = (float *)(observer + 0xe8);
  pos_ptr = observer + 0x208;
  extra_ptr = observer + 0x234;
  timers = (float *)(observer + 0x5c);
  scratch_ptr = scratch;

  /* Validate all 11 velocity floats */
  {
    float *vp = velocities;
    int count = 0xb;
    do {
      if ((*(uint32_t *)vp & 0x7f800000u) == 0x7f800000u) {
        char *msg =
          csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
                   "observer->velocities.n[parameter_index]", *(uint32_t *)vp,
                   (double)*vp);
        display_assert(msg, "c:\\halo\\SOURCE\\camera\\observer.c", 0x2f4, 1);
        system_exit(-1);
      }
      vp++;
      count--;
    } while (count != 0);
  }

  for (comp = 0; comp < 5; comp++) {
    float elapsed = *timers - *(float *)0x335718;

    if (elapsed <= REAL_ZERO_POOL && (*(uint8_t *)(observer + 0x8) & 1)) {
      /* Timer expired and mode active: copy defaults */
      int16_t j = 0;
      if (j < ((int16_t *)0x2ee6ac)[comp]) {
        do {
          *(uint32_t *)(result_ptr + j * 4) =
            *(uint32_t *)(default_ptr + j * 4);
          j++;
        } while (j < ((int16_t *)0x2ee6ac)[comp]);
      }
    } else {
      /* Compute update values */
      if (elapsed <= REAL_ZERO_POOL) {
        /* Timer expired, mode not active: negate velocity*delta_time */
        int16_t j = 0;
        if (j < ((int16_t *)0x2ee6b8)[comp]) {
          do {
            scratch_ptr[j] = -(*(float *)0x335718 * velocities[j]);
            j++;
          } while (j < ((int16_t *)0x2ee6b8)[comp]);
        }
      } else {
        /* Timer active: evaluate quintic polynomial */
        float t2 = elapsed * elapsed;
        float t3 = t2 * elapsed;
        float t4 = t3 * elapsed;
        float t5 = t4 * elapsed;
        int16_t j = 0;

        if (j < ((int16_t *)0x2ee6b8)[comp]) {
          do {
            int idx = (int)j;
            int off = idx * 4;
            j++;
            scratch_ptr[idx] = t5 * *(float *)(snap_ptr + off) +
                               t4 * *(float *)(jerk_ptr + off) +
                               t3 * *(float *)(accel_ptr + off) +
                               t2 * *(float *)(vel_ptr + off) +
                               elapsed * *(float *)(pos_ptr + off) +
                               *(float *)(extra_ptr + off);
          } while (j < ((int16_t *)0x2ee6b8)[comp]);
        }
      }

      if (comp < 4) {
        /* Components 0-3: add scratch to result */
        int16_t j = 0;
        if (j < ((int16_t *)0x2ee6b8)[comp]) {
          do {
            int idx = (int)j;
            int off = idx * 4;
            j++;
            *(float *)(result_ptr + off) =
              scratch_ptr[idx] + *(float *)(result_ptr + off);
          } while (j < ((int16_t *)0x2ee6b8)[comp]);
        }
      } else {
        /* Component 4 (forward/up vectors): axis-angle rotation */
        float axis[3];
        float mag;

        axis[0] = scratch_ptr[0];
        axis[1] = scratch_ptr[1];
        axis[2] = scratch_ptr[2];
        mag = sqrtf(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);

        if (fabsf(mag) >= (float)*(double *)0x2533d0) {
          float inv_mag = *(float *)0x2533c8 / mag;
          axis[0] = axis[0] * inv_mag;
          axis[1] = axis[1] * inv_mag;
          axis[2] = axis[2] * inv_mag;

          if (mag != REAL_ZERO_POOL) {
            float sin_val = x87_fsin(mag);
            float cos_val = x87_fcos(mag);
            rotate_vector3d_by_sincos((float *)result_ptr, axis, sin_val,
                                      cos_val);
            rotate_vector3d_by_sincos((float *)(result_ptr + 0xc), axis,
                                      sin_val, cos_val);
          }
        }
      }
    }

    {
      int result_advance = (int)((int16_t *)0x2ee6ac)[comp] * 4;
      int vel_advance = (int)((int16_t *)0x2ee6b8)[comp] * 4;
      default_ptr += result_advance;
      result_ptr += result_advance;
      velocities += (int)((int16_t *)0x2ee6b8)[comp];
      snap_ptr += vel_advance;
      jerk_ptr += vel_advance;
      accel_ptr += vel_advance;
      vel_ptr += vel_advance;
      scratch_ptr += (int)((int16_t *)0x2ee6b8)[comp];
      pos_ptr += vel_advance;
      extra_ptr += vel_advance;
      timers++;
    }
  }

  /* Orthogonalize forward/up vectors if needed */
  {
    float *up = (float *)(observer + 0xd0);
    float *fwd = (float *)(observer + 0xdc);
    float check;

    /* Check if up is unit length */
    check =
      (up[0] * up[0] + up[1] * up[1] + up[2] * up[2]) - *(float *)0x2533c8;
    if ((*(uint32_t *)&check & 0x7f800000u) == 0x7f800000u ||
        fabsf(check) >= (float)*(double *)0x2549d8) {
      goto orthogonalize;
    }

    /* Check if forward is unit length */
    check = (fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]) -
            *(float *)0x2533c8;
    if ((*(uint32_t *)&check & 0x7f800000u) == 0x7f800000u ||
        fabsf(check) >= (float)*(double *)0x2549d8) {
      goto orthogonalize;
    }

    /* Check if up and forward are perpendicular */
    check = up[2] * fwd[2] + up[0] * fwd[0] + fwd[1] * up[1];
    if ((*(uint32_t *)&check & 0x7f800000u) == 0x7f800000u ||
        fabsf(check) >= (float)*(double *)0x2549d8) {
      goto orthogonalize;
    }

    return;

  orthogonalize: {
    float right[3];
    float mag;

    /* right = cross(fwd, up) */
    right[0] = up[2] * fwd[1] - fwd[2] * up[1];
    right[1] = up[0] * fwd[2] - up[2] * fwd[0];
    right[2] = up[1] * fwd[0] - up[0] * fwd[1];

    /* fwd = cross(up, right) */
    fwd[0] = right[2] * up[1] - right[1] * up[2];
    fwd[1] = right[0] * up[2] - right[2] * up[0];
    fwd[2] = right[1] * up[0] - right[0] * up[1];

    /* Normalize up */
    mag = sqrtf(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
    if (fabsf(mag) >= (float)*(double *)0x2533d0) {
      float inv = *(float *)0x2533c8 / mag;
      up[0] = inv * up[0];
      up[1] = inv * up[1];
      up[2] = inv * up[2];
    }

    /* Normalize forward */
    mag = sqrtf(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
    if (fabsf(mag) >= (float)*(double *)0x2533d0) {
      float inv = *(float *)0x2533c8 / mag;
      fwd[0] = inv * fwd[0];
      fwd[1] = inv * fwd[1];
      fwd[2] = inv * fwd[2];
    }
  }
  }
}

/* Validate two orientation axis-pairs and compute angular velocity delta
 * between them (0x8c030). Validates that (forward0, up0) and (forward1, up1)
 * are each a valid perpendicular pair, builds rotation matrices from each,
 * then extracts the angular velocity vector between them into result_angular.
 * forward0/up0 come from state+0x20/+0x2c; forward1/up1 from
 * velocities+0x20/+0x2c. The caller (FUN_0008c440) passes these via EDI/ESI
 * (forward0/up0) and EBX (up1) in the binary; here normalised as C params. */
void FUN_0008c030(float *forward1, float *result_angular, float *forward0,
                  float *up0, float *up1)
{
  float mat0[13]; /* 3x4 matrix (13 floats, padded to 52 bytes) */
  float new_var;
  float mat1[13];
  float new_var2;

  if (!valid_real_normal3d_perpendicular(forward0, up0)) {
    csprintf(
      (char *)0x5ab100,
      "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
      "forward0", (char *)0x2674e0, (double)forward0[0], (double)forward0[1],
      (double)forward0[2], (double)up0[0], (double)up0[1], (double)up0[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\camera\\observer.c",
                   0x382, 1);
    system_exit(-1);
  }
  new_var2 = forward1[2];
  if (!valid_real_normal3d_perpendicular(forward1, up1)) {
    new_var = up1[2];
    csprintf(
      (char *)0x5ab100,
      "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
      "forward1", (char *)0x267488, (double)forward1[0], (double)forward1[1],
      (double)new_var2, (double)up1[0], (double)up1[1], (double)new_var);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\camera\\observer.c",
                   0x383, 1);
    system_exit(-1);
  }
  matrix_from_forward_and_up(mat0, forward0, up0);
  matrix_from_forward_and_up(mat1, forward1, up1);
  quaternion_to_angle_and_vector(mat0, mat1, result_angular);
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
  char location[8];
  bool indoor_fog;
  float initial_fraction;
  float best_t;
  float test_fraction;
  float best_sign;
  float *best_plane;
  float adjusted_pos[3];
  float test_point[3];
  float up_scaled[3];
  float right_scaled[3];
  float near_plane_scale;
  float dist;
  int16_t i;
  int counter;

  initial_fraction = 1.0f;

  /* Determine location and indoor fog status at the focus position */
  scenario_location_from_point(location, position);
  indoor_fog = FUN_0018f3e0(location, position, 0);

  /* Compute adjusted position: position - (near_plane_dist + *focus_distance) *
   * forward */
  dist = near_plane_dist + *focus_distance;
  adjusted_pos[0] = -(dist * forward[0]) + position[0];
  adjusted_pos[1] = -(dist * forward[1]) + position[1];
  adjusted_pos[2] = -(dist * forward[2]) + position[2];

  /* Cast initial ray from position to adjusted_pos */
  FUN_0008ab90(&initial_fraction, indoor_fog, position, adjusted_pos);

  /* Scale factor for the near-plane probe vectors */
  near_plane_scale = *(float *)0x2673a4 * *focus_distance;

  /* Compute scaled up vector */
  up_scaled[0] = up[0] * near_plane_scale;
  up_scaled[1] = up[1] * near_plane_scale;
  up_scaled[2] = up[2] * near_plane_scale;

  /* Compute scaled right vector = cross(up, forward) * near_plane_scale */
  right_scaled[0] =
    (up[1] * forward[2] - up[2] * forward[1]) * near_plane_scale;
  right_scaled[1] =
    (up[2] * forward[0] - up[0] * forward[2]) * near_plane_scale;
  right_scaled[2] =
    (up[0] * forward[1] - up[1] * forward[0]) * near_plane_scale;

  best_t = initial_fraction;
  best_plane = (float *)0;
  counter = 0;

  /* Sweep 4 directions: -up, -right, +up, +right */
  i = 0;
  do {
    float sign;
    int plane_idx;
    float *plane;

    /* sign: -1 for i=0,1; +1 for i=2,3 */
    sign = (float)(int)(((i & 2) ? 2 : 0) - 1);

    /* plane: up_scaled for even counter, right_scaled for odd */
    plane_idx = counter & 1;
    plane = (plane_idx == 0) ? up_scaled : right_scaled;

    test_point[0] = sign * plane[0] + adjusted_pos[0];
    test_point[1] = sign * plane[1] + adjusted_pos[1];
    test_point[2] = sign * plane[2] + adjusted_pos[2];

    if (FUN_0008ab90(&test_fraction, indoor_fog, position, test_point)) {
      if (test_fraction < best_t) {
        best_t = test_fraction;
        best_plane = plane;
        best_sign = sign;
      }
    }

    i = (int16_t)(i + 1);
    counter = counter + 1;
  } while (i < 4);

  if (best_plane != (float *)0) {
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

      step = (refinement_scale + offset) * *(float *)0x253398;

      test_point[0] = step * best_plane[0] + adjusted_pos[0];
      test_point[1] = step * best_plane[1] + adjusted_pos[1];
      test_point[2] = step * best_plane[2] + adjusted_pos[2];

      hit = FUN_0008ab90(&test_fraction, indoor_fog, position, test_point);

      if (hit &&
          (float)*(double *)0x2674e8 > fabsf(test_fraction - best_frac)) {
        /* Converged: update the refinement boundary */
        best_frac = test_fraction;
        refinement_scale = step;
      } else {
        /* Did not converge: record the step as offset */
        offset = step;
        if (!hit) {
          fraction = *(float *)0x2533c8;
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
      value = (float)(int)(refinement_scale >= REAL_ZERO_POOL);
    }

    if (value == REAL_ZERO_POOL) {
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
                       (*(float *)0x2533c8 - final_scale) * best_t) *
                      *focus_distance;
    return;
  }

  /* No obstruction found: scale focus_distance by initial fraction */
  *focus_distance = initial_fraction * *focus_distance;
}

/* Compute linear and angular velocity deltas between velocities and state
 * (0x8c440). Takes three observer sub-arrays: velocities, result (output),
 * and state. Subtracts state[0..7] from velocities[0..7] into result[0..7]
 * (8 linear floats). Then calls FUN_0008c030 once to compute the angular
 * delta between the forward/up orientation pair at offset +8 (floats) in
 * velocities and state, storing the result into result+8.
 *
 * Register args: result @<eax>, state @<ecx>. Stack arg: velocities. */
void FUN_0008c440(void *velocities, void *result, void *state)
{
  volatile int i;

  i = 8;
  do {
    *(float *)result = *(float *)velocities - *(float *)state;
    velocities = (char *)velocities + 4;
    state = (char *)state + 4;
    result = (char *)result + 4;
    i--;
  } while (i != 0);

  i = 1;
  do {
    FUN_0008c030((float *)velocities, (float *)result, (float *)state,
                 (float *)((char *)state + 0xc),
                 (float *)((char *)velocities + 0xc));
    velocities = (char *)velocities + 0x18;
    state = (char *)state + 0x18;
    result = (char *)result + 0xc;
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
  char *observer;
  float *forward;
  float *up;
  float focus_position[3];
  float focus_distance;
  float fov;
  float mag_xy, inv_mag;
  float fwd_n_x, fwd_n_y;
  float height_diff;
  int location[2]; /* {leaf_index, cluster_index(int16 at +4)} */

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;

  /* Read focus position from observer+0xb0 */
  focus_position[0] = *(float *)(observer + 0xb0);
  focus_position[1] = *(float *)(observer + 0xb4);
  focus_position[2] = *(float *)(observer + 0xb8);

  /* Clamp focus distance to [0, max_distance], or FLT_MAX if over */
  {
    float fd = *(float *)(observer + 0xc8);
    if (fd < REAL_ZERO_POOL) {
      focus_distance = 0.0f;
    } else if (fd > *(float *)0x2548fc) {
      focus_distance = 3.4028235e+38f;
    } else {
      focus_distance = fd;
    }
  }

  /* assert: valid_world_real_point3d(&focus_position) */
  if ((*(uint32_t *)&focus_position[0] & 0x7f800000u) == 0x7f800000u ||
      focus_position[0] < REAL_NEG_5000_POOL ||
      focus_position[0] > REAL_5000_POOL ||
      (*(uint32_t *)&focus_position[1] & 0x7f800000u) == 0x7f800000u ||
      focus_position[1] < REAL_NEG_5000_POOL ||
      focus_position[1] > REAL_5000_POOL ||
      (*(uint32_t *)&focus_position[2] & 0x7f800000u) == 0x7f800000u ||
      focus_position[2] < REAL_NEG_5000_POOL ||
      focus_position[2] > REAL_5000_POOL) {
    display_assert("valid_world_real_point3d(&focus_position)",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x3af, 1);
    system_exit(-1);
  }

  /* Validate forward/up axes */
  forward = (float *)(observer + 0xd0);
  up = (float *)(observer + 0xdc);
  if (!valid_real_normal3d_perpendicular(forward, up)) {
    csprintf(
      (char *)0x5ab100,
      "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
      "&observer->forward", "&observer->up", (double)forward[0],
      (double)forward[1], (double)forward[2], (double)up[0], (double)up[1],
      (double)up[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\camera\\observer.c",
                   0x3b0, 1);
    system_exit(-1);
  }

  /* assert: valid_world_real_point3d(&observer->focus_offset) */
  if ((*(uint32_t *)(observer + 0xbc) & 0x7f800000u) == 0x7f800000u ||
      *(float *)(observer + 0xbc) < REAL_NEG_5000_POOL ||
      *(float *)(observer + 0xbc) > REAL_5000_POOL ||
      (*(uint32_t *)(observer + 0xc0) & 0x7f800000u) == 0x7f800000u ||
      *(float *)(observer + 0xc0) < REAL_NEG_5000_POOL ||
      *(float *)(observer + 0xc0) > REAL_5000_POOL ||
      (*(uint32_t *)(observer + 0xc4) & 0x7f800000u) == 0x7f800000u ||
      *(float *)(observer + 0xc4) < REAL_NEG_5000_POOL ||
      *(float *)(observer + 0xc4) > REAL_5000_POOL) {
    display_assert(
      "valid_world_real_point3d((real_point3d *) &observer->focus_offset)",
      "c:\\halo\\SOURCE\\camera\\observer.c", 0x3b1, 1);
    system_exit(-1);
  }

  /* assert: valid_focus_distance(focus_distance) */
  if ((*(uint32_t *)&focus_distance & 0x7f800000u) == 0x7f800000u ||
      focus_distance < REAL_ZERO_POOL || focus_distance > REAL_5000_POOL) {
    display_assert("valid_focus_distance(focus_distance)",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x3b2, 1);
    system_exit(-1);
  }

  /* Clamp field_of_view to [fov_min, fov_max] */
  {
    float f = *(float *)(observer + 0xcc);
    if (f < REAL_0_001_POOL) {
      fov = REAL_0_001_POOL;
    } else if (f > REAL_HALF_PI_POOL) {
      fov = REAL_HALF_PI_POOL;
    } else {
      fov = f;
    }
    *(float *)(observer + 0xcc) = fov;
  }

  /* Clamp focus_position components to [-5000, 5000] */
  if (focus_position[0] < REAL_NEG_5000_POOL) {
    focus_position[0] = -5000.0f;
  } else if (focus_position[0] > REAL_5000_POOL) {
    focus_position[0] = 5000.0f;
  }

  if (focus_position[1] < REAL_NEG_5000_POOL) {
    focus_position[1] = -5000.0f;
  } else if (focus_position[1] > REAL_5000_POOL) {
    focus_position[1] = 5000.0f;
  }

  if (focus_position[2] < REAL_NEG_5000_POOL) {
    focus_position[2] = -5000.0f;
  } else if (focus_position[2] > REAL_5000_POOL) {
    focus_position[2] = 5000.0f;
  }

  /* Clamp focus_distance to [0, 5000] */
  if (focus_distance < REAL_ZERO_POOL) {
    focus_distance = 0.0f;
  } else if (focus_distance > REAL_5000_POOL) {
    focus_distance = 5000.0f;
  }

  /* Normalize forward vector in XY plane */
  fwd_n_x = forward[0];
  fwd_n_y = forward[1];
  mag_xy = sqrtf(fwd_n_x * fwd_n_x + fwd_n_y * fwd_n_y);
  if (fabsf(mag_xy) >= (float)*(double *)0x2533d0) {
    inv_mag = *(float *)0x2533c8 / mag_xy;
    fwd_n_x = inv_mag * fwd_n_x;
    fwd_n_y = inv_mag * fwd_n_y;
  }

  /* Apply rotated focus_offset to focus_position using normalized forward */
  focus_position[0] = fwd_n_x * *(float *)(observer + 0xbc) +
                      fwd_n_y * *(float *)(observer + 0xc0) + focus_position[0];
  focus_position[1] = (fwd_n_y * *(float *)(observer + 0xbc) -
                       fwd_n_x * *(float *)(observer + 0xc0)) +
                      focus_position[1];
  focus_position[2] = focus_position[2] + *(float *)(observer + 0xc4);

  /* Near-plane collision fix (skip if mode bit 0x10 set or focus_distance == 0)
   */
  if ((*(uint8_t *)(observer + 0x8) & 0x10) == 0 &&
      focus_distance != REAL_ZERO_POOL) {
    FUN_0008c150(up, &focus_distance, 0.02f, forward, focus_position);
  }

  /* Compute result.position = focus_position - focus_distance * forward */
  *(float *)(observer + 0x74) = focus_position[0] - focus_distance * forward[0];
  *(float *)(observer + 0x78) = focus_position[1] - focus_distance * forward[1];
  *(float *)(observer + 0x7c) = focus_position[2] - focus_distance * forward[2];

  /* Determine BSP cluster location for the camera position */
  scenario_location_from_point(&location, observer + 0x74);

  /* If cluster changed, precache resources for the new cluster */
  {
    int16_t cluster = *(int16_t *)((char *)&location + 4);
    if (cluster != -1) {
      if (cluster != *(int16_t *)(observer + 0x84)) {
        void *element = tag_block_get_element((char *)scenario_get() + 0x134,
                                              (int)cluster, 0x68);
        predicted_resources_precache((int *)((char *)element + 0x28));
      }
      *(int *)(observer + 0x80) = location[0];
      *(int *)(observer + 0x84) = *(int *)((char *)&location + 4);
    }
  }

  /* Ground height adjustment */
  {
    float h = FUN_0018f510(observer + 0x80, observer + 0x74);
    height_diff = h;
    if (fabsf(height_diff) < (float)*(double *)0x25f0c8) {
      if (height_diff <= REAL_ZERO_POOL) {
        *(float *)(observer + 0x7c) =
          height_diff + *(float *)(observer + 0x7c) + *(float *)0x2533e8;
      } else {
        *(float *)(observer + 0x7c) =
          *(float *)(observer + 0x7c) - (*(float *)0x2533e8 - height_diff);
      }
    }
  }

  /* assert: valid_world_real_point3d(&observer->result.position) */
  if ((*(uint32_t *)(observer + 0x74) & 0x7f800000u) == 0x7f800000u ||
      *(float *)(observer + 0x74) < REAL_NEG_5000_POOL ||
      *(float *)(observer + 0x74) > REAL_5000_POOL ||
      (*(uint32_t *)(observer + 0x78) & 0x7f800000u) == 0x7f800000u ||
      *(float *)(observer + 0x78) < REAL_NEG_5000_POOL ||
      *(float *)(observer + 0x78) > REAL_5000_POOL ||
      (*(uint32_t *)(observer + 0x7c) & 0x7f800000u) == 0x7f800000u ||
      *(float *)(observer + 0x7c) < REAL_NEG_5000_POOL ||
      *(float *)(observer + 0x7c) > REAL_5000_POOL) {
    display_assert("valid_world_real_point3d(&observer->result.position)",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x41f, 1);
    system_exit(-1);
  }

  /* Validate forward/up axes again */
  if (!valid_real_normal3d_perpendicular(forward, up)) {
    csprintf(
      (char *)0x5ab100,
      "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
      "&observer->forward", "&observer->up", (double)forward[0],
      (double)forward[1], (double)forward[2], (double)up[0], (double)up[1],
      (double)up[2]);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\camera\\observer.c",
                   0x420, 1);
    system_exit(-1);
  }

  /* assert: valid_field_of_view(observer->field_of_view) */
  if ((*(uint32_t *)(observer + 0xcc) & 0x7f800000u) == 0x7f800000u ||
      *(float *)(observer + 0xcc) < REAL_0_001_POOL ||
      *(float *)(observer + 0xcc) > REAL_HALF_PI_POOL) {
    display_assert("valid_field_of_view(observer->field_of_view)",
                   "c:\\halo\\SOURCE\\camera\\observer.c", 0x421, 1);
    system_exit(-1);
  }

  /* Clamp result.position to world bounds */
  {
    float v;
    v = *(float *)(observer + 0x74);
    if (v < REAL_NEG_5000_POOL) {
      v = REAL_NEG_5000_POOL;
    } else if (v > REAL_5000_POOL) {
      v = REAL_5000_POOL;
    }
    *(float *)(observer + 0x74) = v;

    v = *(float *)(observer + 0x78);
    if (v < REAL_NEG_5000_POOL) {
      v = REAL_NEG_5000_POOL;
    } else if (v > REAL_5000_POOL) {
      v = REAL_5000_POOL;
    }
    *(float *)(observer + 0x78) = v;

    v = *(float *)(observer + 0x7c);
    if (v < REAL_NEG_5000_POOL) {
      v = REAL_NEG_5000_POOL;
    } else if (v > REAL_5000_POOL) {
      v = REAL_5000_POOL;
    }
    *(float *)(observer + 0x7c) = v;
  }

  /* Copy forward -> result.forward, negate velocities -> result.velocity,
   * copy up -> result.up, copy field_of_view -> result.field_of_view */
  *(float *)(observer + 0x94) = forward[0];
  *(float *)(observer + 0x88) = -*(float *)(observer + 0xe8);
  *(float *)(observer + 0x98) = forward[1];
  *(float *)(observer + 0x9c) = forward[2];
  *(float *)(observer + 0x8c) = -*(float *)(observer + 0xec);
  *(float *)(observer + 0x90) = -*(float *)(observer + 0xf0);
  *(float *)(observer + 0xa0) = up[0];
  *(float *)(observer + 0xa4) = up[1];
  *(float *)(observer + 0xac) = *(float *)(observer + 0xcc);
  *(float *)(observer + 0xa8) = up[2];
}

/* Compute observer velocities from current and target state (0x8ccf0).
 * Dispatches to FUN_0008c440 (linear+angular delta) with pointers into the
 * observer struct: velocities at +0xc, result at +0x260, state at +0xb0. */
void observer_compute_velocities(int16_t local_player_index)
{
  char *observer;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)local_player_index * 0x29c;
  FUN_0008c440(observer + 0xc, observer + 0x260, observer + 0xb0);
}

/* Update observer position timers and integration (0x8cd40).
 * Validates the player index, checks if the observer is paused (bit 0x20
 * of the byte pointed to by observer+0x4), then dispatches five internal
 * sub-update functions and clamps 5 timer floats at observer+0x5c. */
void observer_update_positions(int16_t local_player_index)
{
  int i;
  char *observer;
  float *timers;
  float val;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  observer = (char *)0x33571c + (int)(int16_t)local_player_index * 0x29c;
  timers = (float *)(observer + 0x5c);

  if ((*(unsigned char *)(*(int *)(observer + 0x4)) & 0x20) == 0) {
    observer_compute_velocities(local_player_index);
    observer_compute_accelerations(local_player_index);
    observer_apply_acceleration(local_player_index);
    observer_integrate(local_player_index);
    observer_compute_update(local_player_index);

    for (i = 5; i != 0; i--) {
      val = *timers - *(float *)0x335718;
      if (!(val > REAL_ZERO_POOL)) {
        val = REAL_ZERO_POOL;
      }
      *timers = val;
      timers++;
    }
  }
}

/* Per-tick observer update for all local players (0x8cde0).
 * Saves the frame's delta-time into the global at 0x335718, then walks
 * each of MAXIMUM_NUMBER_OF_LOCAL_PLAYERS observers (stride 0x29c from
 * 0x33571c), verifies the header/trailer OBSERVER_SIGNATURE ('!dar' =
 * 0x72616421) and that updated_for_frame is clear, marks it set, and
 * dispatches three observer sub-updates:
 *   - observer_update_command (0x8b060): copies/stages camera block from
 *     director into observer state
 *   - observer_update_positions (0x8cd40): time-dependent integration,
 *     skipped when delta_time matches the cached value at 0x2533c0
 *   - observer_update_result (0x8c4b0): derives the final observer camera
 *     result from staged and integrated state */
void observer_update(float delta_time)
{
  int16_t i;
  char *observer = (char *)0x33571c;

  *(float *)0x335718 = delta_time;

  for (i = 0; i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; i++, observer += 0x29c) {
    if (local_player_get_player_index(i) == -1)
      continue;

    if (i < 0 || i >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
      display_assert("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x72, 1);
      system_exit(-1);
    }

    if (*(int *)(observer + 0x0) != 0x72616421 ||
        *(int *)(observer + 0x298) != 0x72616421) {
      display_assert("observer->header_signature==OBSERVER_SIGNATURE && "
                     "observer->trailer_signature==OBSERVER_SIGNATURE",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x108, 1);
      system_exit(-1);
    }

    if (*(char *)(observer + 0x70) != 0) {
      display_assert("!observer->updated_for_frame",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x109, 1);
      system_exit(-1);
    }

    *(char *)(observer + 0x70) = 1;

    observer_update_command(i);

    if (*(float *)0x335718 != REAL_ZERO_POOL) {
      observer_update_positions(i);
    }

    observer_update_result(i);

    if (*(int *)(observer + 0x0) != 0x72616421 ||
        *(int *)(observer + 0x298) != 0x72616421) {
      display_assert("observer->header_signature==OBSERVER_SIGNATURE && "
                     "observer->trailer_signature==OBSERVER_SIGNATURE",
                     "c:\\halo\\SOURCE\\camera\\observer.c", 0x117, 1);
      system_exit(-1);
    }
  }
}

/* Initialize an orbiting camera block (0x8cf10).
 * Layout is derived from the two accesses here: the distance is stored at
 * +0x08, and +0x00/+0x04 are filled by vector_to_angles(), which writes a
 * two-float angle pair (yaw, pitch). Store order matches the reference:
 * the distance FSTP happens before the call. */
void orbiting_camera_new(float *orbiting_camera, float distance, float *vector)
{
  orbiting_camera[2] = distance;
  vector_to_angles(orbiting_camera, vector);
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
  if (info.unit_index != -1) {
    angles_to_vector((float *)&result->forward, (float *)&camera->facing);
    observer_up_from_forward((float *)&result->forward, (float *)&result->up);
    object_get_root_location(info.unit_index, (float *)&result->velocity, NULL);
    result->position.z += *(real *)0x2676ac;
    result->flags = 1;
  }
  result->offset = *(real_vector3d *)global_zero_vector_ptr;
  result->depth = camera->distance;
  result->field_of_view = *(real *)0x26769c;
  result->timer = *(real *)0x2676a8;
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
         result->depth <= 5000.0f && valid_real(*(real *)0x26769c) &&
         *(real *)0x26769c >= 0.001f &&
         *(real *)0x26769c <= 1.57079637f /* pi/2 */ &&
         valid_real(*(real *)0x2676a8) && *(real *)0x2676a8 >= 0.0f &&
         *(real *)0x2676a8 <= 3600.0f))) {
    display_assert(
      csprintf(error_string_buffer, CAMERA_COMMAND_INVALID_FORMAT,
               result->forward.i, result->forward.j, result->forward.k,
               result->up.i, result->up.j, result->up.k, result->position.x,
               result->position.y, result->position.z, result->offset.i,
               result->offset.j, result->offset.k, result->depth,
               result->velocity.i, result->velocity.j, result->velocity.k,
               *(real *)0x26769c, *(real *)0x2676a8, result->flags),
      "c:\\halo\\SOURCE\\camera\\orbiting_camera.c", 0x47, 1);
    system_exit(-1);
  }
}

/* Fill in a static (scripted) camera command block (0x8d3a0).
 * Pure stores, no calls. Field meanings are taken from the consumer at
 * 0x8d410 (static_camera.c), which copies this block into a camera-command
 * result and asserts ranges on it:
 *   +0x00 position       -> result+0x04 ("P:")
 *   +0x0c location       -- never read by 0x8d410
 *   +0x10 forward        -> result+0x24 ("F:")
 *   +0x1c up             -> result+0x30 ("U:")
 *   +0x28 field of view  -> result+0x20, asserted in [0.001, 1.57079637]
 *   +0x2c duration       -> read as *(int *) and converted to float ("T:")
 *   +0x30 command flags  -> result[0] | 1
 *   +0x34 applied flag   -> cleared here, set to 1 by 0x8d410 after use
 * The three vectors are copied as whole 12-byte structs, matching the
 * reference's three dword MOV pairs per vector. */
void static_camera_new(void *camera, const vector3_t *position,
                       uint32_t location, const vector3_t *forward,
                       const vector3_t *up, float field_of_view,
                       int32_t duration_ticks, uint32_t flags)
{
  char *block = (char *)camera;

  *(vector3_t *)(block + 0x00) = *position;
  *(uint32_t *)(block + 0x0c) = location;
  *(vector3_t *)(block + 0x10) = *forward;
  *(vector3_t *)(block + 0x1c) = *up;
  *(float *)(block + 0x28) = field_of_view;
  *(int32_t *)(block + 0x2c) = duration_ticks;
  *(uint8_t *)(block + 0x34) = 0;
  *(uint32_t *)(block + 0x30) = flags;
}
