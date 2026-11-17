/* Absolute-address globals read by units.c, bipeds.c and vehicles.c.
 *
 * Each macro expands to exactly the cast/address expression it replaced, so
 * the access width and indirection at every site are unchanged. When two sites
 * read one address through different types, each type gets its own macro.
 * Script-global names come from the 2276 hs global table (name, type, address
 * triples at 0x2f27d8..0x2f35c4). */
#ifndef UNITS_GLOBALS_H
#define UNITS_GLOBALS_H

/* real_math global pointers (0x31fc1c..0x31fc54 point into the identity and
 * negative-identity 4x3 matrices at 0x28ca84). */
#define GLOBAL_ORIGIN3D (*(real_point3d **)0x31fc1c)
#define GLOBAL_ZERO_VECTOR3D (*(real_vector3d **)0x31fc38)
#define GLOBAL_FORWARD3D (*(real_vector3d **)0x31fc3c)
#define GLOBAL_UP3D (*(real_vector3d **)0x31fc44)
#define GLOBAL_DOWN3D (*(real_vector3d **)0x31fc50)
#define GLOBAL_ZERO_ANGLES2D (*(real_euler_angles2d **)0x31fc54)

/* global_real_argb_* color pointers (table at 0x267700). */
#define GLOBAL_REAL_ARGB_WHITE (*(void **)0x2ee6c4)
#define GLOBAL_REAL_ARGB_RED (*(void **)0x2ee6d0)
#define GLOBAL_REAL_ARGB_BLUE (*(void **)0x2ee6d8)
#define GLOBAL_REAL_ARGB_YELLOW (*(void **)0x2ee6e0)
#define GLOBAL_REAL_ARGB_ORANGE_BITS (*(int *)0x2ee6f0)

/* .rdata double constants. */
#define REAL_EPSILON_DOUBLE_POOL (*(double *)0x2533d0) /* 0.0001 */
#define REAL_ONE_DOUBLE_POOL (*(double *)0x2573d8) /* 1.0 */
#define REAL_QUARTER_PI_DOUBLE_POOL (*(double *)0x25b3f0) /* 0.78539818525314331 */
#define REAL_2_159845_DOUBLE_POOL (*(double *)0x2b7a98) /* 2.159845009446144 */

/* Physics globals. */
#define GLOBAL_GRAVITY (*(float *)0x32512c)
/* depths_of_hell plane (0, 0, 1, -256), copied as four dwords. */
#define DEPTHS_OF_HELL_PLANE_NORMAL_I_BITS (*(int *)0x32513c)
#define DEPTHS_OF_HELL_PLANE_NORMAL_J_BITS (*(int *)0x325140)
#define DEPTHS_OF_HELL_PLANE_NORMAL_K_BITS (*(int *)0x325144)
#define DEPTHS_OF_HELL_PLANE_D_BITS (*(int *)0x325148)
/* 27 fudge vectors (real_vector3d) at 0x2b4b80, read as i/j/k float columns. */
#define FUDGE_VECTORS_I ((float *)0x2b4b80)
#define FUDGE_VECTORS_J ((float *)0x2b4b84)
#define FUDGE_VECTORS_K ((float *)0x2b4b88)

/* Collision user stack. */
#define COLLISION_USER_STACK_DEPTH (*(int16_t *)0x4761d8)
#define COLLISION_USER_STACK ((int16_t *)0x5a8c80)

/* collision_debug_* script globals. */
#define COLLISION_DEBUG_REPEAT_DWORD (*(int *)0x5a8d1c)
#define COLLISION_DEBUG_POINT_X (*(float *)0x5a8d00)
#define COLLISION_DEBUG_POINT_Y_BITS (*(int *)0x5a8d04)
#define COLLISION_DEBUG_POINT_Z_BITS (*(int *)0x5a8d08)
#define COLLISION_DEBUG_VECTOR_I (*(float *)0x5a8cf0)
#define COLLISION_DEBUG_VECTOR_J (*(float *)0x5a8cf4)
#define COLLISION_DEBUG_VECTOR_K (*(float *)0x5a8cf8)
#define COLLISION_DEBUG_LENGTH_BITS (*(int *)0x324fc4)
#define COLLISION_DEBUG_WIDTH (*(float *)0x4761b8)
#define COLLISION_DEBUG_HEIGHT (*(float *)0x4761bc)

/* Profile sections {name, NONE, enabled}: "biped_update", "unit_update",
 * "vehicle_update". */
