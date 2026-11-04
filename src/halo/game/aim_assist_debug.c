/* Aim-assist diagnostics -- DIAGNOSTIC INSTRUMENTATION, NOT recovered game code.
 * See aim_assist_debug.h and docs/aim-assist-debug.md.
 *
 * Three parts:
 *   - rings: the autoaim and magnetism cones of the weapon in hand, drawn as
 *     their edge projected through the render camera, so a ring is exactly as
 *     wide on screen as its cone is in the world at any zoom.  The angles come
 *     from unit_get_aim_assist_parameters (via aim_assist_debug_get_parameters), asked the way each consumer asks it
 *     (player_aim_projectile: the unit's zoom; local_player_aim_assist: the
 *     player control's zoom), so they carry the zoom scaling.
 *   - trace: every term of get_local_player_input_blob's magnetism mix, one
 *     record per tick, plus one record per autoaim shot, in the exported ring
 *     halo_aim_assist_trace (tools/xbox/aim_assist_debug.py --dump).
 *   - a one-line on-screen summary and optional console lines.
 *
 * Only reads game state.  director_get_perspective is deliberately not called
 * here: it writes director state.
 *
 * Runtime toggles: the exported halo_aim_assist_debug bytes, flipped from the
 * host with tools/xbox/aim_assist_debug.py. */
#include "halo/game/aim_assist_debug.h"

#ifdef HALO_AIM_ASSIST_DEBUG

#include "x87_math.h"

#define AIM_DEBUG_LOCAL_PLAYERS 4
#define AIM_DEBUG_SEGMENTS 48
/* Ring vertices sit this far along each cone direction from the camera: past
 * the near plane, short of almost any world geometry. */
#define AIM_DEBUG_RING_DISTANCE 0.5f
#define AIM_DEBUG_RAD_TO_DEG 57.29578f

cs(aim_assist_trace_record_t, 0x80);

halo_aim_assist_debug_t halo_aim_assist_debug = {1, 1, 0, 1};
aim_assist_trace_buffer_t halo_aim_assist_trace = {0};
aim_test_script_t halo_aim_test_script = {AIM_TEST_SCRIPT_MAGIC};

cs(aim_test_keyframe_t, 0x10);
cs(aim_test_pose_t, 0x18);

/* alpha, red, green, blue */
static real_argb_color aim_debug_autoaim_color = {1.0f, 1.0f, 0.48f, 0.19f};
static real_argb_color aim_debug_magnetism_color = {1.0f, 0.25f, 0.85f, 1.0f};

/* What the render pass needs from the last tick of each local player. */
typedef struct {
  int tick;
  int target;
  real autoaim_level;
  real magnetism;
  boolean mixed;
  real_vector3d search_forward;
  boolean search_forward_valid;
  aim_assist_trace_record_t *record; /* this tick's trace record, or 0 */
} aim_debug_player_t;

static aim_debug_player_t aim_debug_players[AIM_DEBUG_LOCAL_PLAYERS];
static boolean aim_debug_initialized;

static void aim_debug_initialize(void)
{
  int i;

  if (aim_debug_initialized) {
    return;
  }
  for (i = 0; i < AIM_DEBUG_LOCAL_PLAYERS; i++) {
    csmemset(&aim_debug_players[i], 0, sizeof(aim_debug_players[i]));
    aim_debug_players[i].target = NONE;
  }
  halo_aim_assist_trace.version = AIM_ASSIST_TRACE_VERSION;
  halo_aim_assist_trace.capacity = AIM_ASSIST_TRACE_CAPACITY;
  halo_aim_assist_trace.write_index = 0;
  halo_aim_assist_trace.magic = AIM_ASSIST_TRACE_MAGIC;
  aim_debug_initialized = 1;
}

static aim_debug_player_t *aim_debug_get(int16_t local_player_index)
{
  aim_debug_initialize();
  if (local_player_index < 0 || local_player_index >= AIM_DEBUG_LOCAL_PLAYERS) {
    return 0;
  }
  return &aim_debug_players[local_player_index];
}

