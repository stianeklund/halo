/* c:\halo\SOURCE\objects\objects.h
 *
 * Header path recovered from the XBE: asserts inside this header's inline
 * functions stamp "..\objects\objects.h" into __FILE__, which proves both
 * that the header existed and the directory it lived in. See the
 * header-recovery skill for the extraction sweep.
 *
 * Holds the object record layouts whose use is confined to src/halo/objects/.
 * object_globals_t stays in types.h: a kb.json declaration names it, so it
 * must be visible inside the generated decl.h, which is included ahead of
 * this header.
 * object_data_t itself stays in types.h: it is embedded by unit_data_t and
 * weapon_data_t and read by units/, game/ and physics/ TUs, so it is a
 * genuinely shared type. item_data_t, weapon_trigger_data_t and
 * weapon_magazine_data_t likewise stay, because weapon_data_t (types.h)
 * embeds them and the dependency must point inward, never back out. */

#ifndef HALO_OBJECTS_OBJECTS_H
#define HALO_OBJECTS_OBJECTS_H

#include "../../types.h"

/* Integer game-tick rate; TICKS_PER_SECOND (common.h) is the 30.0f pool. */
#define TICKS_PER_SECOND_INT 30

/// size=0xc
typedef struct {
  uint16_t unk_0;         ///< offset=0x00
  uint8_t unk_2;          ///< offset=0x02  see .text:0013FF78                 or      byte ptr [esi+2], 40h  flags
  uint8_t type;           ///< offset=0x03  see .text:000F68C3                 movzx   ax, byte ptr [eax+3]
  uint16_t unk_4;         ///< offset=0x04  cluster_index?
  uint16_t data_size;     ///< offset=0x06  see .text:0013E015                 movsx   eax, word ptr [edi+6]
  object_data_t* object;  ///< offset=0x08  see .text:0013D80E                 mov     esi, [eax+8]
} object_header_data_t;

// OBJE -> UNIT -> BIPD
/// size=0x480
typedef struct {
  unit_data_t unit;         ///< offset=0x000
  uint32_t flags;           ///< offset=0x424 .text:001A29F5                 test    byte ptr [esi+424h], 1   ; 1A9BEE shows it's 32-bit
  uint8_t unk_1064;         ///< offset=0x428 .text:001A0EDF                 mov     byte ptr [esi+428h], 0
  uint8_t unk_1065;         ///< offset=0x429 .text:001A0EED                 mov     [esi+429h], al
  uint8_t unk_1066;         ///< offset=0x42A .text:001A2567                 movsx   eax, byte ptr [esi+42Ah]
  uint8_t unk_1067;         ///< offset=0x42B .text:001A4A37                 mov     byte ptr [esi+42Bh], 0
  uint32_t unk_1068;        ///< offset=0x42C .text:00095FBE                 mov     edx, [eax+42Ch]
  uint32_t unk_1072;        ///< offset=0x430 .text:001A0874                 mov     [eax+430h], ecx
  uint32_t unk_1076;        ///< offset=0x434 .text:001A087A                 mov     [eax+434h], ecx
  vector3_t unk_1080;       ///< offset=0x438 .text:0003E1D6                 add     eax, 438h
  uint32_t unk_1092;        ///< offset=0x444 .text:001A1C28                 cmp     eax, [esi+444h]  game time related
  uint32_t unk_1096;        ///< offset=0x448 .text:001A0880                 mov     [eax+448h], ecx
  uint32_t unk_1100;        ///< offset=0x44C .text:001A4A0D                 mov     [esi+44Ch], ebx
  uint32_t unk_1104;        ///< offset=0x450 .text:001A0848                 mov     dword ptr [esi+450h], 0FFFFFFFFh
  datum_handle_t unk_1108;  ///< offset=0x454 .text:001A0AB9                 cmp     [esi+454h], edi 
  uint8_t unk_1112;         ///< offset=0x458 .text:001A0B1D                 mov     byte ptr [esi+458h], 0F1h
  uint8_t unk_1113;         ///< offset=0x459 .text:001A1EE2                 cmp     byte ptr [esi+459h], 1Eh
  uint8_t unk_1114;         ///< offset=0x45A .text:001A2B28                 mov     al, [esi+45Ah]
  uint8_t unk_1115;         ///< offset=0x45B .text:001A2576                 mov     byte ptr [esi+45Bh], 1
  uint8_t unk_1116;         ///< offset=0x45C .text:001A2406                 mov     byte ptr [esi+45Ch], 0
  uint8_t unk_1117;         ///< offset=0x45D .text:001A66F4                 mov     [esi+45Dh], bl
  uint8_t unk_1118;         ///< offset=0x45E .text:001A66EE                 mov     [esi+45Eh], dl
  uint8_t unk_1119;         ///< offset=0x45F
  uint16_t unk_1120;        ///< offset=0x460 .text:001A0ED6                 mov     [esi+460h], cx 
  uint16_t unk_1122;        ///< offset=0x462 
  float unk_1124;           ///< offset=0x464 .text:001A0905                 fmul    dword ptr [edi+464h]
  float unk_1128;           ///< offset=0x468 .text:001A4586                 fld     dword ptr [esi+468h]
  vector3_t unk_1132;       ///< offset=0x46C .text:001A0826                 lea     edx, [esi+46Ch]
  uint32_t unk_1144;        ///< offset=0x478 .text:001A5F90                 mov     [edi+478h], edx
  uint8_t unk_1148;         ///< offset=0x47C .text:0019FAF7                 mov     dl, [esi+47Ch]
  uint8_t unk_1149;         ///< offset=0x47D .text:0019FAE0                 mov     cl, [esi+47Dh]
  char unk_1150[2];         ///< offset=0x47E
} biped_data_t;

