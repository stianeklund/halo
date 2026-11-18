#ifndef HALO_UNITS_UNIT_DEFINITIONS_H
#define HALO_UNITS_UNIT_DEFINITIONS_H

/* Unit tag fields that unit_definition_t does not declare. Each accessor is an
 * lvalue of the field at its tag offset. */

#define ANIMATION_GRAPH_DEFINITION_UNIT_SEATS_OFFSET 0x0c
#define ANIMATION_GRAPH_DEFINITION_FIELD_24_OFFSET 0x24
#define ANIMATION_GRAPH_DEFINITION_FIELD_3C_OFFSET 0x3c
#define ANIMATION_DEFINITION_KEY_FRAME_INDEX_OFFSET 0x34
#define ANIMATION_GRAPH_DEFINITION_FIELD_60_OFFSET 0x60
#define OBJECT_DEFINITION_FIELD_04_OFFSET 0x04
#define OBJECT_DEFINITION_FIELD_20_OFFSET 0x20
#define OBJECT_DEFINITION_MODEL_NAME_OFFSET 0x2c
#define OBJECT_DEFINITION_MODEL_INDEX_OFFSET 0x34
#define OBJECT_DEFINITION_ANIMATION_GRAPH_NAME_OFFSET 0x3c
#define OBJECT_DEFINITION_ANIMATION_GRAPH_INDEX_OFFSET 0x44
#define OBJECT_DEFINITION_COLLISION_MODEL_INDEX_OFFSET 0x7c
#define OBJECT_DEFINITION_PHYSICS_INDEX_OFFSET 0x8c
#define UNIT_DEFINITION_DEFAULT_TEAM_OFFSET 0x180
#define UNIT_DEFINITION_CONSTANT_SOUND_OFFSET 0x182
#define UNIT_DEFINITION_CHILD_DAMAGE_FRACTION_OFFSET 0x184
#define UNIT_DEFINITION_INTEGRATED_LIGHT_TOGGLE_EFFECT_INDEX_OFFSET 0x194
#define UNIT_DEFINITION_EXPORT_FUNCTION_SOURCES_OFFSET 0x198
#define UNIT_DEFINITION_CAMERA_FIELD_OF_VIEW_OFFSET 0x1a0
#define UNIT_DEFINITION_CAMERA_IMPULSE_STIFFNESS_OFFSET 0x1a4
#define UNIT_DEFINITION_CAMERA_MARKER_NAME_OFFSET 0x1a8
#define UNIT_DEFINITION_CAMERA_SUBMERGED_MARKER_NAME_OFFSET 0x1c8
#define UNIT_DEFINITION_CAMERA_PITCH_AUTOLEVEL_OFFSET 0x1e8
#define UNIT_DEFINITION_CAMERA_PITCH_MINIMUM_OFFSET 0x1ec
#define UNIT_DEFINITION_CAMERA_PITCH_MAXIMUM_OFFSET 0x1f0
#define UNIT_DEFINITION_CAMERA_TRACKS_OFFSET 0x1f4
#define UNIT_DEFINITION_ACCELERATION_SCALE_OFFSET 0x200
#define UNIT_DEFINITION_INTEGRATED_LIGHT_POWERON_TIME_OFFSET 0x20c
#define UNIT_DEFINITION_INTEGRATED_LIGHT_POWEROFF_TIME_OFFSET 0x210
#define UNIT_DEFINITION_INTEGRATED_LIGHT_LIFESPAN_OFFSET 0x214
#define UNIT_DEFINITION_SOFT_PING_THRESHOLD_OFFSET 0x218
#define UNIT_DEFINITION_SOFT_PING_MINIMUM_INTERRUPT_TIME_OFFSET 0x21c
#define UNIT_DEFINITION_HARD_PING_THRESHOLD_OFFSET 0x220
#define UNIT_DEFINITION_HARD_PING_MINIMUM_INTERRUPT_TIME_OFFSET 0x224
#define UNIT_DEFINITION_HARD_DEATH_THRESHOLD_OFFSET 0x228
#define UNIT_DEFINITION_FEIGN_DEATH_THRESHOLD_OFFSET 0x22c
#define UNIT_DEFINITION_FEIGN_DEATH_TIME_OFFSET 0x230
#define UNIT_DEFINITION_EVADE_DISTANCE_OFFSET 0x234
#define UNIT_DEFINITION_DIVE_DISTANCE_OFFSET 0x238
#define UNIT_DEFINITION_STUNNED_MOVEMENT_THRESHOLD_OFFSET 0x240
#define UNIT_DEFINITION_FEIGN_DEATH_CHANCE_OFFSET 0x244
#define UNIT_DEFINITION_FEIGN_DEATH_REPEAT_CHANCE_OFFSET 0x248
#define UNIT_DEFINITION_SPAWNED_ACTOR_VARIANT_INDEX_OFFSET 0x258
#define UNIT_DEFINITION_SPAWN_ACTOR_LOWER_BOUND_OFFSET 0x25c
#define UNIT_DEFINITION_SPAWN_ACTOR_UPPER_BOUND_OFFSET 0x25e
#define UNIT_DEFINITION_SPAWN_THROW_VELOCITY_OFFSET 0x260
#define UNIT_DEFINITION_AIMING_VELOCITY_MAXIMUM_OFFSET 0x264
#define UNIT_DEFINITION_AIMING_ACCELERATION_MAXIMUM_OFFSET 0x268
#define UNIT_DEFINITION_CASUAL_AIMING_MODIFIER_OFFSET 0x26c
#define UNIT_DEFINITION_LOOKING_VELOCITY_MAXIMUM_OFFSET 0x270
#define UNIT_DEFINITION_LOOKING_ACCELERATION_MAXIMUM_OFFSET 0x274
#define UNIT_DEFINITION_AI_VEHICLE_AVOIDANCE_RADIUS_OFFSET 0x280
#define UNIT_DEFINITION_MELEE_DAMAGE_INDEX_OFFSET 0x294
#define UNIT_DEFINITION_BLIP_TYPE_OFFSET 0x298
#define UNIT_DEFINITION_HUDS_OFFSET 0x2a8
#define UNIT_DEFINITION_DIALOGUE_VARIANTS_OFFSET 0x2b4
#define UNIT_DEFINITION_GRENADE_VELOCITY_OFFSET 0x2c0
#define UNIT_DEFINITION_GRENADE_TYPE_OFFSET 0x2c4
#define UNIT_DEFINITION_GRENADE_COUNT_OFFSET 0x2c6
#define UNIT_DEFINITION_RUNTIME_SOFT_PING_MINIMUM_INTERRUPT_TICKS_OFFSET 0x2c8
#define UNIT_DEFINITION_RUNTIME_HARD_PING_MINIMUM_INTERRUPT_TICKS_OFFSET 0x2ca
#define UNIT_DEFINITION_POWERED_SEATS_OFFSET 0x2cc
#define UNIT_DEFINITION_INITIAL_WEAPONS_OFFSET 0x2d8
#define UNIT_SEAT_DEFINITION_LABEL_OFFSET 0x04
#define UNIT_SEAT_DEFINITION_MARKER_NAME_OFFSET 0x24
#define UNIT_SEAT_DEFINITION_FIELD_84_OFFSET 0x84
#define UNIT_SEAT_DEFINITION_HUDS_OFFSET 0xdc
#define UNIT_SEAT_DEFINITION_FIELD_104_OFFSET 0x104
#define UNIT_PLACEMENT_FLAGS_OFFSET 0x04