static aim_assist_trace_record_t *aim_debug_trace_new(int kind,
                                                      int16_t local_player_index)
{
  aim_assist_trace_record_t *record;

  if (!halo_aim_assist_debug.trace) {
    return 0;
  }
  record = &halo_aim_assist_trace.records[halo_aim_assist_trace.write_index &
                                          (AIM_ASSIST_TRACE_CAPACITY - 1)];
  csmemset(record, 0, sizeof(*record));
  record->tick = game_time_get();
  record->kind = (uint8_t)kind;
  record->local_player = (uint8_t)local_player_index;
  if (halo_aim_test_script.state == AIM_TEST_SCRIPT_RUNNING) {
    record->flags |= AIM_ASSIST_TRACE_FLAG_SCRIPTED;
  }
  halo_aim_assist_trace.write_index++;
  return record;
}

static real aim_debug_length(const real_vector3d *v)
{
  return sqrtf(v->i * v->i + v->j * v->j + v->k * v->k);
}

/* Angle between two directions, in degrees; -1 when either is degenerate. */
static real aim_debug_angle_between(const real_vector3d *a,
                                    const real_vector3d *b)
{
  real_vector3d cross;
  real dot;

  if (aim_debug_length(a) <= 1e-6f || aim_debug_length(b) <= 1e-6f) {
    return -1.0f;
  }
  cross.i = a->j * b->k - a->k * b->j;
  cross.j = a->k * b->i - a->i * b->k;
  cross.k = a->i * b->j - a->j * b->i;
  dot = a->i * b->i + a->j * b->j + a->k * b->k;
  return x87_fatan2f(aim_debug_length(&cross), dot) * AIM_DEBUG_RAD_TO_DEG;
}

static boolean aim_debug_observer_forward(int16_t local_player_index,
                                          real_vector3d *forward)
{
  char *camera;

  camera = (char *)observer_get_camera((unsigned __int16)local_player_index);
  if (camera == 0) {
    return 0;
  }
  *forward = *(real_vector3d *)(camera + 0x20);
  return 1;
}

/* director_camera_deterministic's forward: the axis player_aim_projectile
 * searches around.  Read-only; its following-camera arm reads the forward slot
 * as input, so it is seeded with `fallback`. */
static void aim_debug_facing_forward(int unit_handle,
                                     const real_vector3d *fallback,
                                     real_vector3d *forward)
{
  real_point3d position;

  position.x = 0.0f;
  position.y = 0.0f;
  position.z = 0.0f;
  *forward = *fallback;
  if (unit_handle != NONE) {
    director_camera_deterministic(unit_handle, (int)&position, (int)forward);
  }
}

static void aim_debug_unit_position(int unit_handle, real_point3d *position)
{
  if (unit_handle != NONE &&
      object_try_and_get_and_verify_type(unit_handle, -1) != 0) {
    object_get_world_position(unit_handle, position);
  }
}

void aim_assist_debug_note_search(int16_t local_player_index, int target,
                                  real autoaim_level, real magnetism,
                                  const real_euler_angles2d *aim_angles,
                                  const real_euler_angles2d *aim_velocity,
                                  real stick_yaw, real stick_pitch,
                                  real yaw_delta, real pitch_delta,
                                  real delta_time)
{
  aim_debug_player_t *state;
  aim_assist_trace_record_t *record;
  player_data_t *player;
  int player_index;
  int previous_target;
  boolean was_magnetized;

  state = aim_debug_get(local_player_index);
  if (state == 0) {
    return;
  }
  previous_target = state->target;
  was_magnetized = state->magnetism > 0.0f;

  state->tick = game_time_get();
  state->target = target;
  state->autoaim_level = autoaim_level;
  state->magnetism = magnetism;
  state->mixed = 0;
  state->search_forward_valid =
    aim_debug_observer_forward(local_player_index, &state->search_forward);

  record = aim_debug_trace_new(AIM_ASSIST_TRACE_KIND_TICK, local_player_index);
  state->record = record;
  if (record != 0) {
    record->target = target;
    record->autoaim_level = autoaim_level;
    record->magnetism = magnetism;
    record->aim_angles = *aim_angles;
    record->aim_velocity = *aim_velocity;
    record->stick_yaw = stick_yaw;
    record->stick_pitch = stick_pitch;
    record->look_yaw = yaw_delta;
    record->look_pitch = pitch_delta;
    record->delta_time = delta_time;
    record->final_yaw = yaw_delta * (delta_time * 30.0f);
    record->final_pitch = pitch_delta * (delta_time * 30.0f);
    if (*(uint8_t *)0x2f0291 != 0) { /* player_magnetism */
      record->flags |= AIM_ASSIST_TRACE_FLAG_MAGNETISM;
    }
    if (state->search_forward_valid) {
      record->flags |= AIM_ASSIST_TRACE_FLAG_SEARCH_AXIS;
      record->search_forward = state->search_forward;
    }
    player_index = local_player_get_player_index(local_player_index);
    if (player_index != NONE) {
      player = (player_data_t *)datum_get(player_data, player_index);
      aim_debug_facing_forward(player->unit_handle, &state->search_forward,
                               &record->facing_forward);
      aim_debug_unit_position(player->unit_handle, &record->unit_position);
    }
  }

  if (halo_aim_assist_debug.console &&
      (target != previous_target || (magnetism > 0.0f) != was_magnetized)) {
    console_printf(0, "aim p%d t%d target %08x autoaim %.3f magnetism %.3f",
                   (int)local_player_index, state->tick, target,
                   (double)autoaim_level, (double)magnetism);
  }
}