// OBJE -> ITEM -> EQUI
/// size=0x1F4
typedef struct {
  item_data_t item;       ///< offset=0x000
  char unk_476[0x18];     ///< offset=0x1DC
} equipment_data_t;

// OBJE -> ITEM -> GARB
/// size=0x1F4
typedef struct {
  item_data_t item;       ///< offset=0x000
  uint16_t unk_476;       ///< offset=0x1DC .text:000F6833                 dec     word ptr [eax+1DCh]
  char unk_478[0x16];     ///< offset=0x1DE
} garbage_data_t;

#define NUMBER_OF_INCOMING_OBJECT_FUNCTIONS 4
#define MAXIMUM_ATTACHMENTS_PER_OBJECT 8
#define TAG_GROUP_PROJ 0x70726f6a /* 'proj' */

// OBJE -> PROJ
/// size=0x228
typedef struct {
  object_data_t object;            ///< offset=0x000
  char pad_1a4[0x1dc - 0x1a4];     ///< offset=0x1A4  item block; never accessed by projectile code
  uint32_t flags;                  ///< offset=0x1DC  .text:000F7CBE mov ecx, [eax+1DCh]; bits _projectile_*_bit
  int16_t action;                  ///< offset=0x1E0  .text:000F7E4B cmp si, [eax+1E0h]; projectile_set_action only raises it
  int16_t hit_material_type;       ///< offset=0x1E2  .text:000F8D84 mov [esi+1E2h], bx
  int32_t ignore_object_index;     ///< offset=0x1E4  .text:000F8D90 mov [esi+1E4h], eax
  int32_t target_object_index;     ///< offset=0x1E8  .text:000F7D44 mov [eax+1E8h], ecx
  int32_t tracer_attachment_index; ///< offset=0x1EC  .text:000F9CAC index into object.attachment_indices
  real detonation_timer;           ///< offset=0x1F0  .text:000F8A91 fmul dword ptr [edi+1F0h]
  real detonation_timer_delta;     ///< offset=0x1F4  .text:000F8DEC fstp dword ptr [esi+1F4h]
  real arming_time;                ///< offset=0x1F8  .text:000F9DBD fld dword ptr [ebx+1F8h]
  real arming_time_delta;          ///< offset=0x1FC  .text:000F8E15 1/(definition arming_time * 30), added to arming_time each tick
  real odometer;          ///< offset=0x200  .text:000F7F66 accumulates per-tick displacement; compared to maximum_range
  real field_204;                  ///< offset=0x204  .text:000F8702 mov dword ptr [esi+204h], 3F800000h
  real field_208;                  ///< offset=0x208  .text:000F86AB per-tick increment of field_204
  real deceleration;               ///< offset=0x20C  .text:000F8677 projectile_calculate_deceleration_from_distances result
  real field_210;                  ///< offset=0x210  .text:000FA304 fcomp dword ptr [ebx+210h]
  real_vector3d field_214;         ///< offset=0x214  .text:000F85E7 angular_velocity / |angular_velocity|
  real field_220;                  ///< offset=0x220  .text:000F8605 sine of the angular speed
  real field_224;                  ///< offset=0x224  .text:000F860D cosine of the angular speed
} projectile_data_t;
cs(projectile_data_t, 0x228);
co(projectile_data_t, flags, 0x1dc);
co(projectile_data_t, action, 0x1e0);
co(projectile_data_t, tracer_attachment_index, 0x1ec);
co(projectile_data_t, detonation_timer, 0x1f0);
co(projectile_data_t, arming_time, 0x1f8);
co(projectile_data_t, odometer, 0x200);
co(projectile_data_t, deceleration, 0x20c);
co(projectile_data_t, field_214, 0x214);
co(projectile_data_t, field_224, 0x224);
co(object_data_t, owner_object_index, 0x74);
co(object_data_t, field_d0, 0xd0);
co(object_data_t, outgoing_function_values, 0xe4);
co(object_data_t, attachment_types, 0xf4);
co(object_data_t, attachment_indices, 0xfc);