/* unit_seat_t.flags bits. */
enum {
  _unit_seat_driver_bit = 2,
  _unit_seat_gunner_bit = 3,
  _unit_seat_allows_noncombatants_bit = 10
};

#define UNIT_DEFINITION_FIELD(definition, type, offset) \
  (*(type *)((char *)(definition) + (offset)))

/* Lvalue view of a real's bit pattern, for copies the original makes with
 * integer MOVs rather than FLD/FSTP. */
#define REAL_BITS(value) (*(uint32_t *)&(value))

/* int16 animation index in a seat's animations block, addressed by a byte
 * offset that the original walks in steps of 2 alongside the index. */
#define SEAT_ANIMATION_AT_BYTE_OFFSET(animations, byte_offset) \
  (*(short *)((char *)(animations).address + (byte_offset)))

/* Half of it raises a newly placed vehicle with physics. */
#define object_definition_field_04(definition) \
  UNIT_DEFINITION_FIELD(definition, real, OBJECT_DEFINITION_FIELD_04_OFFSET)
#define object_definition_field_20(definition) \
  UNIT_DEFINITION_FIELD(definition, real, OBJECT_DEFINITION_FIELD_20_OFFSET)
#define object_definition_model_name(definition) \
  UNIT_DEFINITION_FIELD(definition, char *, OBJECT_DEFINITION_MODEL_NAME_OFFSET)
