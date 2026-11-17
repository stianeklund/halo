/* Camera director constants and file-local layouts (director.obj). */
#ifndef HALO_CAMERA_DIRECTOR_H
#define HALO_CAMERA_DIRECTOR_H

/* Director camera modes: the indices of director_camera_mode_names
 * ("following", "orbiting", "flying", "editor", "first person"), matching the
 * director_rotate_cameras switch arms. */
enum {
  _director_camera_mode_following = 0,
  _director_camera_mode_orbiting = 1,
  _director_camera_mode_flying = 2,
  _director_camera_mode_editor = 3,
  _director_camera_mode_first_person = 4
};

#define NUMBER_OF_DIRECTOR_VARIABLES 4

/* camera_director_t.camera_data holds the flying camera in the flying and
 * editor modes. */
static __inline flying_camera_t *director_flying_camera(camera_director_t *director)
{
  void *camera = director->camera_data;

  return camera;
}

/* editor_camera_globals.unit_offset handed to
 * scripted_camera_set_camera_point_relative as the object-relative
 * position. */
static __inline const real_point3d *editor_camera_unit_relative_position(void)
{
  const void *position = &editor_camera_globals.unit_offset;

  return position;
}

#define k_half_pi 1.5707964f

/* camera_director_t.camera_proc called as a function by director_update. */
typedef void (*director_camera_proc)(void *camera_data,
                                     camera_action_t *action,
                                     camera_command_t *command);

/* Record returned by input_get_mouse_state that the debug camera reads: yaw and pitch deltas, the wheel delta that
 * becomes action->wheel_delta, and a byte of unproven meaning tested next to
 * the key 0x1e check. */
typedef struct {
  int32_t delta_x;
  int32_t delta_y;
  int32_t wheel;
  uint8_t pad_0c[1];
  uint8_t field_0d;
} director_mouse_state_t;
co(director_mouse_state_t, wheel, 0x08);
co(director_mouse_state_t, field_0d, 0x0d);

/* The two player_data_t members director_update_player_camera reads; +0xaa
 * lies inside player_data_t.pad_8c, which is not split in types.h.  The
 * word is read as a signed count (game_engine.c compares it against the
 * lives limit). */
typedef struct {
  uint8_t pad_00[0x34];
  datum_index unit_handle;
  uint8_t pad_38[0x72];
  int16_t field_aa;
} director_player_view_t;
co(director_player_view_t, unit_handle, 0x34);
co(director_player_view_t, field_aa, 0xaa);

#define SCENARIO_PLAYER_STARTING_LOCATION_SIZE 0x34

/* Pointer slot at 0x2ee670.  The editor camera reads three vectors from the
 * pointee: the point, forward and up it hands to
 * scripted_camera_set_camera_point_relative.  The pointee is also read by
 * rasterizer/profile code; nothing else about it is proven. */
typedef struct editor_camera_source {
  uint8_t pad_00[0x10];
  real_point3d position;
  real_vector3d forward;
  real_vector3d up;
} editor_camera_source_t;
co(editor_camera_source_t, position, 0x10);
co(editor_camera_source_t, forward, 0x1c);
co(editor_camera_source_t, up, 0x28);

/* Leading fields of a scenario player starting location: the editor
 * camera's starting focus. */
typedef struct {
  real_point3d position;
  real_euler_angles2d angles;
} director_scenario_start_t;

/* input_get_gamepad_state result, 0x28 bytes; same layout as the
 * input_gamepad_state of input_xbox.c.  The *_hold arrays count the frames a
 * button has been held. */
typedef struct input_gamepad_state {
  uint8_t raw_analog[8];
  uint8_t smoothed_analog[8];
  uint8_t analog_hold[8];
  uint8_t digital_hold[8];
  int16_t stick_lx;
  int16_t stick_ly;
  int16_t stick_rx;
  int16_t stick_ry;
} input_gamepad_state;
cs(input_gamepad_state, 0x28);
co(input_gamepad_state, analog_hold, 0x10);
co(input_gamepad_state, digital_hold, 0x18);
co(input_gamepad_state, stick_lx, 0x20);
co(input_gamepad_state, stick_ry, 0x26);

/* The director's inline copy of the camera command validator.  It tests the
 * float bit pattern for NaN/infinity and range-checks the command instead of
 * calling valid_real().  The field of view and timer operands are separate
 * because editor_camera_orbiting_update validates them from its globals
 * rather than from the command. */
#define k_real_exponent_mask 0x7f800000
#define real_bits_valid(value) \
  ((real_bits_of(&(value))->bits & k_real_exponent_mask) != k_real_exponent_mask)
#define director_world_real_valid(value) \
  (real_bits_valid(value) && (value) >= -5000.0f && (value) <= 5000.0f)
#define director_camera_command_valid(command, field_of_view, timer)      \
  (!((command)->flags & FLAG(_observer_command_valid_bit)) ||             \
   (valid_real_normal3d_perpendicular(&(command)->forward,       \
                                      &(command)->up) &&         \
    director_world_real_valid((command)->position.x) &&                   \
    director_world_real_valid((command)->position.y) &&                   \
    director_world_real_valid((command)->position.z) &&                   \
    director_world_real_valid((command)->offset.i) &&                     \
    director_world_real_valid((command)->offset.j) &&                     \
    director_world_real_valid((command)->offset.k) &&                     \
    real_vector3d_valid(&(command)->velocity) &&                 \
    real_bits_valid((command)->depth) && (command)->depth >= 0.0f &&      \
    (command)->depth <= 5000.0f && real_bits_valid(field_of_view) &&      \
    (field_of_view) >= 0.001f && (field_of_view) <= k_half_pi &&          \
    real_bits_valid(timer) && (timer) >= 0.0f && (timer) <= 3600.0f))

/* "Invalid camera command." arguments with the field of view and timer
 * taken from the given operands (see director_camera_command_valid). */
#define CAMERA_COMMAND_INVALID_ARGUMENTS_AT(command, field_of_view, timer)  \
  (command)->forward.i, (command)->forward.j, (command)->forward.k,        \
    (command)->up.i, (command)->up.j, (command)->up.k,                     \
    (command)->position.x, (command)->position.y, (command)->position.z,   \
    (command)->offset.i, (command)->offset.j, (command)->offset.k,         \
    (command)->depth, (command)->velocity.i, (command)->velocity.j,        \
    (command)->velocity.k, (field_of_view), (timer), (command)->flags

#endif
