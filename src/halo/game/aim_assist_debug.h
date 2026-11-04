/* Aim-assist diagnostics -- DIAGNOSTIC INSTRUMENTATION, NOT recovered game code.
 *
 * Compiled only when HALO_AIM_ASSIST_DEBUG is defined (tools/build/build.py
 * --aim-assist-debug).  Draws the weapon's autoaim and magnetism cones as two
 * rings around the reticle and prints the per-tick magnetism terms of
 * get_local_player_input_blob.  See docs/aim-assist-debug.md. */
#ifndef HALO_AIM_ASSIST_DEBUG_H
#define HALO_AIM_ASSIST_DEBUG_H

#ifdef HALO_AIM_ASSIST_DEBUG

/* Runtime toggles, exported so tools/xbox/aim_assist_debug.py can find them
 * through the appended PE's export table (like halo_rng_trace). */
typedef struct {
  char rings;   /* draw the autoaim (orange) and magnetism (cyan) cone rings */
  char text;    /* draw the one-line on-screen summary */
  char console; /* console_printf on target/engagement change and each shot */
  char trace;   /* record every tick into halo_aim_assist_trace */
} halo_aim_assist_debug_t;
__declspec(dllexport) extern halo_aim_assist_debug_t halo_aim_assist_debug;

/* Per-tick trace, read and formatted by tools/xbox/aim_assist_debug.py
 * --dump.  Angles are radians, exactly as the game holds them. */
#define AIM_ASSIST_TRACE_MAGIC 0x54414941u /* 'AIAT' */
#define AIM_ASSIST_TRACE_VERSION 2u
#define AIM_ASSIST_TRACE_CAPACITY 4096u    /* power of two; ~136 s at 30 Hz */

enum {
  AIM_ASSIST_TRACE_KIND_TICK = 1, /* get_local_player_input_blob */
  AIM_ASSIST_TRACE_KIND_SHOT = 2  /* player_aim_projectile */
};

enum {
  AIM_ASSIST_TRACE_FLAG_MIXED = 0x1,      /* the magnetism branch ran */
  AIM_ASSIST_TRACE_FLAG_MAGNETISM = 0x2,  /* player_magnetism */
  AIM_ASSIST_TRACE_FLAG_SEARCH_AXIS = 0x4, /* search_forward is valid */
  AIM_ASSIST_TRACE_FLAG_SCRIPTED = 0x8     /* halo_aim_test_script running */
};

/* 128 bytes.  A SHOT record uses tick, kind, local_player, target,
 * aim_angles (yaw/pitch of the projectile forward before
 * player_aim_projectile), final_yaw/final_pitch (the same after it: bent toward
 * the target, then pinned to the deviation cone), search_forward (the
 * director_camera_deterministic forward the cone search uses, recomputed
 * read-only), facing_forward (the observer forward at that moment) and
 * unit_position. */
typedef struct {
  int32_t tick;
  uint8_t kind;
  uint8_t local_player;
  uint16_t flags;
  int32_t target;
  real autoaim_level;
  real magnetism;
  real_euler_angles2d aim_angles;
  real_euler_angles2d aim_velocity;
  real stick_yaw;
  real stick_pitch;
  real look_yaw;          /* before the magnetism mix */
  real look_pitch;
  real game_speed;
  real input_scale;
  real magnetism_scale;
  real clamped_yaw_velocity;
  real clamped_pitch_velocity;
  real mixed_yaw;
  real mixed_pitch;
  real final_yaw;         /* input->look_yaw_delta */
  real final_pitch;       /* input->look_pitch_delta */
  real delta_time;
  real_vector3d search_forward; /* observer camera +0x20 handed to the search */
  real_vector3d facing_forward; /* director_camera_deterministic forward */
  real_point3d unit_position;   /* the player's unit, world space */
} aim_assist_trace_record_t;

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t capacity;
  uint32_t write_index; /* total records ever written; slot = idx % capacity */
  aim_assist_trace_record_t records[AIM_ASSIST_TRACE_CAPACITY];
} aim_assist_trace_buffer_t;
__declspec(dllexport) extern aim_assist_trace_buffer_t halo_aim_assist_trace;