#define object_definition_model_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, OBJECT_DEFINITION_MODEL_INDEX_OFFSET)
#define object_definition_animation_graph_name(definition) \
  UNIT_DEFINITION_FIELD(definition, char *, OBJECT_DEFINITION_ANIMATION_GRAPH_NAME_OFFSET)
#define object_definition_animation_graph_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, OBJECT_DEFINITION_ANIMATION_GRAPH_INDEX_OFFSET)
#define object_definition_collision_model_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, OBJECT_DEFINITION_COLLISION_MODEL_INDEX_OFFSET)
#define object_definition_physics_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, OBJECT_DEFINITION_PHYSICS_INDEX_OFFSET)
#define unit_definition_default_team(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, UNIT_DEFINITION_DEFAULT_TEAM_OFFSET)
#define unit_definition_constant_sound(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, UNIT_DEFINITION_CONSTANT_SOUND_OFFSET)
#define unit_definition_child_damage_fraction(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_CHILD_DAMAGE_FRACTION_OFFSET)
#define unit_definition_integrated_light_toggle_effect_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, UNIT_DEFINITION_INTEGRATED_LIGHT_TOGGLE_EFFECT_INDEX_OFFSET)
#define unit_definition_export_function_sources(definition) \
  ((int16_t *)((char *)(definition) + UNIT_DEFINITION_EXPORT_FUNCTION_SOURCES_OFFSET))
#define unit_definition_camera_field_of_view(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_CAMERA_FIELD_OF_VIEW_OFFSET)
#define unit_definition_camera_impulse_stiffness(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_CAMERA_IMPULSE_STIFFNESS_OFFSET)
#define unit_definition_camera_marker_name(definition) \
  ((char *)(definition) + UNIT_DEFINITION_CAMERA_MARKER_NAME_OFFSET)
#define unit_definition_camera_submerged_marker_name(definition) \
  ((char *)(definition) + UNIT_DEFINITION_CAMERA_SUBMERGED_MARKER_NAME_OFFSET)
#define unit_definition_camera_pitch_autolevel(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_CAMERA_PITCH_AUTOLEVEL_OFFSET)
#define unit_definition_camera_pitch_minimum(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_CAMERA_PITCH_MINIMUM_OFFSET)
#define unit_definition_camera_pitch_maximum(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_CAMERA_PITCH_MAXIMUM_OFFSET)
#define unit_definition_camera_tracks(definition) \
  UNIT_DEFINITION_FIELD(definition, tag_block, UNIT_DEFINITION_CAMERA_TRACKS_OFFSET)
#define unit_definition_acceleration_scale(definition) \
  UNIT_DEFINITION_FIELD(definition, real_vector3d, UNIT_DEFINITION_ACCELERATION_SCALE_OFFSET)
