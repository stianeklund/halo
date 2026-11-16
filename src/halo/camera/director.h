/* Camera director constants and file-local layouts (director.obj). */
#ifndef HALO_CAMERA_DIRECTOR_H
#define HALO_CAMERA_DIRECTOR_H

/* Director camera modes: the indices of director_camera_mode_names
 * ("following", "orbiting", "flying", "editor", "first person"), matching the
 * FUN_00086a50 switch arms. */
enum {
  _director_camera_mode_following = 0,
  _director_camera_mode_orbiting = 1,
  _director_camera_mode_flying = 2,
  _director_camera_mode_editor = 3,
  _director_camera_mode_first_person = 4
};

#define NUMBER_OF_DIRECTOR_VARIABLES 4

/* camera_director_t.camera_proc called as a function by director_update. */
typedef void (*director_camera_proc)(void *camera_data,
                                     camera_action_t *action,
                                     camera_command_t *command);

/* Scenario tag block at +0x354 (element size 0x34) that seeds the editor
 * camera focus from its first element: three position dwords and a yaw.
 * It lies inside scenario_t.pad_210, which is not split in types.h. */
#define scenario_block_354(scenario) \
  ((tag_block *)((char *)(scenario) + 0x354))
#define SCENARIO_BLOCK_354_ELEMENT_SIZE 0x34

/* Pointer slot at 0x2ee670.  The editor camera reads three vectors from the
 * pointee: the point, forward and up it hands to
 * scripted_camera_set_camera_point_relative.  The pointee is also read by
 * rasterizer/profile code; nothing else about it is proven. */
typedef struct {
  uint8_t pad_00[0x10];
  real_point3d position;
  real_vector3d forward;
  real_vector3d up;
} editor_camera_source_t;
co(editor_camera_source_t, position, 0x10);
co(editor_camera_source_t, forward, 0x1c);
co(editor_camera_source_t, up, 0x28);
#define editor_camera_source (*(editor_camera_source_t **)0x2ee670)
/* Per-mode editor camera update procs; named by the
 * "update_funcs[camera_mode]" assert in editor_camera_update. */
#define update_funcs ((director_camera_proc *)0x2ee674)

/* kb.json data symbols still spelled by address.  Using the extern loses
 * aligned bytes: director_unknown_335690 + director_stick_pitch_scale +
 * director_stick_yaw_scale together -2 in director_compute_camera_input
 * (any subset as externs loses up to -18), editor_camera_default_position_y
 * -1 in FUN_00087c00, editor_camera_scripted_messages -6 in
 * editor_camera_set_scripted.  Deleting a line switches its uses to the kb
 * symbol. */
#define director_unknown_335690 (*(uint8_t *)0x335690)
#define director_stick_pitch_scale (*(real *)0x2670ac)
#define director_stick_yaw_scale (*(real *)0x2670b0)
#define editor_camera_default_position_y (*(real *)0x2670d8)
#define editor_camera_scripted_messages ((const char **)0x2ee694)

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
 * float bit pattern for NaN/infinity and compares against the constant pools
 * instead of calling valid_real().  The field of view and timer operands are
 * separate because editor_camera_orbiting_update validates them from its
 * globals rather than from the command. */
#define real_bits_valid(value) \
  ((*(uint32_t *)&(value) & 0x7f800000) != 0x7f800000)
#define director_world_real_valid(value)                                  \
  (real_bits_valid(value) && (value) >= REAL_NEG_5000_POOL &&             \
   (value) <= REAL_5000_POOL)
#define director_camera_command_valid(command, field_of_view, timer)      \
  (!((command)->flags & FLAG(_observer_command_valid_bit)) ||             \
   (valid_real_normal3d_perpendicular((float *)&(command)->forward,       \
                                      (float *)&(command)->up) &&         \
    director_world_real_valid((command)->position.x) &&                   \
    director_world_real_valid((command)->position.y) &&                   \
    director_world_real_valid((command)->position.z) &&                   \
    director_world_real_valid((command)->offset.i) &&                     \
    director_world_real_valid((command)->offset.j) &&                     \
    director_world_real_valid((command)->offset.k) &&                     \
    real_vector3d_valid((float *)&(command)->velocity) &&                 \
    real_bits_valid((command)->depth) &&                                  \
    (command)->depth >= REAL_ZERO_POOL &&                                 \
    (command)->depth <= REAL_5000_POOL && real_bits_valid(field_of_view) && \
    (field_of_view) >= REAL_0_001_POOL &&                                 \
    (field_of_view) <= REAL_HALF_PI_POOL && real_bits_valid(timer) &&     \
    (timer) >= REAL_ZERO_POOL && (timer) <= REAL_3600_POOL))

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