enum {
  _projectile_tracer_bit = 1,   /* projectile_make_tracer / projectile_kill_tracer */
  _projectile_attached_bit = 3  /* cleared by projectile_handle_parent_destroyed before the detach */
};

/* 'proj' tag definition: the 0x17c-byte object definition followed by the
 * projectile block.  Offsets from the accesses in items/projectiles.c. */
typedef struct {
  char pad_000[0x140];                 ///< offset=0x000  object definition header
  tag_block attachments;               ///< offset=0x140  0x48-byte elements; index selects the tracer attachment (0xf8d50)
  char pad_14c[0x17c - 0x14c];         ///< offset=0x14C
  uint32_t flags;                      ///< offset=0x17C
  int16_t detonation_timer_starts;     ///< offset=0x180
  int16_t impact_noise;                ///< offset=0x182  ai sound volume passed on impact
  int16_t function_inputs[NUMBER_OF_INCOMING_OBJECT_FUNCTIONS]; ///< offset=0x184  projectile_export_function_values
  tag_reference super_detonation;      ///< offset=0x18C
  real ai_perception_radius;           ///< offset=0x19C
  real collision_radius;               ///< offset=0x1A0
  real arming_time;                    ///< offset=0x1A4  seconds
  real danger_radius;                  ///< offset=0x1A8
  tag_reference effect;                ///< offset=0x1AC
  real timer_lower_bound;              ///< offset=0x1BC
  real timer_upper_bound;              ///< offset=0x1C0
  real minimum_velocity;               ///< offset=0x1C4
  real maximum_range;                  ///< offset=0x1C8
  real air_gravity_scale;              ///< offset=0x1CC
  real air_damage_range_lower_bound;   ///< offset=0x1D0
  real air_damage_range_upper_bound;   ///< offset=0x1D4
  real water_gravity_scale;            ///< offset=0x1D8
  real water_damage_range_lower_bound; ///< offset=0x1DC
  real water_damage_range_upper_bound; ///< offset=0x1E0
  real initial_velocity;               ///< offset=0x1E4
  real final_velocity;                 ///< offset=0x1E8
  real guided_angular_velocity;        ///< offset=0x1EC  radians per second
  int16_t detonation_noise;            ///< offset=0x1F0
  uint8_t pad_1f2[2];                  ///< offset=0x1F2
  tag_reference detonation_started;    ///< offset=0x1F4
  tag_reference flyby_sound;           ///< offset=0x204
  tag_reference attached_detonation_damage; ///< offset=0x214
  tag_reference impact_damage;         ///< offset=0x224
  uint8_t pad_234[0xc];                ///< offset=0x234
  tag_block material_responses;        ///< offset=0x240  0xa0-byte elements
} projectile_definition_t;
co(projectile_definition_t, attachments, 0x140);
co(projectile_definition_t, flags, 0x17c);
co(projectile_definition_t, function_inputs, 0x184);
co(projectile_definition_t, super_detonation, 0x18c);
co(projectile_definition_t, collision_radius, 0x1a0);
co(projectile_definition_t, effect, 0x1ac);
co(projectile_definition_t, maximum_range, 0x1c8);
co(projectile_definition_t, air_gravity_scale, 0x1cc);
co(projectile_definition_t, water_gravity_scale, 0x1d8);
co(projectile_definition_t, initial_velocity, 0x1e4);
co(projectile_definition_t, guided_angular_velocity, 0x1ec);
co(projectile_definition_t, detonation_noise, 0x1f0);
co(projectile_definition_t, detonation_started, 0x1f4);
co(projectile_definition_t, flyby_sound, 0x204);
co(projectile_definition_t, attached_detonation_damage, 0x214);
co(projectile_definition_t, impact_damage, 0x224);
co(projectile_definition_t, material_responses, 0x240);