void aim_assist_debug_note_mix(int16_t local_player_index, real game_speed,
                               real input_scale, real magnetism_scale,
                               real yaw_velocity, real pitch_velocity,
                               real yaw_delta, real pitch_delta)
{
  aim_debug_player_t *state;
  aim_assist_trace_record_t *record;

  state = aim_debug_get(local_player_index);
  if (state == 0) {
    return;
  }
  state->mixed = 1;
  record = state->record;
  if (record == 0) {
    return;
  }
  record->flags |= AIM_ASSIST_TRACE_FLAG_MIXED;
  record->game_speed = game_speed;
  record->input_scale = input_scale;
  record->magnetism_scale = magnetism_scale;
  record->clamped_yaw_velocity = yaw_velocity;
  record->clamped_pitch_velocity = pitch_velocity;
  record->mixed_yaw = yaw_delta;
  record->mixed_pitch = pitch_delta;
  record->final_yaw = yaw_delta * (record->delta_time * 30.0f);
  record->final_pitch = pitch_delta * (record->delta_time * 30.0f);
}

static real_vector3d aim_debug_shot_forward;

static void aim_debug_vector_to_angles(const real_vector3d *v,
                                       real_euler_angles2d *angles)
{
  angles->yaw = x87_fatan2f(v->j, v->i);
  angles->pitch = x87_fatan2f(v->k, sqrtf(v->i * v->i + v->j * v->j));
}

void aim_assist_debug_note_shot_begin(const float *forward)
{
  aim_debug_shot_forward = *(const real_vector3d *)forward;
}

void aim_assist_debug_note_shot(int player_index, const float *forward,
                                int target)
{
  player_data_t *player;
  aim_assist_trace_record_t *record;
  real_vector3d observer_forward;
  real_vector3d search_forward;
  real_euler_angles2d after;
  boolean have_observer;

  player = (player_data_t *)datum_get(player_data, player_index);
  if (aim_debug_get(player->local_player_index) == 0) {
    return;
  }
  have_observer =
    aim_debug_observer_forward(player->local_player_index, &observer_forward);
  aim_debug_facing_forward(player->unit_handle, &aim_debug_shot_forward,
                           &search_forward);

  record = aim_debug_trace_new(AIM_ASSIST_TRACE_KIND_SHOT,
                               player->local_player_index);
  if (record != 0) {
    record->target = target;
    aim_debug_vector_to_angles(&aim_debug_shot_forward, &record->aim_angles);
    aim_debug_vector_to_angles((const real_vector3d *)forward, &after);
    record->final_yaw = after.yaw;
    record->final_pitch = after.pitch;
    record->search_forward = search_forward;
    aim_debug_unit_position(player->unit_handle, &record->unit_position);
    if (have_observer) {
      record->flags |= AIM_ASSIST_TRACE_FLAG_SEARCH_AXIS;
      record->facing_forward = observer_forward;
    }
  }
  if (halo_aim_assist_debug.console) {
    console_printf(0, "aim p%d SHOT target %08x bend %.3f facing-vs-view %.3f",
                   (int)player->local_player_index, target,
                   (double)aim_debug_angle_between(
                     &aim_debug_shot_forward, (const real_vector3d *)forward),
                   have_observer ? (double)aim_debug_angle_between(
                                     &search_forward, &observer_forward)
                                 : -1.0);
  }
}