#define PROFILE_GLOBAL_ENABLE (*(char *)0x449ef1)
#define BIPED_UPDATE_SECTION ((void *)0x0032d1d0)
#define BIPED_UPDATE_SECTION_ENABLED (*(char *)0x0032d1d8)
#define UNIT_UPDATE_SECTION ((void *)0x32de88)
#define UNIT_UPDATE_SECTION_ENABLED (*(char *)0x32de90)
#define VEHICLE_UPDATE_SECTION ((void *)0x32e4a0)
#define VEHICLE_UPDATE_SECTION_ENABLED (*(char *)0x32e4a8)

/* Data arrays and engine state. */
#define UNIT_GLOBALS (*(units_globals_t **)0x4e4cf8)
#define OBJECT_HEADER_DATA (*(data_t **)0x5a8d50)
#define PLAYER_DATA (*(data_t **)0x5aa6d4)
#define PLAYER_DATA_VOID (*(void **)0x5aa6d4)
#define ACTOR_DATA (*(data_t **)0x6325a4)
#define ERROR_STRING_BUFFER_FIRST_DWORD (*(char **)0x5ab100)
#define RENDER_LOCAL_PLAYER_INDEX (*(int16_t *)0x506548)
#define AI_DEBUG_SELECTED_ACTOR_INDEX (*(int *)0x5ac9f8)

/* base_seat_labels[] ("asleep", "alert", "stand", "crouch", "flee",
 * "flaming"). */
#define BASE_SEAT_LABELS_AS_INT ((int *)0x32e484)
#define BASE_SEAT_LABEL_ASLEEP (*(const char **)0x32e484)
#define BASE_SEAT_LABEL_STAND (*(char **)0x32e48c)
#define BASE_SEAT_LABEL_STAND_AS_INT (*(int *)0x32e48c)
#define BASE_SEAT_LABEL_STAND_AS_INT_PTR (*(int **)0x32e48c)
/* Written by scripting_set_magic_base_seat. */
#define MAGIC_BASE_ANIMATION_SEAT_INDEX (*(int16_t *)0x32de80)
#define MAGIC_BASE_ANIMATION_SEAT_INDEX_DWORD (*(int *)0x32de80)
/* Round-robin counter for unit_dialogue_determine_variant. */
#define UNIT_DIALOGUE_VARIANT_COUNTER (*(int *)0x4e4cf4)
/* Game time of the last debug_unit_illumination print. */
#define UNIT_ILLUMINATION_DEBUG_LAST_TIME (*(int *)0x32e480)

/* Script globals (hs global table). */
#define RENDER_MODEL_NO_GEOMETRY (*(char *)0x5aa250)
#define RIDER_EJECTION (*(char *)0x0032d1c8)
#define STUN_ENABLE (*(char *)0x5054f8)
#define DEBUG_BIPED_PHYSICS (*(char *)0x4e4cf0)
#define DEBUG_BIPED_SKIP_UPDATE (*(char *)0x004e4cf1)
#define DEBUG_BIPED_SKIP_COLLISION (*(char *)0x4e4cf2)
#define DEBUG_BIPED_LIMP_BODY_DISABLE (*(char *)0x4e4cf3)
#define DEBUG_BIPED_LIMP_BODY_DISABLE_BYTE (*(uint8_t *)0x4e4cf3)
#define DEBUG_OBJECTS_VEHICLE_POWERED_MASS_POINTS (*(char *)0x5054f4)
#define DEBUG_OBJECTS_UNIT_MOUTH_APETURE (*(char *)0x5054f5)
#define DEBUG_OBJECTS_UNIT_SEATS (*(char *)0x5054f6)
#define DEBUG_OBJECTS_UNIT_VECTORS (*(char *)0x5054f7)
#define DEBUG_DAMAGE_TAKEN (*(char *)0x5054f9)
#define DEBUG_UNIT_ILLUMINATION (*(char *)0x5054fa)
#define DEBUG_UNIT_ANIMATIONS (*(uint8_t *)0x5054fb)
#define DEBUG_UNIT_ALL_ANIMATIONS (*(char *)0x5054fc)
#define DEBUG_OBJECTS_BIPED_AUTOAIM_PILLS (*(char *)0x5054fd)
#define DEBUG_OBJECTS_BIPED_PHYSICS_PILLS (*(char *)0x5054fe)
#define CHEAT_JETPACK (*(char *)0x5aa891)
#define CHEAT_JETPACK_BYTE (*(uint8_t *)0x5aa891)
#define CHEAT_INFINITE_AMMO (*(char *)0x5aa892)
#define CHEAT_BUMP_POSSESSION (*(uint8_t *)0x5aa893)
#define CHEAT_SUPER_JUMP (*(char *)0x5aa894)
#define AI_DEBUG_DISABLE_WOUNDED_SOUNDS (*(char *)0x5ac9ca)
#define AI_DEBUG_FORCE_VOCALIZATIONS (*(char *)0x5ac9cd)
#define AI_PRINT_LOST_SPEECH (*(char *)0x5aca56)