/* object_get_and_verify_type mask for projectiles (object type 5); every
 * projectile accessor passes 0x20. */
enum {
  _object_type_projectile = 5,
  _object_mask_projectile = 1 << _object_type_projectile
};

/* collision_test_result.field_00 type values.  "collision.type==
 * _collision_result_object" is an assert string at 0x29af70; that type
 * carries the hit object in field_38. */
enum {
  _collision_result_object = 3
};

/* 0x31f3a0: {"normal", "incident", "negative incident", "reflection",
 * "gravity"} -- the marker names handed to every projectile effect. */
#define NUMBER_OF_EFFECT_MARKERS 5
#define effect_marker_names ((void *)0x31f3a0)

/* The incoming function values start one slot into object.field_d0 (+0xd4). */
#define object_incoming_function_values(object) (&(object)->field_d0[1])

#define projectile_get(index) \
  ((projectile_data_t *)object_get_and_verify_type((index), _object_mask_projectile))
#define projectile_definition_get(index) \
  ((projectile_definition_t *)tag_get(TAG_GROUP_PROJ, (index)))

// OBJE -> SCEN
/// size=0x1A8
typedef struct {
  object_data_t object;   ///< offset=0x000
  char unk_420[4];        ///< offset=0x1A4
} scenery_data_t;

// OBJE -> DEVI
/// size=0x1C4
typedef struct {
  object_data_t object;   ///< offset=0x000
  uint8_t flags;          ///< offset=0x1A4   .text:00096784                 test    byte ptr [edi+1A4h], 2
  char unk_421[3];        ///< offset=0x1A5
  uint16_t unk_424;       ///< offset=0x1A8   .text:000960EB                 mov     [esi+1A8h], ax
  uint16_t unk_426;       ///< offset=0x1AA
  float unk_428;          ///< offset=0x1AC   .text:00096182                 fld     dword ptr [edi+1ACh]
  float unk_432;          ///< offset=0x1B0   .text:0009618D                 fld     dword ptr [edi+1B0h]
  uint16_t unk_436;       ///< offset=0x1B4   .text:000960E4                 mov     [esi+1B4h], ax
  uint16_t unk_438;       ///< offset=0x1B6
  float unk_440;          ///< offset=0x1B8   .text:000961BB                 fld     dword ptr [edi+1B8h]
  float unk_444;          ///< offset=0x1BC   .text:000961C6                 fld     dword ptr [edi+1BCh]
  uint16_t unk_448;       ///< offset=0x1C0   .text:000962CD                 movsx   edx, word ptr [edi+1C0h]
  uint16_t unk_450;       ///< offset=0x1C2
} device_data_t;

