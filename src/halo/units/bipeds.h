#ifndef HALO_UNITS_BIPEDS_H
#define HALO_UNITS_BIPEDS_H

/* Biped object datum.  Size 0x480 is the datum size (short at +0x8) of the
 * biped object_type_definition at 0x323d48. */
#define BIPED_DATUM_SIZE 0x480

/* Structure-BSP leaves block (+0xe0, element 0x10). Leaf +0x08 is the int16
 * cluster index that scenario_location_from_point copies into
 * location_t.cluster_index. */
#define STRUCTURE_BSP_LEAVES_OFFSET 0xe0
#define STRUCTURE_LEAF_CLUSTER_INDEX_OFFSET 0x08
#define structure_bsp_leaves(bsp) \
  (*(tag_block *)((char *)(bsp) + STRUCTURE_BSP_LEAVES_OFFSET))
#define structure_leaf_cluster_index(leaf) \
  (*(int16_t *)((char *)(leaf) + STRUCTURE_LEAF_CLUSTER_INDEX_OFFSET))

/* Selects first (left_surface/forward_edge) or the adjacent int32
 * (right_surface/reverse_edge) of a collision_edge_t pair. The byte-offset
 * form is codegen-sensitive: ternary and indexed forms reorder
 * biped_update_physics. */
#define COLLISION_EDGE_PAIR_SELECT(first, second) \
  (*(int *)((char *)&(first) + ((second) ? 4 : 0)))

/* An int16 record member pushed as a full dword (MOV r32,[mem]; PUSH r32),
 * for a callee whose kb.json parameter is declared int. */
#define INT16_DWORD_ARG(record, type, member) \
  (*(int *)((char *)&(record) + offsetof(type, member)))

/* collision_model_test_vector (0x14cb00) result record, 0x420 bytes
 * (biped_update_moving frame [EBP-0x584]). FUN_0014dce0 copies +0x02/+0x00/
 * +0x04 into collision_test_result region/node/bsp indices, +0x10/+0x14 into
 * surface_index/plane_designator, +0x18/+0x19/+0x1a into flags/
 * breakable_surface_index/material_index. +0x00 also selects the
 * node_matrices row; a negative +0x14 negates the transformed plane. */
typedef struct collision_model_vector_result {
  int16_t node_index;                 /* +0x00 */
  int16_t region_index;               /* +0x02 */
  int16_t bsp_index;                  /* +0x04 */
  uint8_t pad_06[2];                  /* +0x06 */
  real t;                             /* +0x08 */
  real_plane3d *plane;                /* +0x0c */
  int32_t surface_index;              /* +0x10 */
  int32_t plane_designator;           /* +0x14 */
  int8_t flags;                       /* +0x18 */
  int8_t breakable_surface_index;     /* +0x19 */
  int16_t material_index;             /* +0x1a */
  uint8_t pad_1c[0x404];              /* +0x1c */
} collision_model_vector_result;
cs(collision_model_vector_result, 0x420);
co(collision_model_vector_result, region_index, 0x02);
co(collision_model_vector_result, t, 0x08);
co(collision_model_vector_result, plane, 0x0c);
co(collision_model_vector_result, plane_designator, 0x14);
co(collision_model_vector_result, material_index, 0x1a);

/* Per-node record in the object-header block that object+0x198 references
 * (object_header_block_reference_get). Stride 0x20: object_set_region_count
 * copies count << 5 bytes; unit seat exit stores the root node's
 * default_translation at +0x10. */
typedef struct object_node_record {
  uint8_t pad_00[0x10];               /* +0x00 */
  real_point3d translation;           /* +0x10 */
  uint8_t pad_1c[4];                  /* +0x1c */
} object_node_record;
cs(object_node_record, 0x20);
co(object_node_record, translation, 0x10);

enum {
  _biped_airborne_bit = 0,
  _biped_slipping_bit = 1
};

typedef struct biped_data_t {
  unit_data_t unit;                           /* +0x000 */
  uint32_t flags;                             /* +0x424 */
  int8_t landing_recovery_counter;            /* +0x428 */
  int8_t landing_recovery_time;               /* +0x429 */
  uint8_t state;                              /* +0x42a */
  int8_t elevator_ticks;                      /* +0x42b */
  int32_t elevator_object_index;              /* +0x42c */
  int32_t support_surface_index;              /* +0x430 */
  int32_t pathfinding_surface_index;          /* +0x434 */
  real_point3d pathfinding_point;             /* +0x438 */
  int32_t last_pathfinding_attempt_time;      /* +0x444 */
  int32_t last_pathfinding_surface_index;     /* +0x448 */
  int32_t impact_target_object_index;         /* +0x44c */
  int32_t last_falling_communication_time;    /* +0x450 */
  int32_t bump_object_index;                  /* +0x454 */
  int8_t bump_ticks;                          /* +0x458 */
  int8_t airborne_ticks;                      /* +0x459 */
  int8_t slipping_ticks;                      /* +0x45a */
  int8_t stop_ticks;                          /* +0x45b */
  int8_t jump_recovery_timer;                 /* +0x45c */
  int8_t player_melee_ticks;                  /* +0x45d */
  int8_t player_melee_attack_tick;            /* +0x45e */
  uint8_t pad_45f;                            /* +0x45f */
  int16_t landing;                            /* +0x460 */
  uint8_t pad_462[2];                         /* +0x462 */
  real crouch;                                /* +0x464 */
  real bank;                                  /* +0x468 */
  real_plane3d ground_plane;                  /* +0x46c */
  uint8_t limp_body_current_relaxation_iterations; /* +0x47c */
  uint8_t limp_body_max_relaxation_iterations;     /* +0x47d */
  uint8_t pad_47e[2];                         /* +0x47e */
} biped_data_t;
cs(biped_data_t, BIPED_DATUM_SIZE);
co(biped_data_t, flags, 0x424);
co(biped_data_t, pathfinding_point, 0x438);
co(biped_data_t, airborne_ticks, 0x459);
co(biped_data_t, crouch, 0x464);
co(biped_data_t, ground_plane, 0x46c);
co(biped_data_t, limp_body_current_relaxation_iterations, 0x47c);

#endif