#define unit_definition_integrated_light_poweron_time(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_INTEGRATED_LIGHT_POWERON_TIME_OFFSET)
#define unit_definition_integrated_light_poweroff_time(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_INTEGRATED_LIGHT_POWEROFF_TIME_OFFSET)
#define unit_definition_integrated_light_lifespan(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_INTEGRATED_LIGHT_LIFESPAN_OFFSET)
#define unit_definition_soft_ping_threshold(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_SOFT_PING_THRESHOLD_OFFSET)
#define unit_definition_soft_ping_minimum_interrupt_time(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_SOFT_PING_MINIMUM_INTERRUPT_TIME_OFFSET)
#define unit_definition_hard_ping_threshold(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_HARD_PING_THRESHOLD_OFFSET)
#define unit_definition_hard_ping_minimum_interrupt_time(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_HARD_PING_MINIMUM_INTERRUPT_TIME_OFFSET)
#define unit_definition_hard_death_threshold(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_HARD_DEATH_THRESHOLD_OFFSET)
#define unit_definition_feign_death_threshold(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_FEIGN_DEATH_THRESHOLD_OFFSET)
#define unit_definition_feign_death_time(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_FEIGN_DEATH_TIME_OFFSET)
#define unit_definition_evade_distance(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_EVADE_DISTANCE_OFFSET)
#define unit_definition_dive_distance(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_DIVE_DISTANCE_OFFSET)
#define unit_definition_stunned_movement_threshold(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_STUNNED_MOVEMENT_THRESHOLD_OFFSET)
#define unit_definition_feign_death_chance(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_FEIGN_DEATH_CHANCE_OFFSET)
#define unit_definition_feign_death_repeat_chance(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_FEIGN_DEATH_REPEAT_CHANCE_OFFSET)
#define unit_definition_spawned_actor_variant_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, UNIT_DEFINITION_SPAWNED_ACTOR_VARIANT_INDEX_OFFSET)
#define unit_definition_spawn_actor_lower_bound(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, UNIT_DEFINITION_SPAWN_ACTOR_LOWER_BOUND_OFFSET)
#define unit_definition_spawn_actor_upper_bound(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, UNIT_DEFINITION_SPAWN_ACTOR_UPPER_BOUND_OFFSET)
#define unit_definition_spawn_throw_velocity(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_SPAWN_THROW_VELOCITY_OFFSET)
#define unit_definition_aiming_velocity_maximum(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_AIMING_VELOCITY_MAXIMUM_OFFSET)
#define unit_definition_aiming_acceleration_maximum(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_AIMING_ACCELERATION_MAXIMUM_OFFSET)
#define unit_definition_casual_aiming_modifier(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_CASUAL_AIMING_MODIFIER_OFFSET)
#define unit_definition_looking_velocity_maximum(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_LOOKING_VELOCITY_MAXIMUM_OFFSET)
#define unit_definition_looking_acceleration_maximum(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_LOOKING_ACCELERATION_MAXIMUM_OFFSET)
#define unit_definition_ai_vehicle_avoidance_radius(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_AI_VEHICLE_AVOIDANCE_RADIUS_OFFSET)
#define unit_definition_melee_damage_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, UNIT_DEFINITION_MELEE_DAMAGE_INDEX_OFFSET)
#define unit_definition_blip_type(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, UNIT_DEFINITION_BLIP_TYPE_OFFSET)
#define unit_definition_huds(definition) \
  UNIT_DEFINITION_FIELD(definition, tag_block, UNIT_DEFINITION_HUDS_OFFSET)
#define unit_definition_dialogue_variants(definition) \
  UNIT_DEFINITION_FIELD(definition, tag_block, UNIT_DEFINITION_DIALOGUE_VARIANTS_OFFSET)
#define unit_definition_grenade_velocity(definition) \
  UNIT_DEFINITION_FIELD(definition, real, UNIT_DEFINITION_GRENADE_VELOCITY_OFFSET)