// OBJE -> DEVI -> MACH
/// size=0x1D8
typedef struct {
  device_data_t device;   ///< offset=0x000
  uint32_t flags;         ///< offset=0x1C4   .text:00096247                 mov     ecx, [esi+1C4h]
  uint32_t unk_456;       ///< offset=0x1C8   .text:00095EB2                 mov     edx, [esi+1C8h]
  vector3_t unk_460;      ///< offset=0x1CC   .text:00095F1E                 fsub    dword ptr [esi+1CCh]
} machine_data_t;

// OBJE -> DEVI -> CTRL
/// size=0x1CC
typedef struct {
  device_data_t device;   ///< offset=0x000
  uint32_t flags;         ///< offset=0x1C4   .text:0009571F                 or      [esi+1C4h], eax
  datum_handle_t unk_456; ///< offset=0x1C8   .text:000D06C1                 cmp     word ptr [esi+1C8h], 0FFFFh    datum_handle?
} control_data_t;

// OBJE -> DEVI -> LIFI
/// size=0x1DC
typedef struct {
  device_data_t device;   ///< offset=0x000
  char unk_452[0x10];     ///< offset=0x1C4
  uint32_t unk_468;       ///< offset=0x1D4 .text:00095A08                 mov     [esi+1D4h], ecx
  uint32_t unk_472;       ///< offset=0x1D8 .text:00095A12                 mov     [esi+1D8h], edx
} light_fixture_data_t;

// OBJE -> PLAC
/// size=0x1FC
typedef struct {
  object_data_t object;   ///< offset=0x000
  char unk_420[0x58];     ///< offset=0x1A4
} placeholder_data_t;

// OBJE -> SSCE
/// size=0x1A8
typedef struct {
  object_data_t object;   ///< offset=0x000
  char unk_420[4];        ///< offset=0x1A4
} sound_scenery_data_t;

/* ---------- lights (object_lights.c) */

#define LIGHT_DEFINITION_FLAG_DYNAMIC 0x1
#define LIGHT_DEFINITION_FLAG_NO_SPECULAR 0x2
#define LIGHT_DEFINITION_FLAG_DONT_LIGHT_OWN_OBJECT 0x4
#define LIGHT_DEFINITION_FLAG_SUPERSIZE_IN_FIRST_PERSON 0x8
#define LIGHT_DEFINITION_FLAG_FIRST_PERSON_FLASHLIGHT 0x10
#define LIGHT_DEFINITION_FLAG_DONT_FADE_ACTIVE_CAMOUFLAGE 0x20

#define POINT_LIGHT_FLAG_DYNAMIC 0x1
#define POINT_LIGHT_FLAG_CONNECTS_TO_MAP 0x2
#define POINT_LIGHT_FLAG_CONNECTED_TO_MAP 0x4
#define POINT_LIGHT_FLAG_ATTACHED_TO_FIRST_PERSON_WEAPON 0x8

#define MAXIMUM_SCENE_POINT_LIGHTS 0x80
#define MAXIMUM_QUEUED_LENS_FLARES 8
#define MAXIMUM_LENS_FLARES_PER_LIGHT 8
#define MAXIMUM_RENDERED_POINT_LIGHTS 2
#define MAXIMUM_CLUSTERS_PER_LIGHT 0x200