/* Scripted pad input for repeatable aim tests, in the halo-cea-jukkis
 * --input format (tick port lx ly rx ry buttons).  The host
 * (tools/xbox/aim_assist_debug.py --script) writes the keyframes, count,
 * end_tick and ports, then sets state ARMED.  On the next poll the game
 * records start_tick = game_time and RUNs: each scripted port's XInput state
 * becomes the last keyframe for it with tick <= game_time - start_tick, before
 * the game's own button and stick processing.  Past end_tick it is DONE and the
 * real pads come back.  On start, each enabled pose moves that local player's
 * unit and sets its facing.  Ticks are 30 Hz game ticks.  buttons bit N = gamepad
 * button N (0-7 analog: A B X Y black white LT RT; 8-15 digital: dpad up down
 * left right, start, back, left thumb, right thumb). */
#define AIM_TEST_SCRIPT_MAGIC 0x50534941u /* 'AISP' */
#define AIM_TEST_SCRIPT_CAPACITY 4096u

enum {
  AIM_TEST_SCRIPT_IDLE = 0,
  AIM_TEST_SCRIPT_ARMED = 1,   /* written by the host */
  AIM_TEST_SCRIPT_RUNNING = 2, /* written by the game */
  AIM_TEST_SCRIPT_DONE = 3     /* written by the game */
};

typedef struct {
  int32_t tick; /* game ticks after start_tick */
  uint8_t port;
  uint8_t pad_05;
  uint16_t buttons;
  int16_t lx;
  int16_t ly;
  int16_t rx;
  int16_t ry;
} aim_test_keyframe_t;

/* Where a local player's unit is put when the script starts (the counterpart
 * of halo-cea-jukkis's TEST_STAND/TEST_MEET).  24 bytes. */
typedef struct {
  uint32_t enabled;
  real_point3d position;
  real yaw;   /* radians */
  real pitch; /* radians; the view only, the unit stays upright */
} aim_test_pose_t;

typedef struct {
  uint32_t magic;
  uint32_t state;
  int32_t start_tick; /* game time when the script started */
  uint32_t count;
  uint32_t cursor;    /* next keyframe to apply */
  int32_t end_tick;   /* relative tick after which the script is done */
  uint32_t ports;     /* bit N: port N is scripted */
  uint32_t pad_1c;
  aim_test_pose_t poses[4]; /* by local player index, applied at start */
  aim_test_keyframe_t keyframes[AIM_TEST_SCRIPT_CAPACITY];
} aim_test_script_t;
__declspec(dllexport) extern aim_test_script_t halo_aim_test_script;

/* input_get_device_states, after XInputGetState for gamepad `port`: when the
 * script drives that port, replaces its sticks (XInput range, lx ly rx ry) and
 * button mask, and returns nonzero. */
int aim_assist_debug_script_pad(int port, int16_t *sticks,
                                uint16_t *buttons);

/* get_local_player_input_blob, right after local_player_aim_assist: the
 * search result and the look deltas before magnetism mixes into them. */
void aim_assist_debug_note_search(int16_t local_player_index, int target,
                                  real autoaim_level, real magnetism,
                                  const real_euler_angles2d *aim_angles,
                                  const real_euler_angles2d *aim_velocity,
                                  real stick_yaw, real stick_pitch,
                                  real yaw_delta, real pitch_delta,
                                  real delta_time);

/* get_local_player_input_blob, inside the magnetism branch after mixing. */
void aim_assist_debug_note_mix(int16_t local_player_index, real game_speed,
                               real input_scale, real magnetism_scale,
                               real yaw_velocity, real pitch_velocity,
                               real yaw_delta, real pitch_delta);

/* trigger_create_projectiles, around its player_aim_projectile call (that
 * function is not ported, so the hook sits in the caller): the projectile
 * forward before and after the autoaim bend. */
void aim_assist_debug_note_shot_begin(const float *forward);
void aim_assist_debug_note_shot(int player_index, const float *forward,
                                int target);

/* Wrapper in aim_assist.c over its TU-private unit_get_aim_assist_parameters. */
bool aim_assist_debug_get_parameters(int unit_handle, float *out_params,
                                     int zoom_level);

/* Render pass (render-debug flush), once per player window. */
void aim_assist_debug_render(void);

#endif /* HALO_AIM_ASSIST_DEBUG */
#endif /* HALO_AIM_ASSIST_DEBUG_H */