#define unit_definition_grenade_type(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, UNIT_DEFINITION_GRENADE_TYPE_OFFSET)
#define unit_definition_grenade_count(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, UNIT_DEFINITION_GRENADE_COUNT_OFFSET)
#define unit_definition_runtime_soft_ping_minimum_interrupt_ticks(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, UNIT_DEFINITION_RUNTIME_SOFT_PING_MINIMUM_INTERRUPT_TICKS_OFFSET)
#define unit_definition_runtime_hard_ping_minimum_interrupt_ticks(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, UNIT_DEFINITION_RUNTIME_HARD_PING_MINIMUM_INTERRUPT_TICKS_OFFSET)
#define unit_definition_powered_seats(definition) \
  UNIT_DEFINITION_FIELD(definition, tag_block, UNIT_DEFINITION_POWERED_SEATS_OFFSET)
#define unit_definition_initial_weapons(definition) \
  UNIT_DEFINITION_FIELD(definition, tag_block, UNIT_DEFINITION_INITIAL_WEAPONS_OFFSET)

/* Animation graph tag: block of animation_graph_unit_seat_t elements. */
#define animation_graph_definition_unit_seats(graph) \
  UNIT_DEFINITION_FIELD(graph, tag_block, ANIMATION_GRAPH_DEFINITION_UNIT_SEATS_OFFSET)
/* Vehicle animation code returns early when this block is empty. */
#define animation_graph_definition_field_24(graph) \
  UNIT_DEFINITION_FIELD(graph, tag_block, ANIMATION_GRAPH_DEFINITION_FIELD_24_OFFSET)
#define animation_graph_definition_field_3c(graph) \
  UNIT_DEFINITION_FIELD(graph, tag_block, ANIMATION_GRAPH_DEFINITION_FIELD_3C_OFFSET)
/* Node sphere radius biped_limp_noodle_move_relax_and_constrain_positions uses. */
#define animation_graph_definition_field_60(graph) \
  UNIT_DEFINITION_FIELD(graph, real, ANIMATION_GRAPH_DEFINITION_FIELD_60_OFFSET)

/* Animation (animation_t) frame at which the action takes effect: the melee
 * damage tick and the grenade release tick are counted to it. */
#define animation_definition_key_frame_index(animation) \
  UNIT_DEFINITION_FIELD(animation, int16_t, ANIMATION_DEFINITION_KEY_FRAME_INDEX_OFFSET)

/* Unit seat tag-block element (unit_seat_t, 0x11c bytes). */
#define unit_seat_definition_label(seat) \
  ((char *)(seat) + UNIT_SEAT_DEFINITION_LABEL_OFFSET)
#define unit_seat_definition_marker_name(seat) \
  ((char *)(seat) + UNIT_SEAT_DEFINITION_MARKER_NAME_OFFSET)
#define unit_seat_definition_field_84(seat) \
  ((char *)(seat) + UNIT_SEAT_DEFINITION_FIELD_84_OFFSET)
#define unit_seat_definition_huds(seat) \
  UNIT_DEFINITION_FIELD(seat, tag_block, UNIT_SEAT_DEFINITION_HUDS_OFFSET)
#define unit_seat_definition_field_104(seat) \
  UNIT_DEFINITION_FIELD(seat, int, UNIT_SEAT_DEFINITION_FIELD_104_OFFSET)

/* Placement record passed to unit_place; bit 0 places the unit dead. */
#define unit_placement_flags(placement) \
  UNIT_DEFINITION_FIELD(placement, int, UNIT_PLACEMENT_FLAGS_OFFSET)

/* unit_data_t +0x308: previous tick seat delta. types.h still splits these
 * twelve bytes into powerup_type/unk_778/unk_782/unk_786. */
#define unit_seat_last_velocity(unit) (*(real_vector3d *)&(unit)->powerup_type)

#endif