/* real_rgb_color lives in src/types.h (render_animation, a kb.json-visible
 * type, needs it). */

/// Prefix of the 'ligh' tag definition; the full size is not yet proven.
typedef struct {
  int32_t flags;                     ///< offset=0x00  LIGHT_DEFINITION_FLAG_*
  real radius;                       ///< offset=0x04
  real radius_modifier_lower;        ///< offset=0x08
  real radius_modifier_upper;        ///< offset=0x0c
  char pad_10[4];                    ///< offset=0x10
  real cutoff_angle;                 ///< offset=0x14
  real lens_flare_only_radius;       ///< offset=0x18
  char pad_1c[4];                    ///< offset=0x1c
  real cosine_cutoff_angle;          ///< offset=0x20
  real specular_radius_multiplier;   ///< offset=0x24
  real sine_cutoff_angle;            ///< offset=0x28
  char pad_2c[8];                    ///< offset=0x2c
  uint32_t color_interpolation_flags; ///< offset=0x34
  real_argb_color color_lower_bound; ///< offset=0x38
  real_argb_color color_upper_bound; ///< offset=0x48
  char pad_58[0x60];                 ///< offset=0x58
  int32_t lens_flare_index;          ///< offset=0xb8  'lens' tag index
  char pad_bc[0x38];                 ///< offset=0xbc
  real transition_duration;          ///< offset=0xf4
  char pad_f8[2];                    ///< offset=0xf8
  int16_t falloff_function;          ///< offset=0xfa
} light_definition_t;
co(light_definition_t, radius, 0x04);
co(light_definition_t, radius_modifier_lower, 0x08);
co(light_definition_t, radius_modifier_upper, 0x0c);
co(light_definition_t, cutoff_angle, 0x14);
co(light_definition_t, lens_flare_only_radius, 0x18);
co(light_definition_t, cosine_cutoff_angle, 0x20);
co(light_definition_t, specular_radius_multiplier, 0x24);
co(light_definition_t, sine_cutoff_angle, 0x28);
co(light_definition_t, color_interpolation_flags, 0x34);
co(light_definition_t, color_lower_bound, 0x38);
co(light_definition_t, color_upper_bound, 0x48);
co(light_definition_t, lens_flare_index, 0xb8);
co(light_definition_t, transition_duration, 0xf4);
co(light_definition_t, falloff_function, 0xfa);

/// Attached lights use color_function_index; lights created unattached reuse
/// the same bytes as a node-relative position (see light_datum_t.field_58).
typedef union {
  int16_t color_function_index;   ///< offset=0x00
  real_point3d relative_position; ///< offset=0x00
} light_attachment_t;
cs(light_attachment_t, 0xc);

/// size=0x7c (game_state_data_new("lights", 0x380, 0x7c)).
typedef struct {
  char pad_00[2];                  ///< offset=0x00
  uint16_t flags;                  ///< offset=0x02  POINT_LIGHT_FLAG_*
  int32_t definition_index;        ///< offset=0x04
  int32_t rasterizer_light_index;  ///< offset=0x08
  int32_t marker;                  ///< offset=0x0c
  int32_t cluster_reference;       ///< offset=0x10
  real_rgb_color color;            ///< offset=0x14
  char pad_20[0xc];                ///< offset=0x20
  int32_t object_index;            ///< offset=0x2c
  real_point3d position;           ///< offset=0x30
  real_vector3d forward;           ///< offset=0x3c
  real_vector3d up;                ///< offset=0x48
  real radius;                     ///< offset=0x54
  int32_t field_58;                ///< offset=0x58  creation game time; NONE when attached
  int16_t attachment_marker_index; ///< offset=0x5c
  int16_t function_index;          ///< offset=0x5e
  light_attachment_t attachment;   ///< offset=0x60
  real_vector3d relative_forward;  ///< offset=0x6c
  real scale;                      ///< offset=0x78
} light_datum_t;
cs(light_datum_t, 0x7c);
co(light_datum_t, flags, 0x02);
co(light_datum_t, rasterizer_light_index, 0x08);
co(light_datum_t, marker, 0x0c);
co(light_datum_t, color, 0x14);
co(light_datum_t, object_index, 0x2c);
co(light_datum_t, position, 0x30);
co(light_datum_t, forward, 0x3c);
co(light_datum_t, up, 0x48);
co(light_datum_t, radius, 0x54);
co(light_datum_t, field_58, 0x58);
co(light_datum_t, attachment_marker_index, 0x5c);
co(light_datum_t, function_index, 0x5e);
co(light_datum_t, attachment, 0x60);
co(light_datum_t, relative_forward, 0x6c);
co(light_datum_t, scale, 0x78);