/* Held pad per port, advanced as keyframes come due. */
static aim_test_keyframe_t aim_test_held[4];

static void aim_test_apply_poses(const aim_test_script_t *script)
{
  const aim_test_pose_t *pose;
  player_data_t *player;
  real_point3d position;
  real_vector3d forward;
  real_vector3d facing;
  int player_index;
  int16_t local_player_index;

  for (local_player_index = 0; local_player_index < 4; local_player_index++) {
    pose = &script->poses[local_player_index];
    if (!pose->enabled) {
      continue;
    }
    player_index = local_player_get_player_index(local_player_index);
    if (player_index == NONE) {
      continue;
    }
    player = (player_data_t *)datum_get(player_data, player_index);
    if (player->unit_handle == NONE ||
        object_try_and_get_and_verify_type(player->unit_handle, -1) == 0) {
      continue;
    }
    position = pose->position;
    forward.i = x87_fcos(pose->yaw);
    forward.j = x87_fsin(pose->yaw);
    forward.k = 0.0f;
    object_set_position(player->unit_handle, (float *)&position,
                        (float *)&forward, (float *)0);
    facing.i = forward.i * x87_fcos(pose->pitch);
    facing.j = forward.j * x87_fcos(pose->pitch);
    facing.k = x87_fsin(pose->pitch);
    player_control_set_facing(local_player_index, (float *)&facing);
  }
}

int aim_assist_debug_script_pad(int port, int16_t *sticks,
                                uint16_t *buttons)
{
  aim_test_script_t *script;
  aim_test_keyframe_t *key;
  int now;
  int relative;

  script = &halo_aim_test_script;
  if (port < 0 || port >= 4 ||
      (script->state != AIM_TEST_SCRIPT_ARMED &&
       script->state != AIM_TEST_SCRIPT_RUNNING)) {
    return 0;
  }
  if (game_time_globals == 0 || !game_time_globals->initialized) {
    return 0;
  }
  now = game_time_globals->time;
  if (script->state == AIM_TEST_SCRIPT_ARMED) {
    csmemset(aim_test_held, 0, sizeof(aim_test_held));
    script->start_tick = now;
    script->cursor = 0;
    if (script->count > AIM_TEST_SCRIPT_CAPACITY) {
      script->count = AIM_TEST_SCRIPT_CAPACITY;
    }
    script->state = AIM_TEST_SCRIPT_RUNNING;
    aim_test_apply_poses(script);
  }
  relative = now - script->start_tick;
  if (relative > script->end_tick) {
    script->state = AIM_TEST_SCRIPT_DONE;
    return 0;
  }
  while (script->cursor < script->count &&
         script->keyframes[script->cursor].tick <= relative) {
    key = &script->keyframes[script->cursor++];
    if (key->port < 4) {
      aim_test_held[key->port] = *key;
    }
  }
  if ((script->ports & (1u << port)) == 0) {
    return 0;
  }
  key = &aim_test_held[port];
  sticks[0] = key->lx;
  sticks[1] = key->ly;
  sticks[2] = key->rx;
  sticks[3] = key->ry;
  *buttons = key->buttons;
  return 1;
}

/* One cone's edge around `axis` at half-angle `angle` (radians), as line
 * segments from the camera position. */
