#ifndef HALO_UNITS_BIPED_DEFINITIONS_H
#define HALO_UNITS_BIPED_DEFINITIONS_H

#include "unit_definitions.h"

/* Biped tag fields that biped_definition_t does not declare. Offsets are from
 * the start of the tag; the biped part follows the unit part at 0x2f0. Each
 * accessor is an lvalue of the field at its tag offset. */

#define BIPED_DEFINITION_MOVING_TURNING_SPEED_OFFSET 0x2f0
#define BIPED_DEFINITION_FLAGS_OFFSET 0x2f4
#define BIPED_DEFINITION_FUNCTION_MODES_OFFSET 0x30c
#define BIPED_DEFINITION_FLYING_BANK_AMOUNT_OFFSET 0x324
#define BIPED_DEFINITION_FLYING_BANK_APPLY_TIME_OFFSET 0x328
#define BIPED_DEFINITION_FLYING_BANK_DECAY_TIME_OFFSET 0x32c
#define BIPED_DEFINITION_FLYING_PITCH_AMOUNT_OFFSET 0x330
#define BIPED_DEFINITION_FLYING_VELOCITY_OFFSET 0x334
#define BIPED_DEFINITION_FLYING_SIDESTEP_VELOCITY_OFFSET 0x338
#define BIPED_DEFINITION_FLYING_ACCELERATION_OFFSET 0x33c
#define BIPED_DEFINITION_FLYING_DECELERATION_OFFSET 0x340
#define BIPED_DEFINITION_FLYING_ANGULAR_VELOCITY_OFFSET 0x344
#define BIPED_DEFINITION_FLYING_ANGULAR_ACCELERATION_OFFSET 0x348
#define BIPED_DEFINITION_FLYING_CROUCH_VELOCITY_MODIFIER_OFFSET 0x34c
#define BIPED_DEFINITION_DOWNHILL_VELOCITY_SCALE_OFFSET 0x364
#define BIPED_DEFINITION_UPHILL_VELOCITY_SCALE_OFFSET 0x370
#define BIPED_DEFINITION_MATERIAL_EFFECTS_INDEX_OFFSET 0x398
#define BIPED_DEFINITION_JUMP_VELOCITY_OFFSET 0x3b4
#define BIPED_DEFINITION_MAXIMUM_SOFT_LANDING_TIME_OFFSET 0x3d4
#define BIPED_DEFINITION_MAXIMUM_HARD_LANDING_TIME_OFFSET 0x3d8
#define BIPED_DEFINITION_MINIMUM_SOFT_LANDING_VELOCITY_OFFSET 0x3dc
#define BIPED_DEFINITION_MINIMUM_HARD_LANDING_VELOCITY_OFFSET 0x3e0
#define BIPED_DEFINITION_MAXIMUM_HARD_LANDING_VELOCITY_OFFSET 0x3e4
#define BIPED_DEFINITION_STANDING_CAMERA_HEIGHT_OFFSET 0x400
#define BIPED_DEFINITION_CROUCHING_CAMERA_HEIGHT_OFFSET 0x404
#define BIPED_DEFINITION_COLLISION_HEIGHT_STANDING_OFFSET 0x424
#define BIPED_DEFINITION_COLLISION_HEIGHT_CROUCHING_OFFSET 0x428
#define BIPED_DEFINITION_COLLISION_RADIUS_OFFSET 0x42c
#define BIPED_DEFINITION_AUTOAIM_WIDTH_OFFSET 0x458
#define BIPED_DEFINITION_RUNTIME_COSINE_STATIONARY_TURNING_THRESHOLD_OFFSET 0x4c8
#define BIPED_DEFINITION_RUNTIME_CROUCH_TRANSITION_VELOCITY_OFFSET 0x4cc
#define BIPED_DEFINITION_RUNTIME_MINIMUM_NORMAL_K_OFFSET 0x4d0
#define BIPED_DEFINITION_RUNTIME_DOWNHILL_K0_OFFSET 0x4d4
#define BIPED_DEFINITION_RUNTIME_DOWNHILL_K1_OFFSET 0x4d8
#define BIPED_DEFINITION_RUNTIME_UPHILL_K0_OFFSET 0x4dc
#define BIPED_DEFINITION_RUNTIME_UPHILL_K1_OFFSET 0x4e0
#define BIPED_DEFINITION_RUNTIME_PELVIS_NODE_INDEX_OFFSET 0x4e4
#define BIPED_DEFINITION_RUNTIME_HEAD_NODE_INDEX_OFFSET 0x4e6
#define BIPED_DEFINITION_CONTACT_POINTS_OFFSET 0x4e8

#define biped_definition_moving_turning_speed(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_MOVING_TURNING_SPEED_OFFSET)
#define biped_definition_flags(definition) \
  UNIT_DEFINITION_FIELD(definition, unsigned long, BIPED_DEFINITION_FLAGS_OFFSET)
#define biped_definition_function_modes(definition) \
  ((int16_t *)((char *)(definition) + BIPED_DEFINITION_FUNCTION_MODES_OFFSET))