/// size=0x08 (element size passed to game_state_data_new @0x134b50).
typedef struct {
  int16_t datum_salt;  ///< offset=0x00  standard data_t element prefix
  uint8_t pad_02[2];   ///< offset=0x02
  int32_t field_04;    ///< offset=0x04  MOV dword [EAX+0x4],EDX @0x134c0c (light_volume_new param)
} light_volume_datum_t;
cs(light_volume_datum_t, 0x08);
co(light_volume_datum_t, field_04, 0x04);

#define MAXIMUM_LIGHT_VOLUMES 256 /* game_state_data_new count @0x134b52 */

/// size=0x28 (the lens flare queue stride).
typedef struct {
  void *definition;                ///< offset=0x00  'lens' tag
  real_point3d position;           ///< offset=0x04
  uint32_t compressed_direction;   ///< offset=0x10
  uint32_t compressed_up;          ///< offset=0x14
  uint32_t compressed_light_color; ///< offset=0x18
  int16_t light_identifier;        ///< offset=0x1c
  int16_t light_index;             ///< offset=0x1e
  int16_t lens_flare_index;        ///< offset=0x20
  uint8_t compressed_window_index; ///< offset=0x22
  uint8_t compressed_light_scale;  ///< offset=0x23
  char pad_24[4];                  ///< offset=0x24
} lens_flare_parameters_t;
cs(lens_flare_parameters_t, 0x28);
co(lens_flare_parameters_t, position, 0x04);
co(lens_flare_parameters_t, compressed_direction, 0x10);
co(lens_flare_parameters_t, compressed_up, 0x14);
co(lens_flare_parameters_t, compressed_light_color, 0x18);
co(lens_flare_parameters_t, light_identifier, 0x1c);
co(lens_flare_parameters_t, light_index, 0x1e);
co(lens_flare_parameters_t, lens_flare_index, 0x20);
co(lens_flare_parameters_t, compressed_window_index, 0x22);
co(lens_flare_parameters_t, compressed_light_scale, 0x23);

/// size=0x350.
typedef struct {
  char marker_initialized;                ///< offset=0x000
  char pad_001[3];                        ///< offset=0x001
  int32_t marker;                         ///< offset=0x004
  int16_t scene_point_light_count;        ///< offset=0x008
  char pad_00a[2];                        ///< offset=0x00a
  int32_t scene_point_lights[MAXIMUM_SCENE_POINT_LIGHTS]; ///< offset=0x00c
  lens_flare_parameters_t queued_lens_flares[MAXIMUM_QUEUED_LENS_FLARES]; ///< offset=0x20c
  int16_t queued_lens_flare_count;        ///< offset=0x34c
  char pad_34e[2];                        ///< offset=0x34e
} lights_globals_t;
cs(lights_globals_t, 0x350);
co(lights_globals_t, marker, 0x04);
co(lights_globals_t, scene_point_light_count, 0x08);
co(lights_globals_t, scene_point_lights, 0x0c);
co(lights_globals_t, queued_lens_flares, 0x20c);
co(lights_globals_t, queued_lens_flare_count, 0x34c);