static void aim_debug_draw_ring(const real_point3d *origin,
                                const real_vector3d *axis, real angle,
                                real_argb_color *color)
{
  real_vector3d a;
  real_vector3d e1;
  real_vector3d e2;
  real_point3d previous;
  real_point3d point;
  real length;
  real sine;
  real cosine;
  real phi;
  real c;
  real s;
  int k;

  length = aim_debug_length(axis);
  if (length <= 1e-6f || angle <= 0.0f) {
    return;
  }
  a.i = axis->i / length;
  a.j = axis->j / length;
  a.k = axis->k / length;

  /* e1 = world up x axis (world x when the axis is near vertical). */
  if (fabs(a.k) < 0.99f) {
    e1.i = -a.j;
    e1.j = a.i;
    e1.k = 0.0f;
  } else {
    e1.i = 0.0f;
    e1.j = -a.k;
    e1.k = a.j;
  }
  length = aim_debug_length(&e1);
  e1.i /= length;
  e1.j /= length;
  e1.k /= length;
  e2.i = a.j * e1.k - a.k * e1.j;
  e2.j = a.k * e1.i - a.i * e1.k;
  e2.k = a.i * e1.j - a.j * e1.i;

  sine = x87_fsin(angle) * AIM_DEBUG_RING_DISTANCE;
  cosine = x87_fcos(angle) * AIM_DEBUG_RING_DISTANCE;
  for (k = 0; k <= AIM_DEBUG_SEGMENTS; k++) {
    phi = 6.2831855f * (real)k / (real)AIM_DEBUG_SEGMENTS;
    c = x87_fcos(phi) * sine;
    s = x87_fsin(phi) * sine;
    point.x = origin->x + a.i * cosine + e1.i * c + e2.i * s;
    point.y = origin->y + a.j * cosine + e1.j * c + e2.j * s;
    point.z = origin->z + a.k * cosine + e1.k * c + e2.k * s;
    if (k > 0) {
      FUN_00189270(1, &previous.x, &point.x, color);
    }
    previous = point;
  }
}

void aim_assist_debug_render(void)
{
  int16_t local_player_index;
  aim_debug_player_t *state;
  player_data_t *player;
  int player_index;
  int aiming_unit_index;
  real autoaim_parameters[6];
  real magnetism_parameters[6];
  boolean have_autoaim;
  boolean have_magnetism;
  real_vector3d magnetism_axis;
  real_vector3d autoaim_axis;
  char text[0x100];

  if (!halo_aim_assist_debug.rings && !halo_aim_assist_debug.text) {
    return;
  }
  local_player_index = render.local_player_index;
  state = aim_debug_get(local_player_index);
  if (state == 0) {
    return;
  }
  player_index = local_player_get_player_index(local_player_index);
  if (player_index == NONE) {
    return;
  }
  player = (player_data_t *)datum_get(player_data, player_index);
  if (player->unit_handle == NONE) {
    return;
  }
  aiming_unit_index = unit_get_aiming_unit_index(player->unit_handle);
  if (aiming_unit_index == NONE) {
    return;
  }

  have_autoaim = aim_assist_debug_get_parameters(
    aiming_unit_index, autoaim_parameters,
    unit_get_zoom_level(aiming_unit_index));
  have_magnetism = aim_assist_debug_get_parameters(
    aiming_unit_index, magnetism_parameters,
    player_control_get_zoom_level(local_player_index));

  /* Each ring around the axis its own search uses, as jukkis does: magnetism
   * around the observer forward local_player_aim_assist handed the search on
   * the last tick, autoaim around the deterministic (unit facing) forward. */
  magnetism_axis = state->search_forward_valid ? state->search_forward
                                               : render.camera.forward;
  aim_debug_facing_forward(player->unit_handle, &render.camera.forward,
                           &autoaim_axis);

  if (halo_aim_assist_debug.rings) {
    if (have_magnetism) {
      aim_debug_draw_ring(&render.camera.position, &magnetism_axis,
                          magnetism_parameters[2], &aim_debug_magnetism_color);
    }
    if (have_autoaim) {
      aim_debug_draw_ring(&render.camera.position, &autoaim_axis,
                          autoaim_parameters[0], &aim_debug_autoaim_color);
    }
  }

  if (halo_aim_assist_debug.text) {
    snprintf(text, sizeof(text),
             "|n|n|naim tgt %08x aa %.2f mag %.2f%s  trace %u",
             state->target, (double)state->autoaim_level,
             (double)state->magnetism, state->mixed ? " MIX" : "",
             halo_aim_assist_trace.write_index);
    interface_set_bitmap_text_draw_mode(1, -1, 0, 0, 5, 0);
    rasterizer_text_draw(0, 0, 0, 0, text);
  }
}

#endif /* HALO_AIM_ASSIST_DEBUG */
