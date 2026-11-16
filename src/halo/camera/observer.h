/* Observer and camera-mode layouts private to observer.c.
 *
 * types.h keeps observer_t's +0x74..+0x298 range opaque (field_74) because new
 * members there perturb codegen in unrelated translation units.  observer_data_t
 * spells out the same 0x29c-byte object for this TU; every offset below is
 * read off the 2276 accesses in observer.c and checked against observer_t. */
#ifndef HALO_CAMERA_OBSERVER_H
#define HALO_CAMERA_OBSERVER_H

#include "camera_internal.h"

/* Camera result at observer +0x74, returned by observer_get_camera.
 * Names from the "observer->result.position" and
 * "observer->result.location.cluster_index" asserts; observer_update_result
 * fills velocity/forward/up/field_of_view in this order. */
typedef struct {
  real_point3d position;                   ///< offset=0x00
  location_t location;                     ///< offset=0x0c
  real_vector3d velocity;                  ///< offset=0x14
  real_vector3d forward;                   ///< offset=0x20
  real_vector3d up;                        ///< offset=0x2c
  real field_of_view;                      ///< offset=0x38
} observer_result_t;
cs(observer_result_t, 0x3c);
co(observer_result_t, location, 0x0c);
co(observer_result_t, velocity, 0x14);
co(observer_result_t, forward, 0x20);
co(observer_result_t, up, 0x2c);
co(observer_result_t, field_of_view, 0x38);

/* Camera-track tag ('trak'): the control-point block at +0x4 holds 0x3c-byte
 * points ("camera_track->control_points.count >= 4", following_camera.c:0x56).
 * Size unproven. */
typedef struct {
  uint8_t pad_00[0x4];                     ///< offset=0x00
  tag_block control_points;                ///< offset=0x04
} camera_track_definition_t;
co(camera_track_definition_t, control_points, 0x04);
#define CAMERA_TRACK_CONTROL_POINT_SIZE 0x3c

/* unit_camera_t.unit_camera_tracks element: a 'trak' reference followed by
 * 12 bytes never accessed here. */
typedef struct {
  tag_reference track;                     ///< offset=0x00
  uint8_t pad_10[0xc];                     ///< offset=0x10
} unit_camera_track_t;
cs(unit_camera_track_t, 0x1c);

/* Globals tag (game_globals_get): camera_track_splut falls back to the first
 * 'trak' reference of the block at +0x104 (0x10-byte elements).  Size
 * unproven. */
typedef struct {
  uint8_t pad_00[0x104];                   ///< offset=0x00
  tag_block camera_tracks;                 ///< offset=0x104
} globals_definition_t;
co(globals_definition_t, camera_tracks, 0x104);

/* Number of velocity components (observer_compute_update validates 0xb
 * "observer->velocities.n[parameter_index]" floats); the 0x38-byte stride
 * between velocities and accelerations leaves 12 bytes never accessed. */
#define NUMBER_OF_OBSERVER_VELOCITIES 11
#define NUMBER_OF_OBSERVER_PARAMETERS 14
#define NUMBER_OF_OBSERVER_COMMAND_PARAMETERS 5

typedef struct {
  real n[NUMBER_OF_OBSERVER_VELOCITIES];   ///< offset=0x00
  uint8_t pad_2c[0xc];                     ///< offset=0x2c
} observer_derivative_t;
cs(observer_derivative_t, 0x38);

/* The 14 parameter floats at +0xb0 mirror camera_command_t's 14 floats at
 * +0x04 (position, offset, depth, field_of_view, forward, up);
 * observer_compute_update copies them across by count.  The six 11-float
 * arrays at +0x158 are the t^5..t^0 coefficients summed by
 * observer_compute_update. */
typedef struct {
  int32_t header_signature;                ///< offset=0x00
  camera_command_t *pending_command;       ///< offset=0x04
  camera_command_t last_command;           ///< offset=0x08
  uint8_t updated_for_frame;               ///< offset=0x70
  uint8_t first_command;                   ///< offset=0x71
  uint8_t pad_72[2];                       ///< offset=0x72
  observer_result_t result;                ///< offset=0x74
  real_point3d focus_position;             ///< offset=0xb0
  real_vector3d focus_offset;              ///< offset=0xbc
  real focus_distance;                     ///< offset=0xc8
  real field_of_view;                      ///< offset=0xcc
  real_vector3d forward;                   ///< offset=0xd0
  real_vector3d up;                        ///< offset=0xdc
  observer_derivative_t velocities;        ///< offset=0xe8
  observer_derivative_t accelerations;     ///< offset=0x120
  real coefficient5[NUMBER_OF_OBSERVER_VELOCITIES]; ///< offset=0x158
  real coefficient4[NUMBER_OF_OBSERVER_VELOCITIES]; ///< offset=0x184
  real coefficient3[NUMBER_OF_OBSERVER_VELOCITIES]; ///< offset=0x1b0
  real coefficient2[NUMBER_OF_OBSERVER_VELOCITIES]; ///< offset=0x1dc
  real coefficient1[NUMBER_OF_OBSERVER_VELOCITIES]; ///< offset=0x208
  real coefficient0[NUMBER_OF_OBSERVER_VELOCITIES]; ///< offset=0x234
  observer_derivative_t displacements;     ///< offset=0x260
  int32_t trailer_signature;               ///< offset=0x298
} observer_data_t;
cs(observer_data_t, 0x29c);
co(observer_data_t, pending_command, 0x04);
co(observer_data_t, last_command, 0x08);
co(observer_data_t, updated_for_frame, 0x70);
co(observer_data_t, first_command, 0x71);
co(observer_data_t, result, 0x74);
co(observer_data_t, focus_position, 0xb0);
co(observer_data_t, focus_offset, 0xbc);
co(observer_data_t, focus_distance, 0xc8);
co(observer_data_t, field_of_view, 0xcc);
co(observer_data_t, forward, 0xd0);
co(observer_data_t, up, 0xdc);
co(observer_data_t, velocities, 0xe8);
co(observer_data_t, accelerations, 0x120);
co(observer_data_t, coefficient5, 0x158);
co(observer_data_t, coefficient0, 0x234);
co(observer_data_t, displacements, 0x260);
co(observer_data_t, trailer_signature, 0x298);

#define observer_data_get(local_player_index) \
  ((observer_data_t *)&observer_globals.local_players[(local_player_index)])

/* camera_command_t.flags bits beyond types.h's valid/force-time pair:
 * observer_update_result skips the near-plane obstruction probe for bit 4,
 * observer_update_positions skips the whole integration for bit 5. */
enum {
  _observer_command_ignore_obstructions_bit = 4,
  _observer_command_freeze_camera_bit = 5
};

#ifndef MAXIMUM_COLLISION_USER_STACK_DEPTH
#define MAXIMUM_COLLISION_USER_STACK_DEPTH 0x20
#endif

/* Shared constant-pool floats still read by name.  A plain literal at these
 * sites changes the emitted code: in observer_integrate's loop the compiler
 * keeps 3.0f/4.0f/5.0f in x87 registers instead of re-reading memory with
 * FMUL m32 as 2276 does, and FUN_0008c150's 0.5f step loses 4 bytes the same
 * way. */
#define REAL_0_5_POOL (*(float *)0x253398) /* 0.5f */
#define REAL_3_0_POOL (*(float *)0x254644) /* 3.0f */
#define REAL_4_0_POOL (*(float *)0x2533d8) /* 4.0f */
#define REAL_5_0_POOL (*(float *)0x254cc4) /* 5.0f */

#endif