#define biped_definition_flying_bank_amount(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_FLYING_BANK_AMOUNT_OFFSET)
#define biped_definition_flying_bank_apply_time(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_FLYING_BANK_APPLY_TIME_OFFSET)
#define biped_definition_flying_bank_decay_time(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_FLYING_BANK_DECAY_TIME_OFFSET)
#define biped_definition_flying_pitch_amount(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_FLYING_PITCH_AMOUNT_OFFSET)
#define biped_definition_flying_velocity(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_FLYING_VELOCITY_OFFSET)
#define biped_definition_flying_sidestep_velocity(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_FLYING_SIDESTEP_VELOCITY_OFFSET)
#define biped_definition_flying_acceleration(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_FLYING_ACCELERATION_OFFSET)
#define biped_definition_flying_deceleration(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_FLYING_DECELERATION_OFFSET)
#define biped_definition_flying_angular_velocity(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_FLYING_ANGULAR_VELOCITY_OFFSET)
#define biped_definition_flying_angular_acceleration(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_FLYING_ANGULAR_ACCELERATION_OFFSET)
#define biped_definition_flying_crouch_velocity_modifier(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_FLYING_CROUCH_VELOCITY_MODIFIER_OFFSET)
#define biped_definition_downhill_velocity_scale(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_DOWNHILL_VELOCITY_SCALE_OFFSET)
#define biped_definition_uphill_velocity_scale(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_UPHILL_VELOCITY_SCALE_OFFSET)
#define biped_definition_material_effects_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, BIPED_DEFINITION_MATERIAL_EFFECTS_INDEX_OFFSET)
#define biped_definition_jump_velocity(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_JUMP_VELOCITY_OFFSET)
#define biped_definition_maximum_soft_landing_time(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_MAXIMUM_SOFT_LANDING_TIME_OFFSET)
#define biped_definition_maximum_hard_landing_time(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_MAXIMUM_HARD_LANDING_TIME_OFFSET)
#define biped_definition_minimum_soft_landing_velocity(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_MINIMUM_SOFT_LANDING_VELOCITY_OFFSET)
#define biped_definition_minimum_hard_landing_velocity(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_MINIMUM_HARD_LANDING_VELOCITY_OFFSET)
#define biped_definition_maximum_hard_landing_velocity(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_MAXIMUM_HARD_LANDING_VELOCITY_OFFSET)
#define biped_definition_standing_camera_height(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_STANDING_CAMERA_HEIGHT_OFFSET)
#define biped_definition_crouching_camera_height(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_CROUCHING_CAMERA_HEIGHT_OFFSET)
#define biped_definition_collision_height_standing(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_COLLISION_HEIGHT_STANDING_OFFSET)
#define biped_definition_collision_height_crouching(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_COLLISION_HEIGHT_CROUCHING_OFFSET)
#define biped_definition_collision_radius(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_COLLISION_RADIUS_OFFSET)
#define biped_definition_autoaim_width(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_AUTOAIM_WIDTH_OFFSET)
#define biped_definition_runtime_cosine_stationary_turning_threshold(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_RUNTIME_COSINE_STATIONARY_TURNING_THRESHOLD_OFFSET)
#define biped_definition_runtime_crouch_transition_velocity(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_RUNTIME_CROUCH_TRANSITION_VELOCITY_OFFSET)
#define biped_definition_runtime_minimum_normal_k(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_RUNTIME_MINIMUM_NORMAL_K_OFFSET)
#define biped_definition_runtime_downhill_k0(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_RUNTIME_DOWNHILL_K0_OFFSET)
#define biped_definition_runtime_downhill_k1(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_RUNTIME_DOWNHILL_K1_OFFSET)
#define biped_definition_runtime_uphill_k0(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_RUNTIME_UPHILL_K0_OFFSET)
#define biped_definition_runtime_uphill_k1(definition) \
  UNIT_DEFINITION_FIELD(definition, real, BIPED_DEFINITION_RUNTIME_UPHILL_K1_OFFSET)
#define biped_definition_runtime_pelvis_node_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, BIPED_DEFINITION_RUNTIME_PELVIS_NODE_INDEX_OFFSET)
#define biped_definition_runtime_head_node_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, BIPED_DEFINITION_RUNTIME_HEAD_NODE_INDEX_OFFSET)
#define biped_definition_contact_points(definition) \
  UNIT_DEFINITION_FIELD(definition, tag_block, BIPED_DEFINITION_CONTACT_POINTS_OFFSET)

/* Device machine tag fields read by biped physics. A machine with bit 2 of
 * the byte at +0x292 set and +0x2ea not NONE becomes the elevator object. */
#define MACHINE_DEFINITION_FIELD_292_OFFSET 0x292
#define MACHINE_DEFINITION_FIELD_2EA_OFFSET 0x2ea
#define machine_definition_field_292(definition) \
  UNIT_DEFINITION_FIELD(definition, uint8_t, MACHINE_DEFINITION_FIELD_292_OFFSET)
#define machine_definition_field_2ea(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, MACHINE_DEFINITION_FIELD_2EA_OFFSET)

#endif
