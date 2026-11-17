#ifdef HALO_RNG_TRACE
#include "halo/math/rng_trace.h"
#endif
#include "unit_definitions.h"
#include "units_globals.h"
#include "weapon_definitions.h"
#include "game_globals_definitions.h"
#line 1
/* units.c — unit lifecycle and query helpers.
 *
 * Corresponds to units.obj. Functions sorted by XBE address.
 */

#include "../../common.h"
#include "../../x87_math.h"
#include "../math/real_math.h"
#include "bipeds.h"
#define NUMBER_OF_UNIT_BASE_SEATS 6
#define NUMBER_OF_UNIT_BASE_WEAPONS 1
#define MAXIMUM_WEAPONS_PER_UNIT 4
#define MAXIMUM_COLLISION_USER_STACK_DEPTH 32

/* Unit selector enums. See PROVENANCE.md for the evidence classes
 * behind the name_source tiers below.
 *
 * Every BOUND below was read out of this build before the corpus was consulted,
 * and all six agree with it exactly — the 0563 binary added no unit selector in
 * this group:
 *
 *   speech priority     unit_dialogue.c:0x82  rejects priority > 10      => 11
 *   scream type         units.c              rejects >= 6                => 6
 *   control flags       units.c              rejects & 0xffff8000        => 15
 *   grenade type        units.c              rejects >= 2                => 2
 *   unit state          units.c              rejects >= 0x2c             => 44
 *   vocalization type   unit_dialogue.c:0x90 rejects > 0xd0              => 209
 *
 * The bound identifiers themselves are stamped into our own assert strings, so
 * they are T1. So is _unit_speech_none: unit_dialogue.c:0x16e asserts
 * "unit->unit.speech.current.priority > _unit_speech_none" verbatim in THIS
 * binary, which independently confirms the corpus's member name and its value
 * of 0 (the assert guards against an empty current-speech slot).
 *
 * The remaining member names are name_source: halocea, T2 — halocea cites each
 * of these four to a compiled enum in the 0563 binary (types_enum_values
 * _33B1D83D…, _67894A93…, _C04139DE…, _DAB55C4E…), but that citation is one
 * build away from ours. Two behavioural facts in this TU corroborate the
 * ordering rather than merely the values: priorities 2/7/10 are exactly the set
 * that may interrupt a line already playing, consistent with pain/involuntary/
 * death outranking conversational speech; and priority 6 is the single slot
 * exempted from the priority cap in unit_test_speech (`priority != 6 && !bVar`),
 * which is what a scripted, designer-authored line needs and what no
 * conversational priority would need.
 */
enum unit_speech_priority {
  _unit_speech_none = 0,
  _unit_speech_idle = 1,
  _unit_speech_pain = 2,
  _unit_speech_talk = 3,
  _unit_speech_communicate = 4,
  _unit_speech_shout = 5,
  _unit_speech_scripted = 6,
  _unit_speech_involuntary = 7,
  _unit_speech_exclamation = 8,
  _unit_speech_scream = 9,
  _unit_speech_death = 10,
  NUMBER_OF_UNIT_SPEECH_PRIORITIES = 11
};

enum unit_scream_type {
  _unit_scream_falling = 0,
  _unit_scream_grenade_attached_to_us = 1,
  _unit_scream_burning_to_death = 2,
  _unit_scream_destroyed_limb = 3,
  _unit_scream_destroyed_head = 4,
  _unit_scream_resurrection = 5,
  NUMBER_OF_UNIT_SCREAM_TYPES = 6
};

/* Bit INDICES, not masks — use as (1 << _bit). NUMBER_OF_UNIT_GRENADE_TYPES is
 * already defined in types.h, so only the members are introduced here. */
enum unit_grenade_type {
  _unit_grenade_human_fragmentation = 0,
  _unit_grenade_covenant_plasma = 1
};

enum unit_control_flags {
  _unit_control_crouch_modifier_bit = 0x0,
  _unit_control_jump_bit = 0x1,
  _unit_control_user_animation1_bit = 0x2,
  _unit_control_user_animation2_bit = 0x3,
  _unit_control_integrated_light_bit = 0x4,
  _unit_control_exact_facing_bit = 0x5,
  _unit_control_action_bit = 0x6,
  _unit_control_use_equipment_bit = 0x7,
  _unit_control_look_dont_turn_bit = 0x8,
  _unit_control_force_alert_bit = 0x9,
  _unit_control_weapon_reload_bit = 0xA,
  _unit_control_weapon_primary_trigger_bit = 0xB,
  _unit_control_weapon_secondary_trigger_bit = 0xC,
  _unit_control_throw_grenade_bit = 0xD,
  _unit_control_swap_weapons_bit = 0xE,
  NUMBER_OF_UNIT_CONTROL_FLAGS = 0xF
};

/* Bits at or above NUMBER_OF_UNIT_CONTROL_FLAGS; what VALID_FLAGS rejects. */
#define UNIT_CONTROL_FLAGS_INVALID_MASK 0xffff8000

/* Bounds only. The 44 unit states and 209 vocalization types are not referenced
 * by name in this TU, so their member lists are deliberately not imported.
 * unit_test_speech compares vocalization_type < NUMBER_OF_VOCALIZATION_TYPES
 * (XBE cmp bx, 0xd1; jl). */
#define NUMBER_OF_UNIT_STATES 44
#define NUMBER_OF_VOCALIZATION_TYPES 209

void animation_get_root_matrix(void *mode_tag, void *animation, int animation_index,
                  void *out_matrix)
{
  uint8_t node_data[0x800];

  animation_get_frame_orientations(mode_tag, animation, animation_index, node_data);
  matrix4x3_from_point_and_quaternion(out_matrix, (float *)(node_data + 0x10),
                                  (float *)node_data);
}

/* -----------------------------------------------------------------------
 * render_model_parts — model render geometry (multi-pass)
 *
 * Iterates over model geometry groups and parts, performing up to 3
 * rendering passes (opaque, environment-mapped, transparent).  For each
 * shader part, dispatches to the appropriate render function based on
 * shader type and pass index.
 *
 * Disassembly range: 0x123560 – 0x12398b.
 * Source: c:\halo\SOURCE\models\models.c
 * ----------------------------------------------------------------------- */
#include "../../shader_model.h"

#define NUMBER_OF_DETAIL_LEVELS_PER_MODEL 5
#define MAXIMUM_PARTS_PER_MODEL_GEOMETRY 32

/* Model tag: only the blocks read by render_model_parts. Size unproven. */
typedef struct model_definition_t {
  byte pad_00[0xb8];
  tag_block nodes;      /* model->nodes.count (models.c:0x1bd) */
  tag_block regions;    /* element size 0x4c */
  tag_block geometries; /* element size 0x30 */
  tag_block shaders;    /* element size 0x20 */
} model_definition_t;
co(model_definition_t, nodes, 0xb8);
co(model_definition_t, regions, 0xc4);
co(model_definition_t, geometries, 0xd0);
co(model_definition_t, shaders, 0xdc);

typedef struct model_region_t {
  byte pad_00[0x40];
  tag_block permutations; /* element size 0x58 */
} model_region_t;
cs(model_region_t, 0x4c);

typedef struct model_region_permutation_t {
  byte pad_00[0x40];
  int16_t geometry_indices[NUMBER_OF_DETAIL_LEVELS_PER_MODEL];
  byte pad_4a[0x58 - 0x4a];
} model_region_permutation_t;
cs(model_region_permutation_t, 0x58);

typedef struct model_geometry_t {
  byte pad_00[0x24];
  tag_block parts; /* element size 0x68 */
} model_geometry_t;
cs(model_geometry_t, 0x30);

typedef struct model_geometry_part_t {
  byte flags;              /* bit 0 skips the part */
  byte pad_01[3];
  int16_t shader_index;
  int8_t previous_part_index;
  int8_t next_part_index;
  int16_t centroid_primary_node_index;
  int16_t centroid_secondary_node_index;
  byte pad_0c[8];
  real_point3d centroid;
  byte pad_20[0x44 - 0x20];
  int32_t field_44;        /* triangle buffer, passed by address */
  int32_t field_48;        /* triangle count */
  byte pad_4c[8];
  int32_t field_54;        /* vertex buffer, passed by address */
  byte pad_58[0x68 - 0x58];
} model_geometry_part_t;
cs(model_geometry_part_t, 0x68);
co(model_geometry_part_t, shader_index, 0x04);
co(model_geometry_part_t, previous_part_index, 0x06);
co(model_geometry_part_t, next_part_index, 0x07);
co(model_geometry_part_t, centroid_primary_node_index, 0x08);
co(model_geometry_part_t, centroid_secondary_node_index, 0x0a);
co(model_geometry_part_t, centroid, 0x14);
co(model_geometry_part_t, field_44, 0x44);
co(model_geometry_part_t, field_48, 0x48);
co(model_geometry_part_t, field_54, 0x54);

typedef struct model_shader_reference_t {
  tag_reference shader;
  int16_t permutation_index;
  byte pad_12[0x20 - 0x12];
} model_shader_reference_t;
cs(model_shader_reference_t, 0x20);
co(model_shader_reference_t, permutation_index, 0x10);

/* Transparent-part sort record, filled by rasterizer_model_transparent_geometry_submit and linked below. */
typedef struct render_sort_filth_t {
  int16_t *previous_group_presorted_index_reference;
  int16_t *next_group_presorted_index_reference;
  int16_t group_index;
  int16_t next_part_index;
  int16_t part_index;
  byte pad_0e[2];
} render_sort_filth_t;
cs(render_sort_filth_t, 0x10);

void render_model_parts(int model_tag, int region_permutation_indices,
                  int *node_matrices, int object_index,
                  short geometry_detail_level_index,
                  short forced_shader_permutation_index, unsigned char flags)
{
  model_definition_t *model;
  char shader_valid;
  char shader_transparent;
  short geometry_index;
  model_region_t *region;
  model_geometry_part_t *part;
  model_shader_reference_t *shader_reference;
  shader_base *shader;
  int pass;
  unsigned int last_pass;
  int region_index;
  int part_index;
  tag_block *parts;
  shader_model *model_shader;
  short permutation_index;
  unsigned int sort_filth_count;
  int sort_slot;
  render_sort_filth_t sort_filth[MAXIMUM_PARTS_PER_MODEL_GEOMETRY];
  real_point3d centroid;

  model = (model_definition_t *)model_tag;
  pass = 0;
  last_pass = (unsigned int)((unsigned char)~flags & 2);

  do {
    region_index = 0;
    sort_filth_count = 0;

    if (0 < model->regions.count) {
      do {
        region = tag_block_get_element(&model->regions, region_index,
                                       sizeof(model_region_t));

        if (*(char *)(region_index + region_permutation_indices) != NONE) {
          model_region_permutation_t *permutation;
          permutation = tag_block_get_element(
            &region->permutations,
            (int)*(char *)(region_index + region_permutation_indices),
            sizeof(model_region_permutation_t));
          geometry_index =
            permutation->geometry_indices[geometry_detail_level_index];

          if (RENDER_MODEL_NO_GEOMETRY == '\0' && geometry_index != NONE) {
            model_geometry_t *geometry;
            geometry = tag_block_get_element(&model->geometries,
                                             (int)geometry_index,
                                             sizeof(model_geometry_t));
            part_index = 0;
            parts = &geometry->parts;

            if (0 < geometry->parts.count) {
              int pi;
              pi = 0;
              do {
                part = tag_block_get_element(parts, pi,
                                             sizeof(model_geometry_part_t));
                shader_reference = tag_block_get_element(
                  &model->shaders, (int)part->shader_index,
                  sizeof(model_shader_reference_t));
                shader = tag_get(TAG_GROUP_SHDR,
                                 shader_reference->shader.tag_index);

                shader_valid = ((char (*)(int))shader_type_is_valid_for_model)(
                  (int)(unsigned short)shader->type);

                if (shader_valid != '\0' && (part->flags & 1) == 0) {
                  shader_transparent = shader_type_is_transparent(shader->type);

                  if (shader_transparent != '\0') {
                    /* Transparent shader — pass 2 */
                    if ((short)pass == 2) {
                      if ((flags & 2) != 0) {
                        display_assert(
                          "!TEST_FLAG(flags, _render_model_shadow_bit)",
                          "c:\\halo\\SOURCE\\models\\models.c", 0x1ba, 1);
                        system_exit(-1);
                      }
                      if (part->centroid_primary_node_index < 0 ||
                          (int)part->centroid_primary_node_index >=
                            model->nodes.count) {
                        display_assert(
                          "part->centroid_primary_node_index>=0 && "
                          "part->centroid_primary_node_index<"
                          "model->nodes.count",
                          "c:\\halo\\SOURCE\\models\\models.c", 0x1bd, 1);
                        system_exit(-1);
                      }
                      if (part->centroid_secondary_node_index < 0 ||
                          (int)part->centroid_secondary_node_index >=
                            model->nodes.count) {
                        display_assert(
                          "part->centroid_secondary_node_index>=0 && "
                          "part->centroid_secondary_node_index<"
                          "model->nodes.count",
                          "c:\\halo\\SOURCE\\models\\models.c", 0x1be, 1);
                        system_exit(-1);
                      }
                      matrix_transform_point(
                        (float *)((int)part->centroid_primary_node_index * 0x34 +
                                  *node_matrices),
                        &part->centroid, &centroid);
                      permutation_index = forced_shader_permutation_index;
                      if (forced_shader_permutation_index == 0) {
                        permutation_index = shader_reference->permutation_index;
                      }

                      sort_slot = (int)(short)sort_filth_count;
                      rasterizer_model_transparent_geometry_submit(
                        (void *)shader, (short)permutation_index,
                        (int)&part->field_44, NONE, part->field_48,
                        (int)&part->field_54, NONE, (float *)&centroid,
                        (void *)&sort_filth[sort_slot]);

                      if ((short)sort_filth_count <
                            MAXIMUM_PARTS_PER_MODEL_GEOMETRY &&
                          sort_filth[sort_slot].group_index != NONE &&
                          (flags & 1) == 0 &&
                          (part->next_part_index > '\0' ||
                           part->previous_part_index > '\0')) {
                        sort_filth[sort_slot].part_index = (short)part_index;
                        sort_filth[sort_slot].next_part_index =
                          (short)part->next_part_index;
                        sort_filth_count = sort_filth_count + 1;
                      }
                    }
                  } else {
                    /* Non-transparent shader */
                    if (shader->type == _shader_type_model) {
                      model_shader = ((shader_model *(*)(void *, int))
                                        shader_get_and_verify_type)(
                        shader, _shader_type_model);
                      if ((model_shader->field_28 & 8) != 0) {
                        /* Environment-mapped shader — pass 1 */
                        if ((short)pass == 1) {
                          if ((flags & 2) != 0) {
                            display_assert(
                              "!TEST_FLAG(flags, _render_model_shadow_bit)",
                              "c:\\halo\\SOURCE\\models\\models.c", 0x1eb, 1);
                            system_exit(-1);
                          }
                          permutation_index = forced_shader_permutation_index;
                          if (forced_shader_permutation_index == 0) {
                            permutation_index =
                              shader_reference->permutation_index;
                          }
                          rasterizer_model_draw(
                            (int)shader, (int)permutation_index,
                            (int)&part->field_44, NONE, part->field_48,
                            (int)&part->field_54, NONE);
                        }
                        goto next_part;
                      }
                    }

                    /* Opaque shader — pass 0 */
                    if ((short)pass == 0) {
                      if ((flags & 2) == 0) {
                        permutation_index = forced_shader_permutation_index;
                        if (forced_shader_permutation_index == 0) {
                          permutation_index =
                            shader_reference->permutation_index;
                        }
                        rasterizer_model_draw(
                          (int)shader, (int)permutation_index,
                          (int)&part->field_44, NONE, part->field_48,
                          (int)&part->field_54, NONE);
                        rasterizer_debug_model_vertices(
                          object_index, node_matrices, (unsigned char *)part);
                      } else {
                        permutation_index = forced_shader_permutation_index;
                        if (forced_shader_permutation_index == 0) {
                          permutation_index =
                            shader_reference->permutation_index;
                        }
                        rasterizer_shadow_model_draw((void *)shader, (int)permutation_index,
                                     (void *)&part->field_44,
                                     (void *)&part->field_54);
                      }
                    }
                  }
                }
              next_part:
                part_index = part_index + 1;
                pi = (int)(short)part_index;
              } while (pi < parts->count);
            }
          }
        }

        region_index = (int)(short)((short)region_index + 1);
      } while (region_index < model->regions.count);

      /* Link each transparent part's sort record to its next part's. */
      if (0 < (short)sort_filth_count) {
        unsigned int i;
        unsigned int sort_total;
        sort_total = sort_filth_count & 0xffff;
        for (i = 0; i < sort_total; i++) {
          short j;
          short next_part_index = sort_filth[i].next_part_index;
          for (j = 0; j < (short)sort_filth_count; j++) {
            if (next_part_index == sort_filth[j].part_index &&
                0 < next_part_index) {
              *sort_filth[i].next_group_presorted_index_reference =
                sort_filth[j].group_index;
              *sort_filth[j].previous_group_presorted_index_reference =
                sort_filth[i].group_index;
              break;
            }
          }
        }
      }
    }

    pass = pass + 1;
    if ((short)last_pass < (short)pass) {
      return;
    }
  } while (1);
}

/* biped_update_dead (0x1a6280)
 * Biped death state handler. After a biped is killed, decides which
 * post-death sub-state to enter: limp-noodle (ragdoll-like), dying-airborne
 * (fell while dying), or normal dying. Checks limp-noodle counter, airborne
 * ticks, and animation state.
 * Register args: @edi = unit_handle, @ebx = pointer to state byte pair.
 * Confirmed from caller 0x1a6350 @001a65ed. */
void biped_update_dead(int unit_handle, char *state_out)
{
  biped_data_t *biped;
  biped_definition_t *biped_tag;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  biped_tag = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  /* Check limp-noodle state */
  if ((biped->flags & 0x20) != 0 &&
      biped->limp_body_current_relaxation_iterations <
        biped->limp_body_max_relaxation_iterations) {
    if (DEBUG_BIPED_LIMP_BODY_DISABLE == 0) {
      biped_limp_noodle_relax_nodes_onto_environment(unit_handle);
    }
    biped_verify_object_vectors(unit_handle, "post-limp-noodle");
    state_out[1] = 0;
    return;
  }

  /* Check dying-airborne state */
  if (biped->airborne_ticks >= 3 &&
      (biped_tag->biped_flags & 0x400) == 0) {
    if (biped->unit.animation_state == 0x18) {
      biped_apply_rotation(unit_handle);
    }
    *state_out = 0x18;
    biped_verify_object_vectors(unit_handle, "post-dying-airborne");
    state_out[1] = 0;
    return;
  }

  /* Normal dying state */
  if (biped->unit.animation_state == 0x18) {
    biped->bank = 0.0f;
    biped_snap_facing(unit_handle);
  }
  *state_out = 0x19;
  biped_verify_object_vectors(unit_handle, "post-dying");
  state_out[1] = 0;
}

/* biped_update (0x1a6350)
 * Per-tick biped update. A free biped runs facing, turning, moving,
 * airborne/landing/slipping/dead and footstep updates; a seated one handles
 * ejection and exit. unit_update_animation runs at the end either way.
 * Always returns 1. */
char biped_update(int unit_handle)
{
  biped_data_t *biped;
  biped_definition_t *biped_tag;
  object_data_t *parent;
  vehicle_data_t *vehicle;
  unit_data_t *weapon_unit;
  int weapon_handle;
  int weapon_handle_saved;
  char result1;
  char result2;
  char shifted;
  signed char anim_state;
  float mag_sq;
  real_vector3d *global_vector;
  real_vector3d *facing;
  short anim_result;
  unsigned char
    state_pair[2]; /* [0]=local_8 (flags), [1]=local_7 (ground contact) */

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  biped_tag = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  if (DEBUG_BIPED_SKIP_UPDATE != 0) {
    return 1;
  }

  /* Debug trace enter */
  if (PROFILE_GLOBAL_ENABLE != 0 && BIPED_UPDATE_SECTION_ENABLED != 0) {
    profile_enter_private(BIPED_UPDATE_SECTION);
  }

  biped_verify_object_vectors(unit_handle, "pre-update");

  state_pair[0] = 0;
  state_pair[1] = 0;

  if (biped->unit.object.parent_object_index.value != NONE) {
    parent = object_get_and_verify_type(
      biped->unit.object.parent_object_index.value, _object_mask_all);

    if (parent->type == _object_type_vehicle) {
      vehicle = object_get_and_verify_type(
        biped->unit.object.parent_object_index.value, _object_mask_vehicle);

      biped_vehicle_speech(unit_handle);

      if ((biped->unit.control_flags & 0x40) != 0) {
        unit_try_and_exit_seat(unit_handle);
      }

      if (RIDER_EJECTION != 0 &&
          vehicle->unit.object.up.k < REAL_ZERO_POOL &&
          (vehicle->unit.object.flags & 0x2) != 0) {
        unit_exit_seat_end(unit_handle);
      }
    } else if (parent->type == _object_type_biped) {
      state_pair[0] = (parent->damage_flags & 0x4) | 0x20;
    }
  } else {
    /* Biped is free (not seated) */
    biped_snap_facing(unit_handle);

    if ((biped->unit.object.damage_flags & 0x4) != 0 ||
        (biped_tag->biped_flags & 0x44) == 0) {
      facing = &biped->unit.desired_facing_vector;
      biped->unit.desired_facing_vector.k = 0.0f;
      if (normalize3d(facing) == 0.0f) {
        global_vector = GLOBAL_FORWARD3D;
        *facing = *global_vector;
      }
    }

    anim_state = (signed char)biped->unit.animation_state;
    switch (anim_state) {
    case 0:
    case 2:
    case 3:
      biped->state = 0;
      break;
    case 4:
    case 5:
    case 6:
    case 7:
      biped->state = 1;
      break;
    default:
      biped->state = 2;
      break;
    }

    mag_sq = biped->unit.throttle.x * biped->unit.throttle.x +
             biped->unit.throttle.y * biped->unit.throttle.y +
             biped->unit.throttle.z * biped->unit.throttle.z;
    if (mag_sq < REAL_0_01_POOL) {
      global_vector = GLOBAL_ZERO_VECTOR3D;
      biped->unit.throttle.x = global_vector->i;
      biped->unit.throttle.y = global_vector->j;
      biped->unit.throttle.z = global_vector->k;
    }

    if ((biped->flags & (1 << _biped_airborne_bit)) != 0) {
      if (biped->airborne_ticks < 0x7f) {
        biped->airborne_ticks++;
      }
    } else {
      biped->airborne_ticks = 0;
    }

    if ((biped->flags & (1 << _biped_slipping_bit)) != 0) {
      if (biped->slipping_ticks < 0x7f) {
        biped->slipping_ticks++;
      }
    } else {
      biped->slipping_ticks = 0;
    }

    state_pair[0] = 0;
    state_pair[1] = biped->unit.control_flags & 1;

    biped_verify_object_vectors(unit_handle, "pre-turning");

    if ((biped->unit.object.damage_flags & 0x4) == 0) {
#ifdef HALO_RNG_TRACE
      /* The two vectors biped_update_turning dots, read here rather than reconstructed.
       * The x component alone separates "equal" from "different"; the paired
       * capture already showed the dot is a bit-exact constant, so this only
       * has to say WHICH of the two is standing still.  +0x1b4 goes out whole
       * because bit 0 selects unit_update's static arm, which writes the
       * current facing into the desired-facing slot. */
      RNG_TRACE_EX(RNG_TRACE_KIND_DESIRED_X,
                   REAL_BITS(biped->unit.desired_facing_vector.i),
                   unit_handle);
      RNG_TRACE_EX(RNG_TRACE_KIND_CURRENT_X,
                   REAL_BITS(biped->unit.object.forward.i), unit_handle);
      RNG_TRACE_EX(RNG_TRACE_KIND_UNIT_FLAGS, biped->unit.flags, unit_handle);
      RNG_TRACE_EX(RNG_TRACE_KIND_TURN_GATES,
                   (unsigned int)biped->state |
                     ((unsigned int)biped->unit.base_seat_index << 8) |
                     ((biped->unit.flags & 0x4000u) << 2) |
                     ((biped->unit.control_flags & 0x100u) << 9),
                   unit_handle);
#endif
#line 1124
      biped_update_turning(unit_handle, state_pair);
      biped_verify_object_vectors(unit_handle, "post-turning");
    }

    biped_update_moving(unit_handle, state_pair);
    biped_verify_object_vectors(unit_handle, "post-moving");

    if ((biped->unit.object.damage_flags & 0x4) != 0) {
      biped_update_dead(unit_handle, (char *)state_pair);
    } else {
      if ((biped->flags & (1 << _biped_airborne_bit)) != 0) {
        biped_update_airborne(unit_handle, (char *)state_pair);
      } else if (biped->landing != NONE) {
        biped_update_landing(unit_handle, (char *)state_pair);
      } else if ((biped->flags & (1 << _biped_slipping_bit)) != 0) {
        biped_update_slipping(unit_handle);
      }
    }

    biped_verify_object_vectors(unit_handle, "post-dead/air/land/slip");

    if (biped->player_melee_ticks != 0) {
      if ((unsigned char)biped->player_melee_ticks ==
          (unsigned char)biped->player_melee_attack_tick) {
        unit_cause_player_melee_damage(unit_handle);
      }
      biped->player_melee_ticks--;
    } else {
      if (biped->unit.player_index.value != NONE &&
          (signed char)biped->unit.control_flags < 0) {
        weapon_unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
        weapon_handle = unit_inventory_get_weapon(
          unit_handle, weapon_unit->current_weapon_index);
        weapon_handle_saved = weapon_handle;

        if (!weapon_prevents_melee_attack(weapon_handle) &&
            (signed char)biped->unit.current_zoom_level == -1) {
          unit_animation_start_action(unit_handle, 7);
          weapon_stop_reload(weapon_handle_saved);
          first_person_weapon_message_from_unit(unit_handle, 4);

          result1 = weapon_get_first_person_animation_time(weapon_handle_saved, 0, 0xd, -1);
          result2 = weapon_get_first_person_animation_time(weapon_handle_saved, 1, 0xd, -1);

          shifted = (signed char)result1 >> 2;
          biped->player_melee_ticks = result1 - shifted;
          biped->player_melee_attack_tick = (result1 - result2) - shifted;
        }
      }
    }

    biped_try_to_make_footsteps(unit_handle);
    biped_falling_danger(unit_handle);
    biped_check_discard(unit_handle);
  }

  /* Post-section: animation state update */
  anim_result = unit_update_animation(unit_handle, (char *)state_pair);
  if (anim_result == 1) {
    biped_jump(unit_handle);
  }

  if ((biped->unit.object.damage_flags & 0x4) != 0 &&
      (biped->unit.object.flags & 0x20) != 0) {
    biped->unit.object.unk_108++;
  } else {
    biped->unit.object.unk_108 = 0;
  }

  biped_verify_object_vectors(unit_handle, "post-update");

  /* Debug trace exit */
  if (PROFILE_GLOBAL_ENABLE != 0 && BIPED_UPDATE_SECTION_ENABLED != 0) {
    profile_exit_private(BIPED_UPDATE_SECTION);
  }

  return 1;
}

/* dialogue_get_vocalization_name (0x1a67b0)
 * The name table at 0x32d7c8 holds two strings per vocalization type. */
char *dialogue_get_vocalization_name(short vocalization_type, unsigned char column)
{
  char *result;

  result = "<error>";
  if (vocalization_type >= 0 && vocalization_type < 0xd1) {
    result = VOCALIZATION_NAMES[(unsigned int)column +
                                 (unsigned int)(short)vocalization_type * 2];
  }
  return result;
}

/* dialogue_get_vocalization_type_by_name (0x1a67e0)
 * Matches against column 0 of the name table at 0x32d7c8. */
short dialogue_get_vocalization_type_by_name(const char *name)
{
  short vocalization_type;
  int compare;

  vocalization_type = 0;
  do {
    compare = csstrcmp(VOCALIZATION_NAMES_CONST[(int)vocalization_type * 2], name);
    if (compare == 0) {
      return vocalization_type;
    }
    vocalization_type = vocalization_type + 1;
  } while (vocalization_type < 0xd1);
  return -1;
}

/* unit_definition_get_active_hud_index (0x1a6820)
 * Picks hud interface 0 or 1 from the unit definition block at +0x2a8,
 * clamped to the block count. */
int unit_definition_get_active_hud_index(int definition, char alternate)
{
  int last_index;
  int *block;
  int index;

  index = (int)(short)(unsigned short)(alternate != '\0');
  last_index = unit_definition_huds(definition).count - 1;
  block = (int *)&unit_definition_huds(definition);
  if (index <= last_index) {
    last_index = index;
  }
  if ((short)last_index < 0) {
    return -1;
  }
  return (int)verify_tag_reference(
    (int *)tag_block_get_element(block, (int)(short)last_index, 0x30));
}

/* unit_definition_get_seat_active_hud_index (0x1a6870)
 * Like unit_definition_get_active_hud_index, but reads the hud block at
 * +0xdc of the seat element. */
int unit_definition_get_seat_active_hud_index(int definition, short seat_index, char alternate)
{
  int seat;
  int last_index;
  int *block;
  int index;

  seat =
    (int)tag_block_get_element((void *)&((unit_definition_t *)definition)->seats, (int)seat_index, 0x11c);
  index = (int)(short)(unsigned short)(alternate != '\0');
  last_index = unit_seat_definition_huds(seat).count - 1;
  block = (int *)&unit_seat_definition_huds(seat);
  if (index <= last_index) {
    last_index = index;
  }
  if ((short)last_index < 0) {
    return -1;
  }
  return (int)verify_tag_reference(
    (int *)tag_block_get_element(block, (int)(short)last_index, 0x30));
}

/* unit_test_speech (0x1a68d0) — unit dialogue speech slot allocation.
 *
 * Resolves a vocalization type to a sound definition index via the unit's
 * dialogue tag (udlg). Walks the vocalization fallback table at 0x2b6420
 * when the primary type yields no sound. Evaluates priority-based
 * preemption of the current speech slot against timing thresholds in the
 * global tables at 0x2b65c4 (short[11]) and 0x2b65dc (float[11]).
 *
 * Returns a result code:
 *   0 = no speech possible
 *   1 = interrupt current speech
 *   2 = immediate slot available or priority preemption
 *   3 = priority exceeds current threshold
 *
 * Source file: unit_dialogue.c, lines 0x80–0x82 asserts.
 */
/* 'udlg' tag: one sound reference per vocalization type. Size unproven. */
typedef struct dialogue_definition_t {
  byte pad_00[0x10];
  tag_reference vocalizations[NUMBER_OF_VOCALIZATION_TYPES];
} dialogue_definition_t;
co(dialogue_definition_t, vocalizations, 0x10);

short unit_test_speech(int unit_handle, short priority, char param_3, char param_4,
                   int *out_last_speech_finished_time, short *vocalization_type_ref,
                   int *sound_definition_index_ref)
{
  unit_data_t *unit;
  dialogue_definition_t *dialogue;
  short voc_type;
  int snd_def_idx;
  short slot_priority;
  short slot_secondary;
  short max_priority;
  int priority_int;
  int result;
  char can_queue;
  float timing_threshold;
  short override_priority;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  game_time_get();

  result = 0;

  if (vocalization_type_ref == NULL) {
    display_assert("vocalization_type_reference",
                   "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x80, 1);
    system_exit(-1);
  }
  if (sound_definition_index_ref == NULL) {
    display_assert("sound_definition_index_reference",
                   "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x81, 1);
    system_exit(-1);
  }
  /* XBE: test si,si; jl / cmp si,0xb; jl. */
  if (priority < 0 || priority >= NUMBER_OF_UNIT_SPEECH_PRIORITIES) {
    display_assert(
      "(priority >= 0) && (priority < NUMBER_OF_UNIT_SPEECH_PRIORITIES)",
      "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x82, 1);
    system_exit(-1);
  }

  voc_type = *vocalization_type_ref;
  snd_def_idx = *sound_definition_index_ref;

  /* Walk the dialogue tag's vocalization table with fallback chain */
  if (snd_def_idx == NONE && unit->dialogue_index != NONE && voc_type != -1) {
    dialogue = tag_get(TAG_GROUP_UDLG, unit->dialogue_index);
    do {
      /* XBE: cmp bx, 0xd1; jl. < NUMBER_OF_VOCALIZATION_TYPES (209). */
      if (voc_type < 0 || voc_type >= NUMBER_OF_VOCALIZATION_TYPES) {
        display_assert("(vocalization_type >= 0) && (vocalization_type < "
                       "NUMBER_OF_VOCALIZATION_TYPES)",
                       "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x90, 1);
        system_exit(-1);
      }
      snd_def_idx = dialogue->vocalizations[voc_type].tag_index;
      if (param_3 == '\0' || snd_def_idx != NONE ||
          (game_connection() == 0 && AI_DEBUG_FORCE_VOCALIZATIONS != '\0')) {
        break;
      }
      voc_type = *(short *)(VOCALIZATION_FALLBACK_TYPES_ADDRESS + (int)voc_type * 2);
    } while (voc_type != -1);
  }

  /* Check if unit can speak (flag check and game connection) */
  if ((unit->object.damage_flags & 4) != 0 && priority != _unit_speech_death) {
    goto done;
  }
  if (game_connection() == 0 && AI_DEBUG_FORCE_VOCALIZATIONS != '\0') {
    /* pass through */
  } else if (snd_def_idx == NONE) {
    goto done;
  }

  /* Evaluate speech slot priority */
  slot_priority = (short)unit->unk_824;
  if (slot_priority == _unit_speech_none) {
    result = 2;
  } else {
    slot_secondary = (short)unit->unk_872;
    /* max_priority = MAX(slot_priority, slot_secondary), as a ternary so
     * the compare and branch order follow the reference. */
    max_priority =
      (slot_priority > slot_secondary) ? slot_priority : slot_secondary;

    priority_int = (int)priority;

    /* XBE subtracts 2/5/3. */
    switch (priority_int) {
    case _unit_speech_pain:
    case _unit_speech_involuntary:
    case _unit_speech_death:
      if (unit->speech_played != '\0' && unit->speech_sound_timer == 0 &&
          priority > max_priority) {
        slot_priority = _unit_speech_none;
        max_priority = slot_secondary;
      }
      break;
    }

    override_priority = *(short *)(SPEECH_OVERRIDE_PRIORITIES_ADDRESS + priority_int * 2);
    if (override_priority >= max_priority) {
      result = 3;
    } else if (priority >= _unit_speech_involuntary &&
               override_priority >= slot_priority) {
      result = 2;
    } else if (param_4 != '\0') {
      timing_threshold = *(float *)(SPEECH_TIMING_THRESHOLDS_ADDRESS + priority_int * 4);
      if (timing_threshold != 0.0f) {
        if (timing_threshold == REAL_MAX_POOL) {
          /* REAL_MAX => always interrupt */
          can_queue = 1;
        } else {
          can_queue = ((int)(short)unit->speech_post_delay_timer +
                       (int)(short)unit->speech_sound_timer <
                       (int)(short)(timing_threshold * 30.0f)) ?
                        1 :
                        0;
        }
        if (can_queue) {
          if (priority <= max_priority) {
            if (priority <= slot_secondary) {
              can_queue = 0;
            } else {

              switch ((int)slot_priority) {
              case _unit_speech_pain:
              case _unit_speech_involuntary:
                can_queue = 1;
                break;
              }
              if (priority == _unit_speech_scripted)
                can_queue = 1;
            }
          }
          if (can_queue)
            result = 1;
        }
      }
    }
  }

done:
  *vocalization_type_ref = voc_type;
  *sound_definition_index_ref = snd_def_idx;
  if (out_last_speech_finished_time != NULL) {
    *out_last_speech_finished_time = unit->last_speech_finished_time;
  }
  return result;
}

/* unit_is_speaking (0x1a6bc0) */
int unit_is_speaking(int unit_handle)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  return (short)unit->unk_824 > 0;
}

/* unit_dialogue_determine_variant (0x1a6bf0)
 * Selects dialogue variant for the unit if none is set. */
void unit_dialogue_determine_variant(int unit_handle)
{
  unit_data_t *unit;
  char *unit_tag;
  int *dialogue_block;
  int16_t *variant;
  uint16_t count;
  int16_t i;
  int16_t variants[16];

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  if (unit->object.unk_110 == 0) {
    dialogue_block = (int *)&unit_definition_dialogue_variants(unit_tag);
    count = 0;
    i = 0;
    if (0 < *dialogue_block) {
      do {
        variant =
          (int16_t *)tag_block_get_element(dialogue_block, (int)i, 0x18);
        if (*variant < 100) {
          if (count >= 0xf) {
            error(2,
                  "unit_dialogue_determine_variant overflowed variant array");
            break;
          }
          variants[(int16_t)count] = *variant;
          count++;
        }
        i++;
      } while ((int)i < *dialogue_block);
      if ((int16_t)count > 0) {
        unit->object.unk_110 =
          variants[UNIT_DIALOGUE_VARIANT_COUNTER % (int)(int16_t)count];
        UNIT_DIALOGUE_VARIANT_COUNTER = UNIT_DIALOGUE_VARIANT_COUNTER + 1;
      }
    }
  }
}

/* unit_get_speech_priority_name (0x1a6ca0)
 * Name table at 0x32de50, one string per speech priority. */
char *unit_get_speech_priority_name(short priority)
{
  char *result;

  result = "<error>";
  if (priority >= 0 && priority < 0xb) {
    result = SPEECH_PRIORITY_NAMES[(short)priority];
  }
  return result;
}

/* unit_get_speech_priority_by_name (0x1a6cd0)
 * Returns 0, not NONE, when no name matches. */
short unit_get_speech_priority_by_name(const char *name)
{
  unsigned short result;
  unsigned short i;

  result = 0;
  i = 0;
  do {
    if (csstrcmp(SPEECH_PRIORITY_NAMES[(short)i], name) == 0) {
      return (short)i;
    }
    i++;
  } while ((short)i < 0xb);
  return (short)result;
}

/* unit_describe_speech (0x1a6d10)
 * Writes the current speech sound name ("<none>" when silent) into the
 * buffer, optionally prefixed with its vocalization name, and returns the
 * buffer. */
char *unit_describe_speech(int unit_handle, char full_path, int16_t max_len,
                   char *output)
{
  unit_data_t *unit;
  char *current_name;
  char *found;
  char *variant_name;
  int16_t dialogue_index;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if (unit->unk_824 == 0) {
    snprintf(output, (int)max_len, "<none>");
    return output;
  }

  current_name = "<none>";
  if (unit->unk_828 != NONE) {
    current_name = (char *)tag_get_name(unit->unk_828);
  }

  if (full_path == '\0') {
    found = strrchr(current_name, '\\');
    if (found != NULL) {
      current_name = found + 1;
    }
  } else {
    while (current_name != NULL) {
      found = crt_strchr(current_name, '\\');
      if (found == NULL) {
        break;
      }
      current_name = found + 1;
    }
  }

  dialogue_index = (int16_t)unit->unk_826;
  if (dialogue_index != -1) {
    if (full_path == '\0') {
      variant_name = dialogue_get_vocalization_name(dialogue_index, 0);
      snprintf(output, (int)max_len, "%s", variant_name);
      return output;
    }
    variant_name = dialogue_get_vocalization_name(dialogue_index, 0);
    snprintf(output, (int)max_len, "%s %s", variant_name, current_name);
    return output;
  }

  snprintf(output, (int)max_len, "%s", current_name);
  return output;
}

/* unit_lose_speech (0x1a6e20) — unit_dialogue_log_lost_speech
 *
 * Logs a "lost speech" debug message when a speech item is being replaced.
 * Only produces output when the debug flag at 0x5aca56 is nonzero.
 *
 * Looks up the unit's tag name via tag_get('unit'), then resolves the speech
 * item's dialogue name either via dialogue_get_vocalization_name (if speech_item+2 != -1) or
 * via tag_get_name (if speech_item+4 != -1), falling back to "<unknown>".
 * Logs "lost <waiting|queued> speech <name>" via console_printf.
 *
 * Register args: unit_handle @<eax>, speech_item @<ecx>.
 * Stack arg: priority (short) — 2 = "waiting", otherwise "queued".
 *
 * Confirmed: PUSH EAX (unit_handle) / MOV ESI,ECX (speech_item) at entry.
 * Confirmed: PUSH 0x3 for object_get_and_verify_type.
 * Confirmed: CMP word [EBP+0x8],0x2 for priority check.
 * Confirmed: tag_get(0x756e6974, *unit) for unit tag name.
 * Confirmed: console_printf(0, "%s: lost %s speech %s", ...) at 0xff4d0.
 */
void unit_lose_speech(int unit_handle, void *speech_item, short priority)
{
  unit_data_t *unit;
  char *unit_tag;
  char *speech_name;
  char *lost_type;
  int16_t dialogue_index;
  int tag_index;
  char *found;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if (speech_item == NULL) {
    display_assert("speech_item", "c:\\halo\\SOURCE\\units\\unit_dialogue.c",
                   0x415, 1);
    system_exit(-1);
  }
  if (AI_PRINT_LOST_SPEECH != '\0') {
    unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
    dialogue_index = ((unit_speech_item_t *)speech_item)->vocalization_type;
    if (dialogue_index != (int16_t)-1) {
      speech_name = (char *)dialogue_get_vocalization_name(dialogue_index, 0);
    } else {
      tag_index = ((unit_speech_item_t *)speech_item)->sound_definition_index;
      if (tag_index != -1) {
        speech_name = (char *)tag_get_name(tag_index);
        found = strrchr(speech_name, '\\');
        if (found != NULL) {
          speech_name = found + 1;
        }
      } else {
        speech_name = "<unknown>";
      }
    }
    if (priority == _unit_speech_pain) {
      lost_type = "waiting";
    } else {
      lost_type = "queued";
    }
    console_printf(0, "%s: lost %s speech %s", object_definition_model_name(unit_tag),
                   lost_type, speech_name);
  }
}

/* unit_speak (0x1a6ef0) — unit_dialogue_queue_speech_item
 *
 * Queues or promotes a speech item for the unit's dialogue system.
 * Three priority levels (param_2):
 *   1 = queue into backup slot (unit+0x368)
 *   2 = promote to current slot (unit+0x338), evicting existing if present
 *   3 = promote to current and clear backup if identical
 *
 * Copies 0x30 bytes (12 dwords, REP MOVSD with ECX=0xC) of speech data.
 * After promotion, copies timing fields from the speech item and computes
 * a vocalization timer from the sound tag's duration:
 *   timer = (duration_ms * 30) / 1000  (converting ms to 30Hz ticks).
 * If no sound tag is set, asserts unless it's a standalone server
 * or force_vocalizations is off.
 *
 * Confirmed: ADD ESP,0x8 after object_get_and_verify_type (cdecl, 2 args).
 * Confirmed: REP MOVSD with ECX=0xC for 0x30-byte copy.
 * Confirmed: IMUL ECX,0x1e / MUL 0x10624dd3 / SAR 0x6 = divide by 1000.
 * Confirmed: assert "speech_item" at line 0x12E, file unit_dialogue.c.
 * Confirmed: assert "AI_BEHAVIOR(force_vocalizations)" at line 0x168.
 * Confirmed: assert "unit->unit.speech.current.priority > _unit_speech_none"
 *            at line 0x16E.
 */
/* 'snd!' tag: only the field read by unit_speak. Size unproven. */
typedef struct sound_definition_t {
  byte pad_00[0x84];
  int32_t field_84; /* milliseconds; unit_speak converts it to ticks */
} sound_definition_t;
co(sound_definition_t, field_84, 0x84);

void unit_speak(int unit_handle, short priority, void *speech_item)
{
  unit_data_t *unit;
  int sound_tag_index;
  sound_definition_t *sound_tag;
  int duration_ticks;
  short game_conn;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if (speech_item == NULL) {
    display_assert("speech_item", "c:\\halo\\SOURCE\\units\\unit_dialogue.c",
                   0x12e, 1);
    system_exit(-1);
  }

  /* If unit has AI-controlled speech bit set, only death speech gets through. */
  if ((unit->object.damage_flags & 4) != 0 &&
      *(int16_t *)speech_item != _unit_speech_death) {
    return;
  }

  if (priority >= _unit_speech_pain) {
    /* Promoting to current slot — log existing speech being evicted */
    if ((int16_t)unit->unk_824 > 0 && unit->speech_played == '\0') {
      unit_lose_speech(unit_handle, &unit->unk_824, 2);
    }

    memcpy(&unit->unk_824, speech_item, sizeof(unit_speech_item_t));

    /* Talk clears the backup slot if it exists and isn't the same item. */
    if (priority == _unit_speech_talk && (int16_t)unit->unk_872 > 0) {
      if (speech_item != (void *)&unit->unk_872) {
        unit_lose_speech(unit_handle, &unit->unk_872, 1);
      }
      unit->unk_872 = 0;
    }

    unit->speech_pre_delay_timer = unit->unk_832;
    unit->speech_played = 0;
    unit->speech_notified_ai = 0;
    unit->speech_finished = 0;
    unit->speech_impulse_sound_index = NONE;
    unit->speech_post_delay_timer = unit->unk_836;
    unit->speech_ai_delay_timer = unit->unk_834;

    sound_tag_index = unit->unk_828;
    if (sound_tag_index != -1) {
      sound_tag = tag_get(TAG_GROUP_SND, sound_tag_index);
      duration_ticks = sound_tag->field_84 * 30;
      unit->speech_sound_timer = (int16_t)(duration_ticks / 1000);
      return;
    }
    game_conn = game_connection();
    if (game_conn == 0 && AI_DEBUG_FORCE_VOCALIZATIONS != '\0') {
      /* Standalone with force_vocalizations off — use default 45 ticks */
    } else {
      display_assert("AI_BEHAVIOR(force_vocalizations)",
                     "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x168, 1);
      system_exit(-1);
    }
    unit->speech_sound_timer = 45;
    return;
  }

  if (priority == _unit_speech_idle) {
    /* Queue to backup slot */
    if (!((int16_t)unit->unk_824 > 0)) {
      display_assert("unit->unit.speech.current.priority > _unit_speech_none",
                     "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x16e, 1);
      system_exit(-1);
    }
    memcpy(&unit->unk_872, speech_item, sizeof(unit_speech_item_t));
  }
}

/* unit_notify_impulse_sound (0x1a70d0)
 * Initiates direct sound speech on a unit (used for scripted dialogue). */
void unit_notify_impulse_sound(int unit_handle, int sound_tag, int sound_handle)
{
  unit_data_t *unit;
  int result;
  unit_speech_item_t speech_item;
  int vocalization_type;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  vocalization_type = -1;
  result = unit_test_speech(unit_handle, _unit_speech_scripted, 0, 0, 0,
                        (int16_t *)&vocalization_type, &result);
  if ((int16_t)result < 3) {
    result = 2;
  }
  csmemset(&speech_item, 0, sizeof(speech_item));
  speech_item.priority = _unit_speech_scripted;
  speech_item.vocalization_type = -1;
  speech_item.sound_definition_index = sound_tag;
  speech_item.pause_time = 24;
  ai_communication_packet_new(&speech_item.ai);
  unit_speak(unit_handle, (int16_t)result, &speech_item);
  if (unit->unk_828 != sound_tag) {
    display_assert("unit->unit.speech.current.sound_definition_index == "
                   "sound_definition_index",
                   "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x196, 1);
    system_exit(-1);
  }
  unit->speech_impulse_sound_index = sound_handle;
  unit->speech_played = 1;
  unit->speech_pre_delay_timer = 0;
  ai_communication_started(unit_handle, _unit_speech_scripted, 0xffff, unit->unk_840);
}

/* unit_make_damage_sound (0x1a71c0) — unit_dialogue_activation
 *
 * Complex dialogue activation function. Determines the appropriate
 * dialogue type based on the unit's combat situation (seeing enemy,
 * taking damage, proximity to threats). Handles both voluntary (param_3=0)
 * and involuntary (param_3=1) dialogue with different priority systems.
 *
 * For voluntary speech: checks cooldown timers at +0x39e/0x39c/0x39a,
 * random suppression, and maps vehicle/threat state to dialogue type.
 * For involuntary speech: checks weapon projectile data to determine
 * if the projectile is fast, uses AI actor state for threat level.
 *
 * On success, builds a speech item (0x30 bytes) and queues it via
 * unit_speak. Also optionally fires an AI unit effect via
 * ai_handle_unit_effect.
 *
 * Returns 1 on success, 0 on failure.
 *
 * Confirmed: cdecl, 5 stack params.
 * Confirmed: object_get_and_verify_type(unit_handle, 3).
 * Confirmed: dialogue tag at unit+0x334.
 * Confirmed: 0x253f3c = 0.6f, 0x253524 = 0.4f, 0x2549d4 = 0.2f.
 * Confirmed: speech_buf at [EBP-0x44] (0x30 bytes).
 * Confirmed: DAT_005ac9ca = dialogue mute flag.
 * Confirmed: DAT_006325a4 = actor data pointer.
 */
/* 'jpt!' tag: only the fields read in this file. Size unproven. */
typedef struct damage_effect_definition_t {
  byte pad_000[0x120];
  int32_t sound_tag_index; /* played by unit_melee_sound */
  byte pad_124[0x1c6 - 0x124];
  int16_t category;
  byte pad_1c8[0x1f4 - 0x1c8];
  real instantaneous_acceleration;
} damage_effect_definition_t;
co(damage_effect_definition_t, sound_tag_index, 0x120);
co(damage_effect_definition_t, category, 0x1c6);
co(damage_effect_definition_t, instantaneous_acceleration, 0x1f4);

char unit_make_damage_sound(int unit_handle, int *damage_effect_index, char param_3, char param_4,
                  float param_5, float param_6)
{
  unit_data_t *unit;
  char result;
  char is_above_zero;
  char is_above_threshold;
  int dialogue_type;
  int dialogue_obj_handle;
  int16_t ai_effect_type;
  int ai_effect_count;
  unit_speech_item_t speech_item;
  damage_effect_definition_t *damage_effect;
  actor_t *actor;
  int actor_handle;
  char is_fast_projectile;
  char has_threat;
  char urgent;
  int16_t priority;
  short speech_count;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  result = 0;

  if (unit->dialogue_index == NONE)
    return 0;

  dialogue_type = 0;
  is_above_zero = unit->object.unk_168 > REAL_ZERO_POOL;
  is_above_threshold = unit->object.unk_168 >= REAL_0_6_POOL;

  urgent = 0;
  ai_effect_type = -1;
  ai_effect_count = 0;

  if (damage_effect_index != NULL && *damage_effect_index != -1) {
    damage_effect = tag_get(TAG_GROUP_JPT, *damage_effect_index);
    dialogue_type = (int)damage_effect->category;
  }

  if (param_3 != '\0') {
    /* Involuntary dialogue (being shot at) */
    actor_handle = unit->swarm_actor_index.value;
    if (actor_handle == NONE)
      actor_handle = unit->actor_index.value;

    has_threat = 0;
    is_fast_projectile = 0;
    if (*damage_effect_index != -1) {
      damage_effect = tag_get(TAG_GROUP_JPT, *damage_effect_index);
      is_fast_projectile = 1;
      if (damage_effect->instantaneous_acceleration < REAL_2_0_POOL)
        is_fast_projectile = 0;
    }

    if (actor_handle == NONE) {
      has_threat = param_5 + REAL_0_2_POOL < unit->object.unk_168;
    } else {
      actor = datum_get(ACTOR_DATA, actor_handle);
      has_threat = actor->field_06e >= 3;
    }

    if ((int16_t)dialogue_type == 1) {
      dialogue_type = 0x10;
    } else if ((int16_t)dialogue_type == 7) {
      dialogue_type = 0x11;
    } else if (is_fast_projectile != 0) {
      dialogue_type = 0x13;
    } else if (!has_threat) {
      dialogue_type = 0xe;
    } else {
      dialogue_type = (param_4 != '\0' ? 3 : 0) + 0xf;
    }

    unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
    urgent = 1;

    if ((int16_t)dialogue_type != 0xe) {
      ai_effect_type = 2;
      ai_effect_count = ((int16_t)dialogue_type == 0x12) ? 4 : 1;
    }

    if ((int16_t)dialogue_type == -1)
      goto ai_effect;
  } else {
    /* Voluntary dialogue (spotting enemy) */
    if (unit->damage_major_timer != 0)
      return result;

    if ((int16_t)dialogue_type == 1) {
      urgent = 1;
      dialogue_type = 9;
    } else if (is_above_threshold) {
      urgent = 1;
      dialogue_type = 7;
    } else {
      if (unit->damage_minor_timer != 0)
        return result;

      if ((int16_t)unit->damage_minor_sounds > 2)
        return result;

      if (unit->unk_824 != 0) {
        if (random_math_real(
              (unsigned int *)get_global_random_seed_address()) >=
            REAL_0_4_POOL)
          return result;
      }

      dialogue_type = (is_above_zero == 0 ? 2 : 0) + 6;
    }

    if ((int16_t)dialogue_type == -1)
      goto ai_effect;
  }

  dialogue_obj_handle = -1;
  if (param_3 == '\0') {
    priority =
      (int16_t)(urgent ? _unit_speech_involuntary : _unit_speech_pain);
  } else {
    priority = _unit_speech_death;
  }

  speech_count = unit_test_speech(unit_handle, (int)priority, 1, 0, 0,
                              (short *)&dialogue_type, &dialogue_obj_handle);

  if (AI_DEBUG_DISABLE_WOUNDED_SOUNDS == '\0' && speech_count > 0) {
    csmemset(&speech_item, 0, sizeof(speech_item));
    speech_item.priority = priority;
    speech_item.vocalization_type = (int16_t)dialogue_type;
    speech_item.sound_definition_index = dialogue_obj_handle;
    speech_item.pause_time = 7;
    ai_communication_packet_new(&speech_item.ai);
    unit_speak(unit_handle, speech_count, &speech_item);
    result = 1;

    if (is_above_threshold == 0) {
      unit->damage_minor_sounds += 1;
      unit->damage_minor_timer = 30;
      unit->damage_minor_decay_timer = 22;
    } else {
      unit->damage_major_timer = 60;
    }
  }

ai_effect:
  if (ai_effect_type != -1) {
    ai_handle_unit_effect(unit_handle, (int)ai_effect_type, ai_effect_count);
  }

  return result;
}

/* unit_scream (0x1a74d0) — unit_dialogue_scream
 *
 * Triggers a unit scream dialogue event. Maps scream_type [0..5] to a
 * dialogue index via switch:
 *   0 → 10 (pain_body_minor)
 *   1 → random 50/50: 0x27 (scream) or fall through to case 2
 *   2 → 11 (pain_body_major)
 *   3 → 12 (pain_shield)
 *   4 → 13 (pain_falling)
 *   5 → 0xb7 (183, death)
 * Then looks up the dialogue tag at unit+0x334, fetches the sound reference
 * at dialogue_index*0x10+0x1c, and calls unit_test_speech to allocate a speech
 * slot. On success, builds a speech item struct (0x30 bytes) and queues it
 * via unit_speak.
 *
 * Returns 1 on success, 0 if no dialogue tag or sound not available.
 *
 * Confirmed: cdecl, 2 stack params.
 * Confirmed: switch table at 0x1a7638 (6 entries).
 * Confirmed: 0x253398 = 0.5f for random threshold.
 * Confirmed: ai_communication_packet_new = ai_communication_packet_new.
 * Confirmed: merged ADD ESP,0x1c cleans csmemset(3)+packet_new(1)+6ef0(3).
 */
char unit_scream(int unit_handle, int scream_type)
{
  unit_data_t *unit;
  int16_t dialogue_index;
  int dialogue_tag_index;
  char *dialogue_tag;
  uint32_t sound_ref;
  short result;
  unit_speech_item_t speech_item;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  dialogue_index = (int16_t)scream_type;

  if ((int16_t)scream_type < 0 ||
      (int16_t)scream_type >= NUMBER_OF_UNIT_SCREAM_TYPES) {
    display_assert(
      "(scream_type >= 0) && (scream_type < NUMBER_OF_UNIT_SCREAM_TYPES)",
      "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 599, 1);
    system_exit(-1);
  }

  switch ((int16_t)scream_type) {
  case 0:
    dialogue_index = 10;
    break;
  case 1:
    if (random_math_real((unsigned int *)get_global_random_seed_address()) <
        0.5f) {
      dialogue_index = 0x27;
      break;
    }
    /* fall through */
  case 2:
    dialogue_index = 0xb;
    break;
  case 3:
    dialogue_index = 0xc;
    break;
  case 4:
    dialogue_index = 0xd;
    break;
  case 5:
    dialogue_index = 0xb7;
    break;
  default:
    display_assert("!\"unreachable\"",
                   "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x27b, 1);
    system_exit(-1);
  }

  dialogue_tag_index = unit->dialogue_index;
  if (dialogue_tag_index != -1) {
    dialogue_tag = tag_get(TAG_GROUP_UDLG, dialogue_tag_index);
    sound_ref = *(uint32_t *)(dialogue_tag + (int)dialogue_index * 0x10 + 0x1c);
    if (sound_ref != 0xffffffff) {
      result = unit_test_speech(unit_handle, _unit_speech_scream, 1, 0, 0, &dialogue_index,
                            (int *)&sound_ref);
      if (result > 0) {
        csmemset(&speech_item, 0, sizeof(speech_item));
        speech_item.priority = _unit_speech_scream;
        speech_item.vocalization_type = dialogue_index;
        speech_item.sound_definition_index = sound_ref;
        speech_item.pause_time = 7;
        ai_communication_packet_new(&speech_item.ai);
        unit_speak(unit_handle, result, &speech_item);
        return 1;
      }
    }
  }
  return 0;
}

/* unit_find_dialogue_variant (0x1a7650) — dialogue_definition_get_variant
 *
 * Searches the dialogue variants tag block at tag_data+0x2b4 for entries
 * matching the given dialogue_type. Collects up to 16 matching variant
 * indices into a local array. If exactly one match, returns that variant's
 * dialogue tag index. If multiple matches, picks one at random using
 * get_global_random_seed_address. Asserts if the chosen variant's dialogue
 * tag (0x75646c67 = "udlg") fails to load.
 *
 * Returns the dialogue tag index, or -1 if no matching variant found.
 *
 * Confirmed: thiscall — @ecx = tag_data pointer (MOV ESI,ECX at 0x1a7658).
 * Confirmed: 1 stack param = dialogue_type (int16_t).
 * Confirmed: tag_block at tag_data+0x2b4, element_size=0x18.
 * Confirmed: variant dialogue ref at element+0x14.
 * Confirmed: assert string "dialogue_definition_get(dialogue_index)" at
 * 0x2b6874. Confirmed: line 0x408 in unit_dialogue.c.
 */
/* unit definition +0x2b4 dialogue-variant block element. */
typedef struct unit_dialogue_variant_t {
  int16_t variant_number;
  byte pad_02[6];
  tag_reference dialogue;
} unit_dialogue_variant_t;
cs(unit_dialogue_variant_t, 0x18);
co(unit_dialogue_variant_t, dialogue, 0x08);

int unit_find_dialogue_variant(void *tag_data, int dialogue_type)
{
  int *block;
  int16_t match_count;
  int16_t match_indices[16];
  int16_t i;
  unit_dialogue_variant_t *variant;
  int16_t chosen;
  int chosen_element;
  int dialogue_tag_index;

  block = (int *)&unit_definition_dialogue_variants(tag_data);
  match_count = 0;

  if (*block > 0) {
    for (i = 0; i < *block; i++) {
      variant = tag_block_get_element(block, i, sizeof(unit_dialogue_variant_t));
      if ((int16_t)dialogue_type == -1 ||
          variant->variant_number == (int16_t)dialogue_type) {
        match_indices[match_count] = (int16_t)i;
        match_count++;
      }
    }

    if ((int16_t)match_count > 0) {
      if ((int16_t)match_count == 1) {
        chosen = match_indices[0];
      } else {
        chosen = seed_random_range((unsigned int *)get_global_random_seed_address(),
                              0, (int16_t)match_count);
        chosen = match_indices[chosen];
      }

      chosen_element = (int)tag_block_get_element(block, (int)chosen, 0x18);
      dialogue_tag_index = ((unit_dialogue_variant_t *)chosen_element)->dialogue.tag_index;

      if (tag_get(TAG_GROUP_UDLG, dialogue_tag_index) == NULL) {
        display_assert("dialogue_definition_get(dialogue_index)",
                       "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x408, 1);
        system_exit(-1);
      }

      return dialogue_tag_index;
    }
  }

  return -1;
}

/* unit_dialogue_setup (0x1a7730) — unit_dialogue_select_variant
 *
 * Selects a dialogue variant for the unit. Queries unit_find_dialogue_variant with the
 * unit's current dialogue type (+0x6e). If that fails (returns -1), tries
 * type 0 (default). If that also fails, tries type -1 (wildcard).
 * Stores the resulting dialogue tag index at unit+0x334.
 *
 * Confirmed: @eax = unit_handle (PUSH EAX at 001a7734).
 * Confirmed: unit_find_dialogue_variant takes @ecx = tag data pointer, stack param = type.
 * Confirmed: three fallback calls in sequence.
 * Confirmed: result stored at [ESI + 0x334].
 */
void unit_dialogue_setup(int unit_handle)
{
  unit_data_t *unit;
  char *tag_data;
  int result;
  int16_t dialogue_type;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  tag_data = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);

  dialogue_type = unit->object.unk_110;
  if (dialogue_type > 0) {
    result = unit_find_dialogue_variant(tag_data, (int)dialogue_type);
    if (result != -1) {
      unit->dialogue_index = result;
      return;
    }
  }

  result = unit_find_dialogue_variant(tag_data, 0);
  if (result == -1) {
    result = unit_find_dialogue_variant(tag_data, -1);
  }
  unit->dialogue_index = result;
}

/* The real_matrix4x3 at +0x04 of an object_marker, ahead of its matrix. */
#define OBJECT_MARKER_FIELD_04_OFFSET 0x04
#define object_marker_field_04(marker) \
  UNIT_DEFINITION_FIELD(marker, real_matrix4x3, OBJECT_MARKER_FIELD_04_OFFSET)

/* struct damage_data fields between those types.h declares: the damage
 * location (+0x14) and the material type that was hit (+0x4c). */
#define DAMAGE_DATA_LOCATION_OFFSET 0x14
#define DAMAGE_DATA_MATERIAL_TYPE_OFFSET 0x4c
#define damage_data_location(damage) \
  UNIT_DEFINITION_FIELD(damage, location_t, DAMAGE_DATA_LOCATION_OFFSET)
#define damage_data_material_type(damage) \
  UNIT_DEFINITION_FIELD(damage, int16_t, DAMAGE_DATA_MATERIAL_TYPE_OFFSET)

/* unit_dialogue_update (0x1a7790)
 * Updates the unit's dialogue/speech state machine each tick. */
void unit_dialogue_update(int unit_handle)
{
  int16_t value;
  unit_data_t *unit;
  int16_t marker_count;
  char marker_buf[0x6c];
  real_point3d position;
  real_vector3d forward;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if ((unit->flags & 0x100) != 0) {
    unit_dialogue_setup(unit_handle);
    unit->flags &= ~0x100u;
  }

  if ((int16_t)unit->damage_minor_decay_timer > 0) {
    value = unit->damage_minor_decay_timer - 1;
    unit->damage_minor_decay_timer = value;
    if (value == 0 && (int16_t)unit->damage_minor_sounds > 0) {
      unit->damage_minor_sounds -= 1;
      unit->damage_minor_decay_timer = 22;
    }
  }
  if ((int16_t)unit->damage_minor_timer > 0) {
    unit->damage_minor_timer -= 1;
  }
  if ((int16_t)unit->damage_minor_timer > 0) {
    unit->damage_minor_timer -= 1;
  }

  if ((int16_t)unit->unk_824 > 0) {
    if ((int16_t)unit->speech_pre_delay_timer > 0) {
      unit->speech_pre_delay_timer -= 1;
    } else {
      if (unit->speech_played == 0) {
        marker_count = object_get_marker_by_name(
          unit_handle, MARKER_NAME_HEAD, marker_buf, 1);
        if (marker_count == 0) {
          position.x = GLOBAL_ORIGIN3D->x;
          position.y = GLOBAL_ORIGIN3D->y;
          position.z = GLOBAL_ORIGIN3D->z;
          forward.i = GLOBAL_FORWARD3D->i;
          forward.j = GLOBAL_FORWARD3D->j;
          forward.k = GLOBAL_FORWARD3D->k;
          *(int16_t *)marker_buf = 0;
        } else {
          position.x = object_marker_field_04(marker_buf).position.x;
          position.y = object_marker_field_04(marker_buf).position.y;
          position.z = object_marker_field_04(marker_buf).position.z;
          forward.i = object_marker_field_04(marker_buf).forward.i;
          forward.j = object_marker_field_04(marker_buf).forward.j;
          forward.k = object_marker_field_04(marker_buf).forward.k;
        }
        if (unit->unk_828 != NONE) {
          unit->speech_impulse_sound_index = object_impulse_sound_new(
            unit_handle, unit->unk_828, *(int16_t *)marker_buf, &position,
            &forward, 1.0f);
        }
        ai_communication_started(unit_handle, unit->unk_824,
                     unit->unk_826, unit->unk_840);
        unit->speech_played = 1;
      }
      if ((int16_t)unit->speech_ai_delay_timer > 0) {
        unit->speech_ai_delay_timer -= 1;
      }
      if ((int16_t)unit->speech_sound_timer > 0) {
        if ((int16_t)unit->unk_824 < 1) {
          display_assert(
            "unit->unit.speech.current.priority > _unit_speech_none",
            "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x2f5, 1);
          system_exit(-1);
        }
        unit->speech_sound_timer -= 1;
        if (unit->speech_sound_timer == 0) {
          unit->speech_impulse_sound_index = NONE;
        }
      } else {
        if (unit->speech_finished == 0) {
          ai_communication_finished(unit_handle, unit->unk_824,
                       unit->unk_826, 0, -1, unit->unk_840);
          unit->speech_finished = 1;
        }
        if ((int16_t)unit->speech_post_delay_timer > 0) {
          unit->speech_post_delay_timer -= 1;
        }
        if (unit->speech_post_delay_timer == 0) {
          unit->speech_ai_delay_timer = 0;
        }
      }
    }
  }

  if (unit->speech_ai_delay_timer == 0 && unit->speech_notified_ai == 0) {
    ai_communication_notify(unit_handle, unit->unk_824,
                 unit->unk_826, unit->unk_840);
    unit->speech_notified_ai = 1;
  }

  value = unit->unk_824;
  if (value > 0) {
    if (unit->speech_sound_timer == 0 && unit->speech_post_delay_timer == 0) {
      unit->unk_824 = 0;
    }
    value = unit->unk_824;
  }
  if (value == 0 && (int16_t)unit->unk_872 > 0) {
    unit_speak(unit_handle, _unit_speech_talk, &unit->unk_872);
  }
}

/* unit_scripting_set_maximum_vitality (0x1a7a90)
 * Applies damage to an object if it's not dead (bit 2 of +0xB6 clear).
 *
 * body_dmg/shield_dmg are passed BY VALUE. The original (delinked) takes three
 * 4-byte stack args at [ebp+8]/[ebp+0xc]/[ebp+0x10] and does
 *   lea ecx,[ebp+0xc]  (&body_dmg) / lea eax,[ebp+0x10] (&shield_dmg)
 * to box the by-value floats into the pointers object_initialize_vitality expects (1365d0
 * dereferences arg2/arg3). Declaring them as float* and forwarding the pointers
 * reinterprets the float bit-pattern (1.0f == 0x3f800000) as an address and
 * dereferences it — an infinite page-fault storm that froze PoA after the intro
 * (unit_scripting_set_maximum_vitality_evaluate calls this with floats pushed by value). */
void unit_scripting_set_maximum_vitality(int object_index, float body_dmg, float shield_dmg)
{
  object_data_t *object;

  if (object_index != NONE) {
    object = object_get_and_verify_type(object_index, _object_mask_all);
    if ((object->damage_flags & 4) == 0) {
      object_initialize_vitality(object_index, &body_dmg, &shield_dmg);
    }
  }
}

/* units_scripting_set_maximum_vitality (0x1a7ad0)
 * Iterates child objects and applies damage to each alive object.
 *
 * Confirmed: params 2 and 3 are floats, not ints.  The original copies them
 * with plain dword moves (MOV ECX,[EBP+0x10]; MOV [EBP-0x4],ECX @0x1a7af3)
 * — there is no FILD/int-to-float conversion anywhere in the body, so
 * Ghidra's "(float)param_N" cast is invented.  The sibling 0x1a7b50 takes
 * (int, float, float) as well.  The previous int-typed lift was still
 * bit-faithful (it cast the pointers, never the values); the float typing
 * additionally lets callers reproduce the original's FLD/FSTP argument
 * push instead of a PUSH. */
void units_scripting_set_maximum_vitality(int parent_handle, float body_vitality, float shield_vitality)
{
  int iter_state;
  int child;
  object_data_t *object;
  float body;
  float shield;

  child = object_list_get_first(parent_handle, &iter_state);
  while (child != -1) {
    shield = shield_vitality;
    body = body_vitality;
    if (child != -1) {
      object = object_get_and_verify_type(child, _object_mask_all);
      if ((object->damage_flags & 4) == 0) {
        object_initialize_vitality(child, &body, &shield);
      }
    }
    child = object_list_get_next(parent_handle, &iter_state);
  }
}

/* unit_scripting_set_current_vitality (0x1a7b50)
 * Computes and stores body/shield vitality ratios. Triggers events when
 * vitality transitions from nonzero to zero. */
void unit_scripting_set_current_vitality(int datum_handle, float body_vitality, float shield_vitality)
{
  object_datum_t *obj;
  float new_shield_vitality;
  float new_body_vitality;

  if (datum_handle != -1) {
    obj = (object_datum_t *)object_get_and_verify_type(datum_handle, -1);
    if ((obj->damage_flags & 4) == 0) {
      if (obj->vitality[OBJECT_VITALITY_MAXIMUM_SHIELD] <= 0.0f) {
        new_shield_vitality = 0.0f;
      } else if (shield_vitality >= obj->vitality[OBJECT_VITALITY_MAXIMUM_SHIELD]) {
        new_shield_vitality = 1.0f;
      } else {
        new_shield_vitality = shield_vitality / obj->vitality[OBJECT_VITALITY_MAXIMUM_SHIELD];
      }
      if (obj->vitality[OBJECT_VITALITY_MAXIMUM_BODY] <= 0.0f) {
        new_body_vitality = 0.0f;
      } else if (body_vitality >= obj->vitality[OBJECT_VITALITY_MAXIMUM_BODY]) {
        new_body_vitality = 1.0f;
      } else {
        new_body_vitality = body_vitality / obj->vitality[OBJECT_VITALITY_MAXIMUM_BODY];
      }
      if (obj->vitality[OBJECT_VITALITY_SHIELD] > 0.0f &&
          new_shield_vitality <= 0.0f) {
        object_deplete_shield(datum_handle);
      }
      obj->vitality[OBJECT_VITALITY_SHIELD] = new_shield_vitality;
      if (obj->vitality[OBJECT_VITALITY_BODY] > 0.0f &&
          new_body_vitality <= 0.0f) {
        object_deplete_body(datum_handle);
      }
      obj->vitality[OBJECT_VITALITY_BODY] = new_body_vitality;
    }
  }
}

/* units_scripting_set_current_vitality (0x1a7c70)
 * Iterates child objects and calls unit_scripting_set_current_vitality on each.
 *
 * Params 2 and 3 are FLOATS, not ints. The function itself never touches the
 * FPU -- it forwards them bit-exact through EBX/EDI (MOV EBX,[EBP+0xc]
 * @0x1a7c8b, MOV EDI,[EBP+0x10] @0x1a7c8f, pushed at 0x1a7c92/93) -- but its
 * callee unit_scripting_set_current_vitality takes (int, float, float), and its sole caller
 * units_scripting_set_current_vitality_evaluate loads them with FLD and passes them via FSTP. Declaring them int
 * made C convert the float BIT PATTERN numerically at the unit_scripting_set_current_vitality call:
 * body damage 1.0f (0x3f800000) became 1065353216.0f. Corrected here and in
 * kb.json. See lift-learnings 'XCALL type audit' / feedback on float-vs-int
 * params reading the wrong register. */
void units_scripting_set_current_vitality(int parent_handle, float body_damage, float shield_damage)
{
  int iter_state;
  int child;

  child = object_list_get_first(parent_handle, &iter_state);
  while (child != -1) {
    unit_scripting_set_current_vitality(child, body_damage, shield_damage);
    child = object_list_get_next(parent_handle, &iter_state);
  }
}

/* unit_scripting_get_health (0x1a7cc0)
 * Returns the body vitality (0x90) or 0.0 if dead, default if invalid. */
float unit_scripting_get_health(int datum_handle)
{
  object_datum_t *obj;
  float result;

  obj = object_try_and_get_and_verify_type(datum_handle, _object_mask_all);
  result = REAL_NEGATIVE_ONE_POOL;
  if (obj != NULL) {
    if ((obj->damage_flags & 4) != 0) {
      return 0.0f;
    }
    result = obj->vitality[OBJECT_VITALITY_BODY];
  }
  return result;
}

/* unit_scripting_get_shield (0x1a7d00)
 * Returns the shield vitality (0x94) or 0.0 if dead, default if invalid. */
float unit_scripting_get_shield(int datum_handle)
{
  object_datum_t *obj;
  float result;

  obj = object_try_and_get_and_verify_type(datum_handle, _object_mask_all);
  result = REAL_NEGATIVE_ONE_POOL;
  if (obj != NULL) {
    if ((obj->damage_flags & 4) != 0) {
      return 0.0f;
    }
    result = obj->vitality[OBJECT_VITALITY_SHIELD];
  }
  return result;
}

/* unit_scripting_get_grenade_count (0x1a7d40)
 * Sums grenade counts for all grenade types (2 types). */
int unit_scripting_get_grenade_count(int datum_handle)
{
  int i;
  int sum;
  uint8_t *ptr;
  unit_data_t *unit;

  unit = object_try_and_get_and_verify_type(datum_handle, _object_mask_unit);
  sum = 0;
  if (unit != NULL) {
    ptr = unit->grenade_counts;
    i = NUMBER_OF_UNIT_GRENADE_TYPES;
    do {
      sum += (int16_t)(int8_t)*ptr;
      ptr++;
      i--;
    } while (i != 0);
  }
  return sum;
}

/* unit_scripting_impervious (0x1a7d80)
 * Sets or clears bit 23 (0x800000) in unit flags for all child units. */
void unit_scripting_impervious(int object_list_index, char impervious)
{
  int reference_index;
  int unit_index;
  unit_data_t *unit;
  uint32_t flags;

  unit_index = object_list_get_first(object_list_index, &reference_index);
  while (unit_index != -1) {
    unit = object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
    if (unit != NULL) {
      if (impervious != '\0') {
        flags = unit->flags | 0x800000;
      } else {
        flags = unit->flags & ~0x800000;
      }
      unit->flags = flags;
    }
    unit_index = object_list_get_next(object_list_index, &reference_index);
  }
}

/* unit_scripting_start_user_animation_list (0x1a7df0)
 * Returns true only if unit_start_user_animation returns true for ALL child units. */
char unit_scripting_start_user_animation_list(int datum_handle, int param_2, int param_3, int param_4)
{
  char result;
  char started;
  int child;
  char *unit;
  int iter_state;

  result = 1;
  child = object_list_get_first(datum_handle, &iter_state);
  while (child != -1) {
    unit = object_try_and_get_and_verify_type(child, _object_mask_unit);
    if (unit != NULL) {
      if (result &&
          (started = unit_start_user_animation(child, param_2, (void *)param_3, param_4),
           started != '\0')) {
        result = 1;
      } else {
        result = 0;
      }
    }
    child = object_list_get_next(datum_handle, &iter_state);
  }
  return result;
}

/* unit_scripting_has_weapon (0x1a7e70)
 * Guard wrapper around unit_has_weapon_definition_index.
 * Returns false if either handle is NONE. */
char unit_scripting_has_weapon(int unit_handle, int definition_index)
{
  char result;

  result = 0;
  if (unit_handle != -1 && definition_index != -1) {
    result = unit_has_weapon_definition_index(unit_handle, definition_index);
  }
  return result;
}

/* unit_scripting_has_weapon_readied (0x1a7ea0)
 * Checks if the unit's current weapon has the given tag definition. */
char unit_scripting_has_weapon_readied(int unit_handle, int weapon_def_tag)
{
  unit_data_t *unit;
  int weapon;
  object_data_t *weapon_data;

  if (unit_handle != -1 && weapon_def_tag != -1) {
    unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
    weapon = unit_inventory_get_weapon(unit_handle, unit->current_weapon_index);
    if (weapon != -1) {
      weapon_data = object_get_and_verify_type(weapon, 4);
      return weapon_data->tag_index == weapon_def_tag;
    }
  }
  return 0;
}

/* units_initialize (0x1a7f00)
 *
 * Allocates the 8-byte "unit globals" block from game state and asserts it.
 * Reason string "unit_globals", path and line 0x108 (264) are PUSHed verbatim.
 * Second argument is NULL. */
void units_initialize(void)
{
  unit_globals = game_state_malloc("unit globals", NULL, 8);
  assert_halt_at("c:\\halo\\SOURCE\\units\\units.c", 0x108, unit_globals);
}

/* units_initialize_for_new_map (0x1a7f40)
 *
 * Clears the first 4 bytes of unit_globals.  The layout is not recovered
 * here, so the size stays the literal 4 from the PUSH 4 at 0x1a7f45.
 *
 */
void units_initialize_for_new_map(void)
{
  csmemset(unit_globals, 0, 4);
}

/* units_dispose_from_old_map (0x1a7f60)
 *
 * Confirmed: the body is a single RET (C3); no prologue, no side effects.
 * Confirmed: only reference is the unit object_type_definition table at
 *   0x323ca8, slot +0x1c (dispose_from_old_map).
 */
void units_dispose_from_old_map(void)
{
}

/* unit_kill (0x1a7f80) */
void unit_kill(int unit_handle)
{
  object_data_t *obj;

  obj = object_get_and_verify_type(unit_handle, _object_mask_unit);
  obj->damage_flags |= 0x20;
}

/* unit_kill_silent (0x1a7fa0) */
void unit_kill_silent(int unit_handle)
{
  object_data_t *obj;

  obj = object_get_and_verify_type(unit_handle, _object_mask_unit);
  obj->damage_flags |= 0x40;
}

/* unit_kill_no_statistics (0x1a7fc0)
 * Flags the unit to die without statistics. */
void unit_kill_no_statistics(int datum_handle)
{
  object_data_t *obj;

  obj = object_get_and_verify_type(datum_handle, _object_mask_unit);
  obj->unk_183 |= 0x20;
}

/* unit_delete (0x1a7fe0)
 *
 *
 * Object type delete callback (unit type table 0x323ca8, slot +0x2c).
 */
void unit_delete(int unit_handle)
{
}

/* The units globals record that *(0x4e4cf8) points at; size unproven. */
typedef struct {
  int16_t field_00;
  int16_t field_02;
  uint8_t field_04;
} units_globals_t;
co(units_globals_t, field_02, 0x02);
co(units_globals_t, field_04, 0x04);

/* units_update (0x1a7ff0)
 *
 * Per-tick global update for the units subsystem. Reads the units globals
 * pointer at 0x4e4cf8 (a small {int16_t, int16_t, uint8_t} struct) and
 * rotates: copies the "max ticks this frame" (offset +2) into "current
 * ticks" (offset +0), then zeroes the max-ticks field (+2) and the
 * ready-flag byte (+4). Called once per game tick from the main update loop.
 */
void units_update(void)
{
  units_globals_t *p = UNIT_GLOBALS;

  p->field_00 = p->field_02;
  p->field_02 = 0;
  p->field_04 = 0;
}

/* unit_export_function_values (0x1a8010) */
void unit_export_function_values(int unit_handle)
{
  unit_data_t *unit;
  char *unit_tag;
  float value;
  float *function_value;
  int16_t *function_source;
  animation_graph_t *animation_graph;
  animation_t *animation;
  int count;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  function_value = &unit->object.field_d0[1];
  function_source = unit_definition_export_function_sources(unit_tag);
  count = 4;
  do {
    if (*function_source != 0) {
      value = 0.0f;
      switch (*function_source) {
      case 1:
        value = unit->seat_power[0];
        break;
      case 2:
        value = unit->seat_power[1];
        break;
      case 3:
        value = (float)unit->aiming_change * REAL_ONE_OVER_255_POOL;
        break;
      case 4:
        value = unit->mouth_aperture;
        break;
      case 5:
        value = unit->integrated_light_power;
        break;
      case 6:
        if ((unit->object.damage_flags & 4) == 0 &&
            (unit->flags & 0x400000) == 0) {
          value = REAL_ONE_POOL;
        }
        break;
      case 7:
        animation_graph = tag_get(TAG_GROUP_ANTR, unit->object.unk_124);
        animation = tag_block_get_element(&animation_graph->animations,
                                          (int)unit->object.unk_128,
                                          sizeof(animation_t));
        if (unit->object.unk_128 < animation->field_2e) {
          value = (float)(int)unit->object.unk_128 /
                  (float)(int)animation->field_2e;
        } else {
          value =
            REAL_ONE_POOL -
            (float)(int)(int8_t)unit->shield_sap_timeout * REAL_ONE_NINETIETH_POOL;
        }
        break;
      }
      *function_value = value;
    }
    function_source++;
    function_value++;
    count--;
  } while (count != 0);
}

/* unit_persistent_control (0x1a8190)
 *
 * Sets persistent control state on a unit. Stores animation_ticks at offset
 * 0x1c0 and control_flags at offset 0x1c4. Asserts if control_flags has any
 * bits set beyond position 14 (NUMBER_OF_UNIT_CONTROL_FLAGS = 15).
 */
void unit_persistent_control(int unit_handle, int animation_ticks,
                             int control_flags)
{
  unit_data_t *unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if ((control_flags & UNIT_CONTROL_FLAGS_INVALID_MASK) != 0) {
    display_assert(
      "VALID_FLAGS(persistent_control_flags, NUMBER_OF_UNIT_CONTROL_FLAGS)",
      "c:\\halo\\SOURCE\\units\\units.c", 0x605, 1);
    system_exit(-1);
  }

  unit->persistent_control_flags = control_flags;
  unit->persistent_control_timer = animation_ticks;
}

/* 'antr' weapon-type block element (weapon class +0xb0). */
typedef struct animation_graph_weapon_type_t {
  byte pad_00[0x30];
  tag_block animations; /* int16 animation indices */
} animation_graph_weapon_type_t;
cs(animation_graph_weapon_type_t, 0x3c);

/* Bounds block read by aiming_screen_apply: per-frame yaw/pitch steps and the
 * frame counts on each side of center. */
typedef struct {
  float yaw_per_frame[2];
  int16_t yaw_frame_count[2];
  float pitch_per_frame[2];
  int16_t pitch_frame_count[2];
} animation_aiming_screen_t;
cs(animation_aiming_screen_t, 0x18);
co(animation_aiming_screen_t, yaw_frame_count, 0x08);
co(animation_aiming_screen_t, pitch_per_frame, 0x0c);
co(animation_aiming_screen_t, pitch_frame_count, 0x14);

/* Element of the animation graph's unit-seat block (antr +0x0c), stride
 * 0x64. */
typedef struct {
  char label[0x20];                 /* seat label; length unproven */
  animation_aiming_screen_t looking_screen; /* +0x20 */
  uint8_t pad_38[0x8];
  tag_block animations;             /* +0x40 int16 animation indices */
  tag_block ik_points;              /* +0x4c 0x40-byte elements */
  tag_block weapon_classes;         /* +0x58 */
} animation_graph_unit_seat_t;
cs(animation_graph_unit_seat_t, 0x64);
co(animation_graph_unit_seat_t, looking_screen, 0x20);
co(animation_graph_unit_seat_t, animations, 0x40);
co(animation_graph_unit_seat_t, ik_points, 0x4c);
co(animation_graph_unit_seat_t, weapon_classes, 0x58);

/* Element of a unit seat's weapon-class block, stride 0xbc. */
typedef struct {
  char label[0x20];                 /* weapon label; length unproven */
  uint8_t pad_20[0x40];
  animation_aiming_screen_t aiming_screen; /* +0x60 */
  uint8_t pad_78[0x20];
  tag_block animations;             /* +0x98 int16 animation indices */
  tag_block ik_points;              /* +0xa4 0x40-byte elements */
  tag_block weapon_types;           /* +0xb0 0x3c-byte elements, label first */
} animation_graph_weapon_class_t;
cs(animation_graph_weapon_class_t, 0xbc);
co(animation_graph_weapon_class_t, aiming_screen, 0x60);
co(animation_graph_weapon_class_t, animations, 0x98);
co(animation_graph_weapon_class_t, ik_points, 0xa4);
co(animation_graph_weapon_class_t, weapon_types, 0xb0);

int unit_get_seat_enter_position(int unit_handle, int target_unit_handle,
                                 int16_t seat_index, real_point3d *out_pos_a,
                                 real_point3d *out_pos_b, real_point3d *out_pos_c)
{
  char marker_name[256];
  object_marker hint_marker;
  real_matrix4x3 mode_matrix;
  real_matrix4x3 enter_position_matrix;
  object_marker seat_marker;
  unit_data_t *unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  char *unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  void *mode_tag = tag_get(TAG_GROUP_MODE, object_definition_model_index(unit_tag));
  char *antr_tag = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_tag));
  unit_data_t *target_unit =
    object_get_and_verify_type(target_unit_handle, _object_mask_unit);
  unit_definition_t *target_unit_tag =
    unit_definition_get(target_unit->object.tag_index);
  char *seat = tag_block_get_element(&target_unit_tag->seats,
                                     (int)seat_index, sizeof(unit_seat_t));
  int *mode_block = (int *)&animation_graph_definition_unit_seats(antr_tag);
  int16_t mode_index = 0;

  while ((int)mode_index < *mode_block) {
    animation_graph_unit_seat_t *seat_animations =
      tag_block_get_element(mode_block, (int)mode_index,
                            sizeof(animation_graph_unit_seat_t));
    animation_t *animation;

    if (crt_stricmp(seat_animations->label, seat + 4) == 0) {
      if (seat_animations->animations.count < 8) {
        return 0;
      }

      mode_index = ((int16_t *)seat_animations->animations.address)[7];
      if (mode_index == -1) {
        return 0;
      }

      animation = tag_block_get_element(
        &((animation_graph_t *)antr_tag)->animations, (int)mode_index,
        sizeof(animation_t));
      object_get_marker_by_name(target_unit_handle, seat + 0x24,
                                      &seat_marker, 1);
      animation_get_root_matrix(mode_tag, animation, 0, &mode_matrix);
      matrix4x3_multiply(&seat_marker.matrix.scale,
                         &mode_matrix.scale, &enter_position_matrix.scale);
      csstrcpy(marker_name, seat + 0x24);
      csstrcat(marker_name, " enter-hint");
      object_get_marker_by_name(target_unit_handle, marker_name,
                                      &hint_marker, 1);

      if (out_pos_b != NULL) {
        out_pos_b->x = seat_marker.matrix.position.x;
        out_pos_b->y = seat_marker.matrix.position.y;
        out_pos_b->z = seat_marker.matrix.position.z;
      }

      if (out_pos_a != NULL) {
        out_pos_a->x = enter_position_matrix.position.x;
        out_pos_a->y = enter_position_matrix.position.y;
        out_pos_a->z = enter_position_matrix.position.z;
      }

      if (out_pos_c != NULL) {
        out_pos_c->x = hint_marker.matrix.position.x;
        out_pos_c->y = hint_marker.matrix.position.y;
        out_pos_c->z = hint_marker.matrix.position.z;
      }

      return 1;
    }

    mode_index += 1;
  }

  return 0;
}

/* unit_get_melee_range_and_ticks (0x1a83e0)
 * Gets melee attack animation timing data. */
char unit_get_melee_range_and_ticks(int unit_handle, char is_secondary,
                                    int *out_tick_count, float *out_attack_time,
                                    int16_t *out_frame_count,
                                    float *out_damage_time)
{
  unit_data_t *unit;
  char *unit_tag;
  char *anim_tag;
  animation_graph_unit_seat_t *seat_animations;
  animation_graph_weapon_class_t *weapon_class;
  animation_t *animation;
  int anim_index;
  int16_t anim_id;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  tag_get(TAG_GROUP_MODE, object_definition_model_index(unit_tag));
  anim_tag = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_tag));
  seat_animations = tag_block_get_element(
    (int *)&animation_graph_definition_unit_seats(anim_tag), (int)(int8_t)unit->animation_seat_index,
    sizeof(animation_graph_unit_seat_t));
  weapon_class = tag_block_get_element(
    &seat_animations->weapon_classes,
    (int)(int8_t)unit->animation_weapon_index,
    sizeof(animation_graph_weapon_class_t));
  anim_index = (-(uint32_t)(is_secondary != '\0') & 3) + 0x27;
  if (anim_index < weapon_class->animations.count) {
    anim_id = ((int16_t *)weapon_class->animations.address)[anim_index];
  } else {
    anim_id = -1;
  }
  if (anim_id == -1) {
    return 0;
  }
  animation = tag_block_get_element(
    &((animation_graph_t *)anim_tag)->animations, (int)anim_id,
    sizeof(animation_t));
  animation_get_x_offsets((int)animation, (int)out_attack_time,
                          (int)out_damage_time);
  if (out_tick_count != NULL) {
    *out_tick_count = (int)animation_definition_key_frame_index(animation);
  }
  if (out_frame_count != NULL) {
    *out_frame_count = animation->frame_count;
  }
  return 1;
}

/* unit_get_animation_frames_remaining (0x1a84c0)
 * Returns the number of animation frames remaining.
 * Outputs the current animation state byte to *animation_state_out. */
int unit_get_animation_frames_remaining(int unit_handle,
                                        int16_t *animation_state_out)
{
  unit_data_t *unit;
  animation_graph_t *animation_graph;
  animation_t *animation;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  animation_graph = tag_get(TAG_GROUP_ANTR, unit->object.unk_124);
  animation = tag_block_get_element(&animation_graph->animations,
                                    (int)unit->object.unk_128,
                                    sizeof(animation_t));
  if (animation_state_out == NULL) {
    display_assert("animation_state", "c:\\halo\\SOURCE\\units\\units.c", 0x738,
                   1);
    system_exit(-1);
  }
  *animation_state_out = (int16_t)(int8_t)unit->animation_state;
  return (int)(uint16_t)animation->frame_count -
         (int)(uint16_t)unit->object.unk_130;
}

/* Per-axis acceleration plan; field names from the units.c asserts. */
typedef struct {
  boolean at_rest;      /* +0x00 */
  char pad_01[3];
  real initial_p;       /* +0x04 */
  real initial_v;       /* +0x08 */
  real accel_a;         /* +0x0c */
  real accel_t;         /* +0x10 */
  real coast_t;         /* +0x14 */
  real decel_a;         /* +0x18 */
  real decel_t;         /* +0x1c */
} unit_acceleration_plan_t;
cs(unit_acceleration_plan_t, 0x20);

/* unit_euler_axis_doplan (0x1a8550) — acceleration plan evaluator
 *
 * Evaluates a 3-phase acceleration plan (accelerate, coast, decelerate)
 * over a given time delta. Each phase consumes time from delta_time and
 * updates position and velocity accordingly:
 *   Phase 1 (acceleration): v += accel*t, pos += (0.5*accel*t + vel)*t
 *   Phase 2 (coast):        pos += vel*t
 *   Phase 3 (deceleration): v += accel*t, pos += (0.5*accel*t + vel)*t
 *
 * Returns 1 if all time was consumed (plan still active), 0 if plan
 * complete or flag was already set.
 *
 * Confirmed: thiscall — @ecx = plan struct pointer.
 * Confirmed: 5 stack params: delta_time, position, out_pos*, velocity,
 * out_vel*. Confirmed: 0x253398 = 0.5f, 0x2533c0 = 0.0f. Confirmed: plan+0x0c =
 * accel1, +0x10 = dur1, +0x14 = dur2. Confirmed: plan+0x18 = accel3, +0x1c =
 * dur3.
 */
char unit_euler_axis_doplan(void *plan, float delta_time, float position,
                  float *out_position, float velocity, float *out_velocity)
{
  const unit_acceleration_plan_t *axis_plan;
  char result;
  float current_position;
  float current_velocity;
  float time;
  float remaining_time;
  volatile float acceleration_delta;

  axis_plan = plan;
  result = axis_plan->at_rest;
  current_position = position;
  current_velocity = velocity;

  if (result)
    goto store_result;

  remaining_time = delta_time;
  if (!(remaining_time > 0.f))
    goto store_result;

  /* Phase 1: acceleration. Compare sense is remaining > 0 / remaining > dur
   * (not 0 < remaining) so VC71 emits FCOMP mem-const with TEST AH,0x41. */
  if (axis_plan->accel_t > 0.f) {
    if (remaining_time > axis_plan->accel_t)
      time = axis_plan->accel_t;
    else
      time = remaining_time;
    /* 0x1a85b2: retain the narrowed acceleration delta for both uses. */
    acceleration_delta = time * axis_plan->accel_a;
    current_position +=
      (acceleration_delta * 0.5f + current_velocity) * time;
    current_velocity += acceleration_delta;
    remaining_time -= time;
  }

  if (!(remaining_time > 0.f))
    goto store_result;

  /* Phase 2: coast */
  if (axis_plan->coast_t > 0.f) {
    if (remaining_time > axis_plan->coast_t)
      time = axis_plan->coast_t;
    else
      time = remaining_time;
    current_position += current_velocity * time;
    remaining_time -= time;
  }

  if (!(remaining_time > 0.f))
    goto store_result;

  /* Phase 3: deceleration. 0.5f is the left operand here (accel phase
   * multiplies time*accel*0.5f) -- matches 0x1a8550 FLD/FMUL order. */
  if (axis_plan->decel_t > 0.f) {
    if (remaining_time > axis_plan->decel_t)
      time = axis_plan->decel_t;
    else
      time = remaining_time;
    current_position +=
      (0.5f * time * axis_plan->decel_a + current_velocity) * time;
    current_velocity += time * axis_plan->decel_a;
    remaining_time -= time;
  }

  if (remaining_time > 0.f)
    result = 1;

store_result:
  *out_position = current_position;
  *out_velocity = current_velocity;
  return result;
}

/* unit_get_zoom_level (0x1a8690) */
int16_t unit_get_zoom_level(int unit_handle)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  return (int16_t)(int8_t)unit->current_zoom_level;
}

/* unit_data_t's animation block at +0x248, passed as &unit->animation_flags.
 * Size unproven. */
typedef struct unit_animation_data_t {
  byte pad_00[0x0b];
  int8_t state;  /* unit_data_t.animation_state */
  int8_t action; /* unit_data_t.animation_action */
  byte pad_0d[0x1a - 0x0d];
  int16_t soft_ping_animation_index;
} unit_animation_data_t;
co(unit_animation_data_t, state, 0x0b);
co(unit_animation_data_t, action, 0x0c);
co(unit_animation_data_t, soft_ping_animation_index, 0x1a);

/* unit_animation_state_interruptable (0x1a86b0)
 * Animation state transition check. @ecx = anim state ptr, @edx = target state.
 */
char unit_animation_state_interruptable(void *anim_state, int16_t target_state)
{
  unit_animation_data_t *animation = anim_state;
  char result;

  result = 1;
  switch (animation->state) {
  case 0x1d:
  case 0x1e:
  case 0x1f:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x27:
  case 0x29:
    if (target_state == 0x17) {
      break;
    }
    return 0;
  case 0x18:
  case 0x19: {
    int target;

    target = target_state;
    if (target < 0x18 || target > 0x19) {
      goto not_allowed;
    }
    return 1;
  }
  case 0x2:
  case 0x3:
  case 0x25:
  case 0x26:
    if (target_state != 0) {
      break;
    }
    /* FALLTHROUGH */
  case 0x17:
  case 0x1a:
  case 0x1b:
  case 0x1c:
  not_allowed:
    result = 0;
    break;
  }
  return result;
}

/* unit_animation_busy (0x1a8730)
 * Returns 1 for vehicle-related animation states. @ecx = anim state ptr. */
char unit_animation_busy(void *anim_state)
{
  unit_animation_data_t *animation = anim_state;
  char result;

  result = 0;
  switch (animation->state) {
  case 0x17:
  case 0x18:
  case 0x19:
  case 0x1a:
  case 0x1b:
  case 0x1d:
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x27:
  case 0x29:
    result = 1;
  }
  return result;
}

/* unit_animation_overlay_action_loops (0x1a8770)
 * Returns 1 when the animation state is in the inclusive range [3,4]. */
char unit_animation_overlay_action_loops(void *anim_state)
{
  unit_animation_data_t *animation = anim_state;
  char result;

  result = 0;
  if ((animation->state >= 3) &&
      (animation->state <= 4)) {
    result = 1;
  }
  return result;
}

/* unit_animation_state_loops (0x1a8790)
 * Returns 0 for vehicle/combat animation states. @ecx = anim state ptr. */
char unit_animation_state_loops(void *anim_state)
{
  unit_animation_data_t *animation = anim_state;
  char result;

  result = 1;
  switch (animation->state) {
  case 1:
  case 2:
  case 3:
  case 0x17:
  case 0x1a:
  case 0x1b:
  case 0x1c:
  case 0x1d:
  case 0x1e:
  case 0x1f:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x27:
  case 0x29:
    result = 0;
  }
  return result;
}

/* unit_animation_weapon_ik (0x1a87f0)
 * Boolean: idle and free, unless overridden by certain anim states.
 * @ecx = anim state ptr. Switch table verified from binary. */
char unit_animation_weapon_ik(void *anim_state)
{
  unit_animation_data_t *animation = anim_state;
  char result;

  result = animation->soft_ping_animation_index == -1;
  if (animation->action != '\0') {
    result = 0;
  }
  switch (animation->state) {
  case 0x14:
  case 0x15:
  case 0x16:
  case 0x24:
  case 0x25:
  case 0x26:
    result = 0;
  }
  return result;
}

/* unit_animation_vehicle_ik (0x1a8850)
 * Returns 0 for specific vehicle/boarding animation states.
 * @ecx = anim state ptr. */
char unit_animation_vehicle_ik(void *anim_state)
{
  unit_animation_data_t *animation = anim_state;
  char result;

  result = 1;
  switch (animation->state) {
  case 0x17:
  case 0x18:
  case 0x19:
  case 0x1a:
  case 0x1b:
  case 0x1d:
  case 0x22:
  case 0x23:
    result = 0;
  }
  return result;
}

/* unit_animation_aiming_screen (0x1a8890)
 * True when no action is playing, except in the signed animation-state band
 * [0x17,0x23] and state 0x29. */
char unit_animation_aiming_screen(void *anim_state)
{
  unit_animation_data_t *animation = anim_state;
  char result;
  int8_t state;

  result = (char)(animation->action == 0);
  state = animation->state;
  if (state >= 0x17 && (state <= 0x23 || state == 0x29)) {
    result = 0;
  }
  return result;
}

/* unit_animation_state_get_aiming_screen_index (0x1a88b0)
 * Maps animation state to animation index. @ecx = anim state value. */
int unit_animation_state_get_aiming_screen_index(int16_t anim_state)
{
  int result;

  result = -1;
  switch ((int)anim_state) {
  case 0x0:
  case 0x2:
  case 0x3:
  case 0x10:
  case 0x11:
  case 0x12:
  case 0x13:
  case 0x14:
  case 0x15:
  case 0x16:
  case 0x25:
  case 0x26:
    result = 0x19;
    break;
  case 0x4:
  case 0x5:
  case 0x6:
  case 0x7:
  case 0x8:
  case 0x9:
  case 0xa:
  case 0xb:
  case 0xc:
  case 0xd:
  case 0xe:
  case 0xf:
    result = 0x1a;
    break;
  }
  return result;
}

/* unit_animation_state_can_be_entered_without_animation (0x1a8910)
 *
 * Animation-state predicate. @ecx = anim state value (signed 16-bit; the
 * prologue's MOVSX ECX,CX proves the sign extension).
 *
 * Returns 0 for the six states 0x1e, 0x1f, 0x20, 0x21, 0x27 and 0x29;
 * returns 1 for every other state (including the in-range 0x22-0x26 and
 * 0x28, which fall through to the default arm of the jump table).
 *
 * The original biases by -0x1e and bounds-checks against 0xb, then indexes a
 * 12-byte selector table at 0x1a8938 feeding the 2-entry dword jump table at
 * 0x1a8930. Keep this as a switch: an if-chain or lookup array will not
 * regenerate the byte/jump table pair. */
char unit_animation_state_can_be_entered_without_animation(int16_t anim_state)
{
  char result;

  result = 1;
  switch ((int)anim_state) {
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x27:
  case 0x29:
    result = 0;
  }
  return result;
}

/* unit_animation_compute_interpolation_frame_count (0x1a8950)
 *
 * Classifies an (animation state, target state) pair into one of three
 * categories.  Pure register-only comparison ladder: no memory access, no
 * calls, no FPU.
 *
 * Returns 1 when BOTH states are in {0, 2, 3}, 2 when the animation state is
 * 0x15 or 0x16, and 6 otherwise.  The 0x15/0x16 test is deliberately a
 * SEPARATE sequential `if` (not an `else if`) — in the original the second
 * comparison block at 0x1a897c is reached on every path, including the one
 * that already stored 1, so an anim_state of 0x15/0x16 always yields 2
 * regardless of target_state.  Confirmed against disassembly.
 *
 * Both parameters are compared with 16-bit CMP CX/DX,imm16 — keep them
 * int16_t (do not widen to int).  Result is returned in full 32-bit EAX.
 */
int unit_animation_compute_interpolation_frame_count(int16_t anim_state, int16_t target_state)
{
  int result;

  result = 6;
  if (((anim_state == 0) || (anim_state == 2) || (anim_state == 3)) &&
      ((target_state == 0) || (target_state == 2) || (target_state == 3))) {
    result = 1;
  }
  if ((anim_state == 0x16) || (anim_state == 0x15)) {
    result = 2;
  }
  return result;
}

/* unit_animation_start_action (0x1a8990)
 *
 * Sets the unit's seated animation state by looking up an animation index from
 * the unit's animation tag hierarchy and calling model_animation_choose_random.
 *
 * When state == 0: clears unk_596 (animation state byte at 0x254) and sets
 * unk_602 (0x25a) to 0xffff (NONE), then returns.
 *
 * For state 1-9: walks the unit tag -> antr tag -> mode element -> sub-element
 * -> weapon-element hierarchy using unk_592/593/594 as indices. Then:
 *
 *   States 1-4, 8: index the animation kind table in the sub-element block
 *     (sub_element+0x98/0x9c). Kind indices: 0x15, 0x16, 0x17, 0x18, 0x14.
 *   States 5-7, 9: index the animation kind table in the weapon-element block
 *     (weapon_element+0x30/0x34). Sub-indices: 0, 1, 8, 9.
 *
 * If a valid animation index is found (DI != -1):
 *   - Calls object_set_region_count(object_handle, 6) unless state == 7.
 *   - Calls model_animation_choose_random(1, antr_tag_index, animation_index).
 *   - Stores result in unk_602 (0x25a), clears unk_604 (0x25c), sets
 *     unk_596 (0x254) to (uint8_t)state.
 *
 * Confirmed: switch jump table at 0x1a8af8 (9 entries for ECX=0-8).
 * Confirmed: MOVSX ECX,byte ptr [ESI+0x250/0x251/0x252] for tag block indices.
 * Confirmed: MOV byte ptr [ESI+0x254],CL; MOV word ptr [ESI+0x25a],AX.
 */
void unit_animation_start_action(int object_handle, int16_t state)
{
  unit_data_t *unit;
  char *unit_tag;
  char *antr_tag;
  animation_graph_unit_seat_t *seat_animations;
  animation_graph_weapon_class_t *weapon_class;
  animation_graph_weapon_type_t *weapon_type;
  int16_t animation_index;
  int16_t interpolation_frame_count;
  int anim_kind_idx;
  int anim_sub_idx;

  unit = object_get_and_verify_type(object_handle, _object_mask_unit);

  if (state == 0) {
    unit->animation_action = 0;
    unit->action_animation_index = (uint16_t)0xffff;
    return;
  }

  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  antr_tag = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_tag));
  seat_animations = tag_block_get_element(
    &animation_graph_definition_unit_seats(antr_tag), (int)(int8_t)unit->animation_seat_index,
    sizeof(animation_graph_unit_seat_t));
  weapon_class = tag_block_get_element(
    &seat_animations->weapon_classes,
    (int)(int8_t)unit->animation_weapon_index,
    sizeof(animation_graph_weapon_class_t));
  weapon_type = tag_block_get_element(
    &weapon_class->weapon_types,
    (int)(int8_t)unit->animation_weapon_type_index,
    sizeof(animation_graph_weapon_type_t));

  animation_index = (int16_t)-1;

  switch (state) {
  case 1:
    anim_kind_idx = 0x15;
    goto lookup_sub;
  case 2:
    anim_kind_idx = 0x16;
    goto lookup_sub;
  case 3:
    anim_kind_idx = 0x17;
    goto lookup_sub;
  case 4:
    anim_kind_idx = 0x18;
    goto lookup_sub;
  case 8:
    anim_kind_idx = 0x14;
    goto lookup_sub;
  lookup_sub:
    if (anim_kind_idx < weapon_class->animations.count)
      animation_index =
        ((int16_t *)weapon_class->animations.address)[anim_kind_idx];
    break;

  case 5:
    anim_sub_idx = 0;
    goto lookup_weapon;
  case 6:
    anim_sub_idx = 1;
    goto lookup_weapon;
  case 7:
    anim_sub_idx = 8;
    goto lookup_weapon;
  case 9:
    anim_sub_idx = 9;
    goto lookup_weapon;
  lookup_weapon:
    if (anim_sub_idx < weapon_type->animations.count)
      animation_index =
        ((int16_t *)weapon_type->animations.address)[anim_sub_idx];
    break;
  }

  /* interpolation_frame_count = action==7 ? 0 : 6; 2276 emits SETE/DEC/AND 6. */
  interpolation_frame_count = (int16_t)((state == 7) ? 0 : 6);

  if (animation_index != (int16_t)-1) {
    if (interpolation_frame_count > 0)
      object_set_region_count(object_handle, interpolation_frame_count);
    unit->action_animation_index = (int16_t)model_animation_choose_random(
      1, object_definition_animation_graph_index(unit_tag), animation_index);
    unit->action_animation_frame = 0;
    unit->animation_action = (uint8_t)state;
  }
}

/* unit_animation_start_overlay_action (0x1a8b20)
 *
 * Attempts to set the current weapon animation state on a unit by looking up
 * the appropriate animation index from the unit's animation tag data and
 * calling model_animation_choose_random to select the sequence.
 *
 * Resolves the animation graph via the unit tag (group 'unit') and its nested
 * animation data (group 'antr'). Navigates through the weapon's animation mode
 * and weapon-type blocks using per-unit indices (unk_592, unk_593, unk_594).
 *
 * The incoming state code is remapped to an animation table index:
 *   1 -> 4 (fire-1), 2 -> 5 (fire-2), 3 -> 6 (charged-1), 4 -> 7 (charged-2),
 *   5 -> 2 (chamber-1), 6 -> 3 (chamber-2)
 *
 * Early-outs:
 *   - state < unit->overlay_action (already at or past this state)
 *   - unk_595 is in the set of "active" animation states (0x17–0x23, 0x27,
 * 0x29)
 *   - table index out of range or entry == -1 (animation not defined)
 *
 * On success, writes the chosen random animation index into unk_606 (0x25e),
 * clears unk_608 (0x260), and sets unk_597 (0x255) = state.
 *
 * On failure, if developer mode is enabled (DAT_005054fb != 0) and this is a
 * biped (object.type == 0) with no seat (unk_586 == -1), prints a warning:
 *   MISSING: <tag_path> '<state_name> <mode_name>'
 */
void unit_animation_start_overlay_action(int object_handle, int16_t state)
{
  unit_data_t *unit;
  int16_t current_state;
  char *unit_tag;
  char *antr_tag;
  animation_graph_unit_seat_t *seat_animations;
  animation_graph_weapon_class_t *weapon_class;
  animation_graph_weapon_type_t *weapon_type;
  int anim_table_index;
  int16_t anim_index;
  int16_t chosen;

  unit = object_get_and_verify_type(object_handle, _object_mask_unit);

  /* bail if already at or past this state */
  current_state = (int16_t)(int8_t)unit->overlay_action;
  if (state < current_state) {
    return;
  }

  /* bail for specific animation states that are already active
   * (original sign-extends the state byte: movsbl 0x253(%esi)) */
  switch ((int8_t)unit->animation_state) {
  case 0x17:
  case 0x18:
  case 0x19:
  case 0x1a:
  case 0x1b:
  case 0x1d:
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x27:
  case 0x29:
    return;
  default:
    break;
  }

  /* navigate to the animation destination block for this unit's weapon type */
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  antr_tag = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_tag));
  seat_animations = tag_block_get_element(
    &animation_graph_definition_unit_seats(antr_tag), (int)(int8_t)unit->animation_seat_index,
    sizeof(animation_graph_unit_seat_t));
  weapon_class = tag_block_get_element(
    &seat_animations->weapon_classes,
    (int)(int8_t)unit->animation_weapon_index,
    sizeof(animation_graph_weapon_class_t));
  weapon_type = tag_block_get_element(
    &weapon_class->weapon_types,
    (int)(int8_t)unit->animation_weapon_type_index,
    sizeof(animation_graph_weapon_type_t));

  /* remap state code to animation table index */
  anim_table_index = -1;
  switch (state) {
  case 1:
    anim_table_index = 4;
    break;
  case 2:
    anim_table_index = 5;
    break;
  case 3:
    anim_table_index = 6;
    break;
  case 4:
    anim_table_index = 7;
    break;
  case 5:
    anim_table_index = 2;
    break;
  case 6:
    anim_table_index = 3;
    break;
  default:
    goto missing;
  }

  if (anim_table_index < weapon_type->animations.count) {
    anim_index =
      ((int16_t *)weapon_type->animations.address)[anim_table_index];
    if (anim_index != -1) {
      chosen = (int16_t)model_animation_choose_random(
        1, object_definition_animation_graph_index(unit_tag), anim_index);
      unit->overlay_action_animation_index = chosen;
      unit->overlay_action_animation_frame = 0;
      unit->overlay_action = (uint8_t)state;
      return;
    }
  }

missing:
  /* developer-mode warning: animation not defined */
  if (DEBUG_UNIT_ANIMATIONS != 0 && unit->object.type == _object_type_biped &&
      unit->aiming_screen_index == -1) {
    const char *state_name = animation_list_get_string(WEAPON_TYPE_ANIMATION_NAMES, anim_table_index);
    const char *tag_path =
      tag_name_strip_path(object_definition_animation_graph_name(unit_tag));
    console_warning("MISSING: %s '%s %s'", tag_path, state_name,
                    weapon_class->label);
  }
}

/* unit_find_nearby_seat (0x1a8ce0)
 *
 * Searches the child object chain of target_unit for a unit that occupies the
 * given seat_index, or that belongs to a friendly team (via game_allegiance).
 *
 * Walks the linked list starting at target_unit's first_child_object (offset
 * 0xC8). For each child that is a biped or vehicle (type 0 or 1):
 *   - If the child unit's seat tag index (offset 0x2A0) matches seat_index,
 *     records the child's handle in *out_unit_handle.
 *   - Otherwise, if the searching unit has an active rider/owner (offset 0x1C8
 *     != NONE) and the child's team is friendly with the searching unit's team,
 *     clears the "empty" flag but does not update the output handle.
 *
 * Returns true only if unit_handle != target_unit_handle AND no matching/
 * friendly child was found. If out_unit_handle is non-NULL, writes the handle
 * of the last seat-matching child found (or NONE if none matched).
 */
bool unit_find_nearby_seat(int unit_handle, int target_unit_handle,
                           int16_t seat_index, int *out_unit_handle)
{
  unit_data_t *unit_data;
  object_data_t *target_obj;
  object_data_t *child_obj;
  unit_data_t *child_unit;
  int child_handle;
  int result_handle;
  bool not_found;

  unit_data = object_get_and_verify_type(unit_handle, _object_mask_unit);
  target_obj = object_get_and_verify_type(target_unit_handle, _object_mask_unit);

  not_found = (unit_handle != target_unit_handle);
  result_handle = -1;

  child_handle = target_obj->first_child_object_index.value;
  while (child_handle != -1) {
    child_obj = object_get_and_verify_type(child_handle, _object_mask_all);

    if ((1 << (*(uint8_t *)&child_obj->type & 0x1f)) & _object_mask_unit) {
      child_unit = object_get_and_verify_type(child_handle, _object_mask_unit);

      if (child_unit->parent_seat_index == seat_index) {
        /* child occupies the requested seat */
        result_handle = child_handle;
        not_found = false;
      } else if (unit_data->player_index.value != -1 &&
                 game_allegiance_get_team_is_friendly(
                   (int16_t)unit_data->object.unk_104,
                   (uint16_t)child_obj->unk_104)) {
        /* child is on a friendly team; seat is occupied/blocked */
        not_found = false;
      }
    }

    child_handle = child_obj->next_object_index.value;
  }

  if (out_unit_handle != NULL) {
    *out_unit_handle = result_handle;
  }
  return not_found;
}

/* unit_impulse (0x1a8da0) */
void unit_impulse(int unit_index, int unused, float *impulse_vector,
                  float scale)
{
  unit_data_t *unit;
  char *unit_tag;
  struct physics_definition *physics;
  (void)unused;

  unit = object_get_and_verify_type(unit_index, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  if (object_definition_physics_index(unit_tag) != -1) {
    physics = tag_get(TAG_GROUP_PHYS, object_definition_physics_index(unit_tag));
    scale = scale / physics->mass;
    unit->object.translational_velocity.i =
      scale * impulse_vector[0] + unit->object.translational_velocity.i;
    unit->object.translational_velocity.j =
      scale * impulse_vector[1] + unit->object.translational_velocity.j;
    unit->object.translational_velocity.k =
      scale * impulse_vector[2] + unit->object.translational_velocity.k;
  }
}

/* unit_handle_weapon_state_change (0x1a8e10)
 *
 * Dispatches a unit animation state transition based on an incoming state code.
 * Maps input state values 1-8 to either unit_animation_start_overlay_action (which sets a unit
 * animation transition state with a remapped index) or
 * unit_animation_start_action (which initiates a seat-based animation
 * sequence). The state remapping is: state 1 -> unit_animation_start_overlay_action with index 1 state
 * 2 -> unit_animation_start_overlay_action with index 2 state 3 -> unit_animation_start_overlay_action with index 5 state 4
 * -> unit_animation_start_overlay_action with index 6 state 5 -> unit_animation_start_action with
 * index 5 state 6 -> unit_animation_start_action with index 6 state 7 ->
 * unit_animation_start_overlay_action with index 3 state 8 -> unit_animation_start_overlay_action with index 4
 */
void unit_handle_weapon_state_change(int object_handle, int16_t state)
{
  switch (state) {
  case 1:
    unit_animation_start_overlay_action(object_handle, 1);
    return;
  case 2:
    unit_animation_start_overlay_action(object_handle, 2);
    return;
  case 3:
    unit_animation_start_overlay_action(object_handle, 5);
    return;
  case 4:
    unit_animation_start_overlay_action(object_handle, 6);
    return;
  case 5:
    unit_animation_start_action(object_handle, 5);
    return;
  case 6:
    unit_animation_start_action(object_handle, 6);
    return;
  case 7:
    unit_animation_start_overlay_action(object_handle, 3);
    return;
  case 8:
    unit_animation_start_overlay_action(object_handle, 4);
    return;
  }
}

/* unit_record_damage (0x1a8ee0)
 * Accumulates damage into one of four damage records (merging with a record
 * for the same attacker, else taking an empty or the weakest record), then
 * optionally reports killing sprees to the AI. */
void unit_record_damage(int unit_handle, float damage_amount,
                        int16_t damage_type, char notify_ai,
                        int attacker_object, int16_t attacker_team,
                        int killing_object)
{
  unit_data_t *unit;
  char found_existing;
  int timestamp;
  float *slot_damage;
  int i;
  int16_t empty_slot;
  int16_t best_damage_slot;
  int16_t best_oldest_slot;
  int16_t s;
  unit_data_t *attacker_unit;
  int attacker_unit_handle;
  int seated_handle;
  int killing_spree_count;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  found_existing = 0;
  timestamp = game_time_get();

  /* Scan existing 4 slots for matching attacker or killing object */
  slot_damage = (float *)&unit->unk_992[4];
  i = 4;
  do {
    if ((attacker_object != -1 &&
         *(int *)(((char *)slot_damage) + 8) == attacker_object) ||
        *(int *)(((char *)slot_damage) + 4) == killing_object) {
      *(int *)(((char *)slot_damage) - 4) = timestamp;
      found_existing = 1;
      *slot_damage = damage_amount + *slot_damage;
    }
    slot_damage = (float *)((char *)slot_damage + 0x10);
    i = i - 1;
  } while (i != 0);

  if (!found_existing) {
    /* Find an empty slot (timestamp == -1) */
    empty_slot = 0;
    do {
      if (*(int *)&unit->unk_992[(int)empty_slot * 0x10] == -1) {
        goto write_slot;
      }
      empty_slot = empty_slot + 1;
    } while (empty_slot < 4);

    /* No empty slot — find the slot with the smallest damage (to evict) */
    best_damage_slot = 0;
    s = 1;
    do {
      if (*(float *)&unit->unk_992[(int)best_damage_slot * 0x10 + 4] <
          *(float *)&unit->unk_992[(int)s * 0x10 + 4]) {
        best_damage_slot = s;
      }
      s = s + 1;
    } while (s < 4);

    /* Among remaining slots (excluding best_damage_slot), find the oldest */
    best_oldest_slot = -1;
    s = 0;
    do {
      if (s != best_damage_slot) {
        if (best_oldest_slot == -1 ||
            *(uint32_t *)&unit->unk_992[(int)s * 0x10] <
              *(uint32_t *)&unit->unk_992[(int)best_oldest_slot * 0x10]) {
          best_oldest_slot = s;
        }
      }
      s = s + 1;
    } while (s < 4);

    if (best_oldest_slot == -1) {
      display_assert("best_new_attacker_index!=NONE",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x136c, 1);
      system_exit(-1);
    }

    empty_slot = best_oldest_slot;

  write_slot: {
    int slot_base;
    slot_base = (int)empty_slot * 0x10;
    *(int *)&unit->unk_992[slot_base + 0xc] = attacker_object;
    *(int *)&unit->unk_992[slot_base + 0x8] = killing_object;
    *(float *)&unit->unk_992[slot_base + 0x4] = damage_amount;
    *(int *)&unit->unk_992[(int)empty_slot * 0x10] = timestamp;
  }
  }

  /* AI notification section */
  if (notify_ai == '\0') {
    return;
  }
  if ((int16_t)attacker_team == -1) {
    return;
  }
  if (!game_allegiance_get_team_is_friendly(unit->object.owner_team_index,
                                            attacker_team)) {
    return;
  }

  /* Resolve the actual attacking unit */
  attacker_unit = NULL;
  attacker_unit_handle = killing_object;
  if (attacker_object != -1) {
    {
      player_data_t *player;
      player = datum_get(PLAYER_DATA_VOID, attacker_object);
      seated_handle = player->unit_handle;
      if (seated_handle != -1) {
        attacker_unit = object_get_and_verify_type(seated_handle, _object_mask_unit);
        attacker_unit_handle = seated_handle;
        if (attacker_unit != NULL) {
          goto have_attacker;
        }
      }
    }
  }
  attacker_unit = object_try_and_get_and_verify_type(killing_object, _object_mask_unit);
  attacker_unit_handle = killing_object;
  if (attacker_unit == NULL) {
    return;
  }

have_attacker:
  /* Follow seat hierarchy (driver/gunner) to the controlling unit */
  if (damage_type == 9) {
    seated_handle = attacker_unit->driver_object_index.value;
  } else {
    seated_handle = attacker_unit->gunner_object_index.value;
  }
  if (seated_handle != -1) {
    attacker_unit = object_get_and_verify_type(seated_handle, _object_mask_unit);
    attacker_unit_handle = seated_handle;
  }

  /* Skip AI-controlled units */
  if ((attacker_unit->object.damage_flags & 4) != 0) {
    return;
  }

  /* Update killing spree counter */
  {
    int last_damage_time;
    last_damage_time = attacker_unit->killing_spree_last_time;
    if (last_damage_time == -1 || last_damage_time + 0x78 < timestamp) {
      attacker_unit->killing_spree_count = 0;
    }
    attacker_unit->killing_spree_count = attacker_unit->killing_spree_count + 1;
    killing_spree_count = (int)attacker_unit->killing_spree_count;
    attacker_unit->killing_spree_last_time = timestamp;

    if (ai_handle_killing_spree(attacker_unit_handle,
                                (short)killing_spree_count) != '\0') {
      attacker_unit->killing_spree_count = 0;
    }
  }
}

/* unit_test_spawning (0x1a9130)
 * Tests if unit should spawn attached objects on death. */
uint32_t unit_test_spawning(int unit_handle)
{
  unit_data_t *unit;
  char *unit_tag;
  uint32_t flags;
  unsigned int *seed;
  int16_t count;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  flags = unit->flags;
  if ((flags & 0x20000) == 0) {
    unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
    if (unit_definition_spawned_actor_variant_index(unit_tag) != -1) {
      seed = get_global_random_seed_address();
      count = seed_random_range((unsigned int *)seed, unit_definition_spawn_actor_lower_bound(unit_tag),
                           (int16_t)(unit_definition_spawn_actor_upper_bound(unit_tag) + 1));
      if (count > 0) {
        actors_spawn_from_unit(unit_handle, unit_definition_spawned_actor_variant_index(unit_tag), count,
                     unit_definition_spawn_throw_velocity(unit_tag) * REAL_ONE_THIRTIETH_POOL);
      }
      unit->flags = unit->flags | 0x20000;
      return flags;
    }
  }
  return flags & 0xffff0000;
}

/* unit_destroy (0x1a91e0)
 * Destroys a unit: runs object_destroy then unit_test_spawning. */
void unit_destroy(int unit_handle)
{
  object_destroy(unit_handle);
  unit_test_spawning(unit_handle);
}

/* 0x1a9200 — get world-space position of the "head" marker on a unit.
 * Thin wrapper: calls object_get_marker_by_name for the string at
 * 0x2909e4 ("head"), then extracts XYZ from offset 0x60 in the marker
 * output record. Identical pattern to unit_get_center_of_mass ("body" marker). */
void unit_get_head_position(int object_handle, float *out_position)
{
  object_marker marker;

  object_get_marker_by_name(object_handle, MARKER_NAME_HEAD, &marker, 1);
  *(real_point3d *)out_position = marker.matrix.position;
}

/* unit_get_camera_position (0x1a9240)
 *
 * Computes a 3D position representing the unit's current seat state and writes
 * it into the caller-supplied float[3].
 *
 * Three major paths:
 *
 * 1. Unit has a parent (parent_object_index != -1):
 *    Copies the parent object's world position (offset 0x0C). If the parent's
 *    object type is 0 or 1 (biped/vehicle) and the unit has a valid seat
 *    definition index (parent_seat_index != -1), looks up the seat's marker name (at
 *    seat_def + 0x84) and resolves it on the parent via
 *    object_get_marker_by_name. For seat type 1, skips if the marker
 *    name byte at seat_def + 0x84 is zero.
 *
 * 2. Unit has no parent and no special flags:
 *    If unit flags byte (0xB6) bit 2 is clear AND object type is 0 (biped),
 *    delegates to biped_estimate_position with zeroed optional parameters.
 *
 * 3. Unit has no parent but has flags/non-zero type:
 *    If unk_728 is NONE, gets the "head" marker on the unit itself. Otherwise,
 *    resolves unk_728 as a unit, reads its seat index (parent_seat_index), looks up the
 *    seat definition's marker name (seat_def + 0x24) from the original unit's
 *    tag, and resolves it on the original unit via
 * object_get_marker_by_name.
 */
void unit_get_camera_position(int unit_handle, real_point3d *position)
{
  unit_data_t *unit;
  unit_definition_t *unit_tag;
  int parent_handle;
  object_data_t *parent;
  unit_data_t *gunner;
  char *seat_def;
  uint8_t parent_type;
  uint32_t type_mask;
  object_marker marker;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = unit_definition_get(unit->object.tag_index);
  parent_handle = unit->object.parent_object_index.value;

  if (parent_handle == NONE) {
    if (!(unit->object.damage_flags & 0x04) &&
        unit->object.type == _object_type_biped) {
      biped_estimate_position(unit_handle, 0, (vector3_t *)0,
                              (vector3_t *)0, /* dup-args-ok */
                              (vector3_t *)0, (vector3_t *)position);
      return;
    }

    if (unit->gunner_object_index.value == NONE) {
      object_get_marker_by_name(unit_handle, MARKER_NAME_HEAD, &marker, 1);
      memcpy(position, &marker.matrix.position, sizeof(real_point3d));
      return;
    }

    gunner = object_get_and_verify_type(unit->gunner_object_index.value,
                                        _object_mask_unit);
    seat_def = tag_block_get_element(&unit_tag->seats,
                                     (int)gunner->parent_seat_index,
                                     sizeof(unit_seat_t));
    object_get_marker_by_name(unit_handle, unit_seat_definition_marker_name(seat_def), &marker, 1);
    memcpy(position, &marker.matrix.position, sizeof(real_point3d));
    return;
  }

  parent = object_get_and_verify_type(parent_handle, _object_mask_all);
  memcpy(position, &parent->position, sizeof(real_point3d));

  parent_type = *(uint8_t *)&parent->type;
  type_mask = 1 << parent_type;
  if (!(type_mask & _object_mask_unit))
    return;

  if (unit->parent_seat_index == -1)
    return;

  unit_tag = unit_definition_get(parent->tag_index);
  seat_def = tag_block_get_element(&unit_tag->seats,
                                   (int)unit->parent_seat_index,
                                   sizeof(unit_seat_t));

  if (parent->type == _object_type_vehicle) {
    if (*(uint8_t *)unit_seat_definition_field_84(seat_def) == 0)
      return;
  }

  object_get_marker_by_name(unit->object.parent_object_index.value,
                            unit_seat_definition_field_84(seat_def), &marker, 1);
  position->x = marker.matrix.position.x;
  position->y = marker.matrix.position.y;
  position->z = marker.matrix.position.z;
}

/* unit_estimate_position (0x1a93e0)
 *
 * Estimates the world-space position for a unit given a desired estimation mode
 * and optional predicted body position. Handles three major paths:
 *
 * 1. Unit has no parent AND flag byte (unit+0xb6) bit 2 is clear AND type==0
 *    (biped): delegate entirely to biped_estimate_position and return.
 *
 * 2. Unit type==0 (biped) AND has a parent AND parent type==1 (vehicle): call
 *    vehicle_get_estimated_position to get the vehicle's predicted position
 * into a local vector, and use that as the local_body_pos for the delta. If
 * vehicle_get_estimated_position returns -1 (failed), fall through to path 3.
 *
 * 3. Fallback: call object_get_world_position to get unit's world position as
 *    local_body_pos.
 *
 * After path 2/3: call unit_get_camera_position to compute the unit's current
 * estimated position into out_position, then add the delta
 * (body_position - local_body_pos) to out_position.
 *
 * Confirmed: assert strings "body_position && estimated_position" and
 * "(estimate_mode >= 0) && (estimate_mode <
 * NUMBER_OF_UNIT_ESTIMATE_POSITION_MODES)" reference
 * "c:\halo\SOURCE\units\units.c", lines 0x14b1 and 0x14b2. Confirmed: CMP
 * EAX,-0x1 / JNZ pattern for parent_object_index checks. Confirmed: TEST byte
 * ptr [EDI+0xb6],0x4 for flags check. Confirmed: CMP word ptr [EDI+0x64],0x0
 * and CMP word ptr [EAX+0x64],0x1 for type checks. Confirmed: FPU tail sequence
 * — FXCH after three FLD/FSUB pairs to reorder x/y.
 */
void unit_estimate_position(int unit_handle, int16_t estimate_mode,
                            vector3_t *body_position, vector3_t *desired_facing,
                            vector3_t *desired_gun_offset,
                            vector3_t *out_position)
{
  unit_data_t *unit;
  object_data_t *parent_obj;
  int parent_handle;
  int result;
  vector3_t local_body_pos;
  volatile float delta_z;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if (body_position == NULL || out_position == NULL) {
    display_assert("body_position && estimated_position",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x14b1, true);
    system_exit(-1);
  }
  if (estimate_mode < 0 || estimate_mode >= 4) {
    display_assert("(estimate_mode >= 0) && (estimate_mode < "
                   "NUMBER_OF_UNIT_ESTIMATE_POSITION_MODES)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x14b2, true);
    system_exit(-1);
  }

  parent_handle = unit->object.parent_object_index.value;

  if (parent_handle == -1 && !(unit->object.damage_flags & 0x4)) {
    if (unit->object.type == _object_type_biped) {
      biped_estimate_position(unit_handle, estimate_mode, body_position,
                              desired_facing, desired_gun_offset, out_position);
      return;
    }
  } else if (unit->object.type == _object_type_biped && parent_handle != -1) {
    parent_obj = object_get_and_verify_type(parent_handle, _object_mask_all);
    if (parent_obj->type == _object_type_vehicle) {
      result = vehicle_get_estimated_position(
        unit->object.parent_object_index.value, &local_body_pos);
      if (result != -1)
        goto apply_delta;
    }
  }

  /* Fallback: use world position as local_body_pos reference */
  object_get_world_position(unit_handle, &local_body_pos);

apply_delta:
  /* Get the unit's current estimated seat position into out_position */
  unit_get_camera_position(unit_handle, out_position);

  /* Add (body_position - local_body_pos) delta to out_position */
  out_position->x += body_position->x - local_body_pos.x;
  out_position->y += body_position->y - local_body_pos.y;
  /* 0x1a94f6: the Z delta is narrowed before it is added. */
  delta_z = body_position->z - local_body_pos.z;
  out_position->z += delta_z;
}

/* unit_impulse_to_animation_kind (0x1a9560)
 *
 * Maps an animation impulse index (0-13) to an animation kind index and an
 * update_kind value. The impulse index selects an animation kind (e.g. 0x1d
 * for "impulse", 0x20 for "melee_attack_long_step", etc.) and writes 3 or 6
 * to *out_update_kind depending on whether the impulse uses a ranged or melee
 * update mode.
 *
 * If impulse_index is out of [0, 13] the function asserts and terminates.
 * If out_update_kind is NULL the write is skipped (TEST EBX,EBX gate).
 *
 * Impulse-to-kind mapping (jump table at 0x1a969c):
 *   0  -> 0x1d  (update 6)   8  -> 0x04  (update 3)
 *   1  -> 0x20  (update 6)   9  -> 0x05  (update 3)
 *   2  -> 0x21  (update 6)   10 -> 0x06  (update 3)
 *   3  -> 0x22  (update 6)   11 -> 0x07  (update 3)
 *   4  -> 0x1b  (update 3)   12 -> 0x28  (update 6)
 *   5  -> 0x1c  (update 3)   13 -> 0x29  (update 6)
 *   6  -> 0x1e  (update 6)
 *   7  -> 0x1f  (update 6)
 *
 * Register args: impulse_index @<ax>, out_update_kind @<ebx>.
 * Returns kind index in AX (int16_t).
 *
 * Confirmed: MOV DI,AX at entry; switch dispatch from 0x1a969c.
 * Confirmed: MOV word ptr [EBX],0x3 or 0x6; MOV AX,SI; RET.
 * Confirmed: assert "animation_impulse>=0 &&
 * animation_impulse<NUMBER_OF_UNIT_ANIMATION_IMPULSES" file
 * "c:\\halo\\SOURCE\\units\\units.c", line 0x14f4.
 * Confirmed: kind init to -1 at entry (OR ESI,-1); two jump-table switches
 * (0x1a969c kind, 0x1a96d4/0x1a96dc update) each with a NULL-message assert
 * default (lines 0x1507, 0x1524).
 */
int16_t unit_impulse_to_animation_kind(int16_t impulse_index,
                                       int16_t *out_update_kind)
{
  int idx;
  int16_t kind = -1;

  if (impulse_index < 0 || impulse_index >= 14) {
    display_assert("animation_impulse>=0 && "
                   "animation_impulse<NUMBER_OF_UNIT_ANIMATION_IMPULSES",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x14f4, 1);
    system_exit(-1);
  }

  idx = impulse_index;
  switch (idx) {
  case 0:
    kind = 0x1d;
    break;
  case 1:
    kind = 0x20;
    break;
  case 2:
    kind = 0x21;
    break;
  case 3:
    kind = 0x22;
    break;
  case 4:
    kind = 0x1b;
    break;
  case 5:
    kind = 0x1c;
    break;
  case 6:
    kind = 0x1e;
    break;
  case 7:
    kind = 0x1f;
    break;
  case 8:
    kind = 0x04;
    break;
  case 9:
    kind = 0x05;
    break;
  case 10:
    kind = 0x06;
    break;
  case 11:
    kind = 0x07;
    break;
  case 12:
    kind = 0x28;
    break;
  case 13:
    kind = 0x29;
    break;
  default:
    display_assert(NULL, "c:\\halo\\SOURCE\\units\\units.c", 0x1507, 1);
    system_exit(-1);
    break;
  }

  if (out_update_kind != NULL) {
    switch (idx) {
    case 4:
    case 5:
    case 8:
    case 9:
    case 10:
    case 11:
      *out_update_kind = 3;
      break;
    case 0:
    case 1:
    case 2:
    case 3:
    case 6:
    case 7:
    case 12:
    case 13:
      *out_update_kind = 6;
      break;
    default:
      display_assert(NULL, "c:\\halo\\SOURCE\\units\\units.c", 0x1524, 1);
      system_exit(-1);
      break;
    }
  }

  return kind;
}

/* unit_animation_state_allows_impulse (0x1a96f0)
 *
 * Returns true if the unit's current animation state (unk_595 at +0x253)
 * is one that permits an animation impulse to be applied.
 *
 * States in [0x17..0x29] that block impulses (switch table 0x1a97a8):
 *   0x17-0x1b, 0x1d-0x1f, 0x20-0x23, 0x27, 0x29 -> return false.
 * States outside [0x17..0x29] -> fall to parent check.
 *
 * If parent_object_index != -1 (unit is seated/mounted):
 *   - If parent_seat_index is -1 -> return false.
 *   - Resolves parent via object_try_and_get_and_verify_type (0x13d640).
 *   - Looks up the seat-anim entry at parent_unit_tag+0x2e4, indexed by
 *     parent_seat_index, element size 0x11c.
 *   - If impulse_index is in [0xc, 0xd]: returns bit 8 of seat_anim[0].
 *   - If impulse_index is outside [0xc, 0xd]: returns false.
 *
 * If parent_object_index == -1 (no parent):
 *   - If impulse_index NOT in [0xc, 0xd] -> return true.
 *   - If impulse_index in [0xc, 0xd] -> return false.
 *
 * Register args: unit_handle @<eax>, impulse_index @<edi> (leaked from caller).
 * Returns bool in AL.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type.
 * Confirmed: MOVSX EAX,byte[ESI+0x253]; ADD -0x17; CMP 0x12; jump table
 * 0x1a97a8. Confirmed: MOVSX ECX,DI at 0x1a976c and 0x1a9786 (DI = caller's EDI
 * = anim_index). Confirmed: CMP [ESI+0x2a0],-1 -> parent_seat_index check. Confirmed:
 * object_try_and_get_and_verify_type at 0x13d640 / tag_get /
 * tag_block_get_element. Confirmed: MOV EAX,[EAX]; SHR EAX,8; AND AL,1 -> bit 8
 * gate.
 */
bool unit_animation_state_allows_impulse(int unit_handle, int impulse_index)
{
  unit_data_t *unit;
  int state;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  state = (int)(int8_t)unit->animation_state;

  /* Jump table at 0x1a97a0/0x1a97a8: MSVC normalises to state - 0x17 and
   * range-checks with CMP 0x12 / JA itself -- do not add an outer guard. */
  {
    switch (state) {
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1a:
    case 0x1b:
    case 0x1d:
    case 0x1e:
    case 0x1f:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x27:
    case 0x29:
      return false;
    default:
      break;
    }
  }

  /* Check parent (mounted) case */
  if (unit->object.parent_object_index.value != -1) {
    unit_data_t *parent;
    unit_definition_t *parent_tag;
    unit_seat_t *seat;

    if (unit->parent_seat_index == -1)
      return false;

    parent = object_try_and_get_and_verify_type(
      (int)unit->object.parent_object_index.value, _object_mask_unit);
    if (parent == NULL)
      return false;

    parent_tag = unit_definition_get(parent->object.tag_index);
    seat = tag_block_get_element(&parent_tag->seats,
                                 (int)unit->parent_seat_index,
                                 sizeof(unit_seat_t));

    /* Only impulses 0xc and 0xd can fire when mounted */
    if ((int16_t)impulse_index < 0xc || (int16_t)impulse_index > 0xd)
      return false;

    return (bool)((seat->flags >> 8) & 1);
  }

  /* No parent: impulses 0xc and 0xd are blocked */
  if ((int16_t)impulse_index >= 0xc && (int16_t)impulse_index <= 0xd)
    return false;

  return true;
}

/* unit_test_animation_impulse (0x1a97c0)
 *
 * Tests whether a specific animation impulse can be applied to a unit.
 * First checks if the current animation state allows the impulse, then
 * looks up the unit's animation tag block to verify the impulse has a
 * valid animation entry.
 *
 * Returns true (AL=1) if the animation impulse is available and mapped
 * to a non-NONE animation index; false otherwise.
 *
 * Tag lookup chain:
 *   unit tag (0x756e6974) -> animations tag ref at +0x44 ->
 *   animation tag (0x616e7472) -> block at +0xC, elem size 100 ->
 *   sub-block at result+0x58, elem size 0xBC ->
 *   count at +0x98, data pointer at +0x9C, indexed by animation kind.
 *
 * Confirmed: PUSH [EBP+0x8] / PUSH 0x3 -> cdecl, 2 stack params.
 * Confirmed: MOV EAX,EBX -> @eax for unit_animation_state_allows_impulse.
 * Confirmed: LEA EBX,[EBP-0x4] / MOV EAX,EDI -> @ebx, @ax for
 *            unit_impulse_to_animation_kind.
 * Confirmed: MOVSX EDX,byte[ESI+0x250] and byte[ESI+0x251] for anim indices.
 * Confirmed: CMP AX,0xffff / SETNZ AL for return logic.
 */
uint32_t unit_test_animation_impulse(int unit_handle, int impulse_index)
{
  unit_data_t *unit;
  char *anim_tag;
  animation_graph_unit_seat_t *seat_animations;
  animation_graph_weapon_class_t *weapon_class;
  int16_t anim_kind;
  int16_t update_kind;
  int16_t *data;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if (!unit_animation_state_allows_impulse(unit_handle, impulse_index)) {
    return 0;
  }

  {
    char *unit_tag;
    int anim_tag_index;

    unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
    anim_tag_index = object_definition_animation_graph_index(unit_tag);
    anim_tag = tag_get(TAG_GROUP_ANTR, anim_tag_index);
  }

  seat_animations = tag_block_get_element(
    &animation_graph_definition_unit_seats(anim_tag), (int)(int8_t)unit->animation_seat_index,
    sizeof(animation_graph_unit_seat_t));
  weapon_class = tag_block_get_element(
    &seat_animations->weapon_classes,
    (int)(int8_t)unit->animation_weapon_index,
    sizeof(animation_graph_weapon_class_t));

  /* Map impulse to animation kind */
  anim_kind =
    unit_impulse_to_animation_kind((int16_t)impulse_index, &update_kind);

  if (anim_kind < 0 || (int)anim_kind >= weapon_class->animations.count) {
    /* Out of range — return false (matches OR EAX,-1; CMP AX,0xffff; SETNZ) */
    return (uint32_t)(int16_t)-1 != (int16_t)-1;
  }

  data = weapon_class->animations.address;
  return (uint32_t)(data[anim_kind] != (int16_t)-1);
}

/* unit_get_aiming_unit_index (0x1a9880)
 * Returns the datum index of the unit that controls aiming.
 * If the unit is in a seat with the aiming flag, returns the parent. */
int unit_get_aiming_unit_index(int unit_index)
{
  unit_data_t *unit;
  unit_data_t *parent;
  unit_definition_t *parent_tag;
  uint8_t *seat;

  if (unit_index == -1) {
    return -1;
  }
  unit = object_get_and_verify_type(unit_index, _object_mask_unit);
  if (unit->object.parent_object_index.value != NONE &&
      unit->parent_seat_index != NONE) {
    parent = object_get_and_verify_type(unit->object.parent_object_index.value,
                                        _object_mask_unit);
    parent_tag = unit_definition_get(parent->object.tag_index);
    seat = tag_block_get_element(&parent_tag->seats,
                                 (int)unit->parent_seat_index,
                                 sizeof(unit_seat_t));
    if ((*seat & 9) != 0) {
      return unit->object.parent_object_index.value;
    }
  }
  return unit_index;
}

/* unit_scripting_unit_driver (0x1a9900)
 *
 * Copies the unit's aiming vector (offset 0x1EC) into the output
 * buffer. Resolves the unit via object_get_and_verify_type with type mask
 * 0x3 (biped | vehicle).
 */
void unit_get_aiming_vector(int unit_handle, void *out_aiming)
{
  unit_data_t *unit;
  float *out = (float *)out_aiming;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  out[0] = unit->aiming_vector.i;
  out[1] = unit->aiming_vector.j;
  out[2] = unit->aiming_vector.k;
}

/* unit_scripting_unit_gunner (0x1a9930)
 *
 * Copies the unit's looking vector (unk_528, offset 0x210) into the output
 * buffer. Resolves the unit via object_get_and_verify_type with type mask
 * 0x3 (biped | vehicle).
 */
void unit_get_looking_vector(int unit_handle, void *out_looking)
{
  unit_data_t *unit;
  float *out = (float *)out_looking;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  out[0] = unit->looking_vector.i;
  out[1] = unit->looking_vector.j;
  out[2] = unit->looking_vector.k;
}

/* units_debug_get_closest_unit (0x1a9960)
 *
 * Gets the unit's facing vector by delegating to object_get_orientation (an
 * object-level orientation getter in objects.c). Passes NULL for the up-vector
 * output, requesting only the forward direction.
 */
void unit_get_facing_vector(int unit_handle, void *out_facing)
{
  object_get_orientation(unit_handle, out_facing, 0);
}

/* unit_inventory_next_grenade (0x1a9980)
 * Cycles through grenade types to find the next available one. */
int16_t unit_inventory_next_grenade(int unit_handle, int current_index,
                                    int16_t direction)
{
  unit_data_t *unit;
  int16_t next_index;
  int16_t index;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  next_index = (int16_t)-1;

  if ((int16_t)current_index == (int16_t)-1) {
    current_index = 0;
  } else {
    if ((int16_t)current_index < 0 || (int16_t)current_index >= NUMBER_OF_UNIT_GRENADE_TYPES) {
      display_assert(
        "current_index>=0 && current_index<NUMBER_OF_UNIT_GRENADE_TYPES",
        "c:\\halo\\SOURCE\\units\\units.c", 0x163e, 1);
      system_exit(-1);
    }
  }

  index = (int16_t)current_index;
  do {
    if ((char)unit->grenade_counts[index] > 0) {
      next_index = index;
      if (index != (int16_t)current_index || direction == 0)
        break;
    }

    if (direction < 0) {
      index = (index == 0) ? 1 : (int16_t)(index - 1);
    } else {
      index = (index == 1) ? 0 : (int16_t)(index + 1);
    }
  } while (index != (int16_t)current_index);

  return next_index;
}

/* unit_is_alive (0x1a9a30)
 *
 * Returns whether the given unit handle refers to a unit that is currently
 * alive. Resolves the handle via object_get_and_verify_type with type mask
 * 0x3 (bit 0 = biped, bit 1 = vehicle — accepts any unit object; asserts
 * otherwise) and tests bit 6 of the unit's flag word at offset 0x1B4
 * (unit_data_t.unk_436).
 */
bool unit_is_alive(int unit_handle)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  return (unit->flags >> 6) & 1;
}

/* unit_set_controllable (0x1a9a50)
 * Sets or clears bit 6 (0x40) of unit flags dword at +0x1B4. */
void unit_set_controllable(int unit_handle, char controllable)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if (controllable) {
    unit->flags |= 0x40;
  } else {
    unit->flags &= ~0x40u;
  }
}

/* unit_set_possessed (0x1a9a90)
 * Sets or clears bit 27 (0x8000000) of unit flags dword at +0x1B4. */
void unit_set_possessed(int unit_handle, char possessed)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if (possessed) {
    unit->flags |= 0x8000000;
  } else {
    unit->flags &= ~0x8000000u;
  }
}

/* Check if a unit is in a vehicle seat based on seat state byte at +0x253. */
bool unit_is_busy(int unit_handle)
{
  unit_data_t *unit;
  bool is_busy;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  is_busy = false;
  switch ((signed char)unit->animation_state) {
  case 0x17:
  case 0x18:
  case 0x19:
  case 0x1a:
  case 0x1b:
  case 0x1d:
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x27:
  case 0x29:
    is_busy = true;
    break;
  }
  return is_busy;
}

/* unit_scripting_set_emotion_animation (0x1a9b30)
 * Looks up an animation by name and sets it as the unit's emotion animation. */
void unit_scripting_set_emotion_animation(int unit_index,
                                          const char *animation_name)
{
  unit_data_t *unit;
  int16_t anim;

  if (unit_index != -1) {
    unit = object_get_and_verify_type(unit_index, _object_mask_unit);
    anim = animation_graph_get_animation_by_name(unit->object.unk_124, animation_name);
    if (anim != -1) {
      unit->override_emotion_animation_index = anim;
      return;
    }
    console_warning("couldn\'t find the emotion animation \'%s\'",
                    animation_name);
  }
}

/* unit_scripting_suspended (0x1a9b80)
 * Suspends or unsuspends a unit. Sets/clears bit 24 (0x1000000) at +0x1B4,
 * zeros the velocity from global origin, and clears biped bit if type==0. */
void unit_scripting_suspended(int unit_index, char suspended)
{
  unit_data_t *unit;
  uint32_t flags;
  real_vector3d *global_origin;
  biped_data_t *biped;

  if (unit_index != -1) {
    unit = object_get_and_verify_type(unit_index, _object_mask_unit);
    if (suspended != '\0') {
      flags = unit->flags | 0x1000000;
    } else {
      flags = unit->flags & ~0x1000000;
    }
    unit->flags = flags;
    global_origin = GLOBAL_ZERO_VECTOR3D;
    unit->object.translational_velocity = *global_origin;

    if (unit->object.type == _object_type_biped) {
      biped = object_get_and_verify_type(unit_index, 1 << _object_type_biped);
      biped->flags &= ~(1 << _biped_airborne_bit);
    }
  }
}

/* unit_scripting_can_blink (0x1a9c00)
 * Sets whether a unit can blink. Inverted: bit 22 (0x400000) at +0x1B4
 * means "cannot blink", so can_blink=false sets the bit. */
void unit_scripting_can_blink(int unit_handle, char can_blink)
{
  unit_data_t *unit;

  if (unit_handle != -1) {
    unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
    if (can_blink == '\0') {
      unit->flags |= 0x400000;
    } else {
      unit->flags &= ~0x400000u;
    }
  }
}

/* unit_scripting_doesnt_drop_items (0x1a9c40)
 * Iterates child objects and sets bit 20 (0x100000) on each unit's flags. */
void unit_scripting_doesnt_drop_items(int object_list)
{
  int iter_state;
  int child;
  unit_data_t *unit;

  child = object_list_get_first(object_list, &iter_state);
  while (child != -1) {
    unit = object_try_and_get_and_verify_type(child, _object_mask_unit);
    if (unit != NULL) {
      unit->flags |= 0x100000;
    }
    child = object_list_get_next(object_list, &iter_state);
  }
}

/* unit_scripting_vehicle_test_seat_list (0x1a9c90)
 * True if any unit sitting in a seat named seat_name of the vehicle is in
 * object_list. */
char unit_scripting_vehicle_test_seat_list(int unit_handle, const char *seat_name, int object_list)
{
  unit_data_t *unit_data;
  unit_definition_t *unit_tag;
  tag_block *seats_block;
  char *seat_element;
  int obj_iter[4];
  int child_iter_state;
  int child_handle;
  int seated_handle;
  unit_data_t *seated_unit;
  char result;
  int16_t seat_index;
  int seat_count;

  result = 0;

  if (unit_handle == -1) {
    return 0;
  }

  unit_data = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = unit_definition_get(unit_data->object.tag_index);
  seats_block = &unit_tag->seats;
  seat_count = seats_block->count;
  seat_index = 0;

  if (seat_count <= 0) {
    return 0;
  }

  while ((int)seat_index < seat_count) {
    seat_element =
      tag_block_get_element(seats_block, (int)seat_index, sizeof(unit_seat_t));

    if (crt_stricmp(seat_name, seat_element + 4) == 0) {
      /* Seat name matches — find the occupant */
      object_iterator_new(obj_iter, _object_mask_unit, 0);
      seated_unit = object_iterator_next(obj_iter);

      while (seated_unit != NULL) {
        if (seated_unit->object.parent_object_index.value == unit_handle &&
            seated_unit->parent_seat_index == seat_index) {
          /* Found the unit sitting in this seat — check object list */
          seated_handle = obj_iter[2];
          child_handle = object_list_get_first(object_list, &child_iter_state);
          while (child_handle != -1) {
            if (seated_handle == child_handle) {
              goto found;
            }
            child_handle = object_list_get_next(object_list, &child_iter_state);
          }
          /* child_handle == -1 here; check if seated_handle also -1 */
          if (seated_handle == child_handle) {
            goto found;
          }
          break;
        }
        seated_unit = object_iterator_next(obj_iter);
      }
    }

    seat_index++;
    seat_count = seats_block->count;
    continue;
  found:
    result = 1;
    seat_index++;
    seat_count = seats_block->count;
  }

  return result;
}

/* unit_scripting_vehicle_test_seat (0x1a9da0)
 * Returns true if unit_index is sitting in the named seat of vehicle_index. */
char unit_scripting_vehicle_test_seat(int vehicle_index, const char *seat_name,
                                      int unit_index)
{
  unit_data_t *vehicle;
  unit_definition_t *vehicle_tag;
  tag_block *seats_block;
  char *seat_element;
  unit_data_t *unit_data;
  int16_t i;

  if (vehicle_index != -1 && unit_index != -1) {
    vehicle = object_get_and_verify_type(vehicle_index, _object_mask_unit);
    vehicle_tag = unit_definition_get(vehicle->object.tag_index);
    seats_block = &vehicle_tag->seats;
    if (0 < seats_block->count) {
      i = 0;
      do {
        seat_element =
          tag_block_get_element(seats_block, (int)i, sizeof(unit_seat_t));
        if (crt_stricmp(seat_name, seat_element + 4) == 0) {
          unit_data = object_get_and_verify_type(unit_index, _object_mask_unit);
          if (unit_data->object.parent_object_index.value == vehicle_index &&
              unit_data->parent_seat_index == i) {
            return 1;
          }
        }
        i++;
      } while ((int)i < seats_block->count);
    }
  }
  return 0;
}

/* unit_scripting_unit_riders (0x1a9e40)
 * Creates a list of all child units that have a seat assignment. */
int unit_scripting_unit_riders(int unit_handle)
{
  unit_data_t *unit;
  int list;
  int child;
  unit_data_t *child_obj;

  list = -1;
  if (unit_handle != -1) {
    unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
    list = object_list_new();
    if (list != -1) {
      child = unit->object.first_child_object_index.value;
      while (child != -1) {
        child_obj = object_get_and_verify_type(child, _object_mask_all);
        if (((1 << (*(uint8_t *)&child_obj->object.type & 0x1f)) &
             (unsigned)_object_mask_unit) != 0 &&
            child_obj->parent_seat_index != -1) {
          object_list_add(list, child);
        }
        child = child_obj->object.next_object_index.value;
      }
    }
  }
  return list;
}

/* unit_scripting_unit_driver (0x1a9ec0) */
int unit_scripting_unit_driver(int unit_handle)
{
  unit_data_t *unit;

  unit = object_try_and_get_and_verify_type(unit_handle, _object_mask_unit);
  if (unit != NULL) {
    return unit->driver_object_index.value;
  }
  return -1;
}

/* unit_scripting_unit_gunner (0x1a9ef0) */
int unit_scripting_unit_gunner(int unit_handle)
{
  unit_data_t *unit;

  unit = object_try_and_get_and_verify_type(unit_handle, _object_mask_unit);
  if (unit != NULL) {
    return unit->gunner_object_index.value;
  }
  return -1;
}

/* unit_shield_sapping_update (0x1a9f20)
 * While the unit plays its shield-sapping animation ('*') past the trigger
 * frame, damages every player unit within 4 world units (squared distance
 * below 16) with the unit definition's damage effect at +0x294. */
void unit_shield_sapping_update(int unit_handle)
{
  unit_data_t *unit;
  char *unit_tag;
  animation_graph_t *animation_graph;
  animation_t *animation;
  player_data_t *player;
  int player_unit_handle;
  unit_data_t *player_unit;
  float dx;
  float dy;
  float dz;
  struct damage_data damage;
  data_iter_t player_iter;
  char did_damage;
  int damage_effect_index;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);

  if ((char)unit->animation_state != '*') {
    return;
  }

  animation_graph = tag_get(TAG_GROUP_ANTR, unit->object.unk_124);
  animation = tag_block_get_element(&animation_graph->animations,
                                    (int)unit->object.unk_128,
                                    sizeof(animation_t));

  if (unit->object.unk_128 < animation->field_2e) {
    return;
  }

  did_damage = 0;
  data_iterator_new(&player_iter, PLAYER_DATA);
  player = data_iterator_next(&player_iter);

  while (player != NULL) {
    player_unit_handle = player->unit_handle;
    if (player_unit_handle != -1) {
      player_unit =
        object_get_and_verify_type(player_unit_handle, _object_mask_unit);

      dx = unit->object.bounding_sphere_center.x -
           player_unit->object.bounding_sphere_center.x;
      dy = unit->object.bounding_sphere_center.y -
           player_unit->object.bounding_sphere_center.y;
      dz = unit->object.bounding_sphere_center.z -
           player_unit->object.bounding_sphere_center.z;

      if (dx * dx + dy * dy + dz * dz < 16.0f) {
        damage_effect_index = unit_definition_melee_damage_index(unit_tag);
        damage_data_new(&damage, damage_effect_index);
        damage.owner_object_index = unit_handle;
        object_cause_damage(&damage, player_unit_handle, (short)-1,
                            (short)-1, (short)-1, (float *)0);
        did_damage = 1;
      }
    }
    player = data_iterator_next(&player_iter);
  }

  if (did_damage) {
    unit->shield_sap_timeout = 0;
  } else {
    unit->shield_sap_timeout += 1;
  }
}

/* units_debug_get_next_unit (0x1aa080)
 * Finds the next debug-selectable unit after current_unit. */
int units_debug_get_next_unit(int current_unit)
{
  int result;
  unit_data_t *unit;
  char l_14[8];
  int l_c = 0;

  result = -1;
  if (current_unit != -1) {
    object_iterator_new(l_14, _object_mask_unit, 0);
    unit = object_iterator_next(l_14);
    while (unit != NULL && l_c != current_unit) {
      unit = object_iterator_next(l_14);
    }
    unit = object_iterator_next(l_14);
    while (unit != NULL) {
      if (unit->actor_index.value == -1 && unit->swarm_actor_index.value == -1 &&
          (unit->object.damage_flags & 4) == 0) {
        result = l_c;
        if (l_c != -1) {
          return l_c;
        }
        break;
      }
      unit = object_iterator_next(l_14);
    }
  }
  object_iterator_new(l_14, _object_mask_unit, 0);
  unit = object_iterator_next(l_14);
  while (1) {
    if (unit == NULL) {
      return result;
    }
    if (unit->actor_index.value == -1 && unit->swarm_actor_index.value == -1 &&
        (unit->object.damage_flags & 4) == 0) {
      break;
    }
    unit = object_iterator_next(l_14);
  }
  return l_c;
}

/* units_debug_get_closest_unit (0x1aa170)
 * Nearest biped to reference_object_index; with NONE every candidate has
 * distance 0, so the first valid biped wins. */
int units_debug_get_closest_unit(int reference_object_index)
{
  int closest_index;
  float closest_distance;
  int iterator[4];
  object_data_t *object;
  real_point3d reference_origin;
  real_point3d object_origin;
  float dx, dy, dz, distance;

  closest_index = -1;
  closest_distance = 3.4028235e+38f;

  object_iterator_new(iterator, 1 << _object_type_biped, 0);
  object = object_iterator_next(iterator);
  while (object != NULL) {
    if (iterator[2] != reference_object_index && (object->damage_flags & 4) == 0) {
      if (reference_object_index != -1) {
        object_get_world_position(reference_object_index, &reference_origin);
        object_get_world_position(iterator[2], &object_origin);
        dx = object_origin.x - reference_origin.x;
        dy = object_origin.y - reference_origin.y;
        dz = object_origin.z - reference_origin.z;
        distance = dz * dz;
        distance += dx * dx;
        distance += dy * dy;
        distance = sqrtf(distance);
      } else {
        distance = REAL_ZERO_POOL;
      }
      if (distance < closest_distance) {
        closest_index = iterator[2];
        closest_distance = distance;
      }
    }
    object = object_iterator_next(iterator);
  }
  return closest_index;
}

/* unit_debug_ninja_rope (0x1aa240)
 * Collision-based ninja rope debug function.
 * Casts a ray from the unit's aim position along a scaled facing direction.
 * If the ray hits a surface whose normal points up (k > 0.95), moves the
 * unit to the hit point raised by 0.25. */
void unit_debug_ninja_rope(int unit_handle)
{
  unit_data_t *unit;
  real_point3d origin;
  real_vector3d direction;
  collision_test_result collision;
  int16_t depth;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_get_camera_position(unit_handle, &origin);

  direction.i = unit->aiming_vector.i * 25.0f;
  direction.j = unit->aiming_vector.j * 25.0f;
  direction.k = unit->aiming_vector.k * 25.0f;

  if (global_current_collision_user_depth >=
      MAXIMUM_COLLISION_USER_STACK_DEPTH) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x19f8, 1);
    system_exit(-1);
  }

  depth = global_current_collision_user_depth;
  global_current_collision_user_depth = depth + 1;
  collision_user_stack[depth] = 0x15;

  if (collision_test_vector(0x22, &origin, &direction, unit_handle,
                   (int16_t *)&collision)) {
    if (collision.normal.k > 0.95f) {
      collision.position.z += 0.25f;
      object_translate(unit_handle, &collision.position, 0);
    }
  }

  if (global_current_collision_user_depth < 2) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1a01, 1);
    system_exit(-1);
  }
  global_current_collision_user_depth -= 1;
}

/* unit_set_user_animation (0x1aa360)
 * Only validates the index in this build; always returns 0. */
char unit_set_user_animation(int unit_handle, int param_2, int16_t index)
{
  unit_data_t *unit;
  int unit_tag_index;
  char *unit_tag;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag_index = unit->object.tag_index;
  unit_tag = tag_get(TAG_GROUP_UNIT, unit_tag_index);
  tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_tag));

  if (index < 0 || index >= 2) {
    display_assert("index>=0 && index<NUMBER_OF_UNIT_USER_ANIMATIONS",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1a21, 1);
    system_exit(-1);
  }

  return 0;
}

/* any_unit_is_dangerous (0x1aa3c0)
 * Called by game_all_quiet, game_safe_to_save and players_respawn_coop. */
bool any_unit_is_dangerous(void)
{
  int iter_buf[4];
  unit_data_t *unit;
  uint8_t state;

  object_iterator_new(iter_buf, _object_mask_unit, 1);
  unit = object_iterator_next(iter_buf);

  while (unit != NULL) {
    state = unit->animation_state;
    if (state == 0x21 && unit->grenade_throw_state != 3)
      return true;
    if ((state == 0x19 || state == 0x18) && (unit->animation_flags & 4) == 0)
      return true;
    unit = object_iterator_next(iter_buf);
  }

  return false;
}

/* unit_can_see_point (0x1aa430)
 * True if point lies inside the cone of half_angle around the unit's
 * looking vector, measured from its "head" marker. */
char unit_can_see_point(int unit_handle, float *point, float half_angle)
{
  unit_data_t *unit;
  char marker_buf[0x78];
  real_point3d *marker_pos;
  real_vector3d dir;
  float dot;

  if (unit_handle == -1) {
    return 0;
  }

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  object_get_marker_by_name(unit_handle, MARKER_NAME_HEAD, marker_buf, 1);

  /* dir must stay one contiguous vector: normalize3d reads all three
   * components through its base pointer. */
  marker_pos = (real_point3d *)(marker_buf + 0x60);
  dir.i = point[0] - marker_pos->x;
  dir.j = point[1] - marker_pos->y;
  dir.k = point[2] - marker_pos->z;

  normalize3d(&dir);

  dot = dir.i * unit->looking_vector.i + dir.j * unit->looking_vector.j +
        dir.k * unit->looking_vector.k;

  if (dot > x87_fcos(half_angle)) {
    return 1;
  }

  return 0;
}

/* unit_driven_by_ai (0x1aa4d0) — unit_driven_by_ai
 * Returns true if the unit (or its driver) has an AI actor. */
char unit_driven_by_ai(int unit_handle)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if (unit->driver_object_index.value != NONE) {
    unit = object_get_and_verify_type(unit->driver_object_index.value, _object_mask_unit);
  }
  return unit->actor_index.value != NONE;
}

/* unit_gunned_by_ai (0x1aa510) — unit_gunned_by_ai
 * Returns true if the unit (or its gunner) has an AI actor. */
char unit_gunned_by_ai(int unit_handle)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if (unit->gunner_object_index.value != NONE) {
    unit = object_get_and_verify_type(unit->gunner_object_index.value, _object_mask_unit);
  }
  return unit->actor_index.value != NONE;
}

/* unit_set_desired_flashlight_state (0x1aa550) */
void unit_set_desired_flashlight_state(int unit_handle, char desired)
{
  unit_data_t *unit;

  if (unit_handle != -1) {
    unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
    if (desired != '\0') {
      unit->flags |= 0x10000000;
    } else {
      unit->flags |= 0x20000000;
    }
  }
}

/* unit_get_current_flashlight_state (0x1aa590) */
char unit_get_current_flashlight_state(int unit_handle)
{
  unit_data_t *unit;
  char state = 0;

  if (unit_handle != -1) {
    unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
    state = (char)((unit->flags >> 19) & 1);
  }
  return state;
}

/* unit_detach_from_parent (0x1aa5c0)
 * Ejects the unit away from its parent (along its forward vector when the
 * two positions coincide) with an added velocity of 0.02 (0x2b6af4). */
void unit_detach_from_parent(int object_handle)
{
  unit_data_t *unit;
  int parent_handle;
  real_point3d parent_pos;
  real_point3d child_pos;
  real_vector3d dir;
  float len;

  unit = object_get_and_verify_type(object_handle, _object_mask_unit);
  parent_handle = unit->object.parent_object_index.value;
  if (parent_handle == -1)
    return;

  object_get_and_verify_type(parent_handle, _object_mask_unit);
  object_get_world_position(parent_handle, &parent_pos);
  object_get_world_position(object_handle, &child_pos);

  dir.i = child_pos.x - parent_pos.x;
  dir.j = child_pos.y - parent_pos.y;
  dir.k = child_pos.z - parent_pos.z;

  len = normalize3d(&dir);
  if (len == REAL_ZERO_POOL) {
    dir.i = unit->object.forward.i;
    dir.j = unit->object.forward.j;
    dir.k = unit->object.forward.k;
  }

  dir.i = dir.i * REAL_0_02_POOL;
  dir.j = dir.j * REAL_0_02_POOL;
  dir.k = dir.k * REAL_0_02_POOL;

  object_detach_from_parent(object_handle);

  child_pos.x = unit->object.position.x;
  child_pos.y = unit->object.position.y;
  child_pos.z = unit->object.position.z;
  scenario_ensure_point_within_world(&child_pos);
  object_set_position(object_handle, &child_pos, NULL, NULL);

  unit->object.flags &= ~0x20u;
  unit->flags &= ~0x8000u;

  unit->object.translational_velocity.i += dir.i;
  unit->object.translational_velocity.j += dir.j;
  unit->object.translational_velocity.k += dir.k;

  object_set_visibility(object_handle, 1);
  object_compute_node_matrices(object_handle);
}

char unit_seat_filled(int unit_handle, int16_t seat_index)
{
  unit_data_t *unit;
  char iterator[16];
  char result = 0;

  object_iterator_new(iterator, _object_mask_unit, 0);
  for (unit = object_iterator_next(iterator); unit != NULL;
       unit = object_iterator_next(iterator)) {
    if (unit->object.parent_object_index.value == unit_handle &&
        unit->parent_seat_index == seat_index) {
      return 1;
    }
  }
  return result;
}

/* unit_seat_is_driver (0x1aa770)
 * Returns true if the seat at seat_index has the driver flag (bit 2). */
char unit_seat_is_driver(int unit_handle, int16_t seat_index)
{
  unit_data_t *unit;
  unit_definition_t *unit_tag;
  unit_seat_t *seat;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = unit_definition_get(unit->object.tag_index);
  if (seat_index >= 0) {
    if ((int)seat_index < unit_tag->seats.count) {
      seat = tag_block_get_element(&unit_tag->seats, (int)seat_index,
                                   sizeof(unit_seat_t));
      return (seat->flags >> 2) & 1;
    }
  }
  return 0;
}

/* unit_seat_is_gunner (0x1aa7d0)
 * Returns true if the seat at seat_index has the gunner flag (bit 3). */
enum {
  _unit_seat_driver_bit = 2,
  _unit_seat_gunner_bit = 3,
  _unit_seat_allows_noncombatants_bit = 10
};

char unit_seat_is_gunner(int unit_handle, int16_t seat_index)
{
  unit_data_t *unit;
  unit_definition_t *unit_definition;
  unit_seat_t *seat;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition = unit_definition_get(unit->object.tag_index);
  if (seat_index >= 0) {
    if ((int)seat_index < unit_definition->seats.count) {
      seat = tag_block_get_element(&unit_definition->seats, (int)seat_index,
                                   sizeof(unit_seat_t));
      return (seat->flags >> _unit_seat_gunner_bit) & 1;
    }
  }
  return 0;
}

/* unit_seat_allow_noncombatants (0x1aa830) */
char unit_seat_allow_noncombatants(int unit_handle, int16_t seat_index)
{
  unit_data_t *unit;
  unit_definition_t *unit_definition;
  unit_seat_t *seat;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition = unit_definition_get(unit->object.tag_index);
  if (seat_index >= 0) {
    if ((int)seat_index < unit_definition->seats.count) {
      seat = tag_block_get_element(&unit_definition->seats, (int)seat_index,
                                   sizeof(unit_seat_t));
      return (seat->flags >> _unit_seat_allows_noncombatants_bit) & 1;
    }
  }
  return 0;
}

/* unit_update_seat_occupancy (0x1aa890)
 * Walks the vehicle's children and records the occupants of its driver and
 * gunner seats.  Register arg: vehicle_handle in EAX. */
void unit_update_seat_occupancy(int vehicle_handle)
{
  unit_data_t *unit;
  unit_definition_t *unit_definition;
  int child_handle;
  unit_data_t *child;
  unit_seat_t *seat;
  int seat_flags;

  unit = object_get_and_verify_type(vehicle_handle, _object_mask_unit);
  unit_definition = unit_definition_get(unit->object.tag_index);
  child_handle = unit->object.first_child_object_index.value;

  while (child_handle != NONE) {
    child = object_get_and_verify_type(child_handle, _object_mask_all);

    if (((1 << (child->object.type & 0x1f)) & _object_mask_unit) &&
        child->parent_seat_index != NONE) {
      seat = tag_block_get_element(&unit_definition->seats,
                                   (int)child->parent_seat_index,
                                   sizeof(unit_seat_t));
      seat_flags = seat->flags;

      if ((seat_flags & FLAG(_unit_seat_driver_bit)) == 0 ||
          (unit->flags & 1) ||
          unit->driver_object_index.value != NONE) {
        if (seat_flags & FLAG(_unit_seat_gunner_bit)) {
          if (unit->gunner_object_index.value == NONE ||
              unit->gunner_object_index.value == unit->driver_object_index.value) {
            unit->gunner_object_index.value = child_handle;
          }
        }
      } else {
        unit->driver_object_index.value = child_handle;
        if (seat_flags & FLAG(_unit_seat_gunner_bit)) {
          if (unit->gunner_object_index.value == NONE) {
            unit->gunner_object_index.value = child_handle;
          }
        }
      }
    }

    child_handle = child->object.next_object_index.value;
  }
}

/* unit_get_equipment (0x1aa970) */
int unit_get_equipment(int unit_handle)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  return unit->equipment_object_index.value;
}

/* unit_try_add_grenade (0x1aa990)
 * Picks up a grenade equipment object if the unit has room for another
 * grenade of its type; returns false when full. */
/* Game globals +0x128 grenade block element, indexed by grenade type. */
typedef struct game_globals_grenade_t {
  int16_t maximum_count;
  byte pad_02[0x04 - 0x02];
  tag_reference throwing_effect;
  byte pad_14[0x24 - 0x14];
  tag_reference equipment;  /* dropped by unit_drop_grenades_on_death */
  tag_reference projectile; /* spawned by unit_throw_grenade_move_to_hand */
} game_globals_grenade_t;
cs(game_globals_grenade_t, 0x44);
co(game_globals_grenade_t, throwing_effect, 0x04);
co(game_globals_grenade_t, equipment, 0x24);
co(game_globals_grenade_t, projectile, 0x34);

/* 'eqip' tag: only the fields read here. Size unproven. */
typedef struct equipment_definition_t {
  byte pad_000[0x308];
  int16_t powerup_type; /* equipment_definition->equipment.powerup_type */
  int16_t grenade_type;
} equipment_definition_t;
co(equipment_definition_t, powerup_type, 0x308);
co(equipment_definition_t, grenade_type, 0x30a);

bool unit_try_add_grenade(int unit_handle, int equipment_handle)
{
  object_data_t *equipment;
  equipment_definition_t *equipment_definition;
  unit_data_t *unit;
  int16_t grenade_type;
  game_globals_grenade_t *grenade;
  char current_count;
  int local_player_index;
  player_data_t *player;

  equipment = object_get_and_verify_type(equipment_handle, 8);
  equipment_definition = tag_get(TAG_GROUP_EQIP, equipment->tag_index);
  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  /* NTSC looks up the grenade globals element before the powerup assert. */
  grenade_type = equipment_definition->grenade_type;
  grenade = tag_block_get_element(
    &game_globals_grenades(game_globals_get()), grenade_type,
    sizeof(game_globals_grenade_t));

  if (equipment_definition->powerup_type != _equipment_powerup_grenade) {
    display_assert("equipment_definition->equipment.powerup_type==_equipment_"
                   "powerup_grenade",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1c72, 1);
    system_exit(-1);
  }

  if (grenade) {
    current_count = unit->grenade_counts[grenade_type];
    if ((int16_t)current_count < grenade->maximum_count) {
      unit->grenade_counts[grenade_type] = current_count + 1;

      /* player_index_from_unit_index is called twice (0x1aaa33 and 0x1aaa41). */
      if (player_index_from_unit_index(unit_handle) == NONE) {
        local_player_index = NONE;
      } else {
        player = datum_get(player_data,
                           player_index_from_unit_index(unit_handle));
        local_player_index = player->local_player_index;
      }

      if (local_player_index != -1)
        equipment_handle_pickup(equipment_handle);

      object_delete(equipment_handle);
      return true;
    }
  }

  return false;
}

/* unit_set_grenade_count (0x1aaa90)
 * Adds grenade_count grenades of grenade_type and selects that type; returns
 * the new (sign-extended) count. */
int16_t unit_set_grenade_count(int unit_handle, int16_t grenade_type,
                               int16_t grenade_count)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if (grenade_count < 0) {
    display_assert("grenade_count>=0", "c:\\halo\\SOURCE\\units\\units.c",
                   0x1c8d, 1);
    system_exit(-1);
  }

  if ((grenade_type < 0) || (grenade_type >= NUMBER_OF_UNIT_GRENADE_TYPES)) {
    display_assert(
      "(grenade_type >= 0) && (grenade_type < NUMBER_OF_UNIT_GRENADE_TYPES)",
      "c:\\halo\\SOURCE\\units\\units.c", 0x1c8e, 1);
    system_exit(-1);
  }

  unit->grenade_counts[grenade_type] += (char)grenade_count;
  unit->desired_grenade_index = (char)grenade_type;
  unit->current_grenade_index = (char)grenade_type;

  return (int16_t)(char)unit->grenade_counts[grenade_type];
}

/* unit_pickup_equipment (0x1aab20)
 * Stores a non-grenade powerup in the unit's equipment slot; flag==1
 * replaces equipment already held. */
bool unit_pickup_equipment(int unit_handle, int equipment_handle, short flag)
{
  object_data_t *equipment;
  equipment_definition_t *equipment_definition;
  unit_data_t *unit;
  int player_handle;
  player_data_t *player;

  equipment = object_get_and_verify_type(equipment_handle, 8);
  equipment_definition = tag_get(TAG_GROUP_EQIP, equipment->tag_index);
  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if (equipment_definition->powerup_type == _equipment_powerup_none) {
    display_assert(
      "equipment_definition->equipment.powerup_type!=_equipment_powerup_none",
      "c:\\halo\\SOURCE\\units\\units.c", 0x1ca1, 1);
    system_exit(NONE);
  }
  if (equipment_definition->powerup_type == _equipment_powerup_grenade) {
    display_assert("equipment_definition->equipment.powerup_type!=_equipment_"
                   "powerup_grenade",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1ca2, 1);
    system_exit(NONE);
  }

  if (unit->equipment_object_index.value != NONE && flag == 1) {
    object_delete(unit->equipment_object_index.value);
    unit->equipment_object_index.value = NONE;
  }

  if (unit->equipment_object_index.value == NONE) {
    object_disconnect_from_map(equipment_handle);
    object_set_visibility(equipment_handle, 0);

    player_handle = player_index_from_unit_index(unit_handle);
    if (player_handle != NONE) {
      player_handle = player_index_from_unit_index(unit_handle);
      player = datum_get(player_data, player_handle);
      if (player->local_player_index != NONE) {
        equipment_handle_pickup(equipment_handle);
      }
    }

    item_in_unit_inventory(equipment_handle, unit_handle);
    unit->equipment_object_index.value = equipment_handle;
    return true;
  }

  return false;
}

/* unit_clear_seat_tag (0x1aac40)
 * Deletes the unit's equipment object, if any. */
void unit_clear_seat_tag(int unit_handle)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if (unit->equipment_object_index.value != NONE) {
    object_delete(unit->equipment_object_index.value);
    unit->equipment_object_index.value = NONE;
  }
}

/* unit_clear_weapons (0x1aac80)
 * Deletes every weapon except the current one. */
void unit_clear_weapons(int unit_handle)
{
  unit_data_t *unit;
  int16_t i;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    if (unit->weapon_object_indices[i].value != NONE && i != unit->current_weapon_index) {
      object_delete(unit->weapon_object_indices[i].value);
      unit->weapon_object_indices[i].value = NONE;
      if (i == unit->desired_weapon_index) {
        unit->desired_weapon_index = (uint16_t)NONE;
      }
      if (i == unit->current_weapon_index) {
        unit->current_weapon_index = (uint16_t)NONE;
      }
    }
  }
}

/* unit_has_weapon_definition_index (0x1aad00)
 * Checks whether a unit is carrying a weapon with the given tag definition. */
char unit_has_weapon_definition_index(int unit_handle, int definition_index)
{
  unit_data_t *unit;
  int weapon_handle;
  object_data_t *weapon;
  int16_t i;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  i = 0;
  do {
    weapon_handle = unit->weapon_object_indices[i].value;
    if (weapon_handle != NONE) {
      weapon = object_get_and_verify_type(weapon_handle, 4);
      if (weapon->tag_index == definition_index) {
        return 1;
      }
    }
    i++;
  } while (i < MAXIMUM_WEAPONS_PER_UNIT);
  return 0;
}

/* unit_find_empty_weapon_slot (0x1aad60)
 * Register arg: unit_handle in EAX. */
int16_t unit_find_empty_weapon_slot(int unit_handle)
{
  unit_data_t *unit;
  int16_t i;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    if (unit->weapon_object_indices[i].value == NONE)
      return i;
  }
  return NONE;
}

/* unit_count_weapons (0x1aad90)
 * Counts held weapons whose definition lacks flag bit 4 at weap +0x308. */
int16_t unit_count_weapons(int unit_handle)
{
  unit_data_t *unit;
  int count;
  int weapon_handle;
  object_data_t *weapon;
  char *weapon_definition;
  int i;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  count = 0;

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    weapon_handle = unit->weapon_object_indices[i].value;
    if (weapon_handle != NONE) {
      weapon = object_get_and_verify_type(weapon_handle, 4);
      weapon_definition = tag_get(TAG_GROUP_WEAP, weapon->tag_index);
      if ((*(uint8_t *)&weapon_definition_flags(weapon_definition) & 0x10) == 0) {
        count++;
      }
    }
  }

  return (int16_t)count;
}

/* unit_weapon_is_new (0x1aae00)
 * True when no held weapon shares the weapon's definition. */
bool unit_weapon_is_new(int unit_handle, int weapon_unit_handle)
{
  unit_data_t *unit;
  object_data_t *weapon;
  object_data_t *slot_weapon;
  bool is_new;
  int weapon_handle;
  int i;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  weapon = object_get_and_verify_type(weapon_unit_handle, 4);
  tag_get(TAG_GROUP_WEAP, weapon->tag_index);
  is_new = true;

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    weapon_handle = unit->weapon_object_indices[i].value;
    if (weapon_handle != NONE) {
      slot_weapon = object_get_and_verify_type(weapon_handle, 4);
      if (weapon->tag_index == slot_weapon->tag_index) {
        is_new = false;
      }
    }
  }

  return is_new;
}

/* unit_get_grenade_count (0x1aae70) */
int16_t unit_get_grenade_count(int unit_handle, int16_t grenade_type)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if (grenade_type != NONE) {
    if (grenade_type < 0 || grenade_type >= NUMBER_OF_UNIT_GRENADE_TYPES) {
      display_assert("grenade_type==NONE || (grenade_type>=0 && "
                     "grenade_type<NUMBER_OF_UNIT_GRENADE_TYPES)",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x1ea7, 1);
      system_exit(-1);
    }
    return (int16_t)(char)unit->grenade_counts[grenade_type];
  }

  return 0;
}

/* unit_get_current_grenade_type (0x1aaee0) */
int16_t unit_get_current_grenade_type(int unit_handle)
{
  unit_data_t *unit;
  char grenade_index;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  grenade_index = unit->current_grenade_index;

  if (grenade_index != NONE && (grenade_index < 0 || grenade_index > 1)) {
    display_assert(
      "unit->unit.current_grenade_index==NONE || "
      "(unit->unit.current_grenade_index>=0 && "
      "unit->unit.current_grenade_index<NUMBER_OF_UNIT_GRENADE_TYPES)",
      "c:\\halo\\SOURCE\\units\\units.c", 0x1eb8, 1);
    system_exit(-1);
  }

  return (int16_t)(signed char)unit->current_grenade_index;
}

/* unit_throw_grenade_move_to_hand (0x1aaf40)
 * Spends a grenade (unless grenades are unlimited), spawns it at the
 * grenade marker and attaches it to the unit.
 * Register arg: unit_handle in EDI. */
void unit_throw_grenade_move_to_hand(int unit_handle)
{
  unit_data_t *unit;
  game_globals_grenade_t *grenade;
  int grenade_index;
  object_marker marker;
  object_placement_data placement;
  real_vector3d forward;
  real_vector3d perpendicular[4]; /* only [0] accessed; 0x30-byte slot at EBP-0x48 */
  int grenade_handle;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition_get(unit->object.tag_index);

  grenade_index = (int)(int8_t)unit->current_grenade_index;
  {
    char *game_globals;
    game_globals = game_globals_get();
    grenade = tag_block_get_element(game_globals + 0x128, grenade_index,
                                    sizeof(game_globals_grenade_t));
  }

  if (unit->player_index.value != NONE) {
    if (CHEAT_INFINITE_AMMO != 0) {
      goto skip_decrement;
    }
    if (game_engine_infinite_grenades(unit->player_index.value) != 0) {
      goto skip_decrement;
    }
  }

  if (unit->actor_index.value != NONE) {
    if (actor_has_unlimited_grenades() != 0) {
      goto skip_decrement;
    }
  }

  if ((int8_t)unit->current_grenade_index < 0 ||
      (int8_t)unit->current_grenade_index > 1) {
    display_assert(
      "unit->unit.current_grenade_index>=0 && "
      "unit->unit.current_grenade_index<NUMBER_OF_UNIT_GRENADE_TYPES",
      "c:\\halo\\SOURCE\\units\\units.c", 0x1f1e, true);
    system_exit(-1);
  }

  if ((int8_t)unit->grenade_counts[(int8_t)unit->current_grenade_index] < 1) {
    display_assert(
      "unit->unit.grenade_counts[unit->unit.current_grenade_index]>0",
      "c:\\halo\\SOURCE\\units\\units.c", 0x1f1f, true);
    system_exit(-1);
  }

  unit->grenade_counts[(int8_t)unit->current_grenade_index] -= 1;

skip_decrement:
  object_get_marker_by_name(unit_handle, MARKER_NAME_LEFT_HAND, &marker, 1);

  object_placement_data_new(&placement, grenade->projectile.tag_index,
                            unit_handle);
  placement.field_04 |= 2;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  forward.i = unit->aiming_vector.i;
  forward.j = unit->aiming_vector.j;
  forward.k = unit->aiming_vector.k;

  perpendicular3d((float *)&forward, (float *)&perpendicular[0]);
  normalize3d(&perpendicular[0]);

  placement.position_x = marker.matrix.position.x;
  placement.position_y = marker.matrix.position.y;
  placement.position_z = marker.matrix.position.z;

  grenade_handle = object_new(&placement);
  if (grenade_handle != NONE) {
    object_attach_to_parent(unit_handle, grenade_handle, *(int *)&marker);
    unit->grenade_object_index.value = grenade_handle;
    unit->grenade_throw_state = 2;
    return;
  }

  unit->grenade_throw_state = 3;
}

/* Game globals +0x170 player-information block element. */
typedef struct game_globals_player_information_t {
  byte pad_00[0x34];
  real field_34;
  byte pad_38[0x68 - 0x38];
  real_vector3d grenade_origin;
  byte pad_74[0x8c - 0x74];
  real minimum_stun_time;
  real maximum_stun_time;
  byte pad_94[0xf4 - 0x94];
} game_globals_player_information_t;
cs(game_globals_player_information_t, 0xf4);
co(game_globals_player_information_t, field_34, 0x34);
co(game_globals_player_information_t, grenade_origin, 0x68);
co(game_globals_player_information_t, minimum_stun_time, 0x8c);

/* unit_throw_grenade_release (0x1ab110)
 * Detaches the held grenade and throws it along the aiming vector; a throw
 * cut short (flag) blends in a random spread. */
void unit_throw_grenade_release(int unit_handle, char flag)
{
  unit_data_t *unit;
  unit_definition_t *unit_definition;
  int grenade_handle;
  int ebx;
  game_globals_player_information_t *player_information;
  real_vector3d forward;
  real_vector3d left;
  real_vector3d cross2;
  real_vector3d velocity;
  real_point3d throw_position;
  object_data_t *grenade;
  float ratio_val;
  float throw_speed;
  float rand_val;
  float rand_x;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_THROW_UNIT_XY,
               RNG_TRACE_BITS(unit->object.position.x),
               RNG_TRACE_BITS(unit->object.position.y));
  RNG_TRACE_EX(RNG_TRACE_KIND_THROW_UNIT_Z_HANDLE,
               RNG_TRACE_BITS(unit->object.position.z),
               (unsigned int)unit_handle);
#endif
  unit_definition = unit_definition_get(unit->object.tag_index);

  if (unit->grenade_throw_state != 2) {
    return;
  }

  grenade_handle = unit->grenade_object_index.value;
  ebx = NONE;
  if (grenade_handle == ebx) {
    return;
  }

  object_detach_from_parent(grenade_handle);

  if (unit->actor_index.value != ebx) {
    object_get_world_position(unit->grenade_object_index.value,
                              (vector3_t *)&cross2);
    actor_aim_grenade(unit->actor_index.value, &cross2, &velocity);
  } else {
    if (unit->player_index.value != ebx) {
      /* Players throw from the camera, offset by the globals tag. */
      player_information = tag_block_get_element(
        &game_globals_player_information(game_globals_get()), 0,
        sizeof(game_globals_player_information_t));

      forward = unit->aiming_vector;

      cross_product3d(global_up_vector3d, &forward, &left);
      if (normalize3d(&left) == 0.0f) {
        float *up;
        up = global_up_vector_ptr;
        left.i = up[0];
        left.j = up[1];
        left.k = up[2];
      }

      cross_product3d(&forward, &left, &cross2);
      normalize3d(&cross2);

      unit_get_camera_position(unit_handle, &throw_position);
#ifdef HALO_RNG_TRACE
      RNG_TRACE_EX(RNG_TRACE_KIND_THROW_SEAT_XY, RNG_TRACE_BITS(throw_position.x),
                   RNG_TRACE_BITS(throw_position.y));
      RNG_TRACE_EX(RNG_TRACE_KIND_THROW_SEAT_Z_HANDLE,
                   RNG_TRACE_BITS(throw_position.z), (unsigned int)unit_handle);
#endif

      /* One scale temp per axis group: VC71 reloads it before each
       * FMUL triple rather than preloading all three. */
      {
        float s;
        s = player_information->grenade_origin.i;
        throw_position.x += forward.i * s;
        throw_position.y += forward.j * s;
        throw_position.z += forward.k * s;
        s = player_information->grenade_origin.j;
        throw_position.x += left.i * s;
        throw_position.y += left.j * s;
        throw_position.z += left.k * s;
        s = player_information->grenade_origin.k;
        throw_position.x += cross2.i * s;
        throw_position.y += cross2.j * s;
        throw_position.z += cross2.k * s;
      }

#ifdef HALO_RNG_TRACE
      RNG_TRACE_EX(RNG_TRACE_KIND_THROW_FINAL_XY, RNG_TRACE_BITS(throw_position.x),
                   RNG_TRACE_BITS(throw_position.y));
      RNG_TRACE_EX(RNG_TRACE_KIND_THROW_FINAL_Z_HANDLE,
                   RNG_TRACE_BITS(throw_position.z), (unsigned int)unit_handle);
#endif
      object_translate(grenade_handle, &throw_position, 0);
      ebx = -1;
    }

    /* 0x2546a4 holds 1/30 (seconds per tick). */
    throw_speed = unit_definition_grenade_velocity(unit_definition) * REAL_ONE_THIRTIETH_POOL;
    velocity.i = throw_speed * unit->aiming_vector.i;
    velocity.j = throw_speed * unit->aiming_vector.j;
    velocity.k = throw_speed * unit->aiming_vector.k;
  }

  if (flag != 0) {
    int throw_timer = (int)*(int16_t *)&unit->unk_574;
    int throw_total = (int)unit->grenade_throw_full_power_ticks;
    ratio_val = (float)throw_timer / (float)throw_total;
    if (ratio_val < 1.0f) {
      rand_val =
        random_real_range(get_global_random_seed_address(), 0.020000001f, 0.046666667f);
      rand_x = rand_val * unit->aiming_vector.i;
      cross2.j = rand_val * unit->aiming_vector.j;
      cross2.k = rand_val * unit->aiming_vector.k;

      velocity.i = velocity.i * ratio_val;
      velocity.j = velocity.j * ratio_val;
      velocity.k = velocity.k * ratio_val;

      ratio_val = 1.0f - ratio_val;
      HALO_FLT_ROUNDTRIP(ratio_val);
      velocity.i = rand_x * ratio_val + velocity.i;
      velocity.j = cross2.j * ratio_val + velocity.j;
      velocity.k = cross2.k * ratio_val + velocity.k;
    }
  }

  grenade = object_get_and_verify_type(grenade_handle, ebx);
  {
    real_vector3d *grenade_velocity = &grenade->translational_velocity;
    velocity.i -= grenade_velocity->i;
    velocity.j -= grenade_velocity->j;
    velocity.k -= grenade_velocity->k;
  }

  projectile_accelerate(grenade_handle, &velocity);

  unit->grenade_throw_state = 3;
  unit->grenade_object_index.value = NONE;

  unit_get_camera_position(unit_handle, (real_point3d *)&cross2);
  if (object_try_place(grenade_handle, (real_point3d *)&cross2) == 0) {
    object_delete(grenade_handle);
  }
}

/* unit_aiming_vector (0x1ab410)
 * Despite its name this updates the seat acceleration (+0x320): the second
 * difference of the unit's (or its seat marker's) position, projected onto
 * the unit axes, scaled, biased by 0.5 and clamped to [0, 0.7].
 * Register arg: unit_handle in EAX. */
void unit_aiming_vector(int unit_handle)
{
  unit_data_t *unit;
  int parent_handle;
  unit_data_t *parent;
  unit_definition_t *unit_definition;
  char *seat;
  float *scale;
  int16_t marker_count;
  char marker[60];
  real_point3d position;
  real_vector3d forward;
  real_vector3d up;
  real_vector3d left;
  float dx, dy, dz;
  float ddx, ddy, ddz;
  float value;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  parent_handle = unit->object.parent_object_index.value;

  if (parent_handle == NONE || unit->parent_seat_index == NONE) {
    unit_definition = unit_definition_get(unit->object.tag_index);
    position = unit->object.position;
    forward = unit->object.forward;
    up = unit->object.up;
    scale = (float *)&unit_definition_acceleration_scale(unit_definition);
  } else {
    parent = object_get_and_verify_type(parent_handle, _object_mask_unit);
    unit_definition = unit_definition_get(parent->object.tag_index);
    seat = tag_block_get_element(&unit_definition->seats,
                                 (int)unit->parent_seat_index,
                                 sizeof(unit_seat_t));
    marker_count = object_get_marker_by_name(parent_handle, seat + 0x24,
                                             marker, 1);
    if (marker_count == 0) {
      unit->seat_desired_acceleration.x = 0.5f;
      unit->seat_desired_acceleration.y = 0.5f;
      unit->seat_desired_acceleration.z = 0.5f;
      return;
    }
    object_get_world_position(parent_handle, &position);
    scale = (float *)(seat + 0x64);
  }


  dx = position.x - unit->seat_last_position.x;
  dy = position.y - unit->seat_last_position.y;
  dz = position.z - unit->seat_last_position.z;

  ddx = dx - unit_seat_last_velocity(unit).i;
  ddy = dy - unit_seat_last_velocity(unit).j;
  ddz = dz - unit_seat_last_velocity(unit).k;

  /* left = up x forward (FSUBP order at 0x1ab537-0x1ab55f) */
  left.i = forward.k * up.j - forward.j * up.k;
  left.j = up.k * forward.i - forward.k * up.i;
  left.k = up.i * forward.j - up.j * forward.i;

  unit->seat_desired_acceleration.x =
    (forward.i * ddx + forward.j * ddy + forward.k * ddz) * scale[0] + 0.5f;
  unit->seat_desired_acceleration.y =
    (left.i * ddx + left.j * ddy + left.k * ddz) * scale[1] + 0.5f;
  unit->seat_desired_acceleration.z =
    (up.i * ddx + up.j * ddy + up.k * ddz) * scale[2] + 0.5f;

  value = unit->seat_desired_acceleration.x;
  if (value < 0.0f)
    value = 0.0f;
  else if (value > 0.7f)
    value = 0.7f;
  unit->seat_desired_acceleration.x = value;

  value = unit->seat_desired_acceleration.y;
  if (value < 0.0f)
    value = 0.0f;
  else if (value > 0.7f)
    value = 0.7f;
  unit->seat_desired_acceleration.y = value;

  value = unit->seat_desired_acceleration.z;
  if (value < 0.0f)
    value = 0.0f;
  else if (value > 0.7f)
    value = 0.7f;
  unit->seat_desired_acceleration.z = value;

  unit->seat_last_position = position;

  unit_seat_last_velocity(unit).i = dx;
  unit_seat_last_velocity(unit).j = dy;
  unit_seat_last_velocity(unit).k = dz;
}

/* base_seat_label_get (0x1ab6e0)
 * Returns a pointer to the base seat name string given a base_seat_index.
 * Asserts that the index is in [0, NUMBER_OF_UNIT_BASE_SEATS).
 * The seat name table at 0x32e484 contains: asleep, alert, stand, crouch, flee,
 * flaming. */
const char *base_seat_label_get(int16_t base_seat_index)
{
  if (base_seat_index < 0 || base_seat_index >= NUMBER_OF_UNIT_BASE_SEATS) {
    display_assert(
      "base_seat_index>=0 && base_seat_index<NUMBER_OF_UNIT_BASE_SEATS",
      "c:\\halo\\SOURCE\\units\\units.c", 0x200f, 1);
    system_exit(-1);
  }
  return *(const char **)(BASE_SEAT_LABELS_ADDRESS + (int)base_seat_index * 4);
}

/* seat_label_to_base_seat_index (0x1ab730)
 * Searches the base seat name table for a matching name (case-insensitive).
 * Returns the index [0..5] if found, or -1 if no match.
 * @edi = seat_name string to search for. */
int16_t seat_label_to_base_seat_index(const char *seat_name)
{
  int16_t i;

  for (i = 0; i < NUMBER_OF_UNIT_BASE_SEATS; i++) {
    if (crt_stricmp(seat_name, *(const char **)(BASE_SEAT_LABELS_ADDRESS + (int)i * 4)) == 0) {
      return i;
    }
  }
  return -1;
}

/* base_weapon_label_get (0x1ab770)
 * Returns a pointer to the base weapon name string given a base_weapon_index.
 * Asserts that the index is in [0, NUMBER_OF_UNIT_BASE_WEAPONS).
 * The weapon name table is a local array containing just "unarmed". */
const char *base_weapon_label_get(int16_t base_weapon_index)
{
  const char *weapon_names[1];

  weapon_names[0] = "unarmed";

  if (base_weapon_index < 0 ||
      base_weapon_index >= NUMBER_OF_UNIT_BASE_WEAPONS) {
    display_assert("base_weapon_index>=0 && "
                   "base_weapon_index<NUMBER_OF_UNIT_BASE_WEAPONS",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x2043, 1);
    system_exit(-1);
  }
  return weapon_names[base_weapon_index];
}

/* unit_set_animation (0x1ab7c0)
 * Starts an animation on the unit; the debug flag at 0x5054fc logs it,
 * filtered by the actor handle at 0x5ac9f8. */
void unit_set_animation(int unit_handle, int anim_graph_tag_index,
                        int16_t animation_index)
{
  unit_data_t *unit;
  const char *anim_name;
  int debug_filter;
  animation_graph_t *animation_graph;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  unit->object.unk_124 = anim_graph_tag_index;
  unit->object.unk_128 = animation_index;
  unit->object.unk_130 = 0;

  if (DEBUG_UNIT_ALL_ANIMATIONS != 0) {
    anim_name = "<none>";
    if (anim_graph_tag_index != NONE) {
      animation_graph = tag_get(TAG_GROUP_ANTR, anim_graph_tag_index);
      if (animation_index != NONE) {
        anim_name = tag_block_get_element(&animation_graph->animations,
                                          (int)animation_index, 0xb4);
      }
    }

    debug_filter = AI_DEBUG_SELECTED_ACTOR_INDEX;
    if (debug_filter == NONE || unit->actor_index.value == debug_filter ||
        unit->swarm_actor_index.value == debug_filter) {
      console_printf(0, "%s: animation %s",
                     tag_name_strip_path(tag_get_name(unit->object.tag_index)),
                     anim_name);
    }
  }
}

/* unit_animation_update (0x1ab870)
 * Updates an animation state and optionally triggers an impulse sound.
 * Calls animation_update_internal with update_kind=1, using the animation
 * graph tag index and state pointer passed via @ecx/@edx.
 * If the update produces a sound index (!= -1), plays it via
 * object_impulse_sound_new with scale 1.0. Returns the animation result. */
int16_t unit_animation_update(void *animation_state, int animation_graph_tag_index,
                     int unit_handle)
{
  int16_t result;
  int sound_index;

#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_ANIM_UPDATE_IN,
               (unsigned int)*(uint16_t *)animation_state |
                 ((unsigned int)*((uint16_t *)animation_state + 1) << 16),
               unit_handle);
#endif
#line 6344
  result = (int16_t)animation_update_internal(
    1, animation_graph_tag_index, (short *)animation_state, &sound_index);
#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_ANIM_UPDATE_OUT,
               (unsigned int)(uint16_t)result |
                 ((unsigned int)*(uint16_t *)animation_state << 16),
               unit_handle);
#endif
#line 6346
  if (sound_index != -1) {
    object_impulse_sound_new(unit_handle, sound_index, 0, GLOBAL_ORIGIN3D,
                             GLOBAL_FORWARD3D, 1.0f);
  }
  return result;
}

/* unit_refresh_illumination (0x1ab8c0)
 * A seated unit inherits its parent unit's illumination. */
void unit_refresh_illumination(int unit_handle)
{
  unit_data_t *unit;
  unit_data_t *parent;
  real_rgb_color color;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  parent = object_try_and_get_and_verify_type(
    unit->object.parent_object_index.value, _object_mask_unit);
  if (parent == NULL) {
    lights_illumination_at_point((int)&unit->object.position,
                                 (int)&unit->object.location, &color.red);
    unit->ambient_illumination = real_rgb_color_brightness(&color.red);
    unit->self_illumination = object_get_self_illumination(unit_handle);
    return;
  }
  unit->ambient_illumination = parent->ambient_illumination;
  unit->self_illumination = parent->self_illumination;
}

/* unit_get_weapon (0x1ab940)
 * Returns the weapon handle at the given weapon slot index for a unit.
 * The index is validated against [0, MAXIMUM_WEAPONS_PER_UNIT).
 * Returns -1 (NONE) if the index is -1. */
int unit_get_weapon(int16_t weapon_index, char *unit_data)
{
  if (weapon_index == -1) {
    return -1;
  }
  if (weapon_index < 0 || weapon_index >= MAXIMUM_WEAPONS_PER_UNIT) {
    display_assert("index>=0 && index<MAXIMUM_WEAPONS_PER_UNIT",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x20ac, 1);
    system_exit(-1);
  }
  return ((unit_data_t *)unit_data)->weapon_object_indices[weapon_index].value;
}

/* unit_detach_weapon (0x1ab990)
 * Drops an item the unit holds: attaches a stowed item to the hand marker
 * first, then tosses it from the unit's velocity in a random cone around the
 * aiming vector.  Register args: unit_handle in EDI, weapon_handle in ESI. */
void unit_detach_weapon(int unit_handle, int weapon_handle)
{
  unit_data_t *unit;
  item_data_t *weapon;
  int parent;
  float *global_origin;
  real_vector3d direction;
  real_point3d position;
  unsigned int *seed;
  float scale;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  weapon = object_get_and_verify_type(weapon_handle, 0x1c);
  parent = weapon->object.parent_object_index.value;

  if (parent == NONE) {
    object_connect_to_map(weapon_handle, 0);
    object_set_visibility(weapon_handle, 1);
    object_attach_to_marker(unit_handle, MARKER_NAME_LEFT_HAND, weapon_handle,
                            EMPTY_STRING_VOID);
  } else if (parent != unit_handle) {
    display_assert("item->object.parent_object_index==unit_index",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x20c5, 1);
    system_exit(-1);
  }

  item_in_unit_inventory(weapon_handle, NONE);
  object_detach_from_parent(weapon_handle);

  global_origin = global_zero_vector_ptr;
  weapon->object.translational_velocity.i = global_origin[0];
  weapon->object.translational_velocity.j = global_origin[1];
  weapon->object.translational_velocity.k = global_origin[2];

  global_origin = global_zero_vector_ptr;
  weapon->object.angular_velocity.i = global_origin[0];
  weapon->object.angular_velocity.j = global_origin[1];
  weapon->object.angular_velocity.k = global_origin[2];

  /* Generate random throw direction from unit's aiming vector */
  seed = get_global_random_seed_address();
  seed_random_vector_in_cone3d(seed, &unit->aiming_vector, 0.0f,
                               0.39269909f, &direction);

  /* Scale direction by random amount */
  seed = get_global_random_seed_address();
  scale = random_real_range(seed, 0.026666667f, 0.040000003f); /* 0x3d23d70b, not 0.04f (0x3d23d70a) */
  direction.i *= scale;
  direction.j *= scale;
  direction.k *= scale;

  /* Get root parent position and offset by scaled direction */
  object_get_velocities(unit_handle, (real_vector3d *)&position, 0);
  direction.i = position.x + direction.i;
  direction.j = position.y + direction.j;
  direction.k = position.z + direction.k;

  /* Remember which unit dropped the item. */
  weapon->unk_432 = unit_handle;

  /* Set weapon position and try to place it */
  item_accelerate(weapon_handle, &direction, 0);
  unit_get_camera_position(unit_handle, &position);

  if (!object_try_place(weapon_handle, &position)) {
    if (!game_engine_running()) {
      object_delete(weapon_handle);
    }
  }

  if (unit->flags & 0x100000) {
    object_delete(weapon_handle);
  }
}

/* unit_drop_grenades_on_death (0x1abb20)
 * Spawns and drops one grenade object per grenade carried.
 * Register arg: unit_handle in EAX. */
void unit_drop_grenades_on_death(int unit_handle)
{
  unit_data_t *unit;
  char *grenade_count;
  int count;
  int grenade_type;
  game_globals_grenade_t *grenade;
  int grenade_handle;
  object_placement_data placement;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  grenade_count = (char *)unit->grenade_counts;

  count = NUMBER_OF_UNIT_GRENADE_TYPES;
  do {
    grenade_type = (int)(grenade_count - (char *)unit->grenade_counts);
    grenade = tag_block_get_element(&game_globals_grenades(game_globals_get()),
                                    grenade_type,
                                    sizeof(game_globals_grenade_t));

    while (*grenade_count > 0) {
      object_placement_data_new(&placement, grenade->equipment.tag_index,
                                unit_handle);
      grenade_handle = object_new(&placement);
      if (grenade_handle != NONE) {
        object_disconnect_from_map(grenade_handle);
        unit_detach_weapon(unit_handle, grenade_handle);
      }
      (*grenade_count)--;
    }
    grenade_count++;
    count--;
  } while (count != 0);
}

/* unit_drop_weapons_on_death (0x1abbd0)
 * Drops every weapon but the current one; weapons that can no longer fire
 * are deleted.  Register arg: unit_handle in EAX. */
void unit_drop_weapons_on_death(int unit_handle)
{
  unit_data_t *unit;
  int weapon_handle;
  object_data_t *weapon;
  int slot;
  datum_handle_t *weapon_slot;
  const char *unit_name;
  const char *weapon_name;
  const char *msg;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  slot = 0;
  weapon_slot = unit->weapon_object_indices;

  do {
    weapon_handle = weapon_slot->value;
    if (weapon_handle != NONE &&
        (int16_t)slot != unit->current_weapon_index) {
      weapon = object_get_and_verify_type(weapon_handle, 4);
      if ((weapon->flags & 0x800) != 0) {
        weapon_name = tag_get_name(weapon->tag_index);
        unit_name = tag_get_name(unit->object.tag_index);
        msg =
          csprintf(error_string_buffer,
                   "a %s tried to drop a %s which was connected to the map.",
                   unit_name, weapon_name);
        display_assert(msg, "c:\\halo\\SOURCE\\units\\units.c", 0x2132, 1);
        system_exit(-1);
      }
      unit_detach_weapon(unit_handle, weapon_handle);
      if ((int16_t)slot == unit->desired_weapon_index) {
        unit->desired_weapon_index = unit->current_weapon_index;
      }
      weapon_slot->value = NONE;
      if (!weapon_can_be_fired(weapon_handle)) {
        object_delete(weapon_handle);
      }
    }
    slot++;
    weapon_slot++;
  } while ((int16_t)slot < MAXIMUM_WEAPONS_PER_UNIT);
}

/* unit_handle_region_destroyed (0x1abcd0)
 * Triggers scream when a body region is destroyed. */
void unit_handle_region_destroyed(int unit_handle, int param_2, uint32_t flags)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if ((unit->object.damage_flags & 4) == 0) {
    unit_scream(unit_handle, ((flags & 0x200) != 0) + 3);
  }
}

/* unit_melee_sound (0x1abd10)
 * Plays the material's melee impact sound, then the damage effect's.
 * Register args: material_type in EAX, unit_handle in ESI,
 * weapon_tag_index (a 'jpt!' tag or NONE) in EDI. */
void unit_melee_sound(int16_t material_type, int unit_handle, int weapon_tag_index)
{
  char *material_effects;
  int sound_tag;
  damage_effect_definition_t *damage_effect;
  real_point3d *position;
  real_vector3d *forward;

  position = GLOBAL_ORIGIN3D;
  forward = GLOBAL_FORWARD3D;

  material_effects = (char *)scenario_material_definition_get(material_type);
  sound_tag = material_definition_melee_hit_sound_index(material_effects);
  if (sound_tag != NONE) {
    object_impulse_sound_new(unit_handle, sound_tag, -1, position, forward,
                             1.0f);
  }

  if (weapon_tag_index != NONE) {
    damage_effect = tag_get(TAG_GROUP_JPT, weapon_tag_index);
    sound_tag = damage_effect->sound_tag_index;
    if (sound_tag != NONE) {
      object_impulse_sound_new(unit_handle, sound_tag, -1, position, forward,
                               1.0f);
    }
  }
}

/* unit_cause_continuous_melee_damage (0x1abd90)
 * While in melee state 4, damages the parent object with the unit's melee
 * damage effect, at the collision point when the first test hits.
 * Register arg: unit_handle in EDI. */
/* collision_model_instance_new output; +0x0c is the node matrix array. */
typedef struct collision_model_instance_t {
  int32_t field_00;
  int32_t field_04;
  int32_t field_08;
  int32_t node_matrices; /* real_matrix4x3 array, indexed by node */
} collision_model_instance_t;
cs(collision_model_instance_t, 0x10);

/* collision_model_test_vector output: node/region/permutation of the hit
 * ahead of the bsp test result. */
typedef struct collision_model_test_vector_result_t {
  int16_t node_index;
  int16_t region_index;
  int16_t field_04;
  byte pad_06[2];
  collision_bsp_test_vector_result_t bsp;
} collision_model_test_vector_result_t;
cs(collision_model_test_vector_result_t, 0x420);
co(collision_model_test_vector_result_t, bsp, 0x08);

void unit_cause_continuous_melee_damage(int unit_handle)
{
  unit_data_t *unit;
  char *unit_definition;
  int parent_handle;
  /* Frame layout from the original (EBP-0x4bc frame):
   *   [ebp-0x4bc] collision_result  0x420 bytes
   *   [ebp-0x9c]  collision_instance 0x10 bytes
   *   [ebp-0x8c]  damage            0x54  bytes
   *   [ebp-0x38]  normal_out        4 floats
   *   [ebp-0x28]  surface_out       3 floats
   *   [ebp-0x1c]  point_out         3 floats
   *   [ebp-0xc]   direction         3 floats */
  collision_model_instance_t collision_instance;
  collision_model_test_vector_result_t collision_result;
  struct damage_data damage;
  real_vector3d direction;
  real_point3d point_out;
  real_point3d surface_out;
  real_plane3d normal_out;
  char hit_found;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);

  if (unit->melee_attack_state != 4) {
    return;
  }
  parent_handle = unit->object.parent_object_index.value;
  if (parent_handle == NONE) {
    return;
  }
  if (unit_definition_melee_damage_index(unit_definition) == NONE) {
    return;
  }

  hit_found = 0;

  if (unit->melee_continuous_damage_effect_timer == 0) {
    if (COLLISION_USER_STACK_DEPTH >= MAXIMUM_COLLISION_USER_STACK_DEPTH) {
      display_assert("global_current_collision_user_depth < "
                     "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x22e6, true);
      system_exit(-1);
    }

    {
      int16_t coll_depth;
      char coll_result;

      coll_depth = COLLISION_USER_STACK_DEPTH;
      coll_depth = coll_depth + 1; /* ref: incw %ax on the loaded value */
      COLLISION_USER_STACK_DEPTH = coll_depth;
      *(int16_t *)(0x5a8c80 + (coll_depth - 1) * 2) = 8;

      coll_result = collision_model_instance_new((int *)&collision_instance, parent_handle);

      if (coll_result != 0) {
        object_get_world_position(unit_handle, &point_out);

        direction.i = unit->object.forward.i * REAL_0_2_POOL;
        direction.j = unit->object.forward.j * REAL_0_2_POOL;
        direction.k = unit->object.forward.k * REAL_0_2_POOL;

        vector3d_scale_add(&point_out, &direction, -0.5f, &point_out);

        coll_result = collision_model_test_vector((int)&collision_instance, (void *)3,
                                                  &point_out, &direction,
                                                  (int16_t *)&collision_result);

        if (coll_result != 0) {
          vector3d_scale_add(&point_out, &direction, collision_result.bsp.t,
                             &surface_out);

          /* Get surface normal */
          {
            int surface_base;
            surface_base = collision_instance.node_matrices;
            /* bsp.plane is a POINTER to the surface plane, not an inline
             * plane (ref 0x1abee0 loads the slot's value and pushes it). */
            matrix4x3_transform_plane(
              (float *)(surface_base + collision_result.node_index * 0x34),
              (float *)collision_result.bsp.plane, (float *)&normal_out);
          }

          /* Negate normal if backfacing */
          if (collision_result.bsp.plane_designator < 0) {
            plane_negate((float *)&normal_out, (float *)&normal_out);
          }

          hit_found = 1;
        }
      }

      if (COLLISION_USER_STACK_DEPTH <= 1) { /* ref: cmp 1 / jg */
        display_assert("global_current_collision_user_depth > 1",
                       "c:\\halo\\SOURCE\\units\\units.c", 0x22fe, true);
        system_exit(-1);
      }
      --COLLISION_USER_STACK_DEPTH; /* ref: single decw */
    }
  }

  {
    int damage_effect;
    damage_effect = unit_definition_melee_damage_index(unit_definition);
    damage_data_new(&damage, damage_effect);
  }

  /* Set damage params common fields.  +0x00 holds the damage-effect tag index
   * written by damage_data_new — never overwrite it.
   * Both sources are loaded before the first store, as in the reference. */
  {
    int16_t owner_team;
    int owner_player;

    owner_team = unit->object.owner_team_index;
    owner_player = unit->player_index.value;
    damage.owner_object_index = unit_handle;
    damage.owner_team_index = owner_team;
    damage.owner_player_index = owner_player;
  }
  damage.scale = 1.0f / 30.0f; /* ref imm 0x3d088889 */

  if (hit_found) {
    damage.origin.x = surface_out.x;
    damage.origin.y = surface_out.y;
    damage.origin.z = surface_out.z;
    damage.epicenter.x = surface_out.x;
    damage.epicenter.y = surface_out.y;
    damage.epicenter.z = surface_out.z;

    {
      const float *unit_forward = (const float *)&unit->object.forward;

      damage.direction.i = unit_forward[0];
      damage.direction.j = unit_forward[1];
      damage.direction.k = unit_forward[2];
    }

    damage.flags |= 2;
    unit->melee_continuous_damage_effect_timer = 10;

    object_cause_damage(
      &damage, parent_handle, collision_result.node_index,
      collision_result.region_index, collision_result.bsp.material_index,
      (float *)&normal_out);
  } else {
    object_cause_damage(&damage, parent_handle, (short)-1, (short)-1,
                        (short)-1, (float *)0);
  }

  unit->melee_continuous_damage_effect_timer =
    unit->melee_continuous_damage_effect_timer - 1;
}

/* unit_has_weapon_with_flag (0x1ac3f0)
 * True when any held weapon has bit flag_index set in its weapon flags. */
bool unit_has_weapon_with_flag(int unit_handle, int flag_index)
{
  unit_data_t *unit;
  datum_handle_t *weapon_slots;
  int i;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  weapon_slots = unit->weapon_object_indices;

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    if (weapon_slots[i].value != NONE) {
      weapon_data_t *weapon = object_get_and_verify_type(weapon_slots[i].value, 4);
      if ((1 << (flag_index & 0x1f)) & weapon->unk_476)
        return 1;
    }
  }
  return 0;
}

/* unit_start_running_blindly (0x1ac450)
 * Sets unit flag 0x2000000 and picks a random heading: within 25 degrees of
 * the actor's running-blind vector, else within 100 degrees of the facing. */
void unit_start_running_blindly(int unit_handle)
{
  unit_data_t *unit;
  uint32_t flags;
  int actor_handle;
  float angle_range;
  real_vector3d run_vector;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  flags = unit->flags;
  if ((flags & 0x2000000) != 0) {
    return;
  }
  unit->flags = flags | 0x2000000;

  actor_handle = unit->actor_index.value;
  if (actor_handle != NONE &&
      actor_get_running_blind_vector(actor_handle, (float *)&run_vector)) {
    unit->run_blindly_angle = 0.0f;
    angle_range = 0.43633232f;
  } else {
    real_euler_angles2d facing_angles;

    vector_to_angles(&facing_angles, &unit->object.forward);
    if (facing_angles.yaw > _pi) {
      facing_angles.yaw -= 2.f * _pi;
    }
    unit->run_blindly_angle = facing_angles.yaw;
    angle_range = 1.7453293f;
  }

  unit->run_blindly_angle =
    random_real_range(get_global_random_seed_address(), -angle_range, angle_range) +
    unit->run_blindly_angle;
}

/* unit_stop_running_blindly (0x1ac520) */
void unit_stop_running_blindly(int unit_handle)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit->flags &= ~0x2000000u;
}

/* unit_flame_to_death (0x1ac550)
 * Applies the globals' flaming-death damage effect, crediting the object
 * that set the unit on fire. */
/* Element 0 of the game globals' 0x98-byte block at +0x188. */
typedef struct game_globals_falling_damage_t {
  byte pad_00[0x6c];
  tag_reference flaming_death_damage;
  byte pad_7c[0x98 - 0x7c];
} game_globals_falling_damage_t;
cs(game_globals_falling_damage_t, 0x98);
co(game_globals_falling_damage_t, flaming_death_damage, 0x6c);

void unit_flame_to_death(int unit_handle)
{
  unit_data_t *unit;
  char *globals;
  game_globals_falling_damage_t *falling_damage;
  unit_data_t *unit_again;
  struct damage_data damage;
  object_data_t *attacker;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  globals = game_globals_get();
  falling_damage = tag_block_get_element(globals + 0x188, 0,
                                         sizeof(game_globals_falling_damage_t));

  unit_again = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_again->flags &= ~0x02000000u;

  unit->object.unk_183 &= ~0x08;
  unit->flags &= ~0x80u;

  if (falling_damage != NULL &&
      falling_damage->flaming_death_damage.tag_index != NONE) {
    attacker = object_try_and_get_and_verify_type(
      unit->flaming_death_attacker_object_index.value, _object_mask_all);

    damage_data_new(&damage, falling_damage->flaming_death_damage.tag_index);

    if (attacker != NULL) {
      damage.owner_player_index = attacker->owner_player_index;
      {
        int cause_object = attacker->owner_object_index;
        if (cause_object == NONE) {
          cause_object = unit->flaming_death_attacker_object_index.value;
        }
        damage.owner_object_index = cause_object;
      }
      damage.owner_team_index = attacker->owner_team_index;
    }

    object_cause_damage(&damage, unit_handle, -1, -1, -1, (float *)0);
  }

  if ((unit->object.damage_flags & 0x4) == 0) {
    const char *tag_name;
    const char *stripped;
    tag_name = tag_get_name(unit->object.tag_index);
    stripped = tag_name_strip_path(tag_name);
    error(2, "WARNING: %s tried to die from flaming to death but couldn't",
          stripped);
    unit->object.damage_flags |= 0x20;
  }
}

/* unit_flying_through_air (0x1ac650) */
char unit_flying_through_air(int unit_handle)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if (unit->object.type == _object_type_biped) {
    return biped_flying_through_air(unit_handle);
  }
  return 0;
}

/* unit_euler_axis_buildplan (0x1ac680)
 * Builds an accelerate / coast / decelerate plan that brings initial_p to
 * zero.  Recurses with negated inputs when moving away from the target. */
void unit_euler_axis_buildplan(float initial_p, float initial_v, float max_v, float max_a,
                  int plan)
{
  int at_rest;
  volatile float f1; /* |initial_v| / max_a, later the coasting velocity.
                         * volatile forces the store-once/reload-each-use
                         * pattern of the original (fstps [EBP+0x18] + FLDS),
                         * which is also the faithful x87 float narrowing. */
  float f4; /* discriminated stop distance — FPU-stack resident in
                * the original (FCOMS keeps it live into both time
                * branches), never stored to memory */
  float sq; /* sqrt(disc) — FPU-stack resident */
  float vmax_t; /* velocity-limit time — FPU-stack resident */
  float scratch; /* -max_a in the quadratic, later the coast-time
                  * numerator (shares the single [EBP-4] local slot) */
  char bVar; /* initial_v > 0 */
  unit_acceleration_plan_t *result = (unit_acceleration_plan_t *)plan;

  *(int *)&result->accel_a = 0x7f7fffff;
  *(int *)&result->accel_t = 0x7f7fffff;
  *(int *)&result->coast_t = 0x7f7fffff;
  *(int *)&result->decel_a = 0x7f7fffff;
  *(int *)&result->decel_t = 0x7f7fffff;
  result->initial_p = initial_p;
  result->initial_v = initial_v;

  /* At-rest test computed as a value (MOV EAX,1 / XOR EAX,EAX) and stored
   * once via AL, then re-tested — not two constant stores. fabs is the
   * inline FABS intrinsic; the compare is done at double width (FCOMPL). */
  at_rest = fabs(initial_p) < 0.001f && fabs(initial_v) < 0.001f;
  result->at_rest = (uint8_t)at_rest;
  if ((uint8_t)at_rest) {
    *(int *)&result->accel_a = 0;
    *(int *)&result->accel_t = 0;
    *(int *)&result->coast_t = 0;
    *(int *)&result->decel_a = 0;
    *(int *)&result->decel_t = 0;
    return;
  }

  f1 = (float)(fabs(initial_v) / max_a);
  bVar = (initial_v > 0.0f) ? 1 : 0;

  /* Check if we're moving in the wrong direction (need to reverse) */
  if (f1 * 0.5f * initial_v * 0.5f + initial_p < 0.0f) {
    /* Recursive case: negate and re-plan */
    unit_euler_axis_buildplan(-initial_p, -initial_v, max_v, max_a, plan);
    result->initial_p = result->initial_p * -1.0f;
    result->initial_v = result->initial_v * -1.0f;
    result->accel_a = result->accel_a * -1.0f;
    result->decel_a = result->decel_a * -1.0f;
    goto validate;
  }

  /* f4 stays on the FPU stack across this branch in the original
   * (non-popping FCOMS at 0x1ac7ab). VC71 sinks the large else block past
   * the validate join, reproducing the original layout where the main
   * time computation sits after the epilogue (0x1ac90c..0x1acbea) and
   * jumps back to validate. */
  f4 = initial_v * 0.5f * f1 + initial_p;
  if (f4 < 0.0f) {
    /* Overshoot case: initial_p near zero, velocity away from target.
     * All asserts use the !(cond) macro form so the unordered (NaN) path
     * faithfully falls into the assert, matching the original masks. */
    if (!(initial_p > -0.001f)) {
      display_assert("plan->initial_p > -1e-03f",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x7b7, 1);
      system_exit(-1);
    }
    if (!(result->initial_v < 0.0f)) {
      display_assert("plan->initial_v < 0", "c:\\halo\\SOURCE\\units\\units.c",
                     0x7b8, 1);
      system_exit(-1);
    }
    *(int *)&result->accel_a = 0;
    *(int *)&result->accel_t = 0;
    result->decel_a = result->initial_v * result->initial_v /
                              (result->initial_p + result->initial_p);
    result->decel_t = -(result->initial_v / result->decel_a);
    *(int *)&result->coast_t = 0;
  } else {
    /* Main time computation.
     *
     * MSVC reuses the initial_p ([EBP+8]) and initial_v ([EBP+0xc]) param
     * stack slots as scratch once the parameters are dead. We mirror that
     * by assigning into the parameters themselves:
     *   initial_v -> doubled_v, then the second quadratic root, then the
     *                selected raw time t
     *   initial_p -> discriminant, then the first quadratic root, then
     *                the velocity-clamped time (actual_t) */
    if (bVar) {
      /* Accelerating (initial_v > 0): direct sqrt on the live f4 */
      initial_v = sqrtf(f4 / max_a);
    } else {
      /* Decelerating (initial_v <= 0): quadratic formula */
      scratch = -max_a;
      initial_v = initial_v + initial_v; /* doubled_v */
      initial_p = initial_v * initial_v - scratch * f4 * 4.0f; /* disc */
      if (!(initial_p >= 0.0f)) {
        display_assert("disc >= 0", "c:\\halo\\SOURCE\\units\\units.c", 0x7eb,
                       1);
        system_exit(-1);
      }
      sq = sqrtf(initial_p);
      initial_p = (-initial_v - sq) / (scratch + scratch); /* root t */
      initial_v = (sq - initial_v) / (scratch + scratch); /* root actual_t */
      if (initial_p >= 0.0f && (initial_v < 0.0f || initial_p < initial_v)) {
        initial_v = initial_p; /* use t */
      } else if (0.0f <= initial_v) {
        /* use actual_t: already in initial_v. The original's t = actual_t
         * became a stack-slot self-move (0x1ac9e2: MOV EDX,[ebp+0xc];
         * MOV [ebp+0xc],EDX) — VC71 elides it from C we can write, a
         * 2-insn residual. */
      } else {
        /* Both roots negative: clamp coast time to 0 and skip the
         * "t >= 0" assert (0x1ac9d9: MOV [ebp+0xc],0; JMP 0x1aca18). */
        initial_v = 0.0f;
        goto velocity_constraint;
      }
    }

    if (!(initial_v >= 0.0f)) {
      display_assert("t >= 0", "c:\\halo\\SOURCE\\units\\units.c", 0x7fa, 1);
      system_exit(-1);
    }

  velocity_constraint:
    /* Apply maximum velocity constraint. initial_v = raw time t;
     * initial_p = velocity-clamped time (actual_t). vmax_t stays on the
     * FPU stack in the original (never stored), through clamp and pick. */
    if (max_v > 0.0f) {
      vmax_t = (bVar ? max_v : max_v + result->initial_v) / max_a;
      if (0.0f > vmax_t) {
        vmax_t = 0.0f;
      }
      if (initial_v > vmax_t) {
        initial_p = vmax_t;
      } else {
        initial_p = initial_v;
      }
    } else {
      initial_p = initial_v;
    }

    /* Fill plan fields */
    result->accel_a = -max_a;
    result->decel_a = max_a;
    if (bVar) {
      /* initial_v > 0: accel_t = actual_t + f1, decel_t = actual_t */
      result->accel_t = initial_p + f1;
      result->decel_t = initial_p;
    } else {
      /* initial_v <= 0: accel_t = actual_t, decel_t = actual_t + f1 */
      result->decel_t = initial_p + f1;
      result->accel_t = initial_p;
    }

    /* Check if we need a coasting phase (actual_t < t). f1 is dead
     * here; the original reuses its slot for the coasting velocity, and
     * the coast difference (t - actual_t) lives only on the FPU stack. */
    if (initial_p < initial_v) {
      /* Original reuses the -max_a still on the FPU stack from the
       * accel_a store; reading accel_a back expresses that reuse. */
      f1 = result->accel_a * result->accel_t +
              result->initial_v;
      /* sq holds (diff * coasting_vel) so the volatile coasting
       * velocity is loaded once, doubled on the FPU stack (FADD ST,ST0)
       * as in the original. */
      sq = initial_v - initial_p;
      sq = sq * f1;
      scratch =
        (sq + sq) - (initial_v - initial_p) * (initial_v - initial_p) * max_a;
      if (!(f1 < 0.0f)) {
        display_assert("coasting_vel < 0", "c:\\halo\\SOURCE\\units\\units.c",
                       0x850, 1);
        system_exit(-1);
      }
      result->coast_t = scratch / f1;
      if (!(result->coast_t >= 0.0f)) {
        display_assert("plan->coast_t >= 0", "c:\\halo\\SOURCE\\units\\units.c",
                       0x852, 1);
        system_exit(-1);
      }
      if (!(initial_p + result->coast_t >= initial_v)) {
        display_assert("plan->coast_t + actual_t >= t",
                       "c:\\halo\\SOURCE\\units\\units.c", 0x853, 1);
        system_exit(-1);
      }
      goto validate;
    }

    *(int *)&result->coast_t = 0;
  }

validate:
  if (*(int *)&result->accel_a == 0x7f7fffff) {
    display_assert("REAL_MAX != plan->accel_a",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x85c, 1);
    system_exit(-1);
  }
  if (*(int *)&result->accel_t == 0x7f7fffff) {
    display_assert("REAL_MAX != plan->accel_t",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x85d, 1);
    system_exit(-1);
  }
  if (*(int *)&result->coast_t == 0x7f7fffff) {
    display_assert("REAL_MAX != plan->coast_t",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x85e, 1);
    system_exit(-1);
  }
  if (*(int *)&result->decel_a == 0x7f7fffff) {
    display_assert("REAL_MAX != plan->decel_a",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x85f, 1);
    system_exit(-1);
  }
  if (*(int *)&result->decel_t == 0x7f7fffff) {
    display_assert("REAL_MAX != plan->decel_t",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x860, 1);
    system_exit(-1);
  }
}

/* unit_adjust_plan_overlap (0x1acb70)
 * Shortens the accelerate/decelerate phases of the plan that finishes first
 * so both axes finish together.
 * Register args: plan_a in ECX, plan_b in EAX. */
void unit_adjust_plan_overlap(void *plan_a_ptr, void *plan_b_ptr, int dummy,
                              float delta_time)
{
  unit_acceleration_plan_t *plan_a;
  unit_acceleration_plan_t *plan_b;
  unit_acceleration_plan_t *adjust_plan;
  float total_a;
  float total_b;
  float t_extension;
  float t_factor;
  float disc;
  float adj_amount;
  float min_t;
  float accel_t;
  float decel_t;
  float v_peak;

  plan_a = plan_a_ptr;
  plan_b = plan_b_ptr;

  if (plan_a->at_rest != 0 || plan_b->at_rest != 0) {
    return;
  }

  total_a = plan_a->decel_t + plan_a->coast_t +
            plan_a->accel_t;
  total_b = plan_b->decel_t + plan_b->coast_t +
            plan_b->accel_t;
  /* Original 0x1acb94/0x1acba0 stores and 0x1acbb3/0x1acbb6 reloads. */
  HALO_FLT_ROUNDTRIP(total_a);
  HALO_FLT_ROUNDTRIP(total_b);

  /* Determine which plan to adjust */
  if (plan_a->accel_t > 0.0f && total_b > total_a) {
    /* plan_a has priority — adjust plan_b using plan_a's extension */
    t_extension = total_b - total_a;
    adjust_plan = plan_a;
  } else if (plan_b->accel_t > 0.0f && total_a > total_b) {
    /* plan_b has priority — adjust plan_a using plan_b's extension */
    t_extension = total_a - total_b;
    adjust_plan = plan_b;
  } else {
    return;
  }

  if (adjust_plan == NULL) {
    return;
  }

  /* Assert t_extension > 0 */
  if (t_extension <= 0.0f) {
    display_assert("t_extension > 0", "c:\\halo\\SOURCE\\units\\units.c", 0x8ae,
                   true);
    system_exit(-1);
  }

  /* Solve quadratic for adjustment amount */
  t_factor = (t_extension + adjust_plan->coast_t) * delta_time;

  {
    float abs_vpeak;
    abs_vpeak =
      adjust_plan->accel_t * adjust_plan->accel_a +
      adjust_plan->initial_v;
    if (abs_vpeak < 0.0f) {
      abs_vpeak = -abs_vpeak;
    }
    disc = t_factor * t_factor - (-t_extension) * abs_vpeak * delta_time * 4.0f;
  }

  /* Assert discriminant >= 0 */
  if (disc < 0.0f) {
    display_assert("disc >= 0", "c:\\halo\\SOURCE\\units\\units.c", 0x8c4,
                   true);
    system_exit(-1);
  }

  /* Quadratic formula: (-b + sqrt(disc)) / (2*a) */
  adj_amount = (sqrtf(disc) - t_factor) / (delta_time + delta_time);

  /* Clamp to min(accel_t, decel_t) */
  if (adjust_plan->accel_t <= adjust_plan->decel_t) {
    min_t = adjust_plan->accel_t;
  } else {
    min_t = adjust_plan->decel_t;
  }

  if (adj_amount > min_t) {
    if (adjust_plan->accel_t <= adjust_plan->decel_t) {
      adj_amount = adjust_plan->accel_t;
    } else {
      adj_amount = adjust_plan->decel_t;
    }
  }

  /* Apply adjustment if positive */
  if (adj_amount <= 0.0f) {
    return;
  }

  accel_t = adjust_plan->accel_t - adj_amount;
  adjust_plan->accel_t = accel_t;

  v_peak =
    accel_t * adjust_plan->accel_a + adjust_plan->initial_v;

  decel_t = adjust_plan->decel_t - adj_amount;
  adjust_plan->decel_t = decel_t;

  adjust_plan->coast_t =
    ((v_peak + v_peak + adj_amount * adjust_plan->accel_a) *
     adj_amount) /
    v_peak;

  /* Assert both times remain non-negative */
  if (accel_t < 0.0f || decel_t < 0.0f) {
    display_assert("(adjust_plan->accel_t >= 0) && (adjust_plan->decel_t >= 0)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x8d8, true);
    system_exit(-1);
    return;
  }
}


/* unit_set_or_test_seat_and_weapon_label (0x1acd70)
 * Searches the unit's animation graph for a seat (mode) / weapon / weapon
 * type matching the labels (NULL matches anything, "unarmed" matches an
 * empty weapon-type label).  With apply_state set, the match becomes the
 * unit's animation seat/weapon/type and base seat.
 * Register arg: unit_handle in EAX. */
char unit_set_or_test_seat_and_weapon_label(int unit_handle, const char *seat_label,
                  const char *weapon_name, char apply_state)
{
  unit_data_t *unit;
  char *unit_definition;
  char *animation_graph;
  tag_block *seat_block;
  int seat_count;
  int16_t seat_index;
  animation_graph_unit_seat_t *unit_seat;
  int weapon_class_count;
  int16_t weapon_class_index;
  animation_graph_weapon_class_t *weapon_class;
  tag_block *weapon_block;
  int weapon_count;
  int16_t weapon_index;
  char *weapon_entry;
  char found;
  char has_multi_weapon;
  int16_t base_seat;
  int16_t si;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  animation_graph = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_definition));
  seat_block = &animation_graph_definition_unit_seats(animation_graph);
  seat_count = seat_block->count;
  found = 0;

  if (seat_count < 1)
    return 0;

  seat_index = 0;
  while (1) {
    unit_seat = tag_block_get_element(seat_block, (int)seat_index, 0x64);

    if (seat_label != 0 && crt_stricmp(seat_label, unit_seat->label) != 0) {
      goto next_seat;
    }

    weapon_class_count = unit_seat->weapon_classes.count;
    weapon_class_index = 0;
    if (weapon_class_count < 1)
      goto next_seat;

    while (1) {
      weapon_class = tag_block_get_element(&unit_seat->weapon_classes,
                                           (int)weapon_class_index, 0xbc);
      weapon_block = &weapon_class->weapon_types;
      weapon_count = weapon_block->count;
      weapon_index = 0;

      if (weapon_count < 1)
        goto next_weapon_class;

      while (1) {
        weapon_entry =
          (char *)tag_block_get_element(weapon_block, (int)weapon_index, 0x3c);

        if (weapon_name == 0)
          goto matched;
        if (csstrcmp(weapon_name, "unarmed") == 0 && *weapon_entry == '\0')
          goto matched;
        if (crt_stricmp(weapon_name, weapon_entry) == 0)
          goto matched;

        weapon_index++;
        if ((int)(int16_t)weapon_index >= weapon_count)
          goto next_weapon_class;
        continue;

      matched:
        if (apply_state == 0)
          goto found_match;

        {
          int num_key_types;
          int16_t *key_data;

          num_key_types = unit_seat->animations.count;
          key_data = unit_seat->animations.address;
          has_multi_weapon = 0;

          if ((num_key_types >= 3 && key_data[2] != -1) ||
              (num_key_types >= 4 && key_data[3] != -1) ||
              (num_key_types >= 5 && key_data[4] != -1)) {
            has_multi_weapon = 1;
          }
        }

        if (unit->animation_state != 0x1c)
          unit->animation_state = 0xff;

        unit->animation_seat_index = (uint8_t)seat_index;

        base_seat = NONE;
        for (si = 0; si < NUMBER_OF_UNIT_BASE_SEATS; si++) {
          if (crt_stricmp(seat_label,
                          *(const char **)(BASE_SEAT_LABELS_ADDRESS + (int)si * 4)) == 0) {
            base_seat = si;
            break;
          }
        }

        unit->animation_weapon_type_index = (uint8_t)weapon_index;
        unit->base_seat_index = (int8_t)base_seat;
        unit->animation_weapon_index = (uint8_t)weapon_class_index;

        if (has_multi_weapon) {
          unit->animation_flags |= 0x2;
        } else {
          unit->animation_flags &= ~0x2;
        }

      found_match:
        found = 1;
        goto next_weapon_class;
      }

    next_weapon_class:
      weapon_class_index++;
      if ((int)(int16_t)weapon_class_index >= unit_seat->weapon_classes.count)
        goto next_seat;
    }

  next_seat:
    seat_index++;
    if ((int)(int16_t)seat_index >= seat_block->count)
      break;

  }

  return found;
}

/* unit_adjust_projectile_ray (0x1acf90)
 * Optionally aims the ray along the unit's aiming vector and moves its
 * origin onto the camera line; returns the unit's speed along the ray. */
void unit_adjust_projectile_ray(int unit_handle, float *origin,
                                float *direction, float *velocity_out,
                                char adjust_origin, char use_unit_forward)
{
  unit_data_t *unit;
  real_point3d camera_position;
  float dot;
  real_vector3d unit_velocity;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if (use_unit_forward != '\0') {
    direction[0] = unit->aiming_vector.i;
    direction[1] = unit->aiming_vector.j;
    direction[2] = unit->aiming_vector.k;
  }

  if (adjust_origin != '\0') {
    unit_get_camera_position(unit_handle, &camera_position);
    dot = (origin[0] - camera_position.x) * direction[0] +
          (origin[1] - camera_position.y) * direction[1] +
          (origin[2] - camera_position.z) * direction[2];
    origin[0] = dot * direction[0] + camera_position.x;
    origin[1] = dot * direction[1] + camera_position.y;
    origin[2] = dot * direction[2] + camera_position.z;
  }

  object_get_velocities(unit_handle, &unit_velocity, 0);
  *velocity_out = unit_velocity.i * direction[0] +
                  unit_velocity.k * direction[2] +
                  unit_velocity.j * direction[1];
}

/* unit_render_debug (0x1ad060)
 * Debug drawing behind the flags at 0x5054f5-0x5054f7: aiming/facing
 * vectors, the local player's seat-entry points, and mouth aperture. */
void unit_render_debug(int unit_handle)
{
  unit_data_t *unit;
  unit_definition_t *unit_definition;
  real_point3d eye_pos;
  real_point3d head_pos;
  real_point3d seat_c;
  object_marker marker;
  int seat_idx;
  int player_unit_handle;
  player_data_t *local_player;
  char result;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition = unit_definition_get(unit->object.tag_index);

  if (DEBUG_OBJECTS_UNIT_VECTORS != '\0') {
    unit_get_camera_position(unit_handle, &eye_pos);
    object_get_world_position(unit_handle, &head_pos);
    head_pos.z = head_pos.z + 0.1f;
    render_debug_vector(1, &eye_pos, &unit->aiming_vector, 1.0f,
                 GLOBAL_REAL_ARGB_WHITE);
    render_debug_vector(1, &eye_pos, &unit->desired_aiming_vector,
                 0.5f, GLOBAL_REAL_ARGB_RED);
    render_debug_vector(1, &head_pos, &unit->object.forward, 1.0f,
                 GLOBAL_REAL_ARGB_WHITE);
    render_debug_vector(1, &head_pos, &unit->desired_facing_vector,
                 0.5f, GLOBAL_REAL_ARGB_RED);
  }

  if (DEBUG_OBJECTS_UNIT_SEATS != '\0') {
    {
      int player_idx;
      player_idx = local_player_get_player_index(RENDER_LOCAL_PLAYER_INDEX);
      local_player = datum_get(player_data, player_idx);
    }
    player_unit_handle = local_player->unit_handle;
    if (player_unit_handle != NONE) {
      seat_idx = 0;
      if (seat_idx < unit_definition->seats.count) {
        do {
          result = (char)unit_get_seat_enter_position(
            player_unit_handle, unit_handle, (int16_t)seat_idx, &eye_pos,
            &head_pos, &seat_c);
          if (result != '\0') {
            render_debug_point(1, &eye_pos, 0.25f, GLOBAL_REAL_ARGB_RED);
            render_debug_point(1, &head_pos, 0.25f, GLOBAL_REAL_ARGB_BLUE);
            render_debug_point(1, &seat_c, 0.25f, GLOBAL_REAL_ARGB_YELLOW);
          }
          seat_idx += 1;
        } while ((int)(short)seat_idx < unit_definition->seats.count);
      }
    }
  }

  if (DEBUG_OBJECTS_UNIT_MOUTH_APETURE != '\0') {
    object_get_marker_by_name(unit_handle, MARKER_NAME_HEAD, &marker, 1);
    head_pos = marker.matrix.position;
    render_debug_string_at_point(0, &head_pos,
                 csprintf(error_string_buffer, "%.2f",
                          (double)unit->mouth_aperture),
                 GLOBAL_REAL_ARGB_ORANGE_BITS);
  }
}

/* unit_try_animation_state (0x1acd70)
 * Searches the unit's animation graph for a seat (mode) / weapon / weapon
 * type matching the labels (NULL matches anything, "unarmed" matches an
 * empty weapon-type label).  With reset_flag set, the match becomes the
 * unit's animation seat/weapon/type and base seat.
 * Register arg: unit_handle in EAX. */
bool unit_try_animation_state(int unit_handle, int seat_label, int weapon_label,
                              int reset_flag)
{
  unit_data_t *unit;
  char *unit_definition;
  char *animation_graph;
  tag_block *seat_block;
  int seat_count;
  int seat_index;
  animation_graph_unit_seat_t *unit_seat;
  int weapon_class_count;
  int16_t weapon_class_index;
  animation_graph_weapon_class_t *weapon_class;
  tag_block *weapon_block;
  int weapon_count;
  int16_t weapon_index;
  char *weapon_name;
  bool found;
  bool has_multi_weapon;
  int16_t base_seat;
  int16_t si;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  animation_graph = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_definition));
  seat_block = &animation_graph_definition_unit_seats(animation_graph);
  seat_count = seat_block->count;
  found = false;

  if (seat_count < 1)
    return false;

  seat_index = 0;
  while (1) {
    unit_seat = tag_block_get_element(seat_block, seat_index, 0x64);

    if (seat_label != 0 &&
        crt_stricmp((const char *)seat_label, unit_seat->label) != 0) {
      goto next_seat;
    }

    weapon_class_count = unit_seat->weapon_classes.count;
    weapon_class_index = 0;
    if (weapon_class_count < 1)
      goto next_seat;

    while (1) {
      weapon_class = tag_block_get_element(&unit_seat->weapon_classes,
                                           (int)weapon_class_index, 0xbc);
      weapon_block = &weapon_class->weapon_types;
      weapon_count = weapon_block->count;
      weapon_index = 0;

      if (weapon_count < 1)
        goto next_weapon_class;

      while (1) {
        weapon_name =
          (char *)tag_block_get_element(weapon_block, (int)weapon_index, 0x3c);

        if (weapon_label == 0)
          goto matched;
        if (csstrcmp((const char *)weapon_label, "unarmed") == 0 &&
            *weapon_name == '\0')
          goto matched;
        if (crt_stricmp((const char *)weapon_label, weapon_name) == 0)
          goto matched;

        weapon_index++;
        if ((int)(int16_t)weapon_index >= weapon_count)
          goto next_weapon_class;
        continue;

      matched:
        if (reset_flag == 0)
          goto found_match;

        {
          int num_key_types = unit_seat->animations.count;
          int16_t *key_data = unit_seat->animations.address;
          has_multi_weapon = false;

          if ((num_key_types >= 3 && key_data[2] != -1) ||
              (num_key_types >= 4 && key_data[3] != -1) ||
              (num_key_types >= 5 && key_data[4] != -1)) {
            has_multi_weapon = true;
          }
        }

        if (unit->animation_state != 0x1c)
          unit->animation_state = 0xff;

        unit->animation_seat_index = (uint8_t)seat_index;

        base_seat = NONE;
        for (si = 0; si < NUMBER_OF_UNIT_BASE_SEATS; si++) {
          if (crt_stricmp((const char *)seat_label,
                          *(const char **)(BASE_SEAT_LABELS_ADDRESS + si * 4)) == 0) {
            base_seat = si;
            break;
          }
        }

        unit->animation_weapon_type_index = (uint8_t)weapon_index;
        unit->base_seat_index = (int8_t)base_seat;
        unit->animation_weapon_index = (uint8_t)weapon_class_index;

        if (has_multi_weapon) {
          unit->animation_flags |= 0x2;
        } else {
          unit->animation_flags &= ~0x2;
        }

      found_match:
        found = true;
        goto next_weapon_class;
      }

    next_weapon_class:
      weapon_class_index++;
      if ((int)(int16_t)weapon_class_index >= unit_seat->weapon_classes.count)
        goto next_seat;
    }

  next_seat:
    seat_index++;
    if ((int)(int16_t)seat_index >= seat_block->count)
      break;

  }

  return found;
}

/* unit_animation_set_state (0x1ad260)
 * Switches the unit to animation state anim_state: picks the state's
 * animation from the current weapon block (or the seat's overlay block),
 * starts it, and refreshes the aiming/looking idle screens.  Returns 0 when
 * a required animation (states 0x1e-0x21, 0x27, 0x29) is missing.  With the
 * developer flag at 0x5054fb set, missing biped animations are reported. */
char unit_animation_set_state(int unit_handle, int16_t anim_state)
{
  unit_data_t *unit;
  char *unit_definition;
  char *animation_graph;
  animation_graph_unit_seat_t *unit_seat;
  animation_graph_weapon_class_t *weapon_class;
  int16_t mode_anim_index;
  int16_t overlay_index;
  int16_t anim_index;
  char old_state_byte;
  int16_t old_state;
  char was_none;
  char did_change;
  int16_t weapon_idle_anim;
  int16_t transition_speed;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  animation_graph = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_definition));

  unit_seat = tag_block_get_element(
    &animation_graph_definition_unit_seats(animation_graph), (int)(signed char)unit->animation_seat_index, 100);
  weapon_class = tag_block_get_element(
    &unit_seat->weapon_classes, (int)(signed char)unit->animation_weapon_index, 0xbc);
  tag_block_get_element(&weapon_class->weapon_types,
                        (int)(signed char)unit->animation_weapon_type_index, 0x3c);

  old_state_byte = (char)unit->animation_state;
  old_state = (int16_t)old_state_byte;
  was_none = (old_state_byte == -1);
  did_change = 0;
#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_UNIT_STATE,
               ((unsigned int)(uint16_t)anim_state << 8) |
                 ((unsigned int)old_state_byte & 0xffu),
               unit_handle);
#endif
#line 7898

  if (!was_none && anim_state == old_state)
    goto resolve_weapon_idle;

  mode_anim_index = -1;
  overlay_index = -1;

  if (old_state_byte == 0x21) {
    unit_throw_grenade_release(unit_handle, 1);
  }

  if ((int)anim_state > 0x2b)
    goto set_not_found;

  /* Map anim_state to either a mode animation index or overlay index.
   *
   * NOTE: the original (0x1ad331) lowers this as a jump table whose per-case
   * bodies are "MOV <index>,imm; JMP <shared lookup>". VC71 /O2 instead lowers
   * this "switch selects a constant" pattern as a value-lookup array (compact
   * data table) regardless of whether the cases use break OR goto to a shared
   * label (confirmed 2026-07-08: rewriting all 29 mode cases as `goto
   * mode_path` produced byte-identical 160-insn codegen). The original's
   * jump-table shape came from different build flags, not source structure; it
   * is unreachable from C here. This is a codegen selection difference, not a
   * semantic one: the case->index mapping below is byte-verified against the
   * jump-table bodies at 0x1ad338-0x1ad4a2. */
  switch ((int)anim_state) {
  case 0:
    mode_anim_index = 0;
    break;
  case 1:
    mode_anim_index = 1;
    break;
  case 2:
    mode_anim_index = 2;
    break;
  case 3:
    mode_anim_index = 3;
    break;
  case 4:
    mode_anim_index = 8;
    break;
  case 5:
    mode_anim_index = 9;
    break;
  case 6:
    mode_anim_index = 10;
    break;
  case 7:
    mode_anim_index = 11;
    break;
  case 8:
    mode_anim_index = 0x23;
    break;
  case 9:
    mode_anim_index = 0x24;
    break;
  case 10:
    mode_anim_index = 0x25;
    break;
  case 11:
    mode_anim_index = 0x26;
    break;
  case 12:
    mode_anim_index = 12;
    break;
  case 13:
    mode_anim_index = 13;
    break;
  case 14:
    mode_anim_index = 14;
    break;
  case 15:
    mode_anim_index = 15;
    break;
  case 0x14:
    mode_anim_index = 0x10;
    break;
  case 0x15:
    mode_anim_index = 0x11;
    break;
  case 0x16:
    mode_anim_index = 0x12;
    break;
  case 0x1e:
    mode_anim_index = 0x27;
    break;
  case 0x1f:
    mode_anim_index = 0x2a;
    break;
  case 0x20:
    mode_anim_index = 0x2e;
    break;
  case 0x21:
    mode_anim_index = 0x14;
    break;
  case 0x22:
    mode_anim_index = 0x2c;
    break;
  case 0x23:
    mode_anim_index = 0x2d;
    break;
  case 0x24:
    mode_anim_index = 0x2f;
    break;
  case 0x27:
    mode_anim_index = 0x30;
    break;
  case 0x28:
    mode_anim_index = 0x31;
    break;
  case 0x29:
    mode_anim_index = 0x32;
    break;
  case 0x10:
    overlay_index = 0x17;
    goto overlay_path;
  case 0x11:
    overlay_index = 0x18;
    goto overlay_path;
  case 0x12:
    overlay_index = 0x19;
    goto overlay_path;
  case 0x13:
    overlay_index = 0x1a;
    goto overlay_path;
  case 0x18:
    overlay_index = 0;
    goto overlay_path;
  case 0x19:
    overlay_index = 1;
    goto overlay_path;
  case 0x25:
    overlay_index = 0x1b;
    goto overlay_path;
  case 0x26:
    overlay_index = 0x1c;
    goto overlay_path;
  case 0x2b:
    overlay_index = 0x1d;
    goto overlay_path;
  default:
    goto set_not_found;
  }

  /* Mode animation lookup path */
  if ((int)mode_anim_index < weapon_class->animations.count) {
    anim_index =
      ((int16_t *)weapon_class->animations.address)[(int)mode_anim_index];
  } else {
    goto set_not_found;
  }
  goto check_found;

overlay_path:
  /* Overlay animation lookup path */
  if ((int)overlay_index >= unit_seat->animations.count)
    goto set_not_found;
  anim_index =
    ((int16_t *)unit_seat->animations.address)[(int)overlay_index];
  goto check_found;

set_not_found:
  anim_index = -1;

check_found:
  /* Check if animation was found */
  if (DEBUG_UNIT_ANIMATIONS != 0 && unit->object.type == _object_type_biped) {
    /* Developer mode: warn about missing animations */
    if (anim_index == -1) {
      const char *mn;
      const char *on;
      if (mode_anim_index != -1) {
        mn = animation_list_get_string(WEAPON_ANIMATION_NAMES, mode_anim_index);
      } else {
        mn = EMPTY_STRING;
      }
      if (overlay_index != -1) {
        on = (const char *)animation_list_get_string(UNIT_SEAT_ANIMATION_NAMES, overlay_index);
      } else {
        on = weapon_class->label;
      }
      console_warning("MISSING: %s \'%s %s %s\'",
                      tag_name_strip_path(object_definition_animation_graph_name(unit_definition)),
                      unit_seat->label, on, mn);
      goto check_early_return;
    }
  } else {
    if (anim_index == -1)
      goto check_early_return;
  }
  goto apply_animation;

check_early_return:
  /* For certain states, return 0 if animation not found */
  switch ((int)anim_state) {
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x27:
  case 0x29:
    return 0;
  }

apply_animation:
  anim_index =
    (int16_t)model_animation_choose_random(1, object_definition_animation_graph_index(unit_definition), anim_index);
  unit_set_animation(unit_handle, object_definition_animation_graph_index(unit_definition), anim_index);

  old_state = (int16_t)(signed char)unit->animation_state;

  /* Determine transition speed */
  transition_speed = 6;
  if ((anim_state == 0 || anim_state == 2 || anim_state == 3) &&
      (old_state == 0 || old_state == 2 || old_state == 3)) {
    transition_speed = 1;
  }
  if (anim_state == 0x16 || anim_state == 0x15) {
    transition_speed = 2;
  }

  did_change = 1;

resolve_weapon_idle: {
  int16_t new_weapon_idle;

  new_weapon_idle = unit_animation_state_get_aiming_screen_index(anim_state);

  if (was_none || new_weapon_idle != unit_animation_state_get_aiming_screen_index(old_state)) {
    /* Resolve weapon idle animation */
    if (new_weapon_idle >= 0 &&
        (int)new_weapon_idle < weapon_class->animations.count) {
      weapon_idle_anim =
        ((int16_t *)weapon_class->animations.address)[(int)new_weapon_idle];
    } else {
      weapon_idle_anim = -1;
    }

    weapon_idle_anim = (int16_t)model_animation_choose_random(
      1, object_definition_animation_graph_index(unit_definition), weapon_idle_anim);
    unit->aiming_screen_index = weapon_idle_anim;

    if (DEBUG_UNIT_ANIMATIONS != 0 && unit->object.type == _object_type_biped &&
        weapon_idle_anim == -1 && new_weapon_idle != -1) {
      const char *wn = animation_list_get_string(WEAPON_ANIMATION_NAMES, (int16_t)new_weapon_idle);
      console_warning("MISSING: %s \'%s %s %s\'",
                      tag_name_strip_path(object_definition_animation_graph_name(unit_definition)),
                      unit_seat->label, weapon_class->label, wn);
    }

    /* XBE 0x1ad66e: force speed 6 on weapon-idle class change, before was_none. */
    transition_speed = 6;

    if (was_none) {
      /* Resolve base weapon overlay (index 9) */
      if (unit_seat->animations.count > 9) {
        weapon_idle_anim = ((int16_t *)unit_seat->animations.address)[9];
      } else {
        weapon_idle_anim = -1;
      }
      weapon_idle_anim = (int16_t)model_animation_choose_random(
        1, object_definition_animation_graph_index(unit_definition), weapon_idle_anim);
      unit->looking_screen_index = weapon_idle_anim;

      if (DEBUG_UNIT_ANIMATIONS != 0 && unit->object.type == _object_type_biped &&
          weapon_idle_anim == -1) {
        const char *oln = animation_list_get_string(UNIT_SEAT_ANIMATION_NAMES, 9);
        console_warning("MISSING: %s \'%s %s\'",
                        tag_name_strip_path(object_definition_animation_graph_name(unit_definition)),
                        unit_seat->label, oln);

      }
    }
  } else if (did_change == 0) {
    goto skip_transition;
  }

  object_set_region_count(unit_handle, transition_speed);
}

skip_transition:
  unit->animation_state = (char)anim_state;
  return 1;
}

/* unit_abort_animation (0x1ad7e0)
 * Aborts the current animation by calling unit_animation_set_state with state 0. */
void unit_abort_animation(int unit_handle)
{
  unit_animation_set_state(unit_handle, 0);
}

/* unit_find_best_enter_seat (0x1ad800)
 * Picks the closest usable seat of target_unit: 2 = enter an empty seat,
 * 1 = an AI occupant will make way, 0 = none. */
uint16_t unit_find_best_enter_seat(int unit_handle, int target_unit_handle,
                                   int16_t *out_seat_index)
{
  unit_data_t *unit;
  unit_data_t *target_unit;
  unit_definition_t *target_definition;
  int seat_count;
  tag_block *seat_block;
  int seat_index;
  int best_seat;
  uint16_t best_state;
  float best_distance;
  volatile float distance;
  volatile float distance2;
  float pos_a[3];
  float pos_b[3];
  uint8_t found_flag;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  target_unit = object_get_and_verify_type(target_unit_handle, _object_mask_unit);
  target_definition = unit_definition_get(target_unit->object.tag_index);

  best_state = 0;
  best_seat = NONE;
  found_flag = 0;

  if ((target_unit->object.damage_flags & 4) == 0 &&
      (target_unit->flags & 0x10000) == 0) {
    seat_block = &target_definition->seats;
    seat_count = seat_block->count;
    best_distance = 3.4028235e+38f;

    for (seat_index = 0; seat_index < seat_count; seat_index++) {
      unit_seat_t *seat =
        tag_block_get_element(seat_block, seat_index, sizeof(unit_seat_t));

      if (!unit_get_seat_enter_position(unit_handle, target_unit_handle,
                                        (int16_t)seat_index, (real_point3d *)pos_a,
                                        (real_point3d *)pos_b, NULL))
        continue;

      distance = sqrtf((unit->object.bounding_sphere_center.x - pos_b[0]) *
                         (unit->object.bounding_sphere_center.x - pos_b[0]) +
                       (unit->object.bounding_sphere_center.y - pos_b[1]) *
                         (unit->object.bounding_sphere_center.y - pos_b[1]) +
                       (unit->object.bounding_sphere_center.z - pos_b[2]) *
                         (unit->object.bounding_sphere_center.z - pos_b[2]));

      distance2 = sqrtf((unit->object.bounding_sphere_center.x - pos_a[0]) *
                          (unit->object.bounding_sphere_center.x - pos_a[0]) +
                        (unit->object.bounding_sphere_center.y - pos_a[1]) *
                          (unit->object.bounding_sphere_center.y - pos_a[1]) +
                        (unit->object.bounding_sphere_center.z - pos_a[2]) *
                          (unit->object.bounding_sphere_center.z - pos_a[2]));

      if (distance2 < distance)
        distance = distance2;

      if (distance >= REAL_ONE_POOL)
        continue;

      /* +0x04 is the seat's animation label. */
      if (((seat->flags & 0x200) == 0 ||
           target_unit->driver_object_index.value != NONE) &&
          *(char *)seat->pad_04 != 0 &&
          unit_try_animation_state(unit_handle, (int)seat->pad_04, 0, 0)) {
        int blocking_unit = NONE;
        uint16_t seat_state = 0;

        if (!unit_find_nearby_seat(unit_handle, target_unit_handle,
                                   (int16_t)seat_index, &blocking_unit)) {
          if (blocking_unit != NONE) {
            unit_data_t *blocking =
              object_get_and_verify_type(blocking_unit, _object_mask_unit);
            if (blocking->actor_index.value != NONE &&
                ai_handle_unit_approach(blocking->actor_index.value,
                                        unit_handle, 0)) {
              seat_state = 1;
            }
          }
        } else {
          seat_state = 2;
        }

        if (seat_state != 0) {
          float threshold = REAL_ONE_POOL;
          if (found_flag &&
              ((seat->flags >> _unit_seat_driver_bit) & 1) == 0)
            threshold = REAL_1_5_POOL;

          if (best_seat == NONE || best_state < seat_state ||
              distance * threshold < best_distance) {
            best_distance = distance;
            best_state = seat_state;
            best_seat = seat_index;
            found_flag = ((seat->flags >> _unit_seat_driver_bit) & 1);
          }
        }
      }
    }
  }

  if (out_seat_index == NULL) {
    display_assert("parent_seat_index != NULL",
                   "c:\\halo\\SOURCE\\units\\units.c", 0xff8, 1);
    system_exit(-1);
  }

  *out_seat_index = (int16_t)best_seat;
  return best_state;
}

/* unit_clip_to_aiming_bounds (0x1ada90)
 * Clamps a world-space aiming (flag set) or looking vector to the unit's
 * yaw/pitch screen, measured in the unit's own frame.  Returns 1 if the
 * vector was changed. */
char unit_clip_to_aiming_bounds(int unit_handle, real_vector3d *vector, char flag)
{
  /* Original stack: SUB ESP,0x4c (76 bytes).  The unit's orientation is
   * written straight into the matrix rows, not copied. */
  real_matrix4x3 matrix;
  real_vector3d relative_vector;
  real_euler_angles2d angles;
  char clamped;
  unit_data_t *unit;
  char enabled;
  float *bounds;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  clamped = 0;

  /* bounds: yaw min/max, pitch min/max */
  if (flag) {
    enabled = unit->aiming_with_euler_screen;
    bounds = &unit->unk_616;
  } else {
    enabled = unit->looking_with_euler_screen;
    bounds = &unit->unk_632;
  }

  if (!(char)valid_real_normal3d(vector)) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_normal3d(%f, %f, %f)",
                            "vector", (double)vector->i, (double)vector->j,
                            (double)vector->k),
                   "c:\\halo\\SOURCE\\units\\units.c", 0x15e8, 1);
    system_exit(-1);
  }

  if (enabled) {
    matrix.scale = 1.0f;
    object_get_orientation(unit_handle, &matrix.forward,
                           &matrix.up);

    /* left = up x forward */
    matrix.left.i = matrix.up.j * matrix.forward.k - matrix.forward.j * matrix.up.k;
    matrix.left.j = matrix.forward.i * matrix.up.k - matrix.up.i * matrix.forward.k;
    matrix.left.k = matrix.up.i * matrix.forward.j - matrix.forward.i * matrix.up.j;

    matrix.position = *global_origin3d_ptr;

    real_matrix4x3_transform_point(&matrix, vector, &relative_vector);

    if (!(char)real_vector3d_valid(&relative_vector)) {
      display_assert(csprintf(error_string_buffer,
                              "%s: assert_valid_real_vector2d(%f, %f, %f)",
                              "&relative_vector", (double)relative_vector.i,
                              (double)relative_vector.j,
                              (double)relative_vector.k),
                     "c:\\halo\\SOURCE\\units\\units.c", 0x15fa, 1);
      system_exit(-1);
    }

    vector_to_angles(&angles, &relative_vector);

    if (!valid_real(angles.pitch)) {
      display_assert(csprintf(error_string_buffer,
                              "%s: assert_valid_real(0x%08X %f)",
                              "relative_aiming_angles.pitch",
                              *(uint32_t *)&angles.pitch, (double)angles.pitch),
                     "c:\\halo\\SOURCE\\units\\units.c", 0x15fd, 1);
      system_exit(-1);
    }

    if (!valid_real(angles.yaw)) {
      display_assert(csprintf(error_string_buffer,
                              "%s: assert_valid_real(0x%08X %f)",
                              "relative_aiming_angles.yaw",
                              *(uint32_t *)&angles.yaw, (double)angles.yaw),
                     "c:\\halo\\SOURCE\\units\\units.c", 0x15fe, 1);
      system_exit(-1);
    }

    /* clamp yaw to bounds */
    if (angles.yaw < bounds[0]) {
      angles.yaw = bounds[0];
      clamped = 1;
    } else if (angles.yaw > bounds[1]) {
      angles.yaw = bounds[1];
      clamped = 1;
    }

    /* clamp pitch to bounds */
    if (angles.pitch < bounds[2]) {
      clamped = 1;
      angles.pitch = bounds[2];
    } else if (angles.pitch > bounds[3]) {
      clamped = 1;
      angles.pitch = bounds[3];
    }

    if (clamped) {
      /* rotate the clamped angles back into the world frame */
      if (!valid_real(angles.pitch)) {
        display_assert(csprintf(error_string_buffer,
                                "%s: assert_valid_real(0x%08X %f)",
                                "relative_aiming_angles.pitch",
                                *(uint32_t *)&angles.pitch, (double)angles.pitch),
                       "c:\\halo\\SOURCE\\units\\units.c", 0x161d, 1);
        system_exit(-1);
      }

      if (!valid_real(angles.yaw)) {
        display_assert(csprintf(error_string_buffer,
                                "%s: assert_valid_real(0x%08X %f)",
                                "relative_aiming_angles.yaw",
                                *(uint32_t *)&angles.yaw, (double)angles.yaw),
                       "c:\\halo\\SOURCE\\units\\units.c", 0x161e, 1);
        system_exit(-1);
      }

      angles_to_vector(&relative_vector, &angles);

      if (!(char)real_vector3d_valid(&relative_vector)) {
        display_assert(csprintf(error_string_buffer,
                                "%s: assert_valid_real_vector2d(%f, %f, %f)",
                                "&relative_vector", (double)relative_vector.i,
                                (double)relative_vector.j,
                                (double)relative_vector.k),
                       "c:\\halo\\SOURCE\\units\\units.c", 0x1621, 1);
        system_exit(-1);
      }

      matrix_transform_vector((float *)&matrix, &relative_vector, vector);

      if (!(char)real_vector3d_valid(vector)) {
        display_assert(csprintf(error_string_buffer,
                                "%s: assert_valid_real_vector2d(%f, %f, %f)",
                                "vector", (double)vector->i, (double)vector->j,
                                (double)vector->k),
                       "c:\\halo\\SOURCE\\units\\units.c", 0x1623, 1);
        system_exit(-1);
      }
    }
  }

  return clamped;
}

/* unit_inventory_get_weapon (0x1adeb0) */
int unit_inventory_get_weapon(int unit_handle, int16_t weapon_index)
{
  unit_data_t *unit;
  int result;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  result = NONE;
  if (weapon_index != NONE) {
    if (weapon_index < 0 || weapon_index >= MAXIMUM_WEAPONS_PER_UNIT) {
      display_assert("index>=0 && index<MAXIMUM_WEAPONS_PER_UNIT",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x20ac, 1);
      system_exit(-1);
    }
    result = unit->weapon_object_indices[weapon_index].value;
  }
  return result;
}

/* unit_set_actively_controlled (0x1adf10)
 * Units with an actor, swarm actor or player are always active.  Updates
 * unit flags bits 0 and 6, re-stows the weapons and refreshes the seat
 * occupants. */
void unit_set_actively_controlled(int unit_handle, char actively_controlled)
{
  unit_data_t *unit;
  uint32_t flags;
  datum_handle_t *weapon_slots;
  int i;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if (unit->actor_index.value != NONE || unit->swarm_actor_index.value != NONE ||
      unit->player_index.value != NONE) {
    actively_controlled = 1;
  }

  if ((unit->object.damage_flags & 4) == 0 && actively_controlled != 0) {
    flags = unit->flags;
    unit->flags = flags | 1;
    flags = flags | 0x41;
  } else {
    flags = unit->flags;
    unit->flags = flags & 0xfffffffe;
    flags = flags & 0xffffffbe;
  }
  unit->flags = flags;

  weapon_slots = unit->weapon_object_indices;
  for (i = MAXIMUM_WEAPONS_PER_UNIT; i != 0; i--) {
    if (weapon_slots->value != NONE) {
      item_in_unit_inventory(weapon_slots->value, unit_handle);
    }
    weapon_slots++;
  }

  unit_update_seat_occupancy(unit_handle);
}

/* vehicle_scripting_find_available_seats (0x1adfc0)
 * Collects up to max_seats empty seats whose label contains the substring
 * and that pass seat_desire_type: 0 not driver, 1 gunner, 2 passenger
 * (neither), 3 driver, NONE/other any. */
int16_t vehicle_scripting_find_available_seats(int unit_handle,
                                               int seat_substring_addr,
                                               int16_t seat_desire_type,
                                               int16_t *seat_indices,
                                               int16_t max_seats)
{
  const char *seat_substring;
  unit_data_t *unit;
  unit_definition_t *unit_definition;
  char match_all;
  short found_count;
  int loop_idx;
  unit_seat_t *seat;
  char seat_name_buf[256];
  uint8_t bit_val;
  char seat_filled;

  seat_substring = (const char *)seat_substring_addr;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition = unit_definition_get(unit->object.tag_index);

  if (seat_substring == NULL) {
    display_assert("seat_substring_name", "c:\\halo\\SOURCE\\units\\units.c",
                   0x178f, 1);
    system_exit(-1);
  }
  if (seat_desire_type != -1 &&
      (seat_desire_type < 0 || seat_desire_type >= 5)) {
    display_assert("(seat_desire_type == NONE) || "
                   "((seat_desire_type >= 0) && "
                   "(seat_desire_type < NUMBER_OF_VEHICLE_SEAT_DESIRE_TYPES))",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1790, 1);
    system_exit(-1);
  }

  if (seat_substring != NULL && csstrlen(seat_substring) != 0) {
    match_all = 0;
  } else {
    match_all = 1;
  }

  found_count = 0;
  loop_idx = 0;
  while ((short)loop_idx < unit_definition->seats.count) {
    seat = tag_block_get_element(&unit_definition->seats, loop_idx,
                                 sizeof(unit_seat_t));

    if (found_count >= max_seats) {
      return found_count;
    }

    csstrcpy(seat_name_buf, (const char *)seat->pad_04);
    csstr_tolower(seat_name_buf);

    if (!match_all && crt_strstr(seat_name_buf, seat_substring) == NULL) {
      goto next_seat;
    }

    switch ((int)seat_desire_type) {
    case 0:
      bit_val = ~(uint8_t)(seat->flags >> _unit_seat_driver_bit);
      break;
    case 1:
      bit_val = (uint8_t)(seat->flags >> _unit_seat_gunner_bit);
      break;
    case 2:
      if ((seat->flags & FLAG(_unit_seat_driver_bit)) != 0 ||
          (seat->flags & FLAG(_unit_seat_gunner_bit)) != 0)
        goto next_seat;
      goto accept_seat;
    case 3:
      bit_val = (uint8_t)(seat->flags >> _unit_seat_driver_bit);
      break;
    default:
      goto accept_seat;
    }
    if ((bit_val & 1) == 0)
      goto next_seat;

  accept_seat:
    seat_filled = unit_seat_filled(unit_handle, (int16_t)loop_idx);
    if (seat_filled == '\0') {
      seat_indices[found_count] = (int16_t)loop_idx;
      found_count = found_count + 1;
    }

  next_seat:
    loop_idx += 1;
  }
  return found_count;
}

/* Return first weapon slot requiring a ready transition (0x1ac350). */
int16_t unit_find_weapon_to_ready(int unit_handle)
{
  unit_data_t *unit;
  int weapon_handle;
  short index;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  index = 0;
  do {
    weapon_handle = unit->weapon_object_indices[index].value;
    if (weapon_handle != NONE) {
      if ((char)weapon_must_be_readied(weapon_handle) != 0) {
        return index;
      }
    }
    index++;
  } while (index < MAXIMUM_WEAPONS_PER_UNIT);

  return NONE;
}

/* unit_open (0x1ae160)
 * Opens a unit by transitioning to animation state 0x25. */
void unit_open(int unit_handle)
{
  if (unit_handle != -1) {
    unit_animation_set_state(unit_handle, 0x25);
  }
}

/* unit_close (0x1ae180)
 * Closes a unit by transitioning to animation state 0x26. */
void unit_close(int unit_handle)
{
  if (unit_handle != -1) {
    unit_animation_set_state(unit_handle, 0x26);
  }
}

/* unit_current_weapon_is_busy (0x1ae1a0) */
bool unit_current_weapon_is_busy(int unit_handle)
{
  unit_data_t *unit;
  int weapon_handle;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  weapon_handle = unit_inventory_get_weapon(unit_handle,
                                            unit->current_weapon_index);
  if (weapon_handle != NONE)
    return weapon_overcharged(weapon_handle);
  return false;
}

/* unit_set_seat (0x1ae1e0)
 * Attempts to set the unit's seat via animation lookup. */
char unit_set_seat(int unit_handle, int seat_name)
{
  char result = false;

  if (unit_set_or_test_seat_and_weapon_label(unit_handle, (const char *)seat_name, 0, 1)) {
    result = true;
  }
  return result;
}

/* units_set_desired_flashlight_state (0x1ae210)
 * Iterates child objects and sets flashlight state for each unit. */
void units_set_desired_flashlight_state(int object_list, char desired)
{
  int iter_state;
  int child;
  unit_data_t *unit;
  uint32_t flags;

  child = object_list_get_first(object_list, &iter_state);
  while (child != NONE) {
    unit = object_try_and_get_and_verify_type(child, _object_mask_unit);
    if (unit != NULL && child != NONE) {
      unit = object_get_and_verify_type(child, _object_mask_unit);
      if (desired) {
        flags = unit->flags | 0x10000000;
      } else {
        flags = unit->flags | 0x20000000;
      }
      unit->flags = flags;
    }
    child = object_list_get_next(object_list, &iter_state);
  }
}

/* unit_get_seat_label (0x1ae290)
 * Returns the label of the parent seat the unit occupies, else the name of
 * its base seat from the table at 0x32e484.
 * Register arg: unit_handle passed in EAX. */
int unit_get_seat_label(int unit_handle)
{
  unit_data_t *unit;
  int parent_handle;
  int16_t seat_index;
  unit_data_t *parent;
  unit_definition_t *parent_definition;
  unit_seat_t *seat;
  int16_t base_seat;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  parent_handle = unit->object.parent_object_index.value;
  if (parent_handle != NONE && unit->parent_seat_index != NONE) {
    parent = object_get_and_verify_type(parent_handle, _object_mask_unit);
    parent_definition = unit_definition_get(parent->object.tag_index);
    seat_index = unit->parent_seat_index;
    seat = tag_block_get_element(&parent_definition->seats, (int)seat_index,
                                 sizeof(unit_seat_t));
    return (int)seat->pad_04;
  }

  base_seat = (int16_t)(int8_t)unit->base_seat_index;
  if (base_seat < 0 || base_seat >= NUMBER_OF_UNIT_BASE_SEATS) {
    display_assert(
      "base_seat_index>=0 && base_seat_index<NUMBER_OF_UNIT_BASE_SEATS",
      "c:\\halo\\SOURCE\\units\\units.c", 0x200f, 1);
    system_exit(-1);
  }
  return BASE_SEAT_LABELS_AS_INT[base_seat];
}

/* unit_clear_seat_equipment (0x1ae330) */
void unit_clear_seat_equipment(int unit_handle)
{
  unit_data_t *unit;
  int equipment_handle;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  equipment_handle = unit->equipment_object_index.value;
  if (equipment_handle != NONE) {
    unit_detach_weapon(unit_handle, equipment_handle);
    unit->equipment_object_index.value = NONE;
  }
}

/* unit_can_enter_seat (0x1ae370)
 * True when the unit has an animation for holding the weapon in its seat
 * and the game engine allows the pick-up. */
bool unit_can_enter_seat(int unit_handle, int seat_object_handle)
{
  int seat_label;
  int weapon_label;
  char can_enter;

  object_get_and_verify_type(unit_handle, _object_mask_unit);
  object_get_and_verify_type(seat_object_handle, 4);

  seat_label = unit_get_seat_label(unit_handle);
  weapon_label = (int)weapon_get_label(seat_object_handle);
  can_enter =
    (char)unit_try_animation_state(unit_handle, seat_label, weapon_label, 0);

  if (can_enter != 0) {
    can_enter =
      (char)game_engine_allow_weapon_pick_up(unit_handle, seat_object_handle);
  }
  return can_enter != 0;
}

/* unit_should_swap_weapon (0x1ae3c0)
 * False when the unit already carries this weapon type, unless it is the
 * current weapon and the new one has more of the +0x1f0 value. */
bool unit_should_swap_weapon(int unit_handle, int weapon_handle)
{
  unit_data_t *unit;
  weapon_data_t *weapon;
  int weapon_tag;
  int current_weapon;
  int inventory_index;
  int inventory_weapon_index;
  weapon_data_t *inventory_weapon;
  bool approved;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  weapon = object_get_and_verify_type(weapon_handle, 4);
  weapon_tag = weapon->item.object.tag_index;

  tag_get(TAG_GROUP_WEAP, weapon_tag);

  approved = true;
  /* 2276 emits an extra unit_get (0x1ae3fa) before the call at
   * 0x1adeb0. */
  current_weapon = unit_inventory_get_weapon(unit_handle,
    (int16_t)((unit_data_t *)object_get_and_verify_type(
      unit_handle, _object_mask_unit))->current_weapon_index);
  if (current_weapon == NONE)
    return false;

  for (inventory_index = 0; inventory_index < MAXIMUM_WEAPONS_PER_UNIT;
       inventory_index++) {
    inventory_weapon_index = unit->weapon_object_indices[inventory_index].value;
    if (inventory_weapon_index != NONE) {
      inventory_weapon =
        object_get_and_verify_type(inventory_weapon_index, 4);
      if (weapon_tag == inventory_weapon->item.object.tag_index &&
          (inventory_index != unit->current_weapon_index ||
           !(inventory_weapon->unk_496 > 0.f) ||
           !(weapon->unk_496 < inventory_weapon->unk_496))) {
        approved = false;
      }
    }
  }

  return approved;
}

/* unit_weapon_next_index (0x1ae490) — unit_next_weapon_index
 * Scans weapon slots circularly for the next valid weapon.
 * Register arg: unit_handle in EBX. */
int16_t unit_weapon_next_index(int unit_handle, int16_t current_index, int16_t direction)
{
  unit_data_t *unit;
  int iter_index;
  int weapon_handle;
  int seat_label;
  char *weapon_label;
  char can_use;
  char usable;
  char readied;
  int16_t best_index;
  volatile int16_t current;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  best_index = NONE;

  if (current_index == (int16_t)NONE) {
    current_index = 0;
  } else if (current_index < 0 || current_index >= MAXIMUM_WEAPONS_PER_UNIT) {
    display_assert("current_index>=0 && current_index<MAXIMUM_WEAPONS_PER_UNIT",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1e40, 1);
    system_exit(-1);
  }

  current = current_index;

  do {
    iter_index = (int)(int16_t)current;
    weapon_handle = unit->weapon_object_indices[iter_index].value;

    if (weapon_handle != NONE) {
      object_get_and_verify_type(unit_handle, _object_mask_unit);
      object_get_and_verify_type(weapon_handle, 4);

      seat_label = unit_get_seat_label(unit_handle);
      weapon_label = (char *)weapon_get_label(weapon_handle);
      can_use = unit_set_or_test_seat_and_weapon_label(unit_handle, (const char *)seat_label,
                             (const char *)weapon_label, 0);

      if (can_use != 0) {
        usable =
          (char)game_engine_allow_weapon_pick_up(unit_handle, weapon_handle);
        if (usable != 0) {
          if (direction != 0 || best_index == (int16_t)NONE ||
              unit->weapon_last_used_at_game_time[best_index].value <
                unit->weapon_last_used_at_game_time[iter_index].value) {
            best_index = current;
          }

          readied = (char)weapon_must_be_readied(
            unit->weapon_object_indices[iter_index].value);
          if (readied != 0 || current_index != current)
            break;
        }
      }
    }

    if (direction < 0) {
      if ((int16_t)current == 0)
        current = 3;
      else
        current = iter_index - 1;
    } else {
      if ((int16_t)current == 3)
        current = 0;
      else
        current = iter_index + 1;
    }
  } while ((int16_t)current != current_index);

  return best_index;
}

/* unit_next_weapon_index (0x1ae490)
 * Scans the weapon slots circularly from weapon_index (NONE = slot 0); a zero
 * direction picks the least recently used usable weapon. Returns NONE if none. */
int16_t unit_next_weapon_index(int unit_handle, int16_t weapon_index,
                               int16_t direction)
{
  unit_data_t *unit;
  int current_index;
  int best_index;
  int iter_index;
  int weapon_handle;
  int anim_tag;
  int weapon_tag;
  char can_use;
  char usable;
  char must_be_readied;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  best_index = -1;

  if (weapon_index == (int16_t)-1) {
    weapon_index = 0;
  } else if (weapon_index < 0 || weapon_index >= MAXIMUM_WEAPONS_PER_UNIT) {
    display_assert("current_index>=0 && current_index<MAXIMUM_WEAPONS_PER_UNIT",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1e40, 1);
    system_exit(-1);
  }

  current_index = weapon_index;

  do {
    iter_index = (int)(int16_t)current_index;
    weapon_handle = unit->weapon_object_indices[iter_index].value;

    if (weapon_handle != -1) {
      object_get_and_verify_type(unit_handle, _object_mask_unit);
      object_get_and_verify_type(weapon_handle, 4);

      anim_tag = unit_get_seat_label(unit_handle);
      weapon_tag = (int)weapon_get_label(weapon_handle);
      can_use =
        (char)unit_try_animation_state(unit_handle, anim_tag, weapon_tag, 0);

      if (can_use != 0) {
        usable =
          (char)game_engine_allow_weapon_pick_up(unit_handle, weapon_handle);
        if (usable != 0) {
          /* direction != 0: pick first valid; direction == 0: pick lowest
           * priority */
          if (direction != 0) {
            best_index = current_index;
          } else {
            if ((int16_t)best_index == (int16_t)-1 ||
                unit->weapon_last_used_at_game_time[(int)(int16_t)best_index].value <
                  unit->weapon_last_used_at_game_time[iter_index].value) {
              best_index = current_index;
            }
          }

          must_be_readied =
            (char)weapon_must_be_readied(unit->weapon_object_indices[iter_index].value);
          if (must_be_readied != 0)
            return (int16_t)best_index;

          if ((int16_t)current_index != weapon_index)
            return (int16_t)best_index;
        }
      }
    }

    /* Advance to next slot, wrapping around */
    if (direction < 0) {
      if ((int16_t)current_index == 0)
        current_index = 3;
      else
        current_index = iter_index - 1;
    } else {
      if ((int16_t)current_index == 3)
        current_index = 0;
      else
        current_index = iter_index + 1;
    }
  } while ((int16_t)current_index != weapon_index);

  return (int16_t)best_index;
}

/* unit_set_in_vehicle (0x1ae600)
 * Puts the unit's current weapon away and detaches it; returns true if the
 * weapon was stowed. */
bool unit_set_in_vehicle(int unit_handle, bool flag)
{
  unit_data_t *unit;
  int weapon_handle;
  int16_t new_index;
  object_data_t *weapon;
  int16_t cur_index;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  object_get_and_verify_type(unit_handle, _object_mask_unit);
  weapon_handle = unit_inventory_get_weapon(unit_handle, unit->current_weapon_index);
  new_index = unit_next_weapon_index(unit_handle, unit->current_weapon_index, 1);

  if (weapon_handle == -1)
    return false;
  if (new_index == unit->current_weapon_index && !flag)
    return false;

  weapon = object_get_and_verify_type(weapon_handle, _object_mask_all);
  if (weapon->flags & 1)
    return false;

  if (!weapon_put_away(weapon_handle, flag))
    return false;

  first_person_weapon_message_from_unit(unit_handle, 0xd);

  unit_detach_weapon(unit_handle, weapon_handle);

  cur_index = unit->current_weapon_index;
  unit->weapon_object_indices[cur_index].value = -1;
  unit->current_weapon_index = -1;
  unit->desired_weapon_index = unit_next_weapon_index(unit_handle, -1, 0);

  if (!weapon_can_be_fired(weapon_handle))
    object_delete(weapon_handle);

  return true;
}

/* unit_get_weapon_name (0x1ae700)
 * Returns the current weapon's label, or "unarmed".  Register arg:
 * unit_handle in ESI; no stack args. */
char *unit_get_weapon_name(int unit_handle)
{
  unit_data_t *unit;
  int weapon_handle;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  weapon_handle = unit_inventory_get_weapon(unit_handle, unit->current_weapon_index);
  if (weapon_handle == -1) {
    return "unarmed";
  }
  return weapon_get_label(weapon_handle);
}

/* scripting_set_magic_base_seat (0x1ae730)
 * Sets the global magic base seat from a seat name string. */
void scripting_set_magic_base_seat(const char *seat_name)
{
  /* 0x32de80: global magic base seat, read by unit_update_animation. */
  MAGIC_BASE_ANIMATION_SEAT_INDEX = seat_label_to_base_seat_index(seat_name);
}

/* unit_scripting_set_seat (0x1ae750)
 * Sets the unit's seat override from a seat name string. */
void unit_scripting_set_seat(int unit_handle, const char *seat_name)
{
  unit_data_t *unit;
  char seat;

  if (unit_handle != -1) {
    unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
    seat = (char)seat_label_to_base_seat_index(seat_name);
    unit->magic_seat_index = seat;
  }
}

/* unit_handle_deleted_object (0x1ae780)
 * Cleans up references to a deleted object in all unit fields. */
void unit_handle_deleted_object(int unit_handle, int deleted_handle)
{
  unit_data_t *unit;
  int16_t i;
  int *weapon_slot;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if (unit->grenade_object_index.value == deleted_handle) {
    unit->grenade_object_index.value = -1;
  }
  if (unit->driver_object_index.value == deleted_handle) {
    unit->driver_object_index.value = -1;
  }
  if (unit->gunner_object_index.value == deleted_handle) {
    unit->gunner_object_index.value = -1;
  }
  i = 0;
  weapon_slot = &unit->weapon_object_indices[0].value;
  do {
    if (*weapon_slot == deleted_handle) {
      *weapon_slot = -1;
      if (i == unit->desired_weapon_index) {
        unit->desired_weapon_index = -1;
      }
      if (i == unit->current_weapon_index) {
        unit->current_weapon_index = -1;
      }
    }
    i++;
    weapon_slot++;
  } while (i < 4);
  if (unit->current_weapon_index == -1) {
    unit->desired_weapon_index = unit_weapon_next_index(unit_handle, -1, 0);
  }
  if (unit->equipment_object_index.value == deleted_handle) {
    unit->equipment_object_index.value = -1;
  }
  if (unit->delayed_damage_attacker_object_index == deleted_handle) {
    unit->delayed_damage_attacker_object_index = -1;
  }
}

/* unit_cause_melee_damage (0x1ae840)
 * Applies melee damage from the unit's melee marker to a target; on a miss
 * against a valid material it plays the melee sound instead. */
void unit_cause_melee_damage(int unit_handle, char melee_hit, int target_handle,
                             int node_index, int region_index,
                             int permutation_index, float *impact_direction)
{
  unit_data_t *unit;
  char *unit_tag;
  int16_t marker_count;
  collision_test_result collision;
  real_point3d *position;
  real_point3d melee_position;
  real_vector3d direction;
  char coll_hit;
  int16_t coll_depth;
  int weapon_handle;
  object_data_t *weapon;
  char *weapon_tag;
  int damage_effect_index;
  object_marker marker;
  struct damage_data damage;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);

  if (unit_definition_melee_damage_index(unit_tag) == -1) {
    goto cleanup;
  }

  marker_count = object_get_marker_by_name(unit_handle, MARKER_NAME_MELEE, &marker, 1);

  if (marker_count == 1) {
    melee_position.x = marker.matrix.position.x;
    melee_position.y = marker.matrix.position.y;
    melee_position.z = marker.matrix.position.z;

    if (global_current_collision_user_depth >= MAXIMUM_COLLISION_USER_STACK_DEPTH) {
      display_assert("global_current_collision_user_depth < "
                     "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x21b9, true);
      system_exit(-1);
    }

    coll_depth = global_current_collision_user_depth;
    global_current_collision_user_depth = coll_depth + 1;

    position = &unit->object.bounding_sphere_center;
    collision_user_stack[coll_depth] = 7;

    direction.i = melee_position.x - position->x;
    direction.j = melee_position.y - position->y;
    direction.k = melee_position.z - position->z;

    coll_hit = (char)collision_test_vector(0x1000e9, position, &direction, -1,
                                           (int16_t *)&collision);
    if (coll_hit != 0) {
      melee_position.x = position->x;
      melee_position.y = position->y;
      melee_position.z = position->z;
    }

    if (global_current_collision_user_depth < 2) {
      display_assert("global_current_collision_user_depth > 1",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x21c1, true);
      system_exit(-1);
    }
    global_current_collision_user_depth -= 1;
  } else {
    position = &unit->object.bounding_sphere_center;
    melee_position.x = position->x;
    melee_position.y = position->y;
    melee_position.z = position->z;
  }

  damage_effect_index = unit_definition_melee_damage_index(unit_tag);

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  weapon_handle = unit_inventory_get_weapon(unit_handle, unit->current_weapon_index);

  if (weapon_handle != -1) {
    weapon = object_get_and_verify_type(weapon_handle, 4);
    weapon_tag = tag_get(TAG_GROUP_WEAP, weapon->tag_index);
    /* Weapon definition flags bit 15 selects the weapon's own melee damage. */
    if ((char)(weapon_definition_flags(weapon_tag) >> 8) < 0) {
      damage_effect_index = weapon_definition_melee_damage_index(weapon_tag);
    }
  }

  damage_data_new(&damage, damage_effect_index);

  /* The damage location. */
  damage_data_location(&damage) = unit->object.location;
  damage.owner_team_index = unit->object.owner_team_index;
  damage.owner_player_index = unit->player_index.value;
  damage.origin.y = melee_position.y;
  damage.epicenter.y = unit->object.bounding_sphere_center.y;
  damage.owner_object_index = unit_handle;
  damage.origin.x = melee_position.x;
  damage.epicenter.x = unit->object.bounding_sphere_center.x;
  damage.origin.z = melee_position.z;
  damage.epicenter.z = unit->object.bounding_sphere_center.z;

  if (target_handle == -1) {
    area_of_effect_cause_damage(&damage, -1);
  } else {
    object_cause_damage(&damage, target_handle, (short)node_index,
                        (short)region_index, (short)permutation_index,
                        impact_direction);
  }

  /* Material type written back by object_cause_damage. */
  if (melee_hit == 0 && damage_data_material_type(&damage) != -1) {
    unit_melee_sound(damage_data_material_type(&damage), unit_handle,
                 damage_effect_index);
  }

cleanup:
  unit->melee_attack_state = 0;
}

/* unit_cause_player_melee_damage (0x1aea90)
 * Sweeps a 5x5 fan of rays along the unit's aiming vector, damages the best
 * object hit, pushes hit vehicles, and plays the melee material sound. */
void unit_cause_player_melee_damage(int unit_handle)
{
  unit_data_t *unit;
  int unit_tag_data;
  int best_object;
  int hit_target;
  object_data_t *weapon;
  int weapon_tag_data;
  int melee_damage_effect;
  int melee_response_effect;
  real_vector3d *facing;
  float perp[3];
  float cross_vec[3];
  real_vector3d ray_direction;
  real_point3d ray_origin;
  collision_test_result collision;
  object_marker marker;
  int16_t depth;
  int outer_i;
  int outer_count;
  int inner_i;
  int inner_count;
  float fi;
  float fj;
  char hit;
  object_data_t *hit_object;
  int parent_handle;
  short best_type;
  float best_fraction;
  int16_t material_idx;
  unsigned int material_surface;
  int16_t current_weapon_idx;
  int globals;
  char *globals_element_ptr;
  float dot_product;
  x87_wide_t dot_product_wide;
  float melee_scale;
  real_vector3d kick_vec;
  int16_t hit_material;
  int weapon_handle;
  struct damage_data damage;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag_data = (int)tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  best_object = -1;
  hit_material = -1;
  material_idx = -1;
  melee_response_effect = -1;

  object_get_marker_by_name(unit_handle, MARKER_NAME_HEAD, &marker, 1);

  ray_origin.x = marker.matrix.position.x;
  ray_origin.y = marker.matrix.position.y;
  ray_origin.z = marker.matrix.position.z;

  if (global_current_collision_user_depth >=
      MAXIMUM_COLLISION_USER_STACK_DEPTH) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x2212, 1);
    system_exit(-1);
  }
  depth = global_current_collision_user_depth;
  global_current_collision_user_depth = depth + 1;
  collision_user_stack[depth] = 8;

  facing = &unit->aiming_vector;
  perpendicular3d(&facing->i, perp);
  normalize3d((real_vector3d *)perp);

  best_type = 0;
  best_fraction = 0.0f;

  cross_vec[0] = perp[2] * facing->j - perp[1] * facing->k;
  cross_vec[1] = perp[0] * facing->k - perp[2] * facing->i;
  cross_vec[2] = perp[1] * facing->i - perp[0] * facing->j;

  outer_i = -2;
  outer_count = 5;
  do {
    fi = (float)outer_i;
    inner_i = -2;
    inner_count = 5;
    do {
      fj = (float)inner_i;

      ray_direction.i =
        facing->i * 0.8f + (fi * perp[0] + cross_vec[0] * fj) * 0.1f;
      ray_direction.j =
        facing->j * 0.8f + (fi * perp[1] + cross_vec[1] * fj) * 0.1f;
      ray_direction.k =
        facing->k * 0.8f + (fi * perp[2] + fj * cross_vec[2]) * 0.1f;

      hit = collision_test_vector(0x1000e9, &ray_origin, &ray_direction, unit_handle,
                         (int16_t *)&collision);

      if (hit != 0) {
        if (collision.type == 2) {
          if (best_object == -1) {
            hit_material = collision.material_type;
            if ((collision.flags & 8) != 0) {
              material_idx =
                (int16_t)(unsigned char)collision.breakable_surface_index;
              material_surface = collision.surface_index;
            }
          }
        } else if (collision.type == 3) {
          hit_target = collision.object_index;
          hit_object = object_get_and_verify_type(hit_target, -1);

          if (hit_object->type != 2) {
            parent_handle = hit_object->parent_object_index.value;
            if (parent_handle != -1) {
              hit_target = parent_handle;
              hit_object = object_get_and_verify_type(parent_handle, -1);
            }
          }

          if (best_object == -1 ||
              (hit_object->type == 0 &&
               ((best_type == 0 &&
                 collision.t < best_fraction) ||
                best_type != 0))) {
            best_type = hit_object->type;
            hit_material = collision.material_type;
            best_fraction = collision.t;
            best_object = hit_target;
          }
        }
      }
      inner_i = inner_i + 1;
      inner_count = inner_count - 1;
    } while (inner_count != 0);
    outer_i = outer_i + 1;
    outer_count = outer_count - 1;
  } while (outer_count != 0);

  if (global_current_collision_user_depth < 2) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x2256, 1);
    system_exit(-1);
  }
  global_current_collision_user_depth -= 1;

  {
    unit_data_t *re_unit;
    re_unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
    current_weapon_idx = re_unit->current_weapon_index;
    re_unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

    melee_damage_effect = -1;
    if (current_weapon_idx != -1) {
      if (current_weapon_idx < 0 || current_weapon_idx >= 4) {
        display_assert("index>=0 && index<MAXIMUM_WEAPONS_PER_UNIT",
                       "c:\\halo\\SOURCE\\units\\units.c", 0x20ac, 1);
        system_exit(-1);
      }
      weapon_handle = re_unit->weapon_object_indices[current_weapon_idx].value;

      if (weapon_handle != -1) {
        weapon = object_get_and_verify_type(weapon_handle, 4);
        weapon_tag_data = (int)tag_get(TAG_GROUP_WEAP, weapon->tag_index);
        melee_damage_effect = weapon_definition_melee_damage_index(weapon_tag_data);
        melee_response_effect = weapon_definition_field_3b0(weapon_tag_data);
        if (melee_damage_effect != -1)
          goto got_damage_effect;
      }
    }
    melee_damage_effect = unit_definition_melee_damage_index(unit_tag_data);
  }

got_damage_effect:
  if (best_object != -1) {
    object_data_t *best;
    best = object_get_and_verify_type(best_object, -1);
    if (best->type == _object_type_vehicle) {
      char *hit_obj_tag;
      hit_obj_tag = tag_get(TAG_GROUP_OBJE, best->tag_index);
      melee_scale = object_definition_field_20(hit_obj_tag) * 0.035f;
      kick_vec.i = melee_scale * facing->i;
      kick_vec.j = melee_scale * facing->j;
      kick_vec.k = melee_scale * facing->k;
      vehicle_accelerate(best_object, &kick_vec);
    }
  }

  if (melee_damage_effect != -1) {
    damage_data_new(&damage, melee_damage_effect);
    damage.flags |= 1;
    damage.owner_team_index = unit->object.owner_team_index;
    /* The damage location. */
    damage_data_location(&damage) = unit->object.location;
    damage.owner_player_index = unit->player_index.value;
    damage.owner_object_index = unit_handle;
    damage.origin.x = ray_origin.x;
    damage.origin.y = ray_origin.y;
    damage.origin.z = ray_origin.z;
    damage.epicenter.x = unit->object.bounding_sphere_center.x;
    damage.epicenter.y = unit->object.bounding_sphere_center.y;
    damage.epicenter.z = unit->object.bounding_sphere_center.z;
    damage.direction.i = facing->i;
    damage.direction.j = facing->j;
    damage.direction.k = facing->k;
    /* Material type that was hit. */
    damage_data_material_type(&damage) = hit_material;

    if (best_object == -1) {
      if ((short)material_idx != -1) {
        breakable_surface_damage(material_idx, &damage, material_surface);
      }
    } else {
      object_data_t *hit_type_data;
      hit_type_data = object_get_and_verify_type(best_object, -1);
      if (hit_type_data->type == 7) {
        ((void (*)(int))0x95c10)(best_object);
      }

      globals = (int)game_globals_get();
      globals_element_ptr =
        (char *)(int)tag_block_get_element((void *)(globals + 0x170), 0, 0xf4);

      dot_product = 0.0f;
      if (0.0f < ((game_globals_player_information_t *)globals_element_ptr)->field_34) {
        dot_product_wide =
          ((x87_wide_t)unit->object.forward.i *
             unit->object.translational_velocity.i +
           (x87_wide_t)unit->object.forward.j *
             unit->object.translational_velocity.j +
           (x87_wide_t)unit->object.forward.k *
             unit->object.translational_velocity.k) * 30.0f;
        dot_product_wide =
          dot_product_wide / ((game_globals_player_information_t *)globals_element_ptr)->field_34;
        /* 0x1aef85 keeps the quotient wide for the zero comparison while
         * storing a float32 copy reloaded at 0x1aef9e for the one comparison. */
        dot_product = HALO_NARROW(dot_product_wide);
        if (dot_product_wide < 0.0f) {
          dot_product = 0.0f;
        } else if (dot_product > 1.0f) {
          dot_product = 1.0f;
        }
      }

      if (unit->object.type == _object_type_biped) {
        biped_data_t *biped;
        biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
        if (biped->airborne_ticks > 0x0f) {
          dot_product = 1.5f;
        }
      }

      damage.scale = dot_product;

      hit_type_data = object_get_and_verify_type(best_object, -1);
      if (hit_type_data->type == 0) {
        object_cause_damage(&damage, best_object, -1, -1, -1, (float *)0);
      }
    }
  }

  if ((short)hit_material != -1) {
    unit_melee_sound((short)hit_material, unit_handle, melee_damage_effect);
    if (melee_response_effect != -1) {
      damage_data_new(&damage, melee_response_effect);
      damage.direction.i = -facing->i;
      damage.epicenter.x = unit->object.bounding_sphere_center.x;
      damage.direction.j = -facing->j;
      damage.epicenter.y = unit->object.bounding_sphere_center.y;
      damage.epicenter.z = unit->object.bounding_sphere_center.z;
      damage.direction.k = -facing->k;
      damage.origin.x = unit->object.bounding_sphere_center.x;
      damage.origin.y = unit->object.bounding_sphere_center.y;
      damage.origin.z = unit->object.bounding_sphere_center.z;
      damage.flags |= 8;
      object_cause_damage(&damage, unit_handle, -1, -1, -1, (float *)0);
    }
  }

  unit->melee_attack_state = 0;
}

/* unit_stop_custom_animation (0x1af0d0)
 * Stops a custom animation if the current state is 0x1c. */
void unit_stop_custom_animation(int unit_handle)
{
  unit_data_t *unit;

  if (unit_handle != -1) {
    unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
    if ((char)unit->animation_state == 0x1c) {
      unit_animation_set_state(unit_handle, 0);
    }
  }
}

/* unit_custom_animation_at_frame (0x1af100)
 * Starts a custom animation and sets it to a specific frame. */
char unit_custom_animation_at_frame(int unit_handle, int anim_tag, int entry,
                                    int do_flag, int16_t frame)
{
  unit_data_t *unit;
  animation_graph_t *animation_graph;
  animation_t *animation;

  if (!unit_start_user_animation(unit_handle, anim_tag, (void *)entry, do_flag)) {
    return 0;
  }
  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  animation_graph = tag_get(TAG_GROUP_ANTR, unit->object.unk_124);
  animation = tag_block_get_element(&animation_graph->animations,
                                    unit->object.unk_128, 0xb4);
  if (frame >= 0 && frame < animation->frame_count) {
    unit->object.unk_130 = frame;
    return 1;
  }
  return 0;
}

/* unit_apply_alignment_vector (0x1af180)
 * Faces an unparented unit along a ground-plane alignment vector, with the
 * global up vector as its up axis.
 * Register args: unit_handle @<eax>, alignment_vector @<ecx>. */
void unit_apply_alignment_vector(int unit_handle, const real_vector2d *alignment_vector)
{
  unit_data_t *unit;
  real_vector3d *up_vector;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if (unit->object.parent_object_index.value == -1) {
    if (!valid_real_normal2d((float *)alignment_vector)) {
      display_assert(
        csprintf(error_string_buffer,
                 "%s: assert_valid_real_normal2d(%f, %f)",
                 "alignment_vector", (double)alignment_vector->i,
                 (double)alignment_vector->j),
        "c:\\halo\\SOURCE\\units\\units.c", 0x2482, 1);
      system_exit(-1);
    }

    unit->object.forward.i = alignment_vector->i;
    unit->object.forward.j = alignment_vector->j;
    unit->object.forward.k = 0.0f;

    up_vector = GLOBAL_UP3D;
    unit->object.up.i = up_vector->i;
    unit->object.up.j = up_vector->j;
    unit->object.up.k = up_vector->k;

    if (!valid_real_normal3d_perpendicular(&unit->object.forward,
                                           &unit->object.up)) {
      display_assert(
        csprintf(
          error_string_buffer,
          "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
          "&unit->object.forward", "&unit->object.up",
          (double)unit->object.forward.i, (double)unit->object.forward.j,
          (double)unit->object.forward.k, (double)unit->object.up.i,
          (double)unit->object.up.j, (double)unit->object.up.k),
        "c:\\halo\\SOURCE\\units\\units.c", 0x2486, 1);
      system_exit(-1);
    }
  }
}

/* unit_start_flaming_to_death (0x1af2a0)
 * Initiates the flaming-to-death state for a unit. */
void unit_start_flaming_to_death(int unit_handle, int attacker_handle)
{
  unit_data_t *unit;
  unsigned int *seed;
  int16_t ticks;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_set_in_vehicle(unit_handle, 1);
  unit->flags |= 0x80;
  *(uint16_t *)&unit->object.damage_flags =
    (*(uint16_t *)&unit->object.damage_flags & ~0x4u) | 0x800;
  if (unit->flaming_death_delay == 0) {
    seed = get_global_random_seed_address();
    ticks = seed_random_range((unsigned int *)seed, 0x3c, 0x96);
    if (ticks < 1) {
      ticks = 1;
    } else if (ticks > 0xff) {
      ticks = 0xff;
    }
    unit->flaming_death_delay = (uint8_t)ticks;
    unit->flaming_death_attacker_object_index.value = attacker_handle;
    unit_start_running_blindly(unit_handle);
  }
}

/* unit_update_running_blind (0x1af340) — update running blind direction
 * Computes a random steering angle for the unit when running blind (no actor
 * guidance). Uses fcos/fsin to rotate the run vector around the global up axis.
 * Register args: @eax = unit_handle, @esi = run_vector (float[3]).
 * No stack params. */
void unit_update_running_blind(int unit_handle, real_vector3d *run_vector)
{
  unit_data_t *unit;
  char has_actor_guidance;
  char *msg;
  float max_left;
  float max_right;
  float angle_delta;
  float angle;
  float cos_angle;
  float sin_angle;
  real_vector3d *up_axis;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  has_actor_guidance = 0;

  if (unit->actor_index.value == -1 ||
      actor_get_running_blind_vector(unit->actor_index.value, (float *)run_vector) == 0) {
    /* No actor guidance: run along the global forward vector. */
    real_vector3d *forward;
    forward = GLOBAL_FORWARD3D;
    run_vector->i = forward->i;
    run_vector->j = forward->j;
    run_vector->k = forward->k;
  } else {
    has_actor_guidance = 1;
  }

  if (valid_real_normal3d(run_vector) == 0) {
    msg = csprintf(
      ERROR_STRING_BUFFER, "%s: assert_valid_real_normal3d(%f, %f, %f)",
      "run_vector", (double)run_vector->i, (double)run_vector->j,
      (double)run_vector->k, "c:\\halo\\SOURCE\\units\\units.c", 0x253e, true);
    display_assert(msg, "c:\\halo\\SOURCE\\units\\units.c", 0x253e, true);
    system_exit(-1);
  }

  max_right = 1.0f;
  max_left = REAL_ONE_POOL; /* 1.0f */

  if (has_actor_guidance) {
    /* Compute max turn rates based on current angle */
    float fwd_range;
    float bwd_range;

    /* 0x1af3f3-0x1af448: form both raw ranges first, then scale and
     * clamp each one. This keeps the paired values in the same x87 shape. */
    fwd_range = REAL_QUARTER_PI_POOL - unit->run_blindly_angle;
    bwd_range = unit->run_blindly_angle + REAL_QUARTER_PI_POOL;
    fwd_range *= REAL_4_2441316_POOL;
    if (fwd_range < REAL_ONE_POOL) {
      max_right = fwd_range;
    }
    bwd_range *= REAL_4_2441316_POOL;
    if (bwd_range < REAL_ONE_POOL) {
      max_left = bwd_range;
    }
  }

  /* Apply pitch constraints */
  {
    float fwd_pitch;
    float bwd_pitch;

    fwd_pitch =
      (REAL_12_DEGREES_POOL - unit->run_blindly_angle_delta) * REAL_100_OVER_TWO_PI_POOL;
    if (fwd_pitch < max_right) {
      max_right = fwd_pitch;
    }

    bwd_pitch =
      (unit->run_blindly_angle_delta + REAL_12_DEGREES_POOL) * REAL_100_OVER_TWO_PI_POOL;
    if (bwd_pitch < max_left) {
      max_left = bwd_pitch;
    }
  }

  /* Determine random angle delta */
  if (max_right < max_left) {
    /* Left turn dominant */
    if (max_right < REAL_NEGATIVE_ONE_POOL) {
      angle_delta = REAL_NEG_1_2_DEGREES_POOL;
      goto apply_angle;
    }
    if (max_right >= REAL_ONE_POOL) {
      max_right = REAL_ONE_POOL * REAL_1_2_DEGREES_POOL;
    } else {
      max_right = max_right * REAL_1_2_DEGREES_POOL;
    }
    max_left = -0.02094395f;
  } else {
    /* Right turn dominant */
    if (max_left < REAL_NEGATIVE_ONE_POOL) {
      angle_delta = REAL_1_2_DEGREES_POOL;
      goto apply_angle;
    }
    if (max_left >= REAL_ONE_POOL) {
      max_left = REAL_ONE_POOL;
    }
    max_left = max_left * REAL_NEG_1_2_DEGREES_POOL;
    max_right = 0.02094395f;
  }

  {
    unsigned int *seed;
    seed = get_global_random_seed_address();
    angle_delta = random_real_range(seed, max_left, max_right);
  }

apply_angle:
  /* Accumulate angle delta into unit steering */
  angle_delta += unit->run_blindly_angle_delta;
  unit->run_blindly_angle_delta = angle_delta;

  angle = angle_delta + unit->run_blindly_angle;
  unit->run_blindly_angle = angle;

  /* Wrap angle to [-pi, pi] */
  if (angle < REAL_NEGATIVE_PI_POOL) {
    unit->run_blindly_angle = angle + REAL_TWO_PI_POOL;
  } else if (angle > REAL_PI_POOL) {
    unit->run_blindly_angle = angle - REAL_TWO_PI_POOL;
  }

  /* Rotate run_vector by the angle around the global up axis */
#if defined(_MSC_VER) && !defined(__clang__)
  /* VC71 /Oi inlines cos/sin as FCOS/FSIN sharing ST0, matching the original
   * codegen; the clang runtime build keeps the explicit x87 helpers. */
  cos_angle = (float)cos((double)unit->run_blindly_angle);
  sin_angle = (float)sin((double)unit->run_blindly_angle);
#else
  cos_angle = x87_fcos(unit->run_blindly_angle);
  sin_angle = x87_fsin(unit->run_blindly_angle);
#endif
  up_axis = GLOBAL_UP3D;
  rotate_vector3d_by_sincos(run_vector, up_axis, sin_angle, cos_angle);

  /* Assert valid normal after rotation */
  if (valid_real_normal3d(run_vector) == 0) {
    msg = csprintf(
      ERROR_STRING_BUFFER, "%s: assert_valid_real_normal3d(%f, %f, %f)",
      "run_vector", (double)run_vector->i, (double)run_vector->j,
      (double)run_vector->k, "c:\\halo\\SOURCE\\units\\units.c", 0x2585, true);
    display_assert(msg, "c:\\halo\\SOURCE\\units\\units.c", 0x2585, true);
    system_exit(-1);
  }
}

/* unit_verify_vectors (0x1af620)
 * Register arg: unit_handle @<eax>.  The single && chain reproduces the
 * original's one-setne exit; the validators return bool (callers TEST AL). */
bool unit_verify_vectors(int unit_handle)
{
  unit_data_t *unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  return (bool)(valid_real_normal3d(&unit->desired_facing_vector) &&
                valid_real_normal3d(&unit->desired_aiming_vector) &&
                valid_real_normal3d(&unit->desired_looking_vector) &&
                valid_real_normal3d_perpendicular(&unit->object.forward,
                                                  &unit->object.up) &&
                valid_real_normal3d(&unit->aiming_vector) &&
                valid_real_normal3d(&unit->looking_vector));
}

/* unit_control_trace (0x1af6b0)
 * Dumps the unit's vectors to the error log when unit_verify_vectors fails,
 * then asserts if they are still invalid.  Register arg: unit_handle @<edi>. */
void unit_control_trace(int unit_handle, const char *label)
{
  unit_data_t *unit;
  int32_t actor;
  char location_buf[512];

  if (unit_verify_vectors(unit_handle))
    return;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  actor = unit->actor_index.value;
  if (actor == -1)
    actor = unit->swarm_actor_index.value;

  ai_debug_describe_actor(actor, unit_handle, 1, location_buf, 512);

  error(2, "**** unit_verify_vectors: problems with %s at location %s",
        location_buf, label);

  error(2, "  object: pos %f %f %f, fwd %f %f %f, up %f %f %f",
        (double)unit->object.position.x, (double)unit->object.position.y,
        (double)unit->object.position.z, (double)unit->object.forward.i,
        (double)unit->object.forward.j, (double)unit->object.forward.k,
        (double)unit->object.up.i, (double)unit->object.up.j,
        (double)unit->object.up.k);

  error(
    2, "  desired facing %f %f %f, aiming %f %f %f, looking %f %f %f",
    (double)unit->desired_facing_vector.i, (double)unit->desired_facing_vector.j, (double)unit->desired_facing_vector.k,
    (double)unit->desired_aiming_vector.i, (double)unit->desired_aiming_vector.j, (double)unit->desired_aiming_vector.k,
    (double)unit->desired_looking_vector.i, (double)unit->desired_looking_vector.j, (double)unit->desired_looking_vector.k);

  error(2, "  aiming vector %f %f %f velocity %f %f %f",
        (double)unit->aiming_vector.i, (double)unit->aiming_vector.j,
        (double)unit->aiming_vector.k, (double)unit->aiming_velocity.i,
        (double)unit->aiming_velocity.j, (double)unit->aiming_velocity.k);

  error(2, "  looking vector %f %f %f velocity %f %f %f",
        (double)unit->looking_vector.i, (double)unit->looking_vector.j,
        (double)unit->looking_vector.k, (double)unit->looking_velocity.i,
        (double)unit->looking_velocity.j, (double)unit->looking_velocity.k);

  error(2, "  warning, hex dump follows...");

  error(
    2, "  object: pos %08X %08X %08X, fwd %08X %08X %08X, up %08X %08X %08X",
    REAL_BITS(unit->object.position.x), REAL_BITS(unit->object.position.y),
    REAL_BITS(unit->object.position.z), REAL_BITS(unit->object.forward.i),
    REAL_BITS(unit->object.forward.j), REAL_BITS(unit->object.forward.k),
    REAL_BITS(unit->object.up.i), REAL_BITS(unit->object.up.j),
    REAL_BITS(unit->object.up.k));

  error(2,
        "  desired facing %08X %08X %08X, aiming %08X %08X %08X, looking %08X "
        "%08X %08X",
        REAL_BITS(unit->desired_facing_vector.i), REAL_BITS(unit->desired_facing_vector.j),
        REAL_BITS(unit->desired_facing_vector.k), REAL_BITS(unit->desired_aiming_vector.i),
        REAL_BITS(unit->desired_aiming_vector.j), REAL_BITS(unit->desired_aiming_vector.k),
        REAL_BITS(unit->desired_looking_vector.i), REAL_BITS(unit->desired_looking_vector.j),
        REAL_BITS(unit->desired_looking_vector.k));

  error(2, "  aiming vector %08X %08X %08X velocity %08X %08X %08X",
        REAL_BITS(unit->aiming_vector.i), REAL_BITS(unit->aiming_vector.j),
        REAL_BITS(unit->aiming_vector.k), REAL_BITS(unit->aiming_velocity.i),
        REAL_BITS(unit->aiming_velocity.j), REAL_BITS(unit->aiming_velocity.k));

  error(2, "  looking vector %08X %08X %08X velocity %08X %08X %08X",
        REAL_BITS(unit->looking_vector.i), REAL_BITS(unit->looking_vector.j),
        REAL_BITS(unit->looking_vector.k), REAL_BITS(unit->looking_velocity.i),
        REAL_BITS(unit->looking_velocity.j), REAL_BITS(unit->looking_velocity.k));

  if (unit_verify_vectors(unit_handle))
    return;

  display_assert("unit_verify_vectors FAILURE, see above for details",
                 "c:\\halo\\SOURCE\\units\\units.c", 0x252, true);
  system_exit(-1);
}

/* unit_control_t: the 0x40-byte control block passed to unit_set_control.
 * Member names come from the unit_set_control assert strings; the size is the
 * csmemset length in the ai callers that build it. */
typedef struct unit_control_t {
  int8_t animation_state;    /* NUMBER_OF_UNIT_ANIMATION_STATES */
  int8_t aiming_speed;       /* NUMBER_OF_UNIT_AIMING_SPEEDS */
  uint16_t control_flags;    /* NUMBER_OF_UNIT_CONTROL_FLAGS */
  int16_t weapon_index;
  int16_t grenade_index;
  int16_t zoom_level;
  uint8_t pad_0a[2];
  vector3_t throttle;
  float primary_trigger;
  vector3_t facing_vector;
  vector3_t aiming_vector;
  vector3_t looking_vector;
} unit_control_t;
cs(unit_control_t, 0x40);
co(unit_control_t, aiming_speed, 0x01);
co(unit_control_t, control_flags, 0x02);
co(unit_control_t, weapon_index, 0x04);
co(unit_control_t, grenade_index, 0x06);
co(unit_control_t, zoom_level, 0x08);
co(unit_control_t, throttle, 0x0c);
co(unit_control_t, primary_trigger, 0x18);
co(unit_control_t, facing_vector, 0x1c);
co(unit_control_t, aiming_vector, 0x28);
co(unit_control_t, looking_vector, 0x34);

/* unit_set_control (0x1af990)
 * Validates a unit_control_t block and copies it into the unit's desired
 * control state. */
void unit_set_control(int unit_handle, void *unit_control)
{
  unit_data_t *unit;
  unit_control_t *control_data;
  float mag;
  vector3_t *looking;

  control_data = unit_control;
  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  mag = sqrtf(control_data->throttle.z * control_data->throttle.z +
              control_data->throttle.y * control_data->throttle.y +
              control_data->throttle.x * control_data->throttle.x);
  if (!(mag <= REAL_3_0_POOL)) {
    display_assert("magnitude3d(&control_data->throttle)<=3.0f",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e1, 1);
    system_exit(-1);
  }

  if (control_data->animation_state < 0 || control_data->animation_state >= 7) {
    display_assert("control_data->animation_state>=0 && control_data->"
                   "animation_state<NUMBER_OF_UNIT_ANIMATION_STATES",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e2, 1);
    system_exit(-1);
  }

  if (control_data->aiming_speed < 0 || control_data->aiming_speed >= 2) {
    display_assert("control_data->aiming_speed>=0 && control_data->"
                   "aiming_speed<NUMBER_OF_UNIT_AIMING_SPEEDS",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e3, 1);
    system_exit(-1);
  }

  if (control_data->control_flags & 0x8000) {
    display_assert("VALID_FLAGS(control_data->control_flags, "
                   "NUMBER_OF_UNIT_CONTROL_FLAGS)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e4, 1);
    system_exit(-1);
  }

  if (!valid_real_normal3d((real_vector3d *)&control_data->facing_vector)) {
    display_assert(
      csprintf(error_string_buffer,
               "%s: assert_valid_real_normal3d(%f, %f, %f)",
               "&control_data->facing_vector", (double)control_data->facing_vector.x,
               (double)control_data->facing_vector.y, (double)control_data->facing_vector.z),
      "c:\\halo\\SOURCE\\units\\units.c", 0x5e5, 1);
    system_exit(-1);
  }

  if (!valid_real_normal3d((real_vector3d *)&control_data->aiming_vector)) {
    display_assert(
      csprintf(error_string_buffer,
               "%s: assert_valid_real_normal3d(%f, %f, %f)",
               "&control_data->aiming_vector", (double)control_data->aiming_vector.x,
               (double)control_data->aiming_vector.y, (double)control_data->aiming_vector.z),
      "c:\\halo\\SOURCE\\units\\units.c", 0x5e6, 1);
    system_exit(-1);
  }

  looking = &control_data->looking_vector;
  if (!valid_real_normal3d((real_vector3d *)looking)) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_normal3d(%f, %f, %f)",
                            "&control_data->looking_vector", (double)looking->x,
                            (double)control_data->looking_vector.y,
                            (double)control_data->looking_vector.z),
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e7, 1);
    system_exit(-1);
  }

  if (control_data->weapon_index != NONE &&
      (control_data->weapon_index < 0 ||
       control_data->weapon_index >= MAXIMUM_WEAPONS_PER_UNIT)) {
    display_assert("control_data->weapon_index==NONE || (control_data->"
                   "weapon_index>=0 && control_data->"
                   "weapon_index<MAXIMUM_WEAPONS_PER_UNIT)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e8, 1);
    system_exit(-1);
  }

  if (control_data->grenade_index != NONE &&
      (control_data->grenade_index < 0 ||
       control_data->grenade_index >= NUMBER_OF_UNIT_GRENADE_TYPES)) {
    display_assert("control_data->grenade_index==NONE || (control_data->"
                   "grenade_index>=0 && control_data->"
                   "grenade_index<NUMBER_OF_UNIT_GRENADE_TYPES)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e9, 1);
    system_exit(-1);
  }

  if (control_data->zoom_level != NONE && control_data->zoom_level < 0) {
    display_assert("control_data->zoom_level==NONE || (control_data->"
                   "zoom_level>=0)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5ea, 1);
    system_exit(-1);
  }

  if ((*(uint32_t *)&control_data->primary_trigger & 0x7f800000) == 0x7f800000) {
    display_assert(
      csprintf(error_string_buffer, "%s: assert_valid_real(0x%08X %f)",
               "control_data->primary_trigger", *(uint32_t *)&control_data->primary_trigger,
               (double)control_data->primary_trigger),
      "c:\\halo\\SOURCE\\units\\units.c", 0x5eb, 1);
    system_exit(-1);
  }

  unit->throttle.x = control_data->throttle.x;
  unit->throttle.y = control_data->throttle.y;
  unit->throttle.z = control_data->throttle.z;

  unit->primary_trigger = control_data->primary_trigger;

  unit->aiming_speed = control_data->aiming_speed;

  if (control_data->weapon_index != NONE)
    unit->desired_weapon_index = control_data->weapon_index;

  if (control_data->grenade_index != NONE)
    unit->desired_grenade_index = (int8_t)control_data->grenade_index;

  unit->desired_zoom_level = (int8_t)control_data->zoom_level;

  unit->control_flags = control_data->control_flags;

  unit->desired_looking_vector.i = looking->x;
  unit->desired_looking_vector.j = looking->y;
  unit->desired_looking_vector.k = looking->z;

  unit->desired_aiming_vector.i = control_data->aiming_vector.x;
  unit->desired_aiming_vector.j = control_data->aiming_vector.y;
  unit->desired_aiming_vector.k = control_data->aiming_vector.z;

  unit->desired_facing_vector.i = control_data->facing_vector.x;
  unit->desired_facing_vector.j = control_data->facing_vector.y;
  unit->desired_facing_vector.k = control_data->facing_vector.z;

  unit->desired_animation_state = control_data->animation_state;

  unit_control_trace(unit_handle, "unit-control");
}

/* unit_preprocess_node_orientations (0x1afd30) */
void unit_preprocess_node_orientations(int unit_handle, int node_output)
{
  unit_data_t *unit;
  unit_definition_t *unit_definition;
  animation_graph_t *animation_graph;
  animation_graph_unit_seat_t *unit_seat;
  short anim_index;
  animation_t *anim_entry;
  char seat_anim_idx;
  real_vector3d *aiming_vec;
  char valid;
  real_matrix4x3 matrix;
  real_vector3d relative_aiming_vector;
  real_vector3d relative_looking_vector;
  real_euler_angles2d relative_aiming_angles;
  real_euler_angles2d relative_looking_angles;
  float *steering_ptr;
  float max_frame;
  float temp;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  /* +0x44: the unit definition's animation_graph tag index. */
  animation_graph =
    tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_definition));

  if ((int16_t)unit->action_animation_index != -1) {
    void *repl_anim;
    repl_anim = tag_block_get_element(&animation_graph->animations,
                                           (int16_t)unit->action_animation_index,
                                           0xb4);
    REPLACEMENT_ANIMATION_APPLY_FN(
      repl_anim, unit->action_animation_frame, (void *)node_output);
  }

  if ((int16_t)unit->overlay_action_animation_index != -1) {
    void *ovl_anim;
    ovl_anim = tag_block_get_element(
      &animation_graph->animations,
      (int16_t)unit->overlay_action_animation_index, 0xb4);
    OVERLAY_ANIMATION_APPLY_FN(
      ovl_anim, unit->overlay_action_animation_frame, (void *)node_output);
  }

  if (unit->soft_ping_animation_index != -1) {
    void *ovl_anim2;
    ovl_anim2 = tag_block_get_element(
      &animation_graph->animations, unit->soft_ping_animation_index,
      0xb4);
    OVERLAY_ANIMATION_APPLY_FN(
      ovl_anim2, unit->soft_ping_animation_frame, (void *)node_output);
  }

  unit_control_trace(unit_handle, "unit-preprocess-nodes");
  unit->aiming_with_euler_screen = 0;
  unit->looking_with_euler_screen = 0;

  if ((unit_definition->flags & 0x800) == 0 &&
      (int8_t)unit->animation_seat_index != -1) {
    unit_seat = tag_block_get_element(&animation_graph_definition_unit_seats(animation_graph),
                                      (int8_t)unit->animation_seat_index, 100);

    if ((int8_t)unit->emotion_index != -1) {
      if (unit_seat->animations.count < 0xc) {
        anim_index = -1;
      } else {
        anim_index = ((short *)unit_seat->animations.address)[11];
      }
      if ((int16_t)unit->override_emotion_animation_index != -1) {
        anim_index = unit->override_emotion_animation_index;
      }
      if (anim_index != -1) {
        anim_entry = tag_block_get_element(&animation_graph->animations,
                                                (int)anim_index, 0xb4);
        seat_anim_idx = unit->emotion_index;
        if (seat_anim_idx >= 0 &&
            (short)seat_anim_idx < anim_entry->frame_count) {
          OVERLAY_ANIMATION_APPLY_FN(
            anim_entry, (short)seat_anim_idx, (void *)node_output);
        }
      }
    }

    if (0.0f < unit->mouth_aperture && unit_seat->animations.count > 10) {
      anim_index = ((short *)unit_seat->animations.address)[10];
      if (anim_index != -1) {
        void *throttle_anim;
        throttle_anim = tag_block_get_element(
          &animation_graph->animations, (int)anim_index, 0xb4);
        OVERLAY_ANIMATION_APPLY_SCALED_FN(throttle_anim, 0, unit->mouth_aperture,
                                       (void *)node_output);
      }
    }

    if ((unit->animation_flags & 2) != 0) {
      int steer_idx;
      int steer_offset;
      int steering_count;
      steering_count = 4;
      steer_idx = 2;
      steer_offset = 4;
      steering_ptr = (float *)&unit->seat_acceleration;
      do {
        if (steer_idx >= 0 && steer_idx < unit_seat->animations.count) {
          anim_index = SEAT_ANIMATION_AT_BYTE_OFFSET(unit_seat->animations, steer_offset);
          if (anim_index != -1) {
            anim_entry = tag_block_get_element(
              &animation_graph->animations, (int)anim_index, 0xb4);
            max_frame = (float)(anim_entry->frame_count - 1);
            OVERLAY_ANIMATION_APPLY_CONTINUOUS_FN(
              anim_entry, max_frame * *steering_ptr, (void *)node_output);
          }
        }
        steer_offset = steer_offset + 2;
        steering_ptr = steering_ptr + 1;
        steer_idx = steer_idx + 1;
        steering_count = steering_count - 1;
      } while (steering_count != 0);
    }

    if ((unit_definition->flags & 0x400) == 0) {
      char anim_state_byte;
      anim_state_byte = unit->animation_state;
      if ((anim_state_byte < 0x17 ||
           (anim_state_byte > 0x23 && anim_state_byte != 0x29)) &&
          unit->animation_action == 0) {
        animation_graph_weapon_class_t *weapon_class;
        weapon_class = tag_block_get_element(
          &unit_seat->weapon_classes, (int8_t)unit->animation_weapon_index, 0xbc);

        relative_aiming_angles.yaw = GLOBAL_ZERO_ANGLES2D->yaw;
        relative_aiming_angles.pitch = GLOBAL_ZERO_ANGLES2D->pitch;

        if (unit->aiming_screen_index != -1) {
          aiming_vec = &unit->aiming_vector;
          valid = valid_real_normal3d(aiming_vec);
          if (valid == 0) {
            csprintf(
              ERROR_STRING_BUFFER, "%s: assert_valid_real_normal3d(%f, %f, %f)",
              "&unit->unit.aiming_vector", (double)aiming_vec->i,
              (double)unit->aiming_vector.j, (double)unit->aiming_vector.k);
            display_assert(0, "c:\\halo\\SOURCE\\units\\units.c", 0x670, 1);
            system_exit(-1);
          }

          matrix.scale = 1.0f;
          object_get_orientation(unit_handle, &matrix.forward, &matrix.up);
          /* left = up x forward: the original pushes up as the first operand. */
          cross_product3d(&matrix.up, &matrix.forward, &matrix.left);
          matrix.position.x = GLOBAL_ORIGIN3D->x;
          matrix.position.y = GLOBAL_ORIGIN3D->y;
          matrix.position.z = GLOBAL_ORIGIN3D->z;
          real_matrix4x3_transform_point(&matrix, aiming_vec,
                                         &relative_aiming_vector);

          valid = real_vector3d_valid(&relative_aiming_vector);
          if (valid == 0) {
            csprintf(ERROR_STRING_BUFFER,
                     "%s: assert_valid_real_vector2d(%f, %f, %f)",
                     "&relative_aiming_vector", (double)relative_aiming_vector.i,
                     (double)relative_aiming_vector.j,
                     (double)relative_aiming_vector.k);
            display_assert(0, "c:\\halo\\SOURCE\\units\\units.c", 0x67d, 1);
            system_exit(-1);
          }

          vector_to_angles(&relative_aiming_angles, &relative_aiming_vector);

          temp = relative_aiming_angles.pitch;
          if ((*(unsigned int *)&temp & 0x7f800000) == 0x7f800000) {
            csprintf(ERROR_STRING_BUFFER, "%s: assert_valid_real(0x%08X %f)",
                     "relative_aiming_angles.pitch", *(unsigned int *)&temp,
                     (double)temp);
            display_assert(0, "c:\\halo\\SOURCE\\units\\units.c", 0x680, 1);
            system_exit(-1);
          }
          temp = relative_aiming_angles.yaw;
          if ((*(unsigned int *)&temp & 0x7f800000) == 0x7f800000) {
            csprintf(ERROR_STRING_BUFFER, "%s: assert_valid_real(0x%08X %f)",
                     "relative_aiming_angles.yaw", *(unsigned int *)&temp,
                     (double)temp);
            display_assert(0, "c:\\halo\\SOURCE\\units\\units.c", 0x681, 1);
            system_exit(-1);
          }

          unit->aiming_with_euler_screen = 1;
          unit->unk_616 =
            -((float)(int)weapon_class->aiming_screen.yaw_frame_count[0] *
              weapon_class->aiming_screen.yaw_per_frame[0]);
          unit->unk_620 =
            (float)(int)weapon_class->aiming_screen.yaw_frame_count[1] *
            weapon_class->aiming_screen.yaw_per_frame[1];
          unit->unk_624 =
            -((float)(int)weapon_class->aiming_screen.pitch_frame_count[0] *
              weapon_class->aiming_screen.pitch_per_frame[0]);
          unit->unk_628 =
            (float)(int)weapon_class->aiming_screen.pitch_frame_count[1] *
            weapon_class->aiming_screen.pitch_per_frame[1];

          {
            int aim_overlay;
            aim_overlay =
              (int)tag_block_get_element(&animation_graph->animations,
                                         unit->aiming_screen_index, 0xb4);
            aiming_screen_apply(aim_overlay, weapon_class->aiming_screen.yaw_per_frame,
                         relative_aiming_angles.yaw, relative_aiming_angles.pitch,
                                node_output);
          }
        }

        if ((unit->current_weapon_index != -1 || unit->player_index.value != -1) &&
            unit->looking_screen_index != -1) {
          float *look_ptr;
          look_ptr = unit_seat->looking_screen.yaw_per_frame;
          matrix.scale = 1.0f;
          object_get_orientation(unit_handle, &matrix.forward, &matrix.up);
          cross_product3d(&matrix.up, &matrix.forward, &matrix.left);
          matrix.position.x = GLOBAL_ORIGIN3D->x;
          matrix.position.y = GLOBAL_ORIGIN3D->y;
          matrix.position.z = GLOBAL_ORIGIN3D->z;
          real_matrix4x3_transform_point(&matrix, &unit->looking_vector,
                                         &relative_looking_vector);

          valid = real_vector3d_valid(&relative_looking_vector);
          if (valid == 0) {
            csprintf(ERROR_STRING_BUFFER,
                     "%s: assert_valid_real_vector2d(%f, %f, %f)",
                     "&relative_looking_vector", (double)relative_looking_vector.i,
                     (double)relative_looking_vector.j,
                     (double)relative_looking_vector.k);
            display_assert(0, "c:\\halo\\SOURCE\\units\\units.c", 0x6a7, 1);
            system_exit(-1);
          }

          vector_to_angles(&relative_looking_angles, &relative_looking_vector);
          relative_looking_angles.yaw =
            relative_looking_angles.yaw - relative_aiming_angles.yaw;
          relative_looking_angles.pitch =
            relative_looking_angles.pitch - relative_aiming_angles.pitch;

          temp = relative_looking_angles.pitch;
          if ((*(unsigned int *)&temp & 0x7f800000) == 0x7f800000) {
            csprintf(ERROR_STRING_BUFFER, "%s: assert_valid_real(0x%08X %f)",
                     "relative_looking_angles.pitch", *(unsigned int *)&temp,
                     (double)temp);
            display_assert(0, "c:\\halo\\SOURCE\\units\\units.c", 0x6ae, 1);
            system_exit(-1);
          }
          temp = relative_looking_angles.yaw;
          if ((*(unsigned int *)&temp & 0x7f800000) == 0x7f800000) {
            csprintf(ERROR_STRING_BUFFER, "%s: assert_valid_real(0x%08X %f)",
                     "relative_looking_angles.yaw", *(unsigned int *)&temp,
                     (double)temp);
            display_assert(0, "c:\\halo\\SOURCE\\units\\units.c", 0x6af, 1);
            system_exit(-1);
          }

          unit->looking_with_euler_screen = 1;
          unit->unk_632 =
            -((float)(int)unit_seat->looking_screen.yaw_frame_count[0] * *look_ptr);
          unit->unk_636 = (float)(int)unit_seat->looking_screen.yaw_frame_count[1] *
                          unit_seat->looking_screen.yaw_per_frame[1];
          unit->unk_640 =
            -((float)(int)unit_seat->looking_screen.pitch_frame_count[0] *
              unit_seat->looking_screen.pitch_per_frame[0]);
          unit->unk_644 = (float)(int)unit_seat->looking_screen.pitch_frame_count[1] *
                          unit_seat->looking_screen.pitch_per_frame[1];

          {
            int look_overlay;
            look_overlay =
              (int)tag_block_get_element(&animation_graph->animations,
                                         unit->looking_screen_index, 0xb4);
            aiming_screen_apply(look_overlay, look_ptr, relative_looking_angles.yaw,
                                relative_looking_angles.pitch, node_output);
          }
        }
      }
    }
  }
}

/* unit_postprocess_node_matrices (0x1b04b0): seat and weapon IK. */
void unit_postprocess_node_matrices(int unit_handle, int node_matrices)
{
  unit_data_t *unit;
  unit_definition_t *unit_definition;
  animation_graph_t *animation_graph;
  animation_graph_unit_seat_t *unit_seat;
  animation_graph_weapon_class_t *weapon_class;
  int ik_point;
  int weapon_ik_point;
  unit_data_t *weapon_unit;
  int weapon_handle;
  short ik_index;
  unsigned char *anim_ctrl;
  char ik_active;
  char weapon_ik_active;
  tag_block *ik_block;
  tag_block *weapon_ik_block;
  int idx;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);

  if ((unit_definition->flags & 0x800) != 0) {
    return;
  }
  if ((int8_t)unit->animation_seat_index == -1) {
    return;
  }

  /* +0x44: the unit definition's animation_graph tag index. */
  animation_graph =
    tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_definition));
  unit_seat = tag_block_get_element(&animation_graph_definition_unit_seats(animation_graph),
                                    (int8_t)unit->animation_seat_index, 100);
  weapon_class = tag_block_get_element(
    &unit_seat->weapon_classes, (int8_t)unit->animation_weapon_index, 0xbc);

  if (unit->object.parent_object_index.value != -1) {
    ik_active = unit_animation_vehicle_ik(&unit->animation_flags);
    if (ik_active != '\0') {
      ik_block = &unit_seat->ik_points;
      ik_index = 0;
      if (0 < unit_seat->ik_points.count) {
        idx = 0;
        do {
          ik_point = (int)tag_block_get_element(ik_block, idx, 0x40);
          object_inverse_kinematics(unit_handle, ik_point,
                                    unit->object.parent_object_index.value,
                       ik_point + 0x20, node_matrices);
          ik_index = ik_index + 1;
          idx = (int)ik_index;
        } while (idx < ik_block->count);
      }
    }
  }

  if (unit->current_weapon_index != -1) {
    anim_ctrl = &unit->animation_flags;
    weapon_ik_active = unit_animation_weapon_ik((void *)anim_ctrl);
    if (weapon_ik_active != '\0') {
      weapon_ik_block = &weapon_class->ik_points;
      ik_index = 0;
      if (0 < weapon_class->ik_points.count) {
        idx = 0;
        do {
          weapon_ik_point =
            (int)tag_block_get_element(weapon_ik_block, idx, 0x40);
          weapon_unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
          weapon_handle = unit_inventory_get_weapon(
            unit_handle, weapon_unit->current_weapon_index);
          object_inverse_kinematics(unit_handle, weapon_ik_point, weapon_handle,
                       weapon_ik_point + 0x20, node_matrices);
          ik_index = ik_index + 1;
          idx = (int)ik_index;
        } while (idx < weapon_ik_block->count);
      }
      *anim_ctrl = *anim_ctrl & 0xfe;
    }
  }
}

/* unit_euler_aiming_update (0x1b0630) — euler aiming vector update
 *
 * Updates aiming/desired vectors with angular velocity constraints,
 * motion planning, and bounds clamping.
 * Confirmed: cdecl, 7 stack params.
 */
void unit_euler_aiming_update(int transform_matrix, real_vector3d *aiming_vector,
                  const real_vector3d *desired_vector, real_vector3d *angular_velocity,
                  float *aiming_bounds, float angular_velocity_limit,
                  float angular_acceleration_limit)
{
  real_vector3d local_aim;
  real_vector3d local_desired;
  real_vector3d desired_clamped;
  real_vector3d end_aim;
  real_euler_angles2d aim_angles;
  real_euler_angles2d desired_angles;
  real_euler_angles2d end_angles;
  float vel_yaw;
  float vel_pitch;
  real_vector3d vel_vec;
  real_vector3d rotated;
  real_euler_angles2d rot_angles;
  char plan_yaw[32];
  char plan_pitch[32];
  float cos_v;
  float sin_v;
  char wrapping;
  char yaw_ok;
  float norm_len;
  float delta_yaw;
  float delta_pitch;
  char pitch_ok;

  if (angular_velocity_limit < 0.0f) {
    display_assert("angular_velocity_limit >= 0.0f",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x962, 1);
    system_exit(-1);
  }
  if (angular_acceleration_limit <= 0.0f ||
      angular_acceleration_limit >= 10000.0f) {
    display_assert("(angular_acceleration_limit > 0.0f) && "
                   "(angular_acceleration_limit < 10000.0f)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x963, 1);
    system_exit(-1);
  }

  if (transform_matrix == 0) {
    local_aim.i = aiming_vector->i;
    local_aim.j = aiming_vector->j;
    local_aim.k = aiming_vector->k;
    local_desired.i = desired_vector->i;
    local_desired.j = desired_vector->j;
    local_desired.k = desired_vector->k;
  } else {
    real_matrix4x3_transform_point((void *)transform_matrix, aiming_vector,
                                   &local_aim);
    real_matrix4x3_transform_point((void *)transform_matrix, (void *)desired_vector,
                                   &local_desired);
  }

  wrapping = (aiming_bounds[1] - aiming_bounds[0]) - (2.0f * _pi) > -1.0e-4f;

  vector_to_angles(&aim_angles, &local_aim);
  vector_to_angles(&desired_angles, &local_desired);

  yaw_ok = 1;
  if (wrapping == 0) {
    if (desired_angles.yaw < aiming_bounds[0]) {
      desired_angles.yaw = aiming_bounds[0];
      yaw_ok = 0;
    } else if (desired_angles.yaw > aiming_bounds[1]) {
      desired_angles.yaw = aiming_bounds[1];
      yaw_ok = 0;
    }
  } else {
    if (desired_angles.yaw >= aiming_bounds[0]) {
      if (desired_angles.yaw > aiming_bounds[1]) {
        desired_angles.yaw = desired_angles.yaw - (2.0f * _pi);
        if (desired_angles.yaw < aiming_bounds[0]) {
          display_assert("desired_aiming_angles.yaw >= aiming_bounds->x0",
                         "c:\\halo\\SOURCE\\units\\units.c", 0x986, 1);
          system_exit(-1);
        }
      }
    } else {
      desired_angles.yaw = desired_angles.yaw + (2.0f * _pi);
      /* assert(yaw <= x1): fires when the +2pi wrap OVERSHOOT past x1, i.e.
         desired > x1 (symmetric with the -2pi path's `< x0` check). The Ghidra
         decompile inverted this branch (`<=`), which passes VC71 but trips a
         spurious halt at runtime on the common in-bounds case. */
      if (desired_angles.yaw > aiming_bounds[1]) {
        display_assert("desired_aiming_angles.yaw <= aiming_bounds->x1",
                       "c:\\halo\\SOURCE\\units\\units.c", 0x981, 1);
        system_exit(-1);
      }
    }
  }

  if (desired_angles.pitch < aiming_bounds[2]) {
    desired_angles.pitch = aiming_bounds[2];
    goto clamp_and_reconstruct;
  } else if (desired_angles.pitch > aiming_bounds[3]) {
    desired_angles.pitch = aiming_bounds[3];
    goto clamp_and_reconstruct;
  } else if (yaw_ok) {
    desired_clamped.i = desired_vector->i;
    desired_clamped.j = desired_vector->j;
    desired_clamped.k = desired_vector->k;
    goto done_clamp;
  }

clamp_and_reconstruct:
  angles_to_vector(&desired_clamped, &desired_angles);
  if (transform_matrix != 0) {
    matrix_transform_vector((float *)transform_matrix, &desired_clamped,
                            &desired_clamped);
    normalize3d(&desired_clamped);
  }

done_clamp:
  vel_vec.i = angular_velocity->i;
  vel_vec.j = angular_velocity->j;
  vel_vec.k = angular_velocity->k;
  norm_len = normalize3d(&vel_vec);
  if (norm_len == 0.0f) {
    vel_yaw = 0.0f;
    vel_pitch = 0.0f;
  } else {
    cos_v = x87_fcos(norm_len);
    rotated = local_aim;
    sin_v = x87_fsin(norm_len);
    rotate_vector3d_by_sincos(&rotated, &vel_vec, sin_v, cos_v);
    vector_to_angles(&rot_angles, &rotated);
    vel_yaw = rot_angles.yaw - aim_angles.yaw;
    vel_pitch = rot_angles.pitch - aim_angles.pitch;
  }

  delta_yaw = aim_angles.yaw - desired_angles.yaw;
  delta_pitch = aim_angles.pitch - desired_angles.pitch;

  if (wrapping != 0) {
    if (delta_yaw > _pi) {
      delta_yaw = delta_yaw - (2.0f * _pi);
    } else if (delta_yaw < -_pi) {
      delta_yaw = delta_yaw + (2.0f * _pi);
    }
  }

  unit_euler_axis_buildplan(delta_yaw, vel_yaw, angular_velocity_limit,
               angular_acceleration_limit, (int)plan_yaw);
  unit_euler_axis_buildplan(delta_pitch, vel_pitch, angular_velocity_limit,
               angular_acceleration_limit, (int)plan_pitch);
  unit_adjust_plan_overlap(plan_yaw, plan_pitch, angular_velocity_limit,
                           angular_acceleration_limit);

  pitch_ok = unit_euler_axis_doplan(plan_yaw, 1.0f, delta_yaw, &delta_yaw, vel_yaw,
                          &rot_angles.yaw);
  yaw_ok = unit_euler_axis_doplan(plan_pitch, 1.0f, delta_pitch, &delta_pitch, vel_pitch,
                        &rot_angles.pitch);

  if (pitch_ok == 0 || yaw_ok == 0) {
    end_angles.yaw = delta_yaw + desired_angles.yaw;
    end_angles.pitch = delta_pitch + desired_angles.pitch;

    if (wrapping == 0) {
      if (end_angles.yaw < aiming_bounds[0]) {
        end_angles.yaw = aiming_bounds[0];
      } else if (end_angles.yaw > aiming_bounds[1]) {
        end_angles.yaw = aiming_bounds[1];
      }
    } else {
      if (end_angles.yaw >= aiming_bounds[0]) {
        if (end_angles.yaw > aiming_bounds[1]) {
          end_angles.yaw = end_angles.yaw - (2.0f * _pi);
          if (end_angles.yaw < aiming_bounds[0]) {
            display_assert("end_aiming_angles.yaw >= aiming_bounds->x0",
                           "c:\\halo\\SOURCE\\units\\units.c", 0xaaa, 1);
            system_exit(-1);
          }
        }
      } else {
        end_angles.yaw = end_angles.yaw + (2.0f * _pi);
        /* assert(yaw <= x1): fires on +2pi wrap OVERSHOOT (end > x1), symmetric
           with the -2pi path's `< x0` check. Ghidra inverted this branch (`<=`)
           -> spurious EXCEPTION halt at units.c:0xaa5 at runtime (caught when
           0x1b0630 was reactivated on a10). */
        if (end_angles.yaw > aiming_bounds[1]) {
          display_assert("end_aiming_angles.yaw <= aiming_bounds->x1",
                         "c:\\halo\\SOURCE\\units\\units.c", 0xaa5, 1);
          system_exit(-1);
        }
      }
    }

    if (end_angles.pitch < aiming_bounds[2]) {
      end_angles.pitch = aiming_bounds[2];
    } else if (end_angles.pitch > aiming_bounds[3]) {
      end_angles.pitch = aiming_bounds[3];
    }

    if (end_angles.yaw < aiming_bounds[0] ||
        !(end_angles.yaw <= aiming_bounds[1])) {
      display_assert("(end_aiming_angles.yaw >= aiming_bounds->x0) && "
                     "(end_aiming_angles.yaw <= aiming_bounds->x1)",
                     "c:\\halo\\SOURCE\\units\\units.c", 0xab3, 1);
      system_exit(-1);
    }
    if (end_angles.pitch < aiming_bounds[2] ||
        !(end_angles.pitch <= aiming_bounds[3])) {
      display_assert("(end_aiming_angles.pitch >= aiming_bounds->y0) && "
                     "(end_aiming_angles.pitch <= aiming_bounds->y1)",
                     "c:\\halo\\SOURCE\\units\\units.c", 0xab4, 1);
      system_exit(-1);
    }

    angles_to_vector(&end_aim, &end_angles);

    {
      real_euler_angles2d total_angles;
      real_vector3d vel_end;
      float dot;
      float angle;

      total_angles.yaw = rot_angles.yaw + end_angles.yaw;
      total_angles.pitch = rot_angles.pitch + end_angles.pitch;
      angles_to_vector(&vel_end, &total_angles);

      /* dot = vel_end . end_aim. Read vel_end AFTER the call: angles_to_vector
         overwrote the stack slots the decompiler mislabeled rot_angles[1..2]
         (buffer aliasing). Original stores the dot, then clamps to [-1,1]. */
      dot = vel_end.i * end_aim.i + vel_end.j * end_aim.j +
            vel_end.k * end_aim.k;
      if (dot < -1.0f) {
        dot = -1.0f;
      } else if (dot > 1.0f) {
        dot = 1.0f;
      }

      /* angular_velocity = cross(end_aim, vel_end) */
      angular_velocity->i = end_aim.j * vel_end.k - end_aim.k * vel_end.j;
      angular_velocity->j = end_aim.k * vel_end.i - end_aim.i * vel_end.k;
      angular_velocity->k = end_aim.i * vel_end.j - end_aim.j * vel_end.i;

      normalize3d(angular_velocity);

      /* angle = acos(clamped dot) (orig 0x1d94f0 = MSVC CRT acos, arg in ST0),
         clamped to the angular-velocity limit, then scales the axis. */
      angle = acosf(dot);
      if (angle > angular_velocity_limit) {
        angle = angular_velocity_limit;
      }
      angular_velocity->i *= angle;
      angular_velocity->j *= angle;
      angular_velocity->k *= angle;
    }

    if (transform_matrix != 0) {
      matrix_transform_vector((float *)transform_matrix, &end_aim,
                              aiming_vector);
      normalize3d(aiming_vector);
      goto final_validate;
    }
    aiming_vector->i = end_aim.i;
    aiming_vector->j = end_aim.j;
    /* transform_matrix==0 reconstruct path: the original's shared "+8" store
       (delinked 0x668, reached via 0x657 with eax=aiming_vector) writes
       aiming_vector->k; angular_velocity->k retains its scaled cross-product
       value. The old single trailing store conflated the two LHS pointers. */
    aiming_vector->k = end_aim.k;
  } else {
    aiming_vector->i = desired_clamped.i;
    aiming_vector->j = desired_clamped.j;
    aiming_vector->k = desired_clamped.k;
    {
      real_vector3d *zero_vec;
      zero_vec = GLOBAL_ZERO_VECTOR3D;
      angular_velocity->i = zero_vec->i;
      angular_velocity->j = zero_vec->j;
      /* else path: the shared "+8" store targets angular_velocity here
         (eax=angular_velocity via 0x3b4 -> jmp 0x668). */
      angular_velocity->k = zero_vec->k;
    }
  }

final_validate: {
  char aim_valid;
  aim_valid = valid_real_normal3d(aiming_vector);
  if (aim_valid == 0) {
    csprintf(ERROR_STRING_BUFFER, "%s: assert_valid_real_normal3d(%f, %f, %f)",
             "aiming_vector", (double)aiming_vector->i,
             (double)aiming_vector->j, (double)aiming_vector->k);
    display_assert(0, "c:\\halo\\SOURCE\\units\\units.c", 0xae9, 1);
    system_exit(-1);
  }
}
}

/* unit_has_animation_to_enter_seat (0x1b0d00)
 * Returns true if the unit has an animation to enter the given seat. */
char unit_has_animation_to_enter_seat(int unit_handle, int vehicle_handle,
                                      int16_t seat_index)
{
  unit_data_t *vehicle;
  unit_definition_t *vehicle_definition;
  unit_data_t *unit;
  char *seat;

  vehicle = object_get_and_verify_type(vehicle_handle, _object_mask_unit);
  vehicle_definition = tag_get(TAG_GROUP_UNIT, vehicle->object.tag_index);
  if (seat_index >= 0) {
    if ((int)seat_index < vehicle_definition->seats.count) {
      unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
      if (unit->object.type != _object_type_vehicle) {
        seat = tag_block_get_element(&vehicle_definition->seats, seat_index,
                                     sizeof(unit_seat_t));
        /* +0x04: the seat's label string. */
        if (!unit_set_or_test_seat_and_weapon_label(unit_handle, seat + 4, 0, 0)) {
          return 0;
        }
      }
      return 1;
    }
  }
  return 0;
}

/* unit_update_animation (0x1b0d90) — animation state update
 *
 * Evaluates the current unit animation state and applies transitions.
 * Confirmed: cdecl, 2 stack params.
 * Returns: int16_t (animation flags bitmask).
 */
short unit_update_animation(int unit_handle, char *anim_state)
{
  unit_data_t *unit;
  unit_definition_t *unit_tag_data;
  short desired_state;
  short base_seat;
  short anim_status;
  unsigned short result;
  char apply_flag;
  int global_seat; /* 0x1b0e74: dword load, word comparisons */
  char unit_anim_byte;
  unit_data_t *vehicle;
  int vehicle_tag;
  unsigned char *seat_element;
  int mode_tag;
  int anim_graph;
  int anim_element;
  real_vector3d delta;
  char out_matrix[52];
  biped_data_t *biped;
  biped_definition_t *biped_definition;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag_data = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  desired_state = (short)*anim_state;
  result = 0;
  apply_flag = 0;

  if (desired_state < 0 || desired_state >= NUMBER_OF_UNIT_STATES) {
    display_assert("desired_state>=0 && desired_state<NUMBER_OF_UNIT_STATES",
                   "c:\\halo\\SOURCE\\units\\units.c", 0xb61, 1);
    system_exit(-1);
  }

  base_seat = -1;
  if (unit->object.parent_object_index.value == -1 &&
      (unit->object.damage_flags & 4) == 0) {
    switch ((int8_t)unit->desired_animation_state) {
    case 0:
      base_seat = 0;
      break;
    case 1:
    case 2:
      base_seat = 1;
      break;
    case 3:
      base_seat = (anim_state[1] != 0) + 2;
      if (base_seat == -1)
        goto seat_assert;
      break;
    case 4:
      base_seat = 2;
      break;
    case 5:
      base_seat = 4;
      break;
    case 6:
      base_seat = 5;
      break;
    default:
    seat_assert:
      display_assert("desired_base_seat_index!=NONE",
                     "c:\\halo\\SOURCE\\units\\units.c", 0xb73, 1);
      system_exit(-1);
      break;
    }

    if (unit->player_index.value != -1) {
      global_seat = MAGIC_BASE_ANIMATION_SEAT_INDEX_DWORD;
      if ((short)global_seat != -1) {
        if ((short)global_seat < 0) {
          base_seat = 0;
        } else {
          /* base_seat = MIN(global_seat, 6), as a ternary so the compare
           * and branch order follow the reference. */
          base_seat =
            ((short)global_seat > 6) ? 6 : (short)global_seat;
        }
      }
    }

    if ((char)unit->magic_seat_index != -1) {
      base_seat = (char)unit->magic_seat_index;
    }
    if ((unit->control_flags & 0x200) != 0) {
      base_seat = 1;
    }
    if (unit->flaming_death_delay > 0) {
      base_seat = 5;
    }

    if ((int8_t)unit->base_seat_index != base_seat) {
      char can_change;
      can_change = unit_animation_state_interruptable(&unit->animation_flags, desired_state);
      if (can_change != 0) {
        char *weapon_name;
        const char *seat_label;
        weapon_name = unit_get_weapon_name(unit_handle);
        seat_label = base_seat_label_get(base_seat);
        unit_set_or_test_seat_and_weapon_label(unit_handle, seat_label, weapon_name, 1);
      }
    }
  }

  if (unit->soft_ping_animation_index != -1) {
    anim_status = unit_animation_update(&unit->soft_ping_animation_index,
                               object_definition_animation_graph_index(unit_tag_data), unit_handle);
    if (anim_status == 2) {
      unit->soft_ping_animation_index = -1;
    }
  }

  if (unit->object.unk_128 != -1) {
    anim_status = unit_animation_update(&unit->object.unk_128,
                                        (int)unit->object.unk_124, unit_handle);
    if (anim_status == 1) {
      unit_anim_byte = unit->animation_state;
      switch (unit_anim_byte) {
      case 0x21:
        unit_throw_grenade_release(unit_handle, 0);
        break;
      case 0x1e:
      case 0x1f:
      case 0x29:
        unit_cause_melee_damage(unit_handle, 0, -1, -1, -1, -1, (float *)0);
        break;
      default:
        break;
      }
    } else if (anim_status == 2) {
      unit_anim_byte = unit->animation_state;
      switch (unit_anim_byte) {
      case 0x27:
        result = 1;
        desired_state = 0x28;
        break;

      case 0x19:
        if ((unit_tag_data->flags & 2) != 0) {
          if ((*(unsigned char *)&unit->object.flags & 0x20) != 0) {
            goto destroy_unit;
          }
          if (unit->object.type != 0) {
            goto set_garbage_flag;
          }
          biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
          biped_definition =
            tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);
          if ((*(unsigned char *)&biped->flags & 1) == 0 ||
              (biped_definition->biped_flags & 0x400) != 0) {
          destroy_unit:
            unit_destroy(unit_handle);
            break;
          }
        }
        if (unit->object.type == 0) {
          biped_start_limp_body_physics(unit_handle);
        }
      set_garbage_flag:
        unit->animation_flags = unit->animation_flags | 4;
        unit->object.unk_130 = unit->object.unk_130 - 1;
        break;

      case _unit_state_exiting_seat:
        mode_tag =
          (int)tag_get(TAG_GROUP_MODE, (unsigned int)object_definition_model_index(unit_tag_data));
        anim_graph = (int)tag_get(TAG_GROUP_ANTR, unit->object.unk_124);
        anim_element = (int)tag_block_get_element(
          &((animation_graph_t *)anim_graph)->animations, unit->object.unk_128,
          0xb4);
        animation_get_root_velocity((void *)mode_tag, (void *)anim_element,
                     (unsigned short)unit->object.unk_130, &delta);
        matrix_scale_transform_vector(
          (float *)object_get_world_matrix(unit_handle, out_matrix), &delta, &delta);
        unit_exit_seat_end(unit_handle);
        vector3d_add(&unit->object.translational_velocity, &delta,
                     &unit->object.translational_velocity);
        break;

      case 0x25:
      case 0x26:
        unit->object.unk_130 = unit->object.unk_130 - 1;
        break;

      case _unit_state_entering_seat:
        vehicle = object_get_and_verify_type(unit->object.parent_object_index.value,
                                             _object_mask_unit);
        vehicle_tag = (int)tag_get(TAG_GROUP_UNIT, vehicle->object.tag_index);
        seat_element = tag_block_get_element(
          &((unit_definition_t *)vehicle_tag)->seats,
          unit->parent_seat_index, sizeof(unit_seat_t));
        object_set_visibility(unit_handle, (~*seat_element) & 1);
        if (vehicle->driver_object_index.value == unit_handle) {
          unit_close(unit->object.parent_object_index.value);
        }
        break;

      default:
        break;
      }

      {
        char anim_ok;
        anim_ok = unit_animation_state_loops(&unit->animation_flags);
        if (anim_ok == 0) {
          apply_flag = 1;
        }
      }
    }
  }

  if ((int16_t)unit->action_animation_index != -1) {
    anim_status = unit_animation_update(&unit->action_animation_index,
                               object_definition_animation_graph_index(unit_tag_data), unit_handle);
    if (anim_status == 2) {
      unit_data_t *refreshed_unit;
      object_set_region_count(unit_handle, 6);
      refreshed_unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
      refreshed_unit->animation_action = 0;
      *(int16_t *)&refreshed_unit->action_animation_index = -1;
    }
  }

  if ((int16_t)unit->overlay_action_animation_index != -1) {
    int anim_status_wide;
    anim_status_wide = unit_animation_update(&unit->overlay_action_animation_index,
                               object_definition_animation_graph_index(unit_tag_data), unit_handle);
    switch (anim_status_wide) {
    case 2:
    case 4:
      unit_anim_byte = unit->animation_state;
      if (unit_anim_byte < 3 || unit_anim_byte > 4) {
        unit->overlay_action = 0;
        *(int16_t *)&unit->overlay_action_animation_index = -1;
      }
      break;
    }
  }

  if (apply_flag == 0) {
    if ((short)desired_state == (short)(char)unit->animation_state) {
      goto done;
    }
    {
      char can_apply;
      can_apply = unit_animation_state_interruptable(&unit->animation_flags, desired_state);
      if (can_apply == 0)
        goto done;
    }
  }
  unit_animation_set_state(unit_handle, desired_state);

done:
  return (short)(result & 0xffff);
}

/* unit_reset_weapon_state (0x1b1290)
 *
 * Resets the unit's weapon zoom/ready state. If the unit has an associated
 * player and that player is valid, and the unit's zoom_level is not 0xFF,
 * retrieves the unit's current weapon and plays its zoom-deactivation sound
 * (weapon tag +0x4bc) at scale 1.0. Then clears zoom_level and unk_721 to
 * 0xFF and zeroes unk_760. Finally calls player_control_unzoom.
 */
void unit_reset_weapon_state(int unit_handle)
{
  unit_data_t *unit;
  int player_index;
  player_data_t *player;
  int weapon_handle;
  weapon_data_t *weapon;
  void *weapon_tag;
  int sound_tag_index;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  player_index = player_index_from_unit_index(unit_handle);
  if (player_index != -1) {
    player = datum_get(player_data, player_index_from_unit_index(unit_handle));
    if (player->local_player_index != -1 && unit->current_zoom_level != 0xFF) {
      unit_data_t *unit2 =
        object_get_and_verify_type(unit_handle, _object_mask_unit);
      weapon_handle = unit_inventory_get_weapon(unit_handle, unit2->current_weapon_index);
      if (weapon_handle != -1) {
        weapon = object_get_and_verify_type(weapon_handle, 4);
        weapon_tag = tag_get(TAG_GROUP_WEAP, weapon->item.object.tag_index);
        sound_tag_index = weapon_definition_zoom_out_sound_index(weapon_tag);
        if (sound_tag_index != -1) {
          sound_impulse_start(sound_tag_index, 1.0f);
        }
      }
    }
  }
  unit->current_zoom_level = 0xFF;
  unit->desired_zoom_level = 0xFF;
  unit->integrated_night_vision_power = 0;
  player_control_unzoom(unit_handle);
}

/* unit_get_zoom_magnification (0x1b1350)
 * Returns the zoom magnification for the current weapon. */
float unit_get_zoom_magnification(int unit_handle, int zoom_level)
{
  unit_data_t *unit;
  int weapon;
  float result;

  result = 1.0f;
  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  weapon = unit_inventory_get_weapon(unit_handle, unit->current_weapon_index);
  if (weapon != -1) {
    result = weapon_get_zoom_magnification(weapon, zoom_level);
  }
  return result;
}

/* unit_has_night_vision_weapon (0x1b13a0)
 * Checks whether the unit's current weapon has the night-vision flag
 * (bit 0x4000 at weapon tag offset 0x308). Returns true if zoom_level
 * is not 0xFF, a weapon exists, and the flag is set.
 * Register arg: unit_handle in ESI. */
char unit_has_night_vision_weapon(int unit_handle)
{
  unit_data_t *unit;
  int weapon_handle;
  object_data_t *weapon;
  char *weapon_definition;
  char active;

  active = 0;
  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if (unit->current_zoom_level != 0xff) {
    unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
    weapon_handle =
      unit_inventory_get_weapon(unit_handle, unit->current_weapon_index);
    if (weapon_handle != -1) {
      weapon = object_get_and_verify_type(weapon_handle, 4);
      weapon_definition = tag_get(TAG_GROUP_WEAP, weapon->tag_index);
      /* Weapon definition flags bit 14: night vision. */
      if (weapon_definition_flags(weapon_definition) & 0x4000)
        active = 1;
    }
  }

  return active;
}

/* unit_ping_animation (0x1b1400) — animation impulse
 *
 * Selects movement or attack animation based on unit state.
 * Confirmed: cdecl, 9 stack params.
 */
void unit_ping_animation(int unit_handle, char is_melee, char is_throw,
                  char is_airborne, char is_ground, char is_ping,
                  float throttle_magnitude, int weapon_class,
                  int alignment_vector)
{
  unit_data_t *unit;
  unit_definition_t *unit_tag;
  int anim_graph;
  char moving;
  char fast_moving;
  short throttle_dir;
  char engine_running;
  short anim_idx;
  short chosen_anim;
  int anim_entry_idx; /* 0x1b15ef: movsx ecx,word / or ecx,-1 */
  int anim_state; /* 0x1b1635: dword slot -0xc; cmpw/movb reads */
  int movement_type; /* 0x1b164b: dword slot -0x14 */
  char must_apply;
  biped_data_t *biped;
  biped_definition_t *biped_definition;
  animation_t *animation;
  short frame_count;
  short random_frame;
  real_vector2d align_out;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);

  if (is_melee != 0) {
    is_throw = 0;
    moving = 1;
    /* 0x1b143c/0x1b1457: fcomp; test $0x41; jne -- jump-when-false of a
     * strict '>' (jne, not the jnp a '<=' spelling gives). */
    if (unit_definition_hard_death_threshold(unit_tag) > 0.0f &&
        unit->object.unk_156 > unit_definition_hard_death_threshold(unit_tag)) {
      fast_moving = 1;
      goto check_ping;
    } else {
      fast_moving = 0;
    }
  } else if (is_throw != 0) {
    is_melee = 1;
    moving = 1;
  } else {
    /* 0x1b147b/0x1b148e: flds 0x9c/0x98(%ebx); fcomps 0x218; test $0x41 -- strict
     * '>' on FLOAT fields (an int-typed read here FILDs the float bits). */
    if (unit->object.unk_156 > unit_definition_soft_ping_threshold(unit_tag) ||
        unit->object.unk_152 > unit_definition_soft_ping_threshold(unit_tag)) {
      moving = 1;
    } else {
      moving = 0;
    }
    /* 0x1b14a9: flds 0x9c(%ebx); fcomps 0x220; test $0x41; je -- strict '>'. */
    fast_moving = unit->object.unk_156 > unit_definition_hard_ping_threshold(unit_tag);

    if (is_ground == 0 && *(char *)&unit->flags >= 0) {
      goto check_ping;
    }
  }
  fast_moving = 0;

check_ping:
  if (is_ping != 0) {
    fast_moving = 1;
    moving = 1;
  }

  if ((short)weapon_class == -1) {
    weapon_class = 0;
  }

  {
    float abs_throttle;
    /* 0x1b14f2: fld [ebp+0x20]; fabs -- the inline FABS intrinsic, not a
     * compare-and-negate. */
    abs_throttle = (float)fabs(throttle_magnitude);

    if ((double)abs_throttle < REAL_QUARTER_PI_DOUBLE_POOL) {
      throttle_dir = 3;
    } else if ((double)abs_throttle > REAL_2_159845_DOUBLE_POOL) {
      throttle_dir = 0;
    } else {
      throttle_dir = 1;
      /* 0x1b1530: fcomp 0.0; test $0x41; je -- skip-when-'>' (je), not the
       * jp a '<=' spelling gives. */
      if (!(throttle_magnitude > 0.0f)) {
        throttle_dir = 2;
      }
    }
  }

  engine_running = game_engine_running();
  if (engine_running != 0 && (short)weapon_class == 2 && fast_moving &&
      is_melee != 0) {
    throttle_dir = 1;
  }

  if (!moving && is_melee == 0) {
    return;
  }

  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  anim_graph = (int)tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_tag));

  if (!fast_moving && is_melee == 0) {
    if (unit->soft_ping_animation_index != -1 &&
        unit->soft_ping_animation_frame <= unit_definition_runtime_soft_ping_minimum_interrupt_ticks(unit_tag)) {
      return;
    }
    anim_idx = build_damage_animation_index(0, throttle_dir, weapon_class);
    if (anim_idx < 0 || anim_idx >= animation_graph_definition_field_3c(anim_graph).count) {
      anim_entry_idx = -1;
    } else {
      anim_entry_idx = *(short *)((int)animation_graph_definition_field_3c(anim_graph).address + anim_idx * 2);
    }
    chosen_anim = (short)model_animation_choose_random(
      1, object_definition_animation_graph_index(unit_tag), anim_entry_idx);
    if (chosen_anim == -1) {
      return;
    }
    unit->soft_ping_animation_index = chosen_anim;
    unit->soft_ping_animation_frame = 0;
    return;
  }

  anim_state = (is_melee != 0) * 2 + 0x17;

  if (is_melee == 0) {
    movement_type = 1;
    {
      char can_change;
      can_change = unit_animation_state_interruptable(&unit->animation_flags, (short)anim_state);
      if (can_change != 0)
        goto force_apply;
    }
    must_apply = 0;
  } else {
    movement_type = (fast_moving != 0) + 2; /* 0x1b1645: setne dl; add edx,2 */
  force_apply:
    must_apply = 1;
  }

  if ((char)unit->animation_state == 0x17 &&
      unit->object.unk_130 > unit_definition_runtime_hard_ping_minimum_interrupt_ticks(unit_tag)) {
    must_apply = 1;
  }

  if (is_melee == 0) {
    if ((unit->object.damage_flags & 4) != 0) {
      must_apply = 0;
    }
    if (unit->object.parent_object_index.value != -1) {
      return;
    }
  }

  if (!must_apply) {
    return;
  }

  if (is_melee != 0) {
    char *weapon_name;
    weapon_name = unit_get_weapon_name(unit_handle);
    unit_set_or_test_seat_and_weapon_label(unit_handle, BASE_SEAT_LABEL_STAND, weapon_name, 0);
  }

  if ((short)anim_state == 0x19 && unit->object.type == _object_type_biped) {
    biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
    biped_definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);
    if ((*(unsigned char *)&biped->flags & 1) != 0 &&
        (biped_definition->biped_flags & 0x400) == 0) {
      char transition_ok;
      anim_state = 0x18;
      transition_ok = unit_animation_set_state(unit_handle, 0x18);
      if (transition_ok != 0)
        goto alignment_section;
    }
  }

  anim_idx = build_damage_animation_index(movement_type, throttle_dir, weapon_class);
  if (anim_idx < 0 || anim_idx >= animation_graph_definition_field_3c(anim_graph).count) {
    anim_entry_idx = -1;
  } else {
    anim_entry_idx = *(short *)((int)animation_graph_definition_field_3c(anim_graph).address + anim_idx * 2);
  }

  chosen_anim = model_animation_choose_random(
    1, object_definition_animation_graph_index(unit_tag), anim_entry_idx);

  if (chosen_anim == -1) {
    if (is_melee != 0) {
      unsigned short flags;
      flags = *(unsigned short *)&unit->animation_flags;
      flags = (flags & 0xfff7) | 4;
      *(unsigned short *)&unit->animation_flags = flags;
      if ((unit_tag->flags & 2) != 0) {
        unit_destroy(unit_handle);
      }
    }
  } else {
    if ((char)unit->animation_state == 0x21) {
      unit_throw_grenade_release(unit_handle, 1);
    }
    object_set_region_count(unit_handle, 3);
    unit->animation_state = (uint8_t)anim_state;
    unit_set_animation(unit_handle, object_definition_animation_graph_index(unit_tag),
                       chosen_anim);
    unit->animation_flags = unit->animation_flags | 1;

    if (is_melee != 0) {
      if (is_airborne == 0 && is_throw == 0) {
        short quarter;
        short half;
        animation = tag_block_get_element(
          &((animation_graph_t *)anim_graph)->animations, chosen_anim, 0xb4);
        frame_count = animation->frame_count;
        quarter = frame_count >> 2;
        half = (frame_count >> 1) + quarter;
        random_frame = random_range(quarter, half);
        unit->weapon_drop_delay_ticks = (uint8_t)random_frame;
        if ((char)random_frame < 2) {
          unit->weapon_drop_delay_ticks = 1;
        }
      } else {
        unit->weapon_drop_delay_ticks = 0;
      }
    }

    if (throttle_dir != 0) {
      short standing_ref;
      animation = tag_block_get_element(
        &((animation_graph_t *)anim_graph)->animations, chosen_anim, 0xb4);
      /* 0x1b185d/0x1b186e/0x1b1884: the original evaluates the index helper
       * three times (inlined accessor macro); the callee is pure. */
      if (build_damage_animation_index(movement_type, 0, weapon_class) < 0 ||
          build_damage_animation_index(movement_type, 0, weapon_class) >=
            animation_graph_definition_field_3c(anim_graph).count) {
        standing_ref = -1;
      } else {
        standing_ref = *(short *)((int)animation_graph_definition_field_3c(anim_graph).address +
          build_damage_animation_index(movement_type, 0, weapon_class) * 2);
      }
      if ((short)animation->field_42 == standing_ref) {
        throttle_dir = 0;
      }
    }

    if (is_melee != 0) {
      if (throttle_dir == 3) {
        unit->animation_flags = unit->animation_flags | 8;
      } else {
        unit->animation_flags = unit->animation_flags & 0xf7;
      }
    }
  }

alignment_section:
  if (alignment_vector != 0 &&
      (unit_tag->flags & 0x200) == 0 &&
      unit->object.type == _object_type_biped &&
      unit->object.parent_object_index.value == -1 &&
      (fast_moving || is_melee != 0)) {
    float *avec;
    avec = (float *)alignment_vector;
    switch (throttle_dir) {
    case 0:
      align_out.i = -avec[0];
      align_out.j = -avec[1];
      unit_apply_alignment_vector(unit_handle, &align_out);
      return;
    case 1:
      align_out.i = -avec[1];
      align_out.j = avec[0];
      unit_apply_alignment_vector(unit_handle, &align_out);
      return;
    case 2:
      align_out.i = avec[1];
      align_out.j = -avec[0];
      unit_apply_alignment_vector(unit_handle, &align_out);
      return;
    case 3:
      align_out.i = avec[0];
      align_out.j = avec[1];
      unit_apply_alignment_vector(unit_handle, &align_out);
      return;
    default:
      display_assert(0, "c:\\halo\\SOURCE\\units\\units.c", 0x11d2, 1);
      system_exit(-1);
      /* 0x1b1a04: unreachable after system_exit; the original calls with
       * align_out unassigned, and assigning here lets cl.exe tail-merge this
       * arm with case 3 (dropped call in the shape audit). */
      unit_apply_alignment_vector(unit_handle, &align_out);
      return;
    }
  }
}

enum {
  _unit_seat_animation_seat_enter = 7,
  _unit_seat_animation_seat_exit = 8
};

/* unit_apply_animation_impulse (0x1b1a20)
 * Plays the animation an impulse maps to in the unit's current seat/weapon
 * animation set; returns false when the state forbids it or none exists. */
bool unit_apply_animation_impulse(int unit_handle, int anim_index,
                                  void *anim_data)
{
  unit_data_t *unit;
  char *unit_tag;
  char *antr_tag;
  animation_graph_unit_seat_t *unit_seat;
  animation_graph_weapon_class_t *weapon_class;
  int16_t kind_anim_index;
  int16_t update_kind;
  int16_t chosen_anim;
  int antr_tag_index;
  char result;

  result = 0;
  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  /* 2276 stores result=0 at [ebp-1] and the fail epilogue reloads it. */
  if (unit_animation_state_allows_impulse(unit_handle, anim_index)) {
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  antr_tag = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_tag));

  /* antr +0x0c: unit seats block. */
  unit_seat = tag_block_get_element(&animation_graph_definition_unit_seats(antr_tag),
                                    (int8_t)unit->animation_seat_index,
                                    sizeof(animation_graph_unit_seat_t));
  weapon_class = tag_block_get_element(&unit_seat->weapon_classes,
                                       (int8_t)unit->animation_weapon_index,
                                       sizeof(animation_graph_weapon_class_t));

  /* Writes update_kind (3 or 6) through the pointer. */
  kind_anim_index =
    unit_impulse_to_animation_kind((int16_t)anim_index, &update_kind);

  if (kind_anim_index >= 0 &&
      (int)kind_anim_index < weapon_class->animations.count) {
    kind_anim_index =
      ((int16_t *)weapon_class->animations.address)[kind_anim_index];
    if (kind_anim_index != (int16_t)-1) {
      object_set_region_count(unit_handle, update_kind);

      antr_tag_index = object_definition_animation_graph_index(unit_tag);
      chosen_anim = (int16_t)model_animation_choose_random(
        1, antr_tag_index, kind_anim_index);

      unit_set_animation(unit_handle, antr_tag_index, chosen_anim);

      unit->animation_flags |= 0x1;
      unit->animation_state = 0x1d;

      if (anim_data != NULL && unit->object.type == _object_type_biped &&
          unit->object.parent_object_index.value == -1) {
        unit_apply_alignment_vector(unit_handle, (real_vector2d *)anim_data);
      }

      result = 1;
    }
  }
  }

  return result;
}

/* unit_inventory_next_weapon (0x1b1b40)
 * Advances to the next weapon in the inventory. */
int unit_inventory_next_weapon(int unit_handle, int slot, int direction)
{
  return unit_weapon_next_index(unit_handle, slot, direction);
}

/* unit_melee_attack_begin (0x1b1b60)
 * Starts a melee animation unless the current animation state forbids it.
 * alignment_vector is an optional float[2] pointer passed as an int. */
char unit_melee_attack_begin(int unit_handle, char force_hit, int alignment_vector)
{
  unit_data_t *unit;
  unit_definition_t *unit_definition;
  char anim_state;
  char result;
  uint8_t melee_ready;
  char anim_ok;
  int16_t new_state;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  result = 0;

  switch (unit->animation_state) {
  case 0x17:
  case 0x18:
  case 0x19:
  case _unit_state_entering_seat:
  case _unit_state_exiting_seat:
  case 0x1d:
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x27:
  case 0x29:
    break;
  default:
    melee_ready = 0;
    if (unit->object.type == _object_type_biped) {
      melee_ready = *(uint8_t *)&((biped_data_t *)unit)->flags & 1;
    }

    if (force_hit != '\0') {
      new_state = 0x20;
    } else {
      anim_state = (char)unit->animation_state;
      if (anim_state == 0x28) {
        new_state = 0x29;
      } else {
        new_state = (int16_t)((melee_ready != 0) + 0x1e);
      }
    }

    anim_ok = unit_animation_set_state(unit_handle, new_state);

    if (anim_ok != '\0' || force_hit != '\0') {
      if ((unit_definition->flags & 0x100) != 0) {
        unit->animation_state = 0x19;
      }
      if (alignment_vector != 0) {
        unit_apply_alignment_vector(unit_handle, (real_vector2d *)alignment_vector);
      }
      if (force_hit != '\0') {
        unit->melee_attack_state = 4;
        unit->melee_continuous_damage_effect_timer = 0;
        return 1;
      }
      unit->melee_attack_state = 1;
      result = 1;
    }
    break;
  }
  return result;
}

/* unit_leap_begin (0x1b1c70)
 * Starts the leap animation unless the current state forbids it. */
char unit_leap_begin(int unit_handle, float *alignment_vector)
{
  unit_data_t *unit;
  char result;
  char biped_limping;
  char state;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  state = (char)unit->animation_state;
  result = 0;

  switch (state) {
  case 0x17:
  case 0x18:
  case 0x19:
  case _unit_state_entering_seat:
  case _unit_state_exiting_seat:
  case 0x1d:
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x27:
  case 0x29:
    break;
  default:
    biped_limping = 0;
    if (unit->object.type == _object_type_biped)
      biped_limping = (char)(*(uint8_t *)&((biped_data_t *)unit)->flags & 1);
    if (!biped_limping && unit_animation_set_state(unit_handle, 0x27)) {
      if (alignment_vector != 0)
        unit_apply_alignment_vector(unit_handle, (real_vector2d *)alignment_vector);
      result = 1;
    }
    break;
  }

  return result;
}

/* unit_unsuspecting (0x1b1d00)
 * True when a biped unit without the definition flag 0x10000 faces away from
 * position, or sits in the "asleep" seat. */
char unit_unsuspecting(int object_handle, void *position)
{
  unit_data_t *unit;
  unit_definition_t *unit_definition;
  float dx;
  float dy;
  float dz;
  float dot;
  real_point3d *attacker_position;

  unit = object_try_and_get_and_verify_type(object_handle, _object_mask_unit);
  if (unit != NULL && unit->object.type == _object_type_biped) {
    unit_definition = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
    if ((unit_definition->flags & 0x10000) == 0) {
      attacker_position = position;
      dx = unit->object.bounding_sphere_center.x - attacker_position->x;
      dy = unit->object.bounding_sphere_center.y - attacker_position->y;
      dz = unit->object.bounding_sphere_center.z - attacker_position->z;
      dot = dx * unit->looking_vector.i + dy * unit->looking_vector.j + dz * unit->looking_vector.k;

      if (dot > 0.f)
        return 1;

      /* 0x32e484: the "asleep" seat label. */
      if (csstrcmp(BASE_SEAT_LABEL_ASLEEP,
                   (const char *)unit_get_seat_label(object_handle)) == 0)
        return 1;
    }
  }

  return 0;
}

/* unit_enter_seat (0x1b1db0)
 *
 * Puts an unparented item (object type mask 4, object flag 0x800) into the
 * unit's weapon inventory.  flag 2 clears the inventory first.  On success
 * the item is hidden, stored in a free weapon slot, and the desired weapon
 * becomes: flag 0, the next weapon after the current one; flag 1, the new
 * slot (also telling player control unless control_flags bit 0x800 is set);
 * flag 2, the new slot.
 */
bool unit_enter_seat(int unit_handle, int seat_object_handle, int16_t flag)
{
  object_data_t *seat_obj;
  unit_data_t *unit;
  int16_t seat_index;

  seat_obj = object_get_and_verify_type(seat_object_handle, 4);
  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if (!(seat_obj->flags & 0x800))
    return false;
  if (seat_obj->parent_object_index.value != -1)
    return false;
  if (!unit_can_enter_seat(unit_handle, seat_object_handle))
    return false;
  if (!game_engine_unit_can_enter_seat(unit_handle, seat_object_handle))
    return false;

  if (flag == 2)
    unit_clear_weapons(unit_handle);

  seat_index = unit_find_empty_weapon_slot(unit_handle);

  if (seat_index == -1)
    return false;

  object_disconnect_from_map(seat_object_handle);
  object_set_visibility(seat_object_handle, 0);
  item_in_unit_inventory(seat_object_handle, unit_handle);

  unit->weapon_object_indices[(int16_t)seat_index].value = seat_object_handle;
  unit->weapon_last_used_at_game_time[(int16_t)seat_index].value = 0;

  switch (flag) {
  case 0:
    unit->desired_weapon_index = unit_next_weapon_index(unit_handle, unit->current_weapon_index, 0);
    return true;
  case 1:
    if (!(unit->control_flags & 0x800))
      player_control_set_desired_weapon(unit_handle, seat_index);
    /* fall through */
  case 2:
    unit->desired_weapon_index = seat_index;
    return true;
  default:
    return true;
  }
}

/* unit_update_weapon_readiness (0x1b1ee0)
 *
 * Puts the current weapon away (weapon_put_away) and, once the unit holds
 * nothing, readies the desired weapon: picks the seat/weapon animation
 * state, attaches the weapon at the animation's markers and stamps
 * weapon_last_used_at_game_time.  With no desired weapon the unit goes
 * "unarmed".
 *
 * Register arg: unit_handle passed in ESI.
 */
void unit_update_weapon_readiness(int unit_handle, int flag)
{
  unit_data_t *unit;
  char *unit_tag;
  int next_weapon_handle;
  int cur_weapon_handle;
  int seat_label;
  int weapon_label;
  char *animation_graph;
  char *mode;
  char *weapon_animations;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);

  {
    unit_data_t *u2 = object_get_and_verify_type(unit_handle, _object_mask_unit);
    uint16_t next_idx = u2->desired_weapon_index;
    next_weapon_handle = unit_inventory_get_weapon(unit_handle, (int16_t)next_idx);
  }

  {
    unit_data_t *u3 = object_get_and_verify_type(unit_handle, _object_mask_unit);
    uint16_t cur_idx = u3->current_weapon_index;
    cur_weapon_handle = unit_inventory_get_weapon(unit_handle, (int16_t)cur_idx);
  }

  /* Stow the current weapon. */
  if (cur_weapon_handle != NONE) {
    if (weapon_put_away(cur_weapon_handle, flag)) {
      object_detach_from_parent(cur_weapon_handle);
      object_disconnect_from_map(cur_weapon_handle);
      object_activate(cur_weapon_handle);
      object_set_visibility(cur_weapon_handle, 0);
      item_in_unit_inventory(cur_weapon_handle, unit_handle);
      unit->current_weapon_index = (uint16_t)NONE;
    }
  }

  /* With no active weapon, ready the desired one or go unarmed. */
  if (unit->current_weapon_index == NONE) {
    if (next_weapon_handle != NONE) {
      weapon_label = (int)weapon_get_label(next_weapon_handle);
      seat_label = unit_get_seat_label(unit_handle);
      unit_try_animation_state(unit_handle, seat_label, weapon_label, 1);

      animation_graph = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_tag));
      mode = tag_block_get_element(&animation_graph_definition_unit_seats(animation_graph),
                                   (int)(int8_t)unit->animation_seat_index,
                                   0x64);
      weapon_animations = tag_block_get_element(
        mode + 0x58, (int)(int8_t)unit->animation_weapon_index, 0xbc);

      object_connect_to_map(next_weapon_handle, 0);
      object_set_visibility(next_weapon_handle, 1);
      object_attach_to_marker(unit_handle, weapon_animations + 0x40,
                              next_weapon_handle, weapon_animations + 0x20);

      {
        uint16_t next_idx = unit->desired_weapon_index;
        unit->current_weapon_index = next_idx;
        if (next_idx != (uint16_t)NONE) {
          int16_t cur = unit->current_weapon_index;
          unit->weapon_last_used_at_game_time[(int)cur].value = game_time_get();
        }
      }

      weapon_ready(next_weapon_handle);
      unit_reset_weapon_state(unit_handle);
      return;
    }

    seat_label = unit_get_seat_label(unit_handle);
    unit_try_animation_state(unit_handle, seat_label, (int)"unarmed", 1);
    unit->current_weapon_index = (uint16_t)NONE;
  }

  unit_reset_weapon_state(unit_handle);
}

/* Element of the game globals grenade block (game globals +0x128),
 * stride 0x44. */

/* unit_throw_grenade_begin (0x1b2090) — begin grenade throw animation.
 * Starts the throw animation (state 0x21) when a grenade is available and
 * the animation and weapon states allow it, then plays the grenade's
 * throwing effect. Returns 1 on success, 0 if the throw cannot begin. */
char unit_throw_grenade_begin(int unit_handle, float *alignment_vector)
{
  unit_data_t *unit;
  char *unit_tag;
  int weapon_handle;
  char result;
  int16_t grenade_type;
  int16_t grenade_count;
  char anim_ok;
  animation_graph_t *animation_graph;
  char *animation;
  char *game_globals;
  game_globals_grenade_t *grenade;
  int effect_tag;
  real_vector3d alignment; /* k is never written */
  char weapon_blocks;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);

  {
    unit_data_t *temp_unit;
    temp_unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
    weapon_handle = unit_inventory_get_weapon(
      unit_handle, temp_unit->current_weapon_index);
  }

  result = 0;

  grenade_type = unit_get_current_grenade_type(unit_handle);
  grenade_count = unit_get_grenade_count(unit_handle, grenade_type);
  if (grenade_count < 1) {
    return result;
  }

  switch (unit->animation_state) {
  case 0x17:
  case 0x18:
  case 0x19:
  case _unit_state_entering_seat:
  case _unit_state_exiting_seat:
  case 0x1d:
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x27:
  case 0x29:
    return result;
  }

  weapon_blocks = (char)weapon_prevents_grenade_throwing(weapon_handle);
  if (weapon_blocks != '\0') {
    return result;
  }

  if (weapon_handle != NONE) {
    weapon_stop_reload(weapon_handle);
  }
  biped_stop_melee_attack(unit_handle);

  unit->animation_action = 0;
  unit->action_animation_index = NONE;

  anim_ok = unit_animation_set_state(unit_handle, 0x21);
  if (anim_ok == '\0') {
    return result;
  }

  unit->grenade_throw_state = 1;
  *(int16_t *)&unit->unk_574 = 0;
  animation_graph = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_tag));
  animation = tag_block_get_element(&animation_graph->animations,
                                    (int)unit->object.unk_128, 0xb4);
  unit->grenade_throw_full_power_ticks =
    (int16_t)(animation_definition_key_frame_index(animation) - unit->object.unk_130 + 1);

  if (alignment_vector != NULL) {
    unit_apply_alignment_vector(unit_handle, (real_vector2d *)alignment_vector);
  } else {
    alignment.i = unit->aiming_vector.i;
    alignment.j = unit->aiming_vector.j;
    if (normalize2d((real_vector2d *)&alignment) > 0.0f) {
      unit_apply_alignment_vector(unit_handle, (real_vector2d *)&alignment);
    }
  }
  first_person_weapon_message_from_unit(unit_handle, 0x11);
  player_control_unzoom(unit_handle);

  game_globals = game_globals_get();
  grenade = tag_block_get_element(game_globals + 0x128,
                                  (int)(int8_t)unit->current_grenade_index,
                                  sizeof(game_globals_grenade_t));
  effect_tag = grenade->throwing_effect.tag_index;
  if (effect_tag != NONE) {
    effect_new_from_object(effect_tag, unit_handle, unit_handle, (short)-1,
                           0.0f, 0.0f, 0, 0);
  }
  return 1;
}

/* scripting_magic_melee_attack (0x1b2260)
 * Triggers a melee attack on the first player's unit. */
void scripting_magic_melee_attack(void)
{
  unit_melee_attack_begin(
    ((player_data_t *)datum_get(player_data, 0))->unit_handle, 0, 0);
}

/* unit_impact_melee_damage (0x1b2290) — apply melee damage at impact point.
 *
 * When the unit has the "melee causes instant kill" flag (tag+0x17c bit 13),
 * and the target is a seated unit with body vitality, and the target's unit
 * tag has the "instant kill on melee" flag (tag+0x17c bit 22): performs
 * unit_cause_melee_damage, resets sound, and deletes the attacking unit.
 *
 * Otherwise, when the unit has the "melee causes boarding" flag (bit 12),
 * and the target is a biped/vehicle that is not dying/dead: walks up the
 * parent chain to ensure the attacker is not boarding its own hierarchy,
 * then copies the global forward/up vectors, negates the damage direction,
 * computes a cross product to derive orientation, translates the unit, and
 * starts a forced melee attack. */
void unit_impact_melee_damage(int unit_handle, int param_2, int param_3,
                              int param_4, int param_5, int param_6,
                              float *param_7, int param_8)
{
  unit_data_t *unit;
  unit_definition_t *unit_tag;
  object_data_t *target;
  unit_definition_t *target_tag;
  int parent_handle;
  object_data_t *parent;
  real_vector3d *forward;
  real_vector3d left;
  float mag;
  int flags;
  int unit_flags;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  target = object_get_and_verify_type(param_2, _object_mask_all);

  /* Instant-kill melee: unit tag flag bit 13 against a living biped whose
   * tag sets flag bit 22. */
  if ((unit_tag->flags & 0x2000) != 0 &&
      target->type == _object_type_biped &&
      target->unk_148 > 0.0f) {
    target_tag = tag_get(TAG_GROUP_UNIT, target->tag_index);
    if ((target_tag->flags & 0x400000) != 0) {
      unit_cause_melee_damage(unit_handle, 1, param_2, param_3, param_4,
                              param_5, param_7);
      object_deplete_body(unit_handle);
      object_delete(unit_handle);
      return;
    }
  }

  /* Boarding melee: unit tag flag bit 12. */
  if ((unit_tag->flags & 0x1000) == 0) {
    return;
  }
  if ((_object_mask_unit & (1 << target->type)) == 0) {
    return;
  }
  if ((target->damage_flags & 4) != 0) {
    return;
  }

  /* Refuse to board our own parent chain. */
  parent_handle = target->parent_object_index.value;
  while (parent_handle != NONE) {
    parent = object_get_and_verify_type(parent_handle, _object_mask_all);
    if (parent_handle == unit_handle) {
      return;
    }
    if (parent->type != _object_type_vehicle) {
      return;
    }
    parent_handle = parent->parent_object_index.value;
  }

  unit->object.translational_velocity = *global_zero_vector3d;
  unit->object.angular_velocity = *global_zero_vector3d;

  /* Face against the impact normal; the negation interleaves with the
   * cross_product3d argument pushes, so it stays per component. */
  forward = &unit->object.forward;
  *forward = *(real_vector3d *)param_7;
  forward->i = -forward->i;
  forward->j = -forward->j;
  forward->k = -forward->k;

  cross_product3d(&unit->object.up, forward, &left);
  mag = normalize3d(&left);
  if (mag == 0.0f) {
    cross_product3d(global_up_vector3d, forward, &left);
    mag = normalize3d(&left);
    if (mag == 0.0f) {
      left = *global_forward_vector3d;
    }
  }

  cross_product3d(forward, &left, &unit->object.up);

  object_translate(unit_handle, (real_point3d *)param_6, (void *)param_8);
  object_attach_to_parent(param_2, unit_handle, param_3);

  flags = unit->object.flags;
  unit->object.flags = flags | 0x20;
  unit_flags = unit->flags;
  unit->flags = unit_flags | 0x8000;

  unit_melee_attack_begin(unit_handle, 1, 0);
}

/* unit_place (0x1b24d0) — place a unit with placement data.
 *
 * Applies placement body vitality if > 0.0f, and if the "dead" flag
 * (placement+4, bit 0) is set, kills the unit: sets animation state,
 * clears weapons, clears grenade state, deletes weapon object,
 * marks as dead, and sets up the death animation frame. */
void unit_place(int unit_handle, void *placement)
{
  float *place;
  unit_data_t *unit;
  char *unit_tag;
  uint32_t *object_flags;
  animation_graph_t *animation_graph;
  animation_t *animation;
  int death_frame;
  int flags;
  int obj_flags;

  place = (float *)placement;
  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);

  if (place[0] > 0.0f) {
    unit->object.unk_144 = place[0];
  }

  /* Placement flags bit 0: place the unit dead. */
  if ((unit_placement_flags(placement) & 1) != 0) {
    unit_ping_animation(unit_handle, 1, 0, 0, 0, 0, 0, -1, 0);

    if (unit->animation_state == 0x19) {
      unit_clear_weapons(unit_handle);
      csmemset(unit->grenade_counts, 0, sizeof(unit->grenade_counts));
      object_flags = &unit->object.flags;

      if (unit->equipment_object_index.value != NONE) {
        object_delete(unit->equipment_object_index.value);
        unit->equipment_object_index.value = NONE;
      }

      /* Hold the last frames of the death animation. */
      animation_graph = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_tag));
      animation = tag_block_get_element(&animation_graph->animations,
                                        (int)unit->object.unk_128, sizeof(animation_t));
      death_frame = (int)animation->frame_count - 4;

      unit->object.damage_flags |= 4;
      obj_flags = unit->object.flags;
      *object_flags = obj_flags | 0x20000;

      /* max(death_frame, 0), branchless */
      unit->object.unk_130 =
        (uint16_t)(death_frame & ((death_frame < 0) - 1));

      flags = unit->flags;
      unit->flags = flags | 0x200;

      unit->time_of_death.value = game_time_get();
      unit->object.unk_144 = 0.0f;
      unit->object.unk_148 = 0.0f;

      object_deplete_shield(unit_handle);
      object_update_children_recursive(unit_handle);
    }
  }
}

/* unit_solo_player_integrated_night_vision_is_active (0x1b2610)
 * Returns true if there is exactly one local player, that player has a
 * valid unit, and that unit's current weapon has the night-vision flag. */
char unit_solo_player_integrated_night_vision_is_active(void)
{
  char active;
  int player_index;
  player_data_t *player;
  int unit_handle;

  active = 0;
  if (local_player_count() == 1) {
    player_index =
      local_player_get_player_index(local_player_get_next(-1));
    if (player_index != NONE) {
      player = datum_get(player_data, player_index);
      unit_handle = player->unit_handle;
      if (unit_handle != NONE)
        active = unit_has_night_vision_weapon(unit_handle);
    }
  }

  return active;
}

/* unit_create_initial_weapons (0x1b2660) — create initial weapons for a unit.
 *
 * Iterates the unit tag's initial weapons tag block (at tag+0x2d8, element
 * size 0x24), and for each weapon tag index that is not NONE, creates an
 * object via object_placement_data_new + object_new. Then tries to pick
 * it up: if the game engine is running and the unit already has that weapon
 * definition, delete the duplicate; otherwise attempt unit_enter_seat and
 * delete on failure.
 *
 * Source: units.c
 */
void unit_create_initial_weapons(int unit_handle)
{
  unit_data_t *unit;
  char *unit_tag;
  int *initial_weapons_block;
  int loop_counter;
  int element;
  int weapon_tag_index;
  int weapon_handle;
  object_data_t *weapon;
  char engine_running;
  char has_weapon;
  char enter_ok;
  uint8_t placement[136]; /* object_placement_data, 0x88 bytes */

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  initial_weapons_block = (int *)&unit_definition_initial_weapons(unit_tag);
  loop_counter = 0;

  if (*initial_weapons_block > 0) {
    element = 0;
    do {
      element =
        (int)tag_block_get_element(initial_weapons_block, element, 0x24);
      weapon_tag_index = ((tag_reference *)element)->tag_index;
      if (weapon_tag_index != NONE) {
        object_placement_data_new(placement, weapon_tag_index, unit_handle);
        weapon_handle = object_new(placement);
        if (weapon_handle != NONE) {
          weapon = object_get_and_verify_type(weapon_handle, 4);
          engine_running = game_engine_running();
          if (engine_running != '\0') {
            has_weapon =
              unit_has_weapon_definition_index(unit_handle, weapon->tag_index);
            if (has_weapon != '\0') {
              goto delete_weapon;
            }
          }
          enter_ok = unit_enter_seat(unit_handle, weapon_handle, 0);
          if (enter_ok != '\0') {
            goto next_weapon;
          }
        delete_weapon:
          object_delete(weapon_handle);
        }
      }
    next_weapon:
      loop_counter = loop_counter + 1;
      element = (int)(short)loop_counter;
    } while (element < *initial_weapons_block);
  }
}

/* unit_select_weapon_after_vehicle_exit (0x1b2740)
 * Register arg: unit_handle in EAX. */
void unit_select_weapon_after_vehicle_exit(int unit_handle)
{
  unit_data_t *unit;
  uint16_t current_idx;
  int16_t next_idx;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  current_idx = unit->current_weapon_index;
  next_idx = unit_next_weapon_index(unit_handle, (int16_t)current_idx, 0);
  unit->desired_weapon_index = next_idx;
  unit_update_weapon_readiness(unit_handle, 1);
}

/* unit_new (0x1b2780)
 *
 * Unit creation/initialization. Called when a new unit object is created.
 * Sets default values for all unit-specific fields: weapons, seats,
 * animation state, aiming vectors, control flags, etc.
 * Copies the object's forward vector to all 5 directional vector slots.
 * If the unit tag has initial seat occupancy data and a valid dialog
 * entry, assigns equipment creation. Returns 1 on success, 0 if the
 * unit tag's animation graph index is -1.
 *
 * Confirmed: 1 cdecl param (unit_handle), returns char (0 or 1).
 */
char unit_new(int unit_handle)
{
  unit_data_t *unit;
  unit_definition_t *unit_tag;
  real_vector3d *forward;
  char valid;
  int i;
  short seat_idx;
  char *seat_entry;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);

  if (object_definition_animation_graph_index(unit_tag) == NONE) { /* animation graph */
    return 0;
  }

  unit->equipment_object_index.value = NONE;
  csmemset(unit->weapon_object_indices, 0xff,
           sizeof(unit->weapon_object_indices));

  unit->current_weapon_index = NONE;
  unit->desired_weapon_index = NONE;
  unit->current_grenade_index = 0xff;
  unit->desired_grenade_index = 0xff;
  unit->current_zoom_level = 0xff;
  unit->desired_zoom_level = 0xff;
  unit->player_index.value = NONE;
  unit->actor_index.value = NONE;
  unit->swarm_actor_index.value = NONE;
  unit->swarm_next_unit_index.value = NONE;
  unit->swarm_prev_unit_index.value = NONE;
  unit->parent_seat_index = NONE;
  unit->driver_object_index.value = NONE;
  unit->gunner_object_index.value = NONE;

  *(int16_t *)&unit->animation_flags = 0;
  unit->animation_seat_index = 0xff;
  unit->animation_weapon_index = 0xff;
  unit->animation_weapon_type_index = 0xff;
  unit->animation_state = 0xff;
  unit->animation_action = 0;
  unit->overlay_action = 0;
  unit->aiming_screen_index = NONE;
  unit->looking_screen_index = NONE;
  *(int16_t *)&unit->action_animation_index = NONE;
  *(int16_t *)&unit->overlay_action_animation_index = NONE;
  unit->soft_ping_animation_index = NONE;
  unit->base_seat_index = 2;
  unit->last_ping_animation_index = NONE;
  unit->emotion_index = 0xff;
  *(int16_t *)&unit->override_emotion_animation_index = NONE;
  unit->magic_seat_index = 0xff;
  unit->aiming_with_euler_screen = 0;

  csmemset(&unit->unk_616, 0, 0x10);
  unit->looking_with_euler_screen = 0;
  csmemset(&unit->unk_632, 0, 0x10);

  forward = &unit->object.forward;
  valid = (char)valid_real_normal3d(forward);
  if (valid == 0) {
    csprintf(ERROR_STRING_BUFFER, "%s: assert_valid_real_normal3d(%f, %f, %f)",
             "&unit->object.forward", (double)forward->i,
             (double)unit->object.forward.j, (double)unit->object.forward.k,
             "c:\\halo\\SOURCE\\units\\units.c", 0x189, true);
    display_assert(ERROR_STRING_BUFFER, "c:\\halo\\SOURCE\\units\\units.c", 0x189,
                   true);
    system_exit(-1);
  }

  unit->looking_vector.i = forward->i;
  unit->looking_vector.j = unit->object.forward.j;
  unit->looking_vector.k = unit->object.forward.k;
  unit->desired_looking_vector.i = forward->i;
  unit->desired_looking_vector.j = unit->object.forward.j;
  unit->desired_looking_vector.k = unit->object.forward.k;
  unit->aiming_vector.i = forward->i;
  unit->aiming_vector.j = unit->object.forward.j;
  unit->aiming_vector.k = unit->object.forward.k;
  unit->desired_aiming_vector.i = forward->i;
  unit->desired_aiming_vector.j = unit->object.forward.j;
  unit->desired_aiming_vector.k = unit->object.forward.k;
  unit->desired_facing_vector.i = forward->i;
  unit->desired_facing_vector.j = unit->object.forward.j;
  unit->desired_facing_vector.k = unit->object.forward.k;

  unit->persistent_control_timer = 0;
  unit->dialogue_index = NONE;
  unit->flags |= 0x100;

  csmemset(&unit->unk_824, 0, 0x7c);
  unit->last_speech_finished_time = NONE;

  unit_dialogue_determine_variant(unit_handle);

  csmemset(unit->unk_992, 0xff, sizeof(unit->unk_992));

  unit->last_damage_category = 0;
  unit->delayed_damage_timer = 0;
  unit->delayed_damage_peak = 0;
  unit->killing_spree_count = 0;
  unit->delayed_damage_attacker_object_index = NONE;
  unit->time_of_death.value = NONE;
  unit->fake_encounter_index = NONE;
  unit->fake_squad_index = NONE;
  unit->integrated_light_battery = 1.0f;
  unit->flaming_death_delay = 0;
  unit->flaming_death_attacker_object_index.value = NONE;
  unit->killing_spree_last_time = NONE;

  /* Initial grenade count from the unit tag. */
  {
    short grenade_type;
    grenade_type = unit_definition_grenade_type(unit_tag);
    if (grenade_type >= 0 && grenade_type < NUMBER_OF_UNIT_GRENADE_TYPES &&
        unit_definition_grenade_count(unit_tag) >= 0) {
      unit->grenade_counts[grenade_type] = *(uint8_t *)&unit_definition_grenade_count(unit_tag);
    }
  }

  unit->object.flags |= 0x6000;

  if (unit_definition_feign_death_threshold(unit_tag) > REAL_ZERO_POOL &&
      unit_definition_feign_death_time(unit_tag) > REAL_ZERO_POOL &&
      unit_definition_feign_death_chance(unit_tag) > REAL_ZERO_POOL) {
    float random_val;

    random_val =
      random_math_real((unsigned int *)get_global_random_seed_address());
    if (random_val < unit_definition_feign_death_chance(unit_tag)) {
      unit->flags |= 0x2000;
    } else {
      unit->flags &= ~0x2000;
    }
  }

  /* Default team from the unit tag. */
  valid = (char)game_engine_running();
  if (valid == 0 && (unit->object.owner_team_index == 0 ||
                     unit->object.owner_team_index == NONE)) {
    unit->object.owner_team_index = unit_definition_default_team(unit_tag);
  }

  unit_try_animation_state(unit_handle, BASE_SEAT_LABEL_STAND_AS_INT, 0, 1);

  unit_create_initial_weapons(unit_handle);

  /* Any seat with a mounted weapon? */
  i = 0;
  seat_idx = 0;
  if (unit_tag->seats.count > 0) {
    for (;;) {
      seat_entry = tag_block_get_element(
        &unit_tag->seats, i, sizeof(unit_seat_t));
      if (unit_seat_definition_field_104(seat_entry) != NONE) {
        break;
      }
      seat_idx = seat_idx + 1;
      i = (int)seat_idx;
      if (i >= unit_tag->seats.count) {
        return 1;
      }
    }
    ai_create_mounted_weapons_for_unit(unit_handle);
  }

  return 1;
}

/* unit_board_vehicle (0x1b2b80)
 *
 * Handles a unit boarding a vehicle at a specific seat. Validates the seat via
 * unit_find_nearby_seat, attaches the unit to the vehicle at the seat's marker,
 * sets up weapon state and animations for the boarding sequence. Asserts that
 * the unit is not already parented. If the unit's tag has a boarding animation
 * (animation graph entry index > 7 and valid boarding animation), plays it.
 * Returns true if the boarding succeeds, false if the seat check fails.
 */
bool unit_board_vehicle(int unit_handle, int vehicle_handle, int16_t seat_index)
{
  unit_data_t *unit;
  unit_data_t *vehicle_unit;
  unit_definition_t *unit_tag;
  char *seat_def;
  void *marker_name;
  vector3_t unit_pos;
  union {
    object_marker marker;
    char buffer[0x90];
  } markers;
  vector3_t delta;
  int weapon_handle;
  char *weapon_label;
  char *animation_graph;
  animation_graph_unit_seat_t *unit_seat;
  int16_t boarding_anim_index;
  int16_t anim_result;

  if (!unit_find_nearby_seat(unit_handle, vehicle_handle, seat_index, 0))
    return false;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  vehicle_unit = object_get_and_verify_type(vehicle_handle, _object_mask_unit);

  unit_tag = tag_get(TAG_GROUP_UNIT, vehicle_unit->object.tag_index);
  seat_def =
    tag_block_get_element(&unit_tag->seats, (int)seat_index, sizeof(unit_seat_t));

  if (unit->object.parent_object_index.value != NONE) {
    display_assert("unit->object.parent_object_index==NONE",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1095, 1);
    system_exit(-1);
  }

  object_get_world_position(unit_handle, &unit_pos);

  /* The unit's offset from the seat marker, in marker space. */
  marker_name = unit_seat_definition_marker_name(seat_def);
  object_get_marker_by_name(vehicle_handle, marker_name, &markers, 1);

  delta.x = unit_pos.x - markers.marker.matrix.position.x;
  delta.y = unit_pos.y - markers.marker.matrix.position.y;
  delta.z = unit_pos.z - markers.marker.matrix.position.z;

  real_matrix3x3_transform_vector(&markers.marker.matrix, &delta,
                                  &delta); /* dup-args-ok */

  object_attach_to_marker(vehicle_handle, marker_name, unit_handle,
                          EMPTY_STRING_VOID);

  unit->parent_seat_index = seat_index;
  unit->object.parent_object_index.value = vehicle_handle;

  unit_update_seat_occupancy(vehicle_handle);

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  unit->desired_weapon_index = unit_next_weapon_index(unit_handle, unit->current_weapon_index, 0);

  unit_update_weapon_readiness(unit_handle, 1);

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  weapon_handle = unit_inventory_get_weapon(unit_handle, unit->current_weapon_index);

  if (weapon_handle == NONE)
    weapon_label = "unarmed";
  else
    weapon_label = weapon_get_label(weapon_handle);

  if (!unit_try_animation_state(unit_handle, (int)unit_seat_definition_label(seat_def),
                                (int)weapon_label, 1)) {
    unit_try_animation_state(unit_handle, (int)unit_seat_definition_label(seat_def), 0, 1);
  }

  /* Play the seat's boarding animation, if the mode has one. */
  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  animation_graph =
    tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_tag));
  unit_seat = tag_block_get_element(&animation_graph_definition_unit_seats(animation_graph), (int)(int8_t)unit->animation_seat_index,
                                    sizeof(animation_graph_unit_seat_t));

  if (unit_seat->animations.count > _unit_seat_animation_seat_enter) {
    boarding_anim_index = ((int16_t *)unit_seat->animations.address)[_unit_seat_animation_seat_enter];
    if (boarding_anim_index != NONE) {
      int anim_graph_tag_index;

      object_set_region_count(unit_handle, 6);

      anim_graph_tag_index = object_definition_animation_graph_index(unit_tag);
      anim_result = model_animation_choose_random(1, anim_graph_tag_index,
                                                  boarding_anim_index);
      unit_set_animation(unit_handle, anim_graph_tag_index, anim_result);
      unit->animation_state = _unit_state_entering_seat;

      object_adjust_interpolation_position(unit_handle, &delta);
      object_update_children_recursive(unit_handle);
    }
  }

  unit_vehicle_board_notify(unit_handle, vehicle_handle);
  unit_reset_weapon_state(unit_handle);
  return true;
}

/* Element of the 'mode' tag's node block (mode +0xb8), stride 0x9c.
 * unit_exit_seat_end reads the root node's translation and inverse matrix. */
typedef struct {
  uint8_t pad_00[0x28];
  real_point3d default_translation;               /* +0x28 */
  uint8_t pad_34[0x34];
  real_matrix4x3 runtime_default_inverse_matrix;  /* +0x68 */
} model_node_t;
cs(model_node_t, 0x9c);
co(model_node_t, default_translation, 0x28);
co(model_node_t, runtime_default_inverse_matrix, 0x68);

/* unit_exit_seat_end (0x1b2dd0)
 * Finalizes a unit's exit from a vehicle seat. Gets the seat marker transform,
 * detaches from parent, repositions the unit, applies seat exit matrix, and
 * reselects weapon. If unit is a biped, calls biped_exit_seat_end. */
void unit_exit_seat_end(int unit_handle)
{
  unit_data_t *unit;
  unit_data_t *parent_unit;
  unit_definition_t *parent_tag;
  char *seat_element;
  real_matrix4x3 *node_matrix;
  vector3_t exit_offset;
  object_marker marker;
  vector3_t transform_out;
  real_point3d exit_position;
  real_matrix4x3 matrix_out;
  int seat_handle;
  real_matrix4x3 *root_inverse_matrix;
  vector3_t model_node_pos;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  seat_handle = unit->object.parent_object_index.value;

  if (seat_handle == NONE || unit->parent_seat_index == NONE) {
    return;
  }

  parent_unit = object_get_and_verify_type(seat_handle, _object_mask_unit);
  parent_tag = tag_get(TAG_GROUP_UNIT, parent_unit->object.tag_index);
  seat_element = tag_block_get_element(
    &parent_tag->seats, (int)unit->parent_seat_index,
    sizeof(unit_seat_t));

  node_matrix = object_get_node_matrix(unit_handle, 0);

  object_get_marker_by_name(seat_handle, (void *)(seat_element + 0x24),
                                  &marker, 1);

  exit_offset.x = node_matrix->position.x - marker.matrix.position.x;
  exit_offset.y = node_matrix->position.y - marker.matrix.position.y;
  exit_offset.z = node_matrix->position.z - marker.matrix.position.z;

  /* The marker-space offset is computed but never used. */
  real_matrix3x3_transform_vector(&marker.matrix, &exit_offset,
                                  &transform_out);

  /* Root node of the unit's own model: default position and the rotation
   * applied on exit. */
  {
    char *own_unit_tag;
    char *own_model_tag;
    model_node_t *root_node;

    own_unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
    own_model_tag = tag_get(TAG_GROUP_MODE, object_definition_model_index(own_unit_tag));
    root_node = tag_block_get_element(&((model_definition_t *)own_model_tag)->nodes, 0, sizeof(model_node_t));

    root_inverse_matrix = &root_node->runtime_default_inverse_matrix;
    model_node_pos = root_node->default_translation;
  }

  /* Leaving the driver seat closes the vehicle. */
  if (parent_unit->driver_object_index.value == unit_handle &&
      parent_unit->animation_state != 0x25 &&
      unit->object.parent_object_index.value != NONE) {
    unit_animation_set_state(unit->object.parent_object_index.value, 0x25);
  }

  unit->last_vehicle_index = seat_handle;
  unit->game_time_at_last_vehicle_exit = game_time_get();

  if (unit->driver_object_index.value == unit_handle) {
    unit->driver_object_index.value = NONE;
  }
  if (unit->gunner_object_index.value == unit_handle) {
    unit->gunner_object_index.value = NONE;
  }

  object_detach_from_parent(unit_handle);

  exit_position.x = exit_offset.x + unit->object.position.x;
  exit_position.y = exit_offset.y + unit->object.position.y;
  exit_position.z =
    exit_offset.z + unit->object.position.z - model_node_pos.z;

  object_set_position(unit_handle, &exit_position, 0, 0);

  {
    real_matrix4x3 *node_matrix2;

    node_matrix2 = object_get_node_matrix(unit_handle, 0);
    matrix4x3_multiply(&node_matrix2->scale, &root_inverse_matrix->scale,
                       &matrix_out.scale);

    unit->object.forward = matrix_out.forward;
    unit->object.up = matrix_out.up;
  }

  object_set_visibility(unit_handle, 1);

  unit->parent_seat_index = NONE;
  unit->base_seat_index = 2;

  if (parent_unit->driver_object_index.value == unit_handle) {
    parent_unit->driver_object_index.value = NONE;
  }
  if (parent_unit->gunner_object_index.value == unit_handle) {
    parent_unit->gunner_object_index.value = NONE;
  }

  unit_update_seat_occupancy(seat_handle);
  unit_select_weapon_after_vehicle_exit(unit_handle);

  /* Animation update type 0x14: seat exit. */
  {
    uint8_t anim_data[2];
    anim_data[0] = 0x14;
    anim_data[1] = 0x00;
    unit_update_animation(unit_handle, (char *)anim_data);
  }

  {
    char *block_ref;
    block_ref = object_header_block_reference_get(unit_handle,
                                                  &unit->object.unk_408);

    ((object_node_record *)block_ref)->translation = model_node_pos;
  }

  if (unit->object.type == _object_type_biped) {
    biped_exit_seat_end(unit_handle, seat_handle);
  }

  object_update_children_recursive(unit_handle);
}

/* unit_died (0x1b3060)
 * Handles unit death or feign-death. On real death (param_2=0): sets garbage,
 * releases player/actor refs, clears seats, drops weapons and grenades.
 * On feign death (param_2!=0): checks feign_death_timer and random chance
 * to decide living/feigning.
 * cdecl, 2 stack params. */
void unit_died(int unit_handle, char param_2)
{
  unit_data_t *unit;
  char *unit_tag;
  actor_t *actor;
  int weapon_handle;
  unit_data_t *temp_unit;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if (param_2 != 0) {
    /* Feigning death. */
    if ((int16_t)unit->feign_death_timer <= 0) {
      display_assert("unit->unit.feign_death_timer > 0",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x13eb, true);
      system_exit(-1);
    }

    unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);

    {
      unsigned int *seed;
      float rnd;
      seed = get_global_random_seed_address();
      rnd = random_math_real((unsigned int *)seed);
      if (rnd < unit_definition_feign_death_repeat_chance(unit_tag)) {
        unit->flags |= 0x2000;
      } else {
        unit->flags &= ~0x2000u;
      }
    }
  } else {
    unit->feign_death_timer = 0;
    object_set_garbage(unit_handle, 1);

    if (unit->player_index.value != NONE) {
      player_died(unit->player_index.value);
      unit->player_index.value = NONE;
    }

    if (unit->actor_index.value != NONE) {
      actor = datum_get(actor_data, unit->actor_index.value);
      unit->fake_encounter_index = *(int16_t *)&actor->field_034;
      unit->fake_squad_index = actor->field_03a;
      actor_died(unit->actor_index.value);
      unit->actor_index.value = NONE;
    }

    if (unit->swarm_actor_index.value != NONE) {
      actor = datum_get(actor_data, unit->swarm_actor_index.value);
      unit->fake_encounter_index = *(int16_t *)&actor->field_034;
      unit->fake_squad_index = actor->field_03a;
      actor_swarm_unit_died(unit->swarm_actor_index.value, unit_handle);
      unit->swarm_actor_index.value = NONE;
    }

    unit->time_of_death.value = game_time_get();
  }

  unit->flags &= 0xffffffee;
  unit->control_flags = 0;

  if (unit->current_weapon_index != NONE) {
    /* The original pushes both zeros before evaluating the handle
     * (0x1b3166), so the lookup stays nested in the argument list. */
    weapon_owner_update(
      unit_inventory_get_weapon(
        unit_handle,
        (int16_t)((unit_data_t *)object_get_and_verify_type(
                    unit_handle, _object_mask_unit))->current_weapon_index),
      0, 0.0f);
  }

  /* Integrated light off. */
  temp_unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  temp_unit->flags &= ~0x02000000u;

  if (unit->object.parent_object_index.value != NONE) {
    if (unit->parent_seat_index != NONE) {
      unit_exit_seat_end(unit_handle);
    } else {
      unit_detach_from_parent(unit_handle);
    }
  }

  unit->unk_872 = 0;
  unit_drop_weapons_on_death(unit_handle);

  temp_unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if (temp_unit->equipment_object_index.value != NONE) {
    weapon_handle = temp_unit->equipment_object_index.value;
    unit_detach_weapon(unit_handle, weapon_handle);
    temp_unit->equipment_object_index.value = NONE;
  }

  unit_drop_grenades_on_death(unit_handle);

  if (unit->weapon_drop_delay_ticks == 0) {
    unit_set_in_vehicle(unit_handle, 1);
  }

  unit->overlay_action_animation_index = NONE;
  unit->action_animation_index = NONE;
  unit->melee_attack_state = 0;
  if (unit->grenade_throw_state == 1) {
    unit->grenade_throw_state = 0;
  }
}

/* unit_scripting_enter_vehicle (0x1b32d0) — scripted vehicle entry.
 *
 * Makes a unit enter a specific seat of a vehicle by seat name.
 * Validates both handles, checks the seat name length, verifies the
 * unit is not dying/dead, handles existing vehicle occupancy by
 * calling unit_exit_seat_end, then iterates the vehicle's seats
 * tag block looking for a matching seat name. Checks seat availability
 * and type compatibility before calling unit_board_vehicle.
 *
 * Source: units.c
 */
void unit_scripting_enter_vehicle(int unit_handle, int vehicle_handle,
                                  char *seat_name)
{
  unit_data_t *unit;
  unit_data_t *vehicle_unit;
  unit_definition_t *vehicle_tag;
  tag_block *seats_block;
  int seat_idx;
  int seat_element;
  int stricmp_result;
  char seat_filled;

  if (unit_handle == NONE) {
    return;
  }
  if (vehicle_handle == NONE) {
    return;
  }
  if (csstrlen(seat_name) == 0) {
    return;
  }

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if ((unit->object.damage_flags & 4) != 0) {
    return;
  }

  /* Leave the current seat first; give up if still parented. */
  if (unit->object.parent_object_index.value != NONE) {
    if (unit->parent_seat_index != NONE) {
      unit_exit_seat_end(unit_handle);
    }
    if (unit->object.parent_object_index.value != NONE) {
      return;
    }
  }

  vehicle_unit = object_get_and_verify_type(vehicle_handle, _object_mask_unit);
  vehicle_tag = tag_get(TAG_GROUP_UNIT, vehicle_unit->object.tag_index);
  seats_block = &vehicle_tag->seats;
  seat_idx = 0;

  if (seats_block->count > 0) {
    seat_element = 0;
    while (1) {
      seat_element = (int)tag_block_get_element(seats_block, seat_element,
                                                sizeof(unit_seat_t));
      stricmp_result = crt_stricmp(seat_name, (char *)(seat_element + 4));

      if (stricmp_result == 0) {
        seat_filled = unit_seat_filled(vehicle_handle, (int16_t)seat_idx);
        if (seat_filled == '\0') {
          if (unit->object.type == _object_type_vehicle ||
              unit_set_or_test_seat_and_weapon_label(
                unit_handle, (const char *)(seat_element + 4), 0, 0) != '\0') {
            unit_board_vehicle(unit_handle, vehicle_handle, (int16_t)seat_idx);
            return;
          }
        }
      }

      seat_idx = seat_idx + 1;
      seat_element = (int)(short)seat_idx;
      if (seat_element >= seats_block->count) {
        return;
      }
    }
  }
}

/* vehicle_scripting_load_magic (0x1b3400) — load units into vehicle seats.
 *
 * Iterates child objects of the given group_handle. For each child that is
 * a biped/vehicle type, tries to seat it in the vehicle.  The dead check
 * reads the vehicle's damage flags, not the child's.
 * Finds available seats via vehicle_scripting_find_available_seats, checks
 * seat type compatibility, handles existing vehicle occupancy, and calls
 * unit_board_vehicle. Returns the count of successfully seated units.
 */
uint16_t vehicle_scripting_load_magic(int vehicle_handle, int seat_substring,
                                      int group_handle)
{
  unit_data_t *vehicle_unit;
  unit_definition_t *vehicle_tag;
  int child_handle;
  unit_data_t *child;
  int16_t seat_count;
  short seat_indices[16];
  int iter_state[1];
  int inner_idx;
  int16_t seat_idx;
  char *seat_element;
  char seat_type_ok;
  char board_ok;
  uint16_t loaded_count;

  loaded_count = 0;
  if (vehicle_handle == NONE) {
    return 0;
  }

  vehicle_unit = object_get_and_verify_type(vehicle_handle, _object_mask_unit);
  vehicle_tag = tag_get(TAG_GROUP_UNIT, vehicle_unit->object.tag_index);

  seat_count = vehicle_scripting_find_available_seats(
    vehicle_handle, seat_substring, -1, seat_indices, 0x10);

  child_handle = object_list_get_first(group_handle, iter_state);
  if (child_handle == NONE) {
    return 0;
  }

  do {
    child = object_get_and_verify_type(child_handle, _object_mask_all);

    if (((1 << ((uint8_t)child->object.type & 0x1f)) & _object_mask_unit) != 0 &&
        (vehicle_unit->object.damage_flags & 4) == 0) {
      inner_idx = 0;
      if ((short)seat_count > 0) {
        do {
          seat_idx = seat_indices[(short)inner_idx];
          if (seat_idx != NONE) {
            seat_element = tag_block_get_element(
              &vehicle_tag->seats, (int)seat_idx, sizeof(unit_seat_t));

            if (child->object.type != _object_type_vehicle) {
              seat_type_ok = unit_set_or_test_seat_and_weapon_label(
                child_handle, (const char *)(seat_element + 4), 0, 0);
              if (seat_type_ok == '\0') {
                goto next_seat;
              }
            }

            if (child->object.parent_object_index.value != NONE) {
              if (child->parent_seat_index != NONE) {
                unit_exit_seat_end(child_handle);
              }
              if (child->object.parent_object_index.value != NONE) {
                goto next_seat;
              }
            }

            board_ok =
              unit_board_vehicle(child_handle, vehicle_handle, seat_idx);
            if (board_ok != '\0') {
              seat_indices[(short)inner_idx] = NONE;
              loaded_count = loaded_count + 1;
              break;
            }
          }
        next_seat:
          inner_idx = inner_idx + 1;
        } while ((short)inner_idx < (short)seat_count);
      }
    }

    child_handle = object_list_get_next(group_handle, iter_state);
  } while (child_handle != NONE);

  return loaded_count;
}

/* unit_try_and_exit_seat (0x1b3580) — attempt to exit the current seat.
 * Vehicles exit immediately.  Otherwise the unit needs an idle animation
 * state and a seat-exit animation in its current mode; the driver opens
 * the vehicle, and the unit enters the exiting-seat animation state.
 * Returns 1 when the exit animation started, 0 otherwise.
 */
char unit_try_and_exit_seat(int unit_handle)
{
  unit_data_t *unit;
  int vehicle_handle;
  char *unit_tag;
  char *animation_graph;
  animation_graph_unit_seat_t *unit_seat;
  int anim_block_count;
  int16_t exit_anim;
  unit_data_t *vehicle;
  int anim_graph_tag_index;
  int16_t animation_index;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);

  if (unit->object.parent_object_index.value == NONE ||
      unit->parent_seat_index == NONE) {
    return 0;
  }

  if (unit->object.type == _object_type_vehicle) {
    unit_exit_seat_end(unit_handle);
    return 0;
  }

  if (unit_animation_busy(&unit->animation_flags) != '\0') {
    return 0;
  }

  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  animation_graph = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(unit_tag));

  unit_seat = tag_block_get_element(&animation_graph_definition_unit_seats(animation_graph), (int)(int8_t)unit->animation_seat_index,
                                    sizeof(animation_graph_unit_seat_t));

  anim_block_count = unit_seat->animations.count;
  if (anim_block_count <= _unit_seat_animation_seat_exit) {
    return 0;
  }

  exit_anim = ((short *)unit_seat->animations.address)[_unit_seat_animation_seat_exit];
  if (exit_anim == NONE) {
    return 0;
  }

  vehicle_handle = unit->object.parent_object_index.value;
  vehicle = object_get_and_verify_type(vehicle_handle, _object_mask_unit);
  if (vehicle->driver_object_index.value == unit_handle) {
    unit_open(vehicle_handle);
  }

  anim_graph_tag_index = object_definition_animation_graph_index(unit_tag);
  animation_index =
    animation_choose_random_permutation(anim_graph_tag_index, exit_anim);
  unit_set_animation(unit_handle, anim_graph_tag_index, animation_index);

  object_set_visibility(unit_handle, 1);
  unit->animation_state = _unit_state_exiting_seat;

  ai_handle_exit_vehicle(unit_handle);

  return 1;
}

/* Game globals first-person interface element (game globals +0x17c),
 * stride 0xc0. */
typedef struct {
  uint8_t pad_00[0x48];
  tag_reference night_vision_off_on_effect;  /* +0x48 */
  tag_reference night_vision_on_off_effect;  /* +0x58 */
  uint8_t pad_68[0x58];
} game_globals_first_person_interface_t;
cs(game_globals_first_person_interface_t, 0xc0);
co(game_globals_first_person_interface_t, night_vision_off_on_effect, 0x48);
co(game_globals_first_person_interface_t, night_vision_on_off_effect, 0x58);

/* Element of the unit tag's powered-seat block (unit +0x2cc), stride 0x44.
 * The times ramp the per-seat power level at unit +0x2e8. */
typedef struct {
  uint8_t pad_00[0x4];
  real powerup_time;                         /* +0x04 */
  real powerdown_time;                       /* +0x08 */
  uint8_t pad_0c[0x38];
} powered_seat_definition_t;
cs(powered_seat_definition_t, 0x44);
co(powered_seat_definition_t, powerup_time, 0x04);
co(powered_seat_definition_t, powerdown_time, 0x08);

/* unit_update (0x1b3690)
 * Main per-tick update for a unit (biped or vehicle); always returns 1.
 * Stack frame: SUB ESP, 0x58. */
char unit_update(int unit_handle)
{
  unit_data_t *unit;
  unit_definition_t *unit_tag;
  char timer_wrapped;
  char skip_combat_update;
  char l_8;
  char integrated_lights_allowed;
  char l_6;
  char c6;
  int i10;
  uint32_t u15;
  uint32_t unit_flags;
  float f1;
  float f20;
  float aiming_speed_scale;
  float aim_vel_limit;
  float aim_accel_limit;
  real_vector3d *aim_vec;
  real_vector3d saved_aim;
  /* Euler-aim transform in real_matrix4x3 layout; must stay one contiguous
   * block since unit_euler_aiming_update transforms points through it
   * (separate locals get scattered by clang). */
  struct {
    float scale;
    real_vector3d forward;
    real_vector3d left;
    real_vector3d up;
    real_point3d translation;
  } em;
  float zoom_interp;
  int zoom_sound_ref;
  char zoom_level_new;
  short grenade_idx;
  units_globals_t *anim_ptr;
  short anim_counter;
  unsigned char anim_byte;
  unit_data_t *holder;
  int wpn_tag;
  int game_glob;
  game_globals_first_person_interface_t *fp_iface;
  powered_seat_definition_t *powered_seat;
  float delta_angle;
  float ratio;
  short seat_i;
  int seat_off;
  char is_held;
  float flash_mod;
  int eff_timer_val;
  int eff_handle;
  int eff_data;
  int16_t eff_anim;
  short wpn_idx;
  unit_data_t *unit_data_tmp;
  unit_data_t *refetched;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_tag = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  timer_wrapped = 0;
  skip_combat_update = 0;
  l_8 = 0;
  integrated_lights_allowed = game_engine_allow_integrated_lights();

  /* [1] Debug trace enter */
  if (PROFILE_GLOBAL_ENABLE != 0 && UNIT_UPDATE_SECTION_ENABLED != 0) {
    profile_enter_private(UNIT_UPDATE_SECTION);
  }
  unit_control_trace(unit_handle, TRACE_STRING_UNIT_UPDATE_BEGIN);

  /* [2] Animation frame counter */
  anim_ptr = UNIT_GLOBALS;
  *(short *)&unit->timer = (short)unit->timer + 1;
  anim_counter = (short)unit->timer;

  if (anim_ptr->field_04 == 0 && anim_ptr->field_00 < anim_counter) {
    anim_ptr->field_04 = 1;
    timer_wrapped = 1;
    unit->timer = 0;
  } else {
    if (anim_ptr->field_02 > anim_counter) {
      anim_counter = anim_ptr->field_02;
    }
    anim_ptr->field_02 = anim_counter;
  }

  /* [3] Running blind; otherwise, with flags bit 0 clear, face forward */
  if ((unit->flags & 0x2000000) != 0) {
    unit_update_running_blind(unit_handle, &unit->desired_facing_vector);
    unit->desired_aiming_vector.i = unit->desired_facing_vector.i;
    unit->desired_aiming_vector.j = unit->desired_facing_vector.j;
    unit->desired_aiming_vector.k = unit->desired_facing_vector.k;
    unit->desired_looking_vector.i = unit->desired_facing_vector.i;
    unit->desired_looking_vector.j = unit->desired_facing_vector.j;
    unit->desired_looking_vector.k = unit->desired_facing_vector.k;
    {
      float *g = global_forward_vector_ptr;
      unit->throttle.x = g[0];
      unit->throttle.y = g[1];
      unit->throttle.z = g[2];
    }
    unit->control_flags = 0;
  } else if ((unit->flags & 1) == 0) {
    real_vector3d *forward = &unit->object.forward;
    unit->desired_looking_vector.i = forward->i;
    unit->desired_looking_vector.j = forward->j;
    unit->desired_looking_vector.k = forward->k;
    unit->desired_aiming_vector.i = forward->i;
    unit->desired_aiming_vector.j = forward->j;
    unit->desired_aiming_vector.k = forward->k;
    unit->desired_facing_vector.i = forward->i;
    unit->desired_facing_vector.j = forward->j;
    unit->desired_facing_vector.k = forward->k;
    {
      float *g = global_zero_vector_ptr;
      unit->throttle.x = g[0];
      unit->throttle.y = g[1];
      unit->throttle.z = g[2];
    }
    unit->control_flags = 0;
  }

  /* [4-7] Main update block (unless tag has interlocked flag 0x800) */
  if ((unit_tag->flags & 0x800) == 0) {
    eff_timer_val = unit->persistent_control_timer;
    l_6 = 0;

    /* [4] Effect timer countdown */
    if (eff_timer_val > 0) {
      u15 = (uint32_t)unit->control_flags | (uint32_t)unit->persistent_control_flags;
      unit->control_flags = (int)u15;
      if ((unit->persistent_control_flags & 0x800) == 0) {
        *(int *)&unit->primary_trigger = 0;
      } else {
        if (eff_timer_val % 7 == 0) {
          u15 = u15 | 0x800;
        } else {
          u15 = u15 & 0xfffff7ff;
        }
        unit->control_flags = (int)u15;
        *(int *)&unit->primary_trigger = 0x3f800000;
      }
      unit->persistent_control_timer = eff_timer_val - 1;
      if (eff_timer_val - 1 == 0) {
        unit->persistent_control_flags = 0;
      }
    }

    /* [5] Take facing, aiming and throttle from the driver/gunner */
    if ((unit->flags & 0x8000000) == 0) {
      /* Driver */
      if (unit->driver_object_index.value != -1 &&
          (unit->object.damage_flags & 4) == 0) {
        holder = object_get_and_verify_type(unit->driver_object_index.value,
                                            _object_mask_unit);
        unit->object.owner_team_index = holder->object.owner_team_index;
        l_6 = 1;

        if (holder->player_index.value != NONE ||
            (holder->animation_state != _unit_state_exiting_seat &&
             holder->animation_state != _unit_state_entering_seat)) {
          assert_halt(valid_real_normal3d(&holder->desired_facing_vector));
          unit->control_flags = (int)((uint32_t)unit->control_flags |
                             (holder->control_flags & 0x3f));
          unit->desired_facing_vector.i = holder->desired_facing_vector.i;
          unit->desired_facing_vector.j = holder->desired_facing_vector.j;
          unit->desired_facing_vector.k = holder->desired_facing_vector.k;
          unit->throttle.x = holder->throttle.x;
          unit->throttle.y = holder->throttle.y;
          unit->throttle.z = holder->throttle.z;
        }
      }
      /* Gunner */
      if (unit->gunner_object_index.value != -1 &&
          (unit->object.damage_flags & 4) == 0) {
        holder = object_get_and_verify_type(unit->gunner_object_index.value,
                                            _object_mask_unit);
        if (l_6 == 0) {
          unit->object.owner_team_index = holder->object.owner_team_index;
        }
        if (holder->player_index.value != NONE ||
            (holder->animation_state != _unit_state_exiting_seat &&
             holder->animation_state != _unit_state_entering_seat)) {
          assert_halt(valid_real_normal3d(&holder->desired_aiming_vector));
          unit->desired_aiming_vector.i = holder->desired_aiming_vector.i;
          unit->desired_aiming_vector.j = holder->desired_aiming_vector.j;
          unit->desired_aiming_vector.k = holder->desired_aiming_vector.k;
          unit->desired_looking_vector.i = holder->desired_aiming_vector.i;
          unit->desired_looking_vector.j = holder->desired_aiming_vector.j;
          unit->desired_looking_vector.k = holder->desired_aiming_vector.k;
          unit->control_flags = (int)((uint32_t)unit->control_flags |
                             (holder->control_flags & 0x7c00));
          *(int *)&unit->primary_trigger = *(int *)&holder->primary_trigger;
        }
      }
      /* Ticks since the gunner last gave input */
      if (((uint32_t)unit->control_flags & 0x7c00) == 0) {
        if ((char)unit->gunner_inactive_ticks < 0x7f) {
          *(char *)&unit->gunner_inactive_ticks = (char)unit->gunner_inactive_ticks + 1;
        }
      } else {
        unit->gunner_inactive_ticks = 0;
      }
    }

    /* Flashlight + firing combined check */
    if (CHEAT_JETPACK != 0 && ((uint32_t)unit->control_flags & 0x800) != 0 &&
        ((uint32_t)unit->control_flags & 0x2000) != 0) {
      skip_combat_update = 1;
    }

    /* [6a] Active camouflage */
    if ((*(unsigned char *)&unit->flags & 0x10) == 0) {
      f20 = unit->active_camouflage - REAL_ONE_OVER_120_POOL;
      unit->active_camouflage = f20;
      if (f20 < 0.0f) {
        unit->active_camouflage = 0.0f;
      }
    } else {
      c6 = game_engine_running();
      f20 = REAL_ONE_OVER_120_POOL;
      if (c6 != 0 && unit->cause_for_camo_regrowth != 0 &&
          unit->cause_for_camo_regrowth == 1) {
        refetched = object_get_and_verify_type(unit_handle, _object_mask_unit);
        i10 = unit_inventory_get_weapon(unit_handle, refetched->current_weapon_index);
        f20 = REAL_ONE_OVER_120_POOL;
        if (i10 != -1) {
          int *wo = (int *)object_get_and_verify_type(i10, 4);
          i10 = (int)tag_get(0x77656170, *wo);
          f20 = REAL_ONE_OVER_120_POOL;
          if (weapon_definition_field_4d0(i10) != 0.0f) {
            f20 = weapon_definition_field_4d0(i10);
          }
        }
      }
      f1 = unit->active_camouflage;
      unit->active_camouflage = f20 + f1;
      if (f20 + f1 > 1.0f) {
        unit->active_camouflage = 1.0f;
        unit->cause_for_camo_regrowth = 0;
      }
    }

    /* [6b] Active camouflage super amount */
    if ((*(unsigned char *)&unit->flags & 0x20) == 0) {
      f20 = unit->active_camouflage_super_amount - REAL_ONE_NINETIETH_POOL;
      unit->active_camouflage_super_amount = f20;
      if (f20 < 0.0f) {
        unit->active_camouflage_super_amount = 0.0f;
      }
    } else {
      f20 = unit->active_camouflage_super_amount + REAL_ONE_NINETIETH_POOL;
      unit->active_camouflage_super_amount = f20;
      if (f20 > 1.0f) {
        unit->active_camouflage_super_amount = 1.0f;
      }
    }

    /* [6c] Body stun */
    {
      int16_t t = (int16_t)unit->body_stun_ticks;
      if (t > 0) {
        t--;
        unit->body_stun_ticks = t;
        if (t == 0) {
          *(int *)&unit->body_stun = 0;
        }
      }
    }

    /* [7a] Weapon drop delay */
    {
      char st = (char)unit->weapon_drop_delay_ticks;
      if (st > 0) {
        st--;
        unit->weapon_drop_delay_ticks = st;
        if (st == 0) {
          unit_set_in_vehicle(unit_handle, 1);
        }
      }
    }

    /* [7b] Feign death ends: die, or get back up */
    {
      int16_t et = (int16_t)unit->feign_death_timer;
      if (et > 0 && (*(unsigned char *)&unit->object.flags & 0x20) != 0) {
        et--;
        unit->feign_death_timer = et;
        if (et == 0) {
          if (unit->object.unk_144 <= 0.0f) {
            unit_died(unit_handle, 0);
          } else {
            unit->object.damage_flags =
              unit->object.damage_flags & 0xfb;
            anim_byte = unit->animation_flags;
            unit_set_actively_controlled(unit_handle, 1);
            unit_try_animation_state(unit_handle, (int)BASE_SEAT_LABEL_STAND_AS_INT_PTR, 0, 1);
            unit_animation_set_state(unit_handle,
                         (int16_t)((~(anim_byte >> 3) & 1) | 0x22));
            unit->animation_flags =
              unit->animation_flags & 0xfb;
            if (unit->object.type == 0) {
              biped_stop_limp_body_physics(unit_handle);
            }
            unit_scream(unit_handle, 5);
          }
        }
      }
    }
  } /* end tag 0x800 check */

  /* ================================================================
   * [8-10] Weapon management, aiming, combat state
   * (unless tag has "interlocked weapons" flag 0x400)
   * ================================================================ */
  if ((unit_tag->flags & 0x400) == 0) {
    if ((*(unsigned short *)&unit->object.damage_flags & 4) == 0) {
      /* [8a] Weapon slot switch */
      if ((*(unsigned short *)&unit->object.damage_flags & 0x400) == 0) {
        if (unit->desired_weapon_index != unit->current_weapon_index) {
          c6 = unit_animation_busy(&unit->animation_flags);
          if (c6 == 0) {
            refetched = object_get_and_verify_type(unit_handle, _object_mask_unit);
            i10 = unit_inventory_get_weapon(unit_handle, (int16_t)refetched->desired_weapon_index);
            if (i10 != -1) {
              c6 = unit_can_enter_seat(unit_handle, i10);
              if (c6 != 0) {
                unit_update_weapon_readiness(unit_handle, 1);
              }
            }
          }
        }
      } else {
        unit_set_in_vehicle(unit_handle, 1);
      }

      /* [8b] Grenade type switch */
      if (unit->desired_grenade_index != unit->current_grenade_index) {
        c6 = unit_animation_busy(&unit->animation_flags);
        if (c6 == 0) {
          grenade_idx = unit_inventory_next_grenade(
            unit_handle, (short)(signed char)unit->desired_grenade_index,
            0);
          if (grenade_idx != -1) {
            unit->current_grenade_index = (char)grenade_idx;
          }
        }
      }

      /* [8c] Grenade count enforcement */
      if (CHEAT_INFINITE_AMMO != 0 && unit->player_index.value != -1) {
        char *gc = (char *)unit->grenade_counts;
        int gi;
        for (gi = 2; gi != 0; gi--) {
          signed char gv = *gc;
          if (gv < 2) {
            gv = 1;
          }
          *gc = gv;
          gc++;
        }
        if ((char)unit->desired_grenade_index == (char)-1) {
          unit->desired_grenade_index = 0;
        }
      }

      /* [8d] Zoom level change + weapon heat sound */
      zoom_level_new = (char)unit->desired_zoom_level;
      if (zoom_level_new != (char)unit->current_zoom_level) {
        unit->current_zoom_level = zoom_level_new;
        if (zoom_level_new == (char)-1) {
          *(int *)&unit->integrated_night_vision_power = 0;
        }
        i10 = player_index_from_unit_index(unit_handle);
        if (i10 != -1) {
          int pi = player_index_from_unit_index(unit_handle);
          /* player_data, read through its raw address */
          player_data_t *pd = datum_get(PLAYER_DATA, pi);
          if (pd->local_player_index != NONE) {
            refetched = object_get_and_verify_type(unit_handle, _object_mask_unit);
            i10 = unit_inventory_get_weapon(unit_handle, refetched->current_weapon_index);
            if (i10 != -1) {
              int *wo = (int *)object_get_and_verify_type(i10, 4);
              wpn_tag = (int)tag_get(0x77656170, *wo);
              zoom_level_new = (char)unit->current_zoom_level;
              if (zoom_level_new == (char)-1) {
                zoom_sound_ref = weapon_definition_zoom_out_sound_index(wpn_tag);
              } else {
                zoom_sound_ref = weapon_definition_zoom_in_sound_index(wpn_tag);
              }
              zoom_interp = 1.0f;
              if (zoom_level_new != (char)-1 &&
                  weapon_definition_zoom_levels(wpn_tag) > 1) {
                int denom = (int)weapon_definition_zoom_levels(wpn_tag) - 1;
                zoom_interp =
                  (float)(int)(signed char)zoom_level_new / (float)denom;
              }
              if (zoom_sound_ref != -1) {
                sound_impulse_start(zoom_sound_ref, zoom_interp);
              }
            }
          }
        }
      }
    } /* end seat-held check */

    /* [9] Aiming and looking update */
    unit_control_trace(unit_handle, TRACE_STRING_UNIT_UPDATE_PREVECTORS);

    if (unit->aiming_speed == 1) {
      aiming_speed_scale = unit_definition_casual_aiming_modifier(unit_tag);
    } else {
      aiming_speed_scale = 1.0f;
    }

    aim_vec = &unit->aiming_vector;
    aim_vel_limit =
      aiming_speed_scale * unit_definition_aiming_velocity_maximum(unit_tag) * REAL_ONE_THIRTIETH_POOL;
    saved_aim.i = aim_vec->i;
    saved_aim.j = aim_vec->j;
    saved_aim.k = aim_vec->k;
    aim_accel_limit =
      aiming_speed_scale * unit_definition_aiming_acceleration_maximum(unit_tag) * REAL_ONE_OVER_900_POOL;

    /* [9a] Aiming: 3 paths */
    if (aim_vel_limit == 0.0f && aim_accel_limit == 0.0f) {
      /* Instant */
      real_vector3d *des = &unit->desired_aiming_vector;
      assert_halt(valid_real_normal3d(des));
      aim_vec->i = des->i;
      aim_vec->j = des->j;
      aim_vec->k = des->k;
      unit_clip_to_aiming_bounds(unit_handle, aim_vec, 1);
      {
        float *g = global_zero_vector_ptr;
        unit->aiming_velocity.i = g[0];
        unit->aiming_velocity.j = g[1];
        unit->aiming_velocity.k = g[2];
      }
      unit_control_trace(unit_handle, TRACE_STRING_UNIT_UPDATE_AIM_SET);
    } else if (unit->aiming_with_euler_screen == 0) {
      /* Smooth */
      angular_accelerate_to_position(aim_vec, &unit->desired_aiming_vector,
                                     &unit->aiming_velocity,
                                     aim_vel_limit, aim_accel_limit);
      unit_control_trace(unit_handle, TRACE_STRING_UNIT_UPDATE_AIM_3D);
    } else {
      /* Euler */
      real_point3d *origin;
      em.scale = 1.0f;
      object_get_orientation(unit_handle, &em.forward, &em.up);
      em.left.i = em.forward.k * em.up.j - em.up.k * em.forward.j;
      em.left.j = em.up.k * em.forward.i - em.forward.k * em.up.i;
      em.left.k = em.forward.j * em.up.i - em.up.j * em.forward.i;
      origin = GLOBAL_ORIGIN3D;
      em.translation.x = origin->x;
      em.translation.y = origin->y;
      em.translation.z = origin->z;
      unit_euler_aiming_update((int)&em, aim_vec, &unit->desired_aiming_vector,
                   &unit->aiming_velocity,
                   &unit->unk_616, aim_vel_limit,
                   aim_accel_limit);
      unit_control_trace(unit_handle, TRACE_STRING_UNIT_UPDATE_AIM_EULER);
    }

    /* Delta angle + steering byte */
    angle_between_vectors3d(aim_vec, &saved_aim);
    i10 = (int)unit_tag;
    delta_angle = angle_between_vectors3d(aim_vec, &saved_aim);
    ratio = delta_angle / (unit_definition_aiming_velocity_maximum(i10) * REAL_ONE_THIRTIETH_POOL);
    if (ratio < 0.0f) {
      ratio = 0.0f;
    } else if (ratio > 1.0f) {
      ratio = 1.0f;
    }
    ratio = ratio * REAL_255_POOL;
    unit->aiming_change = (char)(int)ratio;

    /* Assert aim vector */
    assert_halt(valid_real_normal3d((real_vector3d *)aim_vec));

    /* [9c] Looking direction */
    aim_vel_limit = aiming_speed_scale * unit_definition_looking_velocity_maximum(i10) * REAL_ONE_THIRTIETH_POOL;
    aim_accel_limit =
      aiming_speed_scale * unit_definition_looking_acceleration_maximum(i10) * REAL_ONE_OVER_900_POOL;

    if (aim_vel_limit == 0.0f && aim_accel_limit == 0.0f) {
      real_vector3d *des = &unit->desired_looking_vector;
      real_vector3d *lk = &unit->looking_vector;
      lk->i = des->i;
      lk->j = des->j;
      lk->k = des->k;
      unit_clip_to_aiming_bounds(unit_handle, lk, 0);
      {
        float *g = global_zero_vector_ptr;
        unit->looking_velocity.i = g[0];
        unit->looking_velocity.j = g[1];
        unit->looking_velocity.k = g[2];
      }
      unit_control_trace(unit_handle, TRACE_STRING_UNIT_UPDATE_LOOK_SET);
    } else if (unit->looking_with_euler_screen == 0) {
      real_vector3d *lk = &unit->looking_vector;
      angular_accelerate_to_position(lk, &unit->desired_looking_vector,
                                     &unit->looking_velocity,
                                     aim_vel_limit, aim_accel_limit);
      unit_control_trace(unit_handle, TRACE_STRING_UNIT_UPDATE_LOOK_3D);
    } else {
      real_vector3d *lk = &unit->looking_vector;
      real_point3d *origin;
      em.scale = 1.0f;
      object_get_orientation(unit_handle, &em.forward, &em.up);
      em.left.i = em.forward.k * em.up.j - em.up.k * em.forward.j;
      em.left.j = em.up.k * em.forward.i - em.forward.k * em.up.i;
      em.left.k = em.forward.j * em.up.i - em.up.j * em.forward.i;
      origin = GLOBAL_ORIGIN3D;
      em.translation.x = origin->x;
      em.translation.y = origin->y;
      em.translation.z = origin->z;
      unit_euler_aiming_update((int)&em, lk, &unit->desired_looking_vector,
                   &unit->looking_velocity,
                   &unit->unk_632, aim_vel_limit,
                   aim_accel_limit);
      unit_control_trace(unit_handle, TRACE_STRING_UNIT_UPDATE_LOOK_EULER);
    }

    /* Assert look vector */
    assert_halt(valid_real_normal3d(&unit->looking_vector));
    unit_control_trace(unit_handle, TRACE_STRING_UNIT_UPDATE_POSTVECTOR);

    /* [10] Combat state switch */
    if (skip_combat_update == 0) {
      uint32_t ff = (uint32_t)unit->control_flags >> 0xd;
      switch ((signed char)unit->grenade_throw_state) {
      case 0:
        if ((ff & 1) != 0) {
          unit_throw_grenade_begin(unit_handle, 0);
        }
        break;
      case 1:
        if (unit->object.unk_130 > 1) {
          unit_throw_grenade_move_to_hand(unit_handle);
        }
        break;
      case 2:
        *(short *)&unit->unk_574 = *(short *)&unit->unk_574 + 1;
        if (unit->animation_state != 0x21) {
          unit_throw_grenade_release(unit_handle, 1);
        }
        break;
      case 3:
        if (unit->animation_state != 0x21 && (ff & 1) == 0) {
          unit->grenade_throw_state = (char)(ff & 1);
        }
        break;
      }
    }

    /* [11] Weapon firing effects */
    if (unit->current_weapon_index != NONE) {
      flash_mod = unit->primary_trigger;
      u15 = 0;

      if (unit->current_weapon_index == unit->desired_weapon_index) {
        /* Current = desired weapon */
        if ((int)unit->persistent_control_timer > 0 &&
            (unit->persistent_control_flags & 0x800) != 0) {
          l_6 = 1;
        } else {
          l_6 = 0;
        }
        if (skip_combat_update == 0) {
          if (integrated_lights_allowed != 0 &&
              (*(unsigned char *)&unit->control_flags & 0x10) != 0) {
            u15 = 1;
          }
          if (((uint32_t)unit->control_flags & 0x800) != 0) {
            u15 |= 2;
          }
          if (((uint32_t)unit->control_flags & 0x1000) != 0) {
            u15 |= 4;
          }
        }
        {
          unit_definition_t *ut = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
          if ((ut->flags & 0x800000) != 0) {
            unit_data_t *ud2 = object_get_and_verify_type(unit_handle, _object_mask_unit);
            int w = unit_inventory_get_weapon(unit_handle, ud2->current_weapon_index);
            /* Set the held weapon's integrated-light (flashlight)
             * power from this unit's power field (unit+0x2f0, a
             * float; 0.0 = off). The original passes it as arg2 via
             * MSVC stack-arg reuse (MOV ECX,[unit+0x2f0]; PUSH ECX);
             * a prior raw 1-arg fn-ptr cast dropped it, leaving
             * light_power reading a stale stack slot (a light-cluster
             * pointer / -1) that corrupted weapon+0x1f8 -> funcval
             * -> flashlight color (over-bright, or NaN -> assert). */
            weapon_set_integrated_light_power(w,
                                              *(int *)&unit->integrated_light_power);
          }
        }
        if (((uint32_t)unit->control_flags & 0x400) != 0) {
          u15 |= 8;
        }
        c6 = unit_animation_busy(&unit->animation_flags);
        if (c6 != 0 && l_6 == 0) {
          u15 |= 0x10;
        }
        if (unit->object.type == 0 &&
            ((biped_data_t *)unit)->player_melee_ticks > 0) {
          u15 |= 0x10;
        }
        if ((char)unit->current_zoom_level != (char)-1) {
          u15 |= 0x40;
        }
      } else {
        u15 = 0x20;
      }

      {
        unit_data_t *ud3 = object_get_and_verify_type(unit_handle, _object_mask_unit);
        int w2 = unit_inventory_get_weapon(unit_handle, ud3->current_weapon_index);
        weapon_owner_update(w2, (int)u15, flash_mod);
      }
    }
  } /* end tag 0x400 check */

  /* [12] Assert forward/up axes valid */
  {
    real_vector3d *forward = &unit->object.forward;
    real_vector3d *up = &unit->object.up;
    c6 = (char)valid_real_normal3d_perpendicular(forward, up);
    if (c6 == 0) {
      char *msg = csprintf(
        ERROR_STRING_BUFFER_FIRST_DWORD,
        "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
        "&unit->object.forward", "&unit->object.up", (double)forward->i,
        (double)forward->j, (double)forward->k, (double)up->i,
        (double)up->j, (double)up->k);
      display_assert(msg, "c:\\halo\\SOURCE\\units\\units.c", 0x483, 1);
      system_exit(-1);
    }
  }
  assert_halt(valid_real_normal3d(&unit->aiming_vector));
  assert_halt(valid_real_normal3d(&unit->looking_vector));

  /* [13] Aim assist blending + weapon seat loop */
  if ((unit_tag->flags & 0x800) == 0) {
    if ((unit->animation_flags & 2) != 0) {
      unit_aiming_vector(unit_handle);
      unit->seat_acceleration.x =
        unit->seat_acceleration.x * 0.7f +
        unit->seat_desired_acceleration.x * 0.3f;
      unit->seat_acceleration.y =
        unit->seat_acceleration.y * 0.7f +
        unit->seat_desired_acceleration.y * 0.3f;
      unit->seat_acceleration.z =
        unit->seat_acceleration.z * 0.7f +
        unit->seat_desired_acceleration.z * 0.3f;
    }

    /* Weapon seat interpolation loop */
    {
      int *tb = (int *)&unit_definition_powered_seats(unit_tag);
      seat_i = 0;
      if (*tb > 0) {
        seat_off = 0;
        do {
          powered_seat = tag_block_get_element(tb, seat_off, sizeof(powered_seat_definition_t));
          if (seat_i == 0) {
            if (unit->driver_object_index.value == -1 &&
                (*(unsigned char *)&unit->flags & 1) == 0) {
              is_held = 0;
            } else {
              is_held = 1;
            }
          } else {
            if (unit->gunner_object_index.value == -1 || unit->gunner_object_index.value == unit->driver_object_index.value) {
              is_held = 0;
            } else {
              is_held = 1;
            }
          }
          if ((unit->object.damage_flags & 4) == 0 && is_held) {
            if (*(int *)&unit->seat_power[seat_off] != 0x3f800000) {
              f20 = 1.0f / (powered_seat->powerup_time * TICKS_PER_SECOND) +
                       unit->seat_power[seat_off];
              unit->seat_power[seat_off] = f20;
              if (f20 > 1.0f) {
                unit->seat_power[seat_off] = 1.0f;
              }
            }
          } else {
            if (unit->seat_power[seat_off] != 0.0f) {
              f20 = unit->seat_power[seat_off] -
                       1.0f / (powered_seat->powerdown_time * TICKS_PER_SECOND);
              unit->seat_power[seat_off] = f20;
              if (f20 < 0.0f) {
                unit->seat_power[seat_off] = 0.0f;
              }
            }
          }
          seat_i++;
          seat_off = (int)seat_i;
        } while (seat_off < *tb);
      }
    }
  }

  /* [14] Delayed damage */
  {
    int16_t et2 = (int16_t)unit->delayed_damage_timer;
    if (et2 > 0) {
      et2--;
      unit->delayed_damage_timer = et2;
      if (et2 == 0) {
        eff_handle = (int)unit->delayed_damage_attacker_object_index;
        eff_data = (int)unit->delayed_damage_peak;
        eff_anim = (int16_t)unit->last_damage_category;
        AI_HANDLE_DAMAGE_FN(
          unit_handle, eff_handle, eff_anim, eff_data, 0, 1);
        unit->last_damage_category = 0;
        unit->delayed_damage_attacker_object_index = -1;
        unit->delayed_damage_peak = 0;
      }
    }
  }

  /* Post-update calls */
  unit_cause_continuous_melee_damage(unit_handle);
  unit_dialogue_update(unit_handle);

  /* Weapon alert sound */
  if ((timer_wrapped != 0 || unit->player_index.value != -1) &&
      (unit_refresh_illumination(unit_handle), DEBUG_UNIT_ILLUMINATION != 0) && unit->player_index.value != -1) {
    i10 = game_time_get();
    if (i10 >= UNIT_ILLUMINATION_DEBUG_LAST_TIME + 0x1e) {
      ERROR_FN(
        2, FORMAT_STRING_PLAYER_ILLUMINATION, (double)unit->ambient_illumination,
        (double)unit->self_illumination);
      UNIT_ILLUMINATION_DEBUG_LAST_TIME = i10;
    }
  }

  /* Flaming death */
  {
    char ft = (char)unit->flaming_death_delay;
    if (ft > 0) {
      ft--;
      unit->flaming_death_delay = ft;
      if (ft == 0) {
        unit_flame_to_death(unit_handle);
      }
    }
  }

  /* [15] Mouth aperture decay */
  {
    float neg = -(unit->mouth_aperture);
    float cl;
    if (REAL_NEG_0_1_POOL <= neg) {
      cl = neg;
      if (neg > REAL_0_1_POOL) {
        cl = REAL_0_1_POOL;
      }
    } else {
      cl = REAL_NEG_0_1_POOL;
    }
    unit->mouth_aperture = cl + unit->mouth_aperture;
  }

  /* Integrated light toggle flags */
  unit_flags = (uint32_t)unit->flags;
  c6 = l_8;
  if ((unit_flags & 0x10000000) != 0) {
    c6 = 1;
    if ((unit_flags & 0x80000) != 0) {
      c6 = l_8;
    }
    unit->flags = (int)(unit_flags & 0xefffffff);
  }
  unit_flags = (uint32_t)unit->flags;
  if ((unit_flags & 0x20000000) != 0) {
    if ((unit_flags & 0x80000) != 0) {
      c6 = 1;
    }
    unit->flags = (int)(unit_flags & 0xdfffffff);
  }

  /* [17] Weapon flash/camo effect */
  if ((integrated_lights_allowed != 0 &&
       (*(unsigned char *)&unit->control_flags & 0x10) != 0) ||
      unit->integrated_light_battery < 0.0f || c6 != 0) {
    refetched = object_get_and_verify_type(unit_handle, _object_mask_unit);
    if (refetched->current_zoom_level != 0xff) {
      refetched = object_get_and_verify_type(unit_handle, _object_mask_unit);
      i10 = unit_inventory_get_weapon(unit_handle, refetched->current_weapon_index);
      if (i10 != -1) {
        int *wo = (int *)object_get_and_verify_type(i10, 4);
        wpn_tag = (int)tag_get(0x77656170, *wo);
        if ((weapon_definition_flags(wpn_tag) & 0x4000) != 0 &&
            (*(unsigned char *)&unit->control_flags & 0x10) != 0) {
          game_glob = (int)game_globals_get();
          assert_halt_msg_at("game_globals", "c:\\halo\\SOURCE\\units\\units.c", 0x516, game_glob != 0);
          fp_iface = tag_block_get_element(&game_globals_first_person_interface(game_glob), 0,
                                           sizeof(game_globals_first_person_interface_t));
          assert_halt_msg_at("game_globals_first_person_interface", "c:\\halo\\SOURCE\\units\\units.c", 0x518, fp_iface != 0);
          if ((unit->flags & 0x4000000) == 0) {
            i10 = fp_iface->night_vision_off_on_effect.tag_index;
          } else {
            i10 = fp_iface->night_vision_on_off_effect.tag_index;
          }
          if (i10 != -1) {
            effect_new_from_object(i10, unit_handle, unit_handle, -1, 0.0f, 0.0f, 0,
                         0);
          }
          unit->flags = unit->flags ^ 0x4000000;
          if ((*(unsigned char *)&unit->control_flags & 0x10) != 0) {
            goto label_flashlight_done;
          }
        }
      }
    }
    /* Normal flashlight toggle */
    if (((unit->flags & 0x80000) != 0 ||
         unit->integrated_light_battery > REAL_0_2_POOL) &&
        unit->object.parent_object_index.value == -1) {
      effect_new_from_object(unit_definition_integrated_light_toggle_effect_index(unit_tag), unit_handle, unit_handle, -1,
                   0.0f, 0.0f, 0, 0);
      unit->flags = unit->flags ^ 0x80000;
    }
  }
label_flashlight_done:

  /* [18] Integrated light power and battery */
  if ((unit->flags & 0x80000) == 0) {
    if (unit->integrated_light_battery < 1.0f) {
      unit->integrated_light_battery =
        unit->integrated_light_battery + REAL_ONE_OVER_900_POOL;
    }
    if (unit->integrated_light_power != 0.0f) {
      f20 = unit->integrated_light_power - REAL_ONE_OVER_24_POOL;
      unit->integrated_light_power = f20;
      if (f20 < 0.0f) {
        unit->integrated_light_power = 0.0f;
      }
    }
  } else {
    if ((unit_tag->flags & 0x1000000) == 0) {
      unit->integrated_light_battery =
        unit->integrated_light_battery - REAL_ONE_OVER_3600_POOL;
    }
    if (unit->object.parent_object_index.value != -1 ||
        (unit->object.damage_flags & 4) != 0) {
      unit->flags = unit->flags & (int)0xfff7ffff;
    }
    if (*(int *)&unit->integrated_light_power != 0x3f800000) {
      f20 = unit->integrated_light_power + REAL_ONE_SIXTH_POOL;
      unit->integrated_light_power = f20;
      if (f20 > 1.0f) {
        unit->integrated_light_power = 1.0f;
      }
    }
  }

  /* [19] Integrated night vision power */
  refetched = object_get_and_verify_type(unit_handle, _object_mask_unit);
  if (refetched->current_zoom_level != 0xff) {
    unit_data_tmp = object_get_and_verify_type(unit_handle, _object_mask_unit);
    wpn_idx = unit_data_tmp->current_weapon_index;
    unit_data_tmp = object_get_and_verify_type(unit_handle, _object_mask_unit);
    if (wpn_idx != -1) {
      if (wpn_idx < 0 || wpn_idx >= 4) {
        display_assert("index>=0 && index<MAXIMUM_WEAPONS_PER_UNIT",
                       "c:\\halo\\SOURCE\\units\\units.c", 0x20ac, 1);
        system_exit(-1);
      }
      i10 = unit_data_tmp->weapon_object_indices[wpn_idx].value;
      if (i10 != -1) {
        int *wo = (int *)object_get_and_verify_type(i10, 4);
        wpn_tag = (int)tag_get(0x77656170, *wo);
        if ((weapon_definition_flags(wpn_tag) & 0x4000) != 0) {
          if ((unit->flags & 0x4000000) == 0) {
            if (unit->integrated_night_vision_power != 0.0f) {
              f20 = unit->integrated_night_vision_power - REAL_ONE_OVER_24_POOL;
              unit->integrated_night_vision_power = f20;
              if (f20 < 0.0f) {
                unit->integrated_night_vision_power = 0.0f;
              }
            }
          } else {
            if (*(int *)&unit->integrated_night_vision_power != 0x3f800000) {
              f20 = unit->integrated_night_vision_power + REAL_ONE_TWELFTH_POOL;
              unit->integrated_night_vision_power = f20;
              if (f20 > 1.0f) {
                unit->integrated_night_vision_power = 1.0f;
              }
            }
          }
        }
      }
    }
  }

  /* [20] Debug trace exit */
  unit_control_trace(unit_handle, TRACE_STRING_UNIT_UPDATE_END);
  if (PROFILE_GLOBAL_ENABLE != 0 && UNIT_UPDATE_SECTION_ENABLED != 0) {
    profile_exit_private(UNIT_UPDATE_SECTION);
  }

  return 1;
}

/* Damage block of a 'jpt!' damage-effect tag, at +0x1c4 of the tag.
 * Size unproven. */
typedef struct {
  uint8_t pad_00[0x2];
  int16_t category;                 /* +0x02 */
  uint32_t flags;                   /* +0x04 */
  uint8_t pad_08[0x14];
  real active_camouflage_damage;    /* +0x1c */
  real stun;                        /* +0x20 */
  real maximum_stun;                /* +0x24 */
  real stun_time;                   /* +0x28 */
  uint8_t pad_2c[0x4];
  real instantaneous_acceleration;  /* +0x30 */
} damage_definition_t;
co(damage_definition_t, category, 0x02);
co(damage_definition_t, flags, 0x04);
co(damage_definition_t, active_camouflage_damage, 0x1c);
co(damage_definition_t, stun, 0x20);
co(damage_definition_t, maximum_stun, 0x24);
co(damage_definition_t, stun_time, 0x28);
co(damage_definition_t, instantaneous_acceleration, 0x30);

/* Element of the game globals player-information block (game globals
 * +0x170, element size 0xf4). */

/* unit_damage_aftermath (0x1b4dc0)
 * Per-unit follow-up after object damage has been applied: delayed-damage
 * bookkeeping, camouflage drain, feign death, the damage "ping" animation,
 * kill/damage credit, damage sound, unzoom, AI damage/death notification,
 * stun accumulation and finally unit_died for lethal or feigned damage.
 *
 * Confirmed TU: assert "unit->unit.feign_death_timer > 0" with
 *   "c:\halo\SOURCE\units\units.c" line 0x1284 (was listed under vehicles.obj).
 * Confirmed: cdecl, 7 dword stack args, void (bare RET, no EAX setup).
 *   [EBP+0x14] is printed as "shld" and [EBP+0x18] as "body" by the
 *   "p%d: body %.2f shld %.2f from %s %s" debug print, so arg 4 is the shield
 *   damage and arg 5 the body damage. Arg 6 is never read.
 * Confirmed: unit_make_damage_sound receives 6 pushes (ADD ESP,0x18):
 *   body damage then shield damage last. */
void unit_damage_aftermath(int unit_handle, void *damage_data_address,
                           unsigned int damage_flags, float shield_damage,
                           float body_damage, int body_damage_multiplier,
                           int body_part)
{
  unit_data_t *unit;
  struct damage_data *damage_data = damage_data_address;
  unit_definition_t *unit_definition;
  damage_definition_t *damage_definition;
  float total_damage;
  char lethal;
  char feigned;
  char instantaneous;

  unit = object_get_and_verify_type(unit_handle, _object_mask_unit);
  unit_definition = tag_get(TAG_GROUP_UNIT, unit->object.tag_index);
  damage_definition = (damage_definition_t *)((char *)tag_get(TAG_GROUP_JPT, *(int *)damage_data) + 0x1c4);
  total_damage = shield_damage + body_damage;
  lethal = (char)(damage_flags & 1);
  feigned = 0;

  (void)body_damage_multiplier;

  if (DEBUG_DAMAGE_TAKEN != 0 && unit->player_index.value != NONE) {
    const char *owner_name = "<unknown>";
    const char *damage_name = "<unknown>";

    if (damage_data->owner_object_index != NONE) {
      object_data_t *owner_object =
        object_get_and_verify_type(damage_data->owner_object_index, _object_mask_all);
      char *owner_definition = tag_get(TAG_GROUP_OBJE, owner_object->tag_index);
      const char *separator = strrchr(object_definition_model_name(owner_definition), '\\');

      if (separator) {
        owner_name = separator + 1;
      }
    }

    if (*(int *)damage_data != NONE) {
      const char *tag_name = tag_get_name(*(int *)damage_data);
      const char *separator = strrchr(tag_name, '\\');

      if (separator) {
        damage_name = separator + 1;
      }
    }

    console_printf(0, "p%d: body %.2f shld %.2f from %s %s",
                   unit->player_index.value & 0xffff, (double)body_damage,
                   (double)shield_damage, owner_name, damage_name);
  }

  {
    float vitality = unit->object.unk_164 + unit->object.unk_168;

    if (vitality > 0.0f) {
      unit->last_damage_category = damage_definition->category;
      unit->delayed_damage_timer = 45;
      if (vitality < *(float *)&unit->delayed_damage_peak) {
        vitality = *(float *)&unit->delayed_damage_peak;
      }
      *(float *)&unit->delayed_damage_peak = vitality;

      if (damage_data->owner_object_index != NONE) {
        unit->delayed_damage_attacker_object_index =
           damage_data->owner_object_index;
      }
    }
  }

  if (unit->flags & 0x10) {
    unit->active_camouflage -= damage_definition->active_camouflage_damage;

    if (unit->active_camouflage < 0.0f) {
      unit->active_camouflage = 0.0f;
    }
  }

  instantaneous = lethal && damage_definition->instantaneous_acceleration >= 2.0f;

  if (!lethal && (unit->flags & 0x2000) &&
      unit_definition_feign_death_threshold(unit_definition) > 0.0f &&
      unit_definition_feign_death_time(unit_definition) > 0.0f &&
      unit->object.unk_144 > 0.0f &&
      unit->object.unk_168 > unit_definition_feign_death_threshold(unit_definition)) {
    float feign_death_ticks =
      (real_random_range(0.0f, 1.0f) + unit_definition_feign_death_time(unit_definition)) *
      30.0f;

    unit->object.damage_flags |= 4;
    feigned = 1;
    unit->feign_death_timer =
      (short)(1.0f > feign_death_ticks ? 1.0f : feign_death_ticks);
    if (!((short)unit->feign_death_timer > 0)) {
      display_assert("unit->unit.feign_death_timer > 0",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x1284, true);
      system_exit(-1);
    }
  }

  if (!(damage_data->flags & 0x10) &&
      (lethal || feigned || !(unit->object.damage_flags & 4)) &&
      !(unit->flags & 0x800000) &&
      !(damage_definition->flags & 0x10)) {
    real_vector2d damage_direction;
    real_vector2d unit_forward;
    char resists_pings;
    char animation_flag;
    char direction_valid;
    float angle;

    damage_direction.i = damage_data->direction.i;
    damage_direction.j = damage_data->direction.j;
    unit_forward.i = unit->object.forward.i;
    unit_forward.j = unit->object.forward.j;
    resists_pings = 0;
    animation_flag = 0;
    direction_valid = 0;
    angle = 0.0f;

    if (normalize2d(&damage_direction) > 0.0f &&
        normalize2d(&unit_forward) > 0.0f) {
      angle = signed_angle_between_vectors2d(&damage_direction.i, &unit_forward.i);
      direction_valid = 1;
    }

    if ((unit_definition->flags & 0x80) &&
        !(damage_definition->flags & 4)) {
      resists_pings = 1;
    }

    if (unit->flaming_death_delay > 0) {
      resists_pings = 1;
    }

    if (damage_flags & 0x8a) {
      animation_flag = 1;
    }

    unit_ping_animation(unit_handle, lethal, feigned, instantaneous,
                        resists_pings, animation_flag, angle, body_part,
                        (int)(direction_valid ? &damage_direction : NULL));
  }

  {
    int owner_player_index = damage_data->owner_player_index;
    int player_index = unit->player_index.value;

    if (owner_player_index != NONE && player_index != NONE) {
      game_engine_player_damaged_player(owner_player_index, player_index,
                                        (char)(damage_flags >> 4) & 1);
    }
  }

  if (damage_data->owner_player_index != NONE || damage_data->owner_object_index != NONE) {
    unit_record_damage(unit_handle, total_damage,
                       (unsigned short)damage_definition->category, lethal,
                       damage_data->owner_player_index, (unsigned short)damage_data->owner_team_index,
                       damage_data->owner_object_index);
  }

  if (!(damage_data->flags & 0x10) &&
      ((damage_flags & 1) || body_damage > 0.0f || shield_damage > 0.0f)) {
    unit_make_damage_sound(unit_handle, (int *)damage_data, lethal | feigned,
                           (char)(damage_flags >> 6) & 1, body_damage,
                           shield_damage);
  }

  if (body_damage > 0.0f || shield_damage > 0.0f) {
    unit_reset_weapon_state(unit_handle);
  }

  if (unit->object.type == _object_type_biped) {
    if (lethal) {
      ai_handle_death(unit_handle, damage_data->owner_object_index,
                      (unsigned short)damage_definition->category);
    } else if (!(unit->object.damage_flags & 4)) {
      ai_handle_damage(unit_handle, damage_data->owner_object_index,
                       (unsigned short)damage_definition->category,
                       total_damage, (int)&damage_data->direction, 0);
    }
  }

  if (unit->player_index.value != NONE &&
      damage_definition->stun > 0.0f &&
      (game_engine_running() || STUN_ENABLE != 0)) {
    game_globals_player_information_t *player_information =
      tag_block_get_element(&game_globals_player_information(game_globals_get()), 0, 0xf4);
    float stun_increment =
      damage_definition->stun * damage_data->scale;
    float maximum_stun =
      damage_definition->maximum_stun * damage_data->scale;
    short stun_ticks;
    short minimum_stun_ticks;
    short maximum_stun_ticks;

    if (stun_increment < 0.0f) {
      stun_increment = 0.0f;
    }
    if (maximum_stun < 0.0f) {
      maximum_stun = 0.0f;
    } else if (maximum_stun >= 1.0f) {
      maximum_stun = 1.0f;
    }

    if (unit->body_stun < maximum_stun) {
      unit->body_stun += stun_increment;
      if (unit->body_stun > maximum_stun) {
        unit->body_stun = maximum_stun;
      }
    }

    stun_ticks = (short)(damage_definition->stun_time * 30.0f);
    minimum_stun_ticks = (short)(player_information->minimum_stun_time * 30.0f);
    maximum_stun_ticks = (short)(player_information->maximum_stun_time * 30.0f);

    if ((short)unit->body_stun_ticks < minimum_stun_ticks) {
      unit->body_stun_ticks = minimum_stun_ticks;
    }
    unit->body_stun_ticks += stun_ticks;
    if ((short)unit->body_stun_ticks > maximum_stun_ticks) {
      unit->body_stun_ticks = maximum_stun_ticks;
    }
  }

  if (lethal || feigned) {
    unit_died(unit_handle, feigned);
  }
}
