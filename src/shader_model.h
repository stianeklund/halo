#ifndef SHADER_MODEL_H
#define SHADER_MODEL_H

#include "types.h"

/* Kept out of types.h on purpose: adding these to that universally included
 * header shifts VC71's internal counters in every translation unit, which
 * swaps stack slots in unrelated functions (e.g. action_obey_command_list_setup
 * 256 -> 254 of 350 raw-XBE bytes). Only the two rasterizer files below use it. */

/* Names and offsets below are proven from the 2276 binary, not from PAL:
 *   shader->base.type            +0x24, word: `cmp word ptr [esi+0x24], 4`
 *                                (0x16c6f0) guarded by the assert string
 *                                "shader->base.type==_shader_type_model"
 *                                (0x2a3974), so the type value is 4.
 *   shader_model->model.self_illumination_animation_period
 *                                +0x74, float: `fld [esi+0x74]` (0x16cb32)
 *                                guarded by that assert string (0x2a38fc).
 * Every other accessed offset is behavior-only and stays field_<hex>. The size
 * of `base` and where `model` starts are not proven here. */
enum {
  _shader_type_model = 4
};

typedef struct shader_base {
  byte pad_00[0x24];
  int16_t type;      ///< offset=0x24  shader->base.type
} shader_base;
cs(shader_base, 0x26);
co(shader_base, type, 0x24);

/// Type-4 (_shader_type_model) shader, as returned by
/// shader_get_and_verify_type(shader, _shader_type_model). Read by FUN_00172de0
/// (rasterizer_xbox_shadows.c) and the active-camouflage path of
/// rasterizer_xbox_decals.c. Size is unproven.
typedef struct shader_model {
  shader_base base;                   ///< offset=0x00
  byte pad_26[2];
  byte field_28;     ///< offset=0x28  bit 1 selects D3DCULL_NONE over D3DCULL_CCW; bit 2 selects PSTextureModes(0) (no stage-0 bind) over PSTextureModes(1) + bind field_b0
  byte pad_29[0x74 - 0x29];
  real self_illumination_animation_period;  ///< offset=0x74  proven, see above
  byte pad_78[0x9c - 0x78];
  real field_9c;     ///< offset=0x9c  times parameters->field_c4 (FUN_00190e10 arg 3)
  real field_a0;     ///< offset=0xa0  times parameters->field_c8 (FUN_00190e10 arg 4)
  byte pad_a4[0xb0 - 0xa4];
  int32_t field_b0;  ///< offset=0xb0  tag index handed to rasterizer_set_texture
  byte pad_b4[0xd8 - 0xb4];
  real field_d8;     ///< offset=0xd8  vertex-shader constant c0.x; c0.y = field_ec * field_d8
  byte pad_dc[0xec - 0xdc];
  real field_ec;     ///< offset=0xec
  byte pad_f0[0xfc - 0xf0];
  byte field_fc;     ///< offset=0xfc  passed as arg 1 of FUN_00190e10, whose `texture_animation` parameter is asserted non-null (shaders.c:0x113) and whose words at +0x00/+0x10/+0x20 (u_source/v_source/r_source) are asserted < NUMBER_OF_OBJECT_FUNCTION_REFERENCES (0x190e48..0x190eaa)
} shader_model;
co(shader_model, base, 0x00);
co(shader_model, field_28, 0x28);
co(shader_model, self_illumination_animation_period, 0x74);
co(shader_model, field_9c, 0x9c);
co(shader_model, field_a0, 0xa0);
co(shader_model, field_b0, 0xb0);
co(shader_model, field_d8, 0xd8);
co(shader_model, field_ec, 0xec);
co(shader_model, field_fc, 0xfc);

/// Shadow parameter block stashed by FUN_00172590 (environment_shadows_globals
/// local_parameters) and read by FUN_00172de0. Only the accessed offsets are
/// recovered; size is unproven.
typedef struct shadow_parameters {
  byte pad_00[8];
  byte field_08;     ///< offset=0x08  address passed to rasterizer_set_model_skinning
  byte pad_09[0x84 - 0x09];
  byte field_84;     ///< offset=0x84  passed as arg 2 of FUN_00190e10: nullable pointer whose +4 is a float table indexed by (source - 1); a source of 0 reads 1.0f (0x190f7a..0x190fc4)
  byte pad_85[0xc4 - 0x85];
  real field_c4;     ///< offset=0xc4  times shader field_9c
  real field_c8;     ///< offset=0xc8  times shader field_a0
} shadow_parameters;
co(shadow_parameters, field_08, 0x08);
co(shadow_parameters, field_84, 0x84);
co(shadow_parameters, field_c4, 0xc4);
co(shadow_parameters, field_c8, 0xc8);

#endif