#define debug_lights (*(char *)0x5a8d58)
#define lights_globals (*(lights_globals_t *)0x5a8d60)
#define light_data (*(data_t **)0x5a90bc)
#define light_cluster_partition ((void *)0x5a90b0) /* partition type not recovered */
#define global_real_argb_white (*(real_argb_color **)0x2ee6c4)
#define global_real_argb_orange (*(real_argb_color **)0x2ee6f0)
#define REAL_0_8_POOL (*(float *)0x2533f0) /* 0.8f */
#define debug_object_lights (*(char *)0x5a8d59) /* hs global */
#define debug_rasterizer_light_count (*(short *)0x5a8d5a)
#define rendered_cluster_count (*(short *)0x5137cc)
#define render_window_index (*(char *)0x50654a)
#define object_light_ambient_base (*(float *)0x323bf8) /* hs global */
#define object_light_ambient_scale (*(float *)0x323bfc) /* hs global */
#define object_light_secondary_scale (*(float *)0x323c00) /* hs global */
#define REAL_ZERO_POOL (*(float *)0x2533c0) /* 0.0f */
#define REAL_ONE_POOL (*(float *)0x2533c8) /* 1.0f */
#define REAL_0_25_POOL (*(float *)0x25337c) /* 0.25f */
#define REAL_0_5_POOL (*(float *)0x253398) /* 0.5f */
#define REAL_1_5_POOL (*(float *)0x2533ec) /* 1.5f */
#define REAL_3_0_POOL (*(float *)0x254644) /* 3.0f */
#define REAL_1_3_POOL (*(float *)0x255b9c) /* 1.3f */
#define REAL_0_707_POOL (*(float *)0x29b4d0) /* 0.707f */
#define REAL_SQRT_HALF_POOL (*(float *)0x254b50) /* 0.70710677f */
#define REAL_NEGATIVE_SQRT_HALF_POOL (*(float *)0x29b5e0) /* -0.70710677f */
#define REAL_QUARTER_PI_POOL (*(float *)0x254a58) /* 0.7853982f */
#define REAL_HALF_PI_POOL (*(float *)0x2568bc) /* 1.5707964f */
#define REAL_MAX_POOL (*(float *)0x2548fc) /* 3.4028235e38f */
#define DOUBLE_0_25_POOL (*(double *)0x28c8d8) /* 0.25 */
#define REAL_PI_POOL (*(float *)0x256980) /* 3.1415927f */
#define REAL_0_0001_POOL (*(float *)0x253f44) /* 0.0001f */
#define REAL_0_3_POOL (*(float *)0x2533e4) /* 0.3f */
#define REAL_0_55_POOL (*(float *)0x26c744) /* 0.55f */
#define REAL_ONE_THIRTIETH_POOL (*(float *)0x2546a4) /* 0.033333335f */
#define REAL_100_POOL (*(float *)0x253f00) /* 100.0f */
#define REAL_ONE_OVER_TWO_PI_POOL (*(float *)0x29c120) /* 0.15915494f */
#define REAL_ONE_OVER_255_POOL (*(float *)0x261518) /* 0.003921569f */
#define DOUBLE_0_001_POOL (*(double *)0x2549d8) /* (double)0.001f */
#define DOUBLE_0_995_POOL (*(double *)0x29c128) /* 0.995 */
#define glow_data (*(data_t **)0x5a90c8) /* "glow" data_new name */
#define glow_particle_data (*(data_t **)0x5a90cc) /* "glow particles" data_new name */
#define light_volume_data (*(data_t **)0x46f020) /* "light volumes" data_new name */

#define light_get(index) ((light_datum_t *)datum_get(light_data, (index)))
#define light_definition_get(index) \
  ((light_definition_t *)tag_get(TAG_GROUP_LIGH, (index)))

#endif /* HALO_OBJECTS_OBJECTS_H */