/* error_string_buffer (char[256]). */
#define ERROR_STRING_BUFFER ((char *)0x5ab100)
#define ERROR_STRING_BUFFER_CONST ((const char *)0x5ab100)

/* .rdata strings. 0x25386f is the terminator of " face at (p%d)". */
#define EMPTY_STRING ((const char *)0x25386f)
#define EMPTY_STRING_VOID ((void *)0x25386f)
#define MARKER_NAME_HEAD ((void *)0x2909e4)
#define MARKER_NAME_LEFT_HAND ((void *)0x2b6d2c)
#define MARKER_NAME_MELEE ((void *)0x25961c)
#define TRACE_STRING_UNIT_UPDATE_BEGIN ((const char *)0x2b7c98)
#define TRACE_STRING_UNIT_UPDATE_PREVECTORS ((const char *)0x2b7c28)
#define TRACE_STRING_UNIT_UPDATE_AIM_SET ((const char *)0x2b7bf0)
#define TRACE_STRING_UNIT_UPDATE_AIM_3D ((const char *)0x2b7bc4)
#define TRACE_STRING_UNIT_UPDATE_AIM_EULER ((const char *)0x2b7bd8)
#define TRACE_STRING_UNIT_UPDATE_LOOK_SET ((const char *)0x2b7bac)
#define TRACE_STRING_UNIT_UPDATE_LOOK_3D ((const char *)0x2b7b80)
#define TRACE_STRING_UNIT_UPDATE_LOOK_EULER ((const char *)0x2b7b94)
#define TRACE_STRING_UNIT_UPDATE_POSTVECTOR ((const char *)0x2b7b4c)
#define TRACE_STRING_UNIT_UPDATE_END ((const char *)0x2b7aec)
/* "player illumination: self %.2f ambient %.2f" */
#define FORMAT_STRING_PLAYER_ILLUMINATION ((const char *)0x2b7b20)

/* Animation name lists {count, {name, flag}[]} for animation_list_get_string. */
#define WEAPON_TYPE_ANIMATION_NAMES ((void *)0x322148) /* 10: reload-1.. */
#define WEAPON_ANIMATION_NAMES ((void *)0x322308) /* 55: idle, gesture.. */
#define UNIT_SEAT_ANIMATION_NAMES ((void *)0x322450) /* 30: airborne-dead.. */

/* String tables. */
#define VOCALIZATION_NAMES ((char **)0x32d7c8) /* two per type */
#define VOCALIZATION_NAMES_CONST ((const char **)0x32d7c8)
#define SPEECH_PRIORITY_NAMES ((char **)0x32de50) /* "none", "idle", .. */
#define BASE_SEAT_LABELS_ADDRESS 0x32e484

/* unit_test_speech tables, indexed by address arithmetic. */
#define VOCALIZATION_FALLBACK_TYPES_ADDRESS 0x2b6420 /* short[] */
#define SPEECH_OVERRIDE_PRIORITIES_ADDRESS 0x2b65c4 /* short[11] */
#define SPEECH_TIMING_THRESHOLDS_ADDRESS 0x2b65dc /* float[11] */

/* Limp-body scratch buffers (bss, referenced only by 0x19fa20). */
#define LIMP_BODY_COLLISION_FEATURES ((short *)0x4d9de8)
#define LIMP_BODY_COLLISION_FEATURES_VOID ((void *)0x4d9de8)
#define LIMP_BODY_NODE_POSITIONS ((float *)0x4e49f0)
#define LIMP_BODY_NODE_POSITIONS_AS_INT ((int *)0x4e49f0)

/* Callees reached through a call-site function-pointer cast. */
#define REPLACEMENT_ANIMATION_APPLY_FN ((void (*)(void *, short, void *))0x122060)
#define OVERLAY_ANIMATION_APPLY_FN ((void (*)(void *, short, void *))0x122240)
#define OVERLAY_ANIMATION_APPLY_SCALED_FN \
  ((void (*)(void *, int, float, void *))0x122450)
#define OVERLAY_ANIMATION_APPLY_CONTINUOUS_FN \
  ((void (*)(void *, float, void *))0x122690)
#define AI_HANDLE_DAMAGE_FN ((void (*)(int, int, int16_t, int, int, int))0x40460)
#define ERROR_FN ((void (*)(int, const char *, ...))0x8f390)

#endif /* UNITS_GLOBALS_H */
