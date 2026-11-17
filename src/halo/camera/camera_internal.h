/* Camera-internal register-arg shims.
 *
 * The callees below live in the unported portion of the original XBE and use
 * non-standard calling conventions: one or two arguments are passed in CPU
 * registers (EAX/AX/BL/ESI/EBX) rather than on the stack.
 *
 * These are reached by declaring the register positions in kb.json with
 * `@<reg>` annotations and calling the callee by name -- tools/build/patch.py
 * marshals the stack args into the right registers in a generated thunk. That
 * is the project's sanctioned mechanism (CLAUDE.md: "Register-arg callees must
 * be added to kb.json with @<reg> and called by name"; "No Inline ASM").
 *
 * These shims previously used GCC-style `asm volatile`, which cl.exe 13.10
 * cannot parse, so this whole translation unit failed to compile under VC71
 * and its 13 functions were never byte-scored. The asm was also unsound: an
 * empty `asm volatile("" : "+a"(x))` followed by a separate call does not
 * guarantee the value survives in EAX to the call instruction. */
#ifndef HALO_CAMERA_INTERNAL_H
#define HALO_CAMERA_INTERNAL_H

#include "../math/real_math.h"
#include "../math/real_math_inlines.h"

/* Scripted fixed-position camera, filled by static_camera_new (0x8d3a0) and
 * consumed once by static_camera_update (0x8d410). */
typedef struct {
  real_point3d position;                   ///< offset=0x00
  int32_t location;                        ///< offset=0x0c
  real_vector3d forward;                   ///< offset=0x10
  real_vector3d up;                        ///< offset=0x1c
  real field_of_view;                      ///< offset=0x28
  int32_t timer;                           ///< offset=0x2c
  uint32_t flags;                          ///< offset=0x30
  boolean initialized;                     ///< offset=0x34
} static_camera_t;

/* Per-player first-person camera state: the field of view last applied. */
typedef struct {
  real field_of_view;                      ///< offset=0x00
} first_person_camera_t;

typedef struct {
  int16_t local_player_index;              ///< offset=0x00
} first_person_camera_action_t;

/* 0x86de0  mode-0/1 camera set — now ported. */
static __inline void camera_internal_set_mode_0_1(int16_t player, int reset,
                                                  int mode_flags)
{
  director_set_player_camera_normal(player, (char)reset, (char)mode_flags);
}

/* 0x86fa0  mode-2 camera set — now ported. mode_flags arg is unused by the
 * scripted-camera setter (only reset_flag matters). */
static __inline void camera_internal_set_mode_2(int16_t player, int reset,
                                                int mode_flags)
{
  (void)mode_flags;
  director_set_player_camera_scripted(player, (char)reset);
}

/* 0x87050  mode-4 camera set — now ported. */
static __inline void camera_internal_set_mode_4(int reset_flag, int16_t player,
                                                int mode_flags)
{
  director_apply_replay_mode_for_player((char)reset_flag, player,
                                        (char)mode_flags);
}

/* 0x86600  per-player data init — now a real C function. The shim stays as
 * a thin forwarder so director.c call sites don't change when more helpers
 * get ported. */
static __inline void camera_internal_init_player(int16_t player)
{
  director_init_player_cameras(player);
}

/* 0x865a0  install camera fn for player (@si=player, cdecl: fn, reset_byte) */
static __inline void camera_internal_set_camera_fn(int16_t player,
                                                   void *camera_fn,
                                                   char reset_top_timer)
{
  director_set_camera(player, (int)camera_fn, reset_top_timer != 0);
}

/* 0x86a50  cycle camera mode (@eax=player, @ebx=mode_table, cdecl: count).
 * 0x86a50 reads the table as an int16_t array (`movswl (%ebx,%edx,2)`) and
 * the count as a 16-bit signed stack arg (`movswl 0x8(%ebp),%ecx`). */
static __inline void camera_internal_cycle_mode(int player, int16_t *mode_table,
                                                int16_t count)
{
  director_rotate_cameras(player, mode_table, count);
}

/* 0x86be0  director_choose_game_perspective (@eax=player, @bl=force_flag, no stack).
 * 0x86be0 only ever reads BL (five `testb %bl,%bl`, never written first), so
 * the annotation is @<bl> and the generated thunk loads the byte. */
static __inline void camera_internal_reevaluate(int16_t player, char force_flag)
{
  director_choose_game_perspective(player, force_flag);
}

/* 0x86670  per-player look/walk integrator (@ax=player, cdecl: mode_flags
 * flags dword, fwd float). Updates per-player camera-state variables;
 * now ported. */
static __inline void camera_internal_integrate(int16_t player,
                                               uint8_t mode_flags, float fwd)
{
  director_process_variables(player, mode_flags, fwd);
}

/* 0x87110  build camera input — now ported. */
static __inline uint8_t camera_internal_poll_input(void *out_buf, int player)
{
  return (uint8_t)director_compute_camera_input(out_buf, player);
}

/* Camera-command validity test behind the "Invalid camera command." asserts
 * (observer_set_camera 0x8acb0, first_person_camera_for_unit_and_vector
 * 0x88d50).  Both expand it inline in this order: forward/up axes, position
 * and offset components within +-5000, velocity, depth within [0, 5000],
 * field of view within [0.001, pi/2], timer within [0, 3600].  Only commands
 * carrying _observer_command_valid_bit are checked. */
#define valid_world_real(value) \
  (valid_real(value) && (value) >= -5000.0f && (value) <= 5000.0f)
#define camera_command_valid(command)                                      \
  (!((command)->flags & FLAG(_observer_command_valid_bit)) ||              \
   (valid_real_normal3d_perpendicular(&(command)->forward.i,              \
                                      &(command)->up.i) &&                 \
    valid_world_real((command)->position.x) &&                             \
    valid_world_real((command)->position.y) &&                             \
    valid_world_real((command)->position.z) &&                             \
    valid_world_real((command)->offset.i) &&                               \
    valid_world_real((command)->offset.j) &&                               \
    valid_world_real((command)->offset.k) &&                               \
    real_vector3d_valid(&(command)->velocity.i) &&                         \
    valid_real((command)->depth) && (command)->depth >= 0.0f &&            \
    (command)->depth <= 5000.0f &&                                         \
    valid_real((command)->field_of_view) &&                                \
    (command)->field_of_view >= 0.001f &&                                  \
    (command)->field_of_view <= 1.57079637f /* pi/2 */ &&                  \
    valid_real((command)->timer) && (command)->timer >= 0.0f &&            \
    (command)->timer <= 3600.0f))

/* csprintf arguments of the "Invalid camera command." message, in its
 * F/U/P/O/D/V/FOV/T/FL order. */
#define CAMERA_COMMAND_INVALID_FORMAT                                      \
  "Invalid camera command.\n"                                              \
  "F: (%f, %f, %f) U: (%f, %f, %f)\n"                                      \
  "P: (%f, %f, %f) O: (%f, %f, %f)\n"                                      \
  "D: %f V: (%f, %f, %f), FOV: %f, T: %f, FL: %ld"
#define CAMERA_COMMAND_INVALID_ARGUMENTS(command)                          \
  (command)->forward.i, (command)->forward.j, (command)->forward.k,        \
    (command)->up.i, (command)->up.j, (command)->up.k,                     \
    (command)->position.x, (command)->position.y, (command)->position.z,   \
    (command)->offset.i, (command)->offset.j, (command)->offset.k,         \
    (command)->depth, (command)->velocity.i, (command)->velocity.j,        \
    (command)->velocity.k, (command)->field_of_view, (command)->timer,     \
    (command)->flags


#endif
