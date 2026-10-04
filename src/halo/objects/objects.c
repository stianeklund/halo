#if defined(_MSC_VER) && !defined(__clang__)
#include <math.h>
/* VC71 lane keeps <math.h> (no x87_math.h intrinsic pragmas here); the two
 * narrowing helpers are no-ops for cl.exe, which narrows on its own. */
#define HALO_FLT_ROUNDTRIP(lv) ((void)0)
#define HALO_NARROW(e) (e)
typedef float x87_wide_t;
#else
#include "../../x87_math.h"
#ifdef HALO_RNG_TRACE
#include "halo/math/rng_trace.h"
#endif
float sinf(float x);
float cosf(float x);
double atan2(double y, double x);
float sqrtf(float x);
float fabsf(float x);
double fabs(double x);
double pow(double x, double y);
#endif

/* Lift-artifact warnings suppressed file-wide. The original per-function
 * clang-diagnostic push/pop pairs were scrambled by maintain.py address
 * sorting; consolidated to file scope (diagnostic-only, no codegen effect). */
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wuninitialized"

#include "objects.h"
/* Address-based function call macros for callees without proper kb.json entries
 */
#define XCALL(addr, type) ((type)(addr))
#define CALL_FUN_0013d640(a, b) XCALL(0x13d640, int (*)(int, int))(a, b)
#define CALL_FUN_00084a10(a) XCALL(0x84a10, char (*)(float *))(a)
#define CALL_game_time_get_rate() XCALL(0xb5cc0, float (*)(void))()
/* real_rgb_color_brightness (0x7a750) — call by name, no XCALL needed */
/* Wrappers retired in favour of named kb.json calls:
 *   0x1396e0 light_disconnect_from_map        0x13aed0 light_reconnect_to_map
 *   0x140cc0 object_delete      0x13fb30 object_activate
 *   0x143c80 object_new         0x13fc20 object_placement_data_new
 *   0x1196d0 datum_delete
 *   0x1919a0 cluster_partition_remove_object
 *   0x0b5aa0 game_time_get
 *   0x0b5c30 game_time_get_paused
 *   0x08fa40 profile_enter_private
 *   0x08fac0 profile_exit_private
 *   0x0b65c0 player_control_get_aiming_unit_index
 *   0x0b6740 player_control_get_unit_camera_info
 *   0x0b7e30 player_control_get_facing_angles
 *   0x1a9240 unit_get_camera_position
 * Unused wrappers removed (no call sites anywhere in src/):
 *   0x021fb0 valid_real_normal3d
 *   0x0a16b0 valid_real_point3d
 *   0x0d1c90 real_argb_color_to_pixel32
 *   0x0f6d00 valid_real_matrix4x3 (one-argument form)
 *   0x119320 datum_get
 *   0x1193f0 data_verify
 *   0x1198f0 data_next_index
 *   0x143ae0 object_set_position
 *   0x1812b0 FUN_001812b0
 *   0x181410 FUN_00181410
 *   0x189320 FUN_00189320 (five-argument form)
 *   0x1ba1f0 tag_get_name
 *   0x1bf570 xbox_texture_cache_get_hardware_format
 *   0x1d9260 qsort (wrapper was typed as an fprintf-style call)
 *   0x1d9e59 crt_fopen
 * The wrappers' casts were never checked against those declarations.
 *
 * Retired third pass (typed pointers now cast explicitly at the call site):
 *   0x07c270 FUN_0007c270
 *   0x07c490 FUN_0007c490
 *   0x085b60 dead_camera_new
 *   0x08f390 error
 *   0x099530 real_a_rgb_color_to_pixel32
 *   0x0ddb90 first_person_weapon_get_marker_by_name_render
 *   0x10a710 transition_function_evaluate
 *   0x10bbc0 vectors3d_from_euler_angles3d
 *   0x013010 normalize3d
 *   0x123470 animation_get_root_matrix
 *   0x138ee0 FUN_00138ee0
 *   0x1390d0 FUN_001390d0
 *   0x1403a0 object_get_function_value
 *   0x180570 FUN_00180570
 *   0x180660 FUN_00180660
 *   0x180770 FUN_00180770
 *   0x180b10 compress_real_vector3d_to_int32_clamp
 *   0x181670 FUN_00181670
 *   0x184e50 rendered_cluster_get
 *   0x0f6d00 valid_real_matrix4x3 (zero-argument form; dropped &matrix)
 *   0x08e2f0 system_exit (wrapper was misnamed thunk_FUN_001029a0)
 *   0x084a70 valid_real_normal3d_perpendicular
 *   0x089240 first_person_camera_fake (kb decl fixed: 2 stack args)
 *   0x085c80 dead_camera_update (kb decl fixed: 3 stack args)
 *   Both camera decls read from the caller PUSHes at 0x8566b/0x856b3.
 *   (0x084a10 now returns bool; only scripted_camera_update keeps the wrapper)
 *   0x08e370 system_milliseconds (wrapper called 0x1d0581, wrong callee)
 *   0x13fea0 object_get_attachment_marker_name, both forms. The
 *            "5-arg" form was a nested call: MSVC pushes the outer
 *            first_person_weapon_adjust_light args before the inner
 *            call, so only the last two PUSHes belong to 0x13fea0.
 *            Marked noinline: inlining it costs lights_preprocess_scene.
 *   0x140f10 object_get_marker_by_name
 *   0x181900 rasterizer_lens_flare_submit_for_cluster (lens-flare submit for a rendered cluster)
 *   0x189150 FUN_00189150
 *   0x196c90 FUN_00196c90 (structure visibility object search)
 *   0x198cb0 structure_test_vector
 *   (0x0b5cc0 game_time_get_speed stays wrapped: calling it directly
 *   passes the gate but gives back 10 of scripted_camera_update's bytes)
 *
 * Raw-byte gate for this batch against the previous commit:
 * lights_preprocess_scene +48, lights_distant_lighting_at_point +5,
 * and no function lost bytes.
 * 0x13fea0, 0x140f10 and 0x181900 were later renamed in kb.json to
 * object_get_attachment_marker_name, object_get_marker_by_name and
 * rasterizer_lens_flare_submit_for_cluster; the list above uses the
 * new names.
 *
 *
 *
 * Why the line count matters: the byte-campaign doctrine records that
 * objects.c edits which change the file's line count make the raw-byte
 * gate report false drops in later, unedited functions (__LINE__-stamped
 * asserts). Keep this block's height equal to the lines it replaced
 * until the whole wrapper block is gone; then remove the XCALL
 * definition and rebalance the count in one commit, checked with the
 * same gate.
 *
 * The three wrappers left (0x13d640, 0x084a10, 0x0b5cc0) are kept only
 * because calling them by name loses bytes. 0x13d640 and 0x084a10 are
 * defined in this file; a by-name call from scripted_camera_update
 * costs 14 bytes even with noinline, so that one site stays wrapped.
 *
 * 0x1396e0/0x13aed0/0x1a9240 now carry their recovered names
 * (light disconnect/reconnect, unit camera position).
 * This note keeps the file's line count, and so every later
 * __LINE__-stamped assert, unchanged.
 */
/*
 * real_vector3d_valid — check whether a 3D vector contains only finite floats.
 *
 * Tests the IEEE 754 exponent field (bits 23..30) of each of the three
 * components. If all three have an exponent != 0xFF (i.e. the value is
 * neither NaN nor Infinity), returns 1 (valid). Otherwise returns 0.
 *
 * Leaf function, no callees. Reinterprets floats as uint32 via pointer cast.
 *
 * Confirmed: AND with 0x7f800000 and CMP to 0x7f800000 for each component.
 * Confirmed: returns 1 only if all three pass; returns 0 on first failure.
 */
static __inline bool valid_real(float n)
{
  return (*(uint32_t *)&n & 0x7f800000) != 0x7f800000;
}

/* 0x84a10 */
bool real_vector3d_valid(float *vector)
{
  return valid_real(vector[0]) && valid_real(vector[1]) &&
         valid_real(vector[2]);
}

#define valid_ranged_real(x, lo, hi) (valid_real(x) && (x) >= (lo) && (x) <= (hi))
#define valid_world_real(x) valid_ranged_real(x, REAL_NEG_5000_POOL, REAL_5000_POOL)
#define valid_world_real3d(v, a, b, c) \
  (valid_world_real((v).a) && valid_world_real((v).b) && valid_world_real((v).c))
#define valid_camera_command(c, velocity_valid) \
  (valid_real_normal3d_perpendicular(&(c)->forward.i, &(c)->up.i) && \
   valid_world_real3d((c)->position, x, y, z) && \
   valid_world_real3d((c)->offset, i, j, k) && \
   velocity_valid(&(c)->velocity.i) && \
   valid_ranged_real((c)->depth, REAL_ZERO_POOL, REAL_5000_POOL) && \
   valid_ranged_real((c)->field_of_view, REAL_0_001_POOL, REAL_HALF_PI_POOL) && \
   valid_ranged_real((c)->timer, REAL_ZERO_POOL, REAL_3600_POOL))
#define CAMERA_COMMAND_FORMAT \
  "Invalid camera command.\nF: (%f, %f, %f) U: (%f, %f, %f)\nP: (%f, %f, " \
  "%f) O: (%f, %f, %f)\nD: %f V: (%f, %f, %f), FOV: %f, T: %f, FL: %ld"
#define CAMERA_COMMAND_ARGS(c, flags) \
  (double)(c)->forward.i, (double)(c)->forward.j, (double)(c)->forward.k, \
  (double)(c)->up.i, (double)(c)->up.j, (double)(c)->up.k, \
  (double)(c)->position.x, (double)(c)->position.y, (double)(c)->position.z, \
  (double)(c)->offset.i, (double)(c)->offset.j, (double)(c)->offset.k, \
  (double)(c)->depth, (double)(c)->velocity.i, (double)(c)->velocity.j, \
  (double)(c)->velocity.k, (double)(c)->field_of_view, (double)(c)->timer, \
  (flags)
/* 0x84a70 — valid_real_normal3d_perpendicular: check whether two 3D vectors
 * are each valid unit normals AND are perpendicular to each other.
 *
 * First validates each vector individually via valid_real_normal3d (checks
 * that squared length is within 0.001 of 1.0 and not NaN/infinity).
 * Then computes dot(a, b) and returns true only if the dot product is
 * a valid finite float with fabsf(dot) < 0.001f (i.e., nearly perpendicular).
 *
 * Confirmed: CALL 0x21fb0 twice (valid_real_normal3d) for each input vector.
 * Confirmed: FLD/FMUL/FADDP computes dot(a, b) = a[0]*b[0]+a[1]*b[1]+a[2]*b[2].
 * Confirmed: FSTS [EBP-4] stores dot without popping; integer NaN/inf check
 *   on exponent bits (AND 0x7f800000, CMP 0x7f800000) before the FABS compare.
 * Confirmed: FABS / FCOMPL double ptr [0x2549d8] compares against
 * (double)0.001f. Confirmed: FNSTSW AX / TEST AH,5 / JP pattern — returns true
 * when fabsf(dot) < 0.001f. */
bool valid_real_normal3d_perpendicular(float *a, float *b)
{
  float dot;
  char ok;
  ok = (char)valid_real_normal3d(a);
  if (ok && (ok = (char)valid_real_normal3d(b), ok) &&
      (dot = a[2] * b[2] + a[1] * b[1] + a[0] * b[0],
       (*(unsigned int *)&dot & 0x7f800000) != 0x7f800000) &&
      fabs(dot) < 0.001) {
    return 1;
  }
  return 0;
}

/* ====================================================================
 * FINAL 10 OBJECTS.OBJ FUNCTIONS
 * ==================================================================== */

/* 0x84ae0 — bored_camera_update: generates random camera positions/angles
 * when the game is idle (attract mode). Validates the resulting camera
 * parameters and asserts on invalid values.
 *
 * Source: c:\halo\SOURCE\camera\bored_camera.c
 * 3 cdecl params: camera_state, unit_datum, result buffer.
 */
/* 0x84ae0 */
void bored_camera_update(bored_camera_t *camera, camera_control_t *controls,
                         camera_command_t *result)
{
  uint32_t now;
  int threshold;
  int unit_index;
  float *facing;
  int shots;
  player_control_unit_camera_info_t camera_info;
  real_point3d camera_position;
  float angles[2];
  float field_of_view;
  int timer_milliseconds;

  now = (uint32_t)system_milliseconds();
  if (camera == NULL) {
    display_assert("camera", "c:\\halo\\SOURCE\\camera\\bored_camera.c", 0x33,
                   1);
    system_exit(-1);
  }
  if (result == NULL) {
    display_assert("result", "c:\\halo\\SOURCE\\camera\\bored_camera.c", 0x34,
                   1);
    system_exit(-1);
  }
  camera->timer_milliseconds += camera->last_update_milliseconds - now;
  camera->last_update_milliseconds = now;
  threshold = camera->boredom_count;
  if (3 < threshold) {
    threshold = 3;
  }
  if (camera->timer_milliseconds < threshold * 1000) {
    unit_index = player_control_get_aiming_unit_index(controls->local_player_index);
    player_control_get_unit_camera_info(controls->local_player_index, &camera_info);
    result->position = camera_info.position;
    if (unit_index != -1) {
      if (camera_info.camera->unit_camera_tracks.count != 0) {
        tag_block_get_element(&camera_info.camera->unit_camera_tracks, 0, 0x1c);
      }
      facing = (float *)player_control_get_facing_angles(controls->local_player_index);
      angles[0] = facing[0];
      angles[1] = facing[1];
      unit_get_camera_position(unit_index, &camera_position.x);
      /* angles[] is one {yaw, pitch} pair: angles_to_vector reads both. */
      angles[1] = random_real_range((int *)random_math_get_local_seed_address(),
                                    -1.0995574f, 0.39269909f);
      angles[0] += random_real_range((int *)random_math_get_local_seed_address(),
                                     -0.78539819f, 0.78539819f) +
                   3.14159265f;
      angles_to_vector(&result->forward.i, angles);
      observer_up_from_forward(&result->forward.i, &result->up.i);
      field_of_view = random_real_range(
        (int *)random_math_get_local_seed_address(), 0.52359879f, 1.3962634f);
      result->field_of_view = field_of_view;
      result->depth = random_real_range(
        (int *)random_math_get_local_seed_address(), 1.0f, 6.0f);
      result->velocity = *(real_vector3d *)global_zero_vector_ptr;
      shots = camera->boredom_count + 1;
      if (3 < shots) {
        shots = 3;
      }
      timer_milliseconds = shots * 10000;
      camera->timer_milliseconds = timer_milliseconds;
      result->flags = 1;
      result->timer = (float)timer_milliseconds;
      camera->boredom_count++;
      if ((result->flags & 1) != 0 && !valid_camera_command(result, real_vector3d_valid)) {
        display_assert(
          (const char *)csprintf(error_string_buffer, CAMERA_COMMAND_FORMAT,
                                 CAMERA_COMMAND_ARGS(result, result->flags)),
          "c:\\halo\\SOURCE\\camera\\bored_camera.c", 0x5f, 1);
        system_exit(-1);
      }
    }
  }
}

/* scripted_camera_enable (0x84fe0) — Set the bored-camera enable flag and mark
 * the camera state dirty so it will be re-evaluated this tick. Object:
 * objects.obj / source: bored_camera.c (shared DAT_002ee5a0..dc global block
 * with the confirmed bored_camera.c assert at 0x84ae0).
 *
 * Confirmed: MOV AL,[param_1]; MOV [0x2ee5a0],AL; MOV byte ptr [0x2ee5a1],1.
 * Renamed scripted_camera_enable reverted: name contradicted its own cited
 * source evidence (bored_camera.c, not camera_scripting.c).
 */
void scripted_camera_enable(unsigned char enabled)
{
  scripted_camera_globals.enabled = enabled;
  scripted_camera_globals.first_update = 1;
}

/* scripted_camera_set_animation (0x85000 / objects.obj) — select a scripted
 * animation camera by name from an 'antr' tag and update the camera globals.
 *
 * Confirmed: tag block at +0x74, element stride 0xb4, name at element+0,
 * camera duration at element+0x22, and the matched index at +0x2ee5dc.
 * Renamed scripted_camera_set_animation reverted: pending source-file
 * attribution (bored_camera.c vs camera_scripting.c, no direct xref here).
 */
void scripted_camera_set_animation(int animation_tag, const char *camera_name)
{
  animation_graph_t *graph;
  tag_block *animations;
  animation_t *animation;
  short index;

  if (animation_tag == -1)
    return;

  graph = (animation_graph_t *)tag_get(ANIMATION_GRAPH_TAG, animation_tag);
  if (graph->nodes.count != 1)
    return;

  animations = &graph->animations;
  index = 0;
  if (animations->count > 0) {
    do {
      animation = (animation_t *)tag_block_get_element(animations, (int)index,
                                                       sizeof(animation_t));
      if (crt_stricmp(camera_name, animation->name) == 0) {
        scripted_camera_globals.camera_point_index = -1;
        scripted_camera_globals.relative_object_index = -1;
        scripted_camera_globals.mode = 1;
        scripted_camera_globals.first_update = 1;
        scripted_camera_globals.animation_index = index;
        scripted_camera_globals.animation_graph_index = animation_tag;
        scripted_camera_globals.field_of_view =
          SCRIPTED_CAMERA_DEFAULT_FIELD_OF_VIEW;
        scripted_camera_globals.timer =
          (float)((int)animation->frame_count / TICKS_PER_SECOND_INT);
        return;
      }
      index++;
    } while ((int)index < animations->count);
  }
}

/* scripted_camera_set_first_person (0x850d0) — Switch to first-person camera
 * mode 2 for the given unit handle, or report an error if the handle is -1.
 * Object: objects.obj / source: bored_camera.c (shared DAT_002ee5a0..dc
 * global block with the confirmed bored_camera.c assert at 0x84ae0).
 *
 * Confirmed: CMP [param_1],-1; JE error_path; MOV [0x2ee5a2],2;
 * MOV [0x2ee5a1],1; MOV [0x2ee5d4],param_1; RET.
 * Renamed scripted_camera_set_first_person reverted: name contradicted its
 * own cited source evidence (bored_camera.c, not camera_scripting.c).
 */
void scripted_camera_set_first_person(int unit_index)
{
  if (unit_index != -1) {
    scripted_camera_globals.mode = 2;
    scripted_camera_globals.first_update = 1;
    scripted_camera_globals.relative_object_index = unit_index;
    return;
  }
  error(2, "cannot set first person camera on a unit that doesn't exist.");
}

/* scripted_camera_set_dead (0x85110) — Switch to first-person camera mode 3 for
 * the given unit handle, or report an error if the handle is -1. Object:
 * objects.obj / source: bored_camera.c
 *
 * Confirmed: identical to scripted_camera_set_first_person but stores 3 in
 * DAT_002ee5a2.
 */
void scripted_camera_set_dead(int unit_index)
{
  if (unit_index != -1) {
    scripted_camera_globals.mode = 3;
    scripted_camera_globals.first_update = 1;
    scripted_camera_globals.relative_object_index = unit_index;
    return;
  }
  error(2, "cannot set first person camera on a unit that doesn't exist.");
}

/* scripted_camera_object_is_first_person_camera (0x85150) — Check if
 * first-person camera mode 2 is active for the given unit handle. Returns 1 if
 * the bored-camera is enabled, mode is 2, and the stored unit handle matches
 * object_index; otherwise 0. */
int scripted_camera_object_is_first_person_camera(int object_index)
{
  if (scripted_camera_globals.enabled && scripted_camera_globals.mode == 2 &&
      scripted_camera_globals.relative_object_index == object_index) {
    return 1;
  }
  return 0;
}

/* scripted_camera_set (0x85180) — Configure camera globals from a
 * cutscene-camera point in the scenario, with transition_time in ticks and
 * an optional relative object. Triggers director_update and
 * observer_update. Object: objects.obj / source: bored_camera.c
 *
 * Confirmed: CALL global_scenario_get; CALL
 * tag_block_get_element(+0x4f0,param_1,0x68); stores to 0x2ee5a1..0x2ee5d4;
 * CALL vectors3d_from_euler_angles3d; float compare for default speed
 * (0x3f9c61aa); CALL director_update(0); CALL observer_update(0x38d1b717).
 */
void scripted_camera_set(short camera_point_index, short transition_time, int relative_object_index)
{
  scenario_cutscene_camera_point_t *point;

  point = (scenario_cutscene_camera_point_t *)tag_block_get_element(
    &global_scenario_get()->cutscene_camera_points, (int)camera_point_index,
    sizeof(scenario_cutscene_camera_point_t));
  scripted_camera_globals.mode = 0;
  scripted_camera_globals.first_update = 1;
  scripted_camera_globals.camera_point_index = camera_point_index;
  scripted_camera_globals.point = point->position;
  vectors3d_from_euler_angles3d(&scripted_camera_globals.forward.i,
                                &scripted_camera_globals.up.i,
                                point->orientation);
  if (point->field_of_view != REAL_ZERO_POOL) {
    scripted_camera_globals.field_of_view = point->field_of_view;
  } else {
    scripted_camera_globals.field_of_view =
      SCRIPTED_CAMERA_DEFAULT_FIELD_OF_VIEW;
  }
  scripted_camera_globals.timer = (float)((int)transition_time / TICKS_PER_SECOND_INT);
  scripted_camera_globals.relative_object_index = relative_object_index;
  director_update(0.0f);
  observer_update(0.0001f);
}

/* Forwards to scripted_camera_set with no relative object. */
void scripted_camera_set_absolute(short camera_point_index, short transition_time)
{
  scripted_camera_set(camera_point_index, transition_time, -1);
  return;
}

/* scripted_camera_set_camera_point_relative (0x85280 / objects.obj) — set
 * scripted camera globals from position, forward, up, speed, time, and an
 * optional unit handle.
 *
 * Confirmed: the zero-speed comparison uses 0x2533c0; the fallback speed is
 * the binary constant 0x3f9c61aa, and observer_update receives 0x38d1b717.
 */
void scripted_camera_set_camera_point_relative(float *position, float *forward,
                                               float *up, float field_of_view,
                                               short transition_time, int relative_object_index)
{
  scripted_camera_globals.mode = 0;
  scripted_camera_globals.camera_point_index = -1;
  scripted_camera_globals.point = *(real_point3d *)position;
  scripted_camera_globals.forward = *(real_vector3d *)forward;
  scripted_camera_globals.up = *(real_vector3d *)up;
  if (field_of_view != 0.0f) {
    scripted_camera_globals.field_of_view = field_of_view;
  } else {
    scripted_camera_globals.field_of_view = SCRIPTED_CAMERA_DEFAULT_FIELD_OF_VIEW;
  }
  scripted_camera_globals.timer = (float)((int)transition_time / TICKS_PER_SECOND_INT);
  scripted_camera_globals.relative_object_index = relative_object_index;
  director_update(0.0f);
  observer_update(0.0001f);
}

/* scripted_camera_set_camera_point_absolute (0x85350 / objects.obj) — scripted
 * camera helper with no associated unit handle. */
void scripted_camera_set_camera_point_absolute(float *position, float *forward,
                                               float *up, float field_of_view,
                                               short transition_time)
{
  scripted_camera_set_camera_point_relative(position, forward, up, field_of_view, transition_time,
                                            -1);
}

/*
 * scripted_camera_time (0x853a0 / objects.obj) — convert the cutscene-camera
 * time (seconds, at 0x2ee5a8) back to a tick count by multiplying by 30.0 and
 * truncating to int.
 *
 * The camera time global at 0x2ee5a8 is stored as ticks/30 (see
 * scripted_camera_set, which writes (float)(transition_time / 30)); this reverses that
 * to recover ticks.
 *
 * Confirmed: FLD [0x2ee5a8]; FMUL [0x253394 = 30.0f]; JMP _ftol2 (tail-call).
 * Confirmed: return value is the truncated int product (EAX from _ftol2).
 */
int scripted_camera_time(void)
{
  return (int)(scripted_camera_globals.timer * TICKS_PER_SECOND);
}

/* 0x853c0 — camera_scripting_update: handles scripted camera with a 4-case
 * switch on the camera scripting mode (DAT_002ee5a2). Includes orbit camera
 * math with atan2/sin/cos and the same massive camera validation block.
 *
 * Source: c:\halo\SOURCE\camera\camera_scripting.c
 * 3 cdecl params.
 */
/* 0x853c0 */
void scripted_camera_update(int camera, camera_control_t *controls,
                            camera_command_t *result)
{
  real_vector3d *forward;
  int frame_index;
  short frame_count;
  float value;
  short frame;
  animation_t *animation;
  object_data_t *object;
  float yaw;
  float sine;
  real_matrix4x3 matrix;
  float offset_x;
  float offset_y;
  float focus_x;
  float focus_y;
  float focus_z;
  float speed;

  focus_x = global_origin3d_ptr->x;
  focus_y = global_origin3d_ptr->y;
  focus_z = global_origin3d_ptr->z;
  speed = (float)CALL_game_time_get_rate();
  result->flags = 8;
  if (game_time_get_paused()) {
    result->flags |= 0x20;
  } else {
    result->flags &= ~0x20;
  }
  switch (scripted_camera_globals.mode) {
  case 0:
    if (scripted_camera_globals.relative_object_index != -1) {
      object = (object_data_t *)CALL_FUN_0013d640(
        scripted_camera_globals.relative_object_index, -1);
      if (object == NULL)
        break;
      focus_x = object->unk_80;
      focus_y = object->unk_84;
      focus_z = object->unk_88;
    }
    value = REAL_ZERO_POOL;
    if (speed != REAL_ZERO_POOL) {
      value = scripted_camera_globals.timer / speed;
    }
    result->timer = value;
    result->field_of_view = scripted_camera_globals.field_of_view;
    forward = &result->forward;
    forward->i = scripted_camera_globals.forward.i;
    result->forward.j = scripted_camera_globals.forward.j;
    result->forward.k = scripted_camera_globals.forward.k;
    result->up = scripted_camera_globals.up;
    if (scripted_camera_globals.relative_object_index != -1) {
      /* Orbit: rotate the stored offset about the focus by the camera yaw. */
      yaw = (float)atan2((double)result->forward.j, (double)forward->i);
      value = scripted_camera_globals.point.z * forward->k +
              scripted_camera_globals.point.y * forward->j +
              scripted_camera_globals.point.x * forward->i;
      if (value > REAL_ZERO_POOL) {
        value = REAL_ZERO_POOL;
      }
      result->depth = -value;
      result->position.x = focus_x;
      result->position.y = focus_y;
      result->position.z = focus_z;
      offset_x = scripted_camera_globals.point.x - value * forward->i;
      offset_y = scripted_camera_globals.point.y - value * forward->j;
      value = scripted_camera_globals.point.z - value * forward->k;
      result->field_54[0] = 0.0f;
      result->field_4c[0] = 1;
#if defined(_MSC_VER) && !defined(__clang__)
      sine = (float)sin((double)yaw);
#else
      sine = x87_fsin(yaw);
#endif
      result->flags |= 1;
#if defined(_MSC_VER) && !defined(__clang__)
      yaw = (float)cos((double)yaw);
#else
      yaw = x87_fcos(yaw);
#endif
      result->offset.i = offset_x * yaw + offset_y * sine;
      result->offset.j = offset_x * sine - offset_y * yaw;
      result->offset.k = value;
    } else {
      result->position = scripted_camera_globals.point;
      result->flags |= 1;
    }
    break;
  case 1:
    animation = (animation_t *)tag_block_get_element(
      &((animation_graph_t *)tag_get(
          ANIMATION_GRAPH_TAG, scripted_camera_globals.animation_graph_index))
         ->animations,
      (int)scripted_camera_globals.animation_index, sizeof(animation_t));
    frame_count = animation->frame_count;
    frame = (short)(int)((float)frame_count -
                         scripted_camera_globals.timer * 30.0f);
    if (frame < 0) {
      frame_index = 0;
    } else {
      frame_index = (int)frame;
      if (frame_count - 1 < frame) {
        frame_index = frame_count - 1;
      }
    }
    animation_get_root_matrix(0, animation, frame_index, &matrix);
    result->forward = *(real_vector3d *)&matrix.forward;
    result->up = *(real_vector3d *)&matrix.up;
    result->position.x = matrix.position.x;
    result->position.y = matrix.position.y;
    result->depth = 0.0f;
    result->timer = 0.0f;
    result->field_of_view = SCRIPTED_CAMERA_DEFAULT_FIELD_OF_VIEW;
    result->position.z = matrix.position.z;
    result->flags |= 1;
    break;
  case 2:
    if (CALL_FUN_0013d640(scripted_camera_globals.relative_object_index, 3)) {
      first_person_camera_fake(scripted_camera_globals.relative_object_index,
                               result);
    }
    break;
  case 3:
    if (CALL_FUN_0013d640(scripted_camera_globals.relative_object_index, 3)) {
      if (scripted_camera_globals.first_update) {
        dead_camera_new((void *)camera, controls->local_player_index,
                        scripted_camera_globals.relative_object_index);
      }
      dead_camera_update((void *)camera, controls, result);
    }
    break;
  }
  value = scripted_camera_globals.timer - speed * controls->seconds_elapsed;
  if (0.0f <= value) {
    scripted_camera_globals.timer = value;
  } else {
    scripted_camera_globals.timer = 0.0f;
  }
  scripted_camera_globals.first_update = 0;
  if ((result->flags & 1) != 0 &&
      !valid_camera_command(result, CALL_FUN_00084a10)) {
    display_assert(
      (const char *)csprintf(error_string_buffer, CAMERA_COMMAND_FORMAT,
                             CAMERA_COMMAND_ARGS(result, result->flags)),
      "c:\\halo\\SOURCE\\camera\\camera_scripting.c", 0x16e, 1);
    system_exit(-1);
  }
}

/*
 * game_engine_remap_equipment — equipment tag-index remapper for game engine
 * mode 3.
 *
 * Called from game_engine_remap_object_definition when the 'obje' type word is
 * 3 (equipment). Remaps or blocks equipment spawn based on:
 *   - weapon_definition_index_to_list_index returning 0xc or 0xd
 *   - The 'eqip' tag's type field at offset 0x308 vs variant flags at 0x456b18
 *   - Game engine type at 0x456b3c (3→list 0xd, 9→list 0xc, 10→none)
 *   - Complexity flags at 0x5aa720 (bit 3=team game, bit 2=split-screen)
 *   - Random probability gate against 0x2533e4 (team) or 0x26c744 (split)
 * Returns the remapped equipment tag index from game_globals block at +0x14c,
 * tag_index unchanged if not applicable, or -1 if blocked.
 *
 * Confirmed: PUSH 0x65716970 / CALL tag_get — 'eqip' tag lookup.
 * Confirmed: CALL 0xa9620 — weapon_definition_index_to_list_index, cdecl 1 arg.
 * Confirmed: CMP ESI,0xc / CMP ESI,0xd — two special list slot checks.
 * Confirmed: MOV AX,[EBX+0x308] / CMP AX,2 / CMP AX,3 — category field checks.
 * Confirmed: TEST byte ptr [0x456b18],0x8 / 0x10 — variant flags at 0x456b18.
 * Confirmed: MOV EAX,[0x456b3c] / SUB 3 / SUB 6 / DEC chain — game type remap.
 * Confirmed: AND EAX,4 / JNZ skip; SHR EDX,2 / AND DL,1 — conditional flag
 * gate. Confirmed: CALL 0x10b0d0 / CALL 0x10b240 / FCOMP — random probability
 * gate. Confirmed: CALL 0x18e450 / ADD EAX,0x14c / PUSH 0x10 — game_globals
 * block lookup.
 */
/* 0xadf70 */
int game_engine_remap_equipment(int tag_index)
{
  int list_index;
  void *tag;
  void *globals;
  void *element;
  float fRandom;

  if (tag_index == -1)
    tag = NULL;
  else
    tag = tag_get(0x65716970, tag_index);

  list_index = weapon_definition_index_to_list_index(tag_index);

  if (list_index != 0xc && list_index != 0xd) {
    if (tag == NULL)
      goto return_original;
    if (*(short *)((char *)tag + 0x308) == 2) {
      if ((*(unsigned char *)0x456b18 & 0x8) == 0)
        goto return_original;
      return -1;
    }
    if (*(short *)((char *)tag + 0x308) == 3) {
      if ((*(unsigned char *)0x456b18 & 0x10) != 0)
        return -1;
    }
    goto return_original;
  }

  switch (*(int *)0x456b3c) {
  case 3:
    list_index = 0xd;
    break;
  case 9:
    list_index = 0xc;
    break;
  case 10:
    list_index = -1;
    break;
  }

  if ((*(unsigned int *)0x5aa720 & 4) == 0 &&
      ((*(unsigned char *)0x456b18 >> 2) & 1) != 0) {
    list_index = -1;
  }

  if ((*(unsigned int *)0x5aa720 & 8) == 0) {
    if ((*(unsigned int *)0x5aa720 & 4) != 0) {
      fRandom =
        random_math_real((unsigned int *)get_global_random_seed_address());
      if (fRandom >= REAL_0_55_POOL)
        list_index = -1;
    }
  } else {
    fRandom =
      random_math_real((unsigned int *)get_global_random_seed_address());
    if (fRandom >= REAL_0_3_POOL)
      list_index = -1;
  }

  if (list_index == -1)
    return -1;

  globals = game_globals_get();
  element = tag_block_get_element((char *)globals + 0x14c, list_index, 0x10);
  return *(int *)((char *)element + 0xc);

return_original:
  return tag_index;
}

/*
 * game_engine_remap_object_definition — game-engine tag-index remapping
 * dispatch.
 *
 * When a game engine is active (*(int *)0x456b60 != 0) and tag_index is
 * valid (!= -1), reads the object type word from the 'obje' tag header
 * (first 2 bytes) and dispatches to the appropriate engine-specific
 * tag-remapping helper:
 *   type 1 (vehicle) → game_engine_remap_vehicle(tag_index)
 *   type 2 (weapon)  → game_engine_remap_weapon(tag_index)
 *   type 3 (?)       → game_engine_remap_equipment(tag_index)
 * Returns the (possibly remapped) tag index, or the original tag_index
 * if no engine is active, tag_index is -1, or the type is not 1/2/3.
 *
 * Confirmed: MOV EAX,[0x456b60] / TEST EAX,EAX — bool check on engine ptr.
 * Confirmed: PUSH ESI / PUSH 0x6f626a65 / CALL tag_get — 'obje' tag lookup.
 * Confirmed: MOV AX,[EAX] — first 2 bytes of tag data = object type word.
 * Confirmed: CMP AX,1 / CMP AX,2 / CMP AX,3 — three dispatch branches.
 * Confirmed: each branch PUSH ESI / CALL callee / ADD ESP,4; returns EAX.
 * Confirmed: fallthrough MOV EAX,ESI — returns original tag_index unchanged.
 * Note: callee decls carry wrong void return type; binary shows they return
 * int.
 */
int game_engine_remap_object_definition(int tag_index)
{
  short obj_type;
  short *tag_data;

  if ((*(int *)0x456b60 != 0) && (tag_index != -1)) {
    tag_data = (short *)tag_get(0x6f626a65, tag_index);
    obj_type = *tag_data;
    if (obj_type == 1) {
      return game_engine_remap_vehicle(tag_index);
    }
    if (obj_type == 2) {
      return game_engine_remap_weapon(tag_index);
    }
    if (obj_type == 3) {
      return game_engine_remap_equipment(tag_index);
    }
  }
  return tag_index;
}

/* game_engine_get_state_message / objects.obj -- determine respawn state for a
 * player. Returns the "HUD text was produced" flag in AL (original only ever
 * sets AL; see xor al,al at 0xae23e). hud_show_action_response (unported) draws
 * the buffer only when this returns nonzero. */
char game_engine_get_state_message(int param_1, int param_2, int param_3)
{
  int player;
  int respawn_state;
  int time;
  volatile int local_8;
  int field_74;
  char result;

  result = 0;
  if (!current_game_engine)
    goto done;

  player = (int)datum_get(*(data_t **)0x5aa6d4, param_1);
  field_74 = *(int *)(player + 0x74);
  if (field_74 >= 0x17 && field_74 <= 0x1a) {
    *(int *)(player + 0x74) = -1;
  }

  if (*(int *)(player + 0x34) == -1) {
    /* Dead player: choose a respawn/status HUD message. This is the
     * fall-through branch in the original (CMP [ESI+0x34],EBX; JNZ 0xae1f9
     * sends the live case out of line to 0xae1f9). */
    local_8 = 0;
    if (*(char *)(player + 0xd1) == '\x01') {
      respawn_state = 0x1b;
    } else if (game_engine_player_is_out_of_lives(param_1)) {
      respawn_state = 0x18;
    } else if (game_engine_is_player_leading(param_1)) {
      respawn_state = 0x17;
    } else if (*(int *)(player + 0x2c) > 0) {
      local_8 = *(int *)(player + 0x2c) / 30;
      respawn_state = 0x19;
    } else {
      respawn_state = 0x1a;
    }

    {
      void *vtable_fn = *(void **)((char *)current_game_engine + 0x64);
      if (vtable_fn != NULL) {
        char handled = ((char (*)(int, int, int, int, int))vtable_fn)(
          param_1, respawn_state, local_8, param_2, param_3);
        if (handled) {
          result = handled;
          goto done;
        }
      }
    }
    result = (char)game_engine_get_score_hud_text(
      param_1, respawn_state, local_8, (wchar_t *)param_2, param_3);
    goto done;
  }

  /* Live player: both aceb0 paths RETURN the dispatcher's AL ("text was
   * produced"), per original 0xae205-0xae21c and 0xae21d-0xae23d -- there is
   * no xor al,al before those rets. The unported caller
   * hud_show_action_response only draws the buffer when this returns nonzero
   * (test al,al at 0xd0931). */
  time = game_time_get();
  if (time < 0x1c2) {
    result = FUN_000aceb0(param_2, param_3, -1, param_1, 0x1d);
    goto done;
  }
  if (*(int *)(player + 0x74) != -1) {
    result = FUN_000aceb0(param_2, param_3, *(int *)(player + 0x78), param_1,
                          *(int *)(player + 0x74));
  }

done:
  return result;
}

/* Default player-win check used when no game-engine vtable slot 0x84 is set.
 * Returns 1 (won), 0 (not won), or -1 (invalid/undecided).
 *
 * VC71 whole-file score 2.3% is a MEASUREMENT ARTIFACT, not a lift defect:
 * game_engine_did_player_win_default is the LAST function in objects.obj's
 * delinked range, which batch_delink.py's compute_truncated_range()
 * deliberately ends at the last function's START (BFT COFF relocation-bug
 * workaround), truncating this body to 1 instruction in the reference. True
 * match is 92.9% (87/81 insns) against the per-function ref
 * delinked/functions/000ae250.obj (verified via `vc71_verify --function
 * game_engine_did_player_win_default` 2026-06-23). Do NOT bump the committed
 * whole-file floor or re-export objects.obj — both reintroduce the COFF bug /
 * a false regression. See [[reference_inline_delinked_export_when_live_down]].
 */
int game_engine_did_player_win_default(int param_1)
{
  int iVar2;
  int iVar3;
  int iVar4;
  char cVar1;
  int bVar7;
  int buf[7];
  int *puVar5;
  int *puVar6;

  if (*(char *)0x456b14 != '\0') {
    iVar2 = FUN_000a8130(0);
    iVar3 = FUN_000a8130(1);
    iVar4 = (int)datum_get(*(data_t **)0x5aa6d4, param_1);
    cVar1 = game_engine_teams_still_playing();
    if (cVar1 != '\0') {
      if (iVar2 == iVar3)
        goto done_minus1;
      bVar7 = (iVar2 <= iVar3);
    } else {
      cVar1 = (char)FUN_000abb90(0);
      bVar7 = (cVar1 == '\0') ? 1 : 0;
    }
    if (bVar7 == (int)0xffffffff)
      goto done_minus1;
    return (int)(*(unsigned int *)(iVar4 + 0x20) == (unsigned int)bVar7);
  }
  puVar5 = FUN_000abf50(buf, param_1);
  qmemcpy(buf, puVar5, sizeof(buf));
  if (((uint32_t)buf[6] & 0x80000000) <= 0 ||
      ((uint32_t)buf[6] & 0x7fffffff) != 0) {
    return (int)(((unsigned int)buf[6] & 0x7fffffff) == 0);
  }
done_minus1:
  return (int)0xffffffff;
}

/* glow_new (0x132fb0 / objects.obj / glow.c).
 *
 * Creates a glow widget for tag_index when the glow definition's mbit tag is
 * type 3. Call argument grouping follows 0x132fe2-0x13304a; the frame scale
 * is (element+0x0c - element+0x08) times the signed lookup-table value.
 *
 * Returns the new glow datum (EAX from [EBP-4] at 0x13307e, or ESI at
 * 0x133088), even when the bitmap is not type 3; -1 when tag_index is -1 or
 * the allocation fails. The widget layer stores this as the widget handle.
 */
int glow_new(int tag_index)
{
  int glow_datum = -1;
  int glow_widget;
  int glow_definition;
  int bitmap_definition;
  void *sequence_block;
  void *frame;
  void *lookup;

  if (tag_index != -1) {
    glow_datum = data_new_at_index(glow_data);
    if (glow_datum != -1) {
      glow_widget = (int)datum_get(glow_data, glow_datum);
      glow_definition = (int)tag_get(TAG_GROUP_GLW, tag_index);
      bitmap_definition =
        (int)tag_get(0x6269746d, *(int *)(glow_definition + 0x150));
      if (*(int16_t *)bitmap_definition == 3) {
        sequence_block = (void *)(bitmap_definition + 0x54);
        frame = tag_block_get_element(sequence_block, 0, 0x40);
        frame = tag_block_get_element((void *)((int)frame + 0x34), 0, 0x20);
        lookup =
          FUN_00077040(*(int *)(glow_definition + 0x150), 0, *(int16_t *)frame);
        *(int *)(glow_widget + 0x224) = tag_index;
        *(int16_t *)(glow_widget + 0x24c) =
          *(int16_t *)(glow_definition + 0x20);
        *(int16_t *)(glow_widget + 0x228) =
          (int16_t)((*(real *)((int)frame + 0xc) - *(real *)((int)frame + 8)) *
                    (int)*(int16_t *)((int)lookup + 4));
      }
    }
  }
  return glow_datum;
}

/* glow_delete (0x1330a0 / objects.obj / glow.c) — dispose a glow widget:
 * delete every particle datum in its list, then delete the widget's own
 * datum.
 *
 * Resolves the glow widget (datum_get(glow_data, widget_datum),
 * the same widget pool glow_render uses) and walks its particle list
 * rooted at glow_widget+0x250 (next-link at particle+0x5c, per-particle
 * datum handle at particle+4 -- glow.c's documented particle-node layout),
 * deleting each particle from the particle pool (glow_particle_data)
 * before deleting the widget itself from glow_data.
 *
 * Binary-confirmed order (0x1330c1-0x1330db): the next-link is read into
 * ESI BEFORE the delete call for the current particle, so the loop is safe
 * against the just-deleted node -- preserved here by capturing
 * next_particle before calling datum_delete.
 *
 * Single cdecl stack arg (widget_datum, [EBP+8]); no register args.
 * xrefs_to is a data reference (0x323594) -- a function-pointer table
 * dispose slot, not a direct call site.
 */
void glow_delete(int widget_datum)
{
  int glow_widget;
  int particle;
  int next_particle;

  glow_widget = (int)datum_get(glow_data, widget_datum);
  particle = *(int *)(glow_widget + 0x250);

  while (particle != 0) {
    next_particle = *(int *)(particle + 0x5c);
    datum_delete(glow_particle_data, *(int *)(particle + 4));
    particle = next_particle;
  }

  datum_delete(glow_data, widget_datum);
}

/* glow_trailing_particle_update_color (0x1330f0 / objects.obj / glow.c) —
 * age-based fade of a trailing glow particle (the PAL reference calls it
 * glow_trailing_particle_update_color). If glowdef flag 0x8 (byte +0x28) is
 * set, fade (+0x58) = 1 - age(+0x50) / lifetime(+0x52), pinned to [0,1];
 * otherwise fade = 1.
 *
 * glow_widget arrives in EAX and particle_ptr in ESI (both read uninitialized
 * at 0x1330f4 / 0x133110). Sole caller: glow_update (0x1348f5). The fade is
 * FST'd to +0x58 before the pin compares (0x133130), so the pin is a second
 * assignment of the same field, not a local.
 * Name: PAL-2342 glow.c (T2) — same slot in PAL's symbol order, between
 * glow_delete and glow_trailing_particle_update_size; tests the PAL
 * trailing_particles_fade_over_time bit (flags & 0x8).
 */
void glow_trailing_particle_update_color(int glow_widget, int particle_ptr)
{
  glow_particle *particle;
  glow_definition *definition;

  particle = (glow_particle *)particle_ptr;
  definition = (glow_definition *)tag_get(
    TAG_GROUP_GLW, ((glow_datum *)glow_widget)->definition_index);
  if ((definition->flags &
       FLAG(_glow_definition_trailing_particles_fade_over_time_bit)) != 0) {
    particle->fade = REAL_ONE_POOL -
                     (float)particle->ticks_in_existence / particle->lifetime;
    particle->fade =
      particle->fade < REAL_ZERO_POOL ?
        REAL_ZERO_POOL :
        (particle->fade > REAL_ONE_POOL ? REAL_ONE_POOL :
                                               particle->fade);
  } else {
    particle->fade = 1.0f;
  }
}

/* glow_trailing_particle_update_size (0x133170 / objects.obj / glow.c) — if
 * glowdef flag 0x10 is set, size (+0x24) = max(0, 1 - age/lifetime) *
 * initial size (+0x20).
 *
 * No call xrefs: glow_update inlines the same body. ABI is taken from the body
 * alone: EAX (glow_widget, 0x133174) and ESI (particle_ptr, 0x133190) are read
 * without being set. The max is `0 > scale ? 0 : scale` -- FLD 0; FCOMP
 * (0x1331aa-0x1331b0) puts the constant first. */
void glow_trailing_particle_update_size(int glow_widget, int particle_ptr)
{
  glow_particle *particle;
  glow_definition *definition;
  float scale;

  particle = (glow_particle *)particle_ptr;
  definition = (glow_definition *)tag_get(
    TAG_GROUP_GLW, ((glow_datum *)glow_widget)->definition_index);
  if ((definition->flags &
       FLAG(_glow_definition_trailing_particles_shrink_over_time_bit)) != 0) {
    scale = REAL_ONE_POOL -
            (float)particle->ticks_in_existence / particle->lifetime;
    scale = 0.0f > scale ? 0.0f : scale;
    particle->present_size = scale * particle->initial_size;
  }
}

/* glow_trailing_particle_update_velocity (0x1331d0 / objects.obj / glow.c) —
 * velocity of a trailing glow particle (the PAL reference calls it
 * glow_trailing_particle_update_velocity). If glowdef flag 0x20 is set,
 * velocity (+0x44) = max(0, 1 - age/lifetime) * initial velocity (+0x38);
 * otherwise velocity = initial velocity (3-dword copy, 0x13323f-0x133252).
 *
 * glow_widget arrives in EAX and particle_ptr in ECX (MOV ESI,ECX at
 * 0x1331d5). Sole caller: glow_update (0x134951).
 * Name: PAL-2342 glow.c (T2) — same slot in PAL's symbol order, between
 * glow_trailing_particle_update_size and _update_position; tests the PAL
 * trailing_particles_slow_over_time bit (flags & 0x20).
 */
void glow_trailing_particle_update_velocity(int glow_widget, int particle_ptr)
{
  glow_particle *particle;
  glow_definition *definition;
  float scale;

  particle = (glow_particle *)particle_ptr;
  definition = (glow_definition *)tag_get(
    TAG_GROUP_GLW, ((glow_datum *)glow_widget)->definition_index);
  if ((definition->flags &
       FLAG(_glow_definition_trailing_particles_slow_over_time_bit)) != 0) {
    scale = REAL_ONE_POOL -
            (float)particle->ticks_in_existence / particle->lifetime;
    scale = 0.0f > scale ? 0.0f : scale;
    particle->present_velocity[0] = scale * particle->initial_velocity[0];
    particle->present_velocity[1] = scale * particle->initial_velocity[1];
    particle->present_velocity[2] = scale * particle->initial_velocity[2];
  } else {
    *(vector3_t *)particle->present_velocity =
      *(vector3_t *)particle->initial_velocity;
  }
}

/* glow_trailing_particle_update_position (0x133260 / objects.obj / glow.c) —
 * integrate a trailing particle's position (+0x2c) by delta * velocity
 * (+0x44).
 *
 * No call xrefs: glow_update inlines the same body. ABI is taken from the body
 * alone: EAX (glow_widget, 0x133263) and ESI (particle_ptr, 0x133277) are read
 * without being set, and delta is the single cdecl stack arg ([EBP+8]). The
 * glowdef lookup's result is unused (it is never read after the CALL). */
void glow_trailing_particle_update_position(int glow_widget, int particle_ptr,
                                            float delta)
{
  glow_particle *particle;

  particle = (glow_particle *)particle_ptr;
  tag_get(TAG_GROUP_GLW, ((glow_datum *)glow_widget)->definition_index);
  particle->position[0] =
    delta * particle->present_velocity[0] + particle->position[0];
  particle->position[1] =
    delta * particle->present_velocity[1] + particle->position[1];
  particle->position[2] =
    delta * particle->present_velocity[2] + particle->position[2];
}

/* glow_trailing_particle_age (0x1332a0 / objects.obj / glow.c) — retire a
 * trailing particle whose age (+0x50) has passed its lifetime (+0x52): unlink
 * it from the glow's particle list (head +0x250 / tail +0x254), return it to
 * the glow particle pool and decrement the glow's particle count (+0x24c).
 *
 * No call xrefs: glow_update inlines the same body (0x1349ac-0x1349e9). ABI
 * is taken from the body alone: EDI (glow_widget, 0x1332a0) and ESI
 * (particle_ptr, 0x1332b1) are read without being set. The glowdef lookup's
 * result is unused (it is never read after the CALL).
 * Name: PAL-2342 glow.c (T2) — same slot in PAL's symbol order, between
 * glow_trailing_particle_update_position and
 * glow_normal_particle_update_color.
 */
void glow_trailing_particle_age(int glow_widget, int particle_ptr)
{
  glow_datum *glow;
  glow_particle *particle;
  glow_particle *previous;
  glow_particle *next;

  glow = (glow_datum *)glow_widget;
  particle = (glow_particle *)particle_ptr;
  tag_get(TAG_GROUP_GLW, glow->definition_index);
  if (particle->ticks_in_existence > particle->lifetime) {
    previous = particle->previous;
    next = particle->next;
    if (previous)
      previous->next = next;
    else
      glow->head_particle = next;
    if (next)
      next->previous = previous;
    else
      glow->tail_particle = previous;
    datum_delete(glow_particle_data, particle->index);
    glow->number_of_particles--;
  }
}

/* glow_normal_particle_update_color (0x133300 / objects.obj / glow.c) — color
 * and edge fade of a normal glow particle (the PAL reference calls it
 * glow_normal_particle_update_color).
 *
 *  - If the glowdef's color function index (u16 +0xb0) is not 0xffff, color
 *    rgb (+0x10..+0x18) = lerp(lower +0xb8, upper +0xc8, function value, or 0
 *    if object_get_function_value fails), and alpha (+0xc) = 1.
 *  - If glowdef flag 0x1 is set, color rgb = lower + (upper - lower) *
 *    rate(+0xf4) * particle t(+0x28), and alpha = 1.
 *  - fade (+0x58): t = particle t / glow total time (+0x234),
 *    edge = glowdef +0xf8 * 0.5; ramp up over [0,edge), down over
 *    (1-edge,1], else 1; then pinned to [0,1].
 *
 * particle_ptr arrives in EDI and glow_widget in EBX; object_handle is the
 * single stack arg. Sole caller: glow_update (0x1348b8). The final
 * `MOV EAX,[EDI+0x58]; MOV [EDI+0x58],EAX` (0x13348c) is the pin's
 * pass-through arm storing the field back to itself.
 * Name: PAL-2342 glow.c (T2) — same slot in PAL's symbol order, after the
 * trailing-particle helpers and before point_from_parametric_line; tests
 * the PAL modify_particle_color bit (flags & 0x1).
 */
void glow_normal_particle_update_color(int particle_ptr, int object_handle,
                                       int glow_widget)
{
  glow_particle *particle;
  glow_definition *definition;
  float function_value;
  float scale;
  float t;
  float edge_fade;

  particle = (glow_particle *)particle_ptr;
  definition = (glow_definition *)tag_get(
    TAG_GROUP_GLW, ((glow_datum *)glow_widget)->definition_index);
  if (definition->color_attachment_index != 0xffff) {
    if (!object_get_function_value(
          object_handle, definition->color_attachment_index, &function_value))
      scale = REAL_ZERO_POOL;
    else
      scale = function_value;
    particle->color_red = (definition->color_upper_bound_rgb[0] -
                           definition->color_lower_bound_rgb[0]) *
                            scale +
                          definition->color_lower_bound_rgb[0];
    particle->color_green = (definition->color_upper_bound_rgb[1] -
                             definition->color_lower_bound_rgb[1]) *
                              scale +
                            definition->color_lower_bound_rgb[1];
    particle->color_blue = (definition->color_upper_bound_rgb[2] -
                            definition->color_lower_bound_rgb[2]) *
                             scale +
                           definition->color_lower_bound_rgb[2];
    particle->color_alpha = 1.0f;
  }

  /* t is read through the particle_ptr parameter, not the particle local:
   * VC71 orders the two commutative FMULs by operand symbol, and only the
   * parameter spelling reproduces 2276's FMUL rate(+0xf4); FMUL t(+0x28). */
  if ((definition->flags & FLAG(_glow_definition_modify_particle_color_bit)) !=
      0) {
    particle->color_red = (definition->color_upper_bound_rgb[0] -
                           definition->color_lower_bound_rgb[0]) *
                            definition->color_rate_of_change *
                            ((glow_particle *)particle_ptr)->t +
                          definition->color_lower_bound_rgb[0];
    particle->color_green = (definition->color_upper_bound_rgb[1] -
                             definition->color_lower_bound_rgb[1]) *
                              definition->color_rate_of_change *
                              ((glow_particle *)particle_ptr)->t +
                            definition->color_lower_bound_rgb[1];
    particle->color_blue = (definition->color_upper_bound_rgb[2] -
                            definition->color_lower_bound_rgb[2]) *
                             definition->color_rate_of_change *
                             ((glow_particle *)particle_ptr)->t +
                           definition->color_lower_bound_rgb[2];
    particle->color_alpha = 1.0f;
  }

  t = particle->t / ((glow_datum *)glow_widget)->total_time;
  edge_fade = definition->percentage_edge_fade * 0.5f;
  if (t < edge_fade)
    particle->fade = t / edge_fade;
  else if (t > REAL_ONE_POOL - edge_fade)
    particle->fade = (REAL_ONE_POOL - t) / edge_fade;
  else
    particle->fade = 1.0f;

  particle->fade = particle->fade < 0.0f ?
                     0.0f :
                     (particle->fade > 1.0f ? 1.0f : particle->fade);
}

/* glow_normal_particle_update_size (0x1334a0 / objects.obj / glow.c) — a
 * normal particle's present size (+0x24) is its initial size (+0x20), copied
 * as a dword.
 *
 * No call xrefs: glow_update inlines the same copy. particle_ptr arrives in
 * EAX (read without being set at 0x1334a0).
 * Name: PAL-2342 glow.c (T2) — same slot in PAL's symbol order, after
 * glow_normal_particle_update_color.
 */
void glow_normal_particle_update_size(int particle_ptr)
{
  *(uint32_t *)&((glow_particle *)particle_ptr)->present_size =
    *(uint32_t *)&((glow_particle *)particle_ptr)->initial_size;
}

/* glow_particle_new (0x1334b0 / objects.obj / glow.c) — allocate a particle
 * from the glow particle pool and store its datum index in the particle
 * (+0x4). Returns the particle, or NULL when the pool is full.
 *
 * No call xrefs: glow_update inlines the same body. cdecl, no args.
 * Name: PAL-2342 glow.c (T2) — same slot in PAL's symbol order, between
 * glow_normal_particle_update_size and point_from_parametric_line.
 */
void *glow_particle_new(void)
{
  glow_particle *particle;
  int index;

  particle = NULL;
  index = data_new_at_index(glow_particle_data);
  if (index != -1) {
    particle = (glow_particle *)datum_get(glow_particle_data, index);
    particle->index = index;
  }
  return particle;
}

/* point_from_parametric_line (0x1334f0 / objects.obj) — evaluate a point along
 * a parametric line: out = point + t * vector, component-wise.
 *
 * x87 order is fixed per component by the disassembly: FLD [ebp+0x10] (t);
 * FMUL [eax+N] (vector); FADD [ecx+N] (point); FSTP [edx+N] (out), so the
 * expression must stay `t * vector[i] + point[i]`.
 *
 * NOTE (faithful to the binary): the z component adds the y component of
 * `point`, not z — 0x133517 is `FADD float ptr [ECX + 0x4]`, while x uses
 * [ECX] and y uses [ECX + 0x4]. This is an original-binary bug (a copy-paste
 * slip in the Bungie source); it is reproduced deliberately. Do not "fix" it.
 */
void point_from_parametric_line(real *point, real *vector, real t, real *out)
{
  out[0] = t * vector[0] + point[0];
  out[1] = t * vector[1] + point[1];
  out[2] = t * vector[2] + point[1];
}

/* glow_render (0x133520 / objects.obj, object_lights.c) — build and submit
 * the render-sprite batch for a glow widget's particle list.
 *
 * Resolves the glow widget (datum_get(glow_data, widget_datum), the
 * same widget pool glow_submit uses) and its 'glw!' tag definition
 * (tag_get(TAG_GROUP_GLW, glow_widget+0x224)), opens a sprite-build record
 * (build_sprites_begin) sized from the widget's active particle count (+0x24c,
 * zero-extended per the XOR ECX,ECX;MOV CX idiom at 0x133554) and the tag's
 * shader field (glowdef+0x150), then walks the particle list rooted at
 * glow_widget+0x250 (next-link at particle+0x5c) appending one sprite per
 * particle (FUN_0018d6e0) before submitting the batch (FUN_0018d360).
 *
 * Per-particle sprite args (binary-confirmed against 00133580-001335ad):
 *   untransformed_origin    = &particle position   (particle+0x2c)
 *   untransformed_direction = glow_widget + variant*0x6c + 0x44, variant =
 *                             *(int16_t*)(particle+2) (MOVSX at 0x133589) --
 *                             the same per-marker basis-row table
 *                             glow_update indexes as basis_i[-0xb..-9]
 *                             relative to its own +0x70 basis pointer
 *                             (0x70-0x2c == 0x44).
 *   angle                   = 0.0f (constant)
 *   scale                   = particle+0x24 (alpha_current per glow.c;
 *                             forwarded via plain MOV, not FLD/FSTP -- no
 *                             FPU computation happens at this call site)
 *   color                   = &particle color   (particle+0xc)
 *   intensity               = particle+0x58 (forwarded the same way)
 *   flags                   = 0
 *
 * object_handle (param_1, [EBP+8]) is the caller's stack arg per
 * glow_submit's call glow_render(object_handle, widget_datum), but
 * disassembly never reads [EBP+8] in this body -- confirmed unused.
 *
 * Confirmed: the ADD ESP,0x24 at 0x133578 batch-cleans 9 dwords -- the two
 * cdecl pushes each for datum_get and tag_get plus build_sprites_begin's 5
 * pushes -- deferred cdecl cleanup, not an extra argument (see call_site_audit
 * ARG_COUNT note on build_sprites_begin).
 */
void glow_render(int object_handle, int widget_datum)
{
  int glow_widget;
  int glow_tag;
  int particle;
  char record[0xa4];

  glow_widget = (int)datum_get(glow_data, widget_datum);
  glow_tag = (int)tag_get(TAG_GROUP_GLW, *(int *)(glow_widget + 0x224));
  build_sprites_begin((uint32_t *)record, *(uint16_t *)(glow_widget + 0x24c),
                      *(uint32_t *)(glow_tag + 0x150), 0x326a78, 0);

  for (particle = *(int *)(glow_widget + 0x250); particle != 0;
       particle = *(int *)(particle + 0x5c)) {
    FUN_0018d6e0(
      record, 0, 0, 0, (float *)(particle + 0x2c),
      (float *)(glow_widget + *(int16_t *)(particle + 2) * 0x6c + 0x44), 0.0f,
      *(float *)(particle + 0x24), (float *)(particle + 0xc),
      *(float *)(particle + 0x58), 0);
  }

  FUN_0018d360(record);
}

/* Object glow widgets — animated glow effects attached to game objects.
 * TU: c:\halo\SOURCE\objects\widgets\glow.c (confirmed via __FILE__ assert). */

/* data_t* pool for glow particles ("normal" and "trailing" share it). */
#define GLOW_PARTICLE_DATA (*(data_t **)0x005a90cc)

/* glow_widget definition (glowdef) tag layout, group 'glw!' (0x676c7721):
 *   +0x22  int16_t boundary_effect          // 0 = reflect/ping-pong, 1 = wrap
 *   +0x80  int16_t function                 // object function index (-1 =
 * none) +0x84  float   scale_lower              // output remap: low bound
 *   +0x88  float   scale_upper              //               high bound
 *   +0x8c  float   input_lower              // input remap:  low bound
 *   +0x90  float   input_upper              //               high bound
 *
 * particle instance fields:
 *   +0x1c  float   scale                    // computed animated scale
 *   +0x28  float   phase                    // animation phase counter
 *   +0x54  uint    flags                    // bit0 = reverse direction
 *
 * glow_widget instance fields:
 *   +0x224 int32_t definition_tag_index
 *   +0x234 float   period                   // phase wrap period
 */

/* widgets_new — create widgets for an object from its tag definition.
 *
 * Looks up the object's tag (group 'obje'), reads the widget attachments
 * tag block at tag+0x14c, and for each attachment, searches the global
 * widget_types table (5 entries at 0x323528, each 0x28 bytes) for a
 * matching group_tag. When found, allocates a new widget datum from the
 * widget data pool at 0x5a90c4, sets its type field, and either:
 *   - calls the widget type's "new" function (entry+0x18) with the
 *     attachment's definition index (element+0x0c), linking on success
 *   - or directly links the widget with definition_handle = -1 if no
 *     "new" function is defined.
 * Widgets are prepended to a singly-linked list rooted at obj+0x11c.
 *
 * Source: c:\halo\source\objects\widgets\widget_types.h (line 0x96)
 *
 * Confirmed: 1 cdecl arg (object_handle).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (handle, -1).
 * Confirmed: CALL 0x1ba140 (tag_get) with (0x6f626a65, obj[0]).
 * Confirmed: CALL 0x19b210 (tag_block_get_element) with (block, index, 0x20).
 * Confirmed: CALL 0x119610 (data_new_at_index) with (*(data_t**)0x5a90c4).
 * Confirmed: CALL 0x119320 (datum_get) with (*(data_t**)0x5a90c4, handle).
 * Confirmed: CALL 0x1196d0 (datum_delete) with (*(data_t**)0x5a90c4, handle).
 * Confirmed: widget_types table at 0x323528: [+0x00]=group_tag, [+0x18]=new_fn.
 * Confirmed: ADD ESP,0x10 cleans both object_get_and_verify_type + tag_get
 * pushes. Confirmed: outer loop counter is int16_t (MOVSX EAX,AX at 0x1362b2).
 * Confirmed: inner loop counter is int16_t (MOVSX ECX,SI; CMP SI,0x5).
 * Confirmed: indirect CALL EAX at 0x13625a for widget new function.
 * Confirmed: assert_halt for type range check at 0x1361fe.
 */

/* glow_particles_initialize (0x1342a0 / objects.obj, object_lights.c) — build
 * the glow particle chain for a glow-widget instance.
 *
 * Fetches the glow ('glw!' = 0x676c7721) tag block via
 * tag_get(TAG_GROUP_GLW, widget+0x224), then allocates one particle node per
 * widget+0x24c (int16 count) via glow_normal_particle_new, linking them into a
 * doubly-linked list rooted at widget+0x250 (head) / widget+0x254 (tail).
 * Node flag word at +0x54:
 *   tag flag bit1 (0x02): force bit0 set;
 *   tag flag bit2 (0x04): alternate bit0 per node using a persistent per-widget
 *     parity toggle (init true: parity nodes clear bit0, off-parity nodes set
 *     it), preserving the exact AND 0xfffffffe vs OR 1 order and the flip.
 * Node links: +0x5c forward (prev->next = cur), +0x60 back (cur->prev = prev).
 * Early-returns if any allocation fails (node == 0).
 *
 * Confirmed: sole input glow_widget_ptr arrives in ESI (@<esi> per kb decl).
 * Confirmed: both callees are plain cdecl, called by name.
 * Confirmed: loop bound is the int16 field at +0x24c, re-read at both the entry
 * guard and the do/while condition — matched, not cached.
 */
void glow_particles_initialize(int glow_widget_ptr)
{
  void *glow_tag;
  int node;
  int prev_node;
  unsigned int flags;
  char parity;
  short index;

  glow_tag = tag_get(TAG_GROUP_GLW, *(int *)(glow_widget_ptr + 0x224));
  index = 0;
  prev_node = 0;
  parity = 1;
  for (index = 0; index < *(short *)(glow_widget_ptr + 0x24c); index++) {
    node = glow_normal_particle_new(glow_widget_ptr, index,
                                    *(short *)(glow_widget_ptr + 0x24c));
    if (node == 0) {
      break;
    }
    if ((*(unsigned char *)((int)glow_tag + 0x28) &
         FLAG(_glow_definition_particles_move_backwards_bit)) != 0) {
      *(unsigned int *)(node + 0x54) |=
        FLAG(_glow_particle_moving_backwards_bit);
    }
    if ((*(unsigned char *)((int)glow_tag + 0x28) &
         FLAG(_glow_definition_particles_move_in_both_directions_bit)) != 0) {
      if (!parity) {
        flags = *(unsigned int *)(node + 0x54) |
                FLAG(_glow_particle_moving_backwards_bit);
      } else {
        flags = *(unsigned int *)(node + 0x54) &
                ~FLAG(_glow_particle_moving_backwards_bit);
      }
      *(unsigned int *)(node + 0x54) = flags;
      parity = !parity;
    }
    if (*(int *)(glow_widget_ptr + 0x250) == 0) {
      *(int *)(glow_widget_ptr + 0x250) = node;
    }
    if (prev_node != 0) {
      *(int *)(prev_node + 0x5c) = node;
    }
    *(int *)(node + 0x60) = prev_node;
    *(int *)(glow_widget_ptr + 0x254) = node;
    prev_node = node;
  }
}

/* Glow widget trailing-particle spawner.
 *
 * TU: c:\halo\SOURCE\objects\widgets\glow.c (confirmed via __FILE__ assert
 * xref). The glow widget spawns "trailing" particles behind a glowing effect
 * attached to a game object.  Its definition lives in a 'glw!' (0x676c7721) tag
 * block, resolved through object handle at glow_widget+0x224.
 */

/* GLOW_PARTICLE_DATA (data_t* pool, shared with glow_normal_particle_new) is
 * defined above, before glow_normal_particle_new. */

/* glow_submit (0x134ae0 / objects.obj, object_lights.c) — initialize a glow
 * widget instance attached to an object.
 *
 * Given an object handle and a widget datum handle, looks up the object datum,
 * resolves the glow-widget tag ('glw!' = 0x676c7721) referenced at
 * object+0x224, then runs the glow-widget initialization (glow_update) on the
 * object datum, builds the object's marker set for the widget tag
 * (object_get_marker_by_name), and refreshes the widget render batch
 * (glow_render).
 *
 * Confirmed: 2 cdecl args (object_handle @ [EBP+0x8], widget_datum @
 * [EBP+0xc]), early-out if either is -1. Confirmed: first
 * datum_get(glow_data, widget_datum) -> object datum; widget tag =
 * tag_get(TAG_GROUP_GLW, *(object_datum+0x224)). Confirmed: glow_update is
 * register-arg — glow_widget@<eax> receives the second datum_get's return
 * (object datum ptr); object_handle pushed (the EDI push at 0x134b1f) is its
 * single cdecl stack arg. The trailing ADD ESP,0x1c batch-cleans this push plus
 * object_get_marker_by_name's 4 args and glow_render's 2 args.
 * Confirmed: object_get_marker_by_name(object_handle, widget_tag,
 * local_buf[0x6c], 1). Confirmed: glow_render(object_handle, widget_datum).
 */
void glow_submit(int object_handle, int widget_datum)
{
  unsigned char local_buf[0x6c];
  int object_datum;
  void *widget_tag;

  if ((object_handle != -1) && (widget_datum != -1)) {
    object_datum = (int)datum_get(glow_data, widget_datum);
    widget_tag = tag_get(TAG_GROUP_GLW, *(int *)(object_datum + 0x224));
    glow_update((int)datum_get(glow_data, widget_datum),
                object_handle);
    object_get_marker_by_name((int)object_handle, widget_tag, local_buf,
                                    1);
    glow_render(object_handle, widget_datum);
  }
}

/* Allocates a new entry in the 0x46f020 data table and stores param_1 at +4.
 * Returns the datum handle, or -1 on failure.
 * 0x134be0 / objects.obj
 */
int light_volume_new(int param_1)
{
  int iVar1;
  int iVar2;

  iVar1 = data_new_at_index(light_volume_data);
  if (iVar1 != -1) {
    iVar2 = (int)datum_get(light_volume_data, iVar1);
    *(int *)(iVar2 + 4) = param_1;
  }
  return iVar1;
}

/* Deletes the entry at param_1 from the 0x46f020 data table.
 * 0x134c20 / objects.obj
 */
void light_volume_delete(int param_1)
{
  if (param_1 != -1) {
    datum_delete(light_volume_data, param_1);
  }
}

/* Light-volume widget parameter interpolation.
 * TU: c:\halo\SOURCE\objects\widgets\light_volumes.c (confirmed via __FILE__
 * assert xref at line 0x6e).
 *
 * light_volume_interpolate_frames selects a light-volume parameter block for an
 * object.  The definition holds a tag_block of parameter frames (header at
 * +0x120, element stride 0xb0).  With <=1 frame it returns element 0 directly.
 * With more than one frame it reads the object's animation function value
 * (function index at definition+0xb8, minus one) and, when that succeeds,
 * blends two frames into a static scratch block at 0x0046ef70 and returns that
 * block.
 *
 * The binary re-fetches element index 0 in every tag_block_get_element call
 * (verified against disassembly — the three main-path calls and the <=1
 * fallback call all pass index 0); this is preserved verbatim, not "fixed" into
 * an interpolation-index fetch.  definition_ptr arrives in EBX (@<ebx>). */

void *light_volume_interpolate_frames(int definition_ptr, int object_handle)
{
  int *frames;
  void *result;
  void *frame0;
  void *frame1;
  float function_value;
  float inverse_function_value;
  bool ok;

  if (definition_ptr == 0) {
    display_assert("definition",
                   "c:\\halo\\SOURCE\\objects\\widgets\\light_volumes.c", 0x6e,
                   1);
    system_exit(-1);
  }

  frames = (int *)(definition_ptr + 0x120);
  result = tag_block_get_element(frames, 0, 0xb0);

  if (1 < *frames) {
    frame0 = tag_block_get_element(frames, 0, 0xb0);
    frame1 = tag_block_get_element(frames, 0, 0xb0);
    ok = object_get_function_value(
      object_handle, (short)(*(short *)(definition_ptr + 0xb8) - 1), &function_value);
    if (ok) {
      result = (void *)0x0046ef70;
      inverse_function_value = *(float *)0x002533c8 - function_value;
      *(float *)0x0046ef80 = function_value * *(float *)((char *)frame1 + 0x10) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x10);
      *(float *)0x0046ef84 = function_value * *(float *)((char *)frame1 + 0x14) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x14);
      *(float *)0x0046ef88 = function_value * *(float *)((char *)frame1 + 0x18) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x18);

      *(float *)0x0046efac = function_value * *(float *)((char *)frame1 + 0x3c) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x3c);
      *(float *)0x0046efb0 = function_value * *(float *)((char *)frame1 + 0x40) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x40);
      *(float *)0x0046efb4 = function_value * *(float *)((char *)frame1 + 0x44) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x44);

      *(float *)0x0046efd8 = function_value * *(float *)((char *)frame1 + 0x68) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x68);
      *(float *)0x0046efdc = function_value * *(float *)((char *)frame1 + 0x6c) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x6c);
      *(float *)0x0046efe0 = function_value * *(float *)((char *)frame1 + 0x70) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x70);
      *(float *)0x0046efe4 = function_value * *(float *)((char *)frame1 + 0x74) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x74);
      *(float *)0x0046efe8 = function_value * *(float *)((char *)frame1 + 0x78) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x78);
      *(float *)0x0046efec = function_value * *(float *)((char *)frame1 + 0x7c) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x7c);
      *(float *)0x0046eff0 = function_value * *(float *)((char *)frame1 + 0x80) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x80);
      *(float *)0x0046eff4 = function_value * *(float *)((char *)frame1 + 0x84) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x84);
      *(float *)0x0046eff8 = function_value * *(float *)((char *)frame1 + 0x88) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x88);
      *(float *)0x0046effc = function_value * *(float *)((char *)frame1 + 0x8c) +
                             inverse_function_value * *(float *)((char *)frame0 + 0x8c);


    }
  } else {
    result = tag_block_get_element(frames, 0, 0xb0);
  }

  return result;
}

/*
 * pow1 (0x134e50 / objects.obj) — raise a value to a power, skipping
 * the power call when the exponent is exactly 1.0.
 *
 * Returns value (param_1) unchanged when exponent (param_2) == 1.0f; otherwise
 * returns pow(value, exponent). The 1.0 special-case avoids a redundant power
 * call.
 *
 * Confirmed: FCOMP param_2 against [0x2533c8 = 1.0f]; equal -> return param_1.
 * Confirmed: not-equal -> tail-call pow(param_1, param_2) (JMP 0x1d9e70).
 */
float pow1(float value, float exponent)
{
  if (exponent != REAL_ONE_POOL) {
    return (float)pow((double)value, (double)exponent);
  }
  return value;
}

/* light_volume_render (0x134e80 / objects.obj, object_lights.c) — render a
 * light-volume effect (group 'lmgs2'/0x6d677332 contrail-style sprite strip)
 * along an object's marker, fading by view-direction and distance falloff.
 *
 * Resolves the light-volume tag from the light-volume datum, gates on the tag
 * having marker count (+0x6e > 0) and a positive sprite count (+0x120 > 0),
 * then:
 *   - fetches the object's marker buffer for the tag
 * (object_get_marker_by_name);
 *   - computes a view-dependent intensity from the camera forward axis
 *     (globals 0x50655c/0x506560/0x506564) and camera position
 *     (0x506550/0x506554/0x506558), clamped to [0,1];
 *   - applies the tag's distance attenuation (+0x34/+0x38/+0x3c/+0x40) and the
 *     object function value (object_get_function_value, function index tag+0x44
 * - 1);
 *   - if visible, emits a sprite strip via the rendering batch
 *     (FUN_0017cfc0/0017cfd0/0017d010/0017ad90).
 *
 * Confirmed: 2 cdecl args (object_handle @ [EBP+0x8], light_volume_datum @
 * [EBP+0xc]). Confirmed: light tag 'lmgs2' = tag_get(0x6d677332,
 * *(light_datum+4)). Confirmed: light_volume_interpolate_frames is register-arg
 * — light_tag@<ebx> (EBX from MOV EBX,EAX at 0x134ebe), object_handle pushed
 * (EDI). Decompiler dropped the @<ebx> arg. Confirmed: marker buffer base
 * EBP-0xa4, size 0x6c; object_get_marker_by_name fills it. Marker
 * position = buf+0x60/+0x64/+0x68 (FLD [EBP-0x44/-0x40/-0x3c]). Marker forward
 * = buf+0x3c/+0x40/+0x44 (FLD [EBP-0x68/-0x64/-0x60]). Confirmed: object_handle
 * copied to EDI; the [EBP+0x8] param slot is reused as a float scratch (blend
 * value), kept as a separate local here. Confirmed: the per-segment curve is
 * pow(frac, period) with the "skip when period==1.0" identity (four
 * __CIpow_default reloc targets @ 0x1d9e70 in the delinked reference; same
 * helper as pow1 above). pow(x,1.0)==x, hence the exponent==1.0
 * short-circuit. (float)pow((double),(double)) lowers to the _CIpow intrinsic
 * under VC71 and to a pow call under clang. Uncertain: marker_state struct
 * field meanings at +0x10/+0x14/+0x18/+0x3c/+0x40/
 *   +0x44/+0x68/+0x78/+0x88/+0x8c (read-only here).
 */
void light_volume_render(int object_handle, int light_volume_datum)
{
  unsigned char marker_buf[0x6c];
  int light_datum;
  int light_tag;
  int marker_state;
  unsigned short marker_count;
  unsigned int rem;
  short i;
  float cam_x, cam_y, cam_z; /* marker position minus camera globals */
  float intensity;
  volatile float depth_factor;
  float dot_to_marker;
  float t;
  float blend;
  float period;
  float frac;
  float scratch;
  float out_pos[3]; /* local_1c..: world position for sprite */
  float color2[4]; /* local_38..: per-segment ARGB. [0]=alpha (intensity),
                      [1..3]=RGB (FUN_0007c270 out at EBP-0x34); packed as a
                      4-float a_rgb by real_argb_color_to_pixel32 (EBP-0x38). */
  float interp_a, interp_b; /* local_2c / local_28 */
  unsigned char zfn;
  float fn_val;
  unsigned int color_argb;

  if ((object_handle != -1) && (light_volume_datum != -1)) {
    light_datum = (int)datum_get(light_volume_data, light_volume_datum);
    light_tag = (int)tag_get(0x6d677332, *(int *)(light_datum + 4));
    if ((0 < *(short *)(light_tag + 0x6e)) &&
        (0 < *(int *)(light_tag + 0x120))) {
      marker_state =
        (int)light_volume_interpolate_frames(light_tag, object_handle);
      object_get_marker_by_name(object_handle, (void *)light_tag,
                                      marker_buf, 1);

      /* Read marker position (buf+0x60..) and forward (buf+0x3c..) directly
       * from the buffer on every use; the reference never caches them in
       * locals, so mirroring that avoids spill/copy instructions.
       * Match-sensitive. */
      cam_x = *(float *)(marker_buf + 0x60) - *(float *)0x506550;
      cam_y = *(float *)(marker_buf + 0x64) - *(float *)0x506554;
      cam_z = *(float *)(marker_buf + 0x68) - *(float *)0x506558;

      /* view-direction dot with camera forward axis, |.|. Summation order and
       * per-term operand order mirror the reference (fwd_z*g564 + fwd_y*g560 +
       * g55c*fwd_x): x87 (-mno-sse) preserves source association, so this order
       * is load-bearing for both the VC71 match and runtime float fidelity. */
      dot_to_marker = *(float *)(marker_buf + 0x44) * *(float *)0x506564 +
                      *(float *)(marker_buf + 0x40) * *(float *)0x506560 +
                      *(float *)0x50655c * *(float *)(marker_buf + 0x3c);
      if (!(dot_to_marker >= REAL_ZERO_POOL))
        dot_to_marker = -dot_to_marker;

      blend = 1.0f; /* function-value scratch */
      depth_factor = 1.0f;
      if (*(float *)(light_tag + 0x38) > REAL_ZERO_POOL) {
        t = ((cam_z * *(float *)0x506564 + cam_y * *(float *)0x506560 +
              *(float *)0x50655c * cam_x) -
             *(float *)(light_tag + 0x38)) /
            (*(float *)(light_tag + 0x34) - *(float *)(light_tag + 0x38));
        depth_factor = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
      }

      intensity =
        dot_to_marker * *(float *)(light_tag + 0x40) +
        (REAL_ONE_POOL - dot_to_marker) * *(float *)(light_tag + 0x3c);
      scratch =
        (intensity < 0.0f) ? 0.0f : ((intensity > 1.0f) ? 1.0f : intensity);
      depth_factor = scratch * depth_factor;

      zfn = object_get_function_value(
        object_handle, (short)(*(short *)(light_tag + 0x44) - 1), &blend);
      if (zfn != 0) {
        depth_factor = blend * depth_factor;
      }

      if ((depth_factor > REAL_ZERO_POOL) &&
          ((*(float *)(marker_state + 0x68) > REAL_ZERO_POOL) ||
           (*(float *)(marker_state + 0x78) > REAL_ZERO_POOL)) &&
          ((*(float *)(marker_state + 0x3c) > REAL_ZERO_POOL) ||
           (*(float *)(marker_state + 0x40) > REAL_ZERO_POOL))) {
        FUN_0017cfc0(5, 1);
        FUN_0017cfd0(0, *(int *)(light_tag + 0x68),
                     *(short *)(light_tag + 0x6c));
        marker_count = *(unsigned short *)(light_tag + 0x6e);
        if (0 < (short)marker_count) {
          i = 0;
          rem = (unsigned int)marker_count;
          do {
            period = *(float *)(marker_state + 0x14);
            frac = (float)i / (float)(short)(marker_count - 1);
            frac = (period != 1.0f) ? (float)pow((double)frac, (double)period) :
                                      frac;

            period = *(float *)(marker_state + 0x44);
            interp_a = (period != 1.0f) ?
                         (float)pow((double)frac, (double)period) :
                         frac;
            interp_a =
              interp_a * *(float *)(marker_state + 0x40) +
              (REAL_ONE_POOL - interp_a) * *(float *)(marker_state + 0x3c);

            period = *(float *)(marker_state + 0x88);
            interp_b = (period != 1.0f) ?
                         (float)pow((double)frac, (double)period) :
                         frac;

            period = *(float *)(marker_state + 0x8c);
            fn_val = (period != 1.0f) ?
                       (float)pow((double)frac, (double)period) :
                       frac;

            t = frac * *(float *)(marker_state + 0x18) +
                *(float *)(marker_state + 0x10);
            out_pos[0] =
              *(float *)(marker_buf + 0x3c) * t + *(float *)(marker_buf + 0x60);
            out_pos[1] =
              *(float *)(marker_buf + 0x40) * t + *(float *)(marker_buf + 0x64);
            out_pos[2] =
              *(float *)(marker_buf + 0x44) * t + *(float *)(marker_buf + 0x68);

            FUN_0007c270(color2 + 1, *(unsigned char *)(light_tag + 0x22) & 3,
                         (float *)(marker_state + 0x6c),
                         (float *)(marker_state + 0x7c), interp_b);

            /* color2[0] = view/distance-scaled intensity (alpha):
             * ((1-fn)*+0x68 + fn*+0x78) * depth_factor; RGB stays at
             * color2[1..3] where FUN_0007c270 wrote it (reference: c270 out =
             * EBP-0x34, d1c90 arg = EBP-0x38 — one float apart). */
            color2[0] = (fn_val * *(float *)(marker_state + 0x78) +
                         (REAL_ONE_POOL - fn_val) *
                           *(float *)(marker_state + 0x68)) *
                        depth_factor;
            color_argb = real_argb_color_to_pixel32(color2);
            FUN_0017d010(out_pos, interp_a, (float *)0, 0.0f, color_argb);

            i = (short)(i + 1);
            rem = rem - 1;
          } while (rem != 0);
        }
        FUN_0017d020();
      }
    }
  }
}

/* light_volume_submit (0x135210 / objects.obj, object_lights.c) —
 * visibility/submit pre-pass for an object's light-volume effect; if visible,
 * queues it for deferred rendering with light_volume_render as the draw
 * callback.
 *
 * Gates on the same light-volume tag ('lmgs2') having marker count (+0x6e > 0)
 * and sprite count (+0x120 > 0); additionally, when the tag has a function
 * index
 * (+0x44 != 0) and a function-state pointer (param_4) is supplied, requires the
 * indexed function value (param_4->[+4][index-1]) to be > 0. Then fetches the
 * object marker buffer (object_get_marker_by_name) and, if the tag's near
 * distance (+0x38) is 0 or the camera-relative depth along the view forward
 * axis is within it, submits the volume via FUN_0017cfb0(object_handle,
 * light_volume_datum, &marker_position, light_volume_render).
 *
 * Confirmed: 4 cdecl args. object_handle @ [EBP+0x8] (EBX), light_volume_datum
 * @ [EBP+0xc] (EDI); param_3 @ [EBP+0x10] is unused here; param_4 @ [EBP+0x14]
 * is a function-state pointer (reads ptr+0x4 then indexes by tag+0x44).
 * Confirmed: PUSH 0x134e80 at 0x135303 — light_volume_render is the draw
 * callback. Confirmed: marker buffer base EBP-0x6c, size 0x6c; position at
 * buf+0x60/+0x64/ +0x68 (FLD [EBP-0xc/-0x8/-0x4]); &buf[0x60] passed as
 * position to FUN_0017cfb0.
 */
void light_volume_submit(int object_handle, int light_volume_datum, int lighting,
                         int animation) /* name: PAL 2342 light_volumes.c:370 */
{
  unsigned char marker_buf[0x6c];
  int light_tag;
  short source; /* name: PAL 2342 source/objects/widgets/light_volumes.c:382 */
  float *marker_pos;
  float diff[3];

  (void)lighting; /* name: PAL 2342 source/objects/widgets/light_volumes.c:369 */
  if ((object_handle != -1) && (light_volume_datum != -1)) {
    light_tag = (int)tag_get(
      0x6d677332,
      *(int *)((int)datum_get(light_volume_data, light_volume_datum) + 4));
    if ((0 < *(short *)(light_tag + 0x6e)) &&
        (0 < *(int *)(light_tag + 0x120))) {
      source = *(short *)(light_tag + 0x44);
      if ((source == 0) || (animation == 0) ||
          (*(float *)(*(int *)(animation + 4) - 4 + source * 4) >
           REAL_ZERO_POOL)) {
      object_get_marker_by_name(object_handle, (void *)light_tag,
                                      marker_buf, 1);
      marker_pos = (float *)(marker_buf + 0x60);
      /* diff[] as an array (not scalars) forces the three subs to stay eager
       * across the near==0 branch (scalars sink into the || short-circuit).
       * Match-sensitive: do not scalarize. */
      diff[0] = marker_pos[0] - *(float *)0x506550;
      diff[1] = marker_pos[1] - *(float *)0x506554;
      diff[2] = marker_pos[2] - *(float *)0x506558;
      if ((*(float *)(light_tag + 0x38) == REAL_ZERO_POOL) ||
          (*(float *)0x50655c * diff[0] + *(float *)0x506560 * diff[1] +
             *(float *)0x506564 * diff[2] <
           *(float *)(light_tag + 0x38))) {
        FUN_0017cfb0(object_handle, light_volume_datum, marker_pos,
                     (int)light_volume_render);
      }
    }
    }
  }
}

/*
 * tag_group_to_widget_type (0x135f20 / objects.obj) — find the widget_types
 * table index whose group_tag (entry+0x00) matches the requested group tag.
 *
 * Linear search of the 5-entry widget_types table at 0x323528 (stride 0x28
 * bytes). Returns the matching index in [0,4], or -1 (0xffff) if no entry
 * matches.
 *
 * Confirmed: CMP dword ptr [ECX*8 + 0x323528], EDX where ECX = idx*5
 *            -> compares the 4-byte group_tag at 0x323528 + idx*0x28.
 * Confirmed: loop bound CMP AX,0x5 (int16_t counter).
 * Confirmed: miss path MOV AX,SI where SI was OR'd to -1 -> returns (short)-1.
 */
short tag_group_to_widget_type(int group_tag)
{
  short result;
  short i;

  result = -1;
  i = 0;
  do {
    if (*(int *)(0x323528 + (int)i * 0x28) == group_tag) {
      result = i;
      break;
    }
    i = i + 1;
  } while (i < 5);

  return result;
}

/* effect_new_from_object declaration is in generated/decl.h */

/*
 * objects/objects.c — object system lifecycle and placement
 * XBE source: c:\halo\SOURCE\objects\objects.c
 *            + c:\halo\SOURCE\objects\object_lights.c (same .obj)
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0x1396e0  light_disconnect_from_map (object_lights.c)
 *   0x13aed0  light_reconnect_to_map (object_lights.c)
 *   0x13d640  object_try_and_get_and_verify_type
 *   0x13d680  object_get_and_verify_type
 *   0x13d920  object_set_garbage_flag
 *   0x13dfc0  object_header_block_reference_get
 *   0x13e510  object_child_list_remove
 *   0x13eb70  object_reset_markers
 *   0x13ee60  object_propagate_flag_to_children
 *   0x13eff0  object_remove_from_name_list
 *   0x13f060  objects_place
 *   0x13f810  objects_initialize
 *   0x13f950  objects_initialize_for_new_map
 *   0x13f9f0  objects_dispose_from_old_map
 *   0x13fac0  objects_dispose
 *   0x13fb30  object_activate
 *   0x13fb80  object_deactivate (object deactivate)
 *   0x13fc20  object_placement_data_new (object placement data init)
 *   0x13fd00  object_disconnect_from_map
 *   0x13fef0  object_has_node
 *   0x13ff50  object_set_automatic_deactivation (object set/clear hidden)
 *   0x13ffc0  object_set_garbage
 *   0x140160  object_set_region_count
 *   0x140230  object_adjust_interpolation_position
 *   0x140420  object_find_in_cluster
 *   0x1407e0  object_visible_to_any_player
 *   0x140bc0  object_delete_internal
 *   0x140cc0  object_delete
 *   0x140ce0  object_connect_to_map
 *   0x140eb0  object_get_node_matrix
 *   0x140f10  object_get_marker_by_name
 *   0x141020  object_compute_child_marker_position
 *   0x1412f0  object_get_world_position
 *   0x141360  object_get_orientation (object orientation getter)
 *   0x141480  object_get_world_matrix
 *   0x1415f0  object_find_in_radius
 *   0x141b70  object_compute_node_matrices
 *   0x143ae0  object_set_position (object reposition)
 *   0x143be0  object_translate (set object position and reconnect to map)
 *   0x143c80  object_new (object_new — create from placement)
 *   0x144240  object_attach_to_parent
 *   0x1446a0  object_update_children_recursive
 *   0x144860  object_attach_to_marker
 *   0x144b30  objects_garbage_collection (delete and immediately deactivate)
 *   0x145170  objects_update
 */

#include "common.h"

/* Forward declarations for unported callees in the same .obj cluster. */
typedef void (*pfn_void_t)(void);
typedef void (*pfn_int_t)(int);
typedef int (*valid_real_point3d_fn)(float *p);
typedef void (*object_type_validate_fn)(int16_t type);

/* game engine tag-index remapping helpers (called from
 * game_engine_remap_object_definition). Binary: each takes 1 cdecl int arg,
 * returns int in EAX. */
int game_engine_remap_vehicle(int tag_index);
int game_engine_remap_weapon(int tag_index);
int weapon_definition_index_to_list_index(int param_1);

/*
 * object_set_position — reposition an object and recompute its orientation.
 *
 * Disconnects the object from the map, optionally updates its position
 * (forward vector at obj+0x0C) and facing direction (at obj+0x24).
 * If a target (up) vector is provided, it is copied directly to obj+0x30.
 * Otherwise, a perpendicular up vector is computed from the facing via:
 *   temp = {facing.y, -facing.x, 0.0}
 *   normalize(temp)
 *   if degenerate: temp = {1, 0, 0}
 *   up = cross(temp, facing)
 * Then recomputes node matrices and reconnects to the map.
 *
 * Confirmed: 4 cdecl args (object_handle, facing, target, flags).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (handle, -1).
 * Confirmed: CALL 0x13fd00 (object_disconnect_from_map) with 1 stack arg.
 * Confirmed: CALL 0x13010 (normalize3d) for perpendicular temp vector.
 * Confirmed: cross product computed via x87 FPU in-line (not a function call).
 * Confirmed: CALL 0x141b70 (object_compute_node_matrices).
 * Confirmed: CALL 0x140ce0 (object_connect_to_map) with (handle, 0).
 * Confirmed: FCOMP against REAL_ZERO_POOL (0.0f) for degenerate check.
 */
/* Allocate widget data pool, then call each widget type's initialize function.
 * 0x135f90 / objects.obj
 */
void widgets_initialize(void)
{
  short sVar1;
  void **ppuVar2;

  *(data_t **)0x5a90c4 = game_state_data_new("widget", 0x40, 0xc);
  if (*(data_t **)0x5a90c4 == 0) {
    display_assert("widget_data",
                   "c:\\halo\\SOURCE\\objects\\widgets\\widgets.c", 0x2e, 1);
    system_exit(-1);
  }
  sVar1 = 0;
  ppuVar2 = (void **)0x323530;
  do {
    if ((sVar1 < 0) || (sVar1 >= 5)) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }
    if (ppuVar2[-2] == 0) {
      display_assert("type_definition->group_tag",
                     "c:\\halo\\SOURCE\\objects\\widgets\\widgets.c", 0x37, 1);
      system_exit(-1);
    }
    if (*ppuVar2 != 0) {
      ((void (*)(void)) * ppuVar2)();
    }
    sVar1 = sVar1 + 1;
    ppuVar2 = ppuVar2 + 10;
  } while (sVar1 < 5);
}

/* Reset widget data pool, then call each widget type's initialize_for_new_map.
 * 0x136040 / objects.obj
 */
void widgets_initialize_for_new_map(void)
{
  short sVar1;
  void **ppuVar2;

  data_delete_all(*(data_t **)0x5a90c4);
  sVar1 = 0;
  ppuVar2 = (void **)0x323534;
  do {
    if ((sVar1 < 0) || (sVar1 >= 5)) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }
    if (*ppuVar2 != 0) {
      ((void (*)(void)) * ppuVar2)();
    }
    sVar1 = sVar1 + 1;
    ppuVar2 = ppuVar2 + 10;
  } while (sVar1 < 5);
}

/* Call each widget type's dispose_from_old_map, then invalidate widget data
 * pool. 0x1360a0 / objects.obj
 */
void widgets_dispose_from_old_map(void)
{
  short sVar1;
  void **ppuVar2;

  sVar1 = 0;
  ppuVar2 = (void **)0x323538;
  do {
    if ((sVar1 < 0) || (sVar1 >= 5)) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }
    if (*ppuVar2 != 0) {
      ((void (*)(void)) * ppuVar2)();
    }
    sVar1 = sVar1 + 1;
    ppuVar2 = ppuVar2 + 10;
  } while (sVar1 < 5);
  data_make_invalid(*(data_t **)0x5a90c4);
}

/* Call each widget type's dispose function.
 * 0x136100 / objects.obj
 */
void widgets_dispose(void)
{
  short sVar1;
  void **ppuVar2;

  sVar1 = 0;
  ppuVar2 = (void **)0x32353c;
  do {
    if ((sVar1 < 0) || (sVar1 >= 5)) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }
    if (*ppuVar2 != 0) {
      ((void (*)(void)) * ppuVar2)();
    }
    sVar1 = sVar1 + 1;
    ppuVar2 = ppuVar2 + 10;
  } while (sVar1 < 5);
}

void widgets_new(int object_handle)
{
  int *obj;
  char *tag_data;
  int *widget_block; /* tag block at tag+0x14c */
  int *element;
  int widget_handle;
  char *widget;
  char *widget_definition;
  int definition_handle;
  int16_t i;
  int16_t type;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  tag_data = (char *)tag_get(0x6f626a65, obj[0]);

  widget_block = (int *)(tag_data + 0x14c);

  /* Initialize widget list head to NONE. */
  *(int *)((char *)obj + 0x11c) = -1;

  if (*widget_block <= 0)
    return;

  for (i = 0; (int)i < *widget_block; i++) {
    element = (int *)tag_block_get_element(widget_block, (int)i, 0x20);

    /* Search the widget_types table for a matching group_tag. */
    for (type = 0; type < 5; type++) {
      if (*(int *)(0x323528 + (int)type * 0x28) == element[0]) {
        if (type != -1 && element[3] != -1) {
          /* Assert: type is in valid range [0, NUMBER_OF_WIDGET_TYPES). */
          if (type < 0 || type >= 5) {
            display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                           "c:\\halo\\source\\objects\\widgets\\widget_types.h",
                           0x96, 1);
            system_exit(-1);
          }

          widget_definition = (char *)(0x323528 + (int)type * 0x28);

          /* Allocate a new widget datum. */
          widget_handle = data_new_at_index(*(data_t **)0x5a90c4);
          if (widget_handle != -1) {
            widget = (char *)datum_get(*(data_t **)0x5a90c4, widget_handle);

            /* Store the widget type. */
            *(int16_t *)(widget + 0x2) = type;

            /* Check if this widget type has a "new" function (entry+0x18). */
            if (*(int (**)(int))(widget_definition + 0x18) != 0) {
              /* Call the widget type's new function with the definition index.
               */
              definition_handle =
                (*(int (**)(int))(widget_definition + 0x18))(element[3]);
              *(int *)(widget + 0x4) = definition_handle;
              if (definition_handle == -1) {
                /* New function failed — delete the widget datum. */
                datum_delete(*(data_t **)0x5a90c4, widget_handle);
              } else {
                /* Success — link into the object's widget list. */
                *(int *)(widget + 0x8) = *(int *)((char *)obj + 0x11c);
                *(int *)((char *)obj + 0x11c) = widget_handle;
              }
            } else {
              /* No new function — link directly with definition = NONE. */
              *(int *)(widget + 0x8) = *(int *)((char *)obj + 0x11c);
              *(int *)((char *)obj + 0x11c) = widget_handle;
              *(int *)(widget + 0x4) = -1;
            }
          }
        }
        break;
      }
    }
  }
}

/* widgets_delete — delete all widgets from an object's widget list.
 * Walks the linked list of widgets at obj+0x11c, calling each widget type's
 * delete_proc (at widget_types[type]+0x1c) if the widget has a valid
 * definition handle, then deletes the widget datum from the pool.
 *
 * Widget structure (0xc bytes):
 *   +0x02: type (int16_t) - index into widget_types table
 *   +0x04: definition_handle (int) - handle returned by new_proc, or -1
 *   +0x08: next_widget_handle (int) - linked list next pointer
 *
 * Widget types table at 0x323528 (5 entries, 0x28 bytes each):
 *   +0x00: group_tag
 *   +0x18: new_proc
 *   +0x1c: delete_proc
 *
 * Source: c:\halo\SOURCE\objects\widgets\widgets.c (line 0xbe)
 * Assert: c:\halo\source\objects\widgets\widget_types.h (line 0x96)
 */
void widgets_delete(int object_handle)
{
  int *obj;
  int widget_handle;
  char *widget;
  int next_handle;
  int16_t type;

  obj = (int *)object_get_and_verify_type(object_handle, -1);

  widget_handle = *(int *)((char *)obj + 0x11c);
  if (widget_handle == -1) {
    *(int *)((char *)obj + 0x11c) = -1;
    return;
  }

  do {
    widget = (char *)datum_get(*(data_t **)0x5a90c4, widget_handle);
    type = *(int16_t *)(widget + 0x2);

    /* Assert: type is in valid range [0, NUMBER_OF_WIDGET_TYPES). */
    if (type < 0 || type >= 5) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }

    next_handle = *(int *)(widget + 0x8);

    /* If this widget has a valid definition handle, call delete_proc. */
    if (*(int *)(widget + 0x4) != -1) {
      if (*(int (**)(int))(0x323528 + (int)type * 0x28 + 0x1c) == 0) {
        display_assert("type_definition->delete_proc",
                       "c:\\halo\\SOURCE\\objects\\widgets\\widgets.c", 0xbe,
                       1);
        system_exit(-1);
      }
      (*(void (**)(int))(0x323528 + (int)type * 0x28 + 0x1c))(
        *(int *)(widget + 0x4));
    }

    datum_delete(*(data_t **)0x5a90c4, widget_handle);
    widget_handle = next_handle;
  } while (next_handle != -1);

  *(int *)((char *)obj + 0x11c) = -1;
}

/* widgets_need_lighting (0x1363d0 / objects.obj) — walk a widget's chain
 * looking for a widget whose type is flagged in the widget_types table,
 * returning a one-byte status.
 *
 * Starting from the widget handle in param_1, follows the chain link at
 * widget+0x8 until either a flagged widget is found or the chain reaches
 * NONE (-1). For each widget the type (int16_t at widget+0x2) is asserted to
 * be in [0, NUMBER_OF_WIDGET_TYPES) and used to index the widget_types table
 * at 0x323528 (5 entries, 0x28 bytes each); the byte at entry+0x4 is the
 * flag tested.
 *
 * Returns 1 (low byte set) on the first widget whose type flag is non-zero.
 * Returns 0xffffff00 (low byte clear) when the chain ends at NONE without a
 * match — matches the original's EAX: the leftover -1 chain handle in the
 * high three bytes with AL cleared to 0.
 */
int widgets_need_lighting(int widget_index)
{
  int widget_type_table;
  data_t **widget_data;
  char *widget;
  int16_t type;

  widget_data = (data_t **)0x5a90c4;
  while (widget_index != -1) {
    widget = (char *)datum_get(*widget_data, widget_index);
    type = *(int16_t *)(widget + 0x2);
    widget_type_table = 0x323528;

    /* Assert: type is in valid range [0, NUMBER_OF_WIDGET_TYPES). */
    if (type < 0 || type >= 5) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }

    if (*(char *)(widget_type_table + (int)type * 0x28 + 0x4) != '\0') {
      return 1;
    }
    widget_index = *(int *)(widget + 0x8);
  }
  return (int)0xffffff00;
}

void object_initialize_vitality(int object_handle,
                                float *body_vitality_override,
                                float *shield_vitality_override)
{
  int *obj;
  int objtag;
  int coll;
  float body_vitality;
  float shield_vitality;

  obj = (int *)object_get_and_verify_type(object_handle, 0xffffffff);
  objtag = (int)tag_get(0x6f626a65, obj[0]);

  shield_vitality = 0.0f;
  body_vitality = 0.0f;
  if (*(int *)(objtag + 0x7c) != -1) {
    coll = (int)tag_get(0x636f6c6c, *(int *)(objtag + 0x7c));
    if (coll != 0) {
      body_vitality = *(float *)(coll + 0x8);
      shield_vitality = *(float *)(coll + 0xcc);
    }
  }

  if (body_vitality_override != (float *)0)
    body_vitality = *body_vitality_override;
  if (shield_vitality_override != (float *)0)
    shield_vitality = *shield_vitality_override;

  *(float *)((char *)obj + 0x8c) = shield_vitality;
  *(float *)((char *)obj + 0x88) = body_vitality;
  *(float *)((char *)obj + 0x90) = (body_vitality > 0.0f) ? 1.0f : 0.0f;
  if (shield_vitality > 0.0f)
    *(float *)((char *)obj + 0x94) = 1.0f;
  else
    *(int *)((char *)obj + 0x94) = 0;
}

float object_get_maximum_body_vitality(int object_handle, char use_raw_max)
{
  char *obj;
  float max_vitality;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  max_vitality = *(float *)(obj + 0x88);

  if (use_raw_max == 0) {
    return FUN_000b55b0(1, (int)*(unsigned short *)(obj + 0x68)) * max_vitality;
  }

  return max_vitality;
}

/* Disconnects a point light from the map (assert text). */
void light_disconnect_from_map(int object_handle)
{
  light_datum_t *light = light_get(object_handle);
  uint16_t flags = light->flags;

  if ((flags & POINT_LIGHT_FLAG_CONNECTS_TO_MAP) == 0)
    return;
  if ((flags & POINT_LIGHT_FLAG_CONNECTED_TO_MAP) == 0) {
    display_assert("TEST_FLAG(light->flags, _point_light_connected_to_map_bit)",
                   "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x4d0, 1);
    system_exit(-1);
  }

  cluster_partition_remove_object(light_cluster_partition, object_handle,
                                  &light->cluster_reference);
  *(uint8_t *)&light->flags &= ~POINT_LIGHT_FLAG_CONNECTED_TO_MAP;
}

/*
 * lights_disconnect_from_structure_bsp (0x139740 / object_lights.c) —
 * detach every map-connected light from the light cluster partition before
 * a structure-BSP switch, leaving the connected bit set so that
 * lights_reconnect_to_structure_bsp (0x13b150) re-adds it afterwards.
 *
 * Confirmed (disasm 0x139740-0x1397ef): data_next_index/datum_get over the
 * light table *(data_t **)0x5a90bc (global re-read every call); outer gate is
 * a BYTE test of bit 0x4 at light+0x2; the body of light_disconnect_from_map (0x1396e0) is
 * inlined verbatim (second datum_get, word flags, bit 0x2 gate, bit 0x4
 * assert at object_lights.c 0x4d0, cluster_partition_remove_object on
 * 0x5a90b0 with light+0x10, AND byte 0xfb); then OR byte [light+2],0x4.
 * Confirmed: only reference is slot 2 (0-based) of the structure-BSP
 * disconnect table at 0x326a44 (walked by 0x18e240 and 0x18eb40).
 * Name: PAL 2342 objects/object_lights.c lights_disconnect_from_structure_bsp
 * (same assert line 0x4d0 in its callee light_disconnect_from_map) and the
 * matching slot of scenario_structure_bsp_disconnect_proc_table (T2).
 * Inferred: 0x1396e0 is PAL light_disconnect_from_map; it is
 * called here, as in PAL, and defined above so MSVC can inline it.
 */
void lights_disconnect_from_structure_bsp(void)
{
  int index;
  char *light;

  for (index = data_next_index(*(data_t **)0x5a90bc, -1); index != -1;
       index = data_next_index(*(data_t **)0x5a90bc, index)) {
    light = (char *)datum_get(*(data_t **)0x5a90bc, index);
    if ((*(uint8_t *)(light + 0x2) & 0x4) != 0) {
      light_disconnect_from_map(index);
      *(uint8_t *)(light + 0x2) |= 0x4;
    }
  }
}

float light_attenuation(float radius, float distance)
{
  return 1.0f - (distance * distance) / (radius * radius);
}

void brighten_real_rgb_color(float *color /* @<ecx> */, float scale)
{
  float max_comp;
  float factor;

  max_comp = (color[0] > ((color[1] > color[2]) ? color[1] : color[2])) ?
               color[0] :
               ((color[1] > color[2]) ? color[1] : color[2]);

  factor = scale + 1.0f;

  if (factor * max_comp > 1.0f) {
    factor = 1.0f / max_comp;
  } else if (factor * max_comp < scale) {
    factor = scale / max_comp;
  }

  color[0] = color[0] * factor;
  color[1] = color[1] * factor;
  color[2] = color[2] * factor;
}

void cluster_get_first_light(int *iterator, int cluster_index)
{
  cluster_partition_iter_first(light_cluster_partition, iterator,
                               (int16_t)cluster_index);
}

void cluster_get_next_light(int *iterator)
{
  cluster_partition_iter_next(light_cluster_partition, iterator);
}

int light_unmarked(int light_index)
{
  light_datum_t *light = light_get(light_index);

  if (!lights_globals.marker_initialized) {
    display_assert("lights_globals.marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x66f, 1);
    system_exit(-1);
  }
  return light->marker != lights_globals.marker;
}

int light_mark(int light_index)
{
  light_datum_t *light = light_get(light_index);

  if (!lights_globals.marker_initialized) {
    display_assert("lights_globals.marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x67f, 1);
    system_exit(-1);
  }
  if (light->marker != lights_globals.marker) {
    light->marker = lights_globals.marker;
    return 1;
  }
  return 0;
}

/* light_marker_end (0x1399f0)
 *
 * Closes the light marking pass: asserts the pass is open, then clears the
 * marker_initialized flag.  No references to the out-of-line copy exist; the
 * same body is inlined into its caller. */
void light_marker_end(void)
{
  if (!lights_globals.marker_initialized) {
    display_assert("lights_globals.marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x68e, 1);
    system_exit(-1);
  }
  lights_globals.marker_initialized = 0;
}

void render_debug_light(int light_index)
{
  light_datum_t *light;
  light_definition_t *definition;
  real_argb_color color;
  real radius;

  if (!debug_lights)
    return;

  light = light_get(light_index);
  definition = light_definition_get(light->definition_index);
  color = *global_real_argb_orange;
  radius = definition->radius_modifier_upper * definition->radius;
  FUN_00189540(1, &light->position, definition->lens_flare_only_radius,
               global_real_argb_white);
  FUN_00189540(1, &light->position, light->radius, &color);
  color.red *= REAL_0_8_POOL;
  color.green *= REAL_0_8_POOL;
  color.blue *= REAL_0_8_POOL;
  if (!(definition->flags & LIGHT_DEFINITION_FLAG_NO_SPECULAR)) {
    radius *= definition->specular_radius_multiplier;
    FUN_00189540(1, &light->position,
                 definition->specular_radius_multiplier * light->radius, &color);
  }
  color.red *= REAL_0_8_POOL;
  color.green *= REAL_0_8_POOL;
  color.blue *= REAL_0_8_POOL;
  FUN_00189540(1, &light->position, radius, &color);
}

void lights_queue_lens_flare(int definition_index, int *position, int direction,
                             int up, float *color, float scale)
{
  int iVar1;
  char *base;
  int *vec;

  if ((*(short *)0x5a90ac < MAXIMUM_QUEUED_LENS_FLARES) &&
      ((color[0] != REAL_ZERO_POOL || color[1] != REAL_ZERO_POOL) ||
       color[2] != REAL_ZERO_POOL)) {
    iVar1 = *(short *)0x5a90ac * 0x28;
    base = (char *)0x5a8f6c + iVar1;

    *(unsigned int *)(base + 0x18) = real_a_rgb_color_to_pixel32(1.0f, color);
    base[0x23] = (char)FUN_00180770(scale);
    *(void **)(base + 0x00) = tag_get(0x6c656e73, definition_index);
    vec = (int *)(base + 0x04);
    vec[0] = position[0];
    vec[1] = position[1];
    vec[2] = position[2];
    /* permuter 20260721 (+4.2pp raw): offset 0x14 and the counter address
     * held in iVar1 across uses (register-role reuse, value-identical).
     * NB: rank-1 winner also mutated the +0x1c fill to 0xFF — REJECTED as a
     * semantic corruption; 0xffff kept. */
    iVar1 = 0x14;
    *(int *)(base + 0x10) = compress_real_vector3d_to_int32_clamp((float *)direction);
    *(int *)(base + iVar1) = compress_real_vector3d_to_int32_clamp((float *)up);
    base[0x22] = render_window_index;
    iVar1 = 0x5a90ac;
    *(short *)(base + 0x1e) = (short)0xffff;
    *(short *)(base + 0x1c) = (short)0xffff;
    *(short *)(base + 0x20) = *(short *)iVar1;
    *(short *)iVar1 = *(short *)iVar1 + 1;
  }
}

/* light_indices and light_attenuations may alias one caller buffer. */
void find_point_lights_for_object_in_cluster(
  int object_handle, int16_t cluster_index, float *center, float bias,
  int light_indices, float *light_intensities, int light_attenuations,
  int16_t *light_count, int16_t maximum_light_count)
{
  int reference_index;
  float attenuation;
  int light_index;
  light_datum_t *light;
  int16_t slot;
  int16_t dimmest_index;
  int16_t cur_count;
  float dx, dy, dz, distance, radius;
  float minimum_intensity, intensity;
  int slot_offset;
  int marker;

  if (!lights_globals.marker_initialized) {
    display_assert("lights_globals.marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x544, 1);
    system_exit(-1);
  }

  light_index = cluster_partition_iter_first(light_cluster_partition,
                                             &reference_index, cluster_index);
  while (light_index != -1) {
    light = light_get(light_index);
    if (!lights_globals.marker_initialized) {
      display_assert("lights_globals.marker_initialized",
                     "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x66f, 1);
      system_exit(-1);
    }
    if (light->marker != lights_globals.marker) {
      light = light_get(light_index);
      if (light->rasterizer_light_index != -1 &&
          (light->object_index != object_handle ||
           !(light_definition_get(light->definition_index)->flags &
             LIGHT_DEFINITION_FLAG_DONT_LIGHT_OWN_OBJECT))) {
        dx = center[0] - light->position.x;
        dy = center[1] - light->position.y;
        dz = center[2] - light->position.z;
        /* Keep this sum order; it is the original's x87 order. */
        distance = sqrtf(dy * dy + (dx * dx + dz * dz));
        if (distance < bias + light->radius) {
          radius = light->radius;
          attenuation = 1.0f - (distance * distance) / (radius * radius);
          intensity =
            real_rgb_color_brightness(&light->color.red) * attenuation;

          cur_count = *light_count;
          if (cur_count < maximum_light_count) {
            slot = cur_count;
            cur_count++;
            *light_count = cur_count;
          } else {
            minimum_intensity = REAL_MAX_POOL;
            dimmest_index = -1;
            /* slot ends at *light_count unless a dimmer light wins. */
            for (slot = 0; slot < *light_count; slot++) {
              if (minimum_intensity > light_intensities[slot]) {
                minimum_intensity = light_intensities[slot];
                dimmest_index = slot;
              }
            }
            if (minimum_intensity < intensity)
              slot = dimmest_index;
          }

          if (slot < maximum_light_count) {
            slot_offset = slot * 4;
            *(int *)(light_indices + slot_offset) = light_index;
            light_intensities[slot] = intensity;
            *(float *)(light_attenuations + slot_offset) = attenuation;
          }
        }
      }
      light = light_get(light_index);
      if (!lights_globals.marker_initialized) {
        display_assert("lights_globals.marker_initialized",
                       "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x67f, 1);
        system_exit(-1);
      }
      marker = lights_globals.marker;
      if (light->marker != marker)
        light->marker = marker;
    }
    light_index = cluster_partition_iter_next(light_cluster_partition,
                                              &reference_index);
  }
}

void build_distant_lights(unsigned int flags, float *position, float *direction,
                          float distance_scale, float *color_ptr, float *output_ptr,
                          float *intensity_ptr)
{
  float value;
  float red;
  float green;
  float blue;
  float factor;
  float clamped;
  float gel_intensity;
  float half_scale;
  float sq;

  gel_intensity = real_rgb_color_brightness(intensity_ptr);

  output_ptr[0] = intensity_ptr[0] * object_light_ambient_scale + object_light_ambient_base;
  output_ptr[1] = intensity_ptr[1] * object_light_ambient_scale + object_light_ambient_base;
  value = intensity_ptr[2] * object_light_ambient_scale + object_light_ambient_base;
  *(short *)(output_ptr + 3) = 2;
  output_ptr[2] = value;
  output_ptr[4] = intensity_ptr[0];
  output_ptr[5] = intensity_ptr[1];
  output_ptr[6] = intensity_ptr[2];
  output_ptr[7] = -direction[0];
  output_ptr[8] = -direction[1];
  output_ptr[9] = -direction[2];
  output_ptr[10] = color_ptr[0] * object_light_secondary_scale * gel_intensity;
  output_ptr[11] = color_ptr[1] * object_light_secondary_scale * gel_intensity;
  output_ptr[12] = color_ptr[2] * object_light_secondary_scale * gel_intensity;
  output_ptr[13] = position[0];
  output_ptr[14] = position[1];
  output_ptr[15] = position[2];

  clamped = gel_intensity * REAL_1_5_POOL + REAL_0_25_POOL;
  if (clamped < 0.0f)
    clamped = 0.0f;
  else if (clamped > 1.0f)
    clamped = 1.0f;
  output_ptr[0x13] = clamped;

  red = color_ptr[0] * REAL_3_0_POOL + REAL_0_5_POOL;
  if (red < 0.0f)
    red = 0.0f;
  else if (red > 1.0f)
    red = 1.0f;
  output_ptr[0x14] = red;

  green = color_ptr[1] * REAL_3_0_POOL + REAL_0_5_POOL;
  if (green < 0.0f)
    green = 0.0f;
  else if (green > 1.0f)
    green = 1.0f;
  output_ptr[0x15] = green;

  blue = color_ptr[2] * REAL_3_0_POOL + REAL_0_5_POOL;
  if (blue < 0.0f)
    blue = 0.0f;
  else if (blue > 1.0f)
    blue = 1.0f;
  output_ptr[0x16] = blue;

  factor = intensity_ptr[0] + intensity_ptr[0] + REAL_0_25_POOL;
  if (factor < 0.0f)
    factor = 0.0f;
  else if (factor > 1.0f)
    factor = 1.0f;
  output_ptr[0x14] = factor * red;

  factor = intensity_ptr[1] + intensity_ptr[1] + REAL_0_25_POOL;
  if (factor < 0.0f)
    factor = 0.0f;
  else if (factor > 1.0f)
    factor = 1.0f;
  output_ptr[0x15] = factor * green;

  factor = intensity_ptr[2] + intensity_ptr[2] + REAL_0_25_POOL;
  if (factor < 0.0f)
    factor = 0.0f;
  else if (factor > 1.0f)
    factor = 1.0f;
  output_ptr[0x16] = factor * blue;

  half_scale = (float)pow((double)distance_scale, DOUBLE_0_25_POOL);
  output_ptr[0x17] = half_scale * output_ptr[7];
  output_ptr[0x18] = half_scale * output_ptr[8];

  sq = sqrtf(output_ptr[0x18] * output_ptr[0x18] +
             output_ptr[0x17] * output_ptr[0x17]);
  if (REAL_0_707_POOL <= sq) {
    value = REAL_0_707_POOL / sq;
    output_ptr[0x19] = -0.707f;
    output_ptr[0x17] = half_scale * output_ptr[7] * value;
    output_ptr[0x18] = half_scale * output_ptr[8] * value;
  } else {
    output_ptr[0x19] = -sqrtf(REAL_ONE_POOL - sq * sq);
  }

  value = (REAL_ONE_POOL - distance_scale) * REAL_0_5_POOL;
  red = (REAL_ONE_POOL - output_ptr[4] * REAL_1_3_POOL) + value;
  if (red < object_light_ambient_base)
    red = object_light_ambient_base;
  else if (red > 1.0f)
    red = 1.0f;
  output_ptr[0x1a] = red;

  red = (REAL_ONE_POOL - output_ptr[5] * REAL_1_3_POOL) + value;
  if (red < object_light_ambient_base)
    red = object_light_ambient_base;
  else if (red > 1.0f)
    red = 1.0f;
  output_ptr[0x1b] = red;

  red = (REAL_ONE_POOL - output_ptr[6] * REAL_1_3_POOL) + value;
  if (red < object_light_ambient_base)
    red = object_light_ambient_base;
  else if (red > 1.0f)
    red = 1.0f;
  output_ptr[0x1c] = red;

  if ((flags & 4) != 0) {
    brighten_real_rgb_color(output_ptr, 0.2f);
    brighten_real_rgb_color(output_ptr + 7, 0.3f);
    brighten_real_rgb_color(output_ptr + 10, 0.2f);
    brighten_real_rgb_color(output_ptr + 0x14, 0.5f);
    output_ptr[0x13] = 1.0f;
  }
}

void light_compute_bounding_sphere(int light_handle /* @<eax> */,
                                   float *out_position /* @<edi> */,
                                   float *out_radius /* @<ebx> */,
                                   char use_maximum_radius, char include_specular,
                                   char clamp_to_lens_flare_radius)
{
  light_datum_t *light = light_get(light_handle);
  light_definition_t *definition = light_definition_get(light->definition_index);
  float radius;

  if (use_maximum_radius != '\0') {
    radius = definition->radius_modifier_upper * definition->radius;
  } else {
    radius = light->radius;
  }

  if ((*(unsigned char *)&definition->flags &
       LIGHT_DEFINITION_FLAG_NO_SPECULAR) == 0) {
    if (include_specular != '\0' || use_maximum_radius != '\0') {
      radius = radius * definition->specular_radius_multiplier;
    }
  }

  if (clamp_to_lens_flare_radius != '\0' &&
      radius < definition->lens_flare_only_radius) {
    out_position[0] = light->position.x;
    out_position[1] = light->position.y;
    out_position[2] = light->position.z;
    *out_radius = definition->lens_flare_only_radius;
    return;
  }

  if (definition->cutoff_angle < REAL_HALF_PI_POOL) {
    if (definition->cutoff_angle < REAL_QUARTER_PI_POOL) {
      *out_radius = (radius = radius / definition->cosine_cutoff_angle);
    } else {
      *out_radius = radius * definition->sine_cutoff_angle;
      radius = radius * definition->cosine_cutoff_angle;
    }
    out_position[0] = radius * light->forward.i + light->position.x;
    out_position[1] = radius * light->forward.j + light->position.y;
    out_position[2] = radius * light->forward.k + light->position.z;
  } else {
    out_position[0] = light->position.x;
    out_position[1] = light->position.y;
    out_position[2] = light->position.z;
    *out_radius = radius;
  }
}

void light_get_bounding_sphere(int light_index, float *out_center,
                               float *out_radius)
{
  light_datum_t *light = light_get(light_index);
  light_definition_t *definition = light_definition_get(light->definition_index);
  float radius = definition->radius_modifier_upper * definition->radius;

  if (!(definition->flags & LIGHT_DEFINITION_FLAG_NO_SPECULAR)) {
    radius = radius * definition->specular_radius_multiplier;
  }
  if (radius < definition->lens_flare_only_radius) {
    out_center[0] = light->position.x;
    out_center[1] = light->position.y;
    out_center[2] = light->position.z;
    out_radius[0] = definition->lens_flare_only_radius;
    return;
  }
  if (definition->cutoff_angle < 1.5707964f) {
    if (definition->cutoff_angle < 0.7853982f) {
      radius = radius / definition->cosine_cutoff_angle;
      out_radius[0] = radius;
    } else {
      out_radius[0] = radius * definition->sine_cutoff_angle;
      radius = radius * definition->cosine_cutoff_angle;
    }
    out_center[0] = radius * light->forward.i + light->position.x;
    out_center[1] = radius * light->forward.j + light->position.y;
    out_center[2] = radius * light->forward.k + light->position.z;
    return;
  }
  out_center[0] = light->position.x;
  out_center[1] = light->position.y;
  out_center[2] = light->position.z;
  out_radius[0] = radius;
}

void lights_render_diffuse(void)
{
  int16_t i;
  int loop_idx;
  light_datum_t *light;
  light_datum_t *light_reloaded;
  light_definition_t *definition;
  char is_specular;
  int gel_count;
  float position[3];
  float radius;
  int16_t gel_buffer[MAXIMUM_CLUSTERS_PER_LIGHT];

  FUN_0017cc50();

  if (*(char *)*(int *)0x46f074 == '\0')
    goto done;
  if (game_engine_allow_dynamic_lighting() == '\0')
    goto done;

  loop_idx = 0;
  if (lights_globals.scene_point_light_count <= 0)
    goto done;

  do {
    int saved_idx;
    saved_idx = loop_idx;
    i = (int16_t)loop_idx;

    light = light_get(*(int *)(0x5a8d6c + (int)i * 4));

    if ((light->flags & POINT_LIGHT_FLAG_DYNAMIC) == 0 ||
        light->rasterizer_light_index == -1) {
      loop_idx = saved_idx + 1;
      continue;
    }

    if ((light->flags & POINT_LIGHT_FLAG_ATTACHED_TO_FIRST_PERSON_WEAPON) != 0) {
      definition = light_definition_get(light->definition_index);
      is_specular = 1;
      if ((*(unsigned char *)&definition->flags &
           LIGHT_DEFINITION_FLAG_SUPERSIZE_IN_FIRST_PERSON) == 0) {
        is_specular = 0;
      }
    } else {
      is_specular = 0;
    }

    gel_count = 0;
    if (is_specular == '\0') {
      gel_count = light_build_cluster_array(*(int *)(0x5a8d6c + (int)i * 4),
                                            gel_buffer,
                                            MAXIMUM_CLUSTERS_PER_LIGHT);
    }

    light_reloaded = light_get(*(int *)(0x5a8d6c + (int)i * 4));
    definition = light_definition_get(light_reloaded->definition_index);
    radius = light_reloaded->radius;

    if (definition->cutoff_angle < REAL_HALF_PI_POOL) {
      if (definition->cutoff_angle < REAL_QUARTER_PI_POOL) {
        radius = radius / definition->cosine_cutoff_angle;
        position[0] = radius * light_reloaded->forward.i +
                      light_reloaded->position.x;
        position[1] = radius * light_reloaded->forward.j +
                      light_reloaded->position.y;
        position[2] = radius * light_reloaded->forward.k +
                      light_reloaded->position.z;
      } else {
        float inner_scale;
        radius = radius * definition->sine_cutoff_angle;
        inner_scale = light_reloaded->radius * definition->cosine_cutoff_angle;
        position[0] = inner_scale * light_reloaded->forward.i +
                      light_reloaded->position.x;
        position[1] = inner_scale * light_reloaded->forward.j +
                      light_reloaded->position.y;
        position[2] = inner_scale * light_reloaded->forward.k +
                      light_reloaded->position.z;
      }
    } else {
      position[0] = light_reloaded->position.x;
      position[1] = light_reloaded->position.y;
      position[2] = light_reloaded->position.z;
    }

    FUN_00196060(light->rasterizer_light_index, position, radius, gel_count,
                 (int)((unsigned int)((is_specular != '\0') - 1) &
                       (unsigned int)gel_buffer));

    loop_idx = saved_idx + 1;
  } while ((int16_t)loop_idx < lights_globals.scene_point_light_count);

done:
  FUN_0017cc90();
}

void lights_render_specular(void)
{
  int16_t i;
  int loop_idx;
  light_datum_t *light;
  light_definition_t *definition;
  char is_specular;
  int gel_count;
  float position[3];
  float radius;
  int16_t gel_buffer[MAXIMUM_CLUSTERS_PER_LIGHT];

  FUN_0017cd50();

  if (*(char *)*(int *)0x46f074 == '\0')
    goto done;
  if (game_engine_allow_dynamic_lighting() == '\0')
    goto done;

  loop_idx = 0;
  if (lights_globals.scene_point_light_count <= 0)
    goto done;

  do {
    int saved_idx;
    saved_idx = loop_idx;
    i = (int16_t)loop_idx;

    light = light_get(*(int *)(0x5a8d6c + (int)i * 4));

    if ((light->flags & POINT_LIGHT_FLAG_DYNAMIC) == 0 ||
        light->rasterizer_light_index == -1) {
      goto next;
    }

    definition = light_definition_get(light->definition_index);
    if ((*(unsigned char *)&definition->flags & LIGHT_DEFINITION_FLAG_NO_SPECULAR) != 0) {
      goto next;
    }

    if ((light->flags & POINT_LIGHT_FLAG_ATTACHED_TO_FIRST_PERSON_WEAPON) != 0) {
      definition = light_definition_get(light->definition_index);
      is_specular = 1;
      if ((*(unsigned char *)&definition->flags & LIGHT_DEFINITION_FLAG_SUPERSIZE_IN_FIRST_PERSON) == 0) {
        is_specular = 0;
      }
    } else {
      is_specular = 0;
    }

    gel_count = 0;
    if (is_specular == '\0') {
      gel_count = (int)light_build_cluster_array(
        *(int *)(0x5a8d6c + (int)i * 4), gel_buffer, MAXIMUM_CLUSTERS_PER_LIGHT);
    }

    light_compute_bounding_sphere(*(int *)(0x5a8d6c + (int)i * 4),
                                  position, &radius, 0, 1, 0);

    {
      int gel_buf_arg;
      if (is_specular != '\0') {
        gel_buf_arg = 0;
      } else {
        gel_buf_arg = (int)gel_buffer;
      }
      FUN_00195f30(light->rasterizer_light_index, position, radius, gel_count,
                   gel_buf_arg);
    }

    loop_idx = saved_idx;
  next:
    loop_idx = loop_idx + 1;
  } while ((int16_t)loop_idx < lights_globals.scene_point_light_count);

done:
  FUN_0017cd90();
}

/* 0x13a740 */
void lights_illumination_at_point(int point, int location, float *color)
{
  float value;

  *(real_vector3d *)color = **(real_vector3d **)0x2ee710;
  {
    short lightmap_index;
    short material_index;
    int surface_index;
    float s;
    float t;
    float collision_point[3];

    if (structure_test_vector((float *)point, (float *)0x29b204, collision_point,
                              &lightmap_index, &material_index, &surface_index,
                              &s, &t)) {
      char *structure = (char *)scenario_get();
      short *lightmap = (short *)tag_block_get_element(
        structure + 0x104, lightmap_index, 0x20);
      char *material = (char *)tag_block_get_element(lightmap + 10,
                                                     material_index, 0x100);

      if (*(int *)(structure + 0xc) != -1 && *lightmap != -1) {
        void *bitmap = FUN_00076ff0(*(int *)(structure + 0xc), *lightmap);
        unsigned short *surface = (unsigned short *)tag_block_get_element(
          structure + 0xf8, surface_index, 6);

        if (*(short *)(material + 0xc4) != 2 &&
            *(short *)(material + 0xc4) != 3) {
          display_assert("material->lightmap_vertices.type==_rasterizer_vertex_"
                         "type_environment_lightmap_uncompressed || "
                         "material->lightmap_vertices.type==_rasterizer_vertex_"
                         "type_environment_lightmap_compressed",
                         "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x356, 1);
          system_exit(-1);
        }
        if (xbox_texture_cache_get_hardware_format(bitmap, 0, 0)) {
          FUN_00138fd0((int)material, (int)bitmap, surface, s, t, color);
        }
      }
    }
  }

  if (*(short *)(location + 4) != -1) {
    int light_indices[MAXIMUM_RENDERED_POINT_LIGHTS];
    float light_intensities[MAXIMUM_RENDERED_POINT_LIGHTS];
    float light_attenuations[MAXIMUM_RENDERED_POINT_LIGHTS];
    short light_count = 0;
    short light_index;

    if (lights_globals.marker_initialized != '\0') {
      display_assert("!lights_globals.marker_initialized",
                     "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x664, 1);
      system_exit(-1);
    }
    lights_globals.marker = lights_globals.marker + 1;
    lights_globals.marker_initialized = '\x01';
    find_point_lights_for_object_in_cluster(
      -1, (int16_t)*(unsigned short *)(location + 4), (float *)point, 0.0f,
      (int)light_indices, light_intensities, (int)light_attenuations,
      &light_count, MAXIMUM_RENDERED_POINT_LIGHTS);
    if (lights_globals.marker_initialized == '\0') {
      display_assert("lights_globals.marker_initialized",
                     "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x68e, 1);
      system_exit(-1);
    }
    lights_globals.marker_initialized = '\0';
    for (light_index = 0; light_index < light_count; light_index++) {
      light_datum_t *light = light_get(light_indices[light_index]);

      if ((light->flags & POINT_LIGHT_FLAG_DYNAMIC) != 0) {
        color[0] += light->color.red * light_attenuations[light_index];
        color[1] += light->color.green * light_attenuations[light_index];
        color[2] += light->color.blue * light_attenuations[light_index];
      }
    }
  }

  if (color[0] < REAL_ZERO_POOL) {
    value = REAL_ZERO_POOL;
  } else if (color[0] > REAL_ONE_POOL) {
    value = REAL_ONE_POOL;
  } else {
    value = color[0];
  }
  color[0] = value;

  if (color[1] < REAL_ZERO_POOL) {
    value = REAL_ZERO_POOL;
  } else if (color[1] > REAL_ONE_POOL) {
    value = REAL_ONE_POOL;
  } else {
    value = color[1];
  }
  color[1] = value;

  if (color[2] < REAL_ZERO_POOL) {
    value = 0.0f;
  } else if (color[2] > REAL_ONE_POOL) {
    value = 1.0f;
  } else {
    value = color[2];
  }
  color[2] = value;
}

void lights_prepare_for_object_dynamic(int object_index, int lighting)
{
  float center[3];
  float radius;
  unsigned int iterator[2];
  float light_intensities[2];
  float light_attenuations[2];
  short *light_count;
  short cluster_index;
  short i;
  light_datum_t *light;

  object_get_bounding_sphere(object_index, center, &radius);
  light_count = (short *)(lighting + 0x40);
  *light_count = 0;

  if (lights_globals.marker_initialized != '\0') {
    display_assert("!lights_globals.marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x664, 1);
    system_exit(-1);
  }
  lights_globals.marker = lights_globals.marker + 1;
  lights_globals.marker_initialized = '\x01';

  cluster_index = object_get_first_cluster(iterator, object_index);
  if (cluster_index != -1) {
    do {
      find_point_lights_for_object_in_cluster(object_index, cluster_index, center, radius,
                                              lighting + 0x44, light_intensities,
                                              (int)light_attenuations, light_count, MAXIMUM_RENDERED_POINT_LIGHTS);
      cluster_index = object_get_next_cluster(iterator, object_index);
    } while (cluster_index != -1);
  }

  if (lights_globals.marker_initialized == '\0') {
    display_assert("lights_globals.marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x68e, 1);
    system_exit(-1);
  }
  i = 0;
  lights_globals.marker_initialized = '\0';

  if (*light_count > 0) {
    do {
      light = light_get(*(int *)(lighting + 0x44 + (int)i * 4));
      *(int *)(lighting + 0x44 + (int)i * 4) = light->rasterizer_light_index;
      i++;
    } while (i < *light_count);
  }
}

char lights_distant_lighting_at_point(unsigned int flags, int point,
                                      int *lighting)
{
  char hit;
  int structure;
  short *lightmap;
  int iVar4;
  unsigned short *surface;
  int iVar6;
  short ray_count;
  short ray_index;
  int *source;
  int *dest;
  char *rays;
  char collision_point[12];
  char local_88[12];
  char local_7c[12];
  char local_70[12];
  char local_64[12];
  char local_58[12];
  char local_4c[12];
  float local_40[3];
  int surface_index;
  int local_30;
  float local_2c;
  float local_28;
  float local_24;
  short material_index[2];
  short lightmap_index[2];
  short *lightmap_block;
  int local_14;
  float t;
  float s;
  float distance_scale;
  char result;

  result = 0;
  structure = (int)scenario_get();
  source = (int *)(structure + 0x2c);
  iVar6 = 0x1d;
  dest = lighting;
  if (*(float *)(structure + 0x2c) != REAL_ZERO_POOL) {
    memcpy(dest, source, iVar6 * 4);
    *(short *)(lighting + 3) = 2;
  } else {
    source = (int *)0x29b190;
    memcpy(dest, source, iVar6 * 4);
  }

  if ((flags & 1) == 0) {
    rays = (char *)0x29b204;
    ray_count = 1;
  } else {
    rays = (char *)0x29b210;
    ray_count = 4;
  }
  ray_index = 0;
  if (ray_count != 0) {
    do {
      hit =
        structure_test_vector((float *)point, (float *)(rays + (int)ray_index * 0xc), (float *)collision_point,
                              lightmap_index, material_index, (int32_t *)&surface_index, &s, &t);
      if (hit != '\0')
        break;
      ray_index = ray_index + 1;
    } while (ray_index < ray_count);
    if (ray_index >= ray_count) {
      return result;
    }
    structure = (int)scenario_get();
    lightmap = (short *)tag_block_get_element((void *)(structure + 0x104),
                                            (int)lightmap_index[0], 0x20);
    lightmap_block = lightmap;
    iVar6 = (int)tag_block_get_element(lightmap + 10, (int)material_index[0], 0x100);
    iVar4 = (int)tag_get(TAG_GROUP_SHDR, *(int *)(iVar6 + 0xc));
    if (*(short *)(iVar4 + 0x24) == 3 &&
        (local_14 = (int)FUN_001906b0((void *)iVar4, 3),
         *(int *)(structure + 0xc) != -1) &&
        *lightmap != -1 && *(int *)(local_14 + 0x94) != -1) {
      surface = (unsigned short *)tag_block_get_element((void *)(structure + 0xf8),
                                                       surface_index, 6);
      structure = (int)FUN_00076ff0(*(int *)(structure + 0xc), *lightmap_block);
      iVar4 = (int)tag_get(TAG_GROUP_BITM, *(int *)(local_14 + 0x94));
      local_14 = (int)FUN_00076ff0(*(int *)(local_14 + 0x94),
                                   (int)*(short *)(iVar6 + 0x10) %
                                     *(int *)(iVar4 + 0x60));
      if (structure != 0 && local_14 != 0 &&
          (iVar4 = FUN_00138ee0(structure), iVar4 != 0) &&
          (iVar4 = FUN_00138ee0(local_14), iVar4 != 0)) {
        FUN_001390d0(iVar6, local_14, surface, s, t,
                          (void *)local_88);
        FUN_00138fd0(iVar6, structure, surface, s, t, local_40);
        FUN_00180570((unsigned int)*surface * 0x20 + *(int *)(iVar6 + 0xf8),
                          (void *)local_64);
        FUN_00180570((unsigned int)surface[1] * 0x20 +
                            *(int *)(iVar6 + 0xf8),
                          (void *)local_58);
        FUN_00180570((unsigned int)surface[2] * 0x20 +
                            *(int *)(iVar6 + 0xf8),
                          (void *)local_4c);
        FUN_00138f70((float *)local_7c, (float *)local_4c, (float *)local_58,
                     (float *)local_64, s, t);
        normalize3d((void *)local_7c);
        FUN_00180660((unsigned int *)(*(int *)(iVar6 + 0xf8) +
                                           ((unsigned int)*surface +
                                            *(int *)(iVar6 + 0xb4) * 4) *
                                             8),
                          (float *)local_64);
        FUN_00180660((unsigned int *)(*(int *)(iVar6 + 0xf8) +
                                           ((unsigned int)surface[1] +
                                            *(int *)(iVar6 + 0xb4) * 4) *
                                             8),
                          (float *)local_58);
        FUN_00180660((unsigned int *)(*(int *)(iVar6 + 0xf8) +
                                           ((unsigned int)surface[2] +
                                            *(int *)(iVar6 + 0xb4) * 4) *
                                             8),
                          (float *)local_4c);
        local_2c = (float)normalize3d((void *)local_64);
        local_28 = (float)normalize3d((void *)local_58);
        local_24 = (float)normalize3d((void *)local_4c);
        FUN_00138f70((float *)local_70, (float *)local_4c, (float *)local_58,
                     (float *)local_64, s, t);
        distance_scale = (local_24 - local_2c) * t +
                         (local_28 - local_2c) * s + local_2c;
        normalize3d((void *)local_70);
        if (debug_object_lights != '\0') {
          local_2c = local_40[0];
          local_28 = local_40[1];
          local_24 = local_40[2];
          local_30 = 0x3f800000;
          FUN_00189150(1, (float *)point, 0.5f, &local_30);
          {
            int ds_bits;
            memcpy(&ds_bits, &distance_scale, 4);
            ((void (*)(int, void *, void *, int, void *))FUN_00189320)(
              1, (void *)point, local_70, ds_bits, &local_30);
          }
        }
        build_distant_lights(flags, (float *)local_7c, (float *)local_70,
                             distance_scale, (float *)local_88,
                             (float *)lighting, (float *)local_40);
        result = 1;
      }
    }
  }
  return result;
}

/* Reconnects a point light to the map (assert text). */
void light_reconnect_to_map(int object_handle)
{
  light_datum_t *light;
  char *parent_obj;
  void *node_matrix;
  char marker_buf[0x6c]; /* output from object_get_marker_by_name */
  char location[8]; /* scenario location (cluster_index etc.) */
  real_point3d position; /* computed light position */
  float radius; /* committed effective range */
  float range; /* pre-clamp range temp (stays ST0-resident like the ref) */
  uint8_t tag_flags;
  float offset;

  light = light_get(object_handle);
  light_definition_get(light->definition_index);

  if (light->field_58 == -1) {
    /* nested call: the outer call pushes its trailing args first. */
    object_get_marker_by_name(light->object_index,
                                    (char *)object_get_attachment_marker_name(
                                      light->object_index,
                                      light->attachment_marker_index),
                                    marker_buf, 1);

    /* position, forward and up copy as dword moves, not x87 loads. */
    /* position <- marker_buf+0x60 */
    light->position = *(real_point3d *)(marker_buf + 0x60);
    /* forward  <- marker_buf+0x3c */
    light->forward = *(real_vector3d *)(marker_buf + 0x3c);
    /* up       <- marker_buf+0x54 */
    light->up = *(real_vector3d *)(marker_buf + 0x54);
  } else {
    parent_obj =
      (char *)object_try_and_get_and_verify_type(light->object_index, -1);
    if (parent_obj != 0) {
      node_matrix =
        object_get_node_matrix(light->object_index,
                               light->attachment_marker_index);
      matrix_transform_point((float *)node_matrix, &light->attachment.relative_position.x,
                             &light->position.x);
      matrix_transform_vector((float *)node_matrix, &light->relative_forward.i,
                              &light->forward.i);
      perpendicular3d(&light->forward.i, &light->up.i);
      normalize3d(&light->up.i);
    }
  }

  if ((light->flags & POINT_LIGHT_FLAG_CONNECTS_TO_MAP) == 0)
    return;

  {
    light_datum_t *light2 = light_get(object_handle);
    light_definition_t *definition =
      light_definition_get(light2->definition_index);

    tag_flags = *(uint8_t *)&definition->flags;
    range = definition->radius_modifier_upper * definition->radius;

    if ((tag_flags & LIGHT_DEFINITION_FLAG_NO_SPECULAR) == 0)
      range = range * definition->specular_radius_multiplier;

    if (range < definition->lens_flare_only_radius) {
      /* The computed range is discarded; the radius is copied as raw bits. */
      position = light2->position;
      *(int *)&radius = *(int *)&definition->lens_flare_only_radius;
    } else {
      /* the angle is reloaded for each compare; NaN takes the outer else. */
      if (definition->cutoff_angle < REAL_HALF_PI_POOL) {
        if (definition->cutoff_angle < REAL_QUARTER_PI_POOL) {
          radius = range / definition->cosine_cutoff_angle;
          position.x =
            radius * light2->forward.i + light2->position.x;
          position.y =
            radius * light2->forward.j + light2->position.y;
          position.z =
            radius * light2->forward.k + light2->position.z;
        } else {
          /* offset stays in ST0 across the three components. */
          radius = range * definition->sine_cutoff_angle;
          offset = range * definition->cosine_cutoff_angle;
          position.x =
            offset * light2->forward.i + light2->position.x;
          position.y =
            offset * light2->forward.j + light2->position.y;
          position.z =
            offset * light2->forward.k + light2->position.z;
        }
      } else {
        radius = range;
        position = light2->position;
      }
    }

    if ((*(uint8_t *)&light->flags & POINT_LIGHT_FLAG_CONNECTED_TO_MAP) != 0) {
      display_assert(
        "!TEST_FLAG(light->flags, _point_light_connected_to_map_bit)",
        "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x4f9, 1);
      system_exit(-1);
    }

    if (light->object_index == -1 ||
        object_try_and_get_and_verify_type(light->object_index, -1) == 0) {
      scenario_location_from_point((void *)location, (void *)&position);
    } else {
      object_get_location(light->object_index, (void *)location);
    }

    /* cluster_partition_add_object takes the radius as raw float bits. */
    cluster_partition_add_object(light_cluster_partition, object_handle,
                                 (void *)&light->cluster_reference,
                                 (void *)&position,
                                 *(uint32_t *)&radius,
                                 (void *)location);

    light->flags |= POINT_LIGHT_FLAG_CONNECTED_TO_MAP;
  }
}

void lights_reconnect_to_structure_bsp(void)
{
  int index;
  light_datum_t *light;

  index = data_next_index(light_data, -1);
  while (index != -1) {
    light = light_get(index);
    if ((light->flags & POINT_LIGHT_FLAG_CONNECTED_TO_MAP) != 0) {
      light->flags &= ~POINT_LIGHT_FLAG_CONNECTED_TO_MAP;
      light_reconnect_to_map(index);
    }
    index = data_next_index(light_data, index);
  }
}

int light_new(int tag_index, int object_handle, int16_t attachment_index, int16_t function_index,
              int16_t color_function_index)
{
  light_definition_t *definition;
  int handle;
  light_datum_t *light;
  uint16_t flag;

  definition = light_definition_get(tag_index);
  handle = -1;
  if (((*(unsigned char *)&definition->flags & LIGHT_DEFINITION_FLAG_DYNAMIC) != 0 ||
       definition->lens_flare_index != -1) &&
      (handle = data_new_at_index(light_data), handle != -1)) {
    light = light_get(handle);
    light->definition_index = tag_index;
    light->object_index = object_handle;
    light->function_index = function_index;
    light->flags = 0;
    light->attachment_marker_index = attachment_index;
    light->attachment.color_function_index = color_function_index;
    light->flags = (uint16_t)(*(unsigned char *)&definition->flags &
                                LIGHT_DEFINITION_FLAG_DYNAMIC);
    flag = light->flags;
    if ((flag & POINT_LIGHT_FLAG_DYNAMIC) == 0 && definition->lens_flare_index == -1)
      flag &= ~POINT_LIGHT_FLAG_CONNECTS_TO_MAP;
    else
      flag |= POINT_LIGHT_FLAG_CONNECTS_TO_MAP;
    light->flags = flag;
    light->cluster_reference = -1;
    light->field_58 = -1;
    light_reconnect_to_map(handle);
    light->marker = lights_globals.marker - 1;
  }
  return handle;
}

int light_new_unattached(int tag_index, int object_handle, int16_t marker,
                         float *position, float *forward, int scale)
{
  int handle;
  light_datum_t *light;

  handle = data_new_at_index(light_data);
  if (handle != -1) {
    light = light_get(handle);
    light_definition_get(tag_index);
    light->flags = 0;
    {
      int tick = game_time_get();
      *(uint8_t *)&light->flags |=
        POINT_LIGHT_FLAG_DYNAMIC | POINT_LIGHT_FLAG_CONNECTS_TO_MAP;
      light->field_58 = tick;
    }
    light->definition_index = tag_index;
    light->object_index = object_handle;
    *(int *)&light->scale = scale;
    light->cluster_reference = -1;

    if (object_handle == -1) {
      *(float *)((char *)light + 0x30) = position[0];
      *(float *)((char *)light + 0x34) = position[1];
      *(float *)((char *)light + 0x38) = position[2];
      *(float *)((char *)light + 0x3c) = forward[0];
      *(float *)((char *)light + 0x40) = forward[1];
      *(float *)((char *)light + 0x44) = forward[2];
    } else {
      light->attachment_marker_index = marker;
      *(float *)((char *)light + 0x60) = position[0];
      *(float *)((char *)light + 0x64) = position[1];
      *(float *)((char *)light + 0x68) = position[2];
      *(float *)((char *)light + 0x6c) = forward[0];
      *(float *)((char *)light + 0x70) = forward[1];
      *(float *)((char *)light + 0x74) = forward[2];
    }

    light_reconnect_to_map(handle);
    light->marker = lights_globals.marker - 1;
  }
  return handle;
}

void lights_preprocess_scene(void)
{
  float *color;
  float value;
  char adjusted;
  short sVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  short *rendered_cluster;
  unsigned char *definition;
  char *puVar10;
  int uVar11;
  unsigned char *pbVar12;
  char *marker;
  short marker_index;
  float transition;
  char markers[924]; /* markers[60]+local_3a4[864] must be contiguous (MSVC
                          stack alias) */
  char *local_3a4 = markers + 60;
  int light_params[14]; /* must be contiguous — rasterizer reads as 56-byte
                           struct */
  int light_handle;
  lens_flare_parameters_t lens_flare;
  int game_time;
  float local_18;
  int object;
  int index;
  float local_c;
  float local_8;

  game_time = game_time_get();
  if (*(char *)0x449ef1 != '\0' && *(char *)0x323608 != '\0') {
    profile_enter_private((int *)0x323600);
  }
  debug_rasterizer_light_count = 0;
  for (iVar5 = data_next_index(light_data, -1); iVar5 != -1;
       iVar5 = data_next_index(light_data, iVar5)) {
    iVar6 = (int)datum_get(light_data, iVar5);
    *(unsigned char *)(iVar6 + 2) = *(unsigned char *)(iVar6 + 2) & 0xf7;
    *(int *)(iVar6 + 8) = -1;
    if (*(int *)(iVar6 + 0x58) != -1) {
      iVar7 = (int)tag_get(TAG_GROUP_LIGH, *(int *)(iVar6 + 4));
      index = game_time - *(int *)(iVar6 + 0x58);
      if ((float)index > *(float *)(iVar7 + 0xf4)) {
        iVar6 = (int)datum_get(light_data, iVar5);
        cluster_partition_remove_object(light_cluster_partition, iVar5, (void *)(iVar6 + 0x10));
        datum_delete(light_data, iVar5);
      } else {
        iVar6 = CALL_FUN_0013d640(*(int *)(iVar6 + 0x2c), -1);
        if (iVar6 != 0) {
          light_disconnect_from_map(iVar5);
          light_reconnect_to_map(iVar5);
        }
      }
    }
  }
  if (lights_globals.marker_initialized != '\0') {
    display_assert("!lights_globals.marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x664, 1);
    system_exit(-1);
  }
  lights_globals.marker = lights_globals.marker + 1;
  lights_globals.marker_initialized = '\x01';
  lights_globals.scene_point_light_count = FUN_00196c90(
    (int)lights_globals.scene_point_lights, MAXIMUM_SCENE_POINT_LIGHTS, (void *)0x1398b0, (void *)0x1398d0,
    (void *)0x13a340, (void *)0x139930, (void *)0x139990);
  if (lights_globals.marker_initialized == '\0') {
    display_assert("lights_globals.marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x68e, 1);
    system_exit(-1);
  }
  lights_globals.marker_initialized = 0;
  FUN_001812b0();
  iVar5 = 0;
  if (0 < rendered_cluster_count) {
    do {
      rendered_cluster = rendered_cluster_get(iVar5);
      rasterizer_lens_flare_submit_for_cluster(*rendered_cluster);
      iVar5 = iVar5 + 1;
    } while ((short)iVar5 < rendered_cluster_count);
  }
  index = 0;
  if (0 < lights_globals.scene_point_light_count) {
    do {
      uVar11 = lights_globals.scene_point_lights[(short)index];
      light_handle = uVar11;
      iVar5 = (int)datum_get(light_data, uVar11);
      definition = (unsigned char *)tag_get(TAG_GROUP_LIGH, *(int *)(iVar5 + 4));
      local_c = 1.0f;
      render_debug_light(uVar11);
      if (*(int *)(iVar5 + 0x2c) == -1) {
        iVar6 = 0;
      } else {
        iVar6 = CALL_FUN_0013d640(*(int *)(iVar5 + 0x2c), -1);
      }
      object = iVar6;
      if (*(int *)(iVar5 + 0x58) == -1) {
        if (iVar6 == 0) {
          display_assert("object", "c:\\halo\\SOURCE\\objects\\object_lights.c",
                         0x1ac, 1);
          system_exit(-1);
        }
        object_get_function_value(*(int *)(iVar5 + 0x2c),
                          *(unsigned short *)(iVar5 + 0x5e), &local_8);
        puVar10 = *(char **)0x2ee708;
        if (*(short *)(iVar5 + 0x60) != -1) {
          puVar10 = (char *)(iVar6 + (*(short *)(iVar5 + 0x60) + 0x1e) * 0xc);
        }
        FUN_0007c490((float *)(iVar5 + 0x14), *(int *)(definition + 0x34), (float *)(definition + 0x38),
                         (float *)(definition + 0x48), (float *)puVar10, local_8);
      } else {
        local_18 = (float)(game_time - *(int *)(iVar5 + 0x58));
        transition = (float)transition_function_evaluate(*(unsigned short *)(definition + 0xfa),
                                          (float)(int)local_18 /
                                            *(float *)(definition + 0xf4));
        local_8 = (REAL_ONE_POOL - transition) * *(float *)(iVar5 + 0x78);
        FUN_0007c270(
          (float *)(iVar5 + 0x14), *(unsigned int *)(definition + 0x34),
          (float *)(definition + 0x3c), (float *)(definition + 0x4c), local_8);
      }
      local_18 = REAL_ONE_POOL - local_8;
      if (!(*(float *)(iVar5 + 0x14) >= REAL_ZERO_POOL &&
            *(float *)(iVar5 + 0x14) <= REAL_ONE_POOL)) {
        display_assert("light->color.red >=0.0f && light->color.red <=1.0f",
                       "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x1bb, 1);
        system_exit(-1);
      }
      if (!(*(float *)(iVar5 + 0x18) >= REAL_ZERO_POOL &&
            *(float *)(iVar5 + 0x18) <= REAL_ONE_POOL)) {
        display_assert("light->color.green>=0.0f && light->color.green<=1.0f",
                       "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x1bc, 1);
        system_exit(-1);
      }
      if (!(*(float *)(iVar5 + 0x1c) >= REAL_ZERO_POOL &&
            *(float *)(iVar5 + 0x1c) <= REAL_ONE_POOL)) {
        display_assert("light->color.blue >=0.0f && light->color.blue <=1.0f",
                       "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x1bd, 1);
        system_exit(-1);
      }
      if (object != 0) {
        uVar11 = object_get_root_parent(*(int *)(iVar5 + 0x2c));
        iVar6 = (int)object_get_and_verify_type(uVar11, -1);
        if ((1 << (*(unsigned char *)(iVar6 + 100) & 0x1f) & 3u) != 0) {
          iVar6 = (int)object_get_and_verify_type(uVar11, 3);
          if (*(float *)(iVar6 + 0x32c) > REAL_ZERO_POOL) {
            pbVar12 = (unsigned char *)tag_get(TAG_GROUP_LIGH, *(int *)(iVar5 + 4));
            if ((*pbVar12 & LIGHT_DEFINITION_FLAG_DONT_FADE_ACTIVE_CAMOUFLAGE) == 0) {
              local_c = REAL_ONE_POOL - *(float *)(iVar6 + 0x32c);
              value = local_c * *(float *)(iVar5 + 0x14);
              *(float *)(iVar5 + 0x14) = value;
              *(float *)(iVar5 + 0x18) = local_c * *(float *)(iVar5 + 0x18);
              *(float *)(iVar5 + 0x1c) = local_c * *(float *)(iVar5 + 0x1c);
              if (!(value >= REAL_ZERO_POOL &&
                    value <= REAL_ONE_POOL)) {
                display_assert(
                  "light->color.red >=0.0f && light->color.red <=1.0f",
                  "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x1d1, 1);
                system_exit(-1);
              }
              if (!(*(float *)(iVar5 + 0x18) >= REAL_ZERO_POOL &&
                    *(float *)(iVar5 + 0x18) <= REAL_ONE_POOL)) {
                display_assert(
                  "light->color.green>=0.0f && light->color.green<=1.0f",
                  "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x1d2, 1);
                system_exit(-1);
              }
              if (!(*(float *)(iVar5 + 0x1c) >= REAL_ZERO_POOL &&
                    *(float *)(iVar5 + 0x1c) <= REAL_ONE_POOL)) {
                display_assert(
                  "light->color.blue >=0.0f && light->color.blue <=1.0f",
                  "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x1d3, 1);
                system_exit(-1);
              }
            }
          }
        }
      }
      color = (float *)(iVar5 + 0x14);
      if (*(float *)(iVar5 + 0x14) != REAL_ZERO_POOL ||
          *(float *)(iVar5 + 0x18) != REAL_ZERO_POOL ||
          *(float *)(iVar5 + 0x1c) != REAL_ZERO_POOL) {
        if ((*(unsigned char *)(iVar5 + 2) & POINT_LIGHT_FLAG_DYNAMIC) == 0) {
          *(int *)(iVar5 + 0x54) = *(int *)(definition + 4);
        } else {
          value = (local_8 * *(float *)(definition + 0xc) +
                   local_18 * *(float *)(definition + 8)) *
                  *(float *)(definition + 4);
          *(float *)(iVar5 + 0x54) = value;
          if (value != REAL_ZERO_POOL) {
            light_params[0] = (int)tag_get(TAG_GROUP_LIGH, *(int *)(iVar5 + 4));
            light_params[1] = *(int *)(iVar5 + 0x30);
            light_params[2] = *(int *)(iVar5 + 0x34);
            light_params[3] = *(int *)(iVar5 + 0x38);
            light_params[4] = *(int *)(iVar5 + 0x3c);
            light_params[5] = *(int *)(iVar5 + 0x40);
            light_params[6] = *(int *)(iVar5 + 0x44);
            light_params[7] = *(int *)(iVar5 + 0x48);
            light_params[8] = *(int *)(iVar5 + 0x4c);
            light_params[9] = *(int *)(iVar5 + 0x50);
            light_params[13] = *(int *)(iVar5 + 0x54);
            *(float *)&light_params[10] = *color;
            light_params[11] = *(int *)(iVar5 + 0x18);
            light_params[12] = *(int *)(iVar5 + 0x1c);
            if (!(*color >= REAL_ZERO_POOL &&
                  *color <= REAL_ONE_POOL)) {
              display_assert(
                "light->color.red >=0.0f && light->color.red <=1.0f",
                "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x1ee, 1);
              system_exit(-1);
            }
            if (!(*(float *)(iVar5 + 0x18) >= REAL_ZERO_POOL &&
                  *(float *)(iVar5 + 0x18) <= REAL_ONE_POOL)) {
              display_assert(
                "light->color.green>=0.0f && light->color.green<=1.0f",
                "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x1ef, 1);
              system_exit(-1);
            }
            if (!(*(float *)(iVar5 + 0x1c) >= REAL_ZERO_POOL &&
                  *(float *)(iVar5 + 0x1c) <= REAL_ONE_POOL)) {
              display_assert(
                "light->color.blue >=0.0f && light->color.blue <=1.0f",
                "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x1f0, 1);
              system_exit(-1);
            }
            if (*(int *)(iVar5 + 0x58) == -1) {
              if ((*definition & LIGHT_DEFINITION_FLAG_FIRST_PERSON_FLASHLIGHT) == 0) {
                if (*(short *)(object + 100) == 2 &&
                    *(int *)(object + 0xcc) != -1) {
                  adjusted = first_person_weapon_adjust_light(
                    *(int *)(iVar5 + 0x2c),
                    (int)object_get_attachment_marker_name(
                      *(int *)(iVar5 + 0x2c), *(unsigned short *)(iVar5 + 0x5c)),
                    &light_params[1], &light_params[4], &light_params[7]);

                  if (adjusted != '\0')
                    goto LAB_0013baac;
                }
              } else {
                first_person_weapon_center_flashlight(
                  *(int *)(iVar5 + 0x2c), (float *)&light_params[1],
                  (float *)&light_params[4], &light_params[7]);
              LAB_0013baac:
                *(unsigned char *)(iVar5 + 2) =
                  *(unsigned char *)(iVar5 + 2) | POINT_LIGHT_FLAG_ATTACHED_TO_FIRST_PERSON_WEAPON;
              }
            }
            uVar11 = rasterizer_lights_submit(light_params);
            *(int *)(iVar5 + 8) = uVar11;
            debug_rasterizer_light_count = (short)uVar11 + 1;
          }
        }
        if (*(int *)(definition + 0xb8) != -1) {
          lens_flare.definition =
            tag_get(0x6c656e73, *(int *)(definition + 0xb8));
          lens_flare.compressed_light_color =
            real_a_rgb_color_to_pixel32(local_c, (float *)(iVar5 + 0x14));
          lens_flare.compressed_light_scale = FUN_00180770(local_8);
          lens_flare.light_index = (short)light_handle;
          lens_flare.light_identifier =
            (short)((unsigned int)light_handle >> 0x10);
          lens_flare.compressed_window_index = *(unsigned char *)0x50654a;
          if (lens_flare.light_identifier == 0) {
            display_assert("lens_flare_parameters.light_identifier!=0",
                           "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x21c,
                           1);
            system_exit(-1);
          }
          if (lens_flare.light_identifier == -1) {
            lens_flare.light_identifier = 0;
          }
          if (*(int *)(iVar5 + 0x58) == -1) {
            uVar11 = (int)object_get_attachment_marker_name(
              *(int *)(iVar5 + 0x2c), *(unsigned short *)(iVar5 + 0x5c));
            if (*(short *)(object + 100) == 2 &&
                *(int *)(object + 0xcc) != -1) {
              sVar4 =
                first_person_weapon_get_marker_by_name_render(*(int *)(iVar5 + 0x2c), (void *)uVar11, markers, MAXIMUM_LENS_FLARES_PER_LIGHT);
              if (0 < sVar4) {
                lens_flare.compressed_window_index =
                  lens_flare.compressed_window_index | 0x80;
              }
              if (sVar4 == 0)
                goto LAB_0013bbaf;
            } else {
            LAB_0013bbaf:
              sVar4 = object_get_marker_by_name(*(int *)(iVar5 + 0x2c), (void *)uVar11,
                                        markers, MAXIMUM_LENS_FLARES_PER_LIGHT);
            }
            marker_index = 0;
            if (0 < sVar4) {
              marker = local_3a4;
              do {
                *(int *)&lens_flare.position.x = *(int *)(marker + 0x24);
                *(int *)&lens_flare.position.y = *(int *)(marker + 0x28);
                *(int *)&lens_flare.position.z = *(int *)(marker + 0x2c);
                lens_flare.compressed_direction =
                  compress_real_vector3d_to_int32_clamp((float *)marker);
                lens_flare.compressed_up =
                  compress_real_vector3d_to_int32_clamp((float *)(marker + 0x18));
                lens_flare.lens_flare_index = marker_index;
                FUN_00181670((int *)&lens_flare);
                marker_index = marker_index + 1;
                marker = marker + 0x6c;
              } while (marker_index < sVar4);
            }
          } else {
            *(int *)&lens_flare.position.x = *(int *)(iVar5 + 0x30);
            *(int *)&lens_flare.position.y = *(int *)(iVar5 + 0x34);
            *(int *)&lens_flare.position.z = *(int *)(iVar5 + 0x38);
            lens_flare.compressed_direction =
              compress_real_vector3d_to_int32_clamp((float *)(iVar5 + 0x3c));
            lens_flare.compressed_up =
              compress_real_vector3d_to_int32_clamp((float *)(iVar5 + 0x48));
            lens_flare.lens_flare_index = 0;
            FUN_00181670((int *)&lens_flare);
          }
        }
      }
      index = index + 1;
    } while ((short)index < lights_globals.scene_point_light_count);
  }
  sVar4 = 0;
  if (0 < lights_globals.queued_lens_flare_count) {
    do {
      FUN_00181670((int *)&lights_globals.queued_lens_flares[sVar4]);
      sVar4 = sVar4 + 1;
    } while (sVar4 < lights_globals.queued_lens_flare_count);
  }
  lights_globals.queued_lens_flare_count = 0;
  FUN_00181410();
  if (*(char *)0x449ef1 != '\0' && *(char *)0x323608 != '\0') {
    profile_exit_private((int *)0x323600);
  }
}

/* lights_distant_lighting_at_point fills all of sample, so it has no pre-clear. */
void lights_prepare_for_object_static(int object_handle, float *lighting)
{
  int *obj;
  int tag_data;
  uint32_t flags;
  float sample[29];
  float offset_pos[3];
  int16_t sample_count;
  uint16_t corner;
  char ok;
  float scale;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  flags = 0;

  if (lighting == NULL) {
    display_assert("lighting", "c:\\halo\\SOURCE\\objects\\object_lights.c",
                   0x3ca, 1);
    system_exit(-1);
  }

  if ((uint8_t)(obj[1] >> 8) & 0x80)
    flags = 1;

  tag_data = (int)tag_get(0x6f626a65, *obj);
  if (*(uint8_t *)(tag_data + 2) & 4)
    flags |= 4;

  ok =
    lights_distant_lighting_at_point(flags, (int)(obj + 0x14), (int *)lighting);

  if ((obj[1] & 0x4000) != 0)
    return;

  if (ok != '\0') {
    sample_count = 1;
  } else {
    sample_count = 0;
    csmemset(lighting, 0, 0x74);
    *(uint16_t *)(lighting + 3) = 2;
  }

  for (corner = 0; (int16_t)corner < 4; corner++) {
    float xoff;
    float yoff;

    if (corner & 1) {
      xoff = REAL_SQRT_HALF_POOL;
    } else {
      xoff = REAL_NEGATIVE_SQRT_HALF_POOL;
    }
    offset_pos[0] = xoff * *(float *)(obj + 0x17) + *(float *)(obj + 0x14);

    if (corner & 2) {
      yoff = REAL_SQRT_HALF_POOL;
    } else {
      yoff = REAL_NEGATIVE_SQRT_HALF_POOL;
    }
    offset_pos[1] = yoff * *(float *)(obj + 0x17) + *(float *)(obj + 0x15);
    *(int *)(offset_pos + 2) = obj[0x16];

    ok =
      lights_distant_lighting_at_point(flags, (int)offset_pos, (int *)sample);
    if (ok != '\0') {
      sample_count++;
      lighting[0] += sample[0];
      lighting[1] += sample[1];
      lighting[2] += sample[2];
      lighting[0x13] += sample[0x13];
      lighting[0x14] += sample[0x14];
      lighting[0x15] += sample[0x15];
      lighting[0x16] += sample[0x16];
      lighting[4] += sample[4];
      lighting[5] += sample[5];
      lighting[6] += sample[6];
      lighting[7] += sample[7];
      lighting[8] += sample[8];
      lighting[9] += sample[9];
      lighting[0xa] += sample[0xa];
      lighting[0xb] += sample[0xb];
      lighting[0xc] += sample[0xc];
      lighting[0xd] += sample[0xd];
      lighting[0xe] += sample[0xe];
      lighting[0xf] += sample[0xf];
      lighting[0x1a] += sample[0x1a];
      lighting[0x1b] += sample[0x1b];
      lighting[0x1c] += sample[0x1c];
      lighting[0x17] += sample[0x17];
      lighting[0x18] += sample[0x18];
      lighting[0x19] += sample[0x19];
    }
  }

  if (sample_count > 1) {
    scale = REAL_ONE_POOL / (float)(int)sample_count;
    lighting[0] *= scale;
    lighting[1] *= scale;
    lighting[2] *= scale;
    lighting[0x13] *= scale;
    lighting[0x14] *= scale;
    lighting[0x15] *= scale;
    lighting[0x16] *= scale;
    lighting[4] *= scale;
    lighting[5] *= scale;
    lighting[6] *= scale;
    normalize3d(lighting + 7);
    lighting[0xa] *= scale;
    lighting[0xb] *= scale;
    lighting[0xc] *= scale;
    normalize3d(lighting + 0xd);
    lighting[0x1a] *= scale;
    lighting[0x1b] *= scale;
    lighting[0x1c] *= scale;
    lighting[0x17] *= scale;
    lighting[0x18] *= scale;
    lighting[0x19] *= scale;
    normalize3d(lighting + 0x17);
  } else if (sample_count == 0) {
    csmemcpy(lighting, sample, 29 * 4);
  }
}

void build_family_shadow(int object_index, int param_2, int param_3)
{
  int node;

  while (object_index != -1) {
    node = (int)object_get_and_verify_type(object_index, -1);
    object_get_and_verify_type(object_index, -1);
    build_family_shadow(*(int *)(node + 0xc8), param_2, param_3);
    object_index = *(int *)(node + 0xc4);
  }
}

/* Fills the bounding-box-style output struct param_3 from object param_1's tag
   model bounds, then recurses into child/attached objects via
   build_family_shadow, which accumulates into the struct. Returns 1 if the
   count field (param_3[7] low word) ended up > 0, else 0. */
char object_build_shadow(int param_1, int param_2, int *param_3)
{
  int *obj;
  int t;

  obj = (int *)object_get_and_verify_type(param_1, 0xffffffff);
  t = (int)tag_get(0x6f626a65, *obj);
  param_3[0] = *(int *)(t + 4);
  param_3[2] = 0xff7fffff;
  param_3[4] = 0xff7fffff;
  param_3[6] = 0xff7fffff;
  param_3[1] = 0x7f7fffff;
  param_3[3] = 0x7f7fffff;
  param_3[5] = 0x7f7fffff;
  *(short *)(param_3 + 7) = 0;
  *(short *)((char *)param_3 + 0x1e) = 0;
  object_get_and_verify_type(param_1, 0xffffffff);
  build_family_shadow(obj[0x32], param_2, (int)param_3);
  if (*(short *)(param_3 + 7) > 0) {
    return 1;
  }
  return 0;
}

/* 0x13c100 / objects.obj */
void *object_type_definition_get(int16_t object_type)
{
  int iVar1;

  if ((object_type < 0) || (object_type >= 0xc)) {
    display_assert(csprintf((char *)0x5ab100,
                            "#%d isn't a valid object type in [#0,#%d)",
                            (int)object_type, 0xc),
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x277, 1);
    system_exit(-1);
  }
  iVar1 = (int)object_type;
  if (((void **)0x324608)[iVar1] == (void *)0) {
    display_assert("object_type_definitions[object_type]",
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x278, 1);
    system_exit(-1);
  }
  if (*(int *)((char *)((void **)0x324608)[iVar1] + 4) == 0) {
    display_assert("object_type_definitions[object_type]->group_tag",
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x279, 1);
    system_exit(-1);
  }
  return ((void **)0x324608)[iVar1];
}

/* 0x13c1b0 / objects.obj — object type definition field accessor.
 *
 * Validates the object type is in [0, 0xc) and that its definition pointer in
 * the object_type_definitions table (0x324608) is non-NULL, then returns the
 * int16_t at definition+8. Same validation shape as sibling
 * object_type_get_name (two asserts: bounds at object_types.c:0x282, NULL at
 * object_types.c:0x283).
 *
 * Confirmed: bounds check param_1 < 0 || 0xb < param_1.
 * Confirmed: table object_type_definitions at 0x324608 (array of pointers).
 * Confirmed: csprintf assert uses file "object_types.c" (NOT objects.c).
 * Confirmed: returns *(short *)(definition + 8).
 */
short object_type_get_datum_size(short param_1)
{
  int iVar1;

  if (param_1 < 0 || param_1 >= 0xc) {
    display_assert(csprintf((char *)0x5ab100,
                            "#%d isn't a valid object type in [#0,#%d)",
                            (int)param_1, 0xc),
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x282, 1);
    system_exit(-1);
  }
  iVar1 = (int)param_1;
  if (((void **)0x324608)[iVar1] == (void *)0) {
    display_assert("object_type_definitions[object_type]",
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x283, 1);
    system_exit(-1);
  }
  return *(short *)((char *)((void **)0x324608)[iVar1] + 8);
}

/* 0x13c250 / objects.obj */
void *object_type_get_name(int16_t param_1)
{
  int iVar1;

  if (param_1 < 0 || param_1 >= 0xc) {
    display_assert(csprintf((char *)0x5ab100,
                            "#%d isn't a valid object type in [#0,#%d)",
                            (int)param_1, 0xc),
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x28c, 1);
    system_exit(-1);
  }
  iVar1 = (int)param_1;
  if (((void **)0x324608)[iVar1] == (void *)0) {
    display_assert("object_type_definitions[object_type]",
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x28d, 1);
    system_exit(-1);
  }
  return *(void **)((void **)0x324608)[iVar1];
}

/*
 * object_types_initialize (0x13c2e0 / object_types.c) — build the object-type
 * definition dependency list and run each definition's initialize procedure.
 *
 * Threads a singly-linked list through definition+0x9c ('next') in dependency
 * order: for each object type 0..0xb, object_type_definition_get(type) yields
 * the definition; it is appended to the list (asserting its 'next' is still
 * NONE), then each of its up to 16 child-type definitions (def+0x5c[i],
 * stopping at the first 0) is appended if not already linked. The list head is
 * kept at 0x5a8d54. After the list is terminated with a NULL next, it is walked
 * head-to-tail and each definition's initialize callback at def+0x10 (if
 * non-NULL) is invoked.
 *
 * Confirmed (disasm 0x13c2e0): head at DAT_005a8d54; assert "!definition->next"
 * (object_types.c:0x2ea) when def+0x9c != 0; inner child scan def+0x5c+i*4 for
 * i in [0,0x10); outer type loop AX<0xc; tail walk via def+0x9c calling
 * (*def+0x10)() when non-zero.
 *
 * Shape: PAL 2342 source/objects/object_types.c:630-678 -- the list tail
 * pointer is advanced to &definition->next immediately after each append
 * (outer and child loops share one cursor), and the type index is a short
 * loop counter, matching the reference register/stack assignment
 * (type in [ebp-4], cursor in EBX).
 *
 */
void object_types_initialize(void)
{
  int16_t type; /* name: PAL 2342 source/objects/object_types.c:630 */
  int *next_slot; /* where the next definition pointer is written */
  int16_t i;
  int def;
  int child;

  next_slot = (int *)0x5a8d54;
  for (type = 0; type < 0xc; type++) {
    def = (int)object_type_definition_get(type);
    if (*(int *)(def + 0x9c) != 0) {
      display_assert("!definition->next",
                     "c:\\halo\\SOURCE\\objects\\object_types.c", 0x2ea, 1);
      system_exit(-1);
    }
    *next_slot = def;
    next_slot = (int *)(def + 0x9c);

    for (i = 0; i < 0x10; i++) {
      child = *(int *)(def + 0x5c + i * 4);
      if (child == 0) {
        break;
      }
      if (*(int *)(child + 0x9c) == 0) {
        *next_slot = child;
        next_slot = (int *)(child + 0x9c);
      }
    }
  }

  *next_slot = 0;
  for (def = *(int *)0x5a8d54; def != 0; def = *(int *)(def + 0x9c)) {
    if (*(void (**)(void))(def + 0x10) != (void (*)(void))0) {
      (**(void (**)(void))(def + 0x10))();
    }
  }
}

/* Walk the object type definition list and call dispose at +0x14 on each.
 * 0x13c3a0 / objects.obj
 */
void object_types_dispose(void)
{
  int iVar1;

  for (iVar1 = *(int *)0x5a8d54; iVar1 != 0; iVar1 = *(int *)(iVar1 + 0x9c)) {
    if (*(void (**)(void))(iVar1 + 0x14) != 0) {
      (*(void (**)(void))(iVar1 + 0x14))();
    }
  }
}

/* Reset slot counter, walk the list and call initialize_for_new_map at +0x18.
 * 0x13c3d0 / objects.obj
 */
void object_types_initialize_for_new_map(void)
{
  int iVar1;

  *(int *)0x46f078 = 0;
  for (iVar1 = *(int *)0x5a8d54; iVar1 != 0; iVar1 = *(int *)(iVar1 + 0x9c)) {
    if (*(void (**)(void))(iVar1 + 0x18) != 0) {
      (*(void (**)(void))(iVar1 + 0x18))();
    }
  }
}

/* Walk the object type definition list and call dispose_from_old_map at +0x1c.
 * 0x13c400 / objects.obj
 */
void object_types_dispose_from_old_map(void)
{
  int iVar1;

  for (iVar1 = *(int *)0x5a8d54; iVar1 != 0; iVar1 = *(int *)(iVar1 + 0x9c)) {
    if (*(void (**)(void))(iVar1 + 0x1c) != 0) {
      (*(void (**)(void))(iVar1 + 0x1c))();
    }
  }
}

/* Dispatch object placement callback at vtable +0x20 for all type extensions.
 * 0x13c430 / objects.obj
 */
void object_type_adjust_placement(int object_index, void *data)
{
  char *obj;
  char *type_def;
  char *entry;
  short i;

  obj = (char *)object_get_and_verify_type(object_index, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  /* Names and loop form follow PAL 2342 object_types.c (object_type_delete). */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(void (**)(int, void *))(entry + 0x20) != NULL) {
      (*(void (**)(int, void *))(entry + 0x20))(object_index, data);
    }
  }

}

/*
 * object_type_new (0x13c490 / objects.obj) — run an object type's "can delete?"
 * predicate chain.
 *
 * Resolves the object, looks up its type definition via
 * object_type_definition_get(type), and walks the NULL-terminated array of
 * type-handler vtable pointers at type_def+0x5c. For each non-NULL handler, if
 * it has a predicate at +0x24, calls predicate(object_handle); if the predicate
 * returns false the whole function returns false. If the list is empty (first
 * entry NULL) or every predicate passes, returns true.
 *
 * Confirmed: PUSH -1, PUSH handle -> object_get_and_verify_type(handle, -1).
 * Confirmed: MOVSX EAX,[obj+0x64] -> object type, passed to
 * object_type_definition_get. Confirmed: handler list at type_def+0x5c,
 * dword-stride, NULL-terminated. Confirmed: handler predicate at handler+0x24;
 * called handler-less as predicate(handle) (one cdecl arg, ADD ESP,4).
 * Confirmed: empty list -> return 1 (CL=1 path); predicate false -> return 0.
 */
char object_type_new(int object_handle)
{
  typedef char (*type_new_callback_t)(int);
  char *obj;
  char *type_def;
  char *entry;
  char result;
  short i;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  result = 1;

  /* Loop form follows PAL 2342 object_types.c (see object_type_delete). */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(type_new_callback_t *)(entry + 0x24) != NULL &&
        !(*(type_new_callback_t *)(entry + 0x24))(object_handle)) {
      result = 0;
      break;
    }
  }
  return result;

}

/* Dispatch object type extension callback at vtable +0x28 for all extensions.
 * 0x13c500 / objects.obj
 */
void object_type_place(int object_index, int scenario_object)
{
  char *obj;
  char *type_def;
  char *entry;
  short i;

  obj = (char *)object_get_and_verify_type(object_index, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  /* Names and loop form follow PAL 2342 object_types.c (object_type_delete). */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(void (**)(int, int))(entry + 0x28) != NULL) {
      (*(void (**)(int, int))(entry + 0x28))(object_index, scenario_object);
    }
  }

}

/* Dispatch object type extension callback at vtable +0x2c for all extensions.
 * 0x13c560 / objects.obj
 */
void object_type_delete(int object_index)
{
  char *obj;
  char *type_def;
  char *entry;
  short i;

  obj = (char *)object_get_and_verify_type(object_index, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  /* Names/loop form follow PAL 2342 object_types.c: index the NULL-terminated
   * part-definition array, re-reading the slot for each part.
   */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(void (**)(int))(entry + 0x2c) != NULL) {
      (*(void (**)(int))(entry + 0x2c))(object_index);
    }
  }

}

/* 0x13c5c0 / objects.obj — dispatch the type-extension vtable callback at
 * +0x30 for every registered extension of an object's type, accumulating a
 * boolean OR of the callback results. Walks the extension table (base +0x5c)
 * obtained from the object's type via object_type_definition_get, calling each
 * non-NULL fn-ptr at *(extension+0x30) with the object handle until a NULL slot
 * ends the list. Returns AL (1 if any callback returned non-zero, else 0).
 *
 * Confirmed: cdecl, 1 arg (object_handle at [EBP+8]).
 * Confirmed: returns bool in AL (MOV AL,BL at exit).
 * Confirmed: 16-bit loop index (MOVSX EAX,SI), stride 4, base +0x5c.
 */
boolean object_type_update(int object_index)
{
  typedef char (*type_update_callback_t)(int);
  char *obj;
  char *type_def;
  char *entry;
  char result;
  short i;

  obj = (char *)object_get_and_verify_type(object_index, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  result = 0;
  /* Names and loop form follow PAL 2342 object_types.c (object_type_delete). */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(type_update_callback_t *)(entry + 0x30) != NULL &&
        (*(type_update_callback_t *)(entry + 0x30))(object_index)) {
      result = 1;
    }
  }
  return result;

}

/* Dispatch object type extension callback at vtable +0x34 for all extensions.
 * 0x13c620 / objects.obj
 */
void object_type_export_function_values(int object_index)
{
  char *obj;
  char *type_def;
  char *entry;
  short i;

  obj = (char *)object_get_and_verify_type(object_index, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  /* Names/loop form follow PAL 2342 object_types.c: index the NULL-terminated
   * part-definition array, re-reading the slot for each part.
   */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(void (**)(int))(entry + 0x34) != NULL) {
      (*(void (**)(int))(entry + 0x34))(object_index);
    }
  }

}

/* Dispatch vtable slot +0x38 for each extension in the object type's table.
 * 0x13c680 / objects.obj
 */
void object_type_handle_deleted_object(int object_index,
                                       int deleted_object_index)
{
  char *obj;
  char *type_def;
  char *entry;
  short i;
  obj = (char *)object_get_and_verify_type(object_index, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  /* Names and loop form follow PAL 2342 object_types.c (object_type_delete). */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(void (**)(int, int))(entry + 0x38) != NULL)
      (*(void (**)(int, int))(entry + 0x38))(object_index,
                                             deleted_object_index);
  }

}

/*
 * object_type_handle_region_destroyed — dispatch a region-destroyed callback
 * through the object type definition's extension table.
 *
 * Resolves the object's type, looks up its type definition via
 * object_type_definition_get, then walks the pointer array at type_def+0x5c.
 * For each non-NULL entry, reads a function pointer at entry+0x3c and calls it
 * with the original three arguments (object_handle, param_2, param_3).
 *
 * Called from damage.c (FUN_00137690) when a region is destroyed, passing
 * (object_handle, region_index, region_flags).
 *
 * Confirmed: cdecl, 3 args (ADD ESP,0xc at caller and inside loop).
 * Confirmed: MOVSX word [EAX+0x64] — reads object type as int16_t.
 * Confirmed: PUSH -1, PUSH EBX -> object_get_and_verify_type(handle, -1).
 * Confirmed: loop counter is int16_t (MOVSX EAX,SI at 0x13c728).
 * Confirmed: vtable offset 0x3c (MOV EAX,[EAX+0x3c] at 0x13c712).
 * Confirmed: indirect call passes all 3 params (PUSH ECX/EDX/EBX at
 * 0x13c71f-0x13c721).
 */
/* 0x13c6e0 */
void object_type_handle_region_destroyed(int object_handle, int region_index,
                                         unsigned int flags)
{
  typedef void (*type_callback_t)(int, int, unsigned int);
  char *obj;
  char *type_def;
  char *entry;
  int16_t i;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));

  /* Loop form follows PAL 2342 object_types.c: index the NULL-terminated
   * part-definition array, re-reading the slot for each part.
   */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(type_callback_t *)(entry + 0x3c) != NULL) {
      (*(type_callback_t *)(entry + 0x3c))(object_handle, region_index, flags);
    }
  }

}

/*
 * object_type_handle_parent_destroyed — walk the object type definition
 * extension table and check whether any extension's callback at offset +0x40
 * returns true.
 *
 * Resolves the object's type via object_get_and_verify_type(-1), looks up
 * the type definition via object_type_definition_get, then walks the
 * NULL-terminated pointer array at type_def+0x5c. For each non-NULL entry,
 * reads a function pointer at entry+0x40 and calls it with the object handle.
 * If any callback returns non-zero, the function returns 1 (sticky OR).
 *
 * Called from FUN_00136840, which recursively walks child objects. If this
 * function returns 0, the caller recurses into the child.
 *
 * Confirmed: cdecl, 1 arg (ADD ESP,0x4 after indirect CALL).
 * Confirmed: returns char/bool in AL (MOV AL,BL at 0x13c79a).
 * Confirmed: MOVSX EAX,SI — loop counter is int16_t.
 * Confirmed: vtable offset +0x40 (MOV EAX,[EAX+0x40] at 0x13c772).
 * Confirmed: XOR BL,BL — result initialized to 0, set to 1 on any true return.
 */
/* 0x13c740 */
char object_type_handle_parent_destroyed(int object_handle)
{
  typedef char (*type_check_callback_t)(int);
  char *obj;
  char *type_def;
  char *entry;
  char result;
  int16_t i;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  result = 0;

  /* Loop form follows PAL 2342 object_types.c (see object_type_delete). */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(type_check_callback_t *)(entry + 0x40) != NULL &&
        (*(type_check_callback_t *)(entry + 0x40))(object_handle)) {
      result = 1;
    }
  }
  return result;

}

/* Dispatch vtable slot +0x44 for each extension in the object type's table.
 * 0x13c7a0 / objects.obj
 */
void object_type_preprocess_node_orientations(int object_index,
                                              int node_orientations)
{
  char *obj;
  char *type_def;
  char *entry;
  short i;
  obj = (char *)object_get_and_verify_type(object_index, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  /* Names and loop form follow PAL 2342 object_types.c (object_type_delete). */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(void (**)(int, int))(entry + 0x44) != NULL) {
      (*(void (**)(int, int))(entry + 0x44))(object_index, node_orientations);
    }
  }

}

/*
 * object_type_postprocess_node_matrices — dispatch an animation-block
 * initializer callback through the object type definition's extension table.
 *
 * Resolves the object's type, looks up its type definition via
 * object_type_definition_get, then walks the NULL-terminated pointer array at
 * type_def+0x5c. For each non-NULL entry, reads a function pointer at
 * entry+0x48 and calls it with (object_handle, block_data).
 *
 * Called from object_postprocess_node_matrices after resolving the animation
 * block reference, passing the object handle and the resolved block data
 * pointer.
 *
 * Confirmed: cdecl, 2 args (ADD ESP,0x8 after indirect CALL).
 * Confirmed: MOVSX word [EAX+0x64] — reads object type as int16_t.
 * Confirmed: PUSH -1, PUSH EBX -> object_get_and_verify_type(handle, -1).
 * Confirmed: loop counter is int16_t (MOVSX EDX,SI at 0x13c844).
 * Confirmed: vtable offset 0x48 (MOV EAX,[EAX+0x48] at 0x13c832).
 * Confirmed: indirect call passes 2 params (PUSH ECX, PUSH EBX at
 * 0x13c83c-0x13c83d).
 */
/* 0x13c800 */
void object_type_postprocess_node_matrices(int object_handle, void *block_data)
{
  typedef void (*type_anim_callback_t)(int, void *);
  char *obj;
  char *type_def;
  char *entry;
  int16_t i;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));

  /* Loop form follows PAL 2342 object_types.c: index the NULL-terminated
   * part-definition array, re-reading the slot for each part.
   */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(type_anim_callback_t *)(entry + 0x48) != NULL) {
      (*(type_anim_callback_t *)(entry + 0x48))(object_handle, block_data);
    }
  }

}

/*
 * object_type_reset — dispatch per-type reset callbacks for an object.
 *
 * Looks up the object's type via object_type_definition_get(object_type), then
 * iterates the null-terminated handler array at type_data+0x5c. For each
 * handler, calls the reset function pointer at handler+0x4c with object_handle.
 * Used internally by object_reset to apply type-specific re-initialization.
 *
 * 0x13c860 / objects.obj
 */
void object_type_reset(int object_handle)
{
  char *obj;
  char *type_data;
  char *entry;
  short i;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  type_data = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  /* Loop form follows PAL 2342 object_types.c (see object_type_delete). */
  for (i = 0; ((char **)(type_data + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_data + 0x5c))[i];
    if (*(void (**)(int))(entry + 0x4c) != NULL)
      (*(void (**)(int))(entry + 0x4c))(object_handle);
  }

}

/* Dispatch vtable slot +0x50 for each extension in the object type's table.
 * 0x13c8c0 / objects.obj
 */
void object_type_disconnect_from_structure_bsp(int object_index)
{
  char *obj;
  char *type_def;
  char *entry;
  short i;

  obj = (char *)object_get_and_verify_type(object_index, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  /* Names/loop form follow PAL 2342 object_types.c: index the NULL-terminated
   * part-definition array, re-reading the slot for each part.
   */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(void (**)(int))(entry + 0x50) != NULL) {
      (*(void (**)(int))(entry + 0x50))(object_index);
    }
  }

}

/* Dispatch vtable slot +0x58 for each extension in the object type's table.
 * 0x13c920 / objects.obj
 */
void object_type_render_debug(int object_index)
{
  char *obj;
  char *type_def;
  char *entry;
  short i;

  obj = (char *)object_get_and_verify_type(object_index, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  /* Names and loop form follow PAL 2342 object_types.c (object_type_delete). */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(void (**)(int))(entry + 0x58) != NULL) {
      (*(void (**)(int))(entry + 0x58))(object_index);
    }
  }

}

/* Dispatch vtable slot +0x54 for each extension in the object type's table.
 * 0x13c980 / objects.obj
 */
void object_type_notify_impulse_sound(int object_index, int sound_index,
                                      int source_object_index)
{
  char *obj;
  char *type_def;
  char *entry;
  short i;
  obj = (char *)object_get_and_verify_type(object_index, -1);
  type_def = (char *)object_type_definition_get(*(int16_t *)(obj + 0x64));
  /* Names and loop form follow PAL 2342 object_types.c (object_type_delete). */
  for (i = 0; ((char **)(type_def + 0x5c))[i] != NULL; i++) {
    entry = ((char **)(type_def + 0x5c))[i];
    if (*(void (**)(int, int, int))(entry + 0x54) != NULL)
      (*(void (**)(int, int, int))(entry + 0x54))(object_index, sound_index,
                                                  source_object_index);
  }

}

/*
 * object_definition_index_to_object_type (0x13c9e0 / objects.obj) — find the
 * object type definition index whose group tag matches the given tag index's
 * group tag.
 *
 * Resolves the tag's group tag via tag_get_group_tag(tag_index), then scans
 * the 12 object type definitions (object_type_definition_get(i) for i in
 * 0..0xb), comparing each definition's group_tag field at +0x4. Returns the
 * matching index, or -1 (0xffff) if none match.
 *
 * Confirmed: CALL 0x1ba210 (tag_get_group_tag) with tag_index.
 * Confirmed: loop CALL 0x13c100 (object type definition get) per index.
 * Confirmed: CMP [def+0x4], group_tag.
 * Confirmed: loop bound CMP SI,0xc (int16_t counter); miss -> MOV AX,BX (-1).
 */
unsigned short object_definition_index_to_object_type(int tag_index)
{
  int group_tag;
  char *def;
  short i;

  group_tag = tag_get_group_tag(tag_index);

  i = 0;
  do {
    def = (char *)object_type_definition_get(i);
    if (*(int *)(def + 4) == group_tag) {
      return (unsigned short)i;
    }
    i = i + 1;
  } while (i < 0xc);

  return 0xffff;
}

/* Return a pointer into the scenario's placement block for an object type.
 * 0x13ca30 / objects.obj
 */
int scenario_get_object_type_scenario_datums(int param_1, int param_2,
                                             int *param_3)
{
  int iVar1;

  iVar1 = (int)object_type_definition_get((short)param_2);
  if (*(short *)(iVar1 + 10) == -1) {
    display_assert("definition->placement_tag_block_offset!=NONE",
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x4ff, 1);
    system_exit(-1);
  }
  if (((short)*(unsigned short *)(iVar1 + 10) < 0) ||
      (0x5bc < *(unsigned short *)(iVar1 + 10))) {
    display_assert(
      "definition->placement_tag_block_offset>=0 && "
      "definition->placement_tag_block_offset<=sizeof(struct scenario)"
      "+sizeof(struct tag_block)",
      "c:\\halo\\SOURCE\\objects\\object_types.c", 0x500, 1);
    system_exit(-1);
  }
  if (param_3 != (int *)0x0) {
    *param_3 = (int)*(short *)(iVar1 + 0xe);
  }
  return *(short *)(iVar1 + 10) + param_1;
}

/* Return a pointer into the scenario's palette block for an object type.
 * 0x13cab0 / objects.obj
 */
int scenario_get_object_type_scenario_palette(int param_1, int param_2)
{
  int iVar1;

  iVar1 = (int)object_type_definition_get((short)param_2);
  if (*(short *)(iVar1 + 0xc) == -1) {
    display_assert("definition->palette_tag_block_offset!=NONE",
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x50d, 1);
    system_exit(-1);
  }
  if (((short)*(unsigned short *)(iVar1 + 0xc) < 0) ||
      (0x5bc < *(unsigned short *)(iVar1 + 0xc))) {
    display_assert(
      "definition->palette_tag_block_offset>=0 && "
      "definition->palette_tag_block_offset<=sizeof(struct scenario)"
      "+sizeof(struct tag_block)",
      "c:\\halo\\SOURCE\\objects\\object_types.c", 0x50e, 1);
    system_exit(-1);
  }
  return *(short *)(iVar1 + 0xc) + param_1;
}

/*
 * object_types_disconnect_from_structure_bsp (0x13cb30 / object_types.c) —
 * delete all unnamed scenery and light-fixture objects.
 *
 * Walks the object table for type_mask 0x240 (scenery | light_fixture) via an
 * object_iter_t and deletes every matching object whose name_index (obj+0x6a)
 * is -1 (NONE) — i.e. transient placements with no scenario name; named ones
 * are left intact. Called by scenario_switch_structure_bsp (0x18eb40) during a
 * structure-BSP switch; the sibling object_types_place_objects below
 * refreshes/respawns the placements for the new BSP cluster.
 *
 * Confirmed (disasm 0x13cb30): object_iterator_new(&iter,0x240,0); first
 * object_iterator_next(&iter); while EAX!=0 { CMP word ptr [EAX+0x6a],-1
 * (name_index); JNZ skip; MOV EDX,[EBP-0x8]; object_delete(EDX); skip:
 * object_iterator_next(&iter) }. [EBP-0x8] is iter.last_handle (iterator+0x8,
 * EBP-0x10 base) — the handle written by object_iterator_next, NOT a separate
 * local (Ghidra split it as `local_c`; buffer-alias trap).
 */
void object_types_disconnect_from_structure_bsp(void)
{
  object_iter_t iter;
  short *object;

  object_iterator_new(&iter, 0x240, 0);
  object = (short *)object_iterator_next(&iter);
  while (object != (short *)0) {
    if (*(short *)((char *)object + 0x6a) == -1) {
      object_delete(iter.last_handle);
    }
    object = (short *)object_iterator_next(&iter);
  }
}

/*
 * object_types_place_objects (0x13cb80 / object_types.c) — refresh scenario
 * object placement for the currently-loaded BSP cluster slot, and (when
 * do_spawn is set) spawn the eligible placements.
 *
 * No-op when in the editor (game_in_editor()) or no BSP slot is active
 * (DAT_00326a0c == -1). Iterates object types 0..0xb, processing ONLY the mask
 * 0x240 (bits 6 and 9 = scenery and light_fixture — the BSP-cluster-scoped
 * placement types); every other type is skipped. Confirmed from the original at
 * 0x13cb80+0x49: `test $0x240,%eax; je next_type` — i.e. skip when the bit is
 * CLEAR. This is the complement of the sibling object_types_place_all, whose
 * `test $0x240,%eax; jne next_type` places all the non-BSP-scoped types at
 * scenario load. Getting this backwards leaves scenery (e.g. the teleporter
 * plasma effect) unspawned. For each processed type whose definition
 * (object_type_definition_get) has valid placement (def+0xa) and palette
 * (def+0xc) tag-block offsets:
 *   - fetches the scenario placement block
 * (scenario_get_object_type_scenario_datums, also writes the block element
 * size) and the palette base index (scenario_get_object_type_scenario_palette);
 *   - if this BSP slot has NOT yet been processed (bit (1<<DAT_00326a0c) clear
 * in DAT_0046f078): for each placement, builds a rotation matrix from the
 *     placement's Euler angles (element+0x14/+0x18/+0x1c via FUN_00109e90),
 *     stamps the placement position (element+0x8/+0xc/+0x10) as the matrix
 *     translation, transforms it (matrix_transform_point), then queries cluster
 *     membership (FUN_0018e720) for both the raw position and the transformed
 *     point; sets/clears the per-placement "in this BSP slot" flag
 * (element+0x20) accordingly;
 *   - if do_spawn != 0: runs objects_memory_compact, then for each placement
 * not already instantiated (element+0x2 NONE or object_name_list_get_handle ==
 * -1), not flagged no-spawn (element+0x4 bit0), and flagged for this slot
 *     (element+0x20 bit (1<<DAT_00326a0c)): spawns it via
 * object_new_from_scenario and runs objects_garbage_collect_tick. After all
 * types: marks this BSP slot processed (sets bit in DAT_0046f078).
 *
 * Lifecycle note: Gate B cluster edge calls object_new_from_scenario (0x144770)
 * and objects_garbage_collect_tick (0x144b50), both GC/lifecycle cluster
 * members, and mutates streaming state (DAT_0046f078 BSP-loaded mask and
 * per-placement flags element+0x20). Keep this edge covered by runtime checks.
 *
 * Confirmed: 1 cdecl arg (do_spawn @ [EBP+0x8], tested as a byte: MOV
 * AL,[EBP+0x8]). Confirmed: 12-iteration type loop (CMP SI,0xc); dual counter
 * type/shift equal. Confirmed: matrix out buffer base EBP-0x5c; translation
 * stamped at +0x28/+0x2c/ +0x30; matrix_transform_point(matrix, element+0x8,
 * &xform_point). Confirmed: bit slot = DAT_00326a0c; loaded mask = DAT_0046f078
 * (word).
 */
void object_types_place_objects(int do_spawn)
{
  unsigned char matrix[0x34]; /* EBP-0x5c: Euler matrix; translation at +0x28 */
  float xform_point[3]; /* EBP-0x28: transformed position */
  int scenario;
  int type;
  int def;
  int *block;
  int element_size; /* written by scenario_get_object_type_scenario_datums via
                       &element_size */
  int palette_base;
  short *element;
  int obj_tag;
  short index;
  int i;
  int slot_bit;

  if (game_in_editor() || (*(short *)0x326a0c == -1)) {
    return;
  }

  scenario = (int)global_scenario_get();
  type = 0;
  do {
    if (((1 << (type & 0x1f)) & 0x240) == 0) {
      goto next_type;
    }
    def = (int)object_type_definition_get((int16_t)type);
    if ((*(short *)(def + 0xa) == -1) || (*(short *)(def + 0xc) == -1)) {
      goto next_type;
    }

    block = (int *)scenario_get_object_type_scenario_datums(scenario, type,
                                                            &element_size);
    palette_base = scenario_get_object_type_scenario_palette(scenario, type);

    /* Phase 1: refresh per-placement cluster membership, once per BSP slot. */
    if (((unsigned int)*(unsigned short *)0x46f078 &
         (1 << (*(unsigned char *)0x326a0c & 0x1f))) == 0) {
      index = 0;
      if (*block > 0) {
        i = 0;
        do {
          element = (short *)tag_block_get_element(block, i, element_size);
          if (*element != -1) {
            obj_tag = (int)tag_block_get_element((void *)palette_base,
                                                 (int)*element, 0x30);
            obj_tag = (int)tag_get(0x6f626a65, *(int *)(obj_tag + 0xc));
            FUN_00109e90((float *)matrix, *(float *)((char *)element + 0x14),
                         *(float *)((char *)element + 0x18),
                         *(float *)((char *)element + 0x1c));
            /* stamp placement position into matrix translation (+0x28..+0x30)
             */
            *(int *)(matrix + 0x28) = *(int *)((char *)element + 0x8);
            *(int *)(matrix + 0x2c) = *(int *)((char *)element + 0xc);
            *(int *)(matrix + 0x30) = *(int *)((char *)element + 0x10);
            matrix_transform_point((float *)matrix, (float *)(obj_tag + 8),
                                   xform_point);
            if ((FUN_0018e720((int)((char *)element + 8)) == -1) &&
                (FUN_0018e720((int)xform_point) == -1)) {
              element[0x10] =
                (short)(element[0x10] &
                        ~(unsigned short)(1 << (*(unsigned char *)0x326a0c &
                                                0x1f)));
            } else {
              element[0x10] =
                (short)(element[0x10] |
                        (unsigned short)(1 << (*(unsigned char *)0x326a0c &
                                               0x1f)));
            }
          }
          index++;
          i = (int)index;
        } while (i < *block);
      }
    }

    /* Phase 2: spawn eligible placements (param tested as a byte in the
     * original). */
    if ((char)do_spawn != '\0') {
      objects_memory_compact();
      index = 0;
      if (*block > 0) {
        i = 0;
        do {
          element = (short *)tag_block_get_element(block, i, element_size);
          if (((*(short *)((char *)element + 2) == -1) ||
               (object_name_list_get_handle(*(short *)((char *)element + 2)) ==
                -1)) &&
              ((*(unsigned char *)((char *)element + 4) & 1) == 0) &&
              (((unsigned int)*(unsigned short *)((char *)element + 0x20) &
                (1 << (*(unsigned char *)0x326a0c & 0x1f))) != 0)) {
            object_new_from_scenario(element, palette_base);
            objects_garbage_collect_tick();
          }
          index++;
          i = (int)index;
        } while (i < *block);
      }
    }

  next_type:
    type++;
  } while ((short)type < 0xc);

  slot_bit = 1 << (*(unsigned char *)0x326a0c & 0x1f);
  *(unsigned short *)0x46f078 =
    (unsigned short)(*(unsigned short *)0x46f078 | slot_bit);
}

/*
 * object_types_place_all (0x13cdd0 / object_types.c) — place every scenario
 * palette object for all eligible object types.
 *
 * No-op when the game is in the editor (game_in_editor() != 0). Otherwise
 * iterates object types 0..0xb, skipping types in the mask 0x240 (bits 6 and
 * 9). For each remaining type whose definition (object_type_definition_get) has
 * both a valid placement tag-block offset (def+0xa != NONE) and palette
 * tag-block offset (def+0xc != NONE): fetches the scenario placement block via
 * scenario_get_object_type_scenario_datums (writing element_size to a local)
 * and the palette base index via scenario_get_object_type_scenario_palette,
 * then for each element in the block calls object_new_from_scenario
 * (object_new_from_scenario) on the element with that palette base, followed by
 * objects_garbage_collect_tick (objects_garbage_collect_tick). A final
 * object_types_place_objects(1) runs after all types are placed.
 *
 * Confirmed (disasm 0x13cdd0): game_in_editor early-out via AL; type/shift
 * dual-counter always equal (both INC each pass) so mask test is
 * (1<<type)&0x240; tag_block_get_element(block, index, element_size);
 * object_new_from_scenario(element, base); element count re-read from *block
 * each pass; tail object_types_place_objects(1).
 */
void object_types_place_all(int scenario)
{
  int type;
  int def;
  int *block; /* scenario placement tag block (count at *block) */
  int palette_base;
  int element_size; /* written by scenario_get_object_type_scenario_datums via
                       &element_size */
  int16_t index;
  int i;
  void *element;

  if (game_in_editor())
    return;

  type = 0;
  do {
    if (((1 << (type & 0x1f)) & 0x240) == 0) {
      def = (int)object_type_definition_get((int16_t)type);
      if (*(int16_t *)(def + 0xa) != -1 && *(int16_t *)(def + 0xc) != -1) {
        block = (int *)scenario_get_object_type_scenario_datums(scenario, type,
                                                                &element_size);
        palette_base =
          scenario_get_object_type_scenario_palette(scenario, type);
        index = 0;
        if (*block > 0) {
          i = 0;
          do {
            element = tag_block_get_element(block, i, element_size);
            object_new_from_scenario(element, palette_base);
            objects_garbage_collect_tick();
            index++;
            i = (int)index;
          } while (i < *block);
        }
      }
    }
    type++;
  } while ((int16_t)type < 0xc);

  object_types_place_objects(1);
}

/*
 * object_names_postprocess (0x13ce90 / object_types.c) — build the
 * object->cluster back-reference table for the loaded scenario.
 *
 * No-op when editor_flag is nonzero. Otherwise iterates object types 0..0xb.
 * For each type whose definition (object_type_definition_get) has a valid
 * placement tag-block offset (def+0xa != NONE) and palette tag-block offset
 * (def+0xc != NONE): fetches the scenario placement block via
 * scenario_get_object_type_scenario_datums (which also writes the block element
 * size to a local). For each element whose cluster reference word (element+0x2)
 * is not NONE, indexes the scenario cluster block at scenario+0x204 (stride
 * 0x24) by that reference and stamps the placement's (type, element index) back
 * into it at +0x20 / +0x22.
 *
 * Confirmed (disasm 0x13ce90): editor early-out via byte [EBP+0xc]; type loop
 * counter (EBX) is the value stamped at +0x20; element counter (ESI) is stamped
 * at +0x22; both are 16-bit stores (MOV word).
 * scenario_get_object_type_scenario_datums's 3rd arg is the out element-size
 * (original literally reuses the [EBP+0xc] slot; a separate local is used
 * here). Cluster block stride confirmed 0x24 (PUSH 0x24 before
 * tag_block_get_element 0x19b210).
 */
void object_names_postprocess(int scenario, char editor_flag)
{
  int type;
  void *def;
  int *block;
  int element_size; /* out from scenario_get_object_type_scenario_datums */
  int16_t element_index;
  int e;
  int16_t name_index; /* name: PAL 2342 object_types.c:1272 */
  int target;

  if (editor_flag != 0)
    return;

  type = 0;
  do {
    def = object_type_definition_get((int16_t)type);
    if (*(int16_t *)((char *)def + 0xa) != -1 &&
        *(int16_t *)((char *)def + 0xc) != -1) {
      block = (int *)scenario_get_object_type_scenario_datums(scenario, type,
                                                              &element_size);
      element_index = 0;
      if (*block > 0) {
        e = 0;
        do {
          e = (int)tag_block_get_element(block, e, element_size);
          name_index = *(int16_t *)(e + 2);
          if (name_index != -1) {
            target =
              (int)tag_block_get_element((void *)(scenario + 0x204), name_index, 0x24);
            *(int16_t *)(target + 0x20) = (int16_t)type;
            *(int16_t *)(target + 0x22) = element_index;
          }
          element_index++;
          e = (int)element_index;
        } while (e < *block);
      }
    }
    type++;
  } while ((int16_t)type < 0xc);
}

/*
 * object_types_reconnect_to_structure_bsp (0x13cf30 / object_types.c) —
 * re-place the BSP-switch object types after a structure-BSP switch unless
 * a running cinematic suppresses BSP object creation.
 *
 * Confirmed (disasm 0x13cf30-0x13cf4d): CALL cinematic_in_progress (0x930a0);
 * TEST AL,AL; if nonzero, MOV EAX,[0x44df00] and TEST byte [EAX+0xb]; the
 * call PUSH 1 / CALL object_types_place_objects (0x13cb80) / POP ECX runs when
 * either test is zero.
 * Confirmed: only reference is slot 12 (0-based, last) of the structure-BSP
 * reconnect table at 0x326a10 (walked by 0x18e260 and 0x18eb40).
 * Name: PAL 2342 objects/object_types.c object_types_reconnect_to_structure_bsp
 * and the matching slot of scenario_structure_bsp_reconnect_proc_table (T2).
 */
void object_types_reconnect_to_structure_bsp(void)
{
  if (!cinematic_in_progress() ||
      !cinematic_globals->suppress_bsp_object_creation) {
    object_types_place_objects(1);
  }
}

/* 0x13cf50 — object_placement_update: creates or destroys objects based on
 * scenario placement changes. Contains goto patterns for create/recreate.
 *
 * Source: c:\halo\SOURCE\objects\object_types.c
 * 5 cdecl params. Returns int (object handle).
 */
/* 0x13cf50 */
int object_type_synchronize(int param_1, short *param_2, int param_3,
                            short param_4, short param_5)
{
  struct fun_0013cf50_frame {
    char placement[0x88];
    int local_44;
    int local_40;
    float local_3c;
    float matrix[13];
  } frame;
  float fVar1;
  char cVar2;
  int *piVar3;
  int *puVar4;
  int uVar5;
  int iVar6;
  char *puVar8;
  char *pcVar10;
  int uVar11;
  int uVar12;
  float *position;
  int definition_index;

  if (*param_2 == -1) {
    if (param_1 != -1) {
      object_delete(param_1);
      param_1 = -1;
    }
    goto LAB_0013d51f;
  }
  if (param_1 == -1) {
    definition_index = *(int *)((char *)tag_block_get_element((void *)param_3, (int)*param_2, 0x30) + 0xc);
    if (definition_index != -1) {
      object_placement_data_new(frame.placement, definition_index, -1);
      *(int *)(frame.placement + 0x18) = *(int *)((char *)param_2 + 0x8);
      *(int *)(frame.placement + 0x1c) = *(int *)((char *)param_2 + 0xc);
      *(int *)(frame.placement + 0x20) = *(int *)((char *)param_2 + 0x10);
      vectors3d_from_euler_angles3d((float *)(frame.placement + 0x34),
                                    (float *)(frame.placement + 0x40),
                                    (float *)((char *)param_2 + 0x14));
      *(short *)(frame.placement + 0x16) = param_2[3];
      param_1 = object_new(frame.placement);
      if (param_1 != -1)
        object_type_place(param_1, (int)param_2);
    }
  } else {
    piVar3 = (int *)CALL_FUN_0013d640(param_1, -1);
    iVar6 = (int)tag_block_get_element((void *)param_3, (int)*param_2, 0x30);
    if (piVar3 == (int *)0 || *piVar3 != *(int *)(iVar6 + 0xc)) {
      if (piVar3 != (int *)0)
        object_delete(param_1);
      param_1 = -1;
      if (*(int *)(iVar6 + 0xc) != -1) {
        object_placement_data_new(frame.placement, *(int *)(iVar6 + 0xc), -1);
        *(int *)(frame.placement + 0x18) = *(int *)((char *)param_2 + 0x8);
        *(int *)(frame.placement + 0x1c) = *(int *)((char *)param_2 + 0xc);
        *(int *)(frame.placement + 0x20) = *(int *)((char *)param_2 + 0x10);
        vectors3d_from_euler_angles3d((float *)(frame.placement + 0x34),
                                      (float *)(frame.placement + 0x40),
                                      (float *)((char *)param_2 + 0x14));
        *(short *)(frame.placement + 0x16) = param_2[3];
        param_1 = object_new(frame.placement);
        if (param_1 != -1)
          object_type_place(param_1, (int)param_2);
      }
    }
  }
  if (param_1 == -1)
    goto LAB_0013d51f;
  puVar4 = (int *)object_get_and_verify_type(param_1, -1);
  object_activate(param_1);
  FUN_00109e90(frame.matrix, *(float *)((char *)param_2 + 0x14),
               *(float *)((char *)param_2 + 0x18),
               *(float *)((char *)param_2 + 0x1c));
  cVar2 = valid_real_matrix4x3(frame.matrix);
  if (cVar2 == '\0') {
    if ((*(unsigned int *)&frame.matrix[0] & 0x7f800000) == 0x7f800000) {
      uVar5 = (int)csprintf((char *)0x5ab100, "%s had a bad scale %f",
                            "&matrix", (double)frame.matrix[0]);
      display_assert((const char *)uVar5,
                     "c:\\halo\\SOURCE\\objects\\object_types.c", 0x3cf, 1);
      system_exit(-1);
    }
    cVar2 = valid_real_normal3d(&frame.matrix[1]);
    if (cVar2 == '\0') {
      uVar5 = (int)csprintf((char *)0x5ab100, "%s had a bad forward (%f,%f,%f)",
                            "&matrix", (double)frame.matrix[1],
                            (double)frame.matrix[2], (double)frame.matrix[3]);
      display_assert((const char *)uVar5,
                     "c:\\halo\\SOURCE\\objects\\object_types.c", 0x3cf, 1);
      system_exit(-1);
    }
    cVar2 = valid_real_normal3d(&frame.matrix[4]);
    if (cVar2 == '\0') {
      uVar5 = (int)csprintf((char *)0x5ab100, "%s had a bad left (%f,%f,%f)",
                            "&matrix", (double)frame.matrix[4],
                            (double)frame.matrix[5], (double)frame.matrix[6]);
      display_assert((const char *)uVar5,
                     "c:\\halo\\SOURCE\\objects\\object_types.c", 0x3cf, 1);
      system_exit(-1);
    }
    cVar2 = valid_real_normal3d(&frame.matrix[7]);
    if (cVar2 == '\0') {
      uVar5 = (int)csprintf((char *)0x5ab100, "%s had a bad up (%f,%f,%f)",
                            "&matrix", (double)frame.matrix[7],
                            (double)frame.matrix[8], (double)frame.matrix[9]);
      display_assert((const char *)uVar5,
                     "c:\\halo\\SOURCE\\objects\\object_types.c", 0x3cf, 1);
      system_exit(-1);
    }
    cVar2 = valid_real_point3d(&frame.matrix[10]);
    if (cVar2 == '\0') {
      uVar5 = (int)csprintf((char *)0x5ab100,
                            "%s had a bad position (%f,%f,%f)", "&matrix",
                            (double)frame.matrix[10], (double)frame.matrix[11],
                            (double)frame.matrix[12]);
      display_assert((const char *)uVar5,
                     "c:\\halo\\SOURCE\\objects\\object_types.c", 0x3cf, 1);
      system_exit(-1);
    }
    fVar1 = frame.matrix[6] * frame.matrix[3] +
            frame.matrix[5] * frame.matrix[2] +
            frame.matrix[4] * frame.matrix[1];
    if (((*(unsigned int *)&fVar1 & 0x7f800000) == 0x7f800000) ||
        !(fabs((double)fVar1) < DOUBLE_0_001_POOL)) {
      csprintf(
        (char *)0x5ab100,
        "%s had a forward (%f,%f,%f) not perpendicular to left (%f,%f,%f)",
        "&matrix", (double)frame.matrix[1], (double)frame.matrix[2],
        (double)frame.matrix[3], (double)frame.matrix[4],
        (double)frame.matrix[5], (double)frame.matrix[6]);
      display_assert((const char *)0x5ab100,
                     "c:\\halo\\SOURCE\\objects\\object_types.c", 0x3cf, 1);
      system_exit(-1);
    }
    fVar1 = frame.matrix[9] * frame.matrix[6] +
            frame.matrix[8] * frame.matrix[5] +
            frame.matrix[7] * frame.matrix[4];
    if (((*(unsigned int *)&fVar1 & 0x7f800000) == 0x7f800000) ||
        !(fabs((double)fVar1) < DOUBLE_0_001_POOL)) {
      csprintf((char *)0x5ab100,
               "%s had a up (%f,%f,%f) not perpendicular to left (%f,%f,%f)",
               "&matrix", (double)frame.matrix[7], (double)frame.matrix[8],
               (double)frame.matrix[9], (double)frame.matrix[4],
               (double)frame.matrix[5], (double)frame.matrix[6]);
      display_assert((const char *)0x5ab100,
                     "c:\\halo\\SOURCE\\objects\\object_types.c", 0x3cf, 1);
      system_exit(-1);
    }
    fVar1 = frame.matrix[9] * frame.matrix[3] +
            frame.matrix[8] * frame.matrix[2] +
            frame.matrix[7] * frame.matrix[1];
    if (((*(unsigned int *)&fVar1 & 0x7f800000) == 0x7f800000) ||
        !(fabs((double)fVar1) < DOUBLE_0_001_POOL)) {
      csprintf((char *)0x5ab100,
               "%s had a forward (%f,%f,%f) not perpendicular to up (%f,%f,%f)",
               "&matrix", (double)frame.matrix[1], (double)frame.matrix[2],
               (double)frame.matrix[3], (double)frame.matrix[7],
               (double)frame.matrix[8], (double)frame.matrix[9]);
      display_assert((const char *)0x5ab100,
                     "c:\\halo\\SOURCE\\objects\\object_types.c", 0x3cf, 1);
      system_exit(-1);
    }
    cVar2 = valid_real_matrix4x3(frame.matrix);
    if (cVar2 == '\0') {
      uVar5 = (int)csprintf((char *)0x5ab100,
                            "%s: assert_valid_real_matrix4x3", "&matrix");
      display_assert((const char *)uVar5,
                     "c:\\halo\\SOURCE\\objects\\object_types.c", 0x3cf, 1);
      system_exit(-1);
    }
  }
  iVar6 = (int)tag_get(0x6f626a65, *puVar4);
  position = (float *)((char *)param_2 + 0x8);
  if (*(int *)(iVar6 + 0x8c) != -1) {
    frame.local_44 = *(int *)(param_2 + 4);
    frame.local_40 = *(int *)(param_2 + 6);
    frame.local_3c =
      (float)puVar4[0x17] * REAL_0_5_POOL + *(float *)(param_2 + 8);
    position = (float *)&frame.local_44;
  }
  object_set_position(param_1, position, &frame.matrix[1], &frame.matrix[7]);
  *(short *)((int)puVar4 + 0x6a) = param_2[1];
LAB_0013d51f:
  if (param_2[1] != -1) {
    iVar6 = (int)tag_block_get_element(
      (void *)((int)global_scenario_get() + 0x204), (int)param_2[1], 0x24);
    *(short *)(iVar6 + 0x20) = param_4;
    *(short *)(iVar6 + 0x22) = param_5;
    /* Store this object's handle (param_1, held in ESI / the return value) at
     * the scenario name-table index param_2[1]. Confirmed at 0x13d551-0x13d557:
     * PUSH ESI (=param_1); PUSH EDX (=param_2[1]); CALL 0x13d880. */
    object_name_list_set_handle(param_2[1], param_1);
  }
  return param_1;
}

/* Wrap cluster_partition_iter_first for the non-collideable partition
 * (0x5a8d30). 0x13d570 / objects.obj
 * Confirmed: EAX from CALL 0x191a50 passes through to the caller (no write
 * to EAX after the call); actor_perception_refresh compares it with -1
 * (CMP EAX,-1 at 0x352d3), so the return is the first object handle.
 */
int cluster_get_first_noncollideable_object(int *param_1, int param_2)
{
  return cluster_partition_iter_first((void *)0x5a8d30, param_1,
                                      (int16_t)param_2);
}

/* Wrap cluster_partition_iter_next for the non-collideable partition
 * (0x5a8d30). 0x13d590 / objects.obj
 * Confirmed: EAX from CALL 0x191660 passes through (CMP EAX,-1 at 0x35301
 * in actor_perception_refresh), so the return is the next object handle.
 */
int cluster_get_next_noncollideable_object(int *param_1)
{
  return cluster_partition_iter_next((void *)0x5a8d30, param_1);
}

/*
 * cluster_partition_object_iter_first (0x13d5b0) — begin iteration over
 * objects in a BSP cluster using the collideable partition (0x5a8d40).
 *
 * Wraps cluster_partition_iter_first with the collideable object partition
 * constant. Returns the first object handle in the cluster, or -1 if none.
 *
 * Confirmed: PUSH EAX (param_2=cluster_idx), PUSH ECX (param_1=state),
 *            PUSH 0x5a8d40, CALL 0x191a50. EAX passed through.
 * Confirmed: ADD ESP,0xc (3 cdecl args cleaned by caller).
 */
int cluster_partition_object_iter_first(int *state, int16_t cluster_idx)
{
  return cluster_partition_iter_first((void *)0x5a8d40, state, cluster_idx);
}

/*
 * cluster_partition_object_iter_next (0x13d5d0) — advance iteration over
 * objects in a BSP cluster using the collideable partition (0x5a8d40).
 *
 * Wraps cluster_partition_iter_next with the collideable object partition
 * constant. Returns the next object handle, or -1 when exhausted.
 *
 * Confirmed: PUSH EAX (param_1=state), PUSH 0x5a8d40, CALL 0x191660.
 * Confirmed: ADD ESP,0x8 (2 cdecl args cleaned by caller).
 */
int cluster_partition_object_iter_next(int *state)
{
  return cluster_partition_iter_next((void *)0x5a8d40, state);
}

/*
 * object_get_next_cluster (0x13d5f0 / objects.obj) — advance an object's
 * per-object cluster iterator to the next cluster. The iterator state (param_1)
 * holds the cluster partition pointer at +0x00 (must be the collideable
 * 0x5a8d40 or noncollideable 0x5a8d30 partition) and the current cluster
 * handle at +0x04. Asserts the partition pointer is valid, then forwards to
 * FUN_001916d0(partition, &cluster_handle), which returns the next cluster
 * index and advances the handle. Returns the cluster index (or -1 at end).
 *
 * Confirmed: cdecl, param_2 ([EBP+0xc]) is UNUSED in the body.
 * Confirmed: assert string at 0x29b890, file at 0x29b91c, line 0x419.
 * Confirmed: void-EAX return (returns FUN_001916d0's result).
 */
int16_t object_get_next_cluster(void *param_1, int param_2)
{
  int *iter = (int *)param_1;
  (void)param_2;
  if ((void *)iter[0] != (void *)0x5a8d40 &&
      (void *)iter[0] != (void *)0x5a8d30) {
    display_assert(
      "iterator->cluster_partition==&collideable_object_cluster_partition || "
      "iterator->cluster_partition==&noncollideable_object_cluster_partition",
      "c:\\halo\\SOURCE\\objects\\objects.c", 0x419, 1);
    system_exit(-1);
  }
  return (int16_t)FUN_001916d0(iter[0], &iter[1]);
}

/*
 * object_try_and_get_and_verify_type — resolve a datum handle to its
 * object_data_t*, returning NULL if the handle is invalid or the object's
 * type is not among the bits in type_mask.
 *
 * Uses datum_absolute_index_to_index (0x119270, a "try-and-get" that returns
 * 0/NULL on failure) instead of datum_get (which asserts).
 * Reads the compact type byte at header+0x03, not the int16 at object+0x64.
 *
 * Confirmed: CALL 0x119270 with 2 args (ADD ESP,0x8).
 * Confirmed: byte ptr [EDX+0x3] — reads header->type as uint8_t.
 * Confirmed: MOV EAX, [EDX+0x8] — returns header->object.
 * Confirmed: XOR EAX,EAX before both exit paths — returns NULL on failure.
 */
void *object_try_and_get_and_verify_type(int datum_handle, int type_mask)
{
  object_header_data_t *header;
  void *result;

  header = (object_header_data_t *)(int)datum_absolute_index_to_index(
    *(data_t **)0x5a8d50, datum_handle);
  result = NULL;
  if (header != NULL && (type_mask & (1 << (header->type & 0x1f))) != 0) {
    result = header->object;
  }
  return result;
}

/*
 * object_get_and_verify_type — resolve a datum handle to its object_data_t*
 * and assert that the object's type is one of the bits in type_mask.
 *
 * The "object" data table pointer lives at 0x5a8d50 (allocated by
 * objects_initialize as the "object" header data array; distinct from
 * the "objects" memory pool at 0x46f080 and object_header_data at 0x5a8d50).
 *
 * datum_get(data, handle) returns object_header_data_t*; field at +8 is the
 * object_data_t* . Type enum is a signed int16 at object_data_t+0x64.
 *
 * Confirmed: MOVSX ECX, word ptr [ESI+0x64] — signed 16-bit read.
 * Confirmed: ADD ESP,0x8 after datum_get (2 cdecl args).
 * Confirmed: ADD ESP,0x10 after csprintf (4 args cleaned; 3 pre-pushed remain
 *            on stack for display_assert); ADD ESP,0x14 cleans the rest.
 */
void *object_get_and_verify_type(int datum_handle, int type_mask)
{
  /* datum_get: first arg = data table ptr (value at 0x5a8d50) */
  object_header_data_t *header =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, datum_handle);
  object_data_t *obj = header->object;
  int16_t type = obj->type;

  if ((type_mask & (1 << (type & 0x1f))) == 0) {
    /* csprintf with varargs: buffer, format, type_mask, (int)type.
     * The remaining 3 args (filename, lineno, halt) are pre-pushed before
     * csprintf in the original; in C we pass them explicitly to display_assert.
     * Confirmed: ADD ESP,0x10 cleans 4 csprintf args; display_assert receives
     * (reason, filepath, lineno, halt). */
    char *msg = csprintf((char *)0x5ab100,
                         "got an object type we didn't expect (expected one of "
                         "0x%08x but got #%d).",
                         type_mask, (int)type);
    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
    system_exit(-1);
  }
  return obj;
}

/*
 * object_iterator_new (0x13d6f0) — initialise an object_iter_t for a walk.
 *
 * Calls data_verify on the object data table (sanity check), then writes
 * the caller-supplied type_mask and flags into the iterator block and
 * resets its scan state.
 *
 * Layout of object_iter_t (confirmed from disassembly):
 *   +0x00 int32_t  type_mask     — accepted object types (1<<type bit-mask)
 *   +0x04 uint8_t  flags         — required header flag byte (AND+CMP filter)
 *   +0x06 int16_t  current_index — next header-table slot to probe
 *   +0x08 int32_t  last_handle   — handle from last call (NONE = -1 on init)
 *   +0x0c uint32_t cookie        — 0x86868686 (marks as initialized)
 *
 * Confirmed: ADD ESP,0x4 after data_verify (1 arg).
 * Confirmed: byte ptr [EAX+0x4] = DL (flags, byte-sized arg).
 * Confirmed: word ptr [EAX+0x6] = 0x0000; dword ptr [EAX+0x8] = -1.
 * Confirmed: dword ptr [EAX+0xc] = 0x86868686 (cookie, written last).
 */
void object_iterator_new(void *iter, int type_mask, int flags)
{
  object_iter_t *it = (object_iter_t *)iter;
  data_verify(*(data_t **)0x5a8d50);
  it->cookie = 0x86868686;
  it->type_mask = type_mask;
  it->flags = (uint8_t)flags;
  it->current_index = 0;
  it->last_handle = NONE;
}

/*
 * object_iterator_next (0x13d730) — advance iterator, return next match.
 *
 * Walks the object header table starting at iter->current_index, scanning
 * for a non-empty slot (salt != 0) whose header flags satisfy the required
 * flag mask (entry_flags & iter->flags == iter->flags) and whose type bit
 * is set in iter->type_mask.  On a match:
 *   - Stores the composite handle (salt<<16 | index) in iter->last_handle.
 *   - Advances iter->current_index past the matched slot.
 *   - Returns the object_data_t* from entry->object (header+0x8).
 *
 * Returns NULL when the table is exhausted.
 *
 * The header table is an array of 0xc-byte object_header_data_t entries;
 * pointers start at data_t->data (offset +0x34 from the data_t header).
 * The live slot count is at data_t->current_count (offset +0x2e, int16_t).
 *
 * Confirmed: cookie guard == 0x86868686 (assert "uninitialized iterator").
 * Confirmed: MOVSX EAX, word ptr [EAX+0x2e] — current_count as signed 16-bit.
 * Confirmed: MOVSX from DX (current_index) into ECX for OR with shifted salt.
 * Confirmed: entry stride = 0xc (LEA ESI,[ESI+ECX*4] with ECX=index*3).
 * Confirmed: return entry->object at entry+0x8.
 */
void *object_iterator_next(void *iter)
{
  object_iter_t *it = (object_iter_t *)iter;
  data_t *data;
  object_header_data_t *entry;
  int16_t idx;
  int handle;

  handle = 0;

  if (it->cookie != 0x86868686) {
    display_assert("uninitialized iterator passed to object_iterator_next()",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x6b8, 1);
    system_exit(-1);
  }

  data_verify(*(data_t **)0x5a8d50);
  data = *(data_t **)0x5a8d50;

  idx = it->current_index;
  entry = (object_header_data_t *)((char *)data->data + (int)idx * 0xc);

  if (idx < data->current_count) {
    do {
      handle = ((int)(int16_t)entry->unk_0 << 16) | (int)idx;
      idx++;
      if (entry->unk_0 != 0 &&
          ((entry->unk_2 & (uint8_t)it->flags) == (uint8_t)it->flags) &&
          (it->type_mask & (1 << entry->type)) != 0) {
        it->last_handle = handle;
        it->current_index = idx;
        return entry->object;
      }
      entry = (object_header_data_t *)((char *)entry + 0xc);
    } while (idx < data->current_count);

    /*
     * Loop-exhausted epilogue (0x13d7cb): MOV word ptr [EBX+6],DX; XOR EAX,EAX
     * — the original returns NULL here, NOT the last composite handle still
     * live in EDI. The separate never-entered-loop epilogue at 0x13d7e5 does
     * MOV EAX,EDI, but EDI is still the XOR EDI,EDI from function entry, so
     * that path also returns 0.
     */
    it->current_index = idx;
    return (void *)0;
  }

  it->current_index = idx;
  return (void *)handle;
}

/*
 * object_set_garbage_flag — add or remove an object from the garbage
 * collection linked list.
 *
 * The garbage list is a singly-linked list threaded through
 * object_data_t+0xC0 (unk_192), with the head stored at
 * object_globals+0x08 (unk_8).
 *
 * When is_garbage is nonzero (add to garbage list):
 *   - Bails out if bit 0x10000 (garbage) or 0x20000 is already set.
 *   - Prepends the object to the garbage list head.
 *   - Sets bit 0x10000 in object flags.
 *
 * When is_garbage is zero (remove from garbage list):
 *   - Bails out if bit 0x10000 is NOT set.
 *   - Walks the list to find and unlink the object.
 *   - Clears bit 0x10000 in object flags.
 *   - Sets unk_192 to NONE (-1).
 *
 * Two debug validation loops walk the entire garbage list before and
 * after the mutation, asserting that every entry has a valid type and
 * the garbage bit set. These correspond to lines 0x7a0 and 0x7d6 in
 * the original objects.c.
 *
 * Confirmed: 2 cdecl args — PUSH [EBP+8], PUSH -1 before CALL 0x13d680.
 * Confirmed: MOV AL, byte ptr [EBP+0xC] — second arg is char-sized.
 * Confirmed: TEST EAX,0x30000 guards the add path; TEST EAX,0x10000
 *            guards the remove path.
 * Confirmed: garbage list next at object+0xC0, head at og+0x08.
 * Confirmed: assert strings at 0x29b9c4 and line numbers 0x7a0, 0x7d6.
 * Confirmed: object_get_and_verify_type(handle, -1) to resolve.
 */

/*
 * object_get_root_parent — walk the parent chain to the root object.
 *
 * Starting from object_handle, loops through parent_object_index (obj+0xCC)
 * until it reaches -1.  Each iteration validates the object type against the
 * full-type mask (0xFFFFFFFF).  Returns the topmost non-null handle, or -1
 * if the input was already -1.
 *
 * Confirmed: datum_get(DAT_005a8d50, handle) -> header at +0x08 -> type at
 *            +0x64 (int16_t).  Bit-shift check (1 << (type & 0x1f)) against
 *            0 — in practice always passes since mask is -1.
 * Confirmed: Loop terminates when obj->parent_object_index == -1.
 */
int object_get_root_parent(int object_handle)
{
  int current;
  int result;
  object_header_data_t *header;
  object_data_t *obj;
  int16_t type;

  result = -1;
  current = object_handle;
  while (current != -1) {
    header = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, current);
    obj = header->object;
    type = obj->type;
    if ((1 << (type & 0x1f)) == 0) {
      display_assert(csprintf((char *)0x5ab100,
                              "got an object type we didn't expect (expected "
                              "one of 0x%08x but got #%d).",
                              -1, (int)type),
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
      system_exit(-1);
    }
    result = current;
    current = obj->parent_object_index.value;
  }
  return result;
}

void object_add_scenario_permutation(int unit_handle, void *data)
{
  (void)unit_handle;
  (void)data;
}

/*
 * object_name_list_set_handle — store an object handle at a name-table index.
 *
 * Validates that param_1 is non-negative (TEST AX,AX / JL) and less than the
 * scenario's object-name count (at scenario+0x204), then writes param_2 into
 * the object_name_list array (pointer at 0x46f07c) at the given index.
 *
 * Confirmed: MOVSX ESI,AX — sign-extends param_1 before use.
 * Confirmed: MOV ECX,[0x46f07c] — dereferences pointer, not direct array.
 * Confirmed: MOV [ECX + ESI*4],EAX — stores param_2 at name_table[param_1].
 * Confirmed: cdecl, caller at 0x45ffb does ADD ESP,0x8 after call.
 */
void object_name_list_set_handle(short name_index, int object_handle)
{
  int idx;
  void *scenario;

  if (name_index < 0)
    return;

  idx = (int)name_index;
  scenario = global_scenario_get();
  if (idx < *(int *)((char *)scenario + 0x204)) {
    (*(int **)0x46f07c)[idx] = object_handle;
  }
}

/*
 * objects_fix_for_deleted_object (0x13d8b0 / objects.obj) — detach an object
 * handle from every other object that references it.
 *
 * Walks all objects via an inlined object iterator (type_mask = all, flags = 0)
 * and, for each object whose "referenced object" field (object+0xa0) equals the
 * target handle, resets that field to NONE (-1).
 * object_type_handle_deleted_object is then called for every iterated object
 * with (iterator.last_handle, target_handle) to run any per-object detach side
 * effects.
 *
 * Confirmed (disasm 0x13d8b0): data_verify(*(data_t**)0x5a8d50) first; iterator
 * struct inlined at EBP-0x10 (type_mask=-1, flags=0, current_index=0,
 * last_handle=-1, cookie=0x86868686 — matching object_iter_t); object pointer
 * returned by object_iterator_next (0x13d730) in EAX; CMP [EAX+0xa0],ESI then
 * conditional MOV [EAX+0xa0],-1;
 * object_type_handle_deleted_object([EBP-0x8]=last_handle, ESI=handle).
 */
void objects_fix_for_deleted_object(int object_handle)
{
  object_iter_t it;
  object_data_t *obj;

  data_verify(*(data_t **)0x5a8d50);

  /* MSVC writes cookie (EBP-4) first; clang otherwise stores it last. */
  it.cookie = 0x86868686;
  it.type_mask = -1;
  it.flags = 0;
  it.current_index = 0;
  it.last_handle = -1;

  obj = (object_data_t *)object_iterator_next(&it);
  while (obj != (object_data_t *)0) {
    if (*(int *)((char *)obj + 0xa0) == object_handle)
      *(int *)((char *)obj + 0xa0) = -1;
    object_type_handle_deleted_object(it.last_handle, object_handle);
    obj = (object_data_t *)object_iterator_next(&it);
  }
}

void object_set_garbage_flag(int object_handle, int is_garbage)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  object_globals_t *og = object_globals;

  /* Pre-validation: walk the garbage list and assert integrity */
  {
    int handle = og->unk_8.value;
    while (handle != -1) {
      object_header_data_t *hdr =
        (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle);
      object_data_t *gobj = hdr->object;
      int16_t type = gobj->type;
      if ((1 << (type & 0x1f)) == 0) {
        char *msg = csprintf(
          (char *)0x5ab100,
          "got an object type we didn't expect (expected one of 0x%08x but "
          "got #%d).",
          -1, (int)type);
        display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
        system_exit(-1);
      }
      if ((gobj->flags & 0x10000) == 0) {
        display_assert(
          "TEST_FLAG(garbage_object->object.flags, _object_garbage_bit)",
          "c:\\halo\\SOURCE\\objects\\objects.c", 0x7a0, 1);
        system_exit(-1);
      }
      handle = gobj->unk_192;
    }
    og = object_globals;
  }

  if ((char)is_garbage != 0) {
    /* Add to garbage list */
    if ((obj->flags & 0x30000) != 0)
      goto done;

    obj->unk_192 = og->unk_8.value;
    og->unk_8.value = object_handle;
    obj->flags |= 0x10000;
  } else {
    /* Remove from garbage list */
    uint32_t *prev_ptr;
    int cur;

    if ((obj->flags & 0x10000) == 0)
      goto done;

    /* Walk the list to find the previous pointer */
    prev_ptr = (uint32_t *)&og->unk_8.value;
    cur = og->unk_8.value;

    while (cur != object_handle) {
      object_header_data_t *hdr =
        (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, *prev_ptr);
      object_data_t *gobj = hdr->object;
      int16_t type = gobj->type;
      if ((1 << (type & 0x1f)) == 0) {
        char *msg = csprintf(
          (char *)0x5ab100,
          "got an object type we didn't expect (expected one of 0x%08x but "
          "got #%d).",
          -1, (int)type);
        display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
        system_exit(-1);
      }
      prev_ptr = &gobj->unk_192;
      cur = gobj->unk_192;
    }

    /* Unlink: *prev_ptr = obj->next; obj->next = NONE */
    *prev_ptr = obj->unk_192;
    obj->unk_192 = 0xffffffff;
    obj->flags &= ~(uint32_t)0x10000;
  }

done:
  /* Post-validation: walk the garbage list again */
  {
    int handle = og->unk_8.value;
    while (handle != -1) {
      object_header_data_t *hdr =
        (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle);
      object_data_t *gobj = hdr->object;
      int16_t type = gobj->type;
      if ((1 << (type & 0x1f)) == 0) {
        char *msg = csprintf(
          (char *)0x5ab100,
          "got an object type we didn't expect (expected one of 0x%08x but "
          "got #%d).",
          -1, (int)type);
        display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
        system_exit(-1);
      }
      if ((gobj->flags & 0x10000) == 0) {
        display_assert(
          "TEST_FLAG(garbage_object->object.flags, _object_garbage_bit)",
          "c:\\halo\\SOURCE\\objects\\objects.c", 0x7d6, 1);
        system_exit(-1);
      }
      handle = gobj->unk_192;
    }
  }
}

void garbage_collect_now(void)
{
  *(unsigned char *)(*(int *)0x46f084 + 2) = 1;
}

struct objects_information {
  int16_t object_count;
  int16_t active_object_count;
  float used_memory;
};

/* 0x13db60 / objects.obj — Writes object counts and memory-usage fraction into
 * info. info->object_count = total non-empty object slots
 * info->active_object_count = object slots with active flag (elem+2 bit 0)
 * info->used_memory = 1.0 - fraction of pool in use */
void objects_information_get(void *info_ptr)
{
  struct objects_information *info = (struct objects_information *)info_ptr;
  short *psVar1;
  int iVar2;
  short sVar3;
  data_t *objs_data;

  csmemset(info, 0, sizeof(struct objects_information));
  objs_data = *(data_t **)0x5a8d50;
  psVar1 = (short *)*(uint8_t **)((uint8_t *)objs_data + 0x34);
  sVar3 = 0;
  if (0 < *(int16_t *)((uint8_t *)objs_data + 0x2e)) {
    do {
      if (*psVar1 != 0) {
        info->object_count++;
        if (*(unsigned char *)((char *)psVar1 + 2) & 1) {
          info->active_object_count++;
        }
      }
      objs_data = *(data_t **)0x5a8d50;
      sVar3 = sVar3 + 1;
      psVar1 = psVar1 + 6;
    } while (sVar3 < *(int16_t *)((uint8_t *)objs_data + 0x2e));
  }
  iVar2 = memory_pool_get_contiguous_free_size(*(void **)0x46f080);
  info->used_memory = REAL_ONE_POOL - (float)iVar2 * *(float *)0x29ba04;
}

void object_pvs_set_object(int param_1)
{
  int iVar1;

  iVar1 = *(int *)0x46f084;
  if (param_1 == -1) {
    *(short *)(*(int *)0x46f084 + 0x90) = 0;
    return;
  }
  *(short *)(iVar1 + 0x90) = 1;
  *(int *)(iVar1 + 0x94) = param_1;
}

/*
 * object_pvs_set_camera_point (0x13dc10 / objects.obj) —
 * object_pvs_set_camera_point: set the object-PVS source to a scenario camera
 * point.
 *
 * If the camera point index is NONE (-1), clears the PVS mode (object_globals
 * +0x90 = 0).  Otherwise resolves the camera point element from the scenario
 * camera-point block (scenario+0x4f0, element size 0x68), converts its position
 * (element+0x28) to a scenario location, and reads the resulting leaf/cluster
 * index (short) from the location struct (+4).  A leaf index of -1 means the
 * camera point is outside the map: emit error() and clear the PVS mode.
 * Otherwise set mode = 2 and store the leaf index in object_globals +0x94.
 *
 * §7 note: Ghidra groups (idx, 0x68) onto global_scenario_get(); they actually
 * belong to the following tag_block_get_element(scenario+0x4f0, idx, 0x68).
 *
 * Confirmed (disasm 0x13dc10): CMP AX,-1 early-out writes word [og+0x90]=0;
 * MOVSX ECX,AX then PUSH 0x68/PUSH ECX/CALL global_scenario_get(0x18e380);
 * ADD EAX,0x4f0 then CALL tag_block_get_element(0x19b210); ESI=element;
 * scenario_location_from_point(&loc, element+0x28); CMP word [EBP-4],-1;
 * error(2, "...%s...", element+4); else word[og+0x90]=2, word[og+0x94]=leaf.
 */
void object_pvs_set_camera_point(short camera_point_index)
{
  char *globals; /* object_globals (*0x46f084) */
  int cam;
  char location[8]; /* scenario_location_from_point output; +4 = leaf index
                       (short) */

  if (camera_point_index == -1) {
    *(short *)(*(int *)0x46f084 + 0x90) = 0;
    return;
  }
  cam = (int)tag_block_get_element((char *)global_scenario_get() + 0x4f0,
                                   (int)camera_point_index, 0x68);
  scenario_location_from_point(location, (void *)(cam + 0x28));
  if (*(short *)(location + 4) == -1) {
    error(2, "object_pvs_set_camera_point: camera point %s is outside the map",
          (char *)(cam + 4));
    *(short *)(*(int *)0x46f084 + 0x90) = 0;
    return;
  }
  globals = *(char **)0x46f084;
  *(short *)(globals + 0x90) = 2;
  *(short *)(globals + 0x94) = *(short *)(location + 4);
}

void object_pvs_clear(void)
{
  *(short *)(*(int *)0x46f084 + 0x90) = 0;
}

/*
 * objects_get_activating_cluster_index (0x13dcc0 / objects.obj) —
 * object_pvs_get_cluster_index: resolve the PVS/observer camera point to a
 * structure-BSP cluster index.
 *
 * object_globals (*0x46f084) holds a small state machine at +0x90:
 *   state 1 -> the +0x94 field is an object handle; resolve its root object,
 *              verify it is "connected to map" (object flags +0x4 bit 0x800),
 *              and return its location cluster_index (object+0x4c). If the
 *              object is stale/freed, reset state to 0 and return -1.
 *   state 2 -> the +0x94 field is already a cluster index; return it directly.
 *   else    -> return -1.
 *
 * Confirmed: state at *(short*)(*0x46f084 + 0x90); DEC/DEC dispatch (1 then 2).
 * Confirmed: CALL 0x119270 (datum_absolute_index_to_index) with
 *            (*0x5a8d50, *(int*)(*0x46f084 + 0x94)) for the staleness check.
 * Confirmed: staleness reject if entry==0 || (1<<(entry[3]&0x1f))==0 ||
 *            *(int*)(entry+8)==0 -> reset +0x90=0, return 0xffff.
 * Confirmed: CALL 0x13d7f0 (object_get_root_parent), CALL 0x13d680
 *            (object_get_and_verify_type, mask -1).
 * Confirmed: TEST [obj+0x4] bit 0x800 (connected-to-map) -> else return -1.
 * Confirmed: cluster_index at object+0x4c; -1 -> return -1.
 * Confirmed: assert cluster_index in [0, scenario_get()->[+0x134]) at 0x8e7,
 *            followed by system_exit(-1).
 */
short objects_get_activating_cluster_index(void)
{
  int index;
  int entry;
  char *obj;
  void *scenario;
  short result;

  /* Single-exit form: result lives in EDI to the shared epilogue
   * (ref: or edi,-1 ... mov ax,di). */
  result = -1;
  /* object_globals is re-read at each use (PAL 2342 objects.c:530). */

  switch (*(short *)(*(int *)0x46f084 + 0x90)) {
  case 2:
    result = *(short *)(*(int *)0x46f084 + 0x94);
    break;
  case 1:
    index = *(int *)(*(int *)0x46f084 + 0x94);
    entry = (int)datum_absolute_index_to_index(*(data_t **)0x5a8d50, index);
    if (entry != 0 && (1 << *(unsigned char *)(entry + 3)) != 0 &&
        *(int *)(entry + 8) != 0) {
      obj = (char *)object_get_and_verify_type(
        object_get_root_parent(*(int *)(*(int *)0x46f084 + 0x94)), -1);

      if ((*(unsigned int *)(obj + 4) & 0x800) != 0) {
        if (*(short *)(obj + 0x4c) != -1) {
          /* Bounds-check the cluster index: must be >= 0 and <
           * clusters.count; scenario_get() is only evaluated for the
           * upper-bound comparison. */
          if (*(short *)(obj + 0x4c) < 0 ||
              (scenario = scenario_get(),
               (int)*(short *)(obj + 0x4c) >=
                 *(int *)((char *)scenario + 0x134))) {
            display_assert(
              "parent_object->object.location.cluster_index>=0 && "
              "parent_object->object.location.cluster_index<global_"
              "structure_bsp_get()->clusters.count",
              "c:\\halo\\SOURCE\\objects\\objects.c", 0x8e7, 1);
            system_exit(-1);
          }
          result = *(short *)(obj + 0x4c);
        }
      }
    } else {
      *(short *)(*(int *)0x46f084 + 0x90) = 0;
    }
    break;
  default:
    break;
  }

  return result;
}

void object_definition_predict(int param_1)
{
  void *tag;

  if (param_1 != -1) {
    tag = tag_get(0x6f626a65, param_1);
    predicted_resources_precache((char *)tag + 0x170);
  }
}

/*
 * object_predict (0x13ddd0 / objects.obj) — recursively precache the predicted
 * resources for an object and its attachment tree.
 *
 * Iterative+recursive walk over the object datum (table at 0x5a8d50). For each
 * object: resolves its definition pointer (obj+0x8), reads the 16-bit object
 * type at def+0x64, and asserts that (1 << type) is non-zero (i.e. the type is
 * in range; the original message reports the unexpected type). If the object's
 * tag index (def+0x0) is valid, fetches the 'obje' tag (0x6f626a65) and
 * precaches its predicted-resources block at tag+0x170. Then recurses into the
 * first child (def+0xc8) and tail-iterates to the next sibling (def+0xc4) via
 * the enclosing while loop.
 *
 * Confirmed (disasm 0x13ddd0): type is a signed 16-bit load (MOVSX ECX, word
 * ptr [ESI+0x64]); shift mask test is TEST EDX,EDX after SHL EDX,CL; assert
 * uses csprintf(0x5ab100, fmt, -1, type) then display_assert(reason,
 * "...objects.c", 0x69a, 1) then system_exit(-1); child at +0xc8, sibling at
 * +0xc4; precache arg is tag+0x170.
 */
void object_predict(int object_handle)
{
  object_data_t *obj;
  int *defn;
  int type;
  int tag;

  while (object_handle != -1) {
    obj = (object_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
    defn = *(int **)((char *)obj + 8);
    type = *(int16_t *)((char *)defn + 0x64);
    if ((1 << (type & 0x1f)) == 0) {
      display_assert(csprintf((char *)0x5ab100,
                              "got an object type we didn't expect "
                              "(expected one of 0x%08x but got #%d).",
                              0xffffffff, type),
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
      system_exit(-1);
    }
    if (defn[0] != -1) {
      tag = (int)tag_get(0x6f626a65, defn[0]);
      predicted_resources_precache((void *)(tag + 0x170));
    }
    object_predict(defn[0x32]);
    object_handle = defn[0x31];
  }
}

void object_beautify(int object_handle, char beautiful)
{
  if (object_handle != -1) {
    if (beautiful != '\0') {
      ((object_data_t *)object_get_and_verify_type(object_handle, 0xffffffff))->flags |=
        0x400000;
      return;
    }
    ((object_data_t *)object_get_and_verify_type(object_handle, 0xffffffff))->flags &=
      ~0x400000;
  }
}

/*
 * object_header_new — allocate a new datum in an object data table and reserve
 * pool memory for it from the global objects memory pool at 0x46f080.
 *
 * If type_hint == -1, allocates at the next free index (data_new_at_index).
 * Otherwise allocates at the specified handle (data_new_datum).
 * On success, allocates datum_size bytes from the pool into datum+8,
 * records the size at datum+6, and zeros the allocated block.
 * Returns the datum handle, or -1 on failure.
 *
 * Confirmed: CMP EAX,-1 branches to data_new_at_index vs data_new_datum.
 * Confirmed: CALL 0x11e6c0 (memory_pool_block_new) with pool from [0x46f080].
 * Confirmed: MOV [EDI+6],CX stores datum_size as int16_t.
 * Confirmed: CALL 0x8db80 (csmemset) zeros *(void**)(datum+8).
 * Confirmed: datum_delete on pool allocation failure, returns -1.
 */
int object_header_new(data_t *data, int16_t datum_size, int type_hint)
{
  int handle;
  char *datum;
  void **volatile block; /* datum+8, forced to [EBP-4] like the original */
  int ds; /* (int)datum_size, sign-extended once into EBX */

  if (type_hint == -1)
    handle = data_new_at_index(data);
  else
    handle = data_new_datum(data, type_hint);

  if (handle != -1) {
    datum = (char *)datum_get(data, handle);
    ds = (int)datum_size;
    block = (void **)(datum + 8);
    if (memory_pool_block_new(*(void **)0x46f080, block, ds)) {
      *(int16_t *)(datum + 6) = datum_size;
      csmemset(*block, 0, ds);
    } else {
      datum_delete(data, handle);
      return -1;
    }
  }

  return handle;
}

void object_header_delete(data_t *data, int object_handle /* @<ebx> */)
{
  char *header;
  void **object_ptr;

  header = (char *)datum_get(data, object_handle);
  object_ptr = (void **)(header + 8);
  if (*object_ptr != NULL)
    memory_pool_block_free(*(void **)0x46f080, object_ptr);
  datum_delete(data, object_handle);
  *(uint8_t *)(header + 2) = 0;
  *object_ptr = NULL;
}

/*
 * object_header_block_reference_get — resolve an object's inline
 *
 * block-reference pair ({size, offset}) to a pointer into object data.
 *
 *
 * Confirmed: CALL 0x119320 (datum_get) first, then CALL 0x13d680
 *
 * (object_get_and_verify_type).
 * Confirmed: reference fields are signed
 * 16-bit reads at +0 (size)
 * and +2 (offset).
 * Confirmed: asserts
 * "reference->offset>0" at line 0x98b and
 *
 * "reference->offset+reference->size<=object_header->data_size"
 * at line
 * 0x98c, both followed by system_exit(-1).
 * Confirmed: return value is
 * object_ptr + reference->offset.
 */
void *object_header_block_reference_get(int object_handle, void *reference)
{
  object_header_data_t *header =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  char *object = (char *)object_get_and_verify_type(object_handle, -1);
  short *ref = (short *)reference;

  /* reference layout: [+0] = size, [+2] = offset (both signed 16-bit); re-read
   * inline (not cached), and the sum is the LEFT cmp operand, to match codegen.
   */
  if (ref[1] <= 0) {
    display_assert("reference->offset>0",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x98b, 1);
    system_exit(-1);
  }

  if ((int)ref[0] + (int)ref[1] > (int)(short)header->data_size) {
    display_assert(
      "reference->offset+reference->size<=object_header->data_size",
      "c:\\halo\\SOURCE\\objects\\objects.c", 0x98c, 1);
    system_exit(-1);
  }

  return object + ref[1];
}

/*
 * object_header_block_allocate — grow an object's variable-length header data
 * region by `size` bytes and stamp a block_reference record at `offset`.
 *
 * Validates size>=0, data_size+size<=SHORT_MAX, offset>=0, and
 * offset+sizeof(block_reference)<=data_size, then resizes the object's pooled
 * data block (memory_pool_block_resize) to data_size+size. On success it bumps
 * data_size, writes the 4-byte block_reference {size, old_data_size} at
 * obj_base+offset, zero-fills the newly appended region, and returns 1.
 *
 * Confirmed: 3 cdecl args. params read as short via MOVSX (handle is int).
 * Confirmed: data_size at header+0x06 (uint16_t), object at header+0x08.
 * Confirmed: memory_pool_block_resize(*0x46f080, &header->object, new_size).
 * Confirmed: block_reference at obj_base+offset: [+0]=size, [+2]=old_data_size.
 * Confirmed: csmemset(header->object + old_data_size, 0, size).
 * Confirmed: asserts at objects.c lines 0x99b, 0x99c, 0x99e, 0x99f.
 */
int object_header_block_allocate(int object_index, int block_reference_offset, int size)
{
  object_header_data_t *object_header;
  short size16;
  short offset16;
  short original_size;
  char *obj_base;
  short *block;

  object_header =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_index);
  size16 = (short)size;
  if (size16 < 0) {
    display_assert("size>=0", "c:\\halo\\SOURCE\\objects\\objects.c", 0x99b, 1);
    system_exit(-1);
  }
  if (0x7fff < (int)(short)object_header->data_size + (int)size16) {
    display_assert("object_header->data_size+size<=SHORT_MAX",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x99c, 1);
    system_exit(-1);
  }
  offset16 = (short)block_reference_offset;
  if (offset16 < 0) {
    display_assert("block_reference_offset>=0",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x99e, 1);
    system_exit(-1);
  }
  if (!((unsigned int)((int)offset16 + 4) <=
         (unsigned int)(int)(short)object_header->data_size)) {
    display_assert("block_reference_offset+sizeof(struct "
                   "object_header_block_reference)<=object_header->data_size",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x99f, 1);
    system_exit(-1);
  }

  if (memory_pool_block_resize(*(void **)0x46f080, (void **)&object_header->object,
                               (int)(short)object_header->data_size + (int)size16)) {
    original_size = (short)object_header->data_size;
    object_header->data_size = (uint16_t)(original_size + size16);
    obj_base = (char *)object_get_and_verify_type(object_index, -1);
    block = (short *)(obj_base + offset16);
    block[1] = original_size;
    block[0] = size16;
    csmemset((char *)object_header->object + (int)original_size, 0, (int)size16);
    return 1;
  }
  return 0;
}

/*
 * object_postprocess_node_matrices — run animation-block initializer callbacks
 * for an object.
 *
 * Resolves the object's tag definition and checks whether both a model
 * (tag+0x34) and an animation graph (tag+0x44) are present. If so,
 * resolves the object's animation block reference at object_data+0x1a0
 * via object_header_block_reference_get, then dispatches through type
 * callbacks via object_type_postprocess_node_matrices.
 *
 * Confirmed: single register arg object_handle in EDI.
 * Confirmed: PUSH -1, PUSH EDI -> object_get_and_verify_type(handle, -1).
 * Confirmed: PUSH EAX, PUSH 0x6f626a65 -> tag_get('obje', obj[0]).
 * Confirmed: ADD ESP,0x10 cleans both calls (4 pushes).
 * Confirmed: CMP [EAX+0x34],-1 checks model tag index.
 * Confirmed: CMP [EAX+0x44],-1 checks animation graph tag index.
 * Confirmed: ADD ESI,0x1a0 -> object_data+0x1a0 is the animation block ref.
 * Confirmed: PUSH ESI, PUSH EDI -> object_header_block_reference_get(handle,
 * obj+0x1a0). Confirmed: PUSH EAX (return value), PUSH EDI ->
 * object_type_postprocess_node_matrices(handle, block). Confirmed: ADD ESP,0x10
 * cleans both calls (4 pushes).
 */
/* 0x13e1a0 */
void object_postprocess_node_matrices(int object_handle /* @<edi> */)
{
  char *obj;
  char *tag_data;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  tag_data = (char *)tag_get(0x6f626a65, *(int *)obj);

  if (*(int *)(tag_data + 0x34) != -1 && *(int *)(tag_data + 0x44) != -1) {
    void *block = object_header_block_reference_get(object_handle, obj + 0x1a0);
    object_type_postprocess_node_matrices(object_handle, block);
  }
}

/* 0x13e1f0 / objects.obj — Seed the object's four change-color slots during
 * spawn. For each of the 4 slots, copies the base RGB triple from the
 * placement color_data, then (if the model tag declares a change-color
 * animation for this slot index, block at obj_tag+0x164, element size 0x2c)
 * derives a deterministic pseudo-random blend value from the object's basis
 * vectors (obj+0xc/0x10/0x14) and the slot index, walks the slot's permutation
 * sub-block (element+0x20, element size 0x1c) as a cumulative distribution,
 * and on the first permutation whose threshold (perm[0]) is >= the blend
 * value, blends an RGB pair (perm+0x4 .. perm+0x10) into the slot color via
 * FUN_0007c270. Finally clamps each RGB component of the slot to [0,1] and
 * writes the clamped triple to the +0x30 mirror (obj+0x138 slot layout:
 * base RGB at +0, clamped RGB at +0x30).
 * Role: object spawn-appearance setup; establishes per-object tinting.
 * object_handle in EAX (register arg); color_data is first stack argument.
 * Confirmed: PUSH -1; PUSH EAX; CALL object_get_and_verify_type.
 * Confirmed: pseudo-random frac via x87_fmod(|dot|, 1.0) (CALL 0x1daf7e).
 * Confirmed: clamp lo/hi = REAL_ZERO_POOL (0.0) / 0x2533c8 (1.0).
 * Confirmed: 4 iterations; dest stride 0xc bytes, color_data stride 0xc. */
void object_choose_random_change_colors(int object_handle /* @<eax> */,
                                        void *color_data)
{
  char *obj;
  int obj_tag;
  float *src;
  float *dst;
  int i;
  int count;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  obj_tag = (int)tag_get(0x6f626a65, *(int *)obj);
  src = (float *)color_data;
  dst = (float *)(obj + 0x138);

  /* fixed 4-slot loop: separate index i and a 4->0 down-counter, matching
   * the reference's movl $0x4/decl/jne tail. */
  i = 0;
  count = 4;
  do {
    float frac;

    /* base RGB triple from placement data */
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];

    if (i < *(int *)(obj_tag + 0x164)) {
      char *cc_elem =
        (char *)tag_block_get_element((void *)(obj_tag + 0x164), i, 0x2c);

      /* deterministic pseudo-random selector from object basis + slot index;
       * the FABS is applied to the dot sum before the modulo. The whole
       * chain stays FPU-resident: fmul/faddp -> fabs -> fldl 1.0 -> call fmod.
       * VC71 /Oi lowers fmod to _CIfmod (the call at 0x1daf7e) and fabsf to a
       * bare FABS; clang takes the x87_fmod (FPREM) branch. */
#if defined(_MSC_VER) && !defined(__clang__)
      frac =
        (float)fmod(fabs((double)(*(float *)(obj + 0x14) * *(float *)0x29bbe0 +
                                  *(float *)(obj + 0xc) * *(float *)0x29bbdc +
                                  *(float *)(obj + 0x10) * *(float *)0x29bbd8 +
                                  (float)i * *(float *)0x29bbd4)),
                    1.0);
#else
      frac = x87_fmod(fabsf(*(float *)(obj + 0x14) * *(float *)0x29bbe0 +
                            *(float *)(obj + 0xc) * *(float *)0x29bbdc +
                            *(float *)(obj + 0x10) * *(float *)0x29bbd8 +
                            (float)i * *(float *)0x29bbd4),
                      1.0);
#endif

      if (*(int *)(cc_elem + 0x20) > 0) {
        int16_t j = 0;
        do {
          float *perm = (float *)tag_block_get_element((void *)(cc_elem + 0x20),
                                                       (int)j, 0x1c);
          if (frac <= perm[0]) {
            float blend;
            /* FABS applies only to obj+0x10 here, before adding the index
             * term; then fmod. Kept FPU-resident to mirror the reference
             * (flds; fabs; flds i; fmuls; faddp; fldl 1.0; call fmod). */
#if defined(_MSC_VER) && !defined(__clang__)
            blend = (float)fmod(fabs((double)*(float *)(obj + 0x10)) +
                                  (float)i * *(float *)0x29bbd0,
                                1.0);
#else
            blend = x87_fmod(fabsf(*(float *)(obj + 0x10)) +
                               (float)i * *(float *)0x29bbd0,
                             1.0);
#endif
            FUN_0007c270(dst, 1, perm + 1, perm + 4, blend);
            break;
          }
          j = j + 1;
        } while ((int)j < *(int *)(cc_elem + 0x20));
      }
    }

    /* clamp each component to [0,1] and store in the +0x30 mirror.
     * Same min/max idiom as object_compute_change_colors: strictly-below-lo
     * -> lo, strictly-above-hi -> hi, else keep (NaN falls through to the
     * value), matching fcomps/test $5/jp then fcomps/test $0x41/jne. */
    dst[0xc] = (dst[0] < REAL_ZERO_POOL) ?
                 REAL_ZERO_POOL :
                 ((dst[0] > REAL_ONE_POOL) ? REAL_ONE_POOL : dst[0]);
    dst[0xd] = (dst[1] < REAL_ZERO_POOL) ?
                 REAL_ZERO_POOL :
                 ((dst[1] > REAL_ONE_POOL) ? REAL_ONE_POOL : dst[1]);
    dst[0xe] = (dst[2] < REAL_ZERO_POOL) ?
                 REAL_ZERO_POOL :
                 ((dst[2] > REAL_ONE_POOL) ? REAL_ONE_POOL : dst[2]);

    src = src + 3;
    dst = dst + 3;
    i = i + 1;
  } while (--count != 0);
}

/* 0x13e3f0 / objects.obj — Scan a model region's permutation block for
 * permutations matching a given variant number. Returns count of matching
 * permutation indices written to output[].
 * region_element pointer in EAX (register arg).
 * Confirmed: loop iterates tag_block at region+0x40, element size 0x58.
 * Confirmed: skips permutations with flags byte [+0x20] bit 0 set.
 * Confirmed: matches on [+0x24]==variant, or variant==-1 && [+0x24]<100.
 * Confirmed: returns count in AX (int16_t). */
int16_t object_find_region_permutations_available_with_variant(
  void *region_element /* @<eax> */, int16_t variant, int16_t *output)
{
  int region_count;
  int16_t out_count;
  int16_t perm_idx;
  char *region = (char *)region_element;

  region_count = *(int *)(region + 0x40);
  out_count = 0;
  perm_idx = 0;
  if (region_count > 0) {
    do {
      char *perm = (char *)tag_block_get_element((void *)(region + 0x40),
                                                 (int)perm_idx, 0x58);
      if ((*(unsigned char *)(perm + 0x20) & 1) == 0) {
        int16_t perm_variant = *(int16_t *)(perm + 0x24);
        if (perm_variant == variant || (variant == -1 && perm_variant < 100)) {
          output[(int)out_count] = perm_idx;
          out_count = out_count + 1;
        }
      }
      perm_idx = perm_idx + 1;
    } while ((int)perm_idx < *(int *)(region + 0x40));
  }
  return out_count;
}

/* 0x13e460 / objects.obj — Determine the variant number for an object by
 * iterating through model regions. For each region, reads the permutation
 * index from the object data at offset 0x130+region_idx, looks up the
 * permutation in the region's tag block, and returns the variant number.
 * Returns 0 if no variant found (all regions have count 0).
 * object_handle in EAX (register arg).
 * Confirmed: PUSH -1; PUSH EAX; CALL object_get_and_verify_type.
 * Confirmed: tag_block at model_tag+0xc4, element size 0x4c.
 * Confirmed: permutation index from object_data[0x130+region_idx].
 * Confirmed: inner tag_block at region+0x40, element size 0x58.
 * Confirmed: returns variant at perm+0x24 (int16_t). */
int16_t object_determine_variant_number(int object_handle /* @<eax> */,
                                        void *model_tag)
{
  char *obj;
  int16_t result;
  int16_t region_idx;
  char *model = (char *)model_tag;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  result = 0;
  /* Loop form follows PAL 2342 objects.c:4583: the region walk stops as
   * soon as a variant number has been found.
   */
  for (region_idx = 0;
       region_idx < *(int *)(model + 0xc4) && result == 0;
       region_idx++) {
    char *region;
    unsigned int perm_index;

    region = (char *)tag_block_get_element((void *)(model + 0xc4),
                                           (int)region_idx, 0x4c);
    perm_index =
      (unsigned int)*(unsigned char *)(obj + 0x130 + (int)region_idx);
    if ((int)perm_index < *(int *)(region + 0x40)) {
      char *perm = (char *)tag_block_get_element((void *)(region + 0x40),
                                                 (int)perm_index, 0x58);
      result = *(int16_t *)(perm + 0x24);
    }
  }

  return result;
}

/* Remove object_handle from a sibling linked list rooted at list_head.
 * Walks the chain at offset 0xc4 (next_sibling) until it finds the entry
 * matching object_handle, then unlinks it.
 * list_head in EAX, object_handle in EBX (register args). */
void object_child_list_remove(void *list_head /* @<eax> */,
                              int object_handle /* @<ebx> */)
{
  int *head = (int *)list_head;
  int *obj_data;

  if (*head == -1)
    return;

  while (1) {
    obj_data = (int *)datum_get(*(data_t **)0x5a8d50, *head);
    obj_data = (int *)*(int *)((char *)obj_data + 8);

    {
      int type = (int)*(int16_t *)((char *)obj_data + 0x64);
      if ((1 << (type & 0x1f)) == 0) {
        char *msg;
        display_assert(
          csprintf((char *)0x5ab100,
                   "got an object type we didn't expect (expected one of "
                   "0x%08x but got #%d).",
                   -1, type),
          "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
        system_exit(-1);
      }
    }

    if (*head == object_handle) {
      *head = *(int *)((char *)obj_data + 0xc4);
      *(int *)((char *)obj_data + 0xc4) = -1;
      return;
    }

    head = (int *)((char *)obj_data + 0xc4);
    if (*head == -1) {
      display_assert("*first_object_reference!=NONE",
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0xc6b, 1);
      system_exit(-1);
      if (*head == -1)
        return;
    }
  }
}

/* 0x13e5d0 / objects.obj — Recompute the object's live change colors from its
 * current animation-function values. Gated on the object tag flag bit 0 at
 * obj_tag+0x24. For each change-color animation entry (block obj_tag+0x164,
 * element size 0x2c): if its blend-function index (entry+0x2) is nonzero,
 * blends an RGB pair (entry+0x8 .. entry+0x14) into the computed color slot
 * (obj+0x168 + i*0xc) using the precomputed function value as the blend
 * weight; if its scale-function index (entry+0x0) is nonzero, multiplies all
 * three components of the slot by that function value; finally clamps each
 * component to [0,1] in place. Function values are read directly from the
 * precomputed array at obj+0xd0 (filled by object_compute_function_values).
 * Role: object spawn-appearance / per-frame appearance update.
 * object_handle in EAX (register arg).
 * Confirmed: PUSH -1; PUSH EAX; CALL object_get_and_verify_type.
 * Confirmed: gate on (*(uint8_t*)(obj_tag+0x24) & 1).
 * Confirmed: function value = *(float*)(obj + 0xd0 + fn_idx*4).
 * Confirmed: computed color slot base obj+0x168 (the +0x30 change-color
 * mirror). Confirmed: clamp lo/hi = REAL_ZERO_POOL / 0x2533c8. */
void object_compute_change_colors(int object_handle /* @<eax> */)
{
  char *obj;
  int obj_tag;
  int16_t i;
  int16_t counter;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  obj_tag = (int)tag_get(0x6f626a65, *(int *)obj);
  if ((*(unsigned char *)(obj_tag + 0x24) & 1) == 0) {
    return;
  }

  i = 0;
  counter = 0;
  if (*(int *)(obj_tag + 0x164) <= 0) {
    return;
  }
  do {
    char *entry =
      (char *)tag_block_get_element((void *)(obj_tag + 0x164), (int)i, 0x2c);
    float *slot = (float *)(obj + 0x168 + (int)i * 0xc);
    int16_t blend_fn;
    int16_t scale_fn;

    /* blend-function: blend the entry RGB pair into the slot */
    blend_fn = *(int16_t *)(entry + 0x2);
    if (blend_fn != 0) {
      float fn_val = (int)blend_fn >= 5 ?
                       *(float *)(obj + 0xe4 + ((int)blend_fn - 5) * 4) :
                       *(float *)(obj + 0xd0 + (int)blend_fn * 4);
      FUN_0007c270(slot, *(int *)(entry + 0x4), (float *)(entry + 0x8),
                   (float *)(entry + 0x14), fn_val);
    }

    /* scale-function: multiply all three components by the function value */
    scale_fn = *(int16_t *)entry;
    if (scale_fn != 0) {
      float fn_val = (int)scale_fn >= 5 ?
                       *(float *)(obj + 0xe4 + ((int)scale_fn - 5) * 4) :
                       *(float *)(obj + 0xd0 + (int)scale_fn * 4);
      slot[0] = fn_val * slot[0];
      slot[1] = fn_val * slot[1];
      slot[2] = fn_val * slot[2];
    }

    /* clamp each component to [0,1] in place.
     * Faithful to the reference min/max idiom: strictly-below-lo -> lo,
     * strictly-above-hi -> hi, else keep (NaN falls through to the value,
     * matching fcomps/test $5/jp then fcomps/test $0x41/jne polarity). */
    slot[0] = (slot[0] < REAL_ZERO_POOL) ?
                REAL_ZERO_POOL :
                ((slot[0] > REAL_ONE_POOL) ? REAL_ONE_POOL : slot[0]);
    slot[1] = (slot[1] < REAL_ZERO_POOL) ?
                REAL_ZERO_POOL :
                ((slot[1] > REAL_ONE_POOL) ? REAL_ONE_POOL : slot[1]);
    slot[2] = (slot[2] < REAL_ZERO_POOL) ?
                REAL_ZERO_POOL :
                ((slot[2] > REAL_ONE_POOL) ? REAL_ONE_POOL : slot[2]);

    counter = counter + 1;
    i = counter;
  } while ((int)i < *(int *)(obj_tag + 0x164));
}

/* 0x13e7b0 / objects.obj — Evaluate all of the object tag's animation
 * functions for the current frame and store the results into the object's
 * function-value array (obj+0xe4 onward), updating the per-function active
 * bitmask byte at obj+0xd3.
 *
 * For each function definition (block obj_tag+0x158, element size 0x168):
 *   - Builds a per-object time input: (game_time_get() + (handle&0xffff)*0x39)
 *     scaled by a global constant (0x2546a4).
 *   - Evaluates a periodic waveform (FUN_0010a5e0) over that time, optionally
 *     scaled by a referenced function value, then applies inversion (flag 1),
 *     a secondary sinusoidal offset term, a step threshold, an exponent/floor
 *     stage, a modulo wrap, an additive function with clamp-to-1, a final
 *     multiplier function, a transition remap (transition_function_evaluate),
 *     a scale, and a range remap with min/max clamping (modes 1/2).
 *   - Computes an "active" bit from flag bit 2 and a dependency function's
 *     active bit (obj+0xd3 & (1<<elem[+0x36])).
 *   - With flag bit 1, wraps the result by adding the prior slot value and
 *     taking fmod(.,1.0) (accumulator).
 *   - Writes the result to obj+0xe4+i*4 and updates obj+0xd3 bit i.
 *
 * Role: object spawn / per-frame appearance; feeds object_compute_change_colors
 * and node/marker animation. Function values 0-4 (obj+0xd0) are engine
 * built-ins; indices 5+ written here begin at obj+0xe4 (=obj+0xd0+5*4).
 * object_handle in EAX (register arg).
 * Confirmed: ESI=object_handle saved before object_get_and_verify_type(EAX,-1).
 * Confirmed: time scale const REAL_ONE_THIRTIETH_POOL; output array obj+0xe4.
 * Confirmed: CMP 0x5;JL branches are vestigial bounds checks (identical loads).
 * Uncertain: many field offsets within the 0x168-byte element (see inline). */
void object_compute_function_values(int object_handle /* @<eax> */)
{
  char *obj;
  int obj_tag;
  float time_base;
  int func_count;
  int i;
  int16_t counter;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  obj_tag = (int)tag_get(0x6f626a65, *(int *)obj);

  /* per-object time input, scaled to seconds */
  time_base = (float)(game_time_get() + (object_handle & 0xffff) * 0x39) *
              REAL_ONE_THIRTIETH_POOL;

  func_count = *(int *)(obj_tag + 0x158);
  counter = 0;
  if (func_count <= 0) {
    return;
  }
  i = 0;
  do {
    char *elem =
      (char *)tag_block_get_element((void *)(obj_tag + 0x158), i, 0x168);
    unsigned char active;
    float value; /* ref: [ebp-4], narrowed at every assignment */
    x87_wide_t value_wide;
    float t;
    int16_t fn;
    int16_t mode;

    active = 1;

    /* --- primary periodic waveform --- */
    t = *(float *)(elem + 0x144);
    fn = *(int16_t *)(elem + 0x8);
    if (fn != 0) {
      float fv = (int)fn >= 5 ? *(float *)(obj + 0xe4 + ((int)fn - 5) * 4) :
                                *(float *)(obj + 0xd0 + (int)fn * 4);
      if (fv > REAL_ZERO_POOL) {
        t = t / fv;
      }
    }
    t = t * time_base;
    value = FUN_0010a5e0(*(int16_t *)(elem + 0xa), t);
    HALO_FLT_ROUNDTRIP(value);

    /* --- optional amplitude function --- */
    fn = *(int16_t *)(elem + 0xc);
    if (fn != 0) {
      value = ((int)fn >= 5 ? *(float *)(obj + 0xe4 + ((int)fn - 5) * 4) :
                              *(float *)(obj + 0xd0 + (int)fn * 4)) *
              value;
      HALO_FLT_ROUNDTRIP(value);
    }

    /* --- inversion (flag bit 0) --- */
    if ((*(unsigned char *)elem & 1) != 0) {
      value = REAL_ONE_POOL - value;
      HALO_FLT_ROUNDTRIP(value);
    }

    /* --- secondary sinusoidal offset term (when elem+0x14 != 0) --- */
    if (*(float *)(elem + 0x14) != 0.0f) {
      float w = FUN_0010a5e0(*(int16_t *)(elem + 0xe),
                             time_base * *(float *)(elem + 0x10));
      w = (w - REAL_0_5_POOL) * *(float *)(elem + 0x14);
      value = w + w + value;
      HALO_FLT_ROUNDTRIP(value);
    }

    /* --- step threshold (when elem+0x18 != 0): 1.0 if value>thr else 0.0 ---
     */
    if (*(float *)(elem + 0x18) != 0.0f) {
      value = value > *(float *)(elem + 0x18) ? 1.0f : 0.0f;
    }

    /* --- exponent/floor stage (when elem+0x1c > 1) --- */
    if (*(int16_t *)(elem + 0x1c) > 1) {
      value = (float)floor((double)((float)*(int16_t *)(elem + 0x1c) * value)) *
              *(float *)(elem + 0x140);
      HALO_FLT_ROUNDTRIP(value);
    }

    /* --- modulo wrap (when elem+0x13c > 0) --- */
    if (*(float *)(elem + 0x13c) > REAL_ZERO_POOL) {
      /* VC71 /Oi lowers fmod to _CIfmod (flds value; flds elem+0x13c; call);
       * clang takes the x87_fmod (FPREM) branch. */
#if defined(_MSC_VER) && !defined(__clang__)
      value = (float)fmod((double)value, (double)*(float *)(elem + 0x13c));
#else
      value = x87_fmod(value, (double)*(float *)(elem + 0x13c));
#endif
      HALO_FLT_ROUNDTRIP(value);
    }

    /* --- additive function with clamp-to-1 --- */
    fn = *(int16_t *)(elem + 0x22);
    if (fn != 0) {
      /* 0x13e9a7 FST (not FSTP): the clamp compare reads the wide sum while
       * the stored value is narrowed. */
      value_wide =
        (x87_wide_t)((int)fn >= 5 ? *(float *)(obj + 0xe4 + ((int)fn - 5) * 4) :
                                    *(float *)(obj + 0xd0 + (int)fn * 4)) +
        value;
      value = HALO_NARROW(value_wide);
      HALO_FLT_ROUNDTRIP(value);
      if (value_wide > REAL_ONE_POOL) {
        value = 1.0f;
      }
    }

    /* --- final multiplier function --- */
    fn = *(int16_t *)(elem + 0x24);
    if (fn != 0) {
      value = ((int)fn >= 5 ? *(float *)(obj + 0xe4 + ((int)fn - 5) * 4) :
                              *(float *)(obj + 0xd0 + (int)fn * 4)) *
              value;
      HALO_FLT_ROUNDTRIP(value);
    }

    /* --- transition remap --- */
    value = transition_function_evaluate(*(int16_t *)(elem + 0x1e), value);
    HALO_FLT_ROUNDTRIP(value);

    /* --- scale (when elem+0x38 > 0) --- */
    if (*(float *)(elem + 0x38) > REAL_ZERO_POOL) {
      value = value * *(float *)(elem + 0x38);
      HALO_FLT_ROUNDTRIP(value);
    }

    /* --- range remap (modes 1/2) --- */
    mode = *(int16_t *)(elem + 0x26);
    if (mode == 2) {
      value = (*(float *)(elem + 0x2c) - *(float *)(elem + 0x28)) * value +
              *(float *)(elem + 0x28);
      HALO_FLT_ROUNDTRIP(value);
      if (*(float *)(elem + 0x28) + REAL_0_0001_POOL >= value) {
        active = (unsigned char)(*(unsigned int *)elem >> 2) & 1;
      }
    } else {
      if (*(float *)(elem + 0x28) + REAL_0_0001_POOL >= value) {
        value = *(float *)(elem + 0x28);
        active = (unsigned char)(*(unsigned int *)elem >> 2) & 1;
      }
      if (value > *(float *)(elem + 0x2c)) {
        value = *(float *)(elem + 0x2c);
      }
      if (mode == 1) {
        value = (value - *(float *)(elem + 0x28)) * *(float *)(elem + 0x138);
        HALO_FLT_ROUNDTRIP(value);
      }
    }

    /* --- dependency on another function's active bit --- */
    if (*(int16_t *)(elem + 0x36) != -1 &&
        (*(unsigned char *)(obj + 0xd3) &
         (1 << (int)*(int16_t *)(elem + 0x36))) == 0) {
      active = 0;
    }

    /* --- accumulator wrap (flag bit 1), using prior slot value --- */
    if ((*(unsigned char *)elem & 2) != 0) {
      /* accumulator wrap: fmod(value + prior_slot, 1.0) kept FPU-resident
       * (flds value; fadds slot; fldl 1.0; call _CIfmod under VC71). */
#if defined(_MSC_VER) && !defined(__clang__)
      value =
        (float)fmod((double)(value + *(float *)(obj + 0xe4 + (int)i * 4)), 1.0);
#else
      value = x87_fmod(value + *(float *)(obj + 0xe4 + (int)i * 4), 1.0);
#endif
    }

    /* --- store result and update active bitmask --- */
    *(float *)(obj + 0xe4 + (int)i * 4) = value;
    if (active != 0) {
      *(unsigned char *)(obj + 0xd3) =
        *(unsigned char *)(obj + 0xd3) | (unsigned char)(1 << (int)i);
    } else {
      *(unsigned char *)(obj + 0xd3) =
        *(unsigned char *)(obj + 0xd3) & ~(unsigned char)(1 << (int)i);
    }

    counter = counter + 1;
    i = counter;
  } while (i < *(int *)(obj_tag + 0x158));
}

void object_scripting_set_collideable(int param_1, char param_2)
{
  int iVar1;

  if (param_1 != -1) {
    iVar1 = (int)object_get_and_verify_type(param_1, 0xffffffff);
    if (param_2 == '\0') {
      *(unsigned int *)(iVar1 + 4) = *(unsigned int *)(iVar1 + 4) | 0x1000000;
      return;
    }
    *(unsigned int *)(iVar1 + 4) = *(unsigned int *)(iVar1 + 4) & 0xfeffffff;
  }
}

/*
 * object_reset_markers — begin a marker sweep pass.
 *
 * Asserts that no marker pass is in progress, increments the global marker
 * generation counter (0x5a8d28), and sets
 * object_globals->object_marker_initialized to true.
 *
 * Confirmed: void, no params (no stack args referenced).
 * Confirmed: TEST byte ptr [EAX+1] — checks object_marker_initialized.
 * Confirmed: INC dword ptr [0x5a8d28] — increments generation counter.
 * Confirmed: MOV byte ptr [EAX+1], 1 — sets marker_initialized = true.
 */
void object_reset_markers(void)
{
  object_globals_t *g = object_globals;

  if (g->object_marker_initialized) {
    display_assert("!object_globals->object_marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0xdaf, 1);
    system_exit(-1);
    (*(int *)0x5a8d28)++;
    object_globals->object_marker_initialized = 1;
    return;
  }
  (*(int *)0x5a8d28)++;
  g->object_marker_initialized = 1;
}

/*
 * object_marker_end (0x13ebc0) — end a marker sweep pass.
 *
 * Asserts that a marker pass is currently in progress
 * (object_marker_initialized must be true), then clears the flag to signal the
 * sweep is complete. Paired with object_reset_markers which begins the sweep.
 *
 * Confirmed: no prologue, no stack frame, no arguments.
 * Confirmed: MOV EAX,[0x46f084] -> object_globals.
 * Confirmed: MOV CL,[EAX+0x1] -> object_globals->object_marker_initialized.
 * Confirmed: TEST CL,CL; JNZ -> skips assert if initialized (true).
 * Confirmed: assert string "object_globals->object_marker_initialized" at line
 * 0xdba. Confirmed: CALL 0x8d9f0 (display_assert), CALL 0x8e2f0
 * (system_exit(-1)). Confirmed: ADD ESP,0x14 cleans 5 args (display_assert 4 +
 * system_exit 1). Confirmed: MOV byte ptr [EAX+0x1],0x0 -> clears
 * object_marker_initialized.
 */
void object_marker_end(void)
{
  if (!object_globals->object_marker_initialized) {
    display_assert("object_globals->object_marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0xdba, 1);
    system_exit(-1);
  }
  object_globals->object_marker_initialized = 0;
}

/*
 * object_markers_need_update (0x13ec00) — query whether an object still needs
 * marking in the current sweep.
 *
 * Looks up the object (any type), asserts a marker sweep is in progress, then
 * returns whether the object's marker_generation (obj+0x08) differs from the
 * global marker generation counter at 0x5a8d28. Returns nonzero (true) when
 * the object has not yet been stamped this sweep. The read-only predicate
 * counterpart to object_mark (0x13ec50), which performs the same comparison
 * and then stamps the object.
 *
 * Confirmed: object_get_and_verify_type(handle, -1).
 * Confirmed: assert "object_globals->object_marker_initialized" at line 0xdc6.
 * Confirmed: compares obj->marker_generation (obj+0x08) against [0x5a8d28].
 * Confirmed: CONCAT31 => char/bool-width return of (generation != counter).
 */
int object_markers_need_update(int object_handle)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  if (!object_globals->object_marker_initialized) {
    display_assert("object_globals->object_marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0xdc6, 1);
    system_exit(-1);
  }

  return obj->marker_generation != *(uint32_t *)0x5a8d28;
}

/*
 * object_mark (0x13ec50) — mark an object with the current generation.
 *
 * Looks up the object (any type), asserts a marker sweep is in progress,
 * then compares the object's marker_generation (obj+0x08) against the
 * global generation counter at 0x5a8d28. If they differ, stamps the object
 * with the current generation and returns 1 (newly marked). If equal,
 * returns 0 (already marked this sweep).
 *
 * Confirmed: PUSH -1, PUSH EAX -> object_get_and_verify_type(handle, -1).
 * Confirmed: MOV ECX,[0x46f084]; MOV AL,[ECX+0x1] -> object_marker_initialized.
 * Confirmed: ADD ESP,0x8 cleans 2 args for object_get_and_verify_type.
 * Confirmed: assert "object_globals->object_marker_initialized" at line 0xdd7.
 * Confirmed: MOV EAX,[0x5a8d28] -> global marker generation counter.
 * Confirmed: CMP [ESI+0x8],EAX -> obj->marker_generation at offset 0x08.
 * Confirmed: MOV AL,0x1 / XOR AL,AL for return 1/0 (byte-sized).
 */
int object_mark(int object_handle)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  if (!object_globals->object_marker_initialized) {
    display_assert("object_globals->object_marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0xdd7, 1);
    system_exit(-1);
  }

  if (obj->marker_generation != *(uint32_t *)0x5a8d28) {
    obj->marker_generation = *(uint32_t *)0x5a8d28;
    return 1;
  }
  return 0;
}

/* attachments_new (0x13ecb0 / objects.obj) — create every attachment defined in
 * an object's tag and record each in the object datum's attachment table.
 *
 * Resolves the object datum and its 'obje' tag, then for each attachment
 * element (block at tag+0x140, stride 0x48) whose definition tag (element+0xc)
 * is valid, classifies the attachment by its group tag (element+0x0) into one
 * of five types and dispatches to the matching creator: type 0 'ligh' ->
 * light_new  (light)        ; sets object flag 0x100 type 1 'lsnd' ->
 * game_looping_sound_new       ; sets object flag 0x400 type 2 'effe' ->
 * effect_new_looping  (effect) type 3 'cont' -> contrail_new type 4 'pctl' ->
 * particle_system_new_attached  (particle) The attachment type byte is stored
 * at object+0xf4+i and the created handle at object+0xfc+i*4. Marker indices
 * passed to creators are element fields minus 1 (element+0x30/+0x32/+0x34 ->
 * marker / secondary / tertiary).
 *
 * DORMANT — kept ported=false. Gate B: caller edge from object_new (0x143c80,
 * lifecycle cluster member) at 0x144147, AND object-lifecycle mutation (fills
 * the object attachment table at +0xf4/+0xfc and sets creation flags at
 * object+0x4). Part of the object creation path; activate only with the
 * lifecycle cluster.
 *
 * Confirmed: 1 cdecl arg (object_handle @ [EBP+0x8]).
 * Confirmed: jump table at 0x13ee4c maps type 0..4 to the five creators.
 * Confirmed: attachment element def index = element[3] (+0xc); store offsets
 * object+0xf4+i (type byte) and object+0xfc+i*4 (handle).
 * Confirmed: loop index is int16_t (MOVSX EDI,AX); count re-read from
 * tag+0x140.
 */
void attachments_new(int object_handle)
{
  int *obj;
  int obj_tag;
  unsigned int *element;
  unsigned int def;
  unsigned int group;
  short attachment_type;
  int attachment_index;
  int count;
  short attachment_num; /* name: PAL 2342 objects.c:3088 */
  int idx;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  obj_tag = (int)tag_get(0x6f626a65, *(int *)obj);
  attachment_num = 0;
  idx = 0;
  count = *(int *)(obj_tag + 0x140);
  if (0 < count) {
    do {
      element = (unsigned int *)tag_block_get_element((void *)(obj_tag + 0x140),
                                                      idx, 0x48);
      def = element[3];
      attachment_type = -1;
      attachment_index = -1;
      if (def != 0xffffffff) {
        group = element[0];
        switch (group) {
        case 0x6c696768: /* 'ligh' */
          attachment_type = 0;
          break;
        case 0x6c736e64: /* 'lsnd' */
          attachment_type = 1;
          break;
        case 0x65666665: /* 'effe' */
          attachment_type = 2;
          break;
        case 0x636f6e74: /* 'cont' */
          attachment_type = 3;
          break;
        case 0x7063746c: /* 'pctl' */
          attachment_type = 4;
          break;
        }
      }
      switch (attachment_type) {
      case 0:
        attachment_index = light_new((int)def, object_handle, attachment_num,
                           (short)(*(short *)((char *)element + 0x30) - 1),
                           (short)(*(short *)((char *)element + 0x34) - 1));
        if (attachment_index != -1) {
          obj[1] = obj[1] | 0x100;
        }
        break;
      case 1:
        attachment_index = game_looping_sound_new(
          object_handle, (int)def, element + 4,
          (short)(*(short *)((char *)element + 0x30) - 1));
        if (attachment_index != -1) {
          obj[1] = obj[1] | 0x400;
        }
        break;
      case 2:
        attachment_index =
          effect_new_looping((int)def, object_handle,
                             (short)(*(short *)((char *)element + 0x30) - 1),
                             (short)(*(short *)((char *)element + 0x32) - 1),
                             (short)(*(short *)((char *)element + 0x34) - 1));
        break;
      case 3:
        attachment_index = contrail_new((int)def, object_handle, attachment_num);
        break;
      case 4:
        attachment_index = particle_system_new_attached((int)def, object_handle, attachment_num);
        break;
      default:
        break;
      }
      *((char *)obj + 0xf4 + idx) = (char)attachment_type;
      obj[idx + 0x3f] = attachment_index;
      attachment_num++;
      idx = (int)attachment_num;
    } while (idx < *(int *)(obj_tag + 0x140));
  }
}

/* Propagate flags to all children of an object. For each child slot where
 * the "created" flag at obj+0xf4+i is clear and the child handle is valid,
 * optionally calls light_disconnect_from_map (param_1) and/or light_reconnect_to_map (param_2).
 * object_handle in EAX (register arg). */
void object_propagate_flag_to_children(int object_handle /* @<eax> */,
                                       int param_1, int param_2)
{
  int *obj;
  void *tag_data;
  int16_t i;
  int count;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  if ((obj[1] & 0x100) == 0)
    return;

  tag_data = tag_get(0x6f626a65, obj[0]);
  count = *(int *)((char *)tag_data + 0x140);
  i = 0;
  while ((int)i < count) {
    if (*((char *)obj + 0xf4 + (int)i) == 0 && obj[(int)i + 0x3f] != -1) {
      if (param_1 != 0)
        light_disconnect_from_map(obj[(int)i + 0x3f]);
      if (param_2 != 0)
        light_reconnect_to_map(obj[(int)i + 0x3f]);
    }
    i++;
  }
}

/* 0x13ef70 / objects.obj — Add an object to the scenario name table.
 * Validates name_index is in [0, 0x1FF], checks the name slot is free,
 * and writes the object_handle into the name table. Sets the object's
 * name field at obj+0x6a.
 * object_handle in EDI, name_index in SI (register args).
 * Confirmed: PUSH -1; PUSH EDI; CALL object_get_and_verify_type.
 * Confirmed: CMP SI,0x200 for range check.
 * Confirmed: name_table at DAT_0046f07c[name_index].
 * Confirmed: on collision, calls error(2, "an object with the name '%s' already
 * exists!", name). */
void object_name_list_new(int object_handle /* @<edi> */,
                          int16_t name_index /* @<si> */)
{
  char *obj;
  int *name_table;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  if (name_index < 0 || name_index >= 0x200) {
    display_assert(
      "name_index>=0 && name_index<MAXIMUM_OBJECT_NAMES_PER_SCENARIO",
      "c:\\halo\\SOURCE\\objects\\objects.c", 0x1003, 1);
    system_exit(-1);
  }

  name_table = *(int **)0x46f07c;
  if (name_table[(int)name_index] == -1) {
    name_table[(int)name_index] = object_handle;
    *(int16_t *)(obj + 0x6a) = name_index;
    return;
  }

  {
    void *scenario_data = (void *)((char *)global_scenario_get() + 0x204);
    char *name =
      (char *)tag_block_get_element(scenario_data, (int)name_index, 0x24);
    error(2, "an object with the name \'%s\' already exists!", name);
  }
}

/* Remove an object from the scenario object-name lookup table.
 * Clears the name_index field (obj+0x6a) and removes all references
 * to object_handle from the name table at 0x46f07c.
 * object_handle in EDI (register arg). */
void object_remove_from_name_list(int object_handle /* @<edi> */)
{
  char *obj;
  void *scenario;
  int *name_table;
  int16_t i;
  int count;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  if (*(int16_t *)(obj + 0x6a) != -1) {
    scenario = global_scenario_get();
    *(int16_t *)(obj + 0x6a) = -1;
    count = *(int *)((char *)scenario + 0x204);
    i = 0;
    if (count > 0) {
      name_table = *(int **)0x46f07c;
      do {
        if (name_table[(int)i] == object_handle)
          name_table[(int)i] = -1;
        i++;
      } while ((int)i < count);
    }
  }
}

/*
 * objects_place — place all scenario objects for the current map.
 *
 *
 * Sets the object_is_being_placed flag on object_globals, calls the scenario
 * object placer (object_types_place_all, unported), then clears the flag. The
 * flag is at byte offset 0x00 of the object_globals struct.
 *
 * Confirmed: MOV byte ptr [EAX], 0x1 / MOV byte ptr [ECX], 0x0
 * Confirmed: global_scenario_get() result (EAX) pushed as sole arg to placer.
 * Confirmed: ADD ESP,0x4 after the placer call (1 cdecl arg).
 */
void objects_place(void)
{
  scenario_t *scenario;

  /* Set object_is_being_placed = true */
  object_globals->object_is_being_placed = 1;

  /* Get the scenario pointer and pass it to the object placer */
  scenario = global_scenario_get();
  object_types_place_all((int)scenario);

  /* Clear object_is_being_placed */
  object_globals->object_is_being_placed = 0;
}

/* 0x13f080 - Walk the object tree recursively, collecting objects into an
 * output array. Starts from param_1, recurses depth-first into child (obj+0xc8)
 * then sibling (obj+0xc4). param_2: optional filter callback(handle, context) —
 * include object when non-zero; NULL = include all. param_3: opaque context
 * value forwarded to filter callback. param_4: current insertion index into
 * output array. param_5: maximum capacity of output array (stops when param_4
 * >= param_5). param_6: output array (int[]) that receives matching object
 * handles. Returns: updated count after processing this subtree. */
int recursive_object_adder(int param_1, char (*param_2)(int, int), int param_3,
                           int param_4, int param_5, int *param_6)
{
  void *local_c;

  local_c = object_get_and_verify_type(param_1, 0xffffffff);
  if (param_4 < param_5) {
    if (param_2 == 0 || (*param_2)(param_1, param_3) != '\0') {
      param_6[param_4] = param_1;
      param_4 = param_4 + 1;
    }
    if (*(int *)((char *)local_c + 0xc8) != -1) {
      param_4 =
        recursive_object_adder(*(int *)((char *)local_c + 0xc8), param_2,
                               param_3, param_4, param_5, param_6);
    }
    if (*(int *)((char *)local_c + 0xc4) != -1) {
      param_4 =
        recursive_object_adder(*(int *)((char *)local_c + 0xc4), param_2,
                               param_3, param_4, param_5, param_6);
    }
  }
  return param_4;
}

/* find_objects_from_point_vector / objects.obj -- find objects visible from a
 * point by iterating BSP cluster bitmask words. */
int find_objects_from_point_vector(int param_1, int param_2, int param_3,
                                   int param_4, int param_5, int param_6)
{
  int result;
  int bsp_check;
  void *bsp_ref_element;
  short bsp_index;
  int *cluster_data;
  int *cluster_ptr;
  int abs_cluster;
  int cluster_end;
  int num_words;
  int outer_idx;
  int obj_handle;
  int obj_datum;
  int *obj_body;
  int type_val;
  int type_mask, marker;
  int iter_state;

  result = 0;
  bsp_check = FUN_0018e720(param_1);
  if (bsp_check == -1)
    goto done;

  bsp_ref_element = tag_block_get_element(
    (void *)((char *)scenario_get() + 0xe0),
    FUN_0018e720(param_1) & 0x7fffffff, 0x10);
  bsp_index = *(short *)((char *)bsp_ref_element + 8);
  if (bsp_index == -1)
    goto done;

  object_reset_markers();
  cluster_data =
    (int *)structure_bsp_get_cluster_sound_data(scenario_get(), bsp_index);

  {
    void *bsp_data = scenario_get();
    num_words = (*(int *)((char *)bsp_data + 0x134) + 0x1f) >> 5;
  }
  outer_idx = 0;
  if ((short)num_words <= 0)
    goto post_loop;

  cluster_ptr = cluster_data;
  while (1) {
    if (*cluster_ptr != 0) {
      short offset = (short)(outer_idx << 5);
      short size;
      short j;

      cluster_end = offset + 0x20;
      if (cluster_end > *(int *)((char *)scenario_get() + 0x134)) {
        size = *(short *)((char *)scenario_get() + 0x134);
      } else {
        size = (short)cluster_end;
      }

      for (j = offset; j < size; j++) {
        abs_cluster = j;
        if ((cluster_data[abs_cluster >> 5] & (1 << (abs_cluster & 0x1f))) !=
            0) {
          obj_handle = cluster_partition_iter_first((void *)0x5a8d40,
                                                    &iter_state, j);
          while (obj_handle != -1) {
            obj_datum = (int)datum_get(*(data_t **)0x5a8d50, obj_handle);
            obj_body = *(int **)(obj_datum + 8);

            type_val = (int)*(short *)((char *)obj_body + 0x64);
            type_mask = 1 << type_val;
            if (type_mask == 0) {
              display_assert(csprintf((char *)0x5ab100,
                                      "got an object type we didn't expect "
                                      "(expected one of 0x%08x but got #%d).",
                                      -1, type_val),
                             "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
              system_exit(-1);
            }

            if (*(char *)(*(int *)0x46f084 + 1) == '\0') {
              display_assert("object_globals->object_marker_initialized",
                             "c:\\halo\\SOURCE\\objects\\objects.c", 0xdd7, 1);
              system_exit(-1);
            }
            marker = *(int *)0x5a8d28;
            if (*(int *)((char *)obj_body + 8) != marker) {
              *(int *)((char *)obj_body + 8) = marker;
              result = recursive_object_adder(
                obj_handle, (char (*)(int, int))param_3, param_4, result,
                param_5, (int *)param_6);
            }

            obj_handle = cluster_partition_iter_next((void *)0x5a8d40,
                                                     &iter_state);
          }
        }
      }
    }

    outer_idx++;
    cluster_ptr++;
    if ((short)outer_idx >= (short)num_words)
      break;
  }

post_loop:
  if (*(char *)(*(int *)0x46f084 + 1) == '\0') {
    display_assert("object_globals->object_marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0xdba, 1);
    system_exit(-1);
  }
  *(char *)(*(int *)0x46f084 + 1) = 0;

done:
  return result;
}

int sort_dumps(int param_1, int param_2)
{
  if (*(int *)(param_1 + 8) < *(int *)(param_2 + 8)) {
    return 1;
  }
  return (*(int *)(param_1 + 8) <= *(int *)(param_2 + 8)) - 1;
}

struct dump_datum {
  int32_t definition_index; /* +0x00 */
  int16_t object_type; /* +0x04 */
  int16_t maximum_size; /* +0x06 */
  int32_t total_size; /* +0x08 */
  int16_t count; /* +0x0c */
  int16_t active_count; /* +0x0e */
  int16_t garbage_count; /* +0x10 */
  int16_t dead_count; /* +0x12 */
  int16_t outside_map_count; /* +0x14 */
  int16_t at_rest_count; /* +0x16 */
};

/* 0x13f3b0 / objects.obj — Accumulate statistics about one object into a
 * dump record. Reads the object header (via datum_get on 0x5a8d50) and
 * object data (via object_get_and_verify_type), and updates various
 * counters in the stats structure.
 * object_handle in EBX, stats pointer in ESI (register args).
 * Confirmed: PUSH EBX to datum_get and object_get_and_verify_type.
 * Confirmed: ESI+0x6 = max_size, ESI+0x8 = total_size, ESI+0xc = count,
 *   ESI+0xe = header_flag_count, ESI+0x10 = obj_flag_10000,
 *   ESI+0x12 = obj_flag_b6_4, ESI+0x14 = orphaned_count,
 *   ESI+0x16 = obj_flag_20. */
void object_add_to_dump(int object_handle /* @<ebx> */,
                        void *stats /* @<esi> */)
{
  char *hdr;
  char *obj;
  int parent_handle;
  char *parent_obj;
  /* header->data_size (hdr+0x6) is re-read per use, PAL 2342 objects.c:4441 */
  struct dump_datum *st = (struct dump_datum *)stats;

  hdr = (char *)datum_get(*(data_t **)0x5a8d50, object_handle);
  obj = (char *)object_get_and_verify_type(object_handle, -1);

  if (*(int16_t *)(hdr + 0x6) > st->maximum_size) {
    st->maximum_size = *(int16_t *)(hdr + 0x6);
  }
  st->total_size += *(int16_t *)(hdr + 0x6);
  st->count++;

  if ((*(unsigned char *)(hdr + 0x2) & 1) != 0) {
    st->active_count++;
  }
  if ((*(unsigned int *)(obj + 0x4) & 0x10000) != 0) {
    st->garbage_count++;
  }
  if ((*(unsigned char *)(obj + 0xb6) & 4) != 0) {
    st->dead_count++;
  }
  if ((*(unsigned char *)(obj + 0x4) & 0x20) != 0) {
    st->at_rest_count++;
  }

  parent_handle = object_get_root_parent(object_handle);
  parent_obj = (char *)object_get_and_verify_type(parent_handle, -1);
  if ((*(unsigned int *)(parent_obj + 0x4) & 0x200000) != 0 ||
      *(int16_t *)(parent_obj + 0x4c) == -1) {
    st->outside_map_count++;
  }
}

/* 0x13f440 / objects.obj — Write one dump stats record to a file.
 * Formats the stats structure into a single line with counts/sizes.
 * stats pointer in ESI (register arg), file pointer as stack param.
 * Confirmed: fprintf format string at 0x29bcf4.
 * Confirmed: reads stats fields for count, active, orphaned, total_size etc. */
void object_dump_write(void *stats /* @<esi> */, void *file)
{
  char *pcVar1;
  struct dump_datum *st = (struct dump_datum *)stats;

  pcVar1 = "unknown";
  if (st->definition_index != -1) {
    pcVar1 = (char *)tag_get_name((int)st->definition_index);
  } else if (st->object_type != -1) {
    pcVar1 = (char *)object_type_get_name(st->object_type);
  }
  crt_fprintf(file, "% 6d (% 6d) [% 7d/% 7d/% 7d/% 7d] % 7d % 7d %s\r\n",
              (int)st->count, (int)st->active_count, (int)st->garbage_count,
              (int)st->dead_count, (int)st->outside_map_count,
              (int)st->at_rest_count, (int)st->maximum_size,
              (int)st->total_size, pcVar1);
}

/* 0x13f4b0 — objects_dump_memory: diagnostic memory dump of all objects,
 * grouped by type and definition. Writes to a file.
 *
 * Source: objects.c
 * No params. Has 768KB+ stack for large arrays (_chkstk handles this).
 */
/* 0x13f4b0 */
void objects_dump_memory(void)
{
  struct dump_datum dumps[1024];
  struct dump_datum dumps_by_type[12];
  object_iter_t dump_iter;
  object_header_data_t *header;
  object_data_t *object;
  void *stream;
  char *name;
  struct objects_information info;
  struct dump_datum *dump;
  int16_t object_type;
  int16_t object_num;
  int16_t dump_count;
  int16_t overflowed_count;

  dump_count = 0;
  overflowed_count = 0;

  csmemset(dumps, 0, sizeof(dumps));
  csmemset(dumps_by_type, 0, sizeof(dumps_by_type));

  for (object_type = 0; object_type < 12; ++object_type) {
    dumps_by_type[object_type].object_type = object_type;
    dumps_by_type[object_type].definition_index = -1;
  }

  dump_iter.type_mask = -1;
  dump_iter.flags = 0;
  dump_iter.current_index = 0;
  dump_iter.last_handle = NONE;
  dump_iter.cookie = 0x86868686;

  while ((object = (object_data_t *)object_iterator_next(&dump_iter)) != NULL) {
    int16_t index = -1;

    for (object_num = 0; object_num < dump_count; ++object_num) {
      if (dumps[object_num].definition_index == (int32_t)object->tag_index) {
        index = object_num;
        break;
      }
    }

    if (index == -1) {
      if (dump_count < 1024) {
        index = dump_count;
        dump_count++;
        dumps[index].object_type = -1;
        dumps[index].definition_index = (int32_t)object->tag_index;
      } else {
        overflowed_count++;
      }
    }

    header = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50,
                                               dump_iter.last_handle);

    if (index != -1) {
      object_add_to_dump(dump_iter.last_handle, &dumps[index]);
    }

    object_add_to_dump(dump_iter.last_handle, &dumps_by_type[header->type]);
  }

  qsort(dumps, (size_t)dump_count, sizeof(struct dump_datum),
        (int (*)(const void *, const void *))sort_dumps);
  qsort(dumps_by_type, 12, sizeof(struct dump_datum),
        (int (*)(const void *, const void *))sort_dumps);

  stream = (void *)crt_fopen("d:\\object_memory.txt", "wt");
  if (stream != NULL) {
    objects_information_get(&info);

    crt_fprintf(
      stream, "#%d objects (#%d active) using %3.2f%% of available memory\n\n",
      info.object_count, info.active_object_count,
      (double)(100.0f * info.used_memory));

    crt_fprintf(stream, "OBJECTS BY TYPE\n");
    crt_fprintf(
      stream,
      "number (active) [garbage/   dead/outside/at-rest] maxsize totsize\n");
    dump = dumps_by_type;
    for (object_type = 0; object_type < 12; object_type++) {
      name = "unknown";
      if (dump->definition_index != -1) {
        name = (char *)tag_get_name((int)dump->definition_index);
      } else if (dump->object_type != -1) {
        name = (char *)object_type_get_name(dump->object_type);
      }
      crt_fprintf(stream, "% 6d (% 6d) [% 7d/% 7d/% 7d/% 7d] % 7d % 7d %s\r\n",
                  (int)dump->count, (int)dump->active_count,
                  (int)dump->garbage_count, (int)dump->dead_count,
                  (int)dump->outside_map_count, (int)dump->at_rest_count,
                  (int)dump->maximum_size, (int)dump->total_size, name);
      dump++;
    }

    crt_fprintf(stream, "\n");
    crt_fprintf(stream, "OBJECTS BY DEFINITION\n");
    crt_fprintf(
      stream,
      "number (active) [garbage/   dead/outside/at-rest] maxsize totsize\n");
    dump = dumps;
    for (object_num = 0; object_num < dump_count; object_num++) {
      name = "unknown";
      if (dump->definition_index != -1) {
        name = (char *)tag_get_name((int)dump->definition_index);
      } else if (dump->object_type != -1) {
        name = (char *)object_type_get_name(dump->object_type);
      }
      crt_fprintf(stream, "% 6d (% 6d) [% 7d/% 7d/% 7d/% 7d] % 7d % 7d %s\r\n",
                  (int)dump->count, (int)dump->active_count,
                  (int)dump->garbage_count, (int)dump->dead_count,
                  (int)dump->outside_map_count, (int)dump->at_rest_count,
                  (int)dump->maximum_size, (int)dump->total_size, name);
      dump++;
    }

    crt_fprintf(stream, "\n");
    if (overflowed_count > 0) {
      crt_fprintf(stream,
                  "WARNING: overflowed MAXIMUM_DUMPS (%d), this dump does not "
                  "include %d objects that would not fit!\n",
                  1024, (int)overflowed_count);
    }
    crt_fprintf(stream, "\n");
    crt_fclose(stream);
  }
}

/*
 * objects_initialize — one-time initialisation of the object subsystem.
 *
 * Called once at startup (not per-map). Allocates the four root resources:
 *   - object header data table (data_t*) stored at 0x5a8d50
 *   - object memory pool (void*) stored at objects (0x46f080)
 *   - object_globals struct stored at object_globals (0x46f084)
 *   - object_name_list buffer stored at object_name_list (0x46f07c)
 * Then initialises the collideable and noncollideable cluster partition
 * structures at 0x5a8d40 and 0x5a8d30 via FUN_00191500.
 *
 * The allocation strategy differs between editor and non-editor modes:
 *   Non-editor: game_state_data_new("object", 0x800, 0xc) — data_array_new from
 *               game-state block; game_state_memory_pool_new("objects",
 * 0x100000) — memory-pool_new from game-state block. Editor: data_new("object",
 * 0x2800, 0xc) — data_array_new from main heap; memory_pool_new("objects",
 * &DAT_500000) — memory- pool_new from main heap using a size read from
 * 0x500000.
 *
 * Sub-system init call order (confirmed from disasm):
 *   widgets_initialize_for_new_map — unknown object sub-type A init
 *   widgets_initialize — unknown object sub-type B init
 *   object_types_initialize — object type definition list init
 *   lights_initialize — object BSP cluster data init
 *
 * Confirmed: PUSH 0xc pre-pushed before JNZ — shared 3rd arg to both
 *            first-call variants; ADD ESP,0x14 cleans 5 args (3+2).
 * Confirmed: game_in_editor() returns bool via AL; TEST AL,AL / JNZ selects
 *            the editor allocation path.
 * Confirmed: ADD ESP,0x14 after display_assert+system_exit cleans 5 words
 *            (4 for display_assert + 1 for system_exit) in all 3 assert sites.
 * Confirmed: game_state_malloc("object globals", 0, 0x98) allocates
 * object_globals; game_state_malloc("object name list", 0, 0x800) allocates
 * name list. Both are game_state_alloc(name, tag?, size) — 3 cdecl args, ADD
 * ESP,0xc each. Confirmed: FUN_00191500 takes (void *partition, const char
 * *name) — called twice; ADD ESP,0x10 cleans both calls (2 args * 2 calls).
 */
void objects_initialize(void)
{
  /* Initialise sub-systems (order confirmed from disasm) */
  damage_initialize();
  ((pfn_void_t)0x135f90)();
  object_types_initialize();
  ((pfn_void_t)0x1391e0)();

  if (!game_in_editor()) {
    /* Non-editor: allocate from game-state block */
    *(void **)0x5a8d50 =
      ((void *(*)(const char *, int, int))0x1bfe10)("object", 0x800, 0xc);
    objects = ((void *(*)(const char *, int))0x1bfe50)("objects", 0x100000);
  } else {
    /* Editor: allocate from main heap */
    *(void **)0x5a8d50 =
      ((void *(*)(const char *, int, int))0x1194d0)("object", 0x2800, 0xc);
    objects =
      ((void *(*)(const char *, void *))0x11e650)("objects", (void *)0x500000);
  }

  if (*(void **)0x5a8d50 == 0 || objects == 0) {
    display_assert("object_header_data && object_memory_pool",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0xd8, 1);
    system_exit(-1);
  }

  object_globals = ((object_globals_t * (*)(const char *, int, int))0x1bfbf0)(
    "object globals", 0, 0x98);
  if (object_globals == 0) {
    display_assert("object_globals", "c:\\halo\\SOURCE\\objects\\objects.c",
                   0xdb, 1);
    system_exit(-1);
  }

  object_name_list =
    ((void *(*)(const char *, int, int))0x1bfbf0)("object name list", 0, 0x800);
  if (object_name_list == 0) {
    display_assert("object_name_list", "c:\\halo\\SOURCE\\objects\\objects.c",
                   0xfe8, 1);
    system_exit(-1);
  }

  /* Initialise collideable and noncollideable cluster partition structs */
  ((void (*)(void *, const char *))0x191500)((void *)0x5a8d40,
                                             "collideable object");
  ((void (*)(void *, const char *))0x191500)((void *)0x5a8d30,
                                             "noncollideable object");
}

/*
 * objects_initialize_for_new_map — reset object subsystems when loading a map.
 *
 * Call order (confirmed from disasm):
 *   widgets_dispose  — resets a global slot index (object type slot reset)
 *   widgets_initialize_for_new_map  — iterates 5 object type slots, calls
 * initialize_for_new_map vtable entry via [EDI] (slot stride 0x28)
 *   object_types_initialize_for_new_map  — walks the object_type_definition
 * linked list, calls each type's initialize_for_new_map function at +0x18
 *   lights_initialize_for_new_map  — calls data_delete_all on a BSP cluster
 * data table, then object_list_initialize_for_new_map via FUN_1915d0
 *
 * Then:
 *   data_delete_all(*(data_t**)0x5a8d50)        — clear all object headers
 *   csmemset(object_name_list, 0xff, 0x800)      — reset name list
 *   FUN_001915d0(&collideable_cluster_partition)  — reset collideable cluster
 *   FUN_001915d0(&noncollideable_cluster_partition) — reset noncollideable
 *   csmemset(object_globals->combined_pvs, 0, 64)
 *   csmemset(object_globals->combined_pvs_local, 0, 64)
 *   object_globals->pvs_activator_type = 0
 *   object_globals->object_marker_initialized = 0
 *   *(uint32_t*)0x5a8d28 = 0                     — unknown global counter
 *   object_globals->unk_8 = 0xffffffff            — datum handle sentinel
 *   object_globals->unk_4 = 0
 *   object_globals->last_garbage_collection_tick = 0
 *
 * Confirmed: ADD ESP,0x30 cleans 12 args across the 4 csmemset calls and
 *            the two FUN_1915d0 calls, consistent with 12 total cdecl pushes.
 */
void objects_initialize_for_new_map(void)
{
  damage_initialize_for_new_map();
  widgets_initialize_for_new_map();
  object_types_initialize_for_new_map();
  lights_initialize_for_new_map();

  data_delete_all(*(data_t **)0x5a8d50);
  csmemset(object_name_list, 0xff, 0x800);

  /* Reset collideable and noncollideable cluster partition structs.
   * These are 12-byte structs (3 data_t* fields) at fixed addresses. */
  cluster_partition_clear((void *)0x5a8d40);
  cluster_partition_clear((void *)0x5a8d30);

  /* original re-reads object_globals per use (no cached pointer). */
  csmemset(object_globals->combined_pvs, 0, 0x40);
  csmemset(object_globals->combined_pvs_local, 0, 0x40);

  object_globals->pvs_activator_type = 0;
  object_globals->object_marker_initialized = 0;

  *(uint32_t *)0x5a8d28 = 0;

  object_globals->unk_8.value = 0xffffffff;
  object_globals->unk_4 = 0;
  object_globals->last_garbage_collection_tick = 0;
}

/*
 * objects_dispose_from_old_map — per-map teardown of the object subsystem.
 *
 * Called when unloading a map (0x000a70a5 → this function). Counterpart to
 * objects_initialize_for_new_map (0x13f950). Distinct from objects_dispose
 * (0x13fac0), which is the one-time full teardown.
 *
 * Call order (confirmed from disasm):
 *   widgets_dispose_from_old_map  — per-map dispose for type-slot array
 *   widgets_dispose_from_old_map  — per-map dispose for 5 object-type slots
 *   object_types_dispose_from_old_map  — per-map dispose for object type
 * definition list lights_dispose_from_old_map  — per-map dispose for BSP
 * cluster data
 *
 * Then, if the object header data table is valid (byte at data+0x24 != 0):
 *   Walk every datum via data_next_index (0x1198f0):
 *     - datum_get (0x119320) to retrieve element ptr (EBX)
 *     - if element->field_8 != NULL: memory_pool_free(objects,
 * &element->field_8)
 *     - datum_delete (0x1196d0) to remove the datum
 *     - zero element->field_8 and element->field_2
 *   After loop: data_make_invalid (0x119550) on the table
 *
 * Finally dispose collideable and noncollideable cluster partition structs:
 *   FUN_00191600(&collideable_cluster_partition)
 *   FUN_00191600(&noncollideable_cluster_partition)
 *
 * Confirmed: no arguments — caller at 0x000a70a5 uses bare CALL with no PUSH.
 *            Ghidra's __fastcall/param_1 is a misread of the PUSH ECX stack
 *            slot reservation in the function prologue.
 * Confirmed: MOV CL, byte ptr [EAX+0x24] / TEST CL,CL — byte guard on data
 *            valid flag (data_t.valid) before the loop.
 * Confirmed: ADD ESP,0x8 after datum_get and data_next_index calls (2 cdecl
 *            args each); ADD ESP,0x10 at loop-end cleans datum_delete (0x8) +
 *            data_next_index advance call (0x8) together.
 * Confirmed: ADD ESP,0x8 after memory_pool_free (2 cdecl args).
 * Confirmed: ADD ESP,0x4 after data_make_invalid (1 cdecl arg).
 * Confirmed: ADD ESP,0x8 cleans the two FUN_191600 calls at the end.
 * Confirmed: MOV dword ptr [EBP-4], EAX saves data ptr; reloaded at
 *            0x13fa5d for datum_delete after the conditional pool-free.
 * Confirmed: LEA EDI,[EBX+8] — EDI = &element->field_8 — passed as
 *            arg2 to memory_pool_free; also used to zero field_8 at 0x13fa67.
 */
void objects_dispose_from_old_map(void)
{
  data_t *obj_data;

  damage_dispose_from_old_map();
  widgets_dispose_from_old_map();
  object_types_dispose_from_old_map();
  lights_dispose_from_old_map();

  obj_data = *(data_t **)0x5a8d50;

  /* Only walk the table if it has been made valid */
  if (obj_data->valid) {
    int idx = data_next_index(obj_data, -1);
    while (idx != -1) {
      object_header_data_t *header;
      data_t *data = *(data_t **)0x5a8d50;
      header = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, idx);

      if (header->object != 0) {
        /* Free this object's memory pool allocation */
        memory_pool_block_free(*(void **)0x46f080, (void **)&header->object);
      }

      datum_delete(data, idx);

      /* Zero out the object pointer and flags unconditionally after delete */
      header->object = 0;
      header->unk_2 = 0;

      idx = data_next_index(*(data_t **)0x5a8d50, idx);
    }
    data_make_invalid(*(data_t **)0x5a8d50);
  }

  /* Dispose cluster partition sub-tables */
  cluster_partition_dispose((void *)0x5a8d40);
  cluster_partition_dispose((void *)0x5a8d30);
}

/*
 * objects_dispose — tear down all object subsystems.
 *
 * Call order (confirmed from disasm):
 *   widgets_dispose  — iterates 5 type slots, calls dispose vtable entry at
 * [EDI] object_types_dispose  — walks linked list, calls each type's dispose at
 * +0x14 lights_dispose  — disposes the BSP cluster data, calls FUN_191630
 *
 * Then:
 *   if (!game_in_editor()):  null out *(data_t**)0x5a8d50 (don't free)
 *   else:                    data_dispose(*(data_t**)0x5a8d50)
 *
 *   if (objects != NULL):    objects = NULL  (pool not freed here)
 *
 *   FUN_00191630(&collideable_cluster_partition)   — zero 3 ptr fields
 *   FUN_00191630(&noncollideable_cluster_partition)
 *
 * Confirmed: JNZ selects data_dispose path; JZ / JNZ gates objects null.
 * Confirmed: ADD ESP,0x4 after data_dispose (1 cdecl arg).
 * Confirmed: ADD ESP,0x8 cleans 2 FUN_191630 calls at end.
 */
void objects_dispose(void)
{
  widgets_dispose();
  object_types_dispose();
  lights_dispose();

  if (!game_in_editor()) {
    /* Not in editor: just null the pointer, do not free */
    /* object header table pointer */
    if (object_header_data != 0) {
      object_header_data = 0;
    }
  } else {
    data_t *obj_data = object_header_data;
    data_dispose(obj_data);
  }

  if (objects != 0) {
    objects = 0;
  }

  /* Zero out cluster partition structs (3 data_t* fields each) */
  cluster_partition_null_references(collideable_object_cluster_partition);
  cluster_partition_null_references(noncollideable_object_cluster_partition);
}

/*
 * object_activate — mark a root object as "outdoor"/visible if it passes
 * the activation conditions: not already active (bit 0x01), not flagged
 * 0x100000, and has no parent.
 *
 * Confirmed: CALL 0x119320 (datum_get) + CALL 0x13d680
 * (object_get_and_verify_type). Confirmed: tests header->unk_2 bit 0x01,
 * obj->flags bit 0x100000, obj->parent_object_index == -1. Confirmed: sets
 * header->unk_2 |= 0x01 on success.
 */
void object_activate(int object_handle)
{
  object_header_data_t *hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  if ((hdr->unk_2 & 0x01) == 0 && (obj->flags & 0x100000) == 0 &&
      obj->parent_object_index.value == -1) {
    hdr->unk_2 |= 0x01;
  }
}

/*
 * object_deactivate — clear the "active" flag (bit 0x01) from an object's
 * header unk_2 byte, if currently set.
 *
 * Inverse of object_activate: deactivates the object by clearing bit 0x01.
 *
 * Confirmed: CALL 0x119320 (datum_get), CALL 0x13d680
 *   (object_get_and_verify_type) with type_mask=-1.
 * Confirmed: TEST AL,0x1; JZ skip; AND AL,0xFE; MOV [ESI+2],AL.
 * Confirmed: ADD ESP,0x10 cleans datum_get + object_get_and_verify_type.
 */
void object_deactivate(int object_handle)
{
  object_header_data_t *hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  (void)obj; /* return value unused but call required for verification */
  if ((hdr->unk_2 & 0x01) != 0) {
    hdr->unk_2 &= ~0x01;
  }
}

/*
 * object_reset — reset an object to its default world position.
 *
 * Copies the 3-float default-position vector from *(float**)0x31fc38 into
 * both the object's position (+0x18..+0x20) and forward/up (+0x3c..+0x44),
 * clears the "at-rest" flag (bit 5 of object[+4]), then calls
 * object_type_reset to perform a final physics/placement update.
 *
 * Confirmed: single call to object_get_and_verify_type(handle, -1), then
 * two identical 3-dword copies from [0x31fc38]. The 3 cdecl args
 * (−1, handle, handle) are batch-cleaned by ADD ESP,0xc after
 * object_type_reset.
 *
 * 0x13fbc0 / objects.obj
 */
void object_reset(int object_handle)
{
  object_data_t *obj;
  const vector3_t *up;

  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  up = (const vector3_t *)0x31fc38;
  obj->translational_velocity = *up;
  obj->angular_velocity = *up;
  obj->flags &= ~0x20u;
  object_type_reset(object_handle);
}

/*
 * object_placement_data_new — initialise an object placement data struct.
 *
 * Zeroes the 0x88-byte placement buffer, stores the tag index at +0x00,
 * copies default forward {1,0,0} and up {0,0,1} vectors from constant
 * tables, then resolves the parent handle through the object header table:
 *   - If parent is valid: copies parent_object_index (+0x70), cluster
 *     index (+0x68), and the raw handle into the placement.
 *   - Otherwise: sets parent/cluster fields to -1/0xFFFF.
 * Finally fills four scale vectors at +0x58 with {1,1,1} each.
 *
 * Confirmed: csmemset(param_1, 0, 0x88) — 136-byte struct.
 * Confirmed: *(void**)0x31fc3c → {1.0, 0.0, 0.0} default forward.
 * Confirmed: *(void**)0x31fc44 → {0.0, 0.0, 1.0} default up.
 * Confirmed: *(void**)0x2ee708 → {1.0, 1.0, 1.0} default scale.
 * Confirmed: datum_absolute_index_to_index(DAT_005a8d50, parent_handle)
 *            returns header ptr; +0x3 = type, +0x8 = object ptr.
 * Confirmed: word at [ESI+0x16] = 0.
 * Confirmed: 4 iterations of 12-byte copy for scale at [ESI+0x58].
 */
void object_placement_data_new(void *placement, int tag_index,
                               int parent_handle)
{
  char *p = (char *)placement;
  float *src;
  int header;
  int obj;
  int i;

  csmemset(placement, 0, 0x88);

  /* +0x00: tag index */
  *(int *)(p + 0x00) = tag_index;
  /* +0x04: flags = 0 (already zeroed) */
  *(int *)(p + 0x04) = 0;

  /* +0x34: default forward vector {1,0,0} from *(void**)0x31fc3c */
  src = *(float **)0x31fc3c;
  *(float *)(p + 0x34) = src[0];
  *(float *)(p + 0x38) = src[1];
  *(float *)(p + 0x3c) = src[2];

  /* +0x40: default up vector {0,0,1} from *(void**)0x31fc44 */
  src = *(float **)0x31fc44;
  *(float *)(p + 0x40) = src[0];
  *(float *)(p + 0x44) = src[1];
  *(float *)(p + 0x48) = src[2];

  /* +0x16: zero (int16) */
  *(int16_t *)(p + 0x16) = 0;

  /* Resolve parent: datum_absolute_index_to_index returns header or 0 */
  header = datum_absolute_index_to_index(*(data_t **)0x5a8d50, parent_handle);
  if (header == 0 || (1 << (*(uint8_t *)(header + 0x3) & 0x1f)) == 0 ||
      *(int *)(header + 0x8) == 0) {
    /* No valid parent */
    *(int *)(p + 0x0c) = -1;
    *(int *)(p + 0x08) = -1;
    *(int16_t *)(p + 0x14) = -1;
  } else {
    obj = *(int *)(header + 0x8);
    *(int *)(p + 0x0c) = parent_handle;
    *(int *)(p + 0x08) = *(int *)(obj + 0x70);
    *(int16_t *)(p + 0x14) = *(int16_t *)(obj + 0x68);
  }

  /* +0x58: four {1,1,1} scale vectors from *(void**)0x2ee708 */
  {
    char *dst = p + 0x58;
    for (i = 4; i != 0; i--) {
      src = *(float **)0x2ee708;
      *(float *)(dst + 0x0) = src[0];
      *(float *)(dst + 0x4) = src[1];
      *(float *)(dst + 0x8) = src[2];
      dst += 0xc;
    }
  }
}

/*
 * object_disconnect_from_map — remove an object from the BSP cluster
 * partition and its parent's child chain, then clear the
 * _object_connected_to_map_bit (0x800) flag.
 *
 * If the object has a parent (parent_object_index != NONE), it unlinks
 * itself from the parent's child-object linked list starting at
 * parent_obj+0xC8 (via the list-remove helper at 0x13e510, which walks
 * next_object_index links at obj+0xC4).
 *
 * If the object has no parent, it removes itself from the appropriate
 * cluster partition (0x5a8d40 for objects with flag 0x2000000 set,
 * 0x5a8d30 otherwise) via the partition-remove call at 0x1919a0. It
 * then optionally clears the "outdoor" bit (header byte+2, bit 0x1)
 * if the header's bit 0x40 flag is set.
 *
 * Confirmed: single cdecl arg (object_handle).
 * Confirmed: assert strings reference objects.c lines 0x3bd and 0x3be.
 * Confirmed: 0x13e510 reads EAX (ptr to first_child_ref) and EBX
 *   (object_handle) as register args with 0 stack args.
 * Confirmed: 0x1919a0 is cdecl with 3 stack args (partition, handle, ptr).
 */
void object_disconnect_from_map(int object_handle)
{
  object_header_data_t *header;
  object_data_t *obj;

  header =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  obj = header->object;

  /* assert: identifier portion of handle must be nonzero */
  assert_halt_at(__FILE__, 9962, object_handle & 0xffff0000);

  /* assert: object must be connected to map */
  assert_halt_at(__FILE__, 9965, obj->flags & 0x800);

  if (obj->parent_object_index.value != NONE) {
    /* Object has a parent: unlink from parent's child chain.
     * Get the parent's object data, then call the list-remove helper
     * at 0x13e510 with EAX = &parent_obj->unk_200 (child list head)
     * and EBX = object_handle to unlink. */
    object_data_t *parent_obj = (object_data_t *)object_get_and_verify_type(
      obj->parent_object_index.value, -1);
    object_child_list_remove((void *)((char *)parent_obj + 0xc8),
                             object_handle);
  } else {
    /* No parent: remove from cluster partition. */
    object_data_t *self_obj =
      (object_data_t *)object_get_and_verify_type(object_handle, -1);
    void *partition;
    partition = (void *)0x5a8d40;
    if (self_obj->flags & 0x2000000) {
    } else
      partition = (void *)0x5a8d30;

    cluster_partition_remove_object(partition, object_handle,
                                    (void *)((char *)obj + 0xbc));

    /* If header bit 0x40 is set, re-fetch header and clear bit 0x1 */
    if (header->unk_2 & 0x40) {
      object_header_data_t *header2 =
        (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
      object_get_and_verify_type(object_handle, -1);
      if (header2->unk_2 & 0x1) {
        header2->unk_2 &= ~0x1;
      }
    }
  }

  /* Clear _object_connected_to_map_bit (0x800) in object flags */
  obj->flags &= ~(uint32_t)0x800;

  /* Clear bit 0x20 in header flags byte */
  header->unk_2 &= ~0x20;
}

/*
 * object_get_first_cluster (0x13fe10 / objects.obj) — begin iterating the
 * cluster set that an object belongs to; returns the first cluster's marker (or
 * NONE).
 *
 * Resolves the object's root parent (object_get_root_parent), then selects the
 * cluster-partition table based on the root object's flags: table 0x5a8d40 when
 * flag bit 0x2000000 is set, otherwise 0x5a8d30. Stores the table pointer in
 * iter_state[0] and initializes the cluster iterator via FUN_00191690, seeding
 * it with the root object's cluster reference (root_object+0xbc) and writing
 * the iterator state into iter_state[1]. Returns FUN_00191690's first cluster
 * marker.
 *
 * Read-only with respect to object lifecycle: writes only the caller's 8-byte
 * iter_state buffer ([0] table ptr, [4] cluster iterator state). Paired with
 * object_get_next_cluster (cluster-next) on the same iter_state.
 *
 * Confirmed: 2 cdecl args (iter_state @ [EBP+0x8] ESI, object_handle @
 * [EBP+0xc]). Confirmed: object_get_root_parent(object_handle) result reused
 * for both object_get_and_verify_type(root, -1) calls (flags read +0x4, cluster
 * ref +0xbc). Confirmed: returns FUN_00191690's EAX (first cluster marker,
 * int16_t in callers).
 */
int16_t object_get_first_cluster(void *iter_state, int object_handle)
{
  unsigned int **iter = (unsigned int **)iter_state;
  int root;
  int root_obj;
  unsigned int *table;

  root = object_get_root_parent(object_handle);
  root_obj = (int)object_get_and_verify_type(root, -1);
  table = (unsigned int *)0x5a8d40;
  if ((*(unsigned int *)(root_obj + 4) & 0x2000000) == 0) {
    table = (unsigned int *)0x5a8d30;
  }
  iter[0] = table;
  root_obj = (int)object_get_and_verify_type(root, -1);
  return (int16_t)FUN_00191690(iter[0], (int *)(iter + 1),
                               *(int *)(root_obj + 0xbc));
}

/* Get the node matrices reference block for an object.
 * Returns the header block reference at offset 0x1a0 from the object header.
 * 0x13fe70 / objects.obj
 */
void *object_get_node_matrices(int object_handle)
{
  void *obj = object_get_and_verify_type(object_handle, 0xffffffff);
  return object_header_block_reference_get(object_handle, (char *)obj + 0x1a0);
}

/*
 * object_get_attachment_marker_name — get a marker definition from the
 * object's child model tag.
 *
 * Resolves the object's tag data via tag_get('obje', obj->tag_index),
 * then checks if marker_index is in range [0, block_count at tag+0x140).
 * If valid, returns tag_block_get_element(tag+0x140, marker_index, 0x48) +
 * 0x10. Returns NULL if index is out of range or negative.
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type), with -1 mask.
 * Confirmed: CALL 0x1ba140 (tag_get) with 'obje' (0x6f626a65) group tag.
 * Confirmed: CALL 0x19b210 (tag_block_get_element) with block at tag+0x140,
 *            element size 0x48, returns pointer + 0x10.
 * Confirmed: CMP CX,0 / CMP ECX,EDX — signed short check against block count.
 */
__declspec(noinline) void *object_get_attachment_marker_name(int object_handle,
                                         int16_t marker_index)
{
  uint32_t *obj = (uint32_t *)object_get_and_verify_type(object_handle, -1);
  int tag = (int)tag_get(0x6f626a65, (int)*obj);

  if (marker_index >= 0 && marker_index < *(int *)(tag + 0x140)) {
    return (char *)tag_block_get_element((void *)(tag + 0x140), marker_index,
                                         0x48) +
           0x10;
  }
  return NULL;
}

/*
 * object_has_node — check whether a given node index is valid for an object.
 *
 * Returns true if the object's model tag ('mode') has a nodes block and
 * node_index falls within [0, node_count). If the object tag definition has
 * no model reference (tag+0x34 == -1), returns true only when node_index == 0
 * (the implicit root node).
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type), 2 stack args.
 * Confirmed: CALL 0x1ba140 (tag_get) twice — first with 'obje', then 'mode'.
 * Confirmed: CMP word ptr [EBP+0xc],0x0 — node_index is int16_t.
 * Confirmed: model node count at offset 0xb8 in model tag data.
 * Confirmed: XOR BL,BL — false default; MOV AL,0x1 for true paths.
 */
bool object_has_node(int object_handle, int16_t node_index)
{
  register char result;
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* Look up the object's tag definition ('obje') */
  void *obje_tag = tag_get(0x6f626a65, (int)obj->tag_index);
  int model_tag_index = *(int *)((char *)obje_tag + 0x34);

  result = 0;
  if (model_tag_index == -1) {
    /* No model — only node 0 (implicit root) is valid */
    if (node_index == 0)
      return true;
  } else {
    /* Look up the model tag ('mode') and check node count at offset 0xb8 */
    void *mode_tag = tag_get(0x6d6f6465, model_tag_index);
    if (node_index >= 0 && (int)node_index < *(int *)((char *)mode_tag + 0xb8))
      result = 1;
  }

  return result;
}

/*
 * object_set_automatic_deactivation — set or clear the "hidden" flag (bit 0x40)
 * on an object's header unk_2 byte, and optionally activate or deactivate the
 * object.
 *
 * When param_2 != 0 (hide):
 *   Sets bit 0x40 on hdr->unk_2. If the object has no parent
 *   (parent_object_index == -1) AND unk_76.index == -1, calls
 *   object_deactivate to deactivate (clear bit 0x01).
 *
 * When param_2 == 0 (unhide):
 *   Clears bit 0x40 from hdr->unk_2. If bit 0x01 is not set (i.e. the
 *   object is not currently active), calls object_activate.
 *
 * Confirmed: CALL 0x119320 (datum_get), CALL 0x13d680
 *   (object_get_and_verify_type) with type_mask=-1.
 * Confirmed: OR byte [ESI+2],0x40 in true branch; AND AL,0xBF in false.
 * Confirmed: CMP dword [EAX+0xCC],-1 (parent_object_index.value).
 * Confirmed: CMP word [EAX+0x4C],-1 (unk_76.index, 16-bit compare).
 * Confirmed: CALL 0x13fb80 (deactivate) and CALL 0x13fb30 (activate).
 * Confirmed: ADD ESP,0x10 cleans datum_get + object_get_and_verify_type.
 */
void object_set_automatic_deactivation(int object_handle, char param_2)
{
  object_header_data_t *hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  if (param_2 != 0) {
    hdr->unk_2 |= 0x40;
    if (obj->parent_object_index.value == -1 && obj->unk_76.index == -1) {
      object_deactivate(object_handle);
    }
  } else {
    uint8_t val = hdr->unk_2 & ~0x40;
    hdr->unk_2 = val;
    if ((val & 0x01) == 0) {
      object_activate(object_handle);
    }
  }
}

/*
 * object_set_garbage — set or clear the "garbage" activation state for
 * an object and its attached children.
 *
 * param flag: 0 = mark object as garbage (deactivate); non-zero = unmark.
 *
 * Reads the object's tag definition via tag_get to check whether the tag
 * has a children block (tag[0x34] != -1). If it does, and the object's
 * bit 0 of obj->flags (active/inactive state) is out of sync with the
 * requested flag, calls object_propagate_flag_to_children (via EAX register
 * arg) to propagate the state change to child objects before committing the
 * datum update.
 *
 * object_propagate_flag_to_children (0x13ee60) takes (int object_handle @EAX,
 * char param_1, char param_2) — 2 stack args, object_handle in EAX register.
 * Uses args-array inline asm pattern to avoid EAX aliasing.
 *
 * Final datum update:
 *   flag==0: set obj->flags bit 0; clear datum byte[2] bit 1 (0x02).
 *   flag!=0: clear obj->flags bit 0; set datum byte[2] bit 1 (0x02).
 *
 * Confirmed: MOV EAX,EDI before CALL 0x13ee60 — EAX = object_handle.
 * Confirmed: ADD ESP,0x8 after CALL 0x13ee60 — 2 stack args.
 * Confirmed: ADD ESP,0x10 after tag_get (cleans 4 args: 2 for tag_get +
 *   2 pre-pushed for object_get_and_verify_type).
 * Confirmed: OR dword [ESI+4],1 — obj->flags |= 1 for flag==0 path.
 * Confirmed: AND byte [EAX+2],0xfd — hdr->unk_2 &= ~2 for flag==0 path.
 * Confirmed: AND dword [ESI+4],~1 — obj->flags &= ~1 for flag!=0 path.
 * Confirmed: OR byte [EAX+2],2 — hdr->unk_2 |= 2 for flag!=0 path.
 * Confirmed: DAT_005a8d50 as datum_get first arg.
 * Confirmed: CMP dword ptr [EBX+0x34],-1 — children block presence check.
 */
void object_set_garbage(int object_handle, int flag)
{
  /* ESI = object_data_t*, EBX = tag_def, EAX = flags_bit / hdr. Fields are
   * re-read inline (not cached in a bool) to mirror the original's repeated
   * CMP [EBX+0x34],-1 and its reuse of (flags & 1) in EAX. */
  object_data_t *obj;
  object_header_data_t *hdr;
  void *tag_def;

  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  tag_def = tag_get(0x6f626a65, (int)obj->tag_index);

  /* Children block present at tag+0x34 (!= -1)? */
  if (*(int *)((char *)tag_def + 0x34) != -1) {
    if ((obj->flags & 1) != 0 && (char)flag != 0)
      /* bit0 set && flag!=0: propagate (0,1), then fall through to
       * lab_0014000a. */
      object_propagate_flag_to_children(object_handle, 0, 1);
    else
      goto lab_0014003b;
  }

lab_0014000a:
  if ((char)flag == 0)
    goto lab_00140017;
lab_00140011:
  if (*(int *)((char *)tag_def + 0x34) == -1)
    return;
  goto lab_00140017;

lab_00140017:
  /* Commit the datum-level flags. */
  hdr = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  if ((char)flag == 0) {
    /* Mark as garbage: set bit 0 of obj->flags, clear hdr->unk_2 bit 1. */
    obj->flags |= 1;
    hdr->unk_2 &= (uint8_t)~0x02;
    return;
  }
  goto lab_00140056;

lab_0014003b:
  if ((obj->flags & 1) != 0)
    goto lab_0014000a;
  if ((char)flag != 0)
    goto lab_00140011;
  /* bit0 clear && flag==0: propagate (1,0), then commit. */
  object_propagate_flag_to_children(object_handle, 1, 0);
  goto lab_00140017;

lab_00140056:
  /* Unmark garbage: clear bit 0 of obj->flags, set hdr->unk_2 bit 1. */
  obj->flags &= ~(uint32_t)1;
  hdr->unk_2 |= 0x02;
}

/* Walk the parent chain to the root object and copy its position and
 * forward vector to the output buffers (0x140070). Either output may
 * be NULL to skip copying. Position is at object offset 0x18 (3 floats),
 * forward direction at 0x3c (3 floats). */
void object_get_root_location(int object_handle, float *position_out,
                              float *direction_out)
{
  char *obj = (char *)object_get_and_verify_type(object_handle, -1);

  while (*(int *)(obj + 0xcc) != -1) {
    object_header_data_t *header = (object_header_data_t *)datum_get(
      *(data_t **)0x5a8d50, *(int *)(obj + 0xcc));
    obj = (char *)header->object;

    {
      int16_t type = *(int16_t *)(obj + 0x64);
      if ((1 << (type & 0x1f)) == 0) {
        char *msg;
        display_assert(csprintf((char *)0x5ab100,
                                "got an object type we didn't expect "
                                "(expected one of 0x%08x but got #%d).",
                                -1, (int)type),
                       "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
        system_exit(-1);
      }
    }
  }

  if (position_out != NULL) {
    position_out[0] = *(float *)(obj + 0x18);
    position_out[1] = *(float *)(obj + 0x1c);
    position_out[2] = *(float *)(obj + 0x20);
  }

  if (direction_out != NULL) {
    direction_out[0] = *(float *)(obj + 0x3c);
    direction_out[1] = *(float *)(obj + 0x40);
    direction_out[2] = *(float *)(obj + 0x44);
  }
}

/*
 * object_get_location — returns the root object's 8-byte location pair.
 *
 * Resolves the topmost parent handle via object_get_root_parent(handle),
 * verifies that object with object_get_and_verify_type(root_handle, -1), then
 * copies dwords at offsets +0x48 and +0x4c into location_out.
 *
 * Confirmed: CALL 0x13d7f0 with object_handle, then CALL 0x13d680 with
 *            returned handle and mask -1.
 * Confirmed: MOV [obj+0x48] -> [location_out+0], MOV [obj+0x4c] ->
 *            [location_out+4].
 */
void object_get_location(int object_handle, void *location_out)
{
  typedef struct {
    uint32_t a;
    uint32_t b;
  } loc_t;
  *(loc_t *)location_out =
    *(loc_t *)&((object_data_t *)object_get_and_verify_type(
                  object_get_root_parent(object_handle), -1))
       ->unk_72;
}

/*
 * object_set_region_count — update an object's interpolation region count.
 *
 * Copies the object's region node data from the "new" interpolation buffer
 * (at object+0x19c) into the "current" buffer (at object+0x198) via
 * object_header_block_reference_get, using csmemcpy with a size of
 * model_region_count * 32.
 *
 * If the requested region_count is >= (unk_134 - unk_132), it resets
 * unk_132 to 0 and sets unk_134 to the new region_count.
 *
 * Asserts that the object's type is NOT in the "cannot interpolate" mask
 * (types with bits 5-11 set: 0xFE0).
 *
 * Confirmed: 2 cdecl args (PUSH EDI + PUSH [EBP+0xc], ADD ESP via
 *            interleaved cleanup).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type, type_mask=-1).
 * Confirmed: CALL 0x1ba140 (tag_get) twice — 'obje' then 'mode'.
 * Confirmed: CALL 0x13dfc0 twice with pre-pushed args for csmemcpy.
 * Confirmed: assert string at 0x29bf80:
 *   "!TEST_FLAG(_object_mask_cannot_interpolate, object->object.type)"
 * Inferred: unk_408 / unk_412 at offsets 0x198/0x19c are interpolation
 *           buffer references (4 bytes each: {int16_t size, int16_t offset}).
 * Inferred: model tag + 0xb8 is the region count (int16_t).
 */
void object_set_region_count(int object_handle, int16_t region_count)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* Look up the object definition tag ('obje'), then the model tag ('mode')
   * to get the number of model regions. */
  void *obje_tag = tag_get(0x6f626a65, *(int *)obj);
  void *mode_tag = tag_get(0x6d6f6465, *(int *)((char *)obje_tag + 0x34));
  int16_t model_region_count = *(int16_t *)((char *)mode_tag + 0xb8);

  /* Assert that this object type can be interpolated.
   * _object_mask_cannot_interpolate = 0xFE0 (bits 5 through 11).
   * The reference reads the type byte and shifts 1<<type with no explicit
   * 0x1f mask (the SHL masks the count implicitly); match that shape. */
  if ((1 << *(uint8_t *)((char *)obj + 0x64)) & 0xfe0) {
    display_assert(
      "!TEST_FLAG(_object_mask_cannot_interpolate, object->object.type)",
      "c:\\halo\\SOURCE\\objects\\objects.c", 0x5f3, 1);
    system_exit(-1);
  }

  /* Copy region node data from the "new" buffer to the "current" buffer.
   * The two references at obj+0x19c and obj+0x198 each describe a
   * {size, offset} pair into the object's dynamic data region.
   * Written inline so MSVC evaluates the args right-to-left and pushes the
   * "src" pointer immediately after its call (matching the reference) instead
   * of parking it in EBX -- that keeps EBX free for model_region_count. */
  csmemcpy(
    object_header_block_reference_get(object_handle, (char *)obj + 0x198),
    object_header_block_reference_get(object_handle, (char *)obj + 0x19c),
    (int)model_region_count << 5);

  /* If the new region_count is large enough, reset unk_132 and store it. */
  if ((int)region_count >= (int)obj->unk_134 - (int)obj->unk_132) {
    obj->unk_132 = 0;
    obj->unk_134 = region_count;
  }
}

/*
 * object_adjust_interpolation_position — adds a delta vector to an object's
 * interpolation position within its node/region block.
 *
 * Validates that the object type is not in the "cannot interpolate" mask
 * (bits 5-11 = 0xFE0). If the object's unk_134 (int16_t at offset 0x86)
 * is nonzero, resolves the block reference at obj+0x198 and adds the delta
 * to the position vector at offsets +0x10, +0x14, +0x18 in that block.
 *
 * Confirmed: cdecl, 2 stack args (PUSH+PUSH pattern, ADD ESP cleanup).
 * Confirmed: CALL 0x0013d680 — object_get_and_verify_type(handle, -1).
 * Confirmed: TEST EAX,0xfe0 — same _object_mask_cannot_interpolate check as
 *   object_set_region_count.
 * Confirmed: CALL 0x0008d9f0 — display_assert with line 0x60a (1546).
 * Confirmed: CALL 0x0008e2f0 — system_exit(-1).
 * Confirmed: CMP word ptr [ESI+0x86],0x0 — checks unk_134 != 0.
 * Confirmed: ADD ESI,0x198 then PUSH ESI — block reference at obj+0x198.
 * Confirmed: CALL 0x0013dfc0 — object_header_block_reference_get.
 * Confirmed: FLD/FADD/FSTP float ptr at [EAX+0x10], [EAX+0x14], [EAX+0x18].
 */
void object_adjust_interpolation_position(int object_handle, vector3_t *delta)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* Assert that this object type can be interpolated.
   * _object_mask_cannot_interpolate = 0xFE0 (bits 5 through 11). */
  if ((1 << (*(uint8_t *)((char *)obj + 0x64) & 0x1f)) & 0xfe0u) {
    display_assert(
      "!TEST_FLAG(_object_mask_cannot_interpolate, object->object.type)",
      "c:\\halo\\SOURCE\\objects\\objects.c", 0x60a, 1);
    system_exit(-1);
  }

  /* Only adjust if the object has interpolation data (unk_134 != 0). */
  if (obj->unk_134 != 0) {
    float *block = (float *)object_header_block_reference_get(
      object_handle, (char *)obj + 0x198);
    /* Add delta to the position vector at block offsets +0x10, +0x14, +0x18
     * (float indices 4, 5, 6). */
    block[4] += delta->x;
    block[5] += delta->y;
    block[6] += delta->z;
  }
}

/* Set region permutation by marker name (0x1402c0).
 * Searches model regions for a permutation whose name matches marker_name
 * (case-insensitive). If found, sets the object's region permutation index
 * at obj+0x130+region. If region_index is -1, searches all regions;
 * otherwise only the specified region. If param_4 is 0, forces the
 * permutation index to 0 regardless of the match position. */
void object_permute_region(int object_handle, const char *marker_name,
                           short region_index, char param_4)
{
  char *obj;
  char *obje_tag;
  int model_tag_index;
  char *mode_tag;
  int *regions_block;
  short region_iter;
  int region_i;
  char *region_element;
  int *permutations_block;
  short perm_iter;
  int perm_i;
  char *perm_element;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  obje_tag = (char *)tag_get(0x6f626a65, *(int *)obj);
  model_tag_index = *(int *)(obje_tag + 0x34);
  if (model_tag_index == -1)
    return;

  mode_tag = (char *)tag_get(0x6d6f6465, model_tag_index);
  regions_block = (int *)(mode_tag + 0xc4);
  /* permuter 20260721: chained zero-init before the early-out lifts VC71
   * match 87.6 -> 91.0 (register scheduling); semantically identical. */
  region_iter = (region_i = 0);
  if (*regions_block <= 0)
    return;

  do {
    if (region_index == -1 || region_index == region_iter) {
      region_element =
        (char *)tag_block_get_element(regions_block, region_i, 0x4c);
      permutations_block = (int *)(region_element + 0x40);
      perm_iter = 0;
      if (*permutations_block > 0) {
        perm_i = 0;
        do {
          perm_element =
            (char *)tag_block_get_element(permutations_block, perm_i, 0x58);
          if (crt_stricmp(perm_element, marker_name) == 0) {
            *(char *)(obj + 0x130 + region_i) =
              param_4 ? (char)perm_iter : (char)0;
            break;
          }
          perm_iter = perm_iter + 1;
          perm_i = (int)perm_iter;
        } while (perm_i < *permutations_block);
      }
    }
    region_iter = region_iter + 1;
    region_i = (int)region_iter;
  } while (region_i < *regions_block);
}

/* Query an outgoing object function value (0x1403a0).
 * If function_index is -1, writes 1.0f and returns true.
 * Otherwise asserts index is in [0,4), writes the float at
 * object+0xe4+index*4 to out_value, and returns whether the
 * corresponding bit in the function-valid mask at object+0xd3 is set. */
bool object_get_function_value(int object_handle, short function_index,
                               void *out_value)
{
  char *obj;
  bool result; /* name: PAL 2342 source/objects/objects.c:1474 */

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  if (function_index == -1) {
    *(int *)out_value = 0x3f800000;
    result = true;
  } else {
    if (function_index < 0 || function_index >= 4) {
      display_assert(
        "function_index>=0 && function_index<NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS",
        "c:\\halo\\SOURCE\\objects\\objects.c", 0x676, 1);
      system_exit(-1);
    }
    *(int *)out_value = *(int *)(obj + 0xe4 + function_index * 4);
    result = (*(unsigned char *)(obj + 0xd3) & (1 << function_index)) != 0;
  }

  return result;
}

/*
 * object_find_in_cluster — find objects in clusters matching type criteria.
 *
 * Begins a marker pass (object_reset_markers), then iterates over
 * cluster indices.  For each cluster, walks the collideable partition
 * (flags & 1 => 0x5a8d40) and/or noncollideable partition (flags & 2 =>
 * 0x5a8d30) using cluster_partition_iter_first/next (0x191a50/0x191660).
 *
 * Each found object is verified as having a valid type bit.  If the object's
 * marker_generation differs from the current global generation, it's stamped
 * with the new generation and added to the output array.  Objects whose
 * marker_generation already matches are skipped (already collected this pass).
 *
 * Returns when max_count objects are collected or all clusters are exhausted.
 * Ends the marker pass by clearing object_globals->marker_initialized.
 *
 * Parameters (cdecl, 5 args):
 *   flags            — bit 0: collideable, bit 1: noncollideable; 0=>all (-1)
 *   cluster_count    — number of cluster indices
 *   cluster_indices  — int16_t array of cluster indices
 *   max_count        — capacity of out_handles array (int16_t)
 *   out_handles      — output array for found object handles (int*)
 *
 * Confirmed: 5 cdecl params at [EBP+0x8..0x18].
 * Confirmed: CALL 0x13eb70 (object_reset_markers) with 0 pushed args.
 * Confirmed: flags==0 => overwritten with 0xffffffff.
 * Confirmed: cluster_partition_iter_first at 0x191a50 (3 cdecl args:
 *            partition, state, cluster_idx).
 * Confirmed: cluster_partition_iter_next at 0x191660 (2 cdecl args:
 *            partition, state).
 * Confirmed: Both iter functions return handle (int) or -1.
 * Confirmed: datum_get at 0x119320: result+8 = object_data_t*.
 * Confirmed: obj+0x64 is type (int16_t), obj+0x08 is marker_generation
 * (uint32_t). Confirmed: 0x5a8d28 is the global marker generation counter.
 * Confirmed: End-of-pass clears object_globals+0x01 (marker_initialized).
 * Confirmed: Returns uint16 count in AX.
 */
int16_t object_find_in_cluster(int flags, int16_t cluster_count,
                               int16_t *cluster_indices, int16_t max_count,
                               int *out_handles)
{
  int16_t found = 0;
  int16_t i;

  if (flags == 0)
    flags = 0xFFFFFFFF;

  object_reset_markers();

  for (i = 0; i < cluster_count; i++) {
    int16_t cluster_idx = cluster_indices[i];
    int handle;
    int iter_state[2];

    /* Collideable partition (flags & 1) */
    if (flags & 1) {
      handle =
        cluster_partition_iter_first((void *)0x5a8d40, iter_state, cluster_idx);
      while (handle != -1) {
        object_header_data_t *header =
          (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle);
        object_data_t *obj = header->object;
        uint32_t generation;

        /* Ref: `movswl 0x64(%edi),%ecx; shll %cl,%edx` — the native int16_t
         * `type` feeds SHL directly.  An earlier `(uint8_t)type & 0x1f` form
         * was kept because it scored 91.0 vs 87.3, but that 87.3 was an
         * anchor collapse (dp_lcs 95.5); re-anchoring via the int16_t loop
         * index took the pair to 96.8.  Do not reintroduce the byte load. */
        if ((1 << obj->type) == 0) {
          char *msg =
            csprintf((char *)0x5ab100,
                     "got an object type we didn't expect (expected one of 0x%08x but got #%d).",
                     -1, (int)obj->type);
          display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
          system_exit(-1);
        }

        if (!object_globals->object_marker_initialized) {
          display_assert("object_globals->object_marker_initialized",
                         "c:\\halo\\SOURCE\\objects\\objects.c", 0xdd7, 1);
          system_exit(-1);
        }

        generation = *(uint32_t *)0x5a8d28;
        if (obj->marker_generation != generation) {
          obj->marker_generation = generation;
          if (found >= max_count) {
            if (!object_globals->object_marker_initialized) {
              display_assert("object_globals->object_marker_initialized",
                             "c:\\halo\\SOURCE\\objects\\objects.c", 0xdba, 1);
              system_exit(-1);
            }
            object_globals->object_marker_initialized = 0;
            return found;
          }
          out_handles[found] = handle;
          found++;
        }
        handle = cluster_partition_iter_next((void *)0x5a8d40, iter_state);
      }
    }

    /* Noncollideable partition (flags & 2) */
    if (flags & 2) {
      int iter_state2[2];
      handle = cluster_partition_iter_first((void *)0x5a8d30, iter_state2,
                                            cluster_idx);
      while (handle != -1) {
        object_header_data_t *header =
          (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle);
        object_data_t *obj = header->object;
        uint32_t generation;

        /* Ref: `movswl 0x64(%edi),%ecx; shll %cl,%edx` — the native int16_t
         * `type` feeds SHL directly.  An earlier `(uint8_t)type & 0x1f` form
         * was kept because it scored 91.0 vs 87.3, but that 87.3 was an
         * anchor collapse (dp_lcs 95.5); re-anchoring via the int16_t loop
         * index took the pair to 96.8.  Do not reintroduce the byte load. */
        if ((1 << obj->type) == 0) {
          char *msg =
            csprintf((char *)0x5ab100,
                     "got an object type we didn't expect (expected one of 0x%08x but got #%d).",
                     -1, (int)obj->type);
          display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
          system_exit(-1);
        }

        if (!object_globals->object_marker_initialized) {
          display_assert("object_globals->object_marker_initialized",
                         "c:\\halo\\SOURCE\\objects\\objects.c", 0xdd7, 1);
          system_exit(-1);
        }

        generation = *(uint32_t *)0x5a8d28;
        if (obj->marker_generation != generation) {
          obj->marker_generation = generation;
          if (found >= max_count) {
            if (!object_globals->object_marker_initialized) {
              display_assert("object_globals->object_marker_initialized",
                             "c:\\halo\\SOURCE\\objects\\objects.c", 0xdba, 1);
              system_exit(-1);
            }
            object_globals->object_marker_initialized = 0;
            return found;
          }
          out_handles[found] = handle;
          found++;
        }
        handle = cluster_partition_iter_next((void *)0x5a8d30, iter_state2);
      }
    }
  }

  if (!object_globals->object_marker_initialized) {
    display_assert("object_globals->object_marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0xdba, 1);
    system_exit(-1);
  }
  object_globals->object_marker_initialized = 0;
  return found;
}

/*
 * object_name_list_get_handle — look up an object handle by name-table index.
 *
 * Takes a 16-bit name index, validates it is in [0, 0x200), and returns
 * the object handle stored at object_name_list[index]. Returns 0xFFFFFFFF
 * (-1) if the index is out of range.
 *
 * Confirmed: range check [0, 0x200) via TEST AX,AX / CMP AX,0x200.
 * Confirmed: MOV ECX,[0x46f07c] — loads object_name_list pointer.
 * Confirmed: MOVSX EAX,AX — sign-extends index before array access.
 * Confirmed: MOV EAX,[ECX+EAX*4] — returns name_table[index].
 * Confirmed: OR EAX,0xFFFFFFFF on out-of-range — returns -1.
 */
int object_name_list_get_handle(int16_t index)
{
  if (index >= 0 && index < 0x200) {
    int *name_table = *(int **)0x46f07c;
    return name_table[(int)index];
  }
  return 0xffffffff;
}

/*
 * objects_disconnect_from_structure_bsp (0x140750 / objects.obj) — disconnect
 * every map-connected, childless object from the map.
 *
 * Walks all objects via an inlined object iterator (type_mask = -1, flags = 0;
 * the binary inlines object_iterator_new's five field stores rather than
 * calling it).  For each object that is connected to the map (flags bit 0x800)
 * and has no parent (object+0xcc == NONE), it calls object_disconnect_from_map
 * on the iterator's last_handle and re-asserts the 0x800 flag (a redundant
 * store the original preserves).  object_type_disconnect_from_structure_bsp
 * (vtable +0x50 dispatch) is then invoked for every iterated object,
 * unconditionally.
 *
 * Confirmed (disasm 0x140750): data_verify(*(data_t**)0x5a8d50) first; iterator
 * inlined at EBP-0x10 with EAX=-1 written to type_mask(+0)/last_handle(+8),
 * byte flags(+4)=0, word current_index(+6)=0, cookie(+0xc)=0x86868686; object
 * pointer returned in EAX (ESI); EDI reloaded from [EBP-8]=last_handle each
 * iteration; TEST [ESI+4],0x800 then CMP [ESI+0xcc],-1; both callees take EDI.
 */
void objects_disconnect_from_structure_bsp(void)
{
  object_iter_t it;
  object_data_t *obj;

  data_verify(*(data_t **)0x5a8d50);

  it.cookie = 0x86868686;
  it.type_mask = -1;
  it.flags = 0;
  it.current_index = 0;
  it.last_handle = -1;

  obj = (object_data_t *)object_iterator_next(&it);
  while (obj != (object_data_t *)0) {
    if ((*(unsigned int *)((char *)obj + 4) & 0x800) != 0 &&
        *(int *)((char *)obj + 0xcc) == -1) {
      object_disconnect_from_map(it.last_handle);
      *(unsigned int *)((char *)obj + 4) |= 0x800;
    }
    object_type_disconnect_from_structure_bsp(it.last_handle);
    obj = (object_data_t *)object_iterator_next(&it);
  }
}

/*
 * object_visible_to_any_player — check if an object is visible to any player.
 *
 * Returns true (1) if the object occupies a PVS-visible cluster AND is within
 * at least one player's field of view (or within the object's bounding sphere
 * distance from the player's head).
 *
 * The algorithm:
 *   1. Validate the object header flag (bit 0 of unk_2) and object flags
 *      (must have 0x800 set, must NOT have 0x200000 set).
 *   2. Iterate the object's clusters; for each, test visibility against the
 *      combined player PVS bitfield.
 *   3. If a visible cluster is found, iterate all players:
 *      a. Compute distance_squared from player head to object center.
 *      b. If dist_sq < radius_sq (player inside bounding sphere), visible.
 *      c. Otherwise, compute the half-angle subtended by the bounding sphere
 *         plus a PI/4 margin, and check if the object direction lies within
 *         the player's facing cone (dot product vs cos of half-angle).
 *
 * Confirmed: cdecl, 1 arg (object_handle at [EBP+8]).
 * Confirmed: Returns byte in AL (0 or 1).
 * Confirmed: CALL 0x119320 (datum_get) with object data table (0x5a8d50).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with mask -1 and 3.
 * Confirmed: CALL 0xba6c0 (players_get_combined_pvs) with no args.
 * Confirmed: CALL 0x13fe10 (object_get_first_cluster) with 2 cdecl args.
 * Confirmed: CALL 0x13d5f0 (object_get_next_cluster) with 2 cdecl args.
 * Confirmed: CALL 0x1198f0 (data_next_index) with player_data, prev_index.
 * Confirmed: CALL 0x1a9200 (unit_get_head_position) with unit_handle, &out.
 * Confirmed: CALL 0x13010 (normalize3d) with delta vector pointer.
 * Confirmed: PVS bit test pattern: (1 << (cluster & 0x1f)) & pvs[cluster >> 5].
 * Confirmed: Float at 0x254a58 = PI/4 (0.7854f).
 * Confirmed: Player unit handle at player_datum + 0x34.
 * Confirmed: Unit forward vector at unit_obj + 0x1E0 (vector3_t unk_480).
 * Confirmed: Object position at obj + 0x50 (unk_80/unk_84/unk_88).
 * Confirmed: Object bounding radius at obj + 0x5C (unk_92).
 */
char object_visible_to_any_player(int object_handle)
{
  object_header_data_t *header;
  object_data_t *obj;
  int *pvs;
  int16_t cluster_index;
  char iter_state[16];
  int player_index;
  char *player;
  float dy_copy;
  float radius_sq;
  float head_pos[3];
  float dx, dy, dz;
  float dist_sq;
  float delta[3];
  float magnitude;
  float half_angle;
  float dot;
  double radius_d;
  char *unit_obj;
  int unit_handle;
  /* ref keeps the result bool in a stack slot (-0x1(%ebp)); volatile forces
   * the same memory-backed slot under VC71. */
  volatile char result;

  /* Validate object header and flags */
  header =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  result = 0;

  if ((header->unk_2 & 1) && (obj->flags & 0x800) && !(obj->flags & 0x200000)) {
    /* Get combined PVS and iterate object clusters */
    pvs = (int *)players_get_combined_pvs();
    cluster_index = object_get_first_cluster(iter_state, object_handle);
    if (cluster_index != (int16_t)0xFFFF) {
      while (
        !(pvs[(int)cluster_index >> 5] & (1 << ((int)cluster_index & 0x1f)))) {
        cluster_index = object_get_next_cluster(iter_state, object_handle);
        if (cluster_index == (int16_t)0xFFFF)
          return result;
      }

      if (cluster_index != (int16_t)0xFFFF) {
        /* Object is in a visible cluster — check per-player visibility */
        radius_sq = obj->unk_92 * obj->unk_92;

        player_index = data_next_index(*(data_t **)0x5aa6d4, -1);
        if (player_index != -1) {
          while (player_index != -1) {
            player = (char *)datum_get(*(data_t **)0x5aa6d4, player_index);
            unit_handle = *(int *)(player + 0x34);

            if (unit_handle == -1)
              goto next_player;

            /* Get player head position */
            unit_get_head_position(unit_handle, head_pos);

            /* Distance check: is player head within bounding sphere? */
            dx = obj->unk_80 - head_pos[0];
            dy = obj->unk_84 - head_pos[1];
            dz = obj->unk_88 - head_pos[2];
            dy_copy = dy;
            dist_sq = dz * dz + dy_copy * dy_copy + dx * dx;

            if (dist_sq < radius_sq) {
              result = 1;
              return result;
            }

            /* FOV check: is object within player's viewing cone? */
            unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);

            delta[0] = obj->unk_80 - head_pos[0];
            delta[1] = obj->unk_84 - head_pos[1];
            delta[2] = obj->unk_88 - head_pos[2];
            radius_d = (double)obj->unk_92;
            magnitude = normalize3d(delta);

            /* dot product of normalized delta with unit forward vector */
#if defined(_MSC_VER) && !defined(__clang__)
            if ((float)cos(atan2(radius_d, (double)magnitude) +
                           (double)REAL_QUARTER_PI_POOL) <
                delta[2] * *(float *)(unit_obj + 0x1E8) +
                  delta[1] * *(float *)(unit_obj + 0x1E4) +
                  delta[0] * *(float *)(unit_obj + 0x1E0)) {
#else
            half_angle = (float)(atan2(radius_d, (double)magnitude) +
                                 (double)REAL_QUARTER_PI_POOL);
            if (x87_fcos(half_angle) <
                delta[2] * *(float *)(unit_obj + 0x1E8) +
                  delta[1] * *(float *)(unit_obj + 0x1E4) +
                  delta[0] * *(float *)(unit_obj + 0x1E0)) {
#endif
              result = 1;
              return result;
            }

          next_player:
            player_index = data_next_index(*(data_t **)0x5aa6d4, player_index);
          }
        }
      }
    }
  }

  return result;
}

void object_pvs_activate(int param_1)
{
  int iVar1;

  iVar1 = *(int *)0x46f084;
  if (param_1 == -1) {
    *(short *)(iVar1 + 0x90) = 0;
    return;
  }
  *(short *)(iVar1 + 0x90) = 1;
  *(int *)(iVar1 + 0x94) = param_1;
}

/* 0x140a00 / objects.obj — Select random region permutations for an object
 * matching a given variant number. For each region in the model, finds
 * available permutations matching the variant, picks one randomly, and
 * stores its index in the object's region permutation array at obj+0x130.
 * Returns 1 if all regions had at least one valid permutation; 0 if any
 * region had no available permutations matching the variant.
 * object_handle in EAX (register arg).
 * Confirmed: PUSH -1; PUSH EAX; CALL object_get_and_verify_type.
 * Confirmed: tag_block at model_tag+0xc4, element size 0x4c.
 * Confirmed: calls object_find_region_permutations_available_with_variant.
 * Confirmed: if count==0, tries variant=0 as fallback.
 * Confirmed: seed_random_range(get_global_random_seed_address(), 0, count). */
char object_select_random_region_permutations_by_variant(
  int object_handle /* @<eax> */, void *model_tag, int16_t variant)
{
  char *obj;
  int16_t region_count;
  char all_ok;
  int16_t i;
  char *model = (char *)model_tag;
  int16_t avail_buf[32];

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  region_count = 0;
  all_ok = 1;
  i = 0;
  if (*(int *)(model + 0xc4) > 0) {
    /* permuter 20260721 (+1.1pp raw): loop body indexes via region_count
     * (i kept in sync) — pure register-role swap, value-identical. */
    region_count = i;
    do {
      int16_t count;
      char *region;

      region = (char *)tag_block_get_element((void *)(model + 0xc4),
                                             (int)region_count, 0x4c);
      count = object_find_region_permutations_available_with_variant(
        region, variant, avail_buf);
      if (count == 0 &&
          (variant == -1 ||
           (count = object_find_region_permutations_available_with_variant(
              region, 0, avail_buf),
            count == 0))) {
        *(unsigned char *)(obj + 0x130 + (int)region_count) = 0;
        all_ok = 0;
      } else {
        int16_t chosen;
        if (count == 1) {
          chosen = 0;
        } else {
          int *seed = get_global_random_seed_address();
          chosen = seed_random_range((unsigned int *)seed, 0, count);
        }
        *(unsigned char *)(obj + 0x130 + (int)region_count) =
          (unsigned char)*(unsigned char *)((char *)avail_buf + chosen * 2);
      }
      region_count = region_count + 1;
      i = region_count;
    } while ((int)region_count < *(int *)(model + 0xc4));
  }
  return all_ok;
}

/* 0x140ad0 / objects.obj — Choose random region permutations for an object's
 * model during spawn, honoring the object's requested variant (obj+0x6e).
 * Resolves the model tag (group 'mode') from the object tag (group 'obje'),
 * then asks object_select_random_region_permutations_by_variant to populate
 * the per-region permutation indices at obj+0x130. If the requested variant
 * is not positive, or selection by that variant fails for any region, falls
 * back to variant -1 (any), then determines an actual variant number via
 * object_determine_variant_number, records it in obj+0x6e, and (if positive)
 * re-selects permutations for that resolved variant.
 * Role: part of the object spawn-appearance setup chain in object_new.
 * object_handle in EDI (register arg).
 * Confirmed: PUSH -1; PUSH EDI; CALL object_get_and_verify_type.
 * Confirmed: tag_get('obje', obj->tag_index) then tag_get('mode', tag+0x34).
 * Confirmed: variant read as int16_t from obj+0x6e (sign-extended; <=0 path).
 * Confirmed: callees receive object_handle in EAX (MOV EAX,EDI before CALL). */
void object_choose_random_region_permutations(int object_handle /* @<edi> */)
{
  char *obj;
  int obj_tag;
  void *model_tag;
  int16_t variant;
  int16_t resolved;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  obj_tag = (int)tag_get(0x6f626a65, *(int *)obj);
  if (*(int *)(obj_tag + 0x34) == -1) {
    return;
  }

  model_tag = tag_get(0x6d6f6465, *(int *)(obj_tag + 0x34));
  variant = *(int16_t *)(obj + 0x6e);
  if (variant < 1 || object_select_random_region_permutations_by_variant(
                       object_handle, model_tag, variant) == 0) {
    object_select_random_region_permutations_by_variant(object_handle,
                                                        model_tag, -1);
    resolved = object_determine_variant_number(object_handle, model_tag);
    *(int16_t *)(obj + 0x6e) = resolved;
    if (resolved > 0) {
      object_select_random_region_permutations_by_variant(object_handle,
                                                          model_tag, resolved);
    }
  }
}

void objects_scripting_set_scale(int param_1, int param_2, int16_t param_3)
{
  int iVar1;
  unsigned char cl;

  if (param_1 != -1) {
    iVar1 = (int)object_get_and_verify_type(param_1, 0xffffffff);
    *(int *)(iVar1 + 0x60) = param_2;
    cl = *(unsigned char *)(iVar1 + 0x64);
    if ((((unsigned int)1 << cl) & 0xfe0) == 0) {
      object_set_region_count(param_1, param_3);
    }
  }
}

/*
 * object_delete_internal — recursive object deletion implementation.
 *
 * Recursively deletes an object's child chain (obj+0xC8), and optionally
 * its sibling chain (obj+0xC4) when delete_sibling is nonzero. For each
 * object:
 *   1. If the game engine is running and the object is a weapon (type==2),
 *      asserts that it is not a flag (CTF flag weapon).
 *   2. Recursively deletes children and optionally siblings.
 *   3. Sets datum header bit 0x08 (pending deletion).
 *   4. If the object's tag definition has a children block (tag+0x34 != -1)
 *      and obj->flags bit 0 is clear, propagates deletion to attached
 *      children via object_propagate_flag_to_children (EAX=handle, args 1,0).
 *   5. Sets obj->flags bit 0 (deleted/inactive).
 *   6. Clears datum header bit 0x02 (active).
 *   7. Removes the object from the name list via object_remove_from_name_list
 * (EDI=handle).
 *
 * Confirmed: cdecl, 2 stack args (PUSH+PUSH, ADD ESP,0x8 at recursive sites).
 * Confirmed: CALL 0x0013d680 — object_get_and_verify_type(handle, -1).
 * Confirmed: CALL 0x000a8e30 — game_engine_running(), no args, returns bool.
 * Confirmed: CMP word ptr [ESI+0x64],0x2 — checks object type == weapon.
 * Confirmed: CALL 0x000fb0c0 — weapon_is_flag(handle), 1 cdecl arg.
 * Confirmed: CALL 0x0008d9f0 — display_assert with line 0x33d (829).
 * Confirmed: CALL 0x0008e2f0 — system_exit(-1), NOT thunk_FUN_001029a0.
 * Confirmed: [ESI+0xC8] — child object handle for recursive delete.
 * Confirmed: [ESI+0xC4] — sibling object handle (conditional on
 * delete_sibling). Confirmed: OR AL,0x8 / MOV [EBX+0x2],AL — sets datum header
 * bit 0x08. Confirmed: MOV EAX,EDI before CALL 0x0013ee60 — EAX register arg =
 * handle. Confirmed: PUSH 0x0 / PUSH 0x1 — object_propagate_flag_to_children
 * stack args (1, 0). Confirmed: TEST byte ptr [ESI+0x4],0x1 — checks obj->flags
 * bit 0. Confirmed: OR dword ptr [ESI+0x4],0x1 — sets obj->flags bit 0.
 * Confirmed: AND CL,0xfd / MOV [EAX+0x2],CL — clears datum header bit 0x02.
 * Confirmed: CALL 0x0013eff0 — no stack args, EDI register arg = handle.
 */
void object_delete_internal(int object_handle, int delete_sibling)
{
  object_header_data_t *hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* If the game engine is running and this is a weapon, assert it's not a
   * flag (CTF flags should not be deleted this way). */
  if (game_engine_running() && obj->type == 2) {
    if (weapon_is_flag(object_handle)) {
      display_assert("!(weapon_is_flag(object_index))",
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0x33d, 1);
      system_exit(-1);
    }
  }

  /* Recursively delete child objects (obj+0xC8). */
  if (obj->unk_200.value != -1) {
    object_delete_internal(obj->unk_200.value, 1);
  }

  /* Optionally recursively delete sibling objects (obj+0xC4). */
  if ((char)delete_sibling != 0 && obj->next_object_index.value != -1) {
    object_delete_internal(obj->next_object_index.value, 1);
  }

  /* Mark datum header with pending-deletion bit (0x08). */
  hdr->unk_2 |= 0x08;

  /* Re-fetch object pointer (may have been invalidated by recursive calls). */
  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* Check if the object's tag definition has a children block. */
  {
    void *tag_def = tag_get(0x6f626a65, (int)obj->tag_index);
    if (*(int *)((char *)tag_def + 0x34) != -1 && (obj->flags & 1) == 0) {
      /* Propagate deletion to attached children. */
      object_propagate_flag_to_children(object_handle, 1, 0);
    }
  }

  /* Re-fetch datum header (recursive calls may have moved pool memory). */
  hdr = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);

  /* Set obj->flags bit 0 (deleted/inactive). */
  obj->flags |= 1;

  /* Clear datum header bit 0x02 (active). */
  hdr->unk_2 &= (uint8_t)~0x02;

  /* Remove the object from the name list. */
  object_remove_from_name_list(object_handle);
}

/*
 * object_delete — delete an object from the world.
 *
 * Thin wrapper around object_delete_internal with delete_sibling=0,
 * meaning only the target object and its children are deleted, not
 * its siblings in the object list.
 *
 * Confirmed: PUSH 0x0 / PUSH EAX / CALL 0x140bc0 / ADD ESP,0x8 — 2 cdecl args.
 */
void object_delete(int object_handle)
{
  object_delete_internal(object_handle, 0);
}

/*
 * object_connect_to_map — link an object into the BSP/collision world.
 *
 * If the object has a parent (parent_object_index != -1), it chains into the
 * parent's child list via next_object_index/unk_200 and marks the header as
 * attached. Otherwise, it resolves a BSP location (from the caller or by
 * probing the object's bounding position), inserts into the collision/BSP
 * structure, and optionally garbage-collects or activates the object based on
 * PVS visibility.
 *
 * Confirmed: CALL 0x119320 (datum_get) with objects global at 0x5a8d50.
 * Confirmed: assert "DATUM_INDEX_TO_IDENTIFIER(object_index)" at line 0x36f.
 * Confirmed: assert "!TEST_FLAG(object->object.flags,
 * _object_connected_to_map_bit)" at line 0x370. Confirmed: CALL 0x13d680
 * (object_get_and_verify_type) with PUSH -1. Confirmed: CALL 0x140bc0
 * (object_delete_internal) with delete_sibling=0. Confirmed: CALL 0xba6c0
 * (players_get_combined_pvs). Confirmed: offset 0xCC = parent_object_index,
 * 0xC8 = unk_200, 0xC4 = next_object_index. Confirmed: 0x5a8d40 and 0x5a8d30
 * are alternate object list pointers selected by flag 0x2000000.
 */
void object_connect_to_map(int object_handle, void *location)
{
  /* permuter 20260721 (+5.0pp, 87.6 -> 92.6): obj_alias + a volatile slot
   * holding the noncollideable-partition address reshape VC71 register
   * scheduling; both are value-identical to the direct forms. */
  object_data_t *obj_alias;
  volatile unsigned int noncollideable_partition;
  object_header_data_t *hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  object_data_t *obj = hdr->object;
  obj_alias = obj;

  if ((object_handle & 0xffff0000) == 0) {
    display_assert("DATUM_INDEX_TO_IDENTIFIER(object_index)",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x36f, 1);
    system_exit(-1);
  }
  if ((obj->flags & 0x800) != 0) {
    display_assert(
      "!TEST_FLAG(object->object.flags, _object_connected_to_map_bit)",
      "c:\\halo\\SOURCE\\objects\\objects.c", 0x370, 1);
    system_exit(-1);
  }

  noncollideable_partition = 0x5a8d30;
  if (obj->parent_object_index.value != -1) {
    /* Child object: chain into parent's child linked list. */
    object_data_t *parent_obj = (object_data_t *)object_get_and_verify_type(
      obj->parent_object_index.value, -1);
    object_data_t *self_obj =
      (object_data_t *)object_get_and_verify_type(object_handle, -1);
    self_obj->next_object_index.value = parent_obj->unk_200.value;
    parent_obj->unk_200.value = (uint32_t)object_handle;
    hdr->unk_2 |= 0x80;
    *(int16_t *)((char *)obj + 0x4c) = -1;
  } else {
    /* Root object: resolve BSP location and insert into collision world. */
    uint32_t local_loc[2];
    uint32_t *loc;
    object_data_t *self_obj;
    void *obj_list;

    if (location == NULL) {
      scenario_location_from_point(local_loc, (char *)obj + 0x50);
      location = local_loc;
      if ((int16_t)local_loc[1] == -1) {
        scenario_location_from_point(local_loc, (char *)obj + 0x0c);
      }
    }

    loc = (uint32_t *)location;
    if ((int16_t)loc[1] == -1) {
      obj->flags |= 0x200000;
    } else {
      obj->unk_72 = loc[0];
      obj->unk_76.value = loc[1];
      hdr->unk_4 = (uint16_t)loc[1];
      obj->flags &= ~0x200000u;
    }

    hdr->unk_2 &= 0x7f;

    self_obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
    obj_list = (self_obj->flags & 0x2000000) ? (void *)0x5a8d40 :
                                               (void *)noncollideable_partition;
    cluster_partition_add_object(obj_list, object_handle, (char *)obj + 0xbc,
                                 (char *)obj_alias + 0x50,
                                 *(uint32_t *)&obj->unk_92, (char *)obj + 0x48);

    if ((hdr->unk_2 & 0x40) != 0) {
      if (hdr->unk_4 != 0xffff) {
        int16_t cluster = (int16_t)hdr->unk_4;
        int *pvs = (int *)players_get_combined_pvs();
        if ((pvs[cluster >> 5] & (1u << (cluster & 0x1f))) != 0) {
          object_activate(object_handle);
          goto done;
        }
      }
      if ((obj->flags & 0x80000) != 0) {
        object_delete_internal(object_handle, 0);
      }
    }
  }
done:
  obj->flags |= 0x800;
  hdr->unk_2 |= 0x20;
}

/*
 * object_get_node_matrix — return a pointer to a specific node's 4x3 matrix
 * within the object's node matrix block.
 *
 * Asserts that the object actually has the requested node via object_has_node.
 * Resolves the node matrix block reference at object+0x1A0, then indexes into
 * it by node_index * 0x34 (52 bytes per node matrix).
 *
 * Confirmed: CALL 0x13fef0 (object_has_node) with 2 stack args, TEST AL,AL.
 * Confirmed: assert string "object_has_node(object_index, node_index)" at
 *            0x29c070, file "c:\halo\SOURCE\objects\objects.c" at 0x29b91c,
 *            line 0x424.
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with PUSH -1.
 * Confirmed: ADD EAX,0x1A0 — node matrix block reference offset.
 * Confirmed: CALL 0x13dfc0 (object_header_block_reference_get).
 * Confirmed: MOVSX ECX,DI / IMUL ECX,ECX,0x34 — sign-extended int16_t index
 *            multiplied by 52.
 * Confirmed: ADD EAX,ECX — final pointer = base + node_index * 0x34.
 */
void *object_get_node_matrix(int object_handle, int16_t node_index)
{
  object_data_t *obj;
  char *nodes;

  if (!object_has_node(object_handle, node_index)) {
    display_assert("object_has_node(object_index, node_index)",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x424, 1);
    system_exit(-1);
  }
  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  nodes = (char *)object_header_block_reference_get(object_handle,
                                                    (void *)&obj->unk_416);
  return nodes + (int)node_index * 0x34;
}

/*
 * object_get_marker_by_name — find markers on an object by name string,
 * returning a count of matched markers.
 *
 * Delegates to model_find_markers (0x124730) which searches the object's
 * animation graph tag for markers matching marker_name. If no markers are
 * found and marker_name is NULL or empty, fills out_markers[0] with a default
 * identity transform and the node-0 matrix, returning 1.
 *
 * When sVar1 == 0 (no markers found from the model search):
 *   - Asserts max_count > 0
 *   - Zeros the node_index at out_markers[0]+0x00
 *   - Initializes a 52-byte identity transform at out_markers[0]+0x04
 *   - Copies the 52-byte node-0 matrix into out_markers[0]+0x38
 *   - If the object has the 0x1000 flag (mirrored), negates the second row
 *     of the node matrix (offsets +0x48, +0x4C, +0x50 in the marker)
 *   - Returns 1 if marker_name is non-NULL and points to an empty string
 *
 * Confirmed: PUSH -1 / PUSH ESI / CALL 0x13d680 — object_get_and_verify_type.
 * Confirmed: PUSH 0x6f626a65 — tag_get('obje', ...).
 * Confirmed: LEA EDX,[EBX+0x130] — object nodes (unk_304) passed to model
 * search. Confirmed: ADD EAX,0x1A0 — node matrix block reference at
 * object+0x1A0. Confirmed: SHR ECX,0xC / AND ECX,0xFFFFFF01 — flag extraction
 * from obj->flags. Confirmed: REP MOVSD with ECX=0xD — copies 52-byte node
 * matrix (0x34 bytes). Confirmed: TEST AH,0x10 — checks flags bit 12 (0x1000)
 * for mirroring. Confirmed: FCHS on floats at [EAX+0x48], [EAX+0x4C],
 * [EAX+0x50]. Confirmed: ADD ESP,0x44 — cleans all 17 dwords across 5 cdecl
 * calls.
 */
int16_t object_get_marker_by_name(int object_handle, void *marker_name,
                                        void *out_markers, int max_count)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  void *tag_data = tag_get(0x6f626a65, obj->tag_index);
  object_data_t *obj2 =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  void *node_matrices =
    object_header_block_reference_get(object_handle, (char *)obj2 + 0x1a0);

  /* model_find_markers (0x124730): search animation graph for named markers.
   * 9 args: anim_graph_data, marker_name, object_nodes, zero, -1,
   *         node_matrices, flags, out_markers, max_count */
  uint32_t mirror_flags = (obj->flags >> 12) & 0xffffff01;
  int16_t result = ((int16_t(*)(int, void *, void *, int, int, void *, uint32_t,
                                void *, int))0x124730)(
    *(int *)((char *)tag_data + 0x34), marker_name, (char *)obj + 0x130, 0, -1,
    node_matrices, mirror_flags, out_markers, max_count);

  if (result != 0)
    return result;

  /* No markers found — fill in a default marker if possible. */
  if ((int16_t)max_count < 1) {
    display_assert("maximum_marker_count>0",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x459, 1);
    system_exit(-1);
  }

  /* Zero the node index (int16_t at offset 0x00). */
  *(int16_t *)out_markers = 0;

  /* Initialize identity transform at out_markers+0x04 (52 bytes). */
  ((void (*)(void *))0x1090e0)((char *)out_markers + 4);

  /* Copy node-0 matrix (52 bytes / 13 dwords) into out_markers+0x38. */
  {
    void *node_mat = object_get_node_matrix(object_handle, 0);
    qmemcpy((char *)out_markers + 0x38, node_mat, 0x34);
  }

  /* If the object is mirrored (flags bit 12), negate the second row of the
   * node matrix within the marker result (offsets +0x48, +0x4C, +0x50). */
  if ((obj->flags & 0x1000) != 0) {
    *(float *)((char *)out_markers + 0x48) =
      -*(float *)((char *)out_markers + 0x48);
    *(float *)((char *)out_markers + 0x4c) =
      -*(float *)((char *)out_markers + 0x4c);
    *(float *)((char *)out_markers + 0x50) =
      -*(float *)((char *)out_markers + 0x50);
  }

  /* If marker_name is non-NULL and points to an empty string, return 1. */
  if (marker_name != NULL && *(char *)marker_name == '\0')
    return 1;

  return 0;
}

/*
 * object_compute_child_marker_position — given an object pointer, a child
 * marker (containing a matrix4x3 at offset 0x38), and a destination matrix,
 * computes the child marker's position relative to the object and writes the
 * resulting position, forward, and up vectors back into the object.
 *
 * Algorithm:
 *   1. Build a matrix4x3 from the object's forward, up, position vectors.
 *   2. Invert it.
 *   3. Multiply the inverted matrix by the child marker's matrix (at +0x38).
 *   4. Invert the result.
 *   5. Multiply destination_matrix by the inverted result.
 *   6. Extract position, forward, and up from the product back to the object.
 *   7. Re-orthogonalize up via cross(cross(forward, up), forward).
 *   8. Normalize forward and up.
 *
 * Confirmed: 3 cdecl args (object, child_marker, destination_matrix).
 * Confirmed: void return — no caller checks EAX.
 * Confirmed: assert strings match "object", "child_marker",
 *            "destination_matrix", "valid_real_matrix4x3(destination_matrix)".
 * Confirmed: source file "c:\\halo\\SOURCE\\objects\\objects.c", lines
 * 0x495–0x499. Confirmed: CALL 0x10a110 (matrix4x3_from_forward_up_position).
 * Confirmed: CALL 0x109150 (matrix_inverse) x2.
 * Confirmed: CALL 0x109850 (matrix4x3_multiply) x2.
 * Confirmed: CALL 0x13010  (normalize3d) x2.
 * Confirmed: CALL 0xf6d00  (valid_real_matrix4x3) for dest_matrix assertion.
 * Confirmed: ADD ESP,0x40 cleans all 16 cdecl arg dwords at once.
 */
void object_compute_child_marker_position(void *object, void *child_marker,
                                          void *dest_matrix)
{
  float local_mat[13]; /* 0x34 bytes: scale + forward + left + up + position */
  float inv_mat[13]; /* 0x34 bytes */
  float *obj_position;
  float *obj_forward;
  float *obj_up;
  float fwd_x, fwd_y, fwd_z;
  float up_x, up_y, up_z;
  x87_wide_t left_x, left_y, left_z; /* 0x14113e-0x14118d: kept in ST(1..3) */

  assert_halt_at(__FILE__, 11414, object != NULL);
  assert_halt_at(__FILE__, 11415, child_marker != NULL);
  assert_halt_at(__FILE__, 11416, dest_matrix != NULL);
  assert_halt_at(__FILE__, 11417, valid_real_matrix4x3((float *)dest_matrix));

  obj_position = (float *)((char *)object + 0xc);
  obj_forward = (float *)((char *)object + 0x24);
  obj_up = (float *)((char *)object + 0x30);

  /* Build a matrix4x3 from the object's orientation and position */
  matrix4x3_from_forward_up_position(local_mat, obj_position, obj_forward,
                                     obj_up);

  /* Invert it */
  matrix_inverse(local_mat, inv_mat);

  /* Multiply by the child marker's matrix at offset 0x38 */
  matrix4x3_multiply(inv_mat, (float *)((char *)child_marker + 0x38), inv_mat);

  /* Invert the result */
  matrix_inverse(inv_mat, inv_mat);

  /* Multiply dest_matrix by the inverted result, storing in local_mat */
  matrix4x3_multiply((float *)dest_matrix, inv_mat, local_mat);

  /* Extract position back to object (offsets 0x28..0x30 in matrix = indices
   * 10..12) */
  obj_position[0] = local_mat[10];
  obj_position[1] = local_mat[11];
  obj_position[2] = local_mat[12];

  /* Extract forward back to object (offsets 0x04..0x0c in matrix = indices
   * 1..3) */
  obj_forward[0] = local_mat[1];
  obj_forward[1] = local_mat[2];
  obj_forward[2] = local_mat[3];

  /* Re-orthogonalize up: left = cross(forward, up_from_matrix),
   * then up = cross(left, forward).
   * up_from_matrix is at indices 7..9 (offsets 0x1c..0x24). */
  fwd_x = local_mat[1];
  fwd_y = local_mat[2];
  fwd_z = local_mat[3];
  up_x = local_mat[7];
  up_y = local_mat[8];
  up_z = local_mat[9];

  /* left = cross(forward, up) */
  left_x = (x87_wide_t)fwd_y * up_z - (x87_wide_t)fwd_z * up_y;
  left_y = (x87_wide_t)fwd_z * up_x - (x87_wide_t)up_z * fwd_x;
  left_z = (x87_wide_t)up_y * fwd_x - (x87_wide_t)fwd_y * up_x;

  /* up_new = cross(left, forward) */
  obj_up[0] = left_y * fwd_z - left_z * fwd_y;
  obj_up[1] = left_z * fwd_x - fwd_z * left_x;
  obj_up[2] = fwd_y * left_x - left_y * fwd_x;

  normalize3d(obj_forward);
  normalize3d(obj_up);
}

/*
 * object_detach_from_parent — detach an object from its parent in the
 * object hierarchy.
 *
 * Retrieves both the child and parent object data, disconnects the child
 * from the map, then re-computes its orientation in world space using the
 * parent's node matrix.  After updating position/orientation/up vectors from
 * the parent, clears the node index (0xFF) and parent handle (-1), then
 * reconnects to the map.
 *
 * Finally, sets the "connected to cluster" flag (bit 0 of header+0x02) if
 * the object is not already connected, doesn't have the 0x100000 flag, and
 * has no parent.
 *
 * Confirmed: object_get_and_verify_type(handle, -1) for both child and parent.
 * Confirmed: object_disconnect_from_map(handle), then recompute world matrix
 *            via object_get_node_matrix(parent_handle, node_index).
 * Confirmed: matrix4x3_identity_with_position, matrix_from_forward_and_up,
 *            matrix4x3_multiply used to transform orientation.
 * Confirmed: Copies 3 floats at +0x18, +0x1C, +0x20 and +0x3C, +0x40, +0x44
 *            from parent to child.
 * Confirmed: Sets node_index (byte at +0xD0) = 0xFF, parent (int at +0xCC) =
 * -1. Confirmed: datum_get + flag check on header+0x02 bit 0, object+0x04 &
 * 0x100000, object+0xCC == -1 to set connected flag.
 */
void object_detach_from_parent(int object_handle)
{
  object_data_t *child;
  object_data_t *parent;
  void *node_matrix;
  float child_position[13];
  float child_orientation[13];
  float result[13];
  object_header_data_t *header;

  child = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  parent = (object_data_t *)object_get_and_verify_type(
    child->parent_object_index.value, -1);

  object_disconnect_from_map(object_handle);

  node_matrix =
    object_get_node_matrix(child->parent_object_index.value,
                           (int16_t) * (int8_t *)((char *)child + 0xd0));

  matrix4x3_identity_with_position(child_position, (float *)&child->position);
  matrix_from_forward_and_up(child_orientation, (float *)&child->forward,
                             (float *)&child->up);
  matrix4x3_multiply((float *)node_matrix, child_position, result);
  matrix4x3_multiply(result, child_orientation, result); /* dup-args-ok */
  matrix4x3_decompose(result, (float *)&child->position, (float *)&child->forward,
                      (float *)&child->up);

  child->translational_velocity = parent->translational_velocity;
  child->angular_velocity = parent->angular_velocity;

  *(int8_t *)((char *)child + 0xd0) = -1;
  child->parent_object_index.value = NONE;

  object_connect_to_map(object_handle, NULL);

  header =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  child = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  if (!(header->unk_2 & 1) && !(child->flags & 0x100000) &&
      child->parent_object_index.value == NONE) {
    header->unk_2 |= 1;
  }
}

/*
 * object_get_world_position — retrieve the world-space position of an object.
 *
 * If the object has no parent (parent_object_index == -1), copies the local
 * position vector (obj+0x0C) directly to out_position.
 *
 * If the object is attached to a parent, retrieves the parent's node matrix
 * via object_get_node_matrix (using the sign-extended node index byte at
 * obj+0xD0) and transforms the local position through that matrix via
 * matrix_transform_point.
 *
 * Confirmed: PUSH -1, PUSH EAX — object_get_and_verify_type(handle, -1).
 * Confirmed: CMP EAX,-1 — checks parent_object_index at obj+0xCC.
 * Confirmed: MOVSX CX, byte ptr [ESI+0xD0] — sign-extends node index byte.
 * Confirmed: ADD ESP,0x14 cleans 5 args (2 + 3 from two cdecl calls).
 * Confirmed: MOV EAX, EDI — returns out_position pointer.
 */
vector3_t *object_get_world_position(int object_handle, vector3_t *out_position)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  void *node_mat;

  if (obj->parent_object_index.value == NONE) {
    /* No parent — local position is the world position */
    *out_position = obj->position;
    return out_position;
  }

  /* Parented — transform local position through parent's node matrix */
  node_mat = object_get_node_matrix(obj->parent_object_index.value,
                                    (int16_t) * (int8_t *)((char *)obj + 0xd0));
  matrix_transform_point((float *)node_mat, (float *)&obj->position,
                         (float *)out_position);
  return out_position;
}

/*
 * object_get_orientation — get an object's forward and/or up orientation
 * vectors in world space.
 *
 * If the object has no parent (parent_object_index == -1), copies the local
 * forward (obj+0x24) and up (obj+0x30) vectors directly.
 * If parented, transforms both vectors through the parent's node matrix via
 * matrix_transform_vector (0x109680).
 *
 * When both out_forward and out_up are provided, validates that they form
 * perpendicular unit axes via valid_real_normal3d_perpendicular (0x84a70).
 *
 * Confirmed: 3 cdecl args at [EBP+0x8..0x10].
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (handle, -1).
 * Confirmed: CALL 0x140eb0 (object_get_node_matrix) with (parent_handle,
 *            sign-extended byte at obj+0xD0).
 * Confirmed: CALL 0x109680 (matrix_transform_vector) with (node_matrix,
 *            src_vector, out_vector) — 3 cdecl args each call.
 * Confirmed: CALL 0x84a70 (valid_real_normal3d_perpendicular) with
 *            (out_forward, out_up).
 * Confirmed: Assertion at line 0x5b6, strings "forward" (0x28cb2c) and
 *            "up" (0x28cb28), format at 0x267490.
 */
/* 0x141360 */
void object_get_orientation(int object_handle, float *out_forward,
                            float *out_up)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  if (obj->parent_object_index.value == NONE) {
    /* No parent — copy local forward and up vectors directly */
    if (out_forward != NULL) {
      *(real_vector3d *)out_forward = *(real_vector3d *)&obj->forward;
    }
    if (out_up != NULL) {
      *(real_vector3d *)out_up = *(real_vector3d *)&obj->up;
    }
  } else {
    /* Parented — transform through parent's node matrix */
    void *node_mat =
      object_get_node_matrix(obj->parent_object_index.value,
                             (int16_t) * (int8_t *)((char *)obj + 0xd0));

    if (out_forward != NULL) {
      matrix_transform_vector((float *)node_mat, (float *)&obj->forward,
                              out_forward);
    }
    if (out_up != NULL) {
      matrix_transform_vector((float *)node_mat, (float *)&obj->up, out_up);
    }
  }

  /* Validate perpendicularity if both vectors were requested */
  if (out_forward != NULL && out_up != NULL) {
    if (!valid_real_normal3d_perpendicular(out_forward, out_up)) {
      display_assert(
        csprintf(
          (char *)0x5ab100,
          "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
          "forward", "up", (double)out_forward[0], (double)out_forward[1],
          (double)out_forward[2], (double)out_up[0], (double)out_up[1],
          (double)out_up[2]),
        "c:\\halo\\SOURCE\\objects\\objects.c", 0x5b6, 1);
      system_exit(-1);
    }
  }
}

/*
 * object_get_world_matrix — build a 4x3 world-space matrix for an object.
 *
 * Constructs the matrix from the object's position (obj+0xc), forward
 * vector (obj+0x24), and up vector (obj+0x30) via
 * matrix4x3_from_forward_up_position (which calls matrix_from_forward_and_up
 * then copies position to offset 0x28).
 *
 * If the object has a parent (parent_object_index at obj+0xcc != -1),
 * retrieves the parent's node matrix via object_get_node_matrix (using the node
 * index byte at obj+0xd0) and multiplies it with the local matrix via
 * matrix4x3_multiply (matrix_multiply), storing the result in-place.
 *
 * Confirmed: PUSH -1, PUSH EAX — object_get_and_verify_type(handle, -1).
 * Confirmed: ADD ESP,0x18 cleans 6 args (2 + 4 from two cdecl calls).
 * Confirmed: MOVSX CX, byte ptr [ESI+0xd0] — sign-extends node index.
 * Confirmed: ADD ESP,0x14 cleans 5 args (2 + 3 from two cdecl calls).
 * Confirmed: MOV EAX, EDI — returns out_matrix pointer.
 */
void *object_get_world_matrix(int object_handle, void *out_matrix)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* Build local matrix from position, forward, up */
  ((void (*)(void *, float *, float *, float *))0x10a110)(
    out_matrix, (float *)((char *)obj + 0xc), (float *)((char *)obj + 0x24),
    (float *)((char *)obj + 0x30));

  /* If parented, multiply by parent's node matrix */
  if (obj->parent_object_index.value != NONE) {
    void *node_mat =
      object_get_node_matrix(obj->parent_object_index.value,
                             (int16_t) * (int8_t *)((char *)obj + 0xd0));
    matrix4x3_multiply((float *)node_mat, (float *)out_matrix,
                       (float *)out_matrix); /* dup-args-ok */
  }

  return out_matrix;
}

/*
 * object_inverse_kinematics — inverse-kinematics matrix adjustment between two
 * object markers.
 *
 * Resolves two named markers (marker A on object param_1, marker B on object
 * param_3) into local marker buffers via object_get_marker_by_name, then
 * walks marker A's animation-node parent chain (self -> parent -> grandparent)
 * using the model tag's node block (mode tag, 'mode' = 0x6d6f6465). It composes
 * inverse(markerA_matrix) * markerB_matrix into a local 4x3 matrix and hands
 * that plus the three node matrices (self/parent/grandparent, indexed into the
 * caller's node-matrix array at param_5 with a 0x34-byte stride) to the IK
 * solver (inverse_kinematics_adjust_matrices @ 0x120fd0).
 *
 * Early-exits (returns) if either marker lookup fails or a parent index is NONE
 * (-1). param_5 is the caller-owned node-matrix array (node_matrices).
 *
 * ABI: cdecl, 5 stack args (confirmed: MOV ESP,EBP / caller cleanup ADD
 * ESP,...).
 *
 * Decompiler traps resolved:
 *  - Ghidra's local_a4/local_a0 and local_d8 names were systematically off by
 * 4; the marker buffers are single contiguous 0x6c-byte objects
 * (matrix_identity fills +4, the node-matrix copy fills +0x38; the short node
 * index is at +0). Declared as char[0x6c] so MSVC's full-buffer write cannot
 * overflow.
 *  - EBX register reuse: it holds the node block pointer (mode_tag + 0xb8)
 * across both tag_block_get_element calls, then its low word is reused as the
 *    grandparent index. Kept as two distinct C variables (nodes_block,
 *    grandparent_index) to avoid aliasing.
 *  - markerA matrix is read at bufA+4 (matrix_inverse); markerB matrix is read
 * at bufB+0x38 (matrix4x3_multiply). Asymmetric on purpose — preserved.
 *  - matrix4x3_multiply aliases b == out (&composed_matrix twice). Faithful.
 *  - Node indices are signed shorts via MOVSX; NONE test is == -1.
 */
void object_inverse_kinematics(int param_1, int param_2, int param_3,
                               int param_4, int param_5)
{
  char marker_a[0x6c];
  char marker_b[0x6c];
  char composed_matrix[0x34];
  void *obj_datum;
  int obje_tag;
  int mode_tag;
  int nodes_block;
  void *node_element;
  short self_index;
  short parent_index;
  short grandparent_index;

  obj_datum = object_get_and_verify_type(param_1, -1);
  obje_tag = (int)tag_get(0x6f626a65, *(int *)obj_datum);
  mode_tag = (int)tag_get(0x6d6f6465, *(int *)(obje_tag + 0x34));

  if (object_get_marker_by_name(param_1, (void *)param_2, marker_a, 1) ==
      0) {
    return;
  }
  if (object_get_marker_by_name(param_3, (void *)param_4, marker_b, 1) ==
      0) {
    return;
  }

  nodes_block = mode_tag + 0xb8;
  self_index = *(short *)marker_a;

  node_element =
    tag_block_get_element((void *)nodes_block, (int)self_index, 0x9c);
  parent_index = *(short *)((char *)node_element + 0x24);
  if (parent_index == -1) {
    return;
  }

  node_element =
    tag_block_get_element((void *)nodes_block, (int)parent_index, 0x9c);
  grandparent_index = *(short *)((char *)node_element + 0x24);
  if (grandparent_index == -1) {
    return;
  }

  matrix_inverse((float *)(marker_a + 4), (float *)composed_matrix);
  matrix4x3_multiply((float *)(marker_b + 0x38), (float *)composed_matrix,
                     (float *)composed_matrix);

  inverse_kinematics_adjust_matrices(
    (float *)composed_matrix, (int)grandparent_index * 0x34 + param_5,
    (int)parent_index * 0x34 + param_5, (int)self_index * 0x34 + param_5);
}

/*
 * object_find_in_radius — find objects of a given type within a spherical area.
 *
 * Uses the structure system to identify candidate clusters, then iterates
 * through objects in those clusters, filtering by type_mask and distance.
 * Objects within (obj_effective_radius + search_radius) of the search position
 * are collected into out_handles.  Returns the count of found objects.
 *
 * Parameters (cdecl, 7 args):
 *   flags            — passed to object_find_in_cluster
 *   type_mask        — bit mask of object types to include (0 → all types)
 *   cluster_info     — pointer to a cluster location struct; word at +4 is
 *                       the cluster count passed to structure_find_in_cluster
 *   position         — float[3] search center (must be non-NULL)
 *   radius           — search radius, added to each object's effective radius
 *   out_handles      — output array for found object handles (must be non-NULL)
 *   max_count        — maximum number of handles to collect
 *
 * Confirmed: 7 cdecl params at [EBP+0x8..0x20].
 * Confirmed: param_3 (EBX) is a struct pointer, word at +4 = cluster count.
 * Confirmed: param_4 (ESI) is the float* position, used in distance math.
 * Confirmed: CALL 0x199230 with 5 args: (cluster_count_word, position, radius,
 *            0x200, cluster_indices_ptr).
 * Confirmed: CALL 0x140420 with 5 args: (flags, cluster_count, cluster_indices,
 *            0x800, object_indices_ptr). Max counts are hardcoded 512 and 2048.
 * Confirmed: Distance check uses obj+0x50/0x54/0x58 vs position, and
 *            obj+0x5C as effective object radius.
 * Confirmed: Returns short (count of found objects).
 * Confirmed: Only type_mask gets the 0 → -1 treatment; flags is NOT checked.
 * Confirmed: Assert strings: "location" (0x29c114), "center" (0x253f0c),
 *            "object_indices" (0x29c104) at lines 0x6f3, 0x6f4, 0x6f5.
 * Confirmed: Inner assert uses csprintf with full format at 0x29b940:
 *            "got an object type we didn't expect (expected one of 0x%08x
 *             but got #%d)." with args (-1, type).
 * Confirmed: Loop iterator i is int16_t (BX register, CMP BX).
 */
/* 0x1415f0 */
int16_t object_find_in_radius(int flags, unsigned int type_mask,
                              void *cluster_info, float *position, float radius,
                              int *out_handles, int16_t max_count)
{
  int16_t found_count = 0;
  int16_t iter_count;
  int16_t i;

  int16_t cluster_indices[512];
  int object_indices[2048];

  if (cluster_info == NULL) {
    display_assert("location", "c:\\halo\\SOURCE\\objects\\objects.c", 0x6f3,
                   1);
    system_exit(-1);
  }
  if (position == NULL) {
    display_assert("center", "c:\\halo\\SOURCE\\objects\\objects.c", 0x6f4, 1);
    system_exit(-1);
  }
  if (out_handles == NULL) {
    display_assert("object_indices", "c:\\halo\\SOURCE\\objects\\objects.c",
                   0x6f5, 1);
    system_exit(-1);
  }

  if (type_mask == 0)
    type_mask = 0xFFFFFFFF;

  iter_count =
    structure_find_in_cluster(*(uint16_t *)((char *)cluster_info + 4), position,
                              radius, 512, cluster_indices);

  iter_count = object_find_in_cluster(flags, iter_count, cluster_indices, 2048,
                                      object_indices);

  for (i = 0; i < iter_count && found_count < max_count; i++) {
    int handle = object_indices[i];
    object_header_data_t *header =
      (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle);
    object_data_t *obj = header->object;

    if ((1 << obj->type) == 0) {
      char *msg = csprintf((char *)0x5ab100,
                           "got an object type we didn't expect "
                           "(expected one of 0x%08x but got #%d).",
                           (int)-1, (int)obj->type);
      display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
      system_exit(-1);
    }

    if ((type_mask & (1 << obj->type)) != 0) {
      float dx = obj->unk_80 - position[0];
      float dy = obj->unk_84 - position[1];
      float dz = obj->unk_88 - position[2];
      float effective_radius = obj->unk_92 + radius;

      if (dx * dx + dz * dz + dy * dy <= effective_radius * effective_radius) {
#ifdef HALO_RNG_TRACE
        RNG_TRACE_EX(RNG_TRACE_KIND_RADIUS_HIT, *(unsigned int *)&obj->unk_88,
                     handle);
#endif
#line 11177
        out_handles[found_count] = handle;
        found_count++;
      }
    }
  }

  return found_count;
}

/* 0x1417c0 — objects_reconnect_to_structure_bsp: reconnects objects to
 * the current BSP structure after a BSP switch. Iterates all objects and
 * updates their cluster assignments.
 *
 * Source: objects.c
 * No params. EBX is set locally to -1 (not a register arg).
 */
/* 0x1417c0 */
void objects_reconnect_to_structure_bsp(void)
{
  int local_102c[1028];
  int obj;
  object_iter_t bsp_iter;
  char bsp_data[8];

  data_verify(*(data_t **)0x5a8d50);

  bsp_iter.type_mask = -1;
  bsp_iter.flags = 0;
  bsp_iter.current_index = 0;
  bsp_iter.last_handle = NONE;
  bsp_iter.cookie = 0x86868686;
  obj = (int)object_iterator_next(&bsp_iter);
  while (obj != 0) {
    if ((*(unsigned int *)(obj + 4) & 0x800) != 0 &&
        *(int *)(obj + 0xcc) == -1) {
      *(unsigned int *)(obj + 4) = *(unsigned int *)(obj + 4) & 0xfffff7ff;
      *(short *)(obj + 0x4c) = -1;
      {
        int dat_handle;
        dat_handle = (int)datum_get(*(void **)0x5a8d50, bsp_iter.last_handle);
        *(short *)(dat_handle + 4) = -1;
      }
      scenario_location_from_point(bsp_data, (void *)(obj + 0x50));
      if (*(short *)(bsp_data + 4) == -1) {
        /* Sphere-test the object's bounding sphere against the current BSP.
         * Confirmed 6 cdecl args at 0x14185d-0x141873 (single ADD ESP,0x18):
         *   collision_bsp_test_sphere(global_collision_bsp_get(), 0, 0,
         *                             obj+0x50, *(int*)(obj+0x5c), local_102c)
         * The objects.obj mass-lift dropped 5 of these (called with only the
         * bsp), so origin/direction/radius/results* came from stale stack ->
         * wild access -> the "Loading level..." kernel halt. */
        collision_bsp_test_sphere((int)global_collision_bsp_get(), 0, 0,
                                  obj + 0x50, *(int *)(obj + 0x5c), local_102c);
        if (local_102c[771] != 0) {
          /* Hit: record the leaf/cluster index (local_102c[772]) in bsp_data[0]
           * (MOV [EBP-0x8],EAX at 0x14188d — omitted by the original lift),
           * then resolve the structure BSP index from the scenario block. */
          *(int *)bsp_data = local_102c[772];
          if (local_102c[772] == -1) {
            *(short *)(bsp_data + 4) = -1;
          } else {
            *(short *)(bsp_data + 4) =
              *(short *)((char *)tag_block_get_element(
                           (char *)scenario_get() + 0xe0,
                           local_102c[772] & 0x7fffffff, 0x10) +
                         8);
          }
        } else {
          scenario_location_from_point(bsp_data, (void *)(obj + 0xc));
        }
      }
      object_connect_to_map(bsp_iter.last_handle, bsp_data);
    }
    obj = (int)object_iterator_next(&bsp_iter);
  }
}

/*
 * objects_paparazzi (0x141900 / objects.obj) — delete every object flagged for
 * deletion (object flags bit 0x400000).
 *
 * Mirrors objects_disconnect_from_structure_bsp's structure: data_verify the
 * object table, then walk all objects with an inlined iterator (type_mask = -1,
 * flags = 0, the binary inlines object_iterator_new's five field stores).  Each
 * object whose flags carry bit 0x400000 is removed via
 * object_delete_internal(handle, 0).
 *
 * Confirmed (disasm 0x141900): data_verify(*(data_t**)0x5a8d50); iterator at
 * EBP-0x10 with EAX=-1 written to type_mask(+0)/last_handle(+8), byte
 * flags(+4)=0, word index(+6)=0, cookie(+0xc)=0x86868686; TEST [EAX+4],0x400000
 * then object_delete_internal(it.last_handle, 0) with PUSH 0 / PUSH
 * last_handle.
 */
void objects_paparazzi(void)
{
  object_iter_t it;
  object_data_t *obj;

  data_verify(*(data_t **)0x5a8d50);
  it.cookie = 0x86868686;
  it.type_mask = -1;
  it.flags = 0;
  it.current_index = 0;
  it.last_handle = NONE;

  obj = (object_data_t *)object_iterator_next(&it);
  while (obj != (object_data_t *)0) {
    if ((obj->flags & 0x400000) != 0) {
      object_delete_internal(it.last_handle, 0);
    }
    obj = (object_data_t *)object_iterator_next(&it);
  }
}

/*
 * object_export_function_values (0x141970 / objects.obj) — evaluate the four
 * object "function input" values from the object tag and store them into the
 * object's function value cache (object+0xd4, four floats).
 *
 * For each of the four function-input source codes (object tag+0x108, stride
 * 2), a non-zero code selects a value via a jump table (table at 0x141b38, byte
 * index map at 0x141b58 keyed on code-1): code 1  -> object+0x90 (raw float)
 *   code 2  -> object+0x94, clamped <=1.0
 *   code 3  -> object+0x9c
 *   code 4  -> object+0x98
 *   code 5  -> if cached value == 1.0, a new random value (random_math_real)
 *   code 0x12 -> 0.0 when object+0xb6 bit 4 set, else 1.0
 *   code 0x13 -> heading-vs-scenario angle: atan2(marker[+4], marker[+8]) of
 * the base node marker (object_get_node_matrix(handle,0)); wrapped against
 * scenario+0x4c (signed_angular_difference), scaled (0x29c120) + offset
 * (0x253398), clamped to [0,1]; falls back to the cached value when
 * |marker[+0xc]| >= threshold (0x29c128) codes 0xa..0x11 (default) -> region
 * state byte object+0x128+(code-0xa) * 0x261518 any other code in default range
 * -> assert (region_index out of range)
 *
 * Read-only with respect to object lifecycle: writes only the object's own
 * function value cache (object+0xd4..). No GC/garbage/cluster-list mutation.
 *
 * Confirmed: 1 cdecl arg (object_handle @ [EBP+0x8]); 4-iteration loop
 * ([EBP-0x8]). Confirmed: default value is 0.0 (FLOAT 0x2533c0); 1.0 =
 * 0x2533c8. Confirmed (push-then-fstp): signed_angular_difference takes TWO
 * args — param_1 = scenario+0x4c (PUSH ECX at 0x141a9a), param_2 = the FPATAN
 * result stored via FSTP [ESP] at 0x141a8f over the PUSH ECX at 0x141a89; ADD
 * ESP,8 cleans both. Decompiler dropped param_2. Confirmed: jump table at
 * 0x141b38 / index map at 0x141b58 (code-1 keyed).
 */
void object_export_function_values(int object_handle)
{
  int *obj;
  int object_definition;
  short *codes;
  float *values;
  short i;
  short code;
  short region;
  float value;
  int marker;
  float angle;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  object_definition = (int)tag_get(0x6f626a65, *obj);
  codes = (short *)(object_definition + 0x108);
  values = (float *)(obj + 0x35); /* object+0xd4 */
  for (i = 0; i < 4; i++) {
    code = codes[i];
    if (code != 0) {
      value = REAL_ZERO_POOL; /* default 0.0 */
      switch (code) {
      case 3:
        value = *(float *)((char *)obj + 0x9c);
        break;
      case 4:
        value = *(float *)((char *)obj + 0x98);
        break;
      case 1:
        value = *(float *)((char *)obj + 0x90);
        break;
      case 2:
        value = *(float *)((char *)obj + 0x94);
        if (REAL_ONE_POOL < value) {
          value = REAL_ONE_POOL;
        }
        break;
      case 5:
        if (values[i] == 1.0f) {
          value =
            random_math_real((unsigned int *)get_global_random_seed_address());
        }
        break;
      case 0x12:
        if (*(unsigned char *)((char *)obj + 0xb6) & 4) {
          value = REAL_ZERO_POOL;
        } else {
          value = REAL_ONE_POOL;
        }
        break;
      case 0x13:
        marker = (int)object_get_node_matrix(object_handle, 0);
        if (fabs(*(float *)(marker + 0xc)) < DOUBLE_0_995_POOL) {
#if defined(_MSC_VER) && !defined(__clang__)
#undef atan2
          angle = signed_angular_difference(
            *(float *)((char *)global_scenario_get() + 0x4c),
            (float)atan2(*(float *)(marker + 4), *(float *)(marker + 8)));
#define atan2 atan2_
#else
          angle = signed_angular_difference(
            *(float *)((char *)global_scenario_get() + 0x4c),
            (float)xbox_atan2((double)*(float *)(marker + 4),
                              (double)*(float *)(marker + 8)));
#endif
          value = angle * REAL_ONE_OVER_TWO_PI_POOL + REAL_0_5_POOL;
          if (value < REAL_ZERO_POOL) {
            value = REAL_ZERO_POOL;
          } else {
            if (REAL_ONE_POOL < value) {
              value = REAL_ONE_POOL;
            }
          }
        } else {
          value = values[i];
        }
        break;
      default:
        region = (short)(code - 0xa);
        if ((region < 0) || (region >= 8)) {
          display_assert(
            "region_index>=0 && region_index<MAXIMUM_REGIONS_PER_OBJECT",
            "c:\\halo\\SOURCE\\objects\\objects.c", 0xa46, 1);
          system_exit(-1);
        }
        value = (float)*(unsigned char *)((char *)obj + 0x128 + (int)region) *
                REAL_ONE_OVER_255_POOL;
        break;
      }
      values[i] = value;
    }
  }
}




/* Type-cast helpers for object_compute_node_matrices — kept at file scope for
 * C89 compliance */
typedef void (*animation_set_default_fn)(void *model_tag, void *anim_data);
typedef void (*animation_decode_fn)(void *model_tag, void *anim_entry,
                                    int frame_index, void *anim_data);
typedef void (*animation_overlay_keyframe_fn)(void *anim_entry,
                                              float frame_value,
                                              void *anim_data);
typedef void (*animation_overlay_interpolate_fn)(void *anim_entry,
                                                 int frame_index,
                                                 void *anim_data,
                                                 void *node_data);
typedef void (*overlay_adjust_fn)(int object_handle, void *anim_data);
typedef void (*anim_interpolate_fn)(uint16_t node_count, void *interp_data,
                                    void *anim_data, int16_t frame_index,
                                    int16_t frame_count);
typedef char (*valid_real_vectors_fn)(float *fwd, float *left, float *up);
typedef char (*valid_real_matrix4x3_fn)(float *m);
typedef char (*valid_fwd_and_up_fn)(float *fwd, float *up);
/* Byte-returning point3d validator: the original tests AL (testb), so this
 * target uses a char-returning cast rather than the shared int typedef at
 * file scope (which object_new relies on). */
typedef char (*valid_real_point3d_b_fn)(float *p);
typedef void (*matrix_4x3_multiply_fn)(float *a, float *b, float *out);
typedef void (*matrix_4x3_from_point_fn)(float *out, float *point);
typedef void (*model_node_set_default_fn)(float *out, void *anim_data);

/*
 * object_compute_node_matrices — compute the full node matrix hierarchy for an
 * object, transforming each node from local (animation) space into world space.
 *
 * If the object definition has no model (tag+0x34 == -1), builds a trivial
 * single-node matrix from the object's forward, up, and position vectors.
 *
 * Otherwise:
 *  1. Resolves the object type definition and model tag.
 *  2. Gets the parent node matrix if the object is attached to a parent.
 *  3. Decompresses the base animation pose into per-node local transforms.
 *  4. Applies overlay animations from the object definition's animation graph.
 *  5. Scales the root animation data if the object has a non-zero scale factor.
 *  6. Calls type-specific overlay adjustments via object_type_definition.
 *  7. Interpolates node matrices for objects with interpolation data.
 *  8. Validates the object's position and orientation vectors.
 *  9. Walks the node hierarchy (BFS via child/sibling indices), composing each
 *     node's local transform with its parent's world matrix.
 * 10. Applies an origin offset from the model tag and writes the bounding
 *     sphere radius.
 *
 * The bulk of the assembly is assertion/validation code that checks matrix
 * components for NaN/Inf and perpendicularity, expanded from the
 * assert_valid_real_matrix4x3 macro.
 *
 * Confirmed: 1 cdecl arg (object_handle). void return.
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (-1, handle).
 * Confirmed: CALL 0x1ba140 (tag_get) with 'obje' and object definition index.
 * Confirmed: CALL 0x13dfc0 (object_header_block_reference_get) for node matrix
 *            and animation data block references.
 * Confirmed: 0xfe0 type mask = bits 5..11 (_object_mask_cannot_interpolate).
 * Confirmed: CALL 0x109500 (model_node_matrices_set_default) for default pose.
 * Confirmed: CALL 0x109850 (matrix_4x3_multiply) for composing transforms.
 * Confirmed: CALL 0x109280 (matrix_4x3_from_point) for translation matrices.
 * Confirmed: CALL 0x109e10 (matrix_from_forward_and_up) for orientation.
 * Confirmed: CALL 0x109590 (matrix_transform_point) for origin offset.
 * Confirmed: SUB ESP,0xa44 — large stack frame for animation data and node
 *            queue.
 */
void object_compute_node_matrices(int object_handle)
{
  object_data_t *obj;
  void *object_tag;
  float *node_matrices;
  uint8_t obj_type_byte;
  void *anim_data;
  char anim_data_stack[0x84c];

  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  object_tag = tag_get(0x6f626a65, *(int *)obj);
  node_matrices = (float *)object_header_block_reference_get(
    object_handle, (void *)((char *)obj + 0x1a0));

  /* Objects with type bits 5..11 set cannot interpolate and use a stack
   * buffer for animation data; others use the block reference at +0x19c. */
  obj_type_byte = *(uint8_t *)((char *)obj + 0x64);

  if ((1 << (obj_type_byte & 0x1f)) & 0xfe0u) {
    anim_data = anim_data_stack;
  } else {
    anim_data = object_header_block_reference_get(
      object_handle, (void *)((char *)obj + 0x19c));
  }

  if (*(int *)((char *)object_tag + 0x34) == -1) {
    /* No model — build a trivial matrix from object vectors */
    node_matrices[0] = 1.0f; /* scale */
    node_matrices[1] = *(float *)((char *)obj + 0x24); /* forward.x */
    node_matrices[2] = *(float *)((char *)obj + 0x28); /* forward.y */
    node_matrices[3] = *(float *)((char *)obj + 0x2c); /* forward.z */
    node_matrices[7] = *(float *)((char *)obj + 0x30); /* up.x */
    node_matrices[8] = *(float *)((char *)obj + 0x34); /* up.y */
    node_matrices[9] = *(float *)((char *)obj + 0x38); /* up.z */
    /* left = up x forward */
    /* 0x14349f-0x1434ae: both products and the subtraction stay in ST;
     * without the promotion clang spilled one product to a dword. */
    node_matrices[4] = (x87_wide_t)node_matrices[8] * node_matrices[3] -
                       (x87_wide_t)node_matrices[9] * node_matrices[2];
    node_matrices[5] = (x87_wide_t)node_matrices[9] * node_matrices[1] -
                       (x87_wide_t)node_matrices[7] * node_matrices[3];
    node_matrices[6] = (x87_wide_t)node_matrices[7] * node_matrices[2] -
                       (x87_wide_t)node_matrices[8] * node_matrices[1];
    /* position */
    node_matrices[10] = *(float *)((char *)obj + 0x0c);
    node_matrices[11] = *(float *)((char *)obj + 0x10);
    node_matrices[12] = *(float *)((char *)obj + 0x14);
  } else {
    /* Has a model — full animation pipeline */
    void *model_tag;
    float *parent_node_mat;
    uint8_t override_decompressor;
    int anim_tag_index;

    object_type_definition_get(*(int16_t *)((char *)obj + 0x64));
    model_tag = tag_get(0x6d6f6465, *(int *)((char *)object_tag + 0x34));

    /* Get parent node matrix if attached to a parent object */
    parent_node_mat = NULL;
    if (obj->parent_object_index.value != -1) {
      parent_node_mat = (float *)object_get_node_matrix(
        obj->parent_object_index.value,
        (int16_t) * (int8_t *)((char *)obj + 0xd0));
    }

    override_decompressor = 0;

    /* Decode animation pose into anim_data */
    anim_tag_index = *(int *)((char *)obj + 0x7c);
    if (anim_tag_index == -1 || *(int16_t *)((char *)obj + 0x80) == -1) {
      /* No animation graph or no animation index — use default pose */
      FUN_00123aa0(model_tag, anim_data);
    } else {
      void *anim_tag = tag_get(0x616e7472, anim_tag_index);
      void *anim_entry = tag_block_get_element(
        (char *)anim_tag + 0x74, (int)*(int16_t *)((char *)obj + 0x80), 0xb4);

      if ((*(char *)((char *)obj + 0x4) < 0) &&
          (0 < *(int16_t *)((char *)anim_entry + 0x22))) {
        /* Object flag bit 7 set and animation has frames — compute
         * frame from game_time + object_handle modulo frame count */
        int time = game_time_get();
        uint32_t frame_index =
          (uint32_t)(time + object_handle) %
          (uint32_t)(int)*(int16_t *)((char *)anim_entry + 0x22);
        if ((int16_t)frame_index < 0) {
          display_assert("frame_index>=0",
                         "c:\\halo\\SOURCE\\objects\\objects.c", 0xa90, 1);
          system_exit(-1);
        }
        FUN_00121d60(model_tag, anim_entry, (int)(int16_t)frame_index,
                     anim_data);
      } else {
        /* Use the stored frame index at obj+0x82 */
        int16_t frame_idx = *(int16_t *)((char *)obj + 0x82);
        FUN_00121d60(model_tag, anim_entry, (int)frame_idx, anim_data);
      }
      override_decompressor =
        (*(uint8_t *)((char *)anim_entry + 0x3a) >> 1) & 1;
    }

    /* Apply overlay animations from object definition's animation graph */
    if (*(int *)((char *)object_tag + 0x44) != -1) {
      void *overlay_anim_tag =
        tag_get(0x616e7472, *(int *)((char *)object_tag + 0x44));
      int overlay_count = *(int *)overlay_anim_tag;
      int16_t overlay_idx = 0;
      if (overlay_count > 0) {
        int i = 0;
        do {
          int16_t *overlay_entry =
            (int16_t *)tag_block_get_element(overlay_anim_tag, i, 0x14);
          if (*overlay_entry != -1) {
            int16_t region_idx = overlay_entry[1];
            if ((int)region_idx < *(int *)((char *)object_tag + 0x158)) {
              void *region_block = tag_block_get_element(
                (char *)object_tag + 0x158, (int)region_idx, 0x168);
              void *overlay_anim_entry = tag_block_get_element(
                (char *)overlay_anim_tag + 0x74, (int)*overlay_entry, 0xb4);
              int16_t mode = overlay_entry[2];
              float func_value =
                *(float *)((char *)obj + 0xe4 + region_idx * 4);

              if (mode == 0) {
                /* Keyframe overlay */
                float total_frames;
                float frame_value;
                if ((*(uint8_t *)region_block & 2) == 0) {
                  total_frames =
                    (float)(int)(*(int16_t *)((char *)overlay_anim_entry +
                                              0x22) -
                                 1);
                } else {
                  total_frames =
                    (float)(int)*(int16_t *)((char *)overlay_anim_entry + 0x22);
                }
                frame_value = total_frames * func_value;
                overlay_animation_apply_continuous(overlay_anim_entry, frame_value, anim_data);
              } else if (mode == 1) {
                /* Interpolated overlay */
                int time = game_time_get();
                uint32_t frame_mod =
                  (uint32_t)(time + object_handle) %
                  (uint32_t)(int)*(int16_t *)((char *)overlay_anim_entry +
                                              0x22);
                /* 0x141e22: pushes anim_data ([EBP-0x14]), func_value
                 * ([EBP-0xc], stored at 0x141dbb) and the remainder (EDX) --
                 * param 3 is the float animation_scale, not a node buffer. */
                overlay_animation_apply_scaled(
                  overlay_anim_entry, (int16_t)frame_mod, func_value, anim_data);
              }
            }
          }
          overlay_idx = (int16_t)(overlay_idx + 1);
          i = (int)overlay_idx;
        } while (i < overlay_count);
      }
    }

    /* Scale animation data if object has a non-zero scale factor */
    if (*(float *)((char *)obj + 0x60) > REAL_ZERO_POOL) {
      float scale = *(float *)((char *)obj + 0x60);
      *(float *)((char *)anim_data + 0x1c) =
        scale * *(float *)((char *)anim_data + 0x1c);
      *(float *)((char *)anim_data + 0x10) =
        scale * *(float *)((char *)anim_data + 0x10);
      *(float *)((char *)anim_data + 0x14) =
        scale * *(float *)((char *)anim_data + 0x14);
      *(float *)((char *)anim_data + 0x18) =
        scale * *(float *)((char *)anim_data + 0x18);
    }

    /* Call type-specific overlay adjustments */
    if (*(int *)((char *)object_tag + 0x44) != -1) {
      object_type_preprocess_node_orientations(object_handle, (int)anim_data);
    }

    /* Interpolate node matrices if the object has interpolation data */
    if (*(int16_t *)((char *)obj + 0x86) > 0) {
      void *interp_data;
      if ((1 << (obj_type_byte & 0x1f)) & 0xfe0u) {
        display_assert("!TEST_FLAG(_object_mask_cannot_interpolate, "
                       "object->object.type)",
                       "c:\\halo\\SOURCE\\objects\\objects.c", 0xad9, 1);
        system_exit(-1);
      }
      interp_data = object_header_block_reference_get(
        object_handle, (void *)((char *)obj + 0x198));
      interpolate_node_orientations(
        *(int16_t *)((char *)model_tag + 0xb8), interp_data, anim_data,
        *(int16_t *)((char *)obj + 0x84), *(int16_t *)((char *)obj + 0x86));
    }

    /* Validate object position and orientation if not using override */
    if (!override_decompressor) {
      if (!valid_real_point3d((float *)((char *)obj + 0x0c))) {
        char *name = (char *)tag_get_name(*(int *)obj);
        char *msg =
          csprintf((char *)0x5ab100,
                   "%s had a bad position before compute_node_matrices "
                   "(%f,%f,%f)",
                   name, (double)*(float *)((char *)obj + 0x0c),
                   (double)*(float *)((char *)obj + 0x10),
                   (double)*(float *)((char *)obj + 0x14));
        display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xae2, 1);
        system_exit(-1);
      }
      if (!valid_real_normal3d_perpendicular((float *)((char *)obj + 0x24),
                                             (float *)((char *)obj + 0x30))) {
        char *name = (char *)tag_get_name(*(int *)obj);
        char *msg = csprintf((char *)0x5ab100,
                             "%s had a bad forward and up before "
                             "compute_node_matrices (%f,%f,%f)x(%f,%f,%f)",
                             name, (double)*(float *)((char *)obj + 0x24),
                             (double)*(float *)((char *)obj + 0x28),
                             (double)*(float *)((char *)obj + 0x2c),
                             (double)*(float *)((char *)obj + 0x30),
                             (double)*(float *)((char *)obj + 0x34),
                             (double)*(float *)((char *)obj + 0x38));
        display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xae3, 1);
        system_exit(-1);
      }
    }

    /* Walk the node hierarchy via breadth-first traversal */
    {
      uint16_t node_queue[64];
      int queue_read = 0;
      int queue_write = 1;
      void *model_nodes_block = (char *)model_tag + 0xb8;

      float root_anim[13];
      float orientation_matrix[13];
      float translation_matrix[13];
      float phys_offset_matrix[13];
      float origin_matrix[13];
      float parent_copy[13];

      node_queue[0] = 0;

      do {
        int16_t cur_read = (int16_t)queue_read;
        uint16_t node_idx_u16 = node_queue[cur_read];
        int node_idx;
        void *node_data;
        queue_read++;
        node_idx = (int)(int16_t)node_idx_u16;
        node_data = tag_block_get_element(model_nodes_block, node_idx, 0x9c);

        if ((int16_t)node_idx_u16 == 0) {
          /* Root node processing */
          FUN_00109500(root_anim, anim_data);

          if (!override_decompressor) {
            /* Build orientation matrix from object's forward and up */
            matrix4x3_identity_with_position(translation_matrix,
                                             (float *)((char *)obj + 0x0c));
            ((void (*)(float *, float *, float *))0x109e10)(
              orientation_matrix, (float *)((char *)obj + 0x24),
              (float *)((char *)obj + 0x30));

            /* Negate left column if object flag 0x1000 is set */
            if ((*(uint32_t *)((char *)obj + 0x4) & 0x1000) != 0) {
              orientation_matrix[4] = -orientation_matrix[4];
              orientation_matrix[5] = -orientation_matrix[5];
              orientation_matrix[6] = -orientation_matrix[6];
            }

            /* Apply physics center-of-mass offset if present */
            if (*(int *)((char *)object_tag + 0x8c) != -1) {
              void *phys_tag =
                tag_get(0x70687973, *(int *)((char *)object_tag + 0x8c));
              float neg_com[3];
              neg_com[0] = -*(float *)((char *)phys_tag + 0x0c);
              neg_com[1] = -*(float *)((char *)phys_tag + 0x10);
              neg_com[2] = -*(float *)((char *)phys_tag + 0x14);
              matrix4x3_identity_with_position(phys_offset_matrix, neg_com);
              matrix4x3_multiply(orientation_matrix, phys_offset_matrix,
                                 orientation_matrix);
            }

            /* Apply model origin offset */
            matrix4x3_identity_with_position(
              origin_matrix, (float *)((char *)object_tag + 0x14));
            matrix4x3_multiply(orientation_matrix, origin_matrix,
                               orientation_matrix);

            if (parent_node_mat == NULL) {
              /* No parent — compose directly */
              matrix4x3_multiply(translation_matrix, orientation_matrix,
                                 node_matrices);
              matrix4x3_multiply(node_matrices, root_anim, node_matrices);
            } else {
              /* Has parent — may need to scale and adjust */
              if (*(uint32_t *)parent_node_mat != 0x3f800000) {
                /* Parent scale != 1.0: scale the orientation position */
                float pscale = *parent_node_mat;
                int k;
                float *src;
                float *dst;
                orientation_matrix[10] *= pscale;
                orientation_matrix[11] *= pscale;
                orientation_matrix[12] *= pscale;
                /* Copy parent matrix to local buffer and set scale=1 */
                src = parent_node_mat;
                dst = parent_copy;
                for (k = 0xd; k != 0; k--) {
                  *dst = *src;
                  src++;
                  dst++;
                }
                parent_node_mat = parent_copy;
                parent_copy[0] = 1.0f;
                obj = (object_data_t *)object_get_and_verify_type(
                  object_handle, -1); /* decompiler artifact: reload ESI */
              }

              /* Check if parent object has flag 0x1000 (mirrored) */
              {
                void *parent_obj = object_get_and_verify_type(
                  obj->parent_object_index.value, -1);
                if ((*(uint32_t *)((char *)parent_obj + 0x4) & 0x1000) != 0) {
                  /* Copy parent matrix if not already copied */
                  if (parent_node_mat != parent_copy) {
                    float *src2 = parent_node_mat;
                    float *dst2 = parent_copy;
                    int k2;
                    for (k2 = 0xd; k2 != 0; k2--) {
                      *dst2 = *src2;
                      src2++;
                      dst2++;
                    }
                    parent_node_mat = parent_copy;
                    obj = (object_data_t *)object_get_and_verify_type(
                      object_handle, -1);
                  }
                  /* Negate the left column of parent matrix */
                  parent_node_mat[4] = -parent_node_mat[4];
                  parent_node_mat[5] = -parent_node_mat[5];
                  parent_node_mat[6] = -parent_node_mat[6];
                }
              }

              /* Validate parent node matrix */
              {
                uint32_t scale_bits = *(uint32_t *)parent_node_mat & 0x7f800000;
                if (scale_bits == 0x7f800000 ||
                    !valid_real_vector3d_axes3(parent_node_mat + 1,
                                               parent_node_mat + 4,
                                               parent_node_mat + 7) ||
                    !valid_real_point3d(parent_node_mat + 10)) {
                  /* Parent node matrix is invalid — detailed error
                   * reporting */
                  void *parent_obj_2 = object_get_and_verify_type(
                    obj->parent_object_index.value, -1);
                  char *obj_name = (char *)tag_get_name(*(int *)obj);
                  char *parent_name =
                    (char *)tag_get_name(*(int *)parent_obj_2);
                  char *context =
                    csprintf((char *)0x5ab100, "%s as parent node of %s",
                             parent_name, obj_name);

                  /* assert_valid_real_matrix4x3 expanded inline */
                  if ((*(uint32_t *)parent_node_mat & 0x7f800000) ==
                      0x7f800000) {
                    char *msg =
                      csprintf((char *)0x5ab100, "%s had a bad scale %f",
                               context, (double)*parent_node_mat);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
                    system_exit(-1);
                  }
                  if (!valid_real_normal3d(parent_node_mat + 1)) {
                    char *msg = csprintf(
                      (char *)0x5ab100, "%s had a bad forward (%f,%f,%f)",
                      context, (double)parent_node_mat[1],
                      (double)parent_node_mat[2], (double)parent_node_mat[3]);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
                    system_exit(-1);
                  }
                  if (!valid_real_normal3d(parent_node_mat + 4)) {
                    char *msg = csprintf(
                      (char *)0x5ab100, "%s had a bad left (%f,%f,%f)", context,
                      (double)parent_node_mat[4], (double)parent_node_mat[5],
                      (double)parent_node_mat[6]);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
                    system_exit(-1);
                  }
                  if (!valid_real_normal3d(parent_node_mat + 7)) {
                    char *msg = csprintf(
                      (char *)0x5ab100, "%s had a bad up (%f,%f,%f)", context,
                      (double)parent_node_mat[7], (double)parent_node_mat[8],
                      (double)parent_node_mat[9]);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
                    system_exit(-1);
                  }
                  if (!valid_real_point3d(parent_node_mat + 10)) {
                    char *msg = csprintf(
                      (char *)0x5ab100, "%s had a bad position (%f,%f,%f)",
                      context, (double)parent_node_mat[10],
                      (double)parent_node_mat[11], (double)parent_node_mat[12]);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
                    system_exit(-1);
                  }
                  {
                    float dot_fl = parent_node_mat[1] * parent_node_mat[4] +
                                   parent_node_mat[2] * parent_node_mat[5] +
                                   parent_node_mat[3] * parent_node_mat[6];
                    if ((*(uint32_t *)&dot_fl & 0x7f800000) == 0x7f800000 ||
                        !(fabs(dot_fl) < DOUBLE_0_001_POOL)) {
                      char *msg = csprintf(
                        (char *)0x5ab100,
                        "%s had a forward (%f,%f,%f) not perpendicular "
                        "to left (%f,%f,%f)",
                        context, (double)parent_node_mat[1],
                        (double)parent_node_mat[2], (double)parent_node_mat[3],
                        (double)parent_node_mat[4], (double)parent_node_mat[5],
                        (double)parent_node_mat[6]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb37, 1);
                      system_exit(-1);
                    }
                  }
                  {
                    float dot_ul = parent_node_mat[7] * parent_node_mat[4] +
                                   parent_node_mat[8] * parent_node_mat[5] +
                                   parent_node_mat[9] * parent_node_mat[6];
                    if ((*(uint32_t *)&dot_ul & 0x7f800000) == 0x7f800000 ||
                        !(fabs(dot_ul) < DOUBLE_0_001_POOL)) {
                      char *msg = csprintf(
                        (char *)0x5ab100,
                        "%s had a up (%f,%f,%f) not perpendicular to "
                        "left (%f,%f,%f)",
                        context, (double)parent_node_mat[7],
                        (double)parent_node_mat[8], (double)parent_node_mat[9],
                        (double)parent_node_mat[4], (double)parent_node_mat[5],
                        (double)parent_node_mat[6]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb37, 1);
                      system_exit(-1);
                    }
                  }
                  {
                    float dot_uf = parent_node_mat[7] * parent_node_mat[1] +
                                   parent_node_mat[2] * parent_node_mat[8] +
                                   parent_node_mat[3] * parent_node_mat[9];
                    if ((*(uint32_t *)&dot_uf & 0x7f800000) == 0x7f800000 ||
                        !(fabs(dot_uf) < DOUBLE_0_001_POOL)) {
                      char *msg = csprintf(
                        (char *)0x5ab100,
                        "%s had a forward (%f,%f,%f) not perpendicular "
                        "to up (%f,%f,%f)",
                        context, (double)parent_node_mat[1],
                        (double)parent_node_mat[2], (double)parent_node_mat[3],
                        (double)parent_node_mat[7], (double)parent_node_mat[8],
                        (double)parent_node_mat[9]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb37, 1);
                      system_exit(-1);
                    }
                  }
                  if (!valid_real_matrix4x3(parent_node_mat)) {
                    char *msg =
                      csprintf((char *)0x5ab100,
                               "%s: assert_valid_real_matrix4x3", context);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
                    system_exit(-1);
                  }
                }
              }

              /* Compose parent * translation, then node * orientation,
               * then multiply with root anim */
              matrix4x3_multiply(parent_node_mat, translation_matrix,
                                 node_matrices);
              matrix4x3_multiply(node_matrices, orientation_matrix,
                                 node_matrices);
              matrix4x3_multiply(node_matrices, root_anim, node_matrices);
            }
          } else {
            /* override_decompressor — just copy root_anim to node_matrices */
            float *src3 = root_anim;
            float *dst3 = node_matrices;
            int k3;
            for (k3 = 0xd; k3 != 0; k3--) {
              *dst3 = *src3;
              src3++;
              dst3++;
            }
          }

          /* Validate root node matrix — first quick check */
          {
            float *fwd = node_matrices + 1;
            float *left = node_matrices + 4;
            uint32_t scale_bits = *(uint32_t *)node_matrices & 0x7f800000;
            if (scale_bits == 0x7f800000 ||
                !valid_real_vector3d_axes3(fwd, left, node_matrices + 7) ||
                (*(uint32_t *)&node_matrices[10] & 0x7f800000) == 0x7f800000 ||
                (*(uint32_t *)&node_matrices[11] & 0x7f800000) == 0x7f800000 ||
                (*(uint32_t *)&node_matrices[12] & 0x7f800000) == 0x7f800000) {
              {
                /* Root node matrix invalid — dump diagnostic info */
                char *name;
                obj = (object_data_t *)object_get_and_verify_type(object_handle,
                                                                  -1);
                name = (char *)tag_get_name(*(int *)obj);
                error(2,
                      "object_compute_node_matrices FAILURE on root node "
                      "of %s",
                      name);
                error(2, "  object: pos %f %f %f, fwd %f %f %f, up %f %f %f",
                      (double)*(float *)((char *)obj + 0x0c),
                      (double)*(float *)((char *)obj + 0x10),
                      (double)*(float *)((char *)obj + 0x14),
                      (double)*(float *)((char *)obj + 0x24),
                      (double)*(float *)((char *)obj + 0x28),
                      (double)*(float *)((char *)obj + 0x2c),
                      (double)*(float *)((char *)obj + 0x30),
                      (double)*(float *)((char *)obj + 0x34),
                      (double)*(float *)((char *)obj + 0x38));

                if (*(int *)((char *)object_tag + 0x8c) != -1) {
                  void *phys_tag2 =
                    tag_get(0x70687973, *(int *)((char *)object_tag + 0x8c));
                  error(2, "  center-of-mass translation %f %f %f",
                        (double)-*(float *)((char *)phys_tag2 + 0x0c),
                        (double)-*(float *)((char *)phys_tag2 + 0x10),
                        (double)-*(float *)((char *)phys_tag2 + 0x14));
                }
                error(2, "  origin-offset %f %f %f",
                      (double)*(float *)((char *)object_tag + 0x14),
                      (double)*(float *)((char *)object_tag + 0x18),
                      (double)*(float *)((char *)object_tag + 0x1c));

                if (parent_node_mat == NULL) {
                  error(2, "  no parent node");
                } else {
                  error(2, "  parent-node matrix fwd  %f %f %f",
                        (double)parent_node_mat[1], (double)parent_node_mat[2],
                        (double)parent_node_mat[3]);
                  error(2, "                     left %f %f %f",
                        (double)parent_node_mat[4], (double)parent_node_mat[5],
                        (double)parent_node_mat[6]);
                  error(2, "                     up   %f %f %f",
                        (double)parent_node_mat[7], (double)parent_node_mat[8],
                        (double)parent_node_mat[9]);
                  error(2, "                     posn %f %f %f",
                        (double)parent_node_mat[10],
                        (double)parent_node_mat[11],
                        (double)parent_node_mat[12]);
                  error(2,
                        "                     scale (jason's ugly secret) "
                        "%f",
                        (double)*parent_node_mat);
                }

                error(2, "");

                error(2, "computed matrix fwd  %f %f %f",
                      (double)node_matrices[1], (double)node_matrices[2],
                      (double)node_matrices[3]);
                error(2, "                left %f %f %f",
                      (double)node_matrices[4], (double)node_matrices[5],
                      (double)node_matrices[6]);
                error(2, "                up   %f %f %f",
                      (double)node_matrices[7], (double)node_matrices[8],
                      (double)node_matrices[9]);
                error(2, "                posn %f %f %f",
                      (double)node_matrices[10], (double)node_matrices[11],
                      (double)node_matrices[12]);
                error(2, "                scale %f", (double)*node_matrices);

                /* assert_valid_real_matrix4x3 on root node (line 0xb69)
                 */
                if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000 ||
                    !valid_real_vector3d_axes3(fwd, left, node_matrices + 7) ||
                    !valid_real_point3d(node_matrices + 10)) {
                  if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000) {
                    char *msg =
                      csprintf((char *)0x5ab100, "%s had a bad scale %f",
                               "object_compute_node_matrices root node matrix",
                               (double)*node_matrices);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb69, 1);
                    system_exit(-1);
                  }
                  {
                    float mag_fwd = fwd[0] * fwd[0] + fwd[1] * fwd[1] +
                                    fwd[2] * fwd[2] - REAL_ONE_POOL;
                    if ((*(uint32_t *)&mag_fwd & 0x7f800000) == 0x7f800000 ||
                        !(fabs(mag_fwd) < DOUBLE_0_001_POOL)) {
                      char *msg = csprintf(
                        (char *)0x5ab100, "%s had a bad forward (%f,%f,%f)",
                        "object_compute_node_matrices root node matrix",
                        (double)fwd[0], (double)fwd[1], (double)fwd[2]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb69, 1);
                      system_exit(-1);
                    }
                  }
                  {
                    float mag_left = left[0] * left[0] + left[1] * left[1] +
                                     left[2] * left[2] - REAL_ONE_POOL;
                    if ((*(uint32_t *)&mag_left & 0x7f800000) == 0x7f800000 ||
                        !(fabs(mag_left) < DOUBLE_0_001_POOL)) {
                      char *msg = csprintf(
                        (char *)0x5ab100, "%s had a bad left (%f,%f,%f)",
                        "object_compute_node_matrices root node matrix",
                        (double)left[0], (double)left[1], (double)left[2]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb69, 1);
                      system_exit(-1);
                    }
                  }
                  {
                    float mag_up = node_matrices[9] * node_matrices[9] +
                                   node_matrices[8] * node_matrices[8] +
                                   node_matrices[7] * node_matrices[7] -
                                   REAL_ONE_POOL;
                    if ((*(uint32_t *)&mag_up & 0x7f800000) == 0x7f800000 ||
                        !(fabs(mag_up) < DOUBLE_0_001_POOL)) {
                      char *msg = csprintf(
                        (char *)0x5ab100, "%s had a bad up (%f,%f,%f)",
                        "object_compute_node_matrices root node matrix",
                        (double)node_matrices[7], (double)node_matrices[8],
                        (double)node_matrices[9]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb69, 1);
                      system_exit(-1);
                    }
                  }
                  if ((*(uint32_t *)&node_matrices[10] & 0x7f800000) ==
                        0x7f800000 ||
                      (*(uint32_t *)&node_matrices[11] & 0x7f800000) ==
                        0x7f800000 ||
                      (*(uint32_t *)&node_matrices[12] & 0x7f800000) ==
                        0x7f800000) {
                    char *msg = csprintf(
                      (char *)0x5ab100, "%s had a bad position (%f,%f,%f)",
                      "object_compute_node_matrices root node matrix",
                      (double)node_matrices[10], (double)node_matrices[11],
                      (double)node_matrices[12]);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb69, 1);
                    system_exit(-1);
                  }
                  {
                    float dot_fl =
                      fwd[0] * left[0] + fwd[1] * left[1] + fwd[2] * left[2];
                    if ((*(uint32_t *)&dot_fl & 0x7f800000) == 0x7f800000 ||
                        !(fabs(dot_fl) < DOUBLE_0_001_POOL)) {
                      char *msg = csprintf(
                        (char *)0x5ab100,
                        "%s had a forward (%f,%f,%f) not perpendicular "
                        "to left (%f,%f,%f)",
                        "object_compute_node_matrices root node matrix",
                        (double)fwd[0], (double)fwd[1], (double)fwd[2],
                        (double)left[0], (double)left[1], (double)left[2]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb69, 1);
                      system_exit(-1);
                    }
                  }
                  {
                    float dot_ul = left[0] * node_matrices[7] +
                                   node_matrices[8] * left[1] +
                                   node_matrices[9] * left[2];
                    if ((*(uint32_t *)&dot_ul & 0x7f800000) == 0x7f800000 ||
                        !(fabs(dot_ul) < DOUBLE_0_001_POOL)) {
                      char *msg = csprintf(
                        (char *)0x5ab100,
                        "%s had a up (%f,%f,%f) not perpendicular to "
                        "left (%f,%f,%f)",
                        "object_compute_node_matrices root node matrix",
                        (double)node_matrices[7], (double)node_matrices[8],
                        (double)node_matrices[9], (double)left[0],
                        (double)left[1], (double)left[2]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb69, 1);
                      system_exit(-1);
                    }
                  }
                  {
                    float dot_uf = node_matrices[7] * fwd[0] +
                                   node_matrices[8] * fwd[1] +
                                   node_matrices[9] * fwd[2];
                    if ((*(uint32_t *)&dot_uf & 0x7f800000) == 0x7f800000 ||
                        !(fabs(dot_uf) < DOUBLE_0_001_POOL)) {
                      char *msg = csprintf(
                        (char *)0x5ab100,
                        "%s had a forward (%f,%f,%f) not perpendicular "
                        "to up (%f,%f,%f)",
                        "object_compute_node_matrices root node matrix",
                        (double)fwd[0], (double)fwd[1], (double)fwd[2],
                        (double)node_matrices[7], (double)node_matrices[8],
                        (double)node_matrices[9]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb69, 1);
                      system_exit(-1);
                    }
                  }
                  if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000 ||
                      !valid_real_vector3d_axes3(fwd, left,
                                                 node_matrices + 7) ||
                      !valid_real_point3d(node_matrices + 10)) {
                    char *msg = csprintf(
                      (char *)0x5ab100, "%s: assert_valid_real_matrix4x3",
                      "object_compute_node_matrices root node matrix");
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb69, 1);
                    system_exit(-1);
                  }
                }
              }
            }
          }

          /* Final assert_valid_real_matrix4x3 on root (line 0xb77) */
          {
            float *fwd2 = node_matrices + 1;
            float *left2 = node_matrices + 4;
            if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000 ||
                !valid_real_vector3d_axes3(fwd2, left2, node_matrices + 7) ||
                (*(uint32_t *)&node_matrices[10] & 0x7f800000) == 0x7f800000 ||
                (*(uint32_t *)&node_matrices[11] & 0x7f800000) == 0x7f800000 ||
                (*(uint32_t *)&node_matrices[12] & 0x7f800000) == 0x7f800000) {
              char *name2 = (char *)tag_get_name(*(int *)obj);

              if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000) {
                char *msg = csprintf((char *)0x5ab100, "%s had a bad scale %f",
                                     name2, (double)*node_matrices);
                display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                               0xb77, 1);
                system_exit(-1);
              }
              {
                float mag_fwd = fwd2[0] * fwd2[0] + fwd2[1] * fwd2[1] +
                                fwd2[2] * fwd2[2] - REAL_ONE_POOL;
                if ((*(uint32_t *)&mag_fwd & 0x7f800000) == 0x7f800000 ||
                    !(fabs(mag_fwd) < DOUBLE_0_001_POOL)) {
                  char *msg = csprintf(
                    (char *)0x5ab100, "%s had a bad forward (%f,%f,%f)", name2,
                    (double)fwd2[0], (double)fwd2[1], (double)fwd2[2]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb77, 1);
                  system_exit(-1);
                }
              }
              {
                float mag_left = left2[0] * left2[0] + left2[1] * left2[1] +
                                 left2[2] * left2[2] - REAL_ONE_POOL;
                if ((*(uint32_t *)&mag_left & 0x7f800000) == 0x7f800000 ||
                    !(fabs(mag_left) < DOUBLE_0_001_POOL)) {
                  char *msg = csprintf(
                    (char *)0x5ab100, "%s had a bad left (%f,%f,%f)", name2,
                    (double)left2[0], (double)left2[1], (double)left2[2]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb77, 1);
                  system_exit(-1);
                }
              }
              {
                float mag_up = node_matrices[9] * node_matrices[9] +
                               node_matrices[8] * node_matrices[8] +
                               node_matrices[7] * node_matrices[7] -
                               REAL_ONE_POOL;
                if ((*(uint32_t *)&mag_up & 0x7f800000) == 0x7f800000 ||
                    !(fabs(mag_up) < DOUBLE_0_001_POOL)) {
                  char *msg = csprintf(
                    (char *)0x5ab100, "%s had a bad up (%f,%f,%f)", name2,
                    (double)node_matrices[7], (double)node_matrices[8],
                    (double)node_matrices[9]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb77, 1);
                  system_exit(-1);
                }
              }
              if ((*(uint32_t *)&node_matrices[10] & 0x7f800000) ==
                    0x7f800000 ||
                  (*(uint32_t *)&node_matrices[11] & 0x7f800000) ==
                    0x7f800000 ||
                  (*(uint32_t *)&node_matrices[12] & 0x7f800000) ==
                    0x7f800000) {
                char *msg = csprintf(
                  (char *)0x5ab100, "%s had a bad position (%f,%f,%f)", name2,
                  (double)node_matrices[10], (double)node_matrices[11],
                  (double)node_matrices[12]);
                display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                               0xb77, 1);
                system_exit(-1);
              }
              {
                float dot_fl =
                  fwd2[0] * left2[0] + fwd2[1] * left2[1] + fwd2[2] * left2[2];
                if ((*(uint32_t *)&dot_fl & 0x7f800000) == 0x7f800000 ||
                    !(fabs(dot_fl) < DOUBLE_0_001_POOL)) {
                  char *msg = csprintf(
                    (char *)0x5ab100,
                    "%s had a forward (%f,%f,%f) not perpendicular "
                    "to left (%f,%f,%f)",
                    name2, (double)fwd2[0], (double)fwd2[1], (double)fwd2[2],
                    (double)left2[0], (double)left2[1], (double)left2[2]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb77, 1);
                  system_exit(-1);
                }
              }
              {
                float dot_ul = left2[0] * node_matrices[7] +
                               node_matrices[8] * left2[1] +
                               node_matrices[9] * left2[2];
                if ((*(uint32_t *)&dot_ul & 0x7f800000) == 0x7f800000 ||
                    !(fabs(dot_ul) < DOUBLE_0_001_POOL)) {
                  char *msg = csprintf(
                    (char *)0x5ab100,
                    "%s had a up (%f,%f,%f) not perpendicular to "
                    "left (%f,%f,%f)",
                    name2, (double)node_matrices[7], (double)node_matrices[8],
                    (double)node_matrices[9], (double)left2[0],
                    (double)left2[1], (double)left2[2]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb77, 1);
                  system_exit(-1);
                }
              }
              {
                float dot_uf = node_matrices[7] * fwd2[0] +
                               fwd2[1] * node_matrices[8] +
                               fwd2[2] * node_matrices[9];
                if ((*(uint32_t *)&dot_uf & 0x7f800000) == 0x7f800000 ||
                    !(fabs(dot_uf) < DOUBLE_0_001_POOL)) {
                  char *msg = csprintf(
                    (char *)0x5ab100,
                    "%s had a forward (%f,%f,%f) not perpendicular "
                    "to up (%f,%f,%f)",
                    name2, (double)fwd2[0], (double)fwd2[1], (double)fwd2[2],
                    (double)node_matrices[7], (double)node_matrices[8],
                    (double)node_matrices[9]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb77, 1);
                  system_exit(-1);
                }
              }
              if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000 ||
                  !valid_real_vector3d_axes3(fwd2, left2, node_matrices + 7) ||
                  !valid_real_point3d(node_matrices + 10)) {
                char *msg = csprintf((char *)0x5ab100,
                                     "%s: assert_valid_real_matrix4x3", name2);
                display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                               0xb77, 1);
                system_exit(-1);
              }
            }
          }
        } else {
          /* Non-root node: set default pose then multiply by parent */
          float *node_mat = node_matrices + node_idx * 13;
          int16_t parent_idx;
          float *parent_mat;
          FUN_00109500(node_mat,
                       (float *)((char *)anim_data + node_idx * 0x20));

          if (*(int16_t *)((char *)node_data + 0x24) == -1) {
            display_assert("node->parent_node_index!=NONE",
                           "c:\\halo\\SOURCE\\objects\\objects.c", 0xb71, 1);
            system_exit(-1);
          }
          parent_idx = *(int16_t *)((char *)node_data + 0x24);
          parent_mat = node_matrices + parent_idx * 13;
          matrix4x3_multiply(parent_mat, node_mat, node_mat);
        }

        /* Enqueue child and sibling nodes */
        if (*(uint16_t *)((char *)node_data + 0x20) != 0xffff) {
          node_queue[(int16_t)queue_write] =
            *(uint16_t *)((char *)node_data + 0x20);
          queue_write++;
        }
        if (*(uint16_t *)((char *)node_data + 0x22) != 0xffff) {
          node_queue[(int16_t)queue_write] =
            *(uint16_t *)((char *)node_data + 0x22);
          queue_write++;
        }
      } while ((int16_t)queue_read != (int16_t)queue_write);
    }
  }

  /* Apply origin offset from model tag and set bounding sphere radius */
  matrix_transform_point(node_matrices, (float *)((char *)object_tag + 0x08),
                         (float *)((char *)obj + 0x50));
  {
    float radius = *(float *)((char *)object_tag + 0x04);
    *(float *)((char *)obj + 0x5c) = radius;
    if (*(float *)((char *)obj + 0x60) > REAL_ZERO_POOL) {
      *(float *)((char *)obj + 0x5c) = radius * *(float *)((char *)obj + 0x60);
    }
  }
}

/* objects_scripting_detach — scripting wrapper: detach param_2 from param_1.
 * Checks that param_2's parent_object_index (+0xcc) matches param_1, then
 * calls object_detach_from_parent on param_2.
 * 0x143510 / objects.obj
 */
void objects_scripting_detach(int param_1, int param_2)
{
  int iVar1;

  if (param_1 != -1 && param_2 != -1) {
    iVar1 = (int)object_get_and_verify_type(param_2, 0xffffffff);
    if (*(int *)(iVar1 + 0xcc) == param_1) {
      object_detach_from_parent(param_2);
    }
  }
}

/* object_render_debug / objects.obj -- render debug visualizations for an
 * object. */
void object_render_debug(int param_1)
{
  int *obj;
  char *tag_data;
  float world_matrix[10];
  float root_pos[3];
  float info_text_buf[3];
  float point_out[3];
  float bbox_corners[24];

  obj = (int *)object_get_and_verify_type(param_1, -1);
  tag_data = (char *)tag_get(0x6f626a65, *obj);

  if (*(char *)0x5a8d27 != '\0') {
    void *nm;
    nm = object_get_node_matrix(param_1, 0);
    FUN_001894d0(1, nm, 0.3f);
  }

  if (*(char *)0x5a8d24 != '\0' && *(short *)((char *)obj + 0x6a) != -1) {
    int scen_elem;
    void *block_elem;
    scen_elem = (int)global_scenario_get();
    block_elem = tag_block_get_element(
      (void *)(scen_elem + 0x204), (int)*(short *)((char *)obj + 0x6a), 0x24);
    object_get_world_position(param_1, (vector3_t *)root_pos);
    FUN_00189cb0(0, root_pos, block_elem, *(int *)0x2ee6f4);
    tag_data = (char *)tag_get(0x6f626a65, *obj);
  }

  if (*(char *)0x46f088 != '\0') {
    char *backslash;
    char *name_ptr;
    backslash = (char *)strrchr(*(char **)(tag_data + 0x2c), 0x5c);
    object_get_world_matrix(param_1, world_matrix);
    object_get_root_location(param_1, root_pos, 0);
    if (backslash == (char *)0) {
      name_ptr = *(char **)(tag_data + 0x2c);
    } else {
      name_ptr = backslash + 1;
    }
    FUN_00189cb0(0, info_text_buf, name_ptr, *(int *)0x2ee6f0);
    FUN_001894d0(1, world_matrix, ((float *)obj)[0x17]);
    FUN_00189320(1, info_text_buf, root_pos, 1.0f, *(void **)0x2ee6e0);
    tag_data = (char *)tag_get(0x6f626a65, *obj);
  }

  if (*(char *)0x324c30 != '\0') {
    if (*(int *)(tag_data + 0x34) != -1 || *(int *)(tag_data + 0x7c) != -1) {
      void *sphere_color;
      float scale_val;
      sphere_color = *(void **)0x2ee6cc;
      if (*(float *)((char *)obj + 0x5c) <= 0.0f) {
        sphere_color = *(void **)0x2ee6d0;
      } else {
        int datum_body;
        datum_body = (int)datum_get(*(data_t **)0x5a8d50, param_1);
        if (*(unsigned char *)(datum_body + 2) & 1) {
          sphere_color = *(void **)0x2ee6e0;
        }
      }
      if (*(float *)((char *)obj + 0x5c) > 0.0f) {
        scale_val = *(float *)((char *)obj + 0x5c);
      } else {
        scale_val = 1.0f;
      }
      FUN_00189540(1, (char *)obj + 0x50, scale_val, sphere_color);
      tag_data = (char *)tag_get(0x6f626a65, *obj);
    }
  }

  if (*(char *)0x324c31 != '\0') {
    int collision_state[2];
    if (FUN_0014c8e0(collision_state, param_1)) {
      FUN_0014cf20((int)collision_state);
    }
  }

  if (*(char *)0x5a8d26 != '\0') {
    int debug_state[2];
    if (FUN_001509c0(debug_state, param_1)) {
      FUN_001508b0(debug_state);
    }
  }

  if (*(char *)0x5a8d25 != '\0' &&
      (*(unsigned char *)((char *)obj + 4) & 1) == 0 &&
      (*(unsigned char *)((char *)obj + 0xb6) & 4) == 0) {
    int col_tag_data;
    int *coll_block;
    short i;
    float *x_range;
    float *y_range;
    float *z_range;
    short bi;
    short bj;
    short bk;

    if (*(short *)((char *)obj + 0x64) == 7) {
      short vf;
      vf = *(short *)(tag_data + 0x292);
      if (!(vf & 1))
        return;
      if ((vf & 2) && *(int *)((char *)obj + 0x1b8) == 0x3f800000)
        return;
    }

    if (*(int *)(tag_data + 0x7c) == -1)
      return;

    col_tag_data = (int)tag_get(0x636f6c6c, *(int *)(tag_data + 0x7c));
    object_get_world_matrix(param_1, world_matrix);
    coll_block = (int *)(col_tag_data + 0x280);
    i = 0;
    if (*coll_block > 0) {
      int idx = 0;
      do {
        void *coll_elem;
        short elem_node;
        coll_elem = tag_block_get_element(coll_block, idx, 0x20);
        elem_node = *(short *)coll_elem;
        if (elem_node == -1) {
          matrix_transform_point(
            world_matrix, (float *)((char *)coll_elem + 0x10), point_out);
        } else {
          void *nm;
          nm = object_get_node_matrix(param_1, elem_node);
          matrix_transform_point(
            (float *)nm, (float *)((char *)coll_elem + 0x10), point_out);
        }
        FUN_00189540(1, point_out, *(float *)((char *)coll_elem + 0x1c),
                     *(void **)0x2ee6d8);
        i++;
        idx = (int)i;
      } while (idx < *coll_block);
      col_tag_data = (int)tag_get(0x636f6c6c, *(int *)(tag_data + 0x7c));
    }

    x_range = (float *)(col_tag_data + 0x268);
    if (x_range[0] < x_range[1]) {
      y_range = (float *)(col_tag_data + 0x270);
      if (y_range[0] < y_range[1]) {
        z_range = (float *)(col_tag_data + 0x278);
        if (z_range[0] < z_range[1]) {
          {
            float *dst = bbox_corners;
            float *xr = x_range;
            int xc;
            for (xc = 0; xc < 2; xc++) {
              float *yr = y_range;
              int yc;
              for (yc = 0; yc < 2; yc++) {
                float *zr = z_range;
                int zc;
                for (zc = 0; zc < 2; zc++) {
                  dst[0] = *xr;
                  dst[1] = *yr;
                  dst[2] = *zr;
                  matrix_transform_point(world_matrix, dst, dst);
                  zr++;
                  dst += 3;
                }
                yr++;
              }
              xr++;
            }
          }

          for (bi = 0; bi < 2; bi++) {
            for (bj = 0; bj < 2; bj++) {
              for (bk = 0; bk < 2; bk++) {
                if (bi == 0) {
                  int e = bk + bj * 2;
                  FUN_00189270(1, &bbox_corners[e * 3],
                               &bbox_corners[(e + 4) * 3], *(void **)0x2ee6d8);
                }
                if (bj == 0) {
                  int e = bk + bi * 4;
                  FUN_00189270(1, &bbox_corners[e * 3],
                               &bbox_corners[(e + 2) * 3], *(void **)0x2ee6d8);
                }
                if (bk == 0) {
                  int e = bj + bi * 2;
                  FUN_00189270(1, &bbox_corners[e * 6],
                               &bbox_corners[e * 6 + 3], *(void **)0x2ee6d8);
                }
              }
            }
          }
        }
      }
    }
  }
}

/* attachments_delete — delete object attachments (effects, sounds, lights,
 * etc.).
 *
 * Iterates through the object's attachment slots (up to tag+0x140 count)
 * and dispatches cleanup calls based on attachment type:
 *   Type 0: light_delete (effect cleanup)
 *   Type 1: game_looping_sound_delete (sound cleanup)
 *   Type 2: effect_delete (decal cleanup)
 *   Type 3: object_compute_node_matrices + contrail_owner_collision (light
 * cleanup) Type 4: particle_system_orphan (contrail cleanup)
 *
 * Object attachment structure:
 *   obj+0xf4 to obj+0xf4+count: attachment type bytes (-1 = empty)
 *   obj+0xfc + index*4: attachment handle (int)
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (handle, -1).
 * Confirmed: CALL 0x1ba140 (tag_get) with ('obje', obj[0]).
 * Confirmed: switch jump table at 0x143ac0 for 5 cases.
 */
void attachments_delete(int object_handle)
{
  int *obj;
  char *tag;
  int16_t i;
  char type;
  int attachment_handle;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  tag = (char *)tag_get(0x6f626a65, obj[0]);

  for (i = 0; i < *(int *)(tag + 0x140); i++) {
    type = *((char *)obj + 0xf4 + (int)i);
    if (type == -1)
      continue;

    attachment_handle = *(int *)((char *)obj + 0xfc + (int)i * 4);
    if (attachment_handle == -1)
      continue;

    switch (type) {
    case 0:
      light_delete(attachment_handle);
      break;
    case 1:
      game_looping_sound_delete(attachment_handle);
      break;
    case 2:
      effect_delete(attachment_handle);
      break;
    case 3:
      object_compute_node_matrices(object_handle);
      /* Force a post-call reload; EAX is clobbered by the matrices call. */
      attachment_handle = *(int *)((char *)obj + 0xfc + (int)i * 4);
      contrail_owner_collision(attachment_handle, 1, 0);
      break;
    case 4:
      particle_system_orphan(attachment_handle);
      break;
    }
  }
}

/* object_set_position — reposition an object's position and facing.
 *
 * Disconnects the object from the map, optionally updates its position
 * (forward vector at obj+0x0C) and facing direction (at obj+0x24).
 * If a target (up) vector is provided, it is copied directly to obj+0x30.
 * Otherwise, a perpendicular up vector is computed from the facing via:
 *   temp = {facing.y, -facing.x, 0.0}
 *   normalize(temp)
 *   if degenerate: temp = {1, 0, 0}
 *   up = cross(temp, facing)
 * Then recomputes node matrices and reconnects to the map.
 *
 * Confirmed: 4 cdecl args (object_handle, facing, target, flags).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (handle, -1).
 * Confirmed: CALL 0x13fd00 (object_disconnect_from_map) with 1 stack arg.
 * Confirmed: CALL 0x13010 (normalize3d) for perpendicular temp vector.
 * Confirmed: cross product computed via x87 FPU in-line (not a function call).
 * Confirmed: CALL 0x141b70 (object_compute_node_matrices).
 * Confirmed: CALL 0x140ce0 (object_connect_to_map) with (handle, 0).
 * Confirmed: FCOMP against REAL_ZERO_POOL (0.0f) for degenerate check.
 */
void object_set_position(int object_handle, float *position, float *forward,
                         float *up)
{
  char *obj;
  float temp[3];
  float mag;
  float fwd0;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  object_disconnect_from_map(object_handle);

  /* Copy position if provided */
  if (position != NULL) {
    *(float *)(obj + 0x0c) = position[0];
    *(float *)(obj + 0x10) = position[1];
    *(float *)(obj + 0x14) = position[2];
  }

  /* Copy forward direction and compute/set up vector */
  if (forward != NULL) {
    fwd0 = forward[0];
    *(float *)(obj + 0x24) = fwd0;
    *(float *)(obj + 0x28) = forward[1];
    *(float *)(obj + 0x2c) = forward[2];

    if (up != NULL) {
      /* Up vector provided directly */
      *(float *)(obj + 0x30) = up[0];
      *(float *)(obj + 0x34) = up[1];
      *(float *)(obj + 0x38) = up[2];
    } else {
      /* Compute perpendicular up from forward direction:
       * temp = {forward.y, -forward.x, 0.0} */
      temp[0] = forward[1];
      temp[1] = -fwd0;
      temp[2] = 0.0f;

      mag = normalize3d(temp);
      if (REAL_ZERO_POOL == mag) {
        /* Degenerate (forward is along Z) — use X axis */
        temp[0] = 1.0f;
        temp[1] = 0.0f;
        temp[2] = 0.0f;
      }

      /* up = cross(temp, forward) */
      *(float *)(obj + 0x30) = temp[1] * forward[2] - temp[2] * forward[1];
      *(float *)(obj + 0x34) = temp[2] * fwd0 - temp[0] * forward[2];
      *(float *)(obj + 0x38) = temp[0] * forward[1] - temp[1] * fwd0;
    }
  }

  object_compute_node_matrices(object_handle);
  object_connect_to_map(object_handle, 0);
}

/* object_translate — set an object's position and reconnect it to the map.
 *
 * Validates the new position with valid_real_point3d, asserts if invalid.
 * Disconnects the object from the BSP, copies the 3D position into the
 * object data at offset +0x0C, then reconnects with the given location.
 *
 * The assert string identifies this as "new_position" in objects.c line 0x232.
 *
 * Confirmed: 3 cdecl args (object_handle, position, location).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (handle, -1).
 * Confirmed: CALL 0xa16b0 (valid_real_point3d) for point validation.
 * Confirmed: CALL 0x8d9d0 (csprintf) for assert message formatting.
 * Confirmed: CALL 0x8d9f0 (display_assert) with file/line left on stack.
 * Confirmed: CALL 0x8e2f0 (system_exit) with -1.
 * Confirmed: CALL 0x13fd00 (object_disconnect_from_map).
 * Confirmed: CALL 0x140ce0 (object_connect_to_map) with (handle, location).
 * Confirmed: position copied to obj+0x0C, obj+0x10, obj+0x14.
 */
void object_translate(int object_handle, float *position, void *location)
{
  char *obj;

#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_OBJECT_TRANSLATE_POS_XY,
               RNG_TRACE_BITS(position[0]), RNG_TRACE_BITS(position[1]));
  RNG_TRACE_EX(RNG_TRACE_KIND_OBJECT_TRANSLATE_POS_Z_HANDLE,
               RNG_TRACE_BITS(position[2]), (unsigned int)object_handle);
#endif
  obj = (char *)object_get_and_verify_type(object_handle, -1);
  if (!valid_real_point3d(position)) {
    char *msg;
    display_assert(csprintf((char *)0x5ab100,
                            "%s: assert_valid_real_point3d(%f, %f, %f)",
                            "new_position", (double)position[0],
                            (double)position[1], (double)position[2]),
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x232, 1);
    system_exit(-1);
  }
  object_disconnect_from_map(object_handle);
  /* object->object.position = *new_position (PAL 2342 objects.c:3231):
   * one 12-byte aggregate copy through the obj+0x0c address. */
  *(real_point3d *)(obj + 0x0c) = *(real_point3d *)position;
#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_OBJECT_TRANSLATE_PRE_CONNECT_XY,
               RNG_TRACE_BITS(*(float *)(obj + 0x0c)),
               RNG_TRACE_BITS(*(float *)(obj + 0x10)));
  RNG_TRACE_EX(RNG_TRACE_KIND_OBJECT_TRANSLATE_PRE_CONNECT_Z_HANDLE,
               RNG_TRACE_BITS(*(float *)(obj + 0x14)),
               (unsigned int)object_handle);
#endif
  object_connect_to_map(object_handle, location);
#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_OBJECT_TRANSLATE_EXIT_XY,
               RNG_TRACE_BITS(*(float *)(obj + 0x0c)),
               RNG_TRACE_BITS(*(float *)(obj + 0x10)));
  RNG_TRACE_EX(RNG_TRACE_KIND_OBJECT_TRANSLATE_EXIT_Z_HANDLE,
               RNG_TRACE_BITS(*(float *)(obj + 0x14)),
               (unsigned int)object_handle);
#endif
}

/*
 * object_new — create a new object from a placement data struct.
 *
 * This is the core object creation function. Validates the placement data,
 * allocates a datum in the object header table, initialises the object's
 * fields from the placement struct and the object definition tag, then runs
 * the full chain of type-specific initialisers, node matrix computation,
 * map connection, widget creation, and child attachment.
 *
 * On failure (no free slots or type init failure), the datum is freed and
 * an "OUT OF OBJECTS" error is logged. Returns -1 on failure, or the new
 * object's datum handle on success.
 *
 * Confirmed: SUB ESP,0x210 — 528 bytes of locals (includes 512-byte sprintf
 * buffer). Confirmed: tag_get(0x6f626a65, tag_index) for 'obje' tag. Confirmed:
 * object_header_new with EAX=-1 for datum allocation. Confirmed: datum_get +
 * object_get_and_verify_type for header/obj access. Confirmed: header->unk_2 |=
 * 0x44 sets active+type flags. Confirmed: position += scale * up_vector
 * (placement+0x24 multiplied through). Confirmed: tag_get(0x6d6f6465, ...) for
 * 'mode' model tag, node count at +0xb8. Confirmed:
 * object_header_block_allocate for block reference allocation (3 calls).
 * Confirmed: effect_new_from_object creation effect (8 args) if tag_data+0xac
 * != -1. Confirmed: return EBX (object_handle or -1).
 */
int object_new(void *placement)
{
  char *p = (char *)placement;
  int tag_index;
  char *tag_data;
  char *obj;
  char *header;
  int object_handle;
  uint32_t node_count;
  uint8_t saved_active_bit;
  uint8_t success;
  char local_buf[512];

  tag_index = *(int *)p;

  /* --- Validation: position --- */
  if (!((valid_real_point3d_fn)0xa16b0)((float *)(p + 0x18))) {
    char *msg =
      csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
               "&data->position", (double)*(float *)(p + 0x18),
               (double)*(float *)(p + 0x1c), (double)*(float *)(p + 0x20));
    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x26a, 1);
    system_exit(-1);
  }

  /* --- Validation: forward/up axes perpendicularity --- */
  if (!valid_real_normal3d_perpendicular((float *)(p + 0x34),
                                         (float *)(p + 0x40))) {
    char *msg = csprintf(
      (char *)0x5ab100,
      "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
      "&data->forward", "&data->up", (double)*(float *)(p + 0x34),
      (double)*(float *)(p + 0x38), (double)*(float *)(p + 0x3c),
      (double)*(float *)(p + 0x40), (double)*(float *)(p + 0x44),
      (double)*(float *)(p + 0x48));
    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x26b, 1);
    system_exit(-1);
  }

  /* --- Validation: angular velocity --- */
  if (!real_vector3d_valid((float *)(p + 0x4c))) {
    char *msg =
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&data->angular_velocity", (double)*(float *)(p + 0x4c),
               (double)*(float *)(p + 0x50), (double)*(float *)(p + 0x54));
    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x26c, 1);
    system_exit(-1);
  }

  /* --- Validation: translational velocity --- */
  if (!real_vector3d_valid((float *)(p + 0x28))) {
    char *msg =
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&data->translational_velocity", (double)*(float *)(p + 0x28),
               (double)*(float *)(p + 0x2c), (double)*(float *)(p + 0x30));
    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x26d, 1);
    system_exit(-1);
  }

  /* --- Game engine tag remapping --- */
  if (game_engine_running()) {
    if (tag_index == -1)
      return -1;
    tag_index = game_engine_remap_object_definition(tag_index);
  }

  if (tag_index == -1)
    return -1;

  /* --- Get object definition tag --- */
  tag_data = (char *)tag_get(0x6f626a65, tag_index);

  /* --- Get type definition and allocate datum --- */
  {
    uint16_t type_word = *(uint16_t *)tag_data;
    char *type_def = (char *)object_type_definition_get((int16_t)type_word);
    int16_t datum_size = *(int16_t *)(type_def + 0x8);

    object_handle = object_header_new(*(data_t **)0x5a8d50, datum_size, -1);
  }

  if (object_handle == -1)
    goto out_of_objects;

  /* --- Get header and object pointers --- */
  header = (char *)datum_get(*(data_t **)0x5a8d50, object_handle);
  obj = (char *)object_get_and_verify_type(object_handle, -1);

  /* Set header flags and type */
  *(uint8_t *)(header + 0x2) |= 0x44;
  *(uint8_t *)(header + 0x3) = *(uint8_t *)tag_data;

  /* Store tag index and type in object */
  *(int *)obj = tag_index;
  success = 1;
  *(int16_t *)(obj + 0x64) = *(int16_t *)tag_data;

  /* --- Type-specific init callback (object_type_adjust_placement) --- */
  object_type_adjust_placement(object_handle, placement);

  /* --- Copy fields from placement to object --- */
  /* position: placement+0x18 → obj+0x0c */
  *(float *)(obj + 0x0c) = *(float *)(p + 0x18);
  *(float *)(obj + 0x10) = *(float *)(p + 0x1c);
  *(float *)(obj + 0x14) = *(float *)(p + 0x20);

  /* forward: placement+0x34 → obj+0x24 */
  *(float *)(obj + 0x24) = *(float *)(p + 0x34);
  *(float *)(obj + 0x28) = *(float *)(p + 0x38);
  *(float *)(obj + 0x2c) = *(float *)(p + 0x3c);

  /* up: placement+0x40 → obj+0x30 */
  *(float *)(obj + 0x30) = *(float *)(p + 0x40);
  *(float *)(obj + 0x34) = *(float *)(p + 0x44);
  *(float *)(obj + 0x38) = *(float *)(p + 0x48);

  /* velocity: placement+0x28 → obj+0x18 */
  *(float *)(obj + 0x18) = *(float *)(p + 0x28);
  *(float *)(obj + 0x1c) = *(float *)(p + 0x2c);
  *(float *)(obj + 0x20) = *(float *)(p + 0x30);

  /* angular velocity: placement+0x4c → obj+0x3c */
  *(float *)(obj + 0x3c) = *(float *)(p + 0x4c);
  *(float *)(obj + 0x40) = *(float *)(p + 0x50);
  *(float *)(obj + 0x44) = *(float *)(p + 0x54);

  /* position += scale * up_vector (placement+0x24 is the scale factor) */
  {
    float scale = *(float *)(p + 0x24);
    *(float *)(obj + 0x0c) += scale * *(float *)(obj + 0x30);
    *(float *)(obj + 0x10) += scale * *(float *)(obj + 0x34);
    *(float *)(obj + 0x14) += scale * *(float *)(obj + 0x38);
  }

  /* --- Set flag bit 12 based on placement flags bit 0 --- */
  {
    uint32_t flags = *(uint32_t *)(obj + 0x4);
    if (*(uint8_t *)(p + 0x4) & 0x1)
      flags |= 0x1000;
    else
      flags &= ~(uint32_t)0x1000;
    *(uint32_t *)(obj + 0x4) = flags;
  }

  /* --- Initialise various fields to defaults --- */
  *(int16_t *)(obj + 0x4c) = -1;
  *(int16_t *)(header + 0x4) = -1;
  *(uint32_t *)(obj + 0x8) = *(uint32_t *)0x5a8d28 - 1;
  *(int *)(obj + 0xa0) = -1;
  *(int *)(obj + 0xbc) = -1;
  *(int16_t *)(obj + 0x80) = -1;
  *(int *)(obj + 0x7c) = *(int *)(tag_data + 0x44);
  *(int *)(obj + 0x120) = -1;
  *(int *)(obj + 0xcc) = -1;
  *(int *)(obj + 0xc4) = -1;
  *(int *)(obj + 0xc8) = -1;
  *(int16_t *)(obj + 0x6a) = -1;
  *(int *)(obj + 0xac) = -1;
  *(int *)(obj + 0xb0) = -1;

  /* --- Tag flag propagation --- */
  if (*(uint8_t *)(tag_data + 0x2) & 0x1)
    *(uint32_t *)(obj + 0x4) |= 0x40000;

  {
    uint32_t oflags = *(uint32_t *)(obj + 0x4);
    if (*(int *)(tag_data + 0x7c) != -1)
      oflags |= 0x2000000;
    else
      oflags &= ~(uint32_t)0x2000000;
    *(uint32_t *)(obj + 0x4) = oflags;
  }

  /* --- Set garbage flag (1 if model tag exists, 0 if not) --- */
  object_set_garbage(object_handle, (uint8_t)(*(int *)(tag_data + 0x34) != -1));

  /* --- Copy remaining placement fields --- */
  *(int16_t *)(obj + 0x68) = *(int16_t *)(p + 0x14);
  *(int *)(obj + 0x70) = *(int *)(p + 0x08);
  *(int *)(obj + 0x74) = *(int *)(p + 0x0c);
  *(int16_t *)(obj + 0x6e) = *(int16_t *)(p + 0x16);
  *(int16_t *)(obj + 0x126) = *(int16_t *)(tag_data + 0x13e);

  /* --- Get node count from model tag --- */
  if (*(int *)(tag_data + 0x34) == -1) {
    node_count = 1;
  } else {
    char *model_tag = (char *)tag_get(0x6d6f6465, *(int *)(tag_data + 0x34));
    node_count = *(uint16_t *)(model_tag + 0xb8);
  }

  /* --- Allocate block references for node matrices --- */
  if (!object_header_block_allocate(object_handle, 0x1a0, node_count * 0x34)) {
    success = 0;
  } else if (((1 << (*(uint8_t *)tag_data & 0x1f)) & 0xfe0) == 0) {
    /* Non-standard types need additional allocations */
    if (!object_header_block_allocate(object_handle, 0x19c, node_count << 5) ||
        !object_header_block_allocate(object_handle, 0x198, node_count << 5)) {
      success = 0;
    }
  }

  /* --- Re-acquire object pointer (may have moved due to allocation) --- */
  obj = (char *)object_get_and_verify_type(object_handle, -1);

  if (success && object_type_new(object_handle)) {
    /* --- Save and optionally clear the active (bit 19) flag --- */
    saved_active_bit = (uint8_t)((*(uint32_t *)(obj + 0x4) >> 19) & 1);
    if (saved_active_bit && (*(uint8_t *)(p + 0x4) & 0x2)) {
      *(uint32_t *)(obj + 0x4) &= ~(uint32_t)0x80000;
    }

    /* --- Run initialisation chain --- */
    object_choose_random_change_colors(object_handle, p + 0x58);
    object_choose_random_region_permutations(object_handle);
    object_initialize_vitality(object_handle, 0, 0);
    object_compute_node_matrices(object_handle);
    object_connect_to_map(object_handle, 0);
    object_postprocess_node_matrices(object_handle);
    object_type_export_function_values(object_handle);
    object_compute_function_values(object_handle);
    object_compute_change_colors(object_handle);

    /* --- Widget and child attachment --- */
    {
      char *obj2 = (char *)object_get_and_verify_type(object_handle, -1);
      int obj_tag_idx = *(int *)obj2;
      tag_get(0x6f626a65, obj_tag_idx);
      widgets_new(object_handle);
    }
    attachments_new(object_handle);

    /* --- Restore the active flag --- */
    obj = (char *)object_get_and_verify_type(object_handle, -1);
    {
      uint32_t flags2 = *(uint32_t *)(obj + 0x4);
      if (saved_active_bit)
        flags2 |= 0x80000;
      else
        flags2 &= ~(uint32_t)0x80000;
      *(uint32_t *)(obj + 0x4) = flags2;
    }

    /* --- Conditionally wake the object --- */
    if ((*(uint8_t *)(header + 0x2) & 0x1) == 0 &&
        (*(uint32_t *)(obj + 0x4) & 0x80000) != 0 &&
        ((*(uint8_t *)(p + 0x4) & 0x2) == 0 ||
         *(int16_t *)(obj + 0x4c) != -1)) {
      object_delete(object_handle);
    }

    /* --- Creation effect --- */
    if (*(int *)(tag_data + 0xac) != -1) {
      effect_new_from_object(*(int *)(tag_data + 0xac),
                             object_handle, /* dup-args-ok */
                             object_handle, -1, 0, 0, 0, 0);
    }

    return object_handle;
  }

  /* --- Failure: free the allocated datum --- */
  object_type_delete(object_handle);
  object_header_delete(*(data_t **)0x5a8d50, object_handle);
  object_handle = -1;

out_of_objects: {
  const char *name = tag_name_strip_path(tag_get_name(tag_index));
  crt_sprintf(local_buf, "OUT OF OBJECTS: cannot create %s", name);
  console_printf(0, "%s", local_buf);
  error(3, "%s", local_buf);
}
  return object_handle;
}

/*
 * object_attach_to_parent — attach a child object to a parent at a specific
 * node, establishing the parent-child relationship in the object hierarchy.
 *
 * First walks the parent's own parent chain to verify the child is not already
 * an ancestor of the parent (prevents circular attachment). Then computes the
 * inverse of the parent's node matrix and transforms the child's position,
 * up vector, and forward vector into the parent node's local coordinate space.
 * Stores the parent handle and node index in the child's object data
 * (offsets 0xCC and 0xD0). If the child was connected to the map (flag bit 11
 * of object_data_t.flags), it is disconnected before the transform and
 * reconnected afterward.
 *
 * Finally clears the "collideable" flag (bit 0) on the child's header if set,
 * sets the "updated this tick" flag (bit 4), and recomputes node matrices.
 *
 * Confirmed: 3 cdecl args (parent_handle, child_handle, parent_node_index).
 * Confirmed: CALL targets 0x13d680, 0x13fef0, 0x13fd00, 0x140eb0, 0x109150,
 *            0x109590, 0x109680, 0x140ce0, 0x119320, 0x141b70.
 * Confirmed: parent chain walk uses parent_object_index at offset 0xCC.
 * Confirmed: stores parent_handle at child+0xCC, node_index byte at child+0xD0.
 * Confirmed: flag test is (flags >> 0xB) & 1 — bit 11 of object_data_t.flags.
 * Inferred:  bit 11 means "connected to map" based on disconnect/reconnect
 * usage.
 */
void object_attach_to_parent(int parent_handle, int child_handle,
                             int parent_node_index)
{
  int iter;
  object_data_t *child_obj;
  uint8_t connected_to_map;
  float local_matrix[13]; /* 4x3 matrix = 52 bytes */
  float *node_mat;
  object_header_data_t *child_hdr;

  iter = parent_handle;

  /* Walk the parent chain to verify we are not creating a cycle. */
  while (iter != -1) {
    object_header_data_t *hdr =
      (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, iter);
    object_data_t *obj_iter = hdr->object;
    int16_t obj_type = obj_iter->type;

    if ((1 << ((uint8_t)obj_type & 0x1f)) == 0) {
      display_assert(csprintf((char *)0x5ab100,
                              "got an object type we didn't expect "
                              "(expected one of 0x%08x but got #%d).",
                              -1, (int)obj_type),
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
      system_exit(-1);
    }

    if (iter == child_handle)
      break;

    iter = obj_iter->parent_object_index.value;
  }

  if (iter != -1) {
    display_assert("cannot attach an object to one of its children",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x4c9, 1);
    system_exit(-1);
    return;
  }

  /* Get child and parent object pointers. */
  child_obj = (object_data_t *)object_get_and_verify_type(child_handle, -1);
  object_get_and_verify_type(parent_handle, -1);

  connected_to_map = (uint8_t)((child_obj->flags >> 0xB) & 1);

  if (!object_has_node(parent_handle, (int16_t)parent_node_index)) {
    display_assert("object_has_node(parent_object_index, parent_node_index)",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x4d3, 1);
    system_exit(-1);
  }

  /* Disconnect child from map if it was connected. */
  if (connected_to_map) {
    object_disconnect_from_map(child_handle);
  }

  /* Compute inverse of the parent node matrix, then transform the child's
     position, up, and forward vectors into the parent node's local space. */
  node_mat =
    (float *)object_get_node_matrix(parent_handle, (int16_t)parent_node_index);
  matrix_inverse(node_mat, local_matrix);
  matrix_transform_point(local_matrix,
                         (float *)&child_obj->position, /* dup-args-ok */
                         (float *)&child_obj->position);
  matrix_transform_vector(local_matrix,
                          (float *)&child_obj->forward, /* dup-args-ok */
                          (float *)&child_obj->forward);
  matrix_transform_vector(local_matrix,
                          (float *)&child_obj->up, /* dup-args-ok */
                          (float *)&child_obj->up);

  /* Store parent attachment info in the child object. */
  child_obj->parent_object_index.value = parent_handle;
  *(uint8_t *)((char *)child_obj + 0xD0) = (uint8_t)parent_node_index;

  /* Reconnect child to map if it was connected. */
  if (connected_to_map) {
    object_connect_to_map(child_handle, NULL);
  }

  /* Update child header flags. */
  child_hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, child_handle);
  object_get_and_verify_type(child_handle, -1);

  if (child_hdr->unk_2 & 0x01) {
    child_hdr->unk_2 &= 0xfe;
  }

  child_hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, child_handle);
  child_hdr->unk_2 |= 0x10;

  object_compute_node_matrices(child_handle);
}

/*
 * object_try_place — attempt to place an object at a new position by casting
 * a collision ray from the object's current position toward the target
 * position.
 *
 * Pushes a collision user stack entry (user=0x13), computes the delta vector
 * (current_pos - target_pos), then calls FUN_0014df70 to perform a collision
 * test along that ray. If the collision test succeeds or the object has no
 * current cluster placement (field 0x4c == -1), the function checks the
 * collision result for a valid surface. If a valid surface is found, it calls
 * object_translate to update the object's position and reconnect it to the map,
 * then recomputes node matrices. Returns true if the object was placed or
 * already had a valid cluster reference, false otherwise.
 *
 * Confirmed: cdecl, 2 stack args — PUSH position, PUSH handle before CALL.
 * Confirmed: returns bool in AL (callers TEST AL,AL after CALL).
 * Confirmed: collision_result buffer is 0x50 bytes (int16_t[40]).
 * Confirmed: collision user ID 0x13 pushed to stack at 0x5a8c80.
 * Confirmed: assert strings match "objects.c" at lines 0x93d and 0x953.
 */
bool object_try_place(int object_handle, float *position)
{
  char *obj;
  char *col_surface_ptr;
  volatile unsigned char zero_init;
  bool result;
  int16_t collision_result[40]; /* 0x50 bytes at EBP-0x5c */
  float delta[3]; /* 3 floats at EBP-0x0c */

  zero_init = 0;
  obj = (char *)object_get_and_verify_type(object_handle, -1);
#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_TRY_PLACE_IN_POS_XY,
               RNG_TRACE_BITS(*(float *)(obj + 0x0c)),
               RNG_TRACE_BITS(*(float *)(obj + 0x10)));
  RNG_TRACE_EX(RNG_TRACE_KIND_TRY_PLACE_IN_POS_Z_HANDLE,
               RNG_TRACE_BITS(*(float *)(obj + 0x14)),
               (unsigned int)object_handle);
  RNG_TRACE_EX(RNG_TRACE_KIND_TRY_PLACE_TARGET_XY, RNG_TRACE_BITS(position[0]),
               RNG_TRACE_BITS(position[1]));
  RNG_TRACE_EX(RNG_TRACE_KIND_TRY_PLACE_TARGET_Z_HANDLE,
               RNG_TRACE_BITS(position[2]), (unsigned int)object_handle);
#endif
  result = zero_init;

  /* Push collision user stack entry (user = 0x13). */
  if (*(volatile int16_t *)0x4761d8 >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x93d, true);
    system_exit(-1);
  }
  {
    int depth = (int)*(volatile int16_t *)0x4761d8;
    *(volatile int16_t *)0x4761d8 += 1;
    *(int16_t *)(0x5a8c80 + depth * 2) = 0x13;
  }

  /* Compute delta vector: current_position - target_position. */
  col_surface_ptr = (char *)collision_result + 0x10;
  delta[0] = *(float *)(obj + 0x0c) - position[zero_init];
  delta[1] = *(float *)(obj + 0x10) - position[1];
  delta[2] = *(float *)(obj + 0x14) - position[2];

  /* Cast collision ray from target position along delta direction. */
  if (FUN_0014df70(0x1000e9, position, delta, -1, collision_result) ||
      *(int16_t *)(obj + 0x4c) == -1) {
    /* Collision found or object has no cluster placement. */
    if (*(int16_t *)col_surface_ptr == -1) {
      /* No valid surface in collision result — cannot place. */
      goto done;
    }
    /* Place object at collision surface position and reconnect to map. */
    object_translate(object_handle, (float *)((char *)collision_result + 0x18),
                     (void *)((char *)collision_result + 0x0c));
    object_compute_node_matrices(object_handle);
  }
  result = true;

done:
  /* Pop collision user stack entry. */
  if (*(volatile int16_t *)0x4761d8 <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x953, true);
    system_exit(-1);
  }
  *(volatile int16_t *)0x4761d8 -= 1;

#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_TRY_PLACE_OUT_POS_XY,
               RNG_TRACE_BITS(*(float *)(obj + 0x0c)),
               RNG_TRACE_BITS(*(float *)(obj + 0x10)));
  RNG_TRACE_EX(RNG_TRACE_KIND_TRY_PLACE_OUT_POS_Z_RESULT,
               RNG_TRACE_BITS(*(float *)(obj + 0x14)),
               (unsigned int)(unsigned char)result);
  RNG_TRACE_EX(RNG_TRACE_KIND_TRY_PLACE_COLLISION_TYPE_T,
               (unsigned int)(int)collision_result[0],
               RNG_TRACE_BITS(*(float *)((char *)collision_result + 0x14)));
  RNG_TRACE_EX(RNG_TRACE_KIND_TRY_PLACE_COLLISION_OBJECT_SURFACE,
               *(unsigned int *)((char *)collision_result + 0x38),
               (unsigned int)(unsigned short)*(
                 int16_t *)((char *)collision_result + 0x34));
#endif
  return result;
}

/*
 * object_update (0x1444f0) — per-tick update for a single object node and its
 * child/sibling subtree.
 *
 * Resolves the object header (datum_get on the object header table at 0x5a8d50)
 * and the object data (object_get_and_verify_type), plus the 'obje' definition
 * tag. If the header is not marked deactivated (flag 0x10 at header+0x2):
 *   - if flags&0x10000, bump the global object-update counter
 *     (*(int*)0x46f084 -> object_globals, int16 counter at +4).
 *   - if the interpolation period (obj+0x86) is non-zero, advance the
 *     interpolation tick (obj+0x84) and clear the period once it elapses.
 *     Asserts the object type is interpolatable (mask 0xfe0 over
 * 1<<(type&0x1f)).
 *   - runs object_type_update, damage update (if tag+0x7c != -1),
 * object_type_export_function_values, node matrices (unless flags&0x800000),
 * function values, change colors.
 *   - propagates a flag to children when flags&0x2000 and either flags&1 is
 *     clear or the definition's tag+0x34 == -1.
 *   - recurses into the first child (obj+0xC8) and, when a parent link
 *     (obj+0xCC) and sibling link (obj+0xC4) both exist, into the next sibling.
 *   - re-fetches obj/tag after recursion (registers reloaded in the original)
 *     and, if tag+0x34 and tag+0x44 are both != -1, resolves a header block
 *     reference (obj+0x1A0) and hands it to
 * object_type_postprocess_node_matrices. Always returns true (AL=1).
 *
 * Confirmed: header via datum_get(*(data_t**)0x5a8d50, handle); obj via
 * object_get_and_verify_type(handle, -1); tag via tag_get(0x6f626a65, obj[0]).
 * Confirmed: TEST byte[hdr+2],0x10 gates the whole body (JNE -> return 1).
 * Confirmed: INC word[*(int*)0x46f084 + 4] under TEST dword[obj+4],0x10000.
 * Confirmed: interp counter/period are int16 (INC/CMP word
 * [esi+0x84]/[esi+0x86]). Confirmed: display_assert(...,0x9cc,1) then
 * system_exit(-1) on bad type. Confirmed: EAX-passed callees
 * (function_values/change_colors/propagate) via MOV EAX,EDI before CALL —
 * @<eax> in kb.json. Confirmed: self-recursion CALL 0x1444f0 for obj+0xC8 and
 * obj+0xC4. Confirmed: block ref path adds 0x1A0 to obj (obj+0x68 in int*
 * terms).
 */
bool object_update(int object_handle)
{
  object_header_data_t *header;
  object_data_t *obj;
  void *tag_def;

  header =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  tag_def = tag_get(0x6f626a65, (int)obj->tag_index);

  if ((header->unk_2 & 0x10) == 0) {
    if ((obj->flags & 0x10000) != 0) {
      /* active_garbage_object_count: PAL 2342 objects.c:3585 */
      ++*(short *)(*(int *)0x46f084 + 4);
    }

    if (obj->unk_134 != 0) {
      if (((1 << obj->type) & 0xfe0) != 0) {
        display_assert(
          "!TEST_FLAG(_object_mask_cannot_interpolate, object->object.type)",
          "c:\\halo\\SOURCE\\objects\\objects.c", 0x9cc, 1);
        system_exit(-1);
      }
      obj->unk_132 = (int16_t)(obj->unk_132 + 1);
      if (obj->unk_132 >= obj->unk_134) {
        obj->unk_134 = 0;
      }
    }

    object_type_update(object_handle);
    if (*(int *)((char *)tag_def + 0x7c) != -1) {
      object_damage_update(object_handle);
    }
    object_type_export_function_values(object_handle);
    if ((obj->flags & 0x800000) == 0) {
      object_compute_node_matrices(object_handle);
    }
    object_compute_function_values(object_handle);
    object_compute_change_colors(object_handle);

    if ((obj->flags & 0x2000) != 0) {
      if ((obj->flags & 1) == 0 ||
          *(int *)((char *)tag_get(0x6f626a65, (int)obj->tag_index) + 0x34) ==
            -1) {
        object_propagate_flag_to_children(object_handle, 1, 1);
      }
    }

    if (obj->unk_200.value != -1) {
      object_update(obj->unk_200.value);
    }
    if (obj->parent_object_index.value != -1 &&
        obj->next_object_index.value != -1) {
      object_update(obj->next_object_index.value);
    }

    /* Re-fetch obj/tag: the original reloads ESI/EAX after the recursive
     * calls (object_get_and_verify_type + tag_get) before the block-ref step.
     */
    obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
    tag_def = tag_get(0x6f626a65, (int)obj->tag_index);
    if (*(int *)((char *)tag_def + 0x34) != -1 &&
        *(int *)((char *)tag_def + 0x44) != -1) {
      void *block_ref =
        object_header_block_reference_get(object_handle, &obj->unk_416);
      object_type_postprocess_node_matrices(object_handle, block_ref);
    }
  }

  return 1;
}

/*
 * object_update_children_recursive — recursively compute node matrices for an
 * object and all of its child objects.
 *
 * First computes the node matrices for the given object by calling
 * object_compute_node_matrices (0x141b70), then walks the child chain starting
 * at object_data+0xC8 (first child handle). For each child, verifies type via
 * datum_get + type check, recurses, then advances via next_object_index
 * (object_data+0xC4).
 *
 * Confirmed: CALL 0x13d680 with args (-1, handle) — object_get_and_verify_type.
 * Confirmed: CALL 0x141b70 with 1 arg (handle) — object_compute_node_matrices.
 * Confirmed: MOV ESI,[EDI+0xC8] — first child from object data.
 * Confirmed: datum_get(*(data_t**)0x5a8d50, child_handle) for child lookup.
 * Confirmed: MOVSX ECX,word ptr [EDI+0x64] — child object type (int16_t).
 * Confirmed: MOV ESI,[EDI+0xC4] — next sibling from child object data.
 * Confirmed: recursive self-call at 0x144719.
 */
void object_update_children_recursive(int object_handle)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  int child_handle;

  /* compute node matrices for this object */
  object_compute_node_matrices(object_handle);

  /* walk the child object chain */
  child_handle = obj->unk_200.value;
  while (child_handle != -1) {
    object_header_data_t *child_header =
      (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, child_handle);
    object_data_t *child_obj = child_header->object;

    if ((1 << child_obj->type) == 0) {
      char *msg =
        csprintf((char *)0x5ab100,
                 "got an object type we didn't expect (expected one of "
                 "0x%08x but got #%d).",
                 -1, (int)child_obj->type);
      display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
      system_exit(-1);
    }

    object_update_children_recursive(child_handle);
    child_handle = child_obj->next_object_index.value;
  }
}

/* 0x144770 / objects.obj — Create an object from a scenario palette entry.
 * Looks up the palette element, initializes placement data with its tag,
 * copies position and orientation from the placement data, then calls
 * object_new. On success, links the object to the scenario via
 * object_type_place and optionally adds it to the name list.
 * cdecl, 2 params.
 * Confirmed: SUB ESP,0x88 — placement buffer is 0x88 bytes.
 * Confirmed: LEA EAX,[EBP-0x88] = base of placement buffer.
 * Confirmed: object_placement_data_new(buf, tag_index, -1).
 * Confirmed: position at buf+0x18 (3 floats from param+8).
 * Confirmed: vectors3d_from_euler_angles3d(buf+0x34, buf+0x40, param+0x14).
 * Confirmed: bsp_index at buf+0x16 (short from param+6).
 * Confirmed: object_new(buf) returns handle.
 * Confirmed: object_type_place(handle, param) post-links.
 * Confirmed: object_name_list_new called with EDI=handle, SI=name_index. */
int object_new_from_scenario(void *placement_data, int palette_block)
{
  char *param;
  int result;
  int tag_index;
  char placement_buf[0x88];
  char *element;

  param = (char *)placement_data;
  result = -1;

  /* Check that the tag index at param[0] is not -1 */
  if (*(int16_t *)param == -1)
    goto done;

  /* If object_globals byte 0 is nonzero and the placement flag bit 0 is set,
   * skip creation (already placed). */
  if (*(char *)*(int *)0x46f084 != '\0' &&
      (*(unsigned char *)(param + 0x4) & 1) != 0)
    goto done;

  /* Check if name slot is available (name_index valid and slot free) */
  {
    int16_t name_idx = *(int16_t *)(param + 0x2);
    if (name_idx != -1) {
      if (name_idx < 0 || name_idx >= 0x200)
        goto do_create;
      if (*(int *)(*(int *)0x46f07c + (int)name_idx * 4) != -1)
        goto done;
    }
  }

do_create:
  /* Look up the palette element to get the tag index */
  element = (char *)tag_block_get_element((void *)palette_block,
                                          (int)*(int16_t *)param, 0x30);
  tag_index = *(int *)(element + 0xc);
  if (tag_index == -1)
    goto done;

  /* Initialize placement data */
  object_placement_data_new(placement_buf, tag_index, -1);

  /* Copy position (3 floats from param+8 to buf+0x18) */
  {
    /* aggregate copy: ref lea ecx,[esi+8] then three dword moves through
     * ECX (0x144782), not three independent [esi+N] loads. */
    *(real_point3d *)(placement_buf + 0x18) =
      *(real_point3d *)(param + 0x8);
  }

  /* Compute forward/up vectors from euler angles */
  vectors3d_from_euler_angles3d((float *)(placement_buf + 0x34),
                                (float *)(placement_buf + 0x40),
                                (float *)(param + 0x14));

  /* Copy BSP index */
  *(int16_t *)(placement_buf + 0x16) = *(int16_t *)(param + 0x6);

  /* Create the object.  Failure paths all fall through to the single shared
   * exit (return result), mirroring the reference's je/jne 0xde structure
   * with result (EDI) pre-set to -1. */
  result = object_new(placement_buf);
  if (result == -1)
    goto done;
  object_type_place(result, (int)param);
  if (*(int16_t *)(param + 0x2) == -1)
    goto done;
  object_name_list_new(result, *(int16_t *)(param + 0x2));

done:
  return result;
}

/*
 * object_attach_to_marker — attach a child object to a parent at a named
 * marker position.
 *
 * Resolves markers on both parent and child objects via
 * object_get_marker_by_name. Disconnects the child from the map, then:
 *
 * - If child_marker_name is NULL or empty: computes an inverse of the child
 *   marker matrix and transforms the parent marker's position/up/forward
 *   into the child's local frame, writing directly to the child object's
 *   position (offset 0x0C), up (0x24), and forward (0x30).
 *
 * - If child_marker_name is provided: delegates to
 *   object_compute_child_marker_position (0x141020) to compute the relative
 *   transform using both markers.
 *
 * Finally reconnects the child to the map and calls object_attach_to_parent
 * (0x144240) with the parent node index from the parent marker result.
 *
 * Confirmed: 4 cdecl args (PUSH count before CALL, ADD ESP,0x2c combined
 *            cleanup covers first 4 CALLs).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (-1,
 * child_handle). Confirmed: CALL 0x140f10 (object_get_marker_by_name)
 * twice, max_count=1. Confirmed: CALL 0x13fd00 (object_disconnect_from_map)
 * with child_handle. Confirmed: TEST EDI,EDI / CMP byte ptr [EDI],0 —
 * null-or-empty check on child_marker_name. Confirmed: CALL 0x109150
 * (matrix_inverse) with child_markers+4 as source. Confirmed: CALL 0x109590 /
 * 0x109680 transform into object+0xC, +0x24, +0x30. Confirmed: CALL 0x141020
 * (object_compute_child_marker_position) in else branch. Confirmed: CALL
 * 0x140ce0 (object_connect_to_map) with (child_handle, 0). Confirmed: CALL
 * 0x144240 (object_attach_to_parent) with (parent_handle, child_handle,
 * parent_markers[0]). Inferred:  marker result struct is 0x6C (108) bytes;
 * first dword is node index, matrix at offset +4, position/up/forward within
 * parent marker at offsets 0x60, 0x3C, 0x54 respectively.
 */
void object_attach_to_marker(int parent_handle, void *marker_name,
                             int child_handle, void *child_marker_name)
{
  char parent_markers[0x6C];
  char child_markers[0x6C];
  float inverse[13]; /* 4x3 matrix = 52 bytes */

  void *child_obj = object_get_and_verify_type(child_handle, -1);

  object_get_marker_by_name(parent_handle, marker_name, parent_markers,
                                  1);
  object_get_marker_by_name(child_handle, child_marker_name,
                                  child_markers, 1);
  object_disconnect_from_map(child_handle);

  if (child_marker_name == NULL || *(char *)child_marker_name == '\0') {
    /* No child marker name — invert the child marker's matrix and use it to
       transform the parent marker's position/up/forward into the child's
       local coordinate space. */
    matrix_inverse((float *)(child_markers + 4), inverse);
    matrix_transform_point(inverse, (float *)(parent_markers + 0x60),
                           (float *)((char *)child_obj + 0xC));
    matrix_transform_vector(inverse, (float *)(parent_markers + 0x3C),
                            (float *)((char *)child_obj + 0x24));
    matrix_transform_vector(inverse, (float *)(parent_markers + 0x54),
                            (float *)((char *)child_obj + 0x30));
  } else {
    /* Child marker name specified — delegate to
       object_compute_child_marker_position which handles the full
       relative-transform computation. The destination matrix aliases
       parent_markers+0x38 (the parent marker's embedded matrix). */
    object_compute_child_marker_position(child_obj, child_markers,
                                         parent_markers + 0x38);
  }

  object_connect_to_map(child_handle, NULL);
  object_attach_to_parent(parent_handle, child_handle, *(int *)parent_markers);
}

/* 0x144940: spawn a scenario object by name index — resolve the name entry in
 * scenario+0x204 (0x24 stride), look up its palette block and base, fetch the
 * placement element, and hand it to object_new_from_scenario. */
void object_new_by_name(short param_1)
{
  int scn;
  int e;
  int palette;
  int pal_base;
  int placement;
  int elem_size;

  scn = (int)global_scenario_get();
  e = (int)tag_block_get_element((void *)(scn + 0x204), param_1, 0x24);
  palette = scenario_get_object_type_scenario_datums(scn, *(short *)(e + 0x20),
                                                     &elem_size);
  scn = (int)global_scenario_get();
  pal_base =
    scenario_get_object_type_scenario_palette(scn, *(short *)(e + 0x20));
  placement = (int)tag_block_get_element((void *)palette, *(short *)(e + 0x22),
                                         elem_size);
  object_new_from_scenario((void *)placement, pal_base);
}

/*
 * object_delete_recursive — object deactivation and deallocation.
 *
 * Recursively tears down an object and its children/siblings, then deallocates
 * the object from the object pool. Called either from
 * objects_garbage_collection (immediate delete) or from the garbage collection
 * pass in objects_update.
 *
 * Steps:
 *   1. If object has flag 0x10000, clear garbage flag via
 * object_set_garbage_flag.
 *   2. Call deletion callbacks via FUN_00138eb0 (dispatch through function
 * table).
 *   3. Recursively deactivate child object (obj+0xC8).
 *   4. If delete_sibling is nonzero, recursively deactivate sibling (obj+0xC4).
 *   5. Clear collideable bit (datum header bit 0) if set.
 *   6. Call type table cleanup via object_type_definition_get.
 *   7. Call object cleanup via widgets_delete.
 *   8. Call widget detach via attachments_delete.
 *   9. If object has flag 0x800, disconnect from map via
 * object_disconnect_from_map.
 *  10. Call object_type_delete (final cleanup).
 *  11. Free memory pool block if allocated (via memory_pool_block_free).
 *  12. Delete datum from object pool via datum_delete.
 *  13. Clear field_8 and unk_2 in header.
 *
 * Confirmed: cdecl, 2 stack args (object_handle, delete_sibling).
 * Confirmed: delete_sibling is read as byte (MOVZX AL) but compared as bool.
 * Confirmed: Recursive calls at 0x1449ff and 0x144a1c with (child/sibling, 1).
 * Confirmed: Multiple object_get_and_verify_type calls to re-fetch after
 * recursion. Confirmed: EDI preserved across recursive calls (initial object
 * ptr). Confirmed: obj+0xC8 is child handle, obj+0xC4 is sibling handle.
 * Confirmed: 0x10000 flag triggers garbage flag clear.
 * Confirmed: 0x800 flag triggers map disconnect.
 */
/* 0x1449b0 */
void object_delete_recursive(int object_handle, int delete_sibling)
{
  object_data_t *obj;
  object_header_data_t *hdr;
  object_data_t *obj_again;
  int16_t obj_type;
  object_header_data_t *header;

  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  tag_get(0x6f626a65, (int)obj->tag_index);

  /* If object has flag 0x10000, clear the garbage flag. */
  if (obj->flags & 0x10000) {
    object_set_garbage_flag(object_handle, 0);
  }

  /* Dispatch deletion callbacks. */
  FUN_00138eb0(object_handle);

  /* Recursively deactivate child object. */
  if (obj->unk_200.value != -1) {
    object_delete_recursive(obj->unk_200.value, 1);
  }

  /* Optionally deactivate sibling object. */
  if ((char)delete_sibling != 0 && obj->next_object_index.value != -1) {
    object_delete_recursive(obj->next_object_index.value, 1);
  }

  /* Get datum header and clear collideable bit if set. */
  hdr = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  object_get_and_verify_type(object_handle, -1);
  if (hdr->unk_2 & 0x01) {
    hdr->unk_2 &= (uint8_t)~0x01;
  }

  /* Re-fetch object pointer after recursive calls. */
  obj_again = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  tag_get(0x6f626a65, (int)obj_again->tag_index);

  /* Call type table cleanup. */
  obj_type = obj_again->type;
  object_type_definition_get(obj_type);

  /* Object cleanup and widget detach. */
  widgets_delete(object_handle);
  attachments_delete(object_handle);

  /* If flag 0x800 is set, disconnect from map. */
  if (obj->flags & 0x800) {
    object_disconnect_from_map(object_handle);
  }

  object_type_delete(object_handle);

  /* Free memory pool block if allocated. */
  header = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);

  if (header->object != 0) {
    memory_pool_block_free(*(void **)0x46f080, (void **)&header->object);
  }

  /* Delete datum from pool. */
  datum_delete(*(data_t **)0x5a8d50, object_handle);

  /* Clear remaining fields. */
  header->object = 0;
  header->unk_2 = 0;
}

/* Scripting hook: attaches child object param_3 to parent param_1 at a marker,
   but only when both handles are valid and the child is not already attached
   (object+0xcc == -1). */
void objects_scripting_attach(int param_1, int param_2, int param_3,
                              int param_4)
{
  int object_ptr;

  if ((param_1 != -1) && (param_3 != -1)) {
    object_ptr = (int)object_get_and_verify_type(param_3, 0xffffffff);
    if (*(int *)(object_ptr + 0xcc) == -1) {
      object_attach_to_marker(param_1, (void *)param_2, param_3,
                              (void *)param_4);
    }
  }
  return;
}

/*
 * objects_garbage_collection — delete and immediately deactivate an object.
 *
 * Marks the object (and its children) for deletion via object_delete_internal,
 * then immediately tears down / deallocates the object via
 * object_delete_recursive. Used by actor_erase_units as the "soft" deletion
 * path (flag!=0) as an alternative to object_delete, which only marks for
 * deletion and defers actual teardown to the objects_update garbage-collection
 * pass.
 *
 * Confirmed: cdecl, one stack arg (object_handle).
 * Confirmed: PUSH 0x0 / PUSH ESI / CALL 0x140bc0 (object_delete_internal).
 * Confirmed: PUSH 0x0 / PUSH ESI / CALL 0x1449b0 (object_delete_recursive).
 * Confirmed: ADD ESP,0x10 — combined cleanup for both 2-arg calls.
 * Confirmed: ESI saved/restored (callee-saved register for param_1).
 */
/* 0x144b30 */
void objects_garbage_collection(int object_handle)
{
  object_delete_internal(object_handle, 0);
  object_delete_recursive(object_handle, 0);
}

/*
 * objects_garbage_collect_tick — per-tick garbage collection pass.
 *
 * Runs each game tick from objects_update. Determines memory pressure level,
 * walks the garbage object list, deletes objects not visible to any player,
 * compacts the memory pool, and runs AI release callbacks when critical.
 *
 * Three GC levels: 0=forced (external flag), 1=mild (headroom low),
 * 2=critical (memory or slots exhausted). Callback table at 0x29b868 has
 * two AI release entries (swarms and encounters) plus a NULL terminator.
 *
 * Confirmed: void(void) cdecl, _chkstk for 0x2814 bytes of stack.
 * Confirmed: globals at 0x46f080 (pool), 0x46f084 (object_globals),
 *   0x5a8d50 (object_header_data), 0x5a8d4c (debug flag).
 * Confirmed: thresholds 0xcccc, 0x19999, 0x6666, 0x67, 0xCC, 0x32, 0x1E, 150.
 * Confirmed: three deletion calls in sequence: set_garbage_flag,
 * delete_internal, delete_recursive — all with (handle, 0). Confirmed: callback
 * table 2 entries: {NULL, 0x3fa40}, {0x3fb40, 0x3fc90}. Confirmed: FILD + FMUL
 * 100.0f + FMUL (1/1048576.0f) for percentage calc.
 */
/* 0x144b50 */
void objects_garbage_collect_tick(void)
{
  typedef struct {
    void (*init)(void *working_mem, uint16_t mem_size);
    int (*iterate)(char *result_desc, char *more_to_release, void *working_mem,
                   uint16_t mem_size);
  } gc_callback_entry_t;

  /* garbage_handles[2048] and gc_working_mem share stack storage (disjoint
   * lifetimes): the original overlaps the 0x1000-byte working buffer onto the
   * upper half of the handle array, producing a 0x2814-byte frame. The four
   * message buffers are 0x200 each. */
  union {
    int garbage_handles[2048];
    struct {
      char _overlap_pad[0x1000];
      char working[0x1000];
    } cb;
  } gc_mem;
  char result_buf[512];
  char message_buf[512];
  char critical_buf[512];
  char status_buf[512];

  int gc_level; /* 0=forced, 1=mild, 2=critical */
  int gc_level_wide; /* switch subject (sign-extended gc_level) */
  int garbage_object_count;
  int contiguous_free;
  int free_size;
  float mem_pct;
  int handle;
  int slots_free;
  object_header_data_t *hdr;
  object_data_t *obj;
  int16_t type;
  gc_callback_entry_t *entry;
  const char *prefix;
  char is_critical;
  char should_delete; /* delete-decision; reused as "critical" flag */
  char previously_critical;
  char more_to_release;
  char init_called;
  char did_callbacks;
  char timed_out;
  char iterate_returned;

  /* Globals are re-read on every use (no local caches) to mirror the
   * original's repeated absolute loads: objects (pool) @0x46f080,
   * object_globals @0x46f084, object_header_data @0x5a8d50. */

  /* Phase 1: determine GC level */
  if (*(char *)((char *)object_globals + 2) != 0) {
    gc_level = 0;
  } else {
    contiguous_free = memory_pool_get_contiguous_free_size(objects);
    if (contiguous_free <= 0xcccc) {
      memory_pool_compact(objects);
      contiguous_free = memory_pool_get_contiguous_free_size(objects);
      if (0x19999 < contiguous_free)
        goto do_return_clear;
      gc_level = 2;
    } else if (0x800 - *(int16_t *)((char *)*(void **)0x5a8d50 + 0x30) <=
               0x66) {
      gc_level = 2;
    } else {
      if (*(int16_t *)((char *)object_globals + 4) < 0x32)
        goto do_return_clear;
      gc_level = 1;
    }
  }
  garbage_object_count = 0;
  should_delete = 0;

  /* Phase 2: debug output (behind 0x5a8d4c debug flag) */
  if (*(char *)0x5a8d4c) {
    contiguous_free = memory_pool_get_contiguous_free_size(objects);
    console_printf(0, "#%d objects using 0x%x bytes (0x%x contiguous free)",
                   (int)*(int16_t *)((char *)*(void **)0x5a8d50 + 0x30),
                   0x100000 - (int)memory_pool_get_free_size(objects),
                   contiguous_free);
  }

  /* Phase 3: build garbage object list */
  handle = object_globals->unk_8.value;
  while (handle != -1) {
    hdr = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle);
    obj = hdr->object;
    type = obj->type;
    if ((1 << type) == 0) {
      display_assert(csprintf((char *)0x5ab100,
                              "got an object type we didn't expect (expected "
                              "one of 0x%08x but got #%d).",
                              -1, (int)type),
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
      system_exit(-1);
    }
    if ((int16_t)garbage_object_count >= 0x800) {
      display_assert("garbage_object_count<MAXIMUM_OBJECTS_PER_MAP",
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0x10c1, 1);
      system_exit(-1);
    }
    gc_mem.garbage_handles[(int16_t)garbage_object_count] = handle;
    garbage_object_count++;
    handle = (int)obj->unk_192;
  }

  gc_level_wide = (int)(int16_t)gc_level;

  /* Phase 4+5: the GC-level switch is re-dispatched each loop iteration; the
   * delete body does not itself set should_delete — the switch decides when to
   * stop by re-evaluating the memory/slot pressure. */
delete_loop:
  switch (gc_level_wide) {
  case 0:
    should_delete = 0;
    goto pop_next;
  case 1:
    should_delete = *(int16_t *)((char *)object_globals + 4) < 0x1f;
    if (should_delete)
      goto compact_and_callbacks;
    goto pop_next;
  case 2:
    free_size = memory_pool_get_free_size(objects);
    if (free_size < 0x19999 ||
        0x800 - *(int16_t *)((char *)*(void **)0x5a8d50 + 0x2e) < 0xcc) {
      should_delete = 0;
      goto pop_next;
    }
    should_delete = 1;
    goto compact_and_callbacks;
  default:
    display_assert((char *)0, "c:\\halo\\SOURCE\\objects\\objects.c", 0x10da,
                   1);
    system_exit(-1);
  }

pop_next:
  if ((int16_t)garbage_object_count == 0)
    goto compact_and_callbacks;
  garbage_object_count--;
  handle = gc_mem.garbage_handles[(int16_t)garbage_object_count];
  hdr = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle);
  is_critical = 1; /* "should-consider-for-delete"; bl in the original */
  if ((int16_t)gc_level == 1)
    is_critical = (char)(hdr->unk_2 & 1);
  if ((char)object_visible_to_any_player(handle) == 0 && is_critical != 0) {
    obj =
      ((object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle))->object;
    type = obj->type;
    if ((1 << type) == 0) {
      display_assert(csprintf((char *)0x5ab100,
                              "got an object type we didn't expect (expected "
                              "one of 0x%08x but got #%d).",
                              -1, (int)type),
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
      system_exit(-1);
    }
    if (((1 << *(uint8_t *)((char *)obj + 0x64)) & 3) != 0 &&
        (obj->unk_182 & 4) == 0) {
      error(2, "WARNING: garbage collecting a living unit (%s)",
            ai_debug_describe_actor(-1, handle, 1, (char *)0x5ab100, 0x100));
    }
    if (hdr->unk_2 & 1)
      (*(int16_t *)((char *)object_globals + 4))--;
    object_set_garbage_flag(handle, 0);
    object_delete_internal(handle, 0);
    object_delete_recursive(handle, 0);
  }
  goto delete_loop;

compact_and_callbacks:
  /* Phase 6: compact and run GC release callbacks */
  memory_pool_compact(objects);

  if (*(char *)0x5a8d4c) {
    contiguous_free = memory_pool_get_contiguous_free_size(objects);
    console_printf(0, "compacted to #%d with 0x%x contiguous bytes free",
                   (int)*(int16_t *)((char *)*(void **)0x5a8d50 + 0x30),
                   contiguous_free);
  }

  if (should_delete != 0) {
  do_return_clear:
    *(char *)((char *)object_globals + 2) = 0;
    return;
  }

  entry = (gc_callback_entry_t *)0x29b868;

  /* Determine timeout */
  timed_out = 0;
  if (object_globals->last_garbage_collection_tick != (uint32_t)-1) {
    if ((int)(object_globals->last_garbage_collection_tick + 0x96) <
        game_time_get())
      timed_out = 1;
  } else {
    timed_out = 1;
  }

  did_callbacks = 0;
  previously_critical = 0;
  init_called = 0;

  /* Outer loop: classify pressure, build message, run callbacks */
  for (;;) {
    is_critical = 0;
    should_delete = 0; /* reused: "critical" flag for this pass */
    if (gc_level_wide == 2) {
      contiguous_free = memory_pool_get_contiguous_free_size(objects);
      slots_free = 0x800 - *(int16_t *)((char *)*(void **)0x5a8d50 + 0x2e);
      if (contiguous_free <= 0x6666) {
        should_delete = 1;
        goto crit_mem;
      }
      if (slots_free <= 0x33) {
        should_delete = 1;
        goto crit_slots;
      }
      if (contiguous_free <= 0xcccc)
        goto crit_mem;
      if (0x66 < slots_free)
        goto sprintf_mem;
    crit_slots:
      is_critical = 1;
      crt_sprintf(status_buf, "%d slots free", slots_free);
      goto after_status;
    crit_mem:
      is_critical = 1;
    sprintf_mem:
      mem_pct = (float)contiguous_free * REAL_100_POOL;
      crt_sprintf(status_buf, "%4.2f%% memory free",
                  (double)(mem_pct * *(float *)0x29ba04));
    after_status:
      if (should_delete == 0)
        goto not_critical;
      prefix = previously_critical ? "still " : "";
    } else {
    not_critical:
      if (previously_critical == 0) {
        if (is_critical && timed_out) {
          error(2, "garbage collection warning (%s)", status_buf);
        } else if (did_callbacks == 0) {
          *(char *)((char *)object_globals + 2) = 0;
          return;
        }
      finalize:
        object_globals->last_garbage_collection_tick =
          (uint32_t)game_time_get();
        *(char *)((char *)object_globals + 2) = 0;
        return;
      }
      prefix = "not ";
    }

    crt_sprintf(critical_buf, "garbage collection %scritical (%s)", prefix,
                status_buf);
    console_printf(0, "%s", critical_buf);
    error(3, "%s", critical_buf);
    did_callbacks = 1;

    if (should_delete == 0 || entry->iterate == NULL)
      goto finalize;

    /* Inner loop: run each release callback until it stops releasing */
    iterate_returned = 0;
    for (;;) {
      if (iterate_returned != 0)
        break;
      if (init_called == 0 && entry->init != NULL) {
        entry->init(gc_mem.cb.working, 0x1000);
        init_called = 1;
      }
      more_to_release = 0;
      iterate_returned = (char)entry->iterate(result_buf, &more_to_release,
                                              gc_mem.cb.working, 0x1000);
      if (iterate_returned != 0) {
        crt_sprintf(message_buf, "removing objects: %s", result_buf);
        console_printf(0, "%s", message_buf);
        error(3, "%s", message_buf);
      }
      if (more_to_release == 0) {
        entry++;
        init_called = 0;
      }
      if (entry->iterate == NULL)
        break;
    }
    if (iterate_returned == 0)
      goto finalize;

    previously_critical = 1;
    memory_pool_compact(objects);
  }
}

/*
 * objects_update — per-tick update for all active objects.
 *
 * Called once per game tick. Three passes over the object header array, plus
 * PVS comparison logic and a trailing garbage-collection call.
 *
 * Object header array base: *(void**)0x5a8d50 + 0x34.
 * Each element is 0xc bytes:
 *   +0x0 (int16_t): salt/generation (0 = slot empty)
 *   +0x2 (uint8_t): flags byte:
 *       bit 0 (0x01): collideable
 *       bit 2 (0x04): pending forced-update then deactivate
 *       bit 3 (0x08): pending deactivation
 *       bit 4 (0x10): "updated this tick" — cleared unconditionally each frame
 *       bit 5 (0x20): active (scheduled for update)
 *       bit 6 (0x40): PVS-relevant (cluster assigned)
 *       bit 7 (0x80): non-negative guard for non-collideable activation path
 *   +0x3 (uint8_t): object type index (used for double-speed skip mask)
 *   +0x4 (int16_t): cluster_index (-1 = NONE)
 *   +0x8 (uint32_t*): pointer to object_data_t
 *
 * PVS phase (only when PVS changes):
 *   og+0x4c (curr_pvs) receives this frame's combined player PVS; og+0xc
 *   (prev_pvs) receives the previous frame's curr_pvs snapshot.
 *   csmemcmp(prev_pvs, curr_pvs, pvs_size) detects a change.
 *   When they differ, walks all headers where bits 5 and 6 are both set
 *   (active + PVS-relevant):
 *     - Collideable (bit 0): if cluster NOT in curr_pvs:
 *         if [obj_data+4] & 0x80000 → FUN_140bc0(idx,0) (force-delete)
 *         else                      → FUN_13fb80(idx)   (deactivate)
 *     - Non-collideable (bit 0 clear, bit 7 clear), cluster != -1:
 *         if cluster IS in curr_pvs → FUN_13fb30(idx) (activate)
 *   Then calls FUN_1963c0(prev_pvs, curr_pvs, cluster_count) to update decals.
 *
 * Update phase:
 *   For each root object (flags & 1 set, flags & 4 clear):
 *     Asserts parent_object_index == -1 and next_object_index == -1.
 *     If game_players_are_double_speed() and object type is biped or vehicle
 *     (type bit 0 or 1 set) AND obj->field_1c8 != -1: skip FUN_1444f0.
 *     Otherwise: calls FUN_1444f0(handle) — object_update per-tick.
 *
 * Post-update phase:
 *   For each valid slot:
 *     Unconditionally clears bit 4 (0x10) from flags.
 *     If bit 2 (0x04) was set: clears bit 2, calls FUN_1444f0(handle) (flush).
 *     If bit 3 (0x08) set: calls FUN_1449b0(handle, 0) (deactivate/delete).
 *
 * Trailing call: FUN_144b50() — garbage-collect dead/stranded objects.
 *
 * Profiling markers: profile_enter_private / profile_exit_private around the
 * whole function, gated on two byte flags at 0x449ef1 and 0x324640.
 *
 * Confirmed: stride 0xc — ADD ESI,0xc at every loop-bottom.
 * Confirmed: element count = *(int16_t*)(obj_data_ptr+0x2e); compared with BX.
 * Confirmed: datum handle built as (int16_salt << 16) | int16_index via
 *            MOVSX + SHL 0x10 + OR.
 * Confirmed: EBX held as -1 sentinel throughout loop 2 (OR EBX,0xffffffff).
 * Confirmed: csmemcpy (0x8e0b0) copies old PVS to new PVS buffer and vice
 *            versa; players_get_combined_pvs (0xba6c0) provides current PVS.
 * Confirmed: ADD ESP,0xc (3 args) after first two csmemcpy calls; ADD ESP,0x18
 *            (6 args) after csmemcmp + csmemcpy combined cleanup.
 * Confirmed: MOVSX EAX,word ptr [EAX+0x134] — cluster count from scenario.
 * Confirmed: MOVSX EBX,BX / MOVSX ECX,DX used to zero-extend the 16-bit loop
 *            counter before PUSH as datum handle low word.
 * Confirmed: MOV AL,byte ptr [EBP-0x1] — double-speed bool held in stack slot.
 * Confirmed: display_assert + system_exit pattern identical to other functions.
 * Confirmed: FUN_13d680 called as object_get_and_verify_type(handle, -1) for
 *            the parent/next asserts, and (handle, 3) for the type mask check.
 * Confirmed: ADD ESP,0x8 after each 2-arg callee; ADD ESP,0x14 after each
 *            display_assert (4 args) + system_exit (1 arg) block.
 */
void objects_update(void)
{
  bool double_speed;
  object_globals_t *og;
  uint8_t *prev_pvs;
  uint8_t *curr_pvs;
  void *scen;
  int16_t cluster_count_raw;
  int pvs_size;
  int pvs_changed;

  /* --- profiling entry (gated on two flags) --- */
  if ((*(volatile uint8_t *)0x449ef1 != 0) &&
      (*(volatile uint8_t *)0x324640 != 0)) {
    /* 0x324638 IS the profile_section struct (PUSH 0x324638 at 0x1451a2), not
     * a pointer to one.  Its +0x0 field is the name string "objects_update"
     * and +0x8 is the `active` byte the guard above reads as 0x324640. */
    profile_enter_private((void *)0x324638);
  }

  /* --- double-speed player flag --- */
  /* game_time_get() returns the current tick; bit 0 set → odd tick. */
  double_speed = false;
  if ((game_time_get() & 1) != 0) {
    /* game_players_are_double_speed: returns bool via AL */
    if (game_players_are_double_speed()) {
      double_speed = true;
    }
  }

  /* --- PVS setup --- */
  /* object_globals->pending_update_count (int16 at +0x4) = 0 each frame */
  og = object_globals;
  *(int16_t *)((uint8_t *)og + 0x4) = 0;

  /* prev_pvs = og+0xc  (EBX in disasm; holds previous frame's PVS after copy)
   * curr_pvs = og+0x4c (EDI in disasm; receives fresh combined PVS each frame)
   * Confirmed: LEA EBX,[EAX+0xc]; MOV [EBP-0xc],EBX; LEA EDI,[EAX+0x4c]. */
  prev_pvs = (uint8_t *)og + 0xc;
  curr_pvs = (uint8_t *)og + 0x4c;

  /* cluster_count = scenario->bsp_cluster_count (int16 at scenario+0x134).
   * pvs_size = ((cluster_count + 0x1f) >> 5) << 2  (round up to dword).
   * Confirmed: MOVSX EAX,word [EAX+0x134]; MOVSX ESI,AX; ADD ESI,0x1f;
   *            SAR ESI,5; SHL ESI,2.
   * [EBP-8] holds the raw cluster_count_raw as int for later PUSH. */
  scen = scenario_get();
  cluster_count_raw = *(int16_t *)((uint8_t *)scen + 0x134);
  pvs_size = ((cluster_count_raw + 0x1f) >> 5) << 2;

  /* Step 1: save old curr_pvs into prev_pvs.
   * Confirmed: PUSH ESI(pvs_size);PUSH EDI(curr_pvs);PUSH EBX(prev_pvs);
   *            CALL csmemcpy; ADD ESP,0xc. */
  csmemcpy(prev_pvs, curr_pvs, pvs_size);

  /* Step 2: fetch this frame's combined player PVS; copy into curr_pvs.
   * players_get_combined_pvs() takes no arguments.
   * Confirmed: PUSH ESI (pre-push for next csmemcpy, not arg to pvs getter);
   *            CALL 0xba6c0; PUSH EAX(combined); PUSH EDI(curr_pvs);
   *            CALL csmemcpy. */
  /* permuter 20260721 (+0.8pp, 87.8 -> 88.5): call inlined into the arg. */
  csmemcpy(curr_pvs, players_get_combined_pvs(), pvs_size);

  /* Step 3: compare prev vs curr — nonzero means PVS changed this tick.
   * Confirmed: PUSH ESI;PUSH EDI(curr_pvs);PUSH EBX(prev_pvs);
   *            CALL 0x8da40 (csmemcmp); ADD ESP,0x18 cleans steps 2+3. */
  pvs_changed =
    ((int (*)(void *, void *, int))0x8da40)(prev_pvs, curr_pvs, pvs_size);

  /* --- PVS-driven activation/deactivation pass --- */
  if (pvs_changed != 0) {
    data_t *obj_data = *(data_t **)0x5a8d50;
    /* Array base: *(void**)(obj_data+0x34); count: *(int16_t*)(obj_data+0x2e).
     * Confirmed: MOV ESI,[EAX+0x34]; XOR EBX,EBX; CMP word [EAX+0x2e],BX */
    uint8_t *hdr = *(uint8_t **)((uint8_t *)obj_data + 0x34);
    int16_t count = *(int16_t *)((uint8_t *)obj_data + 0x2e);
    int16_t i;
    for (i = 0; i < count; i++, hdr += 0xc) {
      uint8_t flags;
      /* Reload count from live pointer at loop bottom.
       * Confirmed: MOV ECX,[0x5a8d50]; CMP BX,word [ECX+0x2e] at 0x1452d8. */
      count = *(int16_t *)((uint8_t *)(*(data_t **)0x5a8d50) + 0x2e);

      /* Skip empty slots */
      if (*(int16_t *)hdr == 0)
        continue;

      flags = *(uint8_t *)(hdr + 0x2);

      /* Must have both PVS-relevant (0x40) and active (0x20) bits set */
      if ((flags & 0x40) == 0)
        continue;
      if ((flags & 0x20) == 0)
        continue;

      if ((flags & 0x1) != 0) {
        /* Collideable object: should always have a valid cluster_index.
         * Binary asserts cluster_index != -1 here. */
        int16_t cluster_idx = *(int16_t *)(hdr + 0x4);
        if (cluster_idx == -1) {
          display_assert("object_header->cluster_index!=NONE",
                         "c:\\halo\\SOURCE\\objects\\objects.c", 0x171, 1);
          system_exit(-1);
        }
        /* Check if cluster is in the current PVS bitmap (EDI = curr_pvs).
         * Confirmed: SAR EAX,5; TEST [EDI+EAX*4],EDX; JNZ skip. */
        if ((*(uint32_t *)(curr_pvs + ((cluster_idx >> 5) * 4)) &
             (1u << (cluster_idx & 0x1f))) == 0) {
          /* Cluster is NOT in current PVS — deactivate or force-delete.
           * [ESI+8] = pointer to object_data; check object_data[1] & 0x80000.
           * Confirmed: MOV EAX,[ESI+8]; TEST dword [EAX+4],0x80000. */
          uint32_t *obj_dat = *(uint32_t **)(hdr + 0x8);
          if ((obj_dat[1] & 0x80000) != 0) {
            /* Has "always update" flag: force-delete. */
            object_delete_internal((int)i, 0);
          } else {
            /* Normal deactivate via object_deactivate. */
            object_deactivate((int)i);
          }
        }
      } else {
        /* Non-collideable path: activate if cluster is now in PVS.
         * Bit 7 of flags guards this path (skip if negative).
         * Confirmed: TEST AL,AL; JS skip. */
        int16_t cluster_idx;
        if ((flags & 0x80) != 0)
          continue;

        cluster_idx = *(int16_t *)(hdr + 0x4);
        if (cluster_idx == (int16_t)-1)
          continue;

        /* Check if cluster IS in current PVS.
         * Confirmed: TEST [EDI+EAX*4],EDX; JZ skip. */
        if ((*(uint32_t *)(curr_pvs + ((cluster_idx >> 5) * 4)) &
             (1u << (cluster_idx & 0x1f))) != 0) {
          object_activate((int)i);
        }
      }
    }

    /* Update structure decals for changed PVS.
     * FUN_1963c0(prev_pvs, curr_pvs, cluster_count): 3 cdecl args.
     * Confirmed: PUSH EDX([EBP-8]=cluster_count_raw cast to int),
     *            PUSH EDI(curr_pvs), PUSH EAX([EBP-0xc]=prev_pvs);
     *            CALL 0x1963c0; ADD ESP,0xc. */
    ((void (*)(void *, void *, int))0x1963c0)(prev_pvs, curr_pvs,
                                              (int)cluster_count_raw);
  }

  /* --- per-object update pass (root objects only) --- */
  /* Reload array base — may have been invalidated by the PVS pass.
   * Confirmed: MOV EAX,[0x5a8d50]; MOV EDI,[EAX+0x34] at 0x1452fd. */
  {
    data_t *obj_data = *(data_t **)0x5a8d50;
    uint8_t *hdr = *(uint8_t **)((uint8_t *)obj_data + 0x34);
    int16_t count = *(int16_t *)((uint8_t *)obj_data + 0x2e);
    int16_t i;

    for (i = 0; i < count; i++, hdr += 0xc) {
      uint8_t flags;
      int16_t salt;
      int handle;
      bool do_update;
      /* Reload count each iteration (confirmed at 0x1453de-0x1453eb).
       * Confirmed: MOV EAX,[0x5a8d50]; ... CMP DX,word [EAX+0x2e] */
      count = *(int16_t *)((uint8_t *)(*(data_t **)0x5a8d50) + 0x2e);

      if (*(int16_t *)hdr == 0)
        continue;

      flags = *(uint8_t *)(hdr + 0x2);
      /* Must be active (bit 0) and not pending forced-update (bit 2) */
      if ((flags & 0x1) == 0)
        continue;
      if ((flags & 0x4) != 0)
        continue;

      /* Build datum handle: (salt << 16) | index.
       * Confirmed: MOVSX ESI,CX (salt); MOVSX ECX,DX (index); SHL ESI,0x10;
       *            OR EBX,0xffffffff; OR ESI,ECX. */
      salt = *(int16_t *)hdr;
      handle = (int)(((uint32_t)(uint16_t)salt << 16) | (uint16_t)i);

      /* Assert: object must be a root (parent == -1) */
      {
        object_data_t *obj =
          (object_data_t *)object_get_and_verify_type(handle, 0xffffffff);
        if (*(int *)((uint8_t *)obj + 0xcc) != -1) {
          display_assert(
            "object_get(object_index)->object.parent_object_index==NONE",
            "c:\\halo\\SOURCE\\objects\\objects.c", 0x1a0, 1);
          system_exit(-1);
        }
      }

      /* Assert: object must not have a next sibling (next == -1) */
      {
        object_data_t *obj =
          (object_data_t *)object_get_and_verify_type(handle, 0xffffffff);
        if (*(int *)((uint8_t *)obj + 0xc4) != -1) {
          display_assert(
            "object_get(object_index)->object.next_object_index==NONE",
            "c:\\halo\\SOURCE\\objects\\objects.c", 0x1a1, 1);
          system_exit(-1);
        }
      }

      /* Double-speed skip: if double_speed && this object's type is biped or
       * vehicle (type-bit 0 or 1) && obj->field_1c8 != -1 → skip update.
       * Confirmed: MOV CL,[EDI+3] (type byte); SHL EDX,CL; TEST DL,0x3. */
      do_update = true;
      if (double_speed) {
        uint8_t type_byte = *(uint8_t *)(hdr + 0x3);
        if (((1u << (type_byte & 0x1f)) & 0x3) != 0) {
          object_data_t *obj =
            (object_data_t *)object_get_and_verify_type(handle, 3);
          if (*(int *)((uint8_t *)obj + 0x1c8) != -1) {
            do_update = false;
          }
        }
      }

      if (do_update) {
        ((int (*)(int))0x1444f0)(handle);
      }
    }
  }

  /* --- post-update flag cleanup and deferred operations --- */
  /* Confirmed: XOR EDI,EDI (index counter); MOV BL,0xef (& mask for bit 4).
   * Reload base: MOV ESI,[EAX+0x34] at 0x1453f4 after loop 2. */
  {
    data_t *obj_data = *(data_t **)0x5a8d50;
    uint8_t *hdr = *(uint8_t **)((uint8_t *)obj_data + 0x34);
    int16_t count = *(int16_t *)((uint8_t *)obj_data + 0x2e);
    int16_t i;

    for (i = 0; i < count; i++, hdr += 0xc) {
      uint8_t flags;
      /* Reload count each iteration.
       * Confirmed: MOV ECX,[0x5a8d50]; CMP DI,word [ECX+0x2e] at 0x14544a. */
      count = *(int16_t *)((uint8_t *)(*(data_t **)0x5a8d50) + 0x2e);

      if (*(int16_t *)hdr == 0)
        continue;

      /* Read flags, clear bit 4 (0x10) unconditionally ("updated this tick").
       * Confirmed: MOV AL,[ESI+2]; AND AL,0xef; MOV [ESI+2],AL. */
      flags = *(uint8_t *)(hdr + 0x2);
      flags &= (uint8_t)0xef;
      *(uint8_t *)(hdr + 0x2) = flags;

      /* If bit 2 (0x04) was set before the AND (i.e. was set before clearing):
       * also clear bit 2, then call object_update (0x1444f0).
       * Confirmed: TEST AL,0x4; JZ ...; AND AL,0xfb; MOV [ESI+2],AL. */
      if ((flags & 0x4) != 0) {
        int16_t salt;
        int handle;
        flags &= (uint8_t)0xfb;
        *(uint8_t *)(hdr + 0x2) = flags;
        salt = *(int16_t *)hdr;
        handle = (int)(((uint32_t)(uint16_t)salt << 16) | (uint16_t)i);
        ((int (*)(int))0x1444f0)(handle);
      }

      /* If bit 3 (0x08) is set: deactivate/delete the object.
       * Confirmed: TEST byte [ESI+2],0x8; JZ ...; ... CALL 0x1449b0. */
      if ((*(uint8_t *)(hdr + 0x2) & 0x8) != 0) {
        int16_t salt = *(int16_t *)hdr;
        int handle = (int)(((uint32_t)(uint16_t)salt << 16) | (uint16_t)i);
        object_delete_recursive(handle, 0);
      }
    }
  }

  /* --- garbage collection --- */
  /* FUN_144b50: collects dead/stranded objects; no args; no return value.
   * Confirmed: bare CALL 0x144b50 at 0x14545a. */
  ((void (*)(void))0x144b50)();

  /* --- profiling exit --- */
  if ((*(volatile uint8_t *)0x449ef1 != 0) &&
      (*(volatile uint8_t *)0x324640 != 0)) {
    profile_exit_private((void *)0x324638);
  }
}

/*
 * objects_memory_compact (0x145490 / objects.obj) — flush deferred object work:
 * run one garbage-collect tick, then compact the global objects memory pool
 * (0x46f080).
 *
 * Confirmed (disasm 0x145490): CALL objects_garbage_collect_tick (0x144b50);
 * MOV EAX,[0x46f080]; PUSH EAX; CALL memory_pool_compact (0x11e840); POP ECX.
 */
void objects_memory_compact(void)
{
  objects_garbage_collect_tick();
  memory_pool_compact(*(void **)0x46f080);
}

/* 0x1a9520 — get world-space position of the "body" marker on an object.
 * Thin wrapper: calls object_get_marker_by_name for marker "body",
 * then extracts XYZ from offset 0x60 in the marker output record. */
void unit_get_center_of_mass(int object_handle, float *out_position)
{
  char marker_buf[0x6c];
  /* Force the original stack-based offset reload for the first position word.
   */
  volatile unsigned int marker_position_offset;

  marker_position_offset = 0x60;
  object_get_marker_by_name(object_handle, "body", marker_buf, 1);
  out_position[0] = *(float *)(marker_buf + marker_position_offset);
  out_position[1] = *(float *)(marker_buf + 0x64);
  out_position[2] = *(float *)(marker_buf + 0x68);
}
