#ifndef TYPES_H
#define TYPES_H

#ifdef MSVC
#define __noreturn
#define offsetof(t, f) ( (int) &((t*)0)->f )
#define static_assert(cond) static_assert(cond, #cond)
#else
#define __noreturn __attribute__((noreturn))
#define offsetof(t, f) __builtin_offsetof(t, f)
#define static_assert(cond) _Static_assert(cond, #cond)
#endif
#define NULL ((void*)0)
#define true 1
#define false 0
#define NONE -1

#ifndef XDK_BUILD
#define cs(t, s)    static_assert(sizeof(t) == s)
#define co(t, f, o) static_assert(offsetof(t, f) == o)
#else
#define cs(t, s)
#define co(t, f, o)
#endif

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
#ifdef MSVC
typedef __int64 int64_t;
#else
typedef signed long long int64_t;
#endif

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#ifdef MSVC
typedef unsigned __int64 uint64_t;
#else
typedef unsigned long long uint64_t;
#endif
typedef unsigned int uintptr_t;

#ifndef __cplusplus
typedef unsigned char bool;
#endif
typedef unsigned short wchar_t;
typedef unsigned int size_t;

// FIXME: Normalize
typedef uint32_t _DWORD;
typedef uint16_t _WORD;
typedef uint8_t _BYTE;

/* Bungie cseries primitive aliases (readable-lift initiative, Phase 0).
 * Codegen-neutral typedefs over existing widths so lifted code can use the
 * original engine type names instead of stdint / Ghidra spellings. Struct
 * types (real_vector3d, real_euler_angles2d, real_point3d, ...) are defined
 * per object during struct-recovery, not here. */
typedef uint8_t  boolean;
typedef uint8_t  byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef float    real;
typedef uint32_t datum_index;

/* Bungie's 2D real vector. Lives here rather than in its recovering TU
 * (rasterizer_xbox_screen_effect.c) because ___reciprocal_vector2d returns it by value,
 * so the type appears in that function's kb.json decl and therefore in the
 * generated decl.h, which every TU includes.
 *
 * The component names are not guessed: the assert string at 0x1700d0 reads
 * "v->i!=0.0f && v->j!=0.0f", giving both field names. Size 8 with the
 * components at +0x00 / +0x04 follows from FLD [ESI] @001700f7 and
 * FLD [ESI+0x4] @00170106. Returned in EAX:EDX by both MSVC and
 * clang -target i386-pc-win32. */
typedef struct {
  real i;                          ///< offset=0x00
  real j;                          ///< offset=0x04
} real_vector2d;
cs(real_vector2d, 0x8);
co(real_vector2d, i, 0x0);
co(real_vector2d, j, 0x4);

/// size=0x10
typedef struct real_plane3d {
  real normal[3];                  ///< offset=0x00
  real d;                          ///< offset=0x0c
} real_plane3d;

/// Mirror/refraction surface consumed by render_camera_mirror (0x186ef0):
/// plane at +0x00, index_of_refraction at +0x10 (0.0f = reflective mirror),
/// depth at +0x14.  Only these offsets are proven; total size unverified.
typedef struct {
  real_plane3d plane;               ///< offset=0x00
  real         index_of_refraction; ///< offset=0x10
  real         depth;               ///< offset=0x14
} render_mirror_t;
cs(real_plane3d, 0x10);
co(real_plane3d, normal, 0x0);
co(real_plane3d, d, 0xc);

#define __int16 short
#define __int8 char

#pragma pack(1)

/// size=0x0C
typedef struct {
  float x; ///< offset=0x00
  float y; ///< offset=0x04
  float z; ///< offset=0x08
} vector3_t;

/// size=0x04
typedef union {
  int32_t value;   ///< offset=0x00
  struct {
    int16_t index; ///< offset=0x00
    int16_t salt;  ///< offset=0x02
  };
} datum_handle_t;

/// size=0x10c
typedef struct {
  uint32_t unk_0;         ///< offset=0x00
  uint16_t unk_4;         ///< offset=0x04
  int16_t  difficulty;    ///< offset=0x06
  uint32_t random_seed;   ///< offset=0x08
  char     map_name[256]; ///< offset=0x0c
} game_options_t;

/// size=0x114
typedef struct {
  bool           map_loaded;           ///< offset=0x00
  bool           active;               ///< offset=0x01
  bool           players_double_speed; ///< offset=0x02
  bool           map_loading;          ///< offset=0x03
  float          map_load_progress;    ///< offset=0x04
  game_options_t game_options;         ///< offset=0x08
} game_globals_t;

/* First field is a 2-byte scalar: every variant-default initializer in the
 * original zeroes a local copy as MOV word [base],DX then REP STOSD from
 * base+2 (0x19 dwords) + STOSW - MSVC's member-wise {0} zeroing of a struct
 * whose first member is 16-bit. */
/// size=0x68
typedef struct {
  int16_t unk_0;                      ///< offset=0x00
  char pad_02[0x16];                  ///< offset=0x02
  int32_t engine_type;                ///< offset=0x18
  uint8_t team_play;                  ///< offset=0x1c
  char pad_1d[0x23];                  ///< offset=0x1d
  int32_t score_limit;                ///< offset=0x40
  char pad_44[0x8];                   ///< offset=0x44
  uint8_t field_4c;                   ///< offset=0x4c
  uint8_t field_4d;                   ///< offset=0x4d
  uint8_t field_4e;                   ///< offset=0x4e
  uint8_t field_4f;                   ///< offset=0x4f
  int32_t field_50;                   ///< offset=0x50
  int32_t field_54;                   ///< offset=0x54
  int32_t field_58;                   ///< offset=0x58
  int32_t field_5c;                   ///< offset=0x5c
  int32_t field_60;                   ///< offset=0x60
  char pad_64[4];                     ///< offset=0x64
} game_variant_t;
cs(game_variant_t, 0x68);
co(game_variant_t, engine_type, 0x18);
co(game_variant_t, team_play,   0x1c);
co(game_variant_t, score_limit, 0x40);
co(game_variant_t, field_4c,    0x4c);
co(game_variant_t, field_4d,    0x4d);
co(game_variant_t, field_4e,    0x4e);
co(game_variant_t, field_4f,    0x4f);
co(game_variant_t, field_50,    0x50);
co(game_variant_t, field_54,    0x54);
co(game_variant_t, field_58,    0x58);
co(game_variant_t, field_5c,    0x5c);
co(game_variant_t, field_60,    0x60);

/// size=0x20
/// Evidence: recovery/evidence/network_player_record.json
/// The record is packed because the containing player array starts at game+0x226.
typedef struct {
  wchar_t name[12];             ///< offset=0x00
  uint16_t field_18;             ///< offset=0x18
  uint16_t field_1a;             ///< offset=0x1a
  int8_t machine_index;          ///< offset=0x1c
  int8_t controller_index;       ///< offset=0x1d
  int8_t team_index;             ///< offset=0x1e
  int8_t player_index;           ///< offset=0x1f
} network_player_record_t;
cs(network_player_record_t, 0x20);
co(network_player_record_t, name,              0x00);
co(network_player_record_t, field_18,          0x18);
co(network_player_record_t, field_1a,          0x1a);
co(network_player_record_t, machine_index,     0x1c);
co(network_player_record_t, controller_index,  0x1d);
co(network_player_record_t, team_index,        0x1e);
co(network_player_record_t, player_index,      0x1f);

/// size=0x44
/// Evidence: recovery/evidence/network_machine_record.json
/// Only the signed machine-index byte at +0x40 is field-recovered.
typedef struct {
  wchar_t name[32];              ///< offset=0x00
  int8_t machine_index;          ///< offset=0x40
  char pad_41[3];                ///< offset=0x41
} network_machine_record_t;
cs(network_machine_record_t, 0x44);
co(network_machine_record_t, machine_index, 0x40);

/// size=0x10
/// Evidence: recovery/evidence/network_server_machine_slot.json
typedef struct {
  uint32_t connection;            ///< offset=0x00
  uint32_t last_received_update_sequence_number; ///< offset=0x04
  uint32_t stall_start_time;      ///< offset=0x08
  int16_t machine_index;          ///< offset=0x0c
  uint16_t flags;                 ///< offset=0x0e
} network_server_machine_slot_t;
cs(network_server_machine_slot_t, 0x10);
co(network_server_machine_slot_t, connection,                         0x00);
co(network_server_machine_slot_t, last_received_update_sequence_number, 0x04);
co(network_server_machine_slot_t, stall_start_time,                   0x08);
co(network_server_machine_slot_t, machine_index,                       0x0c);
co(network_server_machine_slot_t, flags,                               0x0e);

/// size=0x434
/// Evidence: recovery/evidence/network_game_blob.json
/// This packed blob is serialized as a 0x434-byte network settings message.
typedef struct {
  char pad_00[0x24];              ///< offset=0x00
  char map_name[0x80];            ///< offset=0x24
  game_variant_t game_variant;    ///< offset=0xa4
  char pad_10c[1];                ///< offset=0x10c
  int8_t field_10d;                ///< offset=0x10d
  int8_t maximum_player_count;    ///< offset=0x10e
  uint8_t field_10f;               ///< offset=0x10f
  int16_t difficulty;             ///< offset=0x110
  int16_t machine_count;          ///< offset=0x112
  network_machine_record_t machines[4]; ///< offset=0x114
  int16_t player_count;           ///< offset=0x224
  network_player_record_t players[16]; ///< offset=0x226
  char pad_426[2];                ///< offset=0x426
  int32_t random_seed;            ///< offset=0x428
  int32_t number_of_games_played; ///< offset=0x42c
  uint8_t map_loaded;             ///< offset=0x430
  char pad_431[3];                ///< offset=0x431
} network_game_blob_t;
cs(network_game_blob_t, 0x434);
co(network_game_blob_t, map_name,                0x24);
co(network_game_blob_t, game_variant,            0xa4);
co(network_game_blob_t, field_10d,               0x10d);
co(network_game_blob_t, maximum_player_count,    0x10e);
co(network_game_blob_t, field_10f,               0x10f);
co(network_game_blob_t, difficulty,              0x110);
co(network_game_blob_t, machine_count,           0x112);
co(network_game_blob_t, machines,                0x114);
co(network_game_blob_t, player_count,            0x224);
co(network_game_blob_t, players,                 0x226);
co(network_game_blob_t, random_seed,             0x428);
co(network_game_blob_t, number_of_games_played,  0x42c);
co(network_game_blob_t, map_loaded,              0x430);

#define GAME_STATE_CPU_SIZE 0x305000

/// size=0x20
typedef struct {
  void     *log_file;            ///< offset=0x00
  char     *base_address;        ///< offset=0x04
  int       cpu_allocation_size; ///< offset=0x08
  uint32_t  gpu_allocation_size; ///< offset=0x0c  gpu_alloc grows into GPU buf
  uint32_t  checksum;            ///< offset=0x10
  bool      locked;              ///< offset=0x14
  bool      saved;               ///< offset=0x15
  char      unk_16[2];           ///< offset=0x16
  int32_t   unk_18;              ///< offset=0x18
  char     *header;              ///< offset=0x1c
} game_state_globals_t;
cs(game_state_globals_t, 0x20);
co(game_state_globals_t, locked, 0x14);
co(game_state_globals_t, header, 0x1c);

/// size=0x20
typedef struct {
  bool     initialized; ///< offset=0x00
  bool     active;      ///< offset=0x01
  bool     paused;      ///< offset=0x02
  char     unk_3[9];    ///< offset=0x03
  uint32_t time;        ///< offset=0x0c
  uint16_t elapsed;     ///< offset=0x10
  char     unk_18[6];   ///< offset=0x12
  float    speed;       ///< offset=0x18
  float    leftover_dt; ///< offset=0x1c
} game_time_globals_t;

/// size=0x98
typedef struct {
  bool object_is_being_placed;                ///< offset=0x00    see .text:0013F060 _objects_place
  bool object_marker_initialized;             ///< offset=0x01    see .text:0013EB70 _object_marker_begin
  bool garbage_collect_now;                   ///< offset=0x02    see .text:0013DB50 _garbage_collect_now
  char unk_3;                                 ///< offset=0x03    padding?
  uint16_t unk_4;                             ///< offset=0x04    see .text:00144B50 _objects_garbage_collection & .text:001444F0 _object_update
  uint16_t unk_6;                             ///< offset=0x06    padding?
  datum_handle_t unk_8;                       ///< offset=0x08    see .text:0013D939 mov     eax, [ecx+8]   datum handle for object_header_data
  char combined_pvs[64];                      ///< offset=0x0C    see .text:0013F9A1 add     edx, 0Ch
  char combined_pvs_local[64];                ///< offset=0x4C    see .text:0013F9B2 add     eax, 4Ch
  uint32_t last_garbage_collection_tick;      ///< offset=0x8C    see .text:00144EF6 mov     edx, [ecx+8Ch]
  uint16_t pvs_activator_type;                ///< offset=0x90    see .text:0013DBE0 _object_pvs_set_object
  uint16_t unk_146;                           ///< offset=0x92    padding?
  datum_handle_t pvs_activator_object_index;  ///< offset=0x94    see .text:0013DBE0 _object_pvs_set_object & .text:0013DCE4 mov     ecx, [ecx+94h]
} object_globals_t;

/// size=0x04
typedef struct {
  bool enabled;                               ///< offset=0x00
  uint8_t pad_01[3];
} lights_game_globals_t;
cs(lights_game_globals_t, 0x04);
co(lights_game_globals_t, enabled, 0x00);

#define NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS 4
#define MAXIMUM_REGIONS_PER_OBJECT 8

/// size=0x1A4
typedef struct {
  uint32_t tag_index;       ///< offset=0x00
  uint32_t flags;           ///< offset=0x04  .text:00095B7B                 mov     [esi+4], ecx
  uint32_t marker_generation; ///< offset=0x08  .text:0013EC41 compared against global object_marker_generation
  vector3_t unk_12;         ///< offset=0x0C
  vector3_t unk_24;         ///< offset=0x18
  vector3_t unk_36;         ///< offset=0x24
  vector3_t unk_48;         ///< offset=0x30
  vector3_t unk_60;         ///< offset=0x3C
  uint32_t unk_72;          ///< offset=0x48  .text:00140149                 mov     edx, [ecx+48h] location.???, leaf index?

  // .text:00031FEE                 mov     edx, [eax+4Ch]  
  // .text:00034D1F                 movsx   eax, word ptr [eax+4Ch] object.location.cluster_index
  datum_handle_t unk_76;    ///< offset=0x4C

  float unk_80;             ///< offset=0x50  .text:0009D161                 fsub    dword ptr [ebx+50h]
  float unk_84;             ///< offset=0x54  .text:0009D167                 fsub    dword ptr [ebx+54h]
  float unk_88;             ///< offset=0x58  .text:0009D16D                 fsub    dword ptr [ebx+58h]
  float unk_92;             ///< offset=0x5C  .text:0009D155                 fld     dword ptr [ebx+5Ch]
  float unk_96;             ///< offset=0x60  .text:00141E5B                 fld     dword ptr [esi+60h]
  int16_t type;             ///< offset=0x64  .text:0013D811                 movsx   ecx, word ptr [esi+64h] type enum
  int16_t unk_102;          ///< offset=0x66
  int16_t unk_104;          ///< offset=0x68  .text:00032344                 movsx   eax, word ptr [eax+68h] team-related index
  int16_t unk_106;          ///< offset=0x6A
  int16_t unk_108;          ///< offset=0x6C  .text:000A8741                 cmp     [eax+6Ch], si
  int16_t unk_110;          ///< offset=0x6E  .text:0003EC0B                 cmp     word ptr [edi+6Eh], 64h
  uint32_t unk_112;         ///< offset=0x70  .text:00143FFA                 mov     [edi+70h], edx
  uint32_t unk_116;         ///< offset=0x74  .text:000348B6                 mov     eax, [ecx+74h]
  uint32_t unk_120;         ///< offset=0x78
  uint32_t unk_124;         ///< offset=0x7C  .text:00141C2B                 mov     eax, [esi+7Ch]
  int16_t unk_128;          ///< offset=0x80  .text:00141C3E                 cmp     word ptr [esi+80h], 0FFFFh  animation related, possibly datum index
  int16_t unk_130;          ///< offset=0x82  .text:000FBB39                 mov     word ptr [esi+82h], 0  animation related, possibly datum index
  int16_t unk_132;          ///< offset=0x84  .text:001401F8                 movsx   edx, word ptr [esi+84h]
  int16_t unk_134;          ///< offset=0x86  .text:001401FF                 movsx   ecx, word ptr [esi+86h]
  uint32_t unk_136;         ///< offset=0x88  .text:00136654                 mov     [esi+88h], ecx   float?   body vitality?
  float unk_140;            ///< offset=0x8C  .text:000C9C40                 fmul    dword ptr [ecx+8Ch]  shield vitality?
  float unk_144;            ///< offset=0x90  .text:00136675                 fstp    dword ptr [esi+90h]  shield/vitality related
  float unk_148;            ///< offset=0x94  .text:000C9C46                 fstp    dword ptr [ecx+94h]  shield related, double charge?
  uint32_t unk_152;         ///< offset=0x98  .text:00136BA8                 mov     dword ptr [esi+98h], 0   float? shield
  float unk_156;            ///< offset=0x9C  .text:0001FA9E                 fld     dword ptr [edi+9Ch]
  uint32_t unk_160;         ///< offset=0xA0  .text:00137F25                 cmp     dword ptr [ebx+0A0h], 0FFFFFFFFh   datum_handle?
  float unk_164;            ///< offset=0xA4  .text:00138865                 fld     dword ptr [esi+0A4h]
  float unk_168;            ///< offset=0xA8  .text:001387AF                 fld     dword ptr [esi+0A8h]
  uint32_t unk_172;         ///< offset=0xAC  .text:00143FB0                 mov     [edi+0ACh], eax
  uint32_t unk_176;         ///< offset=0xB0  .text:0013877C                 mov     eax, [esi+0B0h]   datum_handle?

  // 32-bit flags?
  int16_t unk_180;          ///< offset=0xB4  .text:00138775                 mov     [esi+0B4h], ax
  int8_t unk_182;           ///< offset=0xB6  .text:00018832                 or      byte ptr [eax+0B6h], 40h
  int8_t unk_183;           ///< offset=0xB7  .text:0003B35D                 test    byte ptr [eax+0B7h], 1  ranged weapon

  uint32_t unk_184;         ///< offset=0xB8
  uint32_t unk_188;         ///< offset=0xBC  .text:00143F81                 mov     [edi+0BCh], eax
  uint32_t unk_192;         ///< offset=0xC0
  datum_handle_t next_object_index;   ///< offset=0xC4  .text:0014537B                 mov     ecx, [eax+0C4h]
  datum_handle_t unk_200;   ///< offset=0xC8  .text:000320C3                 mov     eax, [edi+0C8h]
  datum_handle_t parent_object_index;   ///< offset=0xCC  .text:00145348                 mov     ecx, [eax+0CCh]
  float unk_208[5];         ///< offset=0xD0  .text:0013E640                 fld     dword ptr [ebx+edx*4+0D0h] Colors related, 5 4-byte elements
  float unk_228[NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS]; ///< offset=0xE4  .text:001403FF                 mov     edx, [esi+ecx*4+0E4h] function stuff
  char unk_244[8];          ///< offset=0xF4
  char unk_252[32];         ///< offset=0xFC  .text:00097B90                 mov     eax, [edx+ecx*4+0FCh] illumination related?
  uint32_t unk_284;         ///< offset=0x11C .text:0013617A                 mov     dword ptr [esi+11Ch], 0FFFFFFFFh widget-related?
  uint32_t unk_288;         ///< offset=0x120 .text:00143F94                 mov     [edi+120h], eax
  uint16_t unk_292;         ///< offset=0x124 .text:001376FF                 movzx   eax, word ptr [ebx+124h] region-related?
  uint16_t unk_294;         ///< offset=0x126 .text:00144015                 mov     [edi+126h], dx
  uint8_t unk_296[MAXIMUM_REGIONS_PER_OBJECT];  ///< offset=0x128 .text:00141B01                 movzx   eax, byte ptr [edx+edi+128h]
  uint8_t unk_304[MAXIMUM_REGIONS_PER_OBJECT];  ///< offset=0x130 .text:00136A62                 mov     [esi+ebx+130h], al  shield/vitality regions?
  char unk_312[0x60];       ///< offset=0x138 .text:0013E21D                 lea     ebx, [edi+138h] color stuff
  uint32_t unk_408;         ///< offset=0x198 .text:001401E2                 lea     edx, [esi+198h] mode related
  uint32_t unk_412;         ///< offset=0x19C .text:001401D1                 lea     ecx, [esi+19Ch] mode related
  uint32_t unk_416;         ///< offset=0x1A0 .text:00140EF1                 add     eax, 1A0h node matrix reference?
} object_data_t;

#define MAXIMUM_WEAPONS_PER_UNIT 4
#define NUMBER_OF_UNIT_GRENADE_TYPES 2

/* Equipment powerup kind, stored in the 'eqip' tag at +0x308 (the
 * `powerup_type` field below).  Distinct from `enum player_powerup`, which
 * indexes the two-entry per-player timer array at player+0x68: only camo and
 * full-spectrum vision get a slot there, because only they need a per-player
 * countdown.
 *
 * The `_equipment_powerup_` prefix is 2276's own, verbatim from three assert
 * strings in units.c, so halocea's consumer-facing `_powerup_type_*` renames
 * are deliberately not used here.
 *
 * Values 0 and 6 are T1 — identifier and value both from our binary:
 *   units.c 0x1ca1  `powerup_type != _equipment_powerup_none`     guards == 0
 *   units.c 0x1c72  `powerup_type == _equipment_powerup_grenade`  guards != 6
 *   units.c 0x1ca2  `powerup_type != _equipment_powerup_grenade`  guards == 6
 *
 * Values 1-5 are T2 (name_source: halocea, DB-verified there via
 * types_enum_values _270498BB874CAD5ECABAECA7DA81ECAE).  Our binary proves
 * their MEANING independently — the dispatch in
 * player_handle_powerup_equipment routes each to an already-named
 * handler: game_set_players_are_double_speed (1), object_double_charge_shield
 * + player_over_shield_screen_effect (2), powerup slot 0 (3), powerup slot 1
 * (4), object_restore_body + player_health_pack_screen_effect (5).  The spellings
 * are still borrowed.
 *
 * No bound is named: 6 is the largest value our binary compares against, which
 * does not prove 7 is the count.  halocea states NUMBER_OF_POWERUP_TYPES = 7;
 * that stays unadopted until a 2276 site needs it. */
enum equipment_powerup_type {
  _equipment_powerup_none = 0,
  _equipment_powerup_double_speed = 1,
  _equipment_powerup_over_shield = 2,
  _equipment_powerup_active_camouflage = 3,
  _equipment_powerup_full_spectrum_vision = 4,
  _equipment_powerup_health = 5,
  _equipment_powerup_grenade = 6
};

// OBJE -> UNIT
/// size=0x424
typedef struct {
  object_data_t object;               ///< offset=0x000
  datum_handle_t actor_index;         ///< offset=0x1A4 .text:0003EB73                 cmp     dword ptr [edi+1A4h], 0FFFFFFFFh
  datum_handle_t swarm_actor_index;   ///< offset=0x1A8 .text:0003EB9C                 cmp     dword ptr [edi+1A8h], 0FFFFFFFFh
  datum_handle_t unk_428;             ///< offset=0x1AC .text:00031492                 mov     eax, [eax+1ACh]
  datum_handle_t unk_432;             ///< offset=0x1B0 .text:0003AF89                 mov     eax, [esi+1B0h]   datum index?
  uint32_t unk_436;                   ///< offset=0x1B4 .text:001A80D0                 test    dword ptr [esi+1B4h], 400000h   flags
  uint32_t unk_440;                   ///< offset=0x1B8 .text:000D93E7                 mov     ecx, [eax+1B8h] flags
  uint16_t unk_444;                   ///< offset=0x1BC .text:001B3701                 inc     word ptr [ebx+1BCh]
  uint8_t unk_446;                    ///< offset=0x1BE .text:001A8125                 movsx   edx, byte ptr [esi+1BEh]
  uint8_t unk_447;                    ///< offset=0x1BF .text:001AE773                 mov     [esi+1BFh], al    seat index
  uint32_t unk_448;                   ///< offset=0x1C0 .text:001A81DA                 mov     [esi+1C0h], ecx
  uint32_t persistent_control_flags;  ///< offset=0x1C4 .text:001A81D3                 mov     [esi+1C4h], edi
  datum_handle_t unk_456;             ///< offset=0x1C8 .text:00030656                 cmp     dword ptr [ebx+1C8h], 0FFFFFFFFh
  uint16_t unk_460;                   ///< offset=0x1CC .text:0004091B                 cmp     bx, [esi+1CCh]
  uint16_t unk_462;                   ///< offset=0x1CE .text:001A9B5E                 mov     [esi+1CEh], ax
  uint32_t unk_464;                   ///< offset=0x1D0 .text:00040924                 mov     ecx, [esi+1D0h] game time related
  vector3_t unk_468;                  ///< offset=0x1D4 .text:001AF62B                 lea     ecx, [esi+1D4h]
  vector3_t unk_480;                  ///< offset=0x1E0 .text:001AF63E                 lea     edx, [esi+1E0h]
  vector3_t unk_492;                  ///< offset=0x1EC .text:001AF678                 lea     eax, [esi+1ECh]
  vector3_t unk_504;                  ///< offset=0x1F8 .text:001AF7E5                 fld     dword ptr [esi+1F8h]
  vector3_t unk_516;                  ///< offset=0x204 .text:001AF651                 lea     eax, [esi+204h]
  vector3_t unk_528;                  ///< offset=0x210 .text:001AF68B                 add     esi, 210h
  vector3_t unk_540;                  ///< offset=0x21C .text:001AF82F                 fld     dword ptr [esi+21Ch]
  vector3_t unk_552;                  ///< offset=0x228 .text:001B39A3                 lea     edx, [ebx+228h]
  float unk_564;                      ///< offset=0x234 .text:001B387D                 mov     dword ptr [ebx+234h], 3F800000h
  uint8_t unk_568;                    ///< offset=0x238
  uint8_t unk_569;                    ///< offset=0x239 .text:001ABDB1                 mov     cl, [esi+239h]
  uint8_t unk_570;                    ///< offset=0x23A .text:001ABDE5                 mov     cl, [esi+23Ah]
  uint8_t unk_571;                    ///< offset=0x23B .text:001B0EC8                 mov     al, [edi+23Bh]
  uint8_t unk_572;                    ///< offset=0x23C .text:0003D1D3                 mov     [eax+23Ch], bl
  uint8_t unk_573;                    ///< offset=0x23D .text:001AB0FB                 mov     byte ptr [esi+23Dh], 3
  uint8_t unk_574;                    ///< offset=0x23E .text:001AB2F7                 movsx   ecx, word ptr [esi+23Eh]
  uint8_t unk_575;                    ///< offset=0x23F padding?
  uint16_t unk_576;                   ///< offset=0x240 .text:001AB2FE                 movsx   edx, word ptr [esi+240h]
  uint16_t unk_578;                   ///< offset=0x242
  datum_handle_t unk_580;             ///< offset=0x244 .text:001AB147                 mov     edi, [esi+244h]
  uint8_t unk_584;                    ///< offset=0x248 .text:001ACF38                 or      byte ptr [eax+248h], 2
  uint8_t unk_585;                    ///< offset=0x249
  uint16_t unk_586;                   ///< offset=0x24A .text:001A8C5D                 cmp     word ptr [esi+24Ah], 0FFFFh
  uint16_t unk_588;                   ///< offset=0x24C .text:001AD69B                 mov     [esi+24Ch], ax
  uint16_t unk_590;                   ///< offset=0x24E .text:001B2887                 mov     [esi+24Eh], bx
  uint8_t unk_592;                    ///< offset=0x250 .text:001A841D                 movsx   ecx, byte ptr [esi+250h]
  uint8_t unk_593;                    ///< offset=0x251 .text:001A8432                 movsx   ecx, byte ptr [esi+251h]
  uint8_t unk_594;                    ///< offset=0x252 .text:001A8A0C                 movsx   eax, byte ptr [esi+252h]
  uint8_t unk_595;                    ///< offset=0x253 .text:001A8B46                 movsx   eax, byte ptr [esi+253h]
  uint8_t unk_596;                    ///< offset=0x254 .text:001A8AEB                 mov     [esi+254h], cl
  uint8_t unk_597;                    ///< offset=0x255 .text:001A8B31                 movsx   cx, byte ptr [esi+255h]
  uint8_t unk_598;                    ///< offset=0x256 .text:001B0E17                 movsx   eax, byte ptr [edi+256h]
  uint8_t base_seat_index;            ///< offset=0x257 .text:001AE2E8                 movsx   si, byte ptr [esi+257h]
  uint8_t unk_600;                    ///< offset=0x258 .text:001AC08A                 mov     [eax+258h], cl
  uint8_t unk_601;                    ///< offset=0x259 padding? 
  uint16_t unk_602;                   ///< offset=0x25A .text:001AFD69                 mov     ax, [esi+25Ah]   tag block index
  uint16_t unk_604;                   ///< offset=0x25C .text:001AFD7E                 mov     cx, [esi+25Ch]   tag block index
  uint16_t unk_606;                   ///< offset=0x25E .text:001AFDA5                 mov     ax, [esi+25Eh]   tag block index
  uint16_t unk_608;                   ///< offset=0x260 .text:001AFDB4                 mov     cx, [esi+260h]   tag block index
  uint16_t unk_610;                   ///< offset=0x262 .text:001AFDDB                 mov     ax, [esi+262h]   tag block index
  uint16_t unk_612;                   ///< offset=0x264 .text:001AFDEA                 mov     cx, [esi+264h]   tag block index
  uint8_t unk_614;                    ///< offset=0x266 .text:001ADAB2                 mov     bl, [eax+266h]
  uint8_t unk_615;                    ///< offset=0x267 .text:001ADAC0                 mov     bl, [eax+267h]
  float unk_616;                      ///< offset=0x268 .text:001ADAB8                 lea     edi, [eax+268h]  quat?
  float unk_620;                      ///< offset=0x26C .text:001B0225                 fstp    dword ptr [esi+26Ch]
  float unk_624;                      ///< offset=0x270 .text:001B0244                 fstp    dword ptr [esi+270h]
  float unk_628;                      ///< offset=0x274 .text:001B0263                 fstp    dword ptr [esi+274h]
  float unk_632;                      ///< offset=0x278 .text:001ADAC6                 lea     edi, [eax+278h]  quat?
  float unk_636;                      ///< offset=0x27C .text:001B0459                 fstp    dword ptr [esi+27Ch]
  float unk_640;                      ///< offset=0x280 .text:001B0472                 fstp    dword ptr [esi+280h]
  float unk_644;                      ///< offset=0x284 .text:001B0491                 fstp    dword ptr [esi+284h]
  float unk_648;                      ///< offset=0x288
  uint32_t unk_652;                   ///< offset=0x28C
  float unk_656;                      ///< offset=0x290 .text:001AB900                 fstp    dword ptr [esi+290h]   rgb color brightness
  float unk_660;                      ///< offset=0x294 .text:001AB90C                 fstp    dword ptr [esi+294h]   self illumination
  float unk_664;                      ///< offset=0x298 .text:001A80AC                 fld     dword ptr [esi+298h]
  uint32_t unk_668;                   ///< offset=0x29C
  uint16_t unk_672;                   ///< offset=0x2A0 .text:000B6805                 movsx   ecx, word ptr [edi+2A0h]   tag block index
  uint16_t unk_674;                   ///< offset=0x2A2 .text:000B0B06                 mov     ax, [ebx+2A2h]   current weapon index into (0x2A8)
  uint16_t unk_676;                   ///< offset=0x2A4 .text:000B707C                 mov     ax, [ebx+2A4h]   next weapon index
  uint16_t unk_678;                   ///< offset=0x2A6
  datum_handle_t unk_680[MAXIMUM_WEAPONS_PER_UNIT]; ///< offset=0x2A8 .text:001AAD23                 mov     eax, [edi+ecx*4+2A8h]
  datum_handle_t unk_696[MAXIMUM_WEAPONS_PER_UNIT]; ///< offset=0x2B8 .text:001B1E5D                 mov     dword ptr [edi+eax*4+2B8h], 0
  datum_handle_t unk_712;             ///< offset=0x2C8 .text:001AA97E                 mov     eax, [eax+2C8h] current equipment
  uint8_t current_grenade_index;      ///< offset=0x2CC .text:001AAEF1                 mov     al, [esi+2CCh] unit->unit.current_grenade_index
  uint8_t unk_717;                    ///< offset=0x2CD .text:000B7087                 movsx   cx, byte ptr [ebx+2CDh]
  uint8_t unk_718[NUMBER_OF_UNIT_GRENADE_TYPES];  ///< offset=0x2CE .text:001A99E3                 cmp     byte ptr [ecx+ebx+2CEh], 0   grenade counts
  uint8_t zoom_level;                 ///< offset=0x2D0 .text:001A869E                 movsx   ax, byte ptr [eax+2D0h]
  uint8_t unk_721;                    ///< offset=0x2D1 .text:000B7093                 movsx   dx, byte ptr [ebx+2D1h]
  uint8_t unk_722;                    ///< offset=0x2D2
  uint8_t unk_723;                    ///< offset=0x2D3 .text:001A8090                 movzx   edx, byte ptr [esi+2D3h]
  datum_handle_t unk_724;             ///< offset=0x2D4 .text:001AA4DE                 mov     ecx, [eax+2D4h]
  datum_handle_t unk_728;             ///< offset=0x2D8 .text:000D8D8D                 cmp     [eax+2D8h], edi
  uint32_t unk_732;                   ///< offset=0x2DC .text:0003AC65                 mov     ecx, [esi+2DCh]  game time related
  uint32_t unk_736;                   ///< offset=0x2E0 .text:000BC220                 mov     [ebx+2E0h], eax  game time related
  uint16_t unk_740;                   ///< offset=0x2E4 .text:00057E26                 mov     ax, [eax+2E4h]   actor related
  uint16_t unk_742;                   ///< offset=0x2E6 .text:0003DF30                 mov     ax, [edi+2E6h]   squad related
  float unk_744;                      ///< offset=0x2E8 .text:001A8078                 fld     dword ptr [esi+2E8h]
  float unk_748;                      ///< offset=0x2EC .text:001A8085                 fld     dword ptr [esi+2ECh]
  float unk_752;                      ///< offset=0x2F0 .text:000D7F87                 cmp     dword ptr [ecx+2F0h], 3F800000h
  float unk_756;                      ///< offset=0x2F4 .text:000D7FAD                 fld     dword ptr [ecx+2F4h]
  float unk_760;                      ///< offset=0x2F8 .text:001B1337                 mov     dword ptr [edi+2F8h], 0  zoom-related?
  vector3_t unk_764;                  ///< offset=0x2FC .text:001AB4F7                 lea     ecx, [esi+2FCh]
  uint16_t powerup_type;              ///< offset=0x308 .text:001AA9DD                 cmp     word ptr [esi+308h], 6  equipment_definition->equipment.powerup_type
  uint16_t unk_778;                   ///< offset=0x30A .text:001AA9BD                 movsx   eax, word ptr [esi+30Ah]
  float unk_782;                      ///< offset=0x30C .text:001AB528                 fsub    dword ptr [esi+30Ch]
  float unk_786;                      ///< offset=0x310 .text:001AB531                 fsub    dword ptr [esi+310h]
  vector3_t unk_790;                  ///< offset=0x314 .text:001B46F1                 fld     dword ptr [ebx+314h]
  vector3_t unk_800;                  ///< offset=0x320 .text:001AB57C                 fstp    dword ptr [esi+320h]
  float unk_812;                      ///< offset=0x32C .text:0013B79C                 fld     dword ptr [edi+32Ch]
  float unk_816;                      ///< offset=0x330 .text:0003CA20                 fstp    dword ptr [eax+330h]
  uint32_t unk_820;                   ///< offset=0x334 .text:001A698A                 mov     eax, [ecx+334h]  tag index
  uint16_t unk_824;                   ///< offset=0x338 .text:001A6BD3                 cmp     [eax+338h], cx  command type?, also see action_obey_command_perform
  uint16_t unk_826;                   ///< offset=0x33A .text:001A6D9A                 mov     ax, [edi+33Ah]
  uint32_t unk_828;                   ///< offset=0x33C .text:001A6D4B                 mov     eax, [edi+33Ch]  tag index
  uint16_t unk_832;                   ///< offset=0x340 .text:001A6FB3                 mov     ax, [ebx+340h]
  uint16_t unk_834;                   ///< offset=0x342 .text:001A6FC1                 mov     dx, [ebx+342h]
  uint16_t unk_836;                   ///< offset=0x344 .text:001A6FBA                 mov     cx, [ebx+344h]
  uint16_t unk_838;                   ///< offset=0x346
  uint8_t unk_840[0x20];              ///< offset=0x348 .text:001A7913                 lea     eax, [esi+348h]  4th arg to ai_communication_started
  uint16_t unk_872;                   ///< offset=0x368 .text:001A6A73                 mov     dx, [ecx+368h]
  uint8_t unk_874[0x2E];              ///< offset=0x36A
  uint16_t unk_920;                   ///< offset=0x398 .text:001A77D9                 mov     [esi+398h], ax
  uint16_t unk_922;                   ///< offset=0x39A .text:001A77E4                 mov     ax, [esi+39Ah]
  uint16_t unk_924;                   ///< offset=0x39C .text:001A7803                 mov     ax, [esi+39Ch]
  uint16_t unk_926;                   ///< offset=0x39E
  uint32_t unk_928;                   ///< offset=0x3A0 .text:001A6BA2                 mov     edx, [ecx+3A0h]
  uint8_t unk_932;                    ///< offset=0x3A4 .text:001A718B                 mov     byte ptr [esi+3A4h], 1 bool?
  uint8_t unk_933;                    ///< offset=0x3A5 .text:001A6FDF                 mov     byte ptr [ebx+3A5h], 0 bool?
  uint8_t unk_934;                    ///< offset=0x3A6 .text:001A79C5                 mov     byte ptr [esi+3A6h], 1 bool?
  uint8_t unk_935;                    ///< offset=0x3A7
  uint16_t unk_936;                   ///< offset=0x3A8 .text:001A7192                 mov     word ptr [esi+3A8h], 0
  uint16_t unk_938;                   ///< offset=0x3AA .text:00043F5E                 movsx   eax, word ptr [edi+3AAh]   vocalization timer related
  uint16_t unk_940;                   ///< offset=0x3AC .text:001A6FFE                 mov     [ebx+3ACh], dx
  uint16_t unk_942;                   ///< offset=0x3AE .text:001A6B28                 movsx   ebx, word ptr [eax+3AEh]
  uint32_t unk_944;                   ///< offset=0x3B0 .text:001A6FED                 mov     dword ptr [ebx+3B0h], 0FFFFFFFFh
  uint16_t unk_948;                   ///< offset=0x3B4 .text:001B29DF                 mov     [esi+3B4h], di
  uint16_t unk_950;                   ///< offset=0x3B6 .text:001B29E6                 mov     [esi+3B6h], di
  uint32_t unk_952;                   ///< offset=0x3B8 .text:001B29ED                 mov     [esi+3B8h], edi
  uint32_t unk_956;                   ///< offset=0x3BC .text:001B489C                 mov     dword ptr [ebx+3BCh], 0FFFFFFFFh
  datum_handle_t unk_960;             ///< offset=0x3C0 .text:0001C789                 mov     ecx, [eax+3C0h]
  float unk_964;                      ///< offset=0x3C4 .text:001AF3F9                 fsub    dword ptr [edi+3C4h]
  float unk_968;                      ///< offset=0x3C8 .text:001AF451                 fsub    dword ptr [edi+3C8h]
  datum_handle_t unk_972;             ///< offset=0x3CC .text:0002F7DC                 mov     eax, [eax+3CCh]
  uint16_t feign_death_timer;         ///< offset=0x3D0 .text:001B5025                 mov     [esi+3D0h], ax  unit->unit.feign_death_timer
  uint16_t unk_978;                   ///< offset=0x3D2 .text:000BC3C1                 mov     [eax+3D2h], si   datum index only, not a full handle?
  float unk_980;                      ///< offset=0x3D4 .text:000B7471                 fld     dword ptr [eax+3D4h]
  uint16_t unk_984;                   ///< offset=0x3D8
  uint16_t unk_986;                   ///< offset=0x3DA .text:0005BE6E                 movsx   eax, word ptr [eax+3DAh] combat related?
  uint32_t unk_988;                   ///< offset=0x3DC .text:001A90DA                 mov     ecx, [esi+3DCh]

  // array size of 4, struct size of 0x10  .text:0002FB1B                 add     edi, 10h
  // .text:0002FAAB                 lea     edi, [eax+3E0h]
  // .text:001A8F4A                 cmp     dword ptr [eax+edi], 0FFFFFFFFh (0x3E0 + index*16)
  // .text:001A8F05                 lea     eax, [edi+3E4h]
  // .text:001A8F7B                 fcomp   dword ptr [eax+edi+3E4h]
  // .text:0002FAB8                 mov     edx, [edi+8]
  // .text:001A8F6D                 lea     edx, [edi+3F4h]
  char unk_992[0x10 * 4];             ///< offset=0x3E0
  uint32_t unk_1056;                  ///< offset=0x420
} unit_data_t;

// OBJE -> ITEM
/// size=0x1DC
typedef struct {
  object_data_t object;   ///< offset=0x000
  uint32_t flags;         ///< offset=0x1A4   .text:000F6BBB                 mov     edx, [ecx+1A4h]
  uint16_t unk_424;       ///< offset=0x1A8   .text:000F7BC6                 mov     ax, [ebx+1A8h]
  uint16_t unk_426;       ///< offset=0x1AA
  char unk_428[4];        ///< offset=0x1AC
  uint32_t unk_432;       ///< offset=0x1B0   .text:000F693A                 mov     dword ptr [esi+1B0h], 0FFFFFFFFh   datum_handle?
  uint32_t unk_436;       ///< offset=0x1B4   .text:000F6934                 mov     [esi+1B4h], eax game time related
  char unk_440[16];       ///< offset=0x1B8
  float unk_456;          ///< offset=0x1C8   .text:000F6BDE                 fstp    dword ptr [ecx+1C8h]
  float unk_460;          ///< offset=0x1CC   .text:000F6BE9                 fstp    dword ptr [ecx+1CCh]
  float unk_464;          ///< offset=0x1D0   .text:000F6BF2                 fstp    dword ptr [ecx+1D0h]
  float unk_468;          ///< offset=0x1D4   .text:000F6BFC                 fstp    dword ptr [ecx+1D4h]
  float unk_472;          ///< offset=0x1D8   .text:000F6C04                 fstp    dword ptr [ecx+1D8h]
} item_data_t;

/// size=0x24
typedef struct
{
  uint8_t unk_528;                  ///< offset=0x00
  uint8_t unk_529;                  ///< offset=0x01 .text:000FCEA2                 mov     byte ptr [eax+211h], 7 & .text:000FB3D5                 mov     cl, [eax+235h]
  uint16_t unk_530;                 ///< offset=0x02 .text:000FB8FA                 mov     [eax+212h], dx
  char unk_532[12];                 ///< offset=0x04
  float unk_544;                    ///< offset=0x10 .text:000FC17C                 fld     dword ptr [edi+ecx*4+220h]
  float unk_548;                    ///< offset=0x14 .text:000FC156                 fld     dword ptr [edi+eax*4+224h]
  char unk_552[4];                  ///< offset=0x18
  float unk_556;                    ///< offset=0x1C .text:000D137D                 fld     dword ptr [esi+22Ch]
  char unk_560[4];                  ///< offset=0x20
} weapon_trigger_data_t;

/// size=0xC
typedef struct
{
  uint16_t unk_0;                  ///< offset=0x00 .text:000DE270                 cmp     word ptr [edi+258h], 0
  uint16_t unk_2;                  ///< offset=0x02 .text:000DD7F7                 sub     bx, [edx+25Ah]
  uint16_t unk_4;                  ///< offset=0x04 .text:000DD7E7                 mov     bx, [eax+25Ch]
  uint16_t unk_6;                  ///< offset=0x06 .text:000D1228                 movsx   ecx, word ptr [esi+25Eh]
  uint16_t unk_8;                  ///< offset=0x08 .text:000D120A                 movsx   edx, word ptr [esi+260h]
  char unk_10[2];                  ///< offset=0x0A 
} weapon_magazine_data_t;

#define MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON 2
#define MAXIMUM_NUMBER_OF_MAGAZINES_PER_WEAPON 2  // TODO: confirm
// Confirmed via disassembly at 0xfb880 (weapon_trigger_change_state):
// CMP BX,0x9 / JL 0xfb8e7 bounds new_state before the assert_halt fires.
#define NUMBER_OF_TRIGGER_STATES 9

// OBJE -> ITEM -> WEAP
/// size=0x27C
typedef struct {
  item_data_t item;                 ///< offset=0x000
  uint32_t unk_476;                 ///< offset=0x1DC .text:000A87F0                 mov     ecx, [eax+1DCh]
  uint8_t unk_480;                  ///< offset=0x1E0 .text:000FDA9E                 test    byte ptr [ebx+1E0h], 40h
  char unk_481[3];                  ///< offset=0x1E1
  float unk_484;                    ///< offset=0x1E4 .text:000D1375                 fld     dword ptr [esi+1E4h]
  uint8_t unk_488;                  ///< offset=0x1E8 .text:000FD158                 mov     al, [eax+1E8h]   state related?
  uint8_t unk_489;                  ///< offset=0x1E9
  uint16_t unk_490;                 ///< offset=0x1EA .text:000FD343                 mov     [edi+1EAh], ax
  float unk_492;                    ///< offset=0x1EC .text:000D121A                 fld     dword ptr [esi+1ECh]
  float unk_496;                    ///< offset=0x1F0 .text:000D1204                 fld     dword ptr [esi+1F0h]
  float unk_500;                    ///< offset=0x1F4 .text:000DD8F9                 fld     dword ptr [edx+1F4h]
  uint32_t integrated_light_power;  ///< offset=0x1F8 .text:000FAEC4                 mov     [eax+1F8h], ecx
  char unk_508[4];                  ///< offset=0x1FC
  uint32_t unk_512;                 ///< offset=0x200 .text:000FD55C                 mov     dword ptr [edi+200h], 0FFFFFFFFh
  char unk_516[8];                  ///< offset=0x204
  uint16_t unk_524;                 ///< offset=0x20C .text:000FD906                 cmp     word ptr [ebx+20Ch], 0
  char unk_526[2];                  ///< offset=0x20E
  weapon_trigger_data_t triggers[MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON];  ///< offset=0x210 .text:000FCFA3                 lea     edi, [edi+eax*4+210h]
  weapon_magazine_data_t magazines[MAXIMUM_NUMBER_OF_MAGAZINES_PER_WEAPON];  ///< offset=0x258 .text:000FBC8D                 lea     edi, [ebx+eax*4]   &v2[3 * magazine_index? + 0x96]; (0x258 is the base address, 12-byte struct due to dword access)
  char unk_624[4];                  ///< offset=0x270
  uint32_t unk_628;                 ///< offset=0x274 .text:000FBD3A                 mov     dword ptr [edi+274h], 0FFFFFFFFh
  char unk_632[4];                  ///< offset=0x278
} weapon_data_t;

/// size=0xd4
typedef struct {
  char pad_00[2];
  int16_t local_player_index;         ///< offset=0x02
  wchar_t name[12];                   ///< offset=0x04
  char pad_1c[4];                     ///< offset=0x1c
  int32_t team_index;                 ///< offset=0x20
  int32_t field_24;                   ///< offset=0x24  object handle (enter/pickup/board)
  int16_t action_state;               ///< offset=0x28
  char pad_2a[0xa];                   ///< offset=0x2a
  datum_index unit_handle;            ///< offset=0x34
  datum_index previous_unit_handle;   ///< offset=0x38
  int16_t cluster_index;              ///< offset=0x3c
  char pad_3e[0x2a];                  ///< offset=0x3e
  int16_t field_68[2];                ///< offset=0x68  hud.c comments +0x68 as ac_timer
  real speed_multiplier;              ///< offset=0x6c
  char pad_70[0x18];                  ///< offset=0x70
  datum_index target_player_index;    ///< offset=0x88
  char pad_8c[0x36];                  ///< offset=0x8c
  int16_t race_score;                 ///< offset=0xc2
  int16_t field_c4;                   ///< offset=0xc4  lap counter in one path, lap time in another
  char pad_c6[6];                     ///< offset=0xc6
  int32_t quit_at;                    ///< offset=0xcc
  char pad_d0;                        ///< offset=0xd0
  boolean quitting;                   ///< offset=0xd1
  char pad_d2[2];                     ///< offset=0xd2
} player_data_t;
cs(player_data_t, 0xd4);
co(player_data_t, local_player_index,       0x02);
co(player_data_t, name,                     0x04);
co(player_data_t, team_index,               0x20);
co(player_data_t, field_24,                 0x24);
co(player_data_t, action_state,             0x28);
co(player_data_t, unit_handle,              0x34);
co(player_data_t, previous_unit_handle,     0x38);
co(player_data_t, cluster_index,            0x3c);
co(player_data_t, field_68,                 0x68);
co(player_data_t, speed_multiplier,         0x6c);
co(player_data_t, target_player_index,      0x88);
co(player_data_t, race_score,               0xc2);
co(player_data_t, field_c4,                 0xc4);
co(player_data_t, quit_at,                  0xcc);
co(player_data_t, quitting,                 0xd1);

/// size=0xd0
typedef struct {
  uint32_t track_flags_bitmask;    ///< offset=0x00 (0x456f10)
  int32_t team_current_flags[16];  ///< offset=0x04 (0x456f14)
  uint32_t team_visited_flags[16]; ///< offset=0x44 (0x456f54)
  int32_t random_flag;             ///< offset=0x84 (0x456f94)
  int32_t team_scores[16];         ///< offset=0x88 (0x456f98)
  char pad_c8[4];                  ///< offset=0xc8
  boolean flag_set;                ///< offset=0xcc (0x456fdc)
  char pad_cd[3];                  ///< offset=0xcd
} game_engine_race_globals_t;
cs(game_engine_race_globals_t, 0xd0);
co(game_engine_race_globals_t, track_flags_bitmask, 0x00);
co(game_engine_race_globals_t, team_current_flags,  0x04);
co(game_engine_race_globals_t, team_visited_flags,  0x44);
co(game_engine_race_globals_t, random_flag,         0x84);
co(game_engine_race_globals_t, team_scores,         0x88);
co(game_engine_race_globals_t, flag_set,            0xcc);

/// size=0x80
typedef struct {
  int32_t team_scores[16];   ///< offset=0x00 (0x456fe0)
  int32_t player_scores[16]; ///< offset=0x40 (0x457020)
} slayer_globals_t;
cs(slayer_globals_t, 0x80);
co(slayer_globals_t, team_scores,   0x00);
co(slayer_globals_t, player_scores, 0x40);

/// size=0x40
typedef struct {
  char unk_0[0x40];
} team_data_t;

/// size=0xb0
typedef struct {
  char unk_0[0xb0];
} players_globals_t;

/// size=0x110
typedef struct {
  char unk_0[0x110];
} player_control_globals_t;

/// size=0x40
/// One per local player; lives in player_control_globals at +0x10, stride 0x40.
/// Names carrying a "player->..." comment are recovered verbatim from the
/// binary's own assert strings (desired_angles.yaw/pitch, primary_trigger);
/// field_0xNN are offsets whose purpose is not yet established.
typedef struct {
  int32_t unit_index;            ///< offset=0x00 owning unit datum handle
  int32_t field_0x04;            ///< offset=0x04
  uint16_t action_flags;         ///< offset=0x08 (player_control_inhibit_buttons)
  uint16_t persistent_action_flags; ///< offset=0x0a (persistent variant)
  real    desired_angles_yaw;    ///< offset=0x0c player->desired_angles.yaw
  real    desired_angles_pitch;  ///< offset=0x10 player->desired_angles.pitch
  real    field_0x14;            ///< offset=0x14
  int32_t field_0x18;            ///< offset=0x18
  real    primary_trigger;       ///< offset=0x1c player->primary_trigger
  int16_t desired_weapon_index;  ///< offset=0x20
  int16_t desired_grenade_index; ///< offset=0x22
  int16_t desired_zoom_level;    ///< offset=0x24
  uint8_t field_0x26;            ///< offset=0x26 aim-assist enabled flag
  int8_t  field_0x27;            ///< offset=0x27 aim-assist idle counter
  int32_t target_object_index;   ///< offset=0x28 aim-assist target object handle (new_unit initializes to -1)
  real    autoaim_level;         ///< offset=0x2c aim-assist level returned by
                                 ///< player_control_get_autoaim_level (FLD
                                 ///< [globals + index*0x40 + 0x3c])
  real    field_0x30;            ///< offset=0x30
  uint8_t pad_0x34[0x4];         ///< offset=0x34
  real    pitch_minimum;         ///< offset=0x38 lower clamp for desired_angles.pitch
  real    pitch_maximum;         ///< offset=0x3c upper clamp for desired_angles.pitch
} player_control_t;
cs(player_control_t, 0x40);
co(player_control_t, unit_index,             0x00);
co(player_control_t, action_flags,           0x08);
co(player_control_t, persistent_action_flags, 0x0a);
co(player_control_t, desired_angles_yaw,     0x0c);
co(player_control_t, desired_angles_pitch,   0x10);
co(player_control_t, primary_trigger,        0x1c);
co(player_control_t, desired_weapon_index,   0x20);
co(player_control_t, target_object_index,    0x28);
co(player_control_t, desired_grenade_index,  0x22);
co(player_control_t, desired_zoom_level,     0x24);
co(player_control_t, autoaim_level,          0x2c);
co(player_control_t, pitch_minimum,          0x38);
co(player_control_t, pitch_maximum,          0x3c);

/// size=0x20
/// One frame of controller input for a local player, filled by
/// get_local_player_input_blob (0xb70b0, buffer in EBX) and consumed by
/// handle_one_player_input. Bungie calls the parameter "input" -- recovered
/// from that function's own assert string "input->primary_trigger", which
/// guards a load of +0x08. Field widths are taken from the producer's stores
/// (byte at +0x14/+0x15, dword elsewhere); field_0xNN are offsets whose
/// purpose is not yet established.
typedef struct {
  real    field_0x00;            ///< offset=0x00
  real    field_0x04;            ///< offset=0x04
  real    primary_trigger;       ///< offset=0x08 input->primary_trigger
  real    look_yaw_delta;        ///< offset=0x0c added to desired_angles.yaw
  real    look_pitch_delta;      ///< offset=0x10 added to desired_angles.pitch
  uint8_t field_0x14;            ///< offset=0x14
  uint8_t field_0x15;            ///< offset=0x15
  uint8_t pad_0x16[0x2];         ///< offset=0x16
  uint32_t field_0x18;           ///< offset=0x18
  uint32_t action_flags;         ///< offset=0x1c bit1 grenade switch, bit2 melee/throw
} player_input_t;
/// size=0x20
/// The action handle_one_player_input builds from a player control slot and
/// hands to update_client_queue. Bungie calls the local "action" and the angle
/// pair "desired_facing" -- both verbatim from this function's own assert
/// strings "action.desired_facing.yaw"/".pitch" (player_control.c:0x369-0x36a),
/// which guard +0x04 and +0x08. The three index fields and primary_trigger are
/// copied straight from the identically-named player_control_t fields;
/// field_0xNN are copied from player_control_t fields that are themselves not
/// yet identified.
/// buttons/throttle_x/throttle_y names come from the prior recovery that lived
/// as a local typedef in game/players.c (bit 6 binoculars, bit 14 zoom, bit 7
/// alt_attack); consolidated here so there is one definition.
typedef struct {
  uint32_t buttons;              ///< offset=0x00 bit6 binoculars, bit7 alt_attack, bit14 zoom
  real    desired_facing_yaw;    ///< offset=0x04 action.desired_facing.yaw
  real    desired_facing_pitch;  ///< offset=0x08 action.desired_facing.pitch
  real    throttle_x;            ///< offset=0x0c
  real    throttle_y;            ///< offset=0x10
  real    primary_trigger;       ///< offset=0x14
  int16_t desired_weapon_index;  ///< offset=0x18
  int16_t desired_grenade_index; ///< offset=0x1a
  int16_t desired_zoom_level;    ///< offset=0x1c
  uint8_t pad_0x1e[0x2];         ///< offset=0x1e
} player_action_t;
cs(player_action_t, 0x20);
co(player_action_t, buttons,               0x00);
co(player_action_t, throttle_x,            0x0c);
co(player_action_t, throttle_y,            0x10);
co(player_action_t, desired_facing_yaw,    0x04);
co(player_action_t, desired_facing_pitch,  0x08);
co(player_action_t, primary_trigger,       0x14);
co(player_action_t, desired_weapon_index,  0x18);
co(player_action_t, desired_grenade_index, 0x1a);
co(player_action_t, desired_zoom_level,    0x1c);

cs(player_input_t, 0x20);
co(player_input_t, primary_trigger,  0x08);
co(player_input_t, look_yaw_delta,   0x0c);
co(player_input_t, look_pitch_delta, 0x10);
co(player_input_t, field_0x14,       0x14);
co(player_input_t, field_0x15,       0x15);
co(player_input_t, field_0x18,       0x18);
co(player_input_t, action_flags,     0x1c);

/// size=0x38
typedef struct {
  char    name[32];                ///< offset=0x00
  int16_t maximum_count;           ///< offset=0x20
  int16_t size;                    ///< offset=0x22
  bool    valid;                   ///< offset=0x24
  bool    identifier_zero_invalid; ///< offset=0x25
  char    unk_38[2];               ///< offset=0x26
  int     magic;                   ///< offset=0x28
  char    unk_44[2];               ///< offset=0x2c
  int16_t current_count;           ///< offset=0x2e
  int16_t unk_48;                  ///< offset=0x30
  char    unk_50[2];               ///< offset=0x32
  void    *data;                   ///< offset=0x34
} data_t;

/// size=0x10
typedef struct {
  data_t   *data;        ///< offset=0x00
  uint16_t index;        ///< offset=0x04
  char     unk_6[2];     ///< offset=0x06
  uint32_t datum_handle; ///< offset=0x08
  uint32_t cookie;       ///< offset=0x0c
} data_iter_t;

/// Object iterator state block, 0x10 bytes.
/// Initialised by object_iterator_new (0x13d6f0),
/// advanced by object_iterator_next (0x13d730).
/// size=0x10
typedef struct {
  int32_t  type_mask;     ///< offset=0x00  bitmask of accepted object types (1<<type)
  uint8_t  flags;         ///< offset=0x04  required header flags byte (AND/CMP filter)
  char     pad_5[1];      ///< offset=0x05
  int16_t  current_index; ///< offset=0x06  next slot index to probe
  int32_t  last_handle;   ///< offset=0x08  handle returned by previous call (or NONE)
  uint32_t cookie;        ///< offset=0x0c  0x86868686 when initialized
} object_iter_t;
cs(object_iter_t, 0x10);
co(object_iter_t, type_mask, 0x00);
co(object_iter_t, flags, 0x04);
co(object_iter_t, current_index, 0x06);
co(object_iter_t, last_handle, 0x08);
co(object_iter_t, cookie, 0x0c);

/// size=8
typedef struct
{
  int16_t y0; ///< offset=0x00
  int16_t x0; ///< offset=0x02
  int16_t y1; ///< offset=0x04
  int16_t x1; ///< offset=0x06
} viewport_bounds_t;

/// size=0x10. Field order from render_camera_build_frustum (0x187250): the
/// default bounds store -1.0 to +0x00/+0x08 and +1.0 to +0x04/+0x0c.
typedef struct {
  real x0; ///< offset=0x00
  real x1; ///< offset=0x04
  real y0; ///< offset=0x08
  real y1; ///< offset=0x0c
} real_rectangle2d;
cs(real_rectangle2d, 0x10);

/// size=0x18. Axis-aligned box; render_frustum_cube_view_fraction (0x185ad0)
/// asserts x0<=x1 at +0x00/+0x04, y0<=y1 at +0x08/+0x0c, z0<=z1 at +0x10/+0x14.
typedef struct {
  real x0; ///< offset=0x00
  real x1; ///< offset=0x04
  real y0; ///< offset=0x08
  real y1; ///< offset=0x0c
  real z0; ///< offset=0x10
  real z1; ///< offset=0x14
} real_rectangle3d;
cs(real_rectangle3d, 0x18);

/// size=0x34. Scale, three basis rows, then translation; matrix_inverse
/// (0x109150) / matrix_transform_point (0x109590) operand layout.
typedef struct {
  real      scale;    ///< offset=0x00
  vector3_t forward;  ///< offset=0x04
  vector3_t left;     ///< offset=0x10
  vector3_t up;       ///< offset=0x1c
  vector3_t position; ///< offset=0x28
} real_matrix4x3;
cs(real_matrix4x3, 0x34);
co(real_matrix4x3, forward, 0x04);
co(real_matrix4x3, position, 0x28);

/// size=0x54
typedef struct {
  vector3_t         field_00;               ///< offset=0x00
  vector3_t         field_0c;               ///< offset=0x0c
  vector3_t         field_18;               ///< offset=0x18
  uint8_t           unk_36;                 ///< offset=0x24
  char              unk_37[3];              ///< offset=0x25
  float             vertical_field_of_view; ///< offset=0x28
  viewport_bounds_t viewport_bounds;        ///< offset=0x2c
  viewport_bounds_t unk_52;                 ///< offset=0x34
  float             z_near;                 ///< offset=0x3c
  float             z_far;                  ///< offset=0x40
  float             field_44[4];            ///< offset=0x44
} camera_t;
cs(camera_t, 0x54);

/// size=0x10. PAL real_argb_color: alpha first.
typedef struct {
  real alpha; ///< offset=0x00
  real red;   ///< offset=0x04
  real green; ///< offset=0x08
  real blue;  ///< offset=0x0c
} real_argb_color;
cs(real_argb_color, 0x10);
co(camera_t, field_00, 0x00);
co(camera_t, field_0c, 0x0c);
co(camera_t, field_18, 0x18);
co(camera_t, vertical_field_of_view, 0x28);
co(camera_t, viewport_bounds, 0x2c);
co(camera_t, z_near, 0x3c);
co(camera_t, z_far, 0x40);
co(camera_t, field_44, 0x44);

/// size=0x18c. Recovered from render_camera_build_frustum (0x187250).
typedef struct {
  real_rectangle2d field_00;  ///< offset=0x000
  real_matrix4x3 field_10;    ///< offset=0x010
  real_matrix4x3 field_44;    ///< offset=0x044
  real_plane3d field_78[6];   ///< offset=0x078
  float        field_d8;      ///< offset=0x0d8
  float        field_dc;      ///< offset=0x0dc
  vector3_t    field_e0[5];   ///< offset=0x0e0 [4] = camera position
  vector3_t    field_11c;     ///< offset=0x11c
  float        field_128[6];  ///< offset=0x128
  uint8_t      field_140;     ///< offset=0x140
  uint8_t      pad_141[3];    ///< offset=0x141
  float        field_144[16]; ///< offset=0x144
  float        field_184[2];  ///< offset=0x184
} render_frustum_t;
cs(render_frustum_t, 0x18c);
co(render_frustum_t, field_00, 0x000);
co(render_frustum_t, field_10, 0x010);
co(render_frustum_t, field_44, 0x044);
co(render_frustum_t, field_78, 0x078);
co(render_frustum_t, field_d8, 0x0d8);
co(render_frustum_t, field_dc, 0x0dc);
co(render_frustum_t, field_e0, 0x0e0);
co(render_frustum_t, field_11c, 0x11c);
co(render_frustum_t, field_128, 0x128);
co(render_frustum_t, field_140, 0x140);
co(render_frustum_t, field_144, 0x144);
co(render_frustum_t, field_184, 0x184);

/// size=0xac
typedef struct {
  int16_t  unk_0; ///< offset=0x00
  int8_t   unk_2; ///< offset=0x02
  int8_t   unk_3; ///< offset=0x03
  camera_t cam0;  ///< offset=0x04
  camera_t cam1;  ///< offset=0x58
} pregame_render_info_t;

/// size=0x258
typedef struct {
  __int16  unk_0[4];     ///< offset=0x00
  camera_t camera;       ///< offset=0x08
  float    frustum[127]; ///< offset=0x5c
} window_parameters_t;

/// size=0x28
typedef struct {
  uint32_t unk_0;  ///< offset=0x00
  uint32_t unk_4;  ///< offset=0x04
  int64_t  unk_8;  ///< offset=0x08
  int64_t  unk_16; ///< offset=0x10
  int64_t  unk_24; ///< offset=0x18
  int64_t  unk_32; ///< offset=0x20
} unk_time_globals_t;

/// size=0xAC
typedef struct
{
  __int16           player;      ///< offset=0x00
  bool              unk_2;       ///< offset=0x02
  char              unk_3[129];  ///< offset=0x03
  viewport_bounds_t unk_132;     ///< offset=0x84
  viewport_bounds_t unk_140;     ///< offset=0x8c
  char              unk_148[24]; ///< offset=0x94
} window_t;

// FIXME: Structure size
/// size=0xF0
typedef struct
{
  _BYTE unk_0[60];   ///< offset=0x00
  _WORD type;        ///< offset=0x3C
  _BYTE unk_62[174]; ///< offset=0x3E
  int   unk_236;     ///< offset=0xEC
} scenario_t;

// FIXME: Merge adjacent globals into this structure
/// size=0x01
typedef struct
{
  bool main_menu_scenario_loaded; ///< offset=0x00
} main_globals_t;

/// size=0x10C
typedef struct
{
  uint32_t magic;      ///< offset=0x00
  char     unk_4[2];   ///< offset=0x04
  int16_t  unk_6;      ///< offset=0x06
  char     unk_8[260]; ///< offset=0x08
} file_ref_t;

/// size=0x1C
typedef struct
{
  /* Letterbox coverage fraction in [0,1]. cinematic_render (0x93140) ramps it
   * by elapsed_ticks/30 s toward 1 while unk_8 is set and toward 0 once it is
   * cleared, then scales it by 0.125 to size the bars. */
  float   letterbox_fraction; ///< offset=0x00
  int32_t field_04;    ///< offset=0x04 — game_time_get() stamped at cinematic_start
  bool unk_8;          ///< offset=0x08
  bool in_progress;    ///< offset=0x09
  bool can_be_skipped; ///< offset=0x0A
  /* Named from the kb.json symbol of its only writer,
   * cinematic_suppress_bsp_object_creation (0x93030), which stores its byte
   * parameter here (MOV byte ptr [ECX+0xb],AL). Byte-wide: keep it 1 byte. */
  char suppress_bsp_object_creation; ///< offset=0x0B
  /* The 16 bytes from +0x0C are memset to 0xFF by
   * cinematic_initialize_for_new_map (datum-handle style init).
   * cinematic_force_title writes 16-bit values at +0x0C and +0x0E. */
  int16_t field_0c;   ///< offset=0x0C — title index written by cinematic_force_title
  int16_t field_0e;   ///< offset=0x0E — cleared to 0 by cinematic_force_title
  char    unk_10[12]; ///< offset=0x10 — initialized to 0xFF (datum handles)
} cinematic_globals_t;

#define GAME_STATE_BASE_ADDRESS 0x80061000
#define TAG_CACHE_BASE_ADDRESS  0x803A6000

/// size=0x10
typedef struct
{
  void *game_state_base_address;    ///< offset=0x00
  void *tag_cache_base_address;     ///< offset=0x04
  void *texture_cache_base_address; ///< offset=0x08
  void *sound_cache_base_address;   ///< offset=0x0C
} physical_memory_map_globals_t;

#pragma pack()

/// size=0x14
/// Original source: c:\halo\SOURCE\memory\data_packets.c
typedef struct
{
    const char *name;        ///< offset=0x00  checked non-NULL
    uint32_t field_04;       ///< offset=0x04  unknown
    int16_t size;            ///< offset=0x08  decoded size, checked >= 0
    int16_t version;         ///< offset=0x0A  checked >= 0
    int16_t *fields;         ///< offset=0x0C  checked non-NULL
    uint8_t validated;       ///< offset=0x10  set to 1 after verification
} packet_definition;
cs(packet_definition, 0x14);
co(packet_definition, name, 0x00);
co(packet_definition, field_04, 0x04);
co(packet_definition, size, 0x08);
co(packet_definition, version, 0x0A);
co(packet_definition, fields, 0x0C);
co(packet_definition, validated, 0x10);

/// size=0x8
typedef struct
{
    int16_t packet_class;           ///< offset=0x00
    int16_t field_02;                ///< offset=0x02  always 0, unknown purpose
    packet_definition *definition;  ///< offset=0x04  can be NULL (skipped if so)
} packet_entry;
cs(packet_entry, 0x8);
co(packet_entry, packet_class, 0x00);
co(packet_entry, field_02, 0x02);
co(packet_entry, definition, 0x04);

/// size=0x14
/// Original source: c:\halo\SOURCE\memory\data_packet_groups.c
typedef struct
{
    const char *name;                      ///< offset=0x00  "network_game_messages_group"
    int16_t packet_count;                  ///< offset=0x04  35
    int16_t packet_class_count;            ///< offset=0x06  8
    int32_t maximum_decoded_packet_size;    ///< offset=0x08  1536
    int32_t maximum_encoded_packet_size;    ///< offset=0x0C  2048
    packet_entry *packets;                 ///< offset=0x10
} group_definition;
cs(group_definition, 0x14);
co(group_definition, name, 0x00);
co(group_definition, packet_count, 0x04);
co(group_definition, packet_class_count, 0x06);
co(group_definition, maximum_decoded_packet_size, 0x08);
co(group_definition, maximum_encoded_packet_size, 0x0C);
co(group_definition, packets, 0x10);

/// size=0x1
/// The encoded packet header is a single byte identifying the packet type.
/// sizeof(packet_header) == 1 per assert string in data_packet_groups.c:0x2a
typedef struct
{
    uint8_t type;
} packet_header;
cs(packet_header, 0x1);
co(packet_header, type, 0x00);

/// size=0x10
/// Original source: c:\halo\SOURCE\memory\data_encoding.c
typedef struct data_encoding_state
{
    void *buffer;                          ///< offset=0x00
    int32_t offset;                        ///< offset=0x04
    int32_t buffer_size;                   ///< offset=0x08
    char overflow;                         ///< offset=0x0C
    char pad_0d[3];                        ///< offset=0x0D
} data_encoding_state_t;
cs(data_encoding_state_t, 0x10);
co(data_encoding_state_t, buffer, 0x00);
co(data_encoding_state_t, offset, 0x04);
co(data_encoding_state_t, buffer_size, 0x08);
co(data_encoding_state_t, overflow, 0x0C);

/// Dynamic array struct used by memory/array.c.
/// Size: 0x0C bytes.
/// Original source: c:\halo\SOURCE\memory\array.c
typedef struct dynamic_array {
    int element_size;                      ///< offset=0x00
    int count;                             ///< offset=0x04
    void *elements;                        ///< offset=0x08
} dynamic_array_t;
cs(dynamic_array_t, 0x0C);
co(dynamic_array_t, element_size, 0x00);
co(dynamic_array_t, count, 0x04);
co(dynamic_array_t, elements, 0x08);

typedef int (*hashtable_hash_proc_t)(int user_data, const void *key);
typedef bool (*hashtable_compare_proc_t)(int user_data, const void *element, const void *key);

/// Hash table struct used by memory/hashtable.c.
/// Size: 0x28 bytes.
/// Original source: c:\halo\SOURCE\memory\hashtable.c
typedef struct hashtable {
    int16_t key_size;                      ///< offset=0x00
    int16_t element_size;                  ///< offset=0x02
    int16_t count;                         ///< offset=0x04
    int16_t capacity_bits;                 ///< offset=0x06
    float load_factor;                     ///< offset=0x08
    int32_t user_data;                     ///< offset=0x0C
    hashtable_hash_proc_t hash_proc;       ///< offset=0x10
    hashtable_compare_proc_t compare_proc; ///< offset=0x14
    uint32_t *bitmap;                      ///< offset=0x18
    dynamic_array_t array;                 ///< offset=0x1C
} hashtable_t;
cs(hashtable_t, 0x28);
co(hashtable_t, key_size, 0x00);
co(hashtable_t, element_size, 0x02);
co(hashtable_t, count, 0x04);
co(hashtable_t, capacity_bits, 0x06);
co(hashtable_t, load_factor, 0x08);
co(hashtable_t, user_data, 0x0C);
co(hashtable_t, hash_proc, 0x10);
co(hashtable_t, compare_proc, 0x14);
co(hashtable_t, bitmap, 0x18);
co(hashtable_t, array, 0x1C);

/* ai_firing_pos_entry_t — one slot in the firing-position candidate buffer
 * built by ai_find_line_of_fire_friend_pills and consumed by ai_test_line_of_fire.
 * Entry stride = 0x28 bytes; buffer holds up to 0x20 entries.
 *
 * Note: vec_b[3] as declared occupies +0x10..+0x18, but the binary only ever
 * writes two elements (vec_b[0] and vec_b[1] = 0.0f) via ai_generate_line_of_fire_pill.
 * scalar_a at +0x18 shares the same offset as vec_b[2] — the name
 * distinguishes its role (height_offset from biped_get_camera_height_and_offset).
 * Layout confirmed from ai_generate_line_of_fire_pill disasm stores at 0x41402–0x4141a. */
typedef struct {
    bool       occupied;   /* +0x00: 0 = candidate; 1 = selected winner */
    bool       is_sphere;  /* +0x01: 0 = segment test; 1 = sphere test  */
    int16_t    _pad;       /* +0x02: unused                              */
    float      vec_a[3];   /* +0x04: biped eye position (from biped_get_camera_height_and_offset) */
    float      vec_b[2];   /* +0x10: line direction or zero for sphere   */
    float      scalar_a;   /* +0x18: height_offset (biped camera height) */
    int        handle_a;   /* +0x1c: actor handle (return from prop_get_active_by_unit_index / local_10[0]) */
    int        handle_b;   /* +0x20: object/unit handle (EDI at call to ai_generate_line_of_fire_pill) */
    float      radius;     /* +0x24: camera_height + DAT_00256140        */
} ai_firing_pos_entry_t;   /* size = 0x28 */
cs(ai_firing_pos_entry_t, 0x28);
co(ai_firing_pos_entry_t, occupied,  0x00);
co(ai_firing_pos_entry_t, is_sphere, 0x01);
co(ai_firing_pos_entry_t, vec_a,     0x04);
co(ai_firing_pos_entry_t, vec_b,     0x10);
co(ai_firing_pos_entry_t, scalar_a,  0x18);
co(ai_firing_pos_entry_t, handle_a,  0x1c);
co(ai_firing_pos_entry_t, handle_b,  0x20);
co(ai_firing_pos_entry_t, radius,    0x24);

/* ---------------------------------------------------------------------------
 * actor_action_type — the discriminant in actor->state.action, an int16 field
 * at actor+0x6c (MOVSX EDX,word ptr [ESI+0x6c] @0x1d0da proves signed 16-bit).
 *
 * Bound: assert "(actor->state.action >= 0) && (actor->state.action <
 * NUMBER_OF_ACTOR_ACTIONS)" compiles to CMP AX,0xe at 0x1d0b4 and 0x1c325.
 *
 * Ordering comes from the action-definition table (stride 0x38, one char*
 * name per entry) whose names read, in index order: none, sleep, alert,
 * fight, flee, uncover, guard, search, wait, vehicle, charge, obey, converse,
 * avoid. Three asserts pin exact values against that order, and all three
 * agree (a one-stride shift of the table base would break all three):
 *   "actor->state.action == _actor_action_fight"  -> CMP word [ESI+0x6c],0x3  @0x1ef57
 *   "actor->state.action == _actor_action_guard"  -> CMP word [ESI+0x6c],0x6  @0x1cf29
 *   "actor->state.action == _actor_action_charge" -> CMP word [ESI+0x6c],0xa  @0x1eec3
 *
 * Values live in an int16_t field, so these are #defines rather than a C89
 * enum (which is int-width and could widen a load; see lift-learnings §24).
 * No typedef is declared: a typedef consumes MSVC internal symbol numbers and
 * perturbs $L label counters in every TU including types.h. Verified inert as
 * plain #defines (actions.obj .text byte-identical before/after).
 * ------------------------------------------------------------------------- */
#define _actor_action_none      0
#define _actor_action_sleep     1
#define _actor_action_alert     2
#define _actor_action_fight     3
#define _actor_action_flee      4
#define _actor_action_uncover   5
#define _actor_action_guard     6
#define _actor_action_search    7
#define _actor_action_wait      8
#define _actor_action_vehicle   9
#define _actor_action_charge    10
#define _actor_action_obey      11
#define _actor_action_converse  12
#define _actor_action_avoid     13
#define NUMBER_OF_ACTOR_ACTIONS 14

/* actor->target.target_type is an int16 field at actor+0x268 (MOVSX EAX,word
 * ptr [ESI+0x268] @0x3033c). Bound from assert "(actor->target.target_type >=
 * 0) && (actor->target.target_type < NUMBER_OF_ACTOR_TARGET_TYPES)" ->
 * CMP AX,0xc @0x30316. The individual member names are NOT yet recovered —
 * only the count is proven. */
#define NUMBER_OF_ACTOR_TARGET_TYPES 12

/* actor->control.current_fire_target_type — int16 at actor+0x60c (CMP word ptr
 * [ESI+0x60c],1 @0x22032, where ESI comes straight from datum_get on the actor
 * pool pointer 0x6325a4 loaded @0x22013, so the base register is proven).
 *
 * Values come from asserts that name the member and compare the field:
 *   "... == _actor_fire_target_prop"          -> CMP word [ESI+0x60c],1 @0x22032
 *   "... == _actor_fire_target_manual_point"  -> CMP AX,2 @0x232ee and @0x23b4f
 * Two independent sites agree on 2. The value 0 is never named by any assert
 * string, so it is deliberately left undefined rather than guessed. int16
 * field, so #define rather than a C89 int-width enum (lift-learnings §24). */
#define _actor_fire_target_prop         1
#define _actor_fire_target_manual_point 2

/* path_destination_t — actor movement target/order substructure (24 bytes).
 * Confirmed: assert "control.path_destination.orders_ignore_target_object_index"
 * at +0x14 (+0x480 in actor_t). */
typedef struct path_destination_t {
  int16_t mode;                                      /* +0x00 */
  char field_02;                                     /* +0x02 */
  char pad_03[0x1];                                  /* +0x03 */
  int16_t position_index;                            /* +0x04 */
  char pad_06[0x2];                                  /* +0x06 */
  float field_08;                                    /* +0x08 */
  float field_0c;                                    /* +0x0c */
  int32_t dest_node;                                 /* +0x10 */
  int32_t orders_ignore_target_object_index;         /* +0x14 */
} path_destination_t;
cs(path_destination_t, 0x18);
co(path_destination_t, mode, 0x00);
co(path_destination_t, field_02, 0x02);
co(path_destination_t, position_index, 0x04);
co(path_destination_t, field_08, 0x08);
co(path_destination_t, field_0c, 0x0c);
co(path_destination_t, dest_node, 0x10);
co(path_destination_t, orders_ignore_target_object_index, 0x14);

/* ---------------------------------------------------------------------------
 * actor_t — an element of the "actor" data_t pool.
 *
 * Size and count are exact, from the pool constructor at 0x3a995:
 *     push 0x724            ; element size = 1828
 *     push 0x100            ; maximum_count = 256
 *     push 0x256d04         ; name = "actor"
 *     call 0x1bfe10         ; game_state_data_new
 *     mov  [0x6325a4], eax  ; == ACTOR_TABLE_PTR (tools/equivalence/qmp_capture.py)
 *
 * Every named field below is anchored to an assert string in the XBE that spells
 * the field's full path verbatim (e.g. "realcmp(actor->input.facing_vector.k,
 * 0.0f)"), with the cited instruction giving offset, width, and signedness.
 * Widths come from the listing only, never the decompiler (lift-learnings §24).
 *
 * The original is NESTED — the assert strings show substructures meta, state,
 * control, input, output, target, stimuli, emotions, danger_zone and
 * firing_positions. Their exact start/end boundaries are NOT proven, so this
 * skeleton is deliberately FLAT with the dotted path flattened into the field
 * name. Re-nest only when a boundary is evidenced; do not guess one.
 *
 * Everything not cited stays `pad_XXX`. Unobserved is not the same as absent:
 * a pad byte means "never seen accessed", not "padding in the original".
 * Cross-reference: the prose block above actor_input_update in halo/ai/actors.c
 * records further INFERRED offsets (0x158 vehicle_handle, 0x1b0
 * active_grenade_handle, ...) which are deliberately NOT promoted to fields
 * here — they lack assert-string evidence. It also notes actor+0x120 is
 * actor_input_t of size 0xa8 (so 0x120..0x1c7), which contains the three input
 * vectors below and independently corroborates their offsets.
 * ------------------------------------------------------------------------- */
#pragma pack(1)
typedef struct {
  int16_t salt;                                       /* +0x000  data_t pool convention: 16-bit datum salt at element +0 */
  char pad_002[0x2];
  int16_t field_004;                                 /* +0x004  accessed 5x, meaning unproven */
  char field_006;                                    /* +0x006  accessed 7x, meaning unproven */
  char field_007;                                    /* +0x007  accessed 1x, meaning unproven */
  char field_008;                                    /* +0x008  accessed 7x, meaning unproven */
  char field_009;                                    /* +0x009  accessed 1x, meaning unproven */
  char field_00a;                                    /* +0x00a  accessed 1x, meaning unproven */
  char field_00b;                                    /* +0x00b  accessed 1x, meaning unproven */
  int32_t field_00c;                                 /* +0x00c  accessed 2x, meaning unproven */
  char pad_010[0x2];
  char field_012;                                    /* +0x012  accessed 3x, meaning unproven */
  char field_013;                                    /* +0x013  accessed 1x, meaning unproven */
  int16_t field_014;                                 /* +0x014  accessed 4x, meaning unproven */
  char pad_016[0x2];
  int32_t field_018;                                 /* +0x018  accessed 55x, meaning unproven */
  char field_01c;                                    /* +0x01c  accessed 4x, meaning unproven */
  char pad_01d[0x1];
  int16_t field_01e;                                 /* +0x01e  accessed 15x, meaning unproven */
  int16_t field_020;                                 /* +0x020  accessed 2x, meaning unproven */
  char pad_022[0x2];
  int32_t field_024;                                 /* +0x024  accessed 18x, meaning unproven */
  int32_t meta_swarm_cache_index;                     /* +0x028  CMP dword [ESI+0x28],-1 @0x16d66 (NONE sentinel) */
  int32_t field_02c;                                 /* +0x02c  accessed 15x, meaning unproven */
  int32_t field_030;                                 /* +0x030  accessed 1x, meaning unproven */
  uint32_t field_034;                                /* +0x034  accessed 4x, meaning unproven */
  int16_t field_038;                                 /* +0x038  accessed 1x, meaning unproven */
  int16_t field_03a;                                 /* +0x03a  accessed 1x, meaning unproven */
  int16_t field_03c;                                 /* +0x03c  accessed 1x, meaning unproven */
  int16_t field_03e;                                 /* +0x03e  accessed 5x, meaning unproven */
  char field_040;                                    /* +0x040  accessed 4x, meaning unproven */
  char pad_041[0x3];
  int32_t field_044;                                 /* +0x044  accessed 2x, meaning unproven */
  int16_t field_048;                                 /* +0x048  accessed 2x, meaning unproven */
  int16_t field_04a;                                 /* +0x04a  accessed 4x, meaning unproven */
  char field_04c;                                    /* +0x04c  accessed 9x, meaning unproven */
  char pad_04d[0x1];
  int16_t field_04e;                                 /* +0x04e  accessed 5x, meaning unproven */
  int32_t field_050;                                 /* +0x050  accessed 4x, meaning unproven */
  int32_t field_054;                                 /* +0x054  accessed 1x, meaning unproven */
  int32_t field_058;                                 /* +0x058  accessed 23x, meaning unproven */
  int32_t field_05c;                                 /* +0x05c  accessed 4x, meaning unproven */
  int16_t field_060;                                 /* +0x060  accessed 2x, meaning unproven */
  int16_t field_062;                                 /* +0x062  accessed 3x, meaning unproven */
  int32_t field_064;                                 /* +0x064  accessed 1x, meaning unproven */
  char field_068;                                    /* +0x068  accessed 1x, meaning unproven */
  char pad_069[0x1];
  int16_t field_06a;                                 /* +0x06a  accessed 9x, meaning unproven */
  int16_t state_action;                               /* +0x06c  CMP word [ESI+0x6c],3/6/0xa @0x1ef57/0x1cf29/0x1eec3 */
  int16_t field_06e;                                 /* +0x06e  accessed 15x, meaning unproven */
  char field_070;                                    /* +0x070  accessed 3x, meaning unproven */
  char pad_071[0x1];
  int16_t field_072;                                 /* +0x072  accessed 2x, meaning unproven */
  int16_t field_074;                                 /* +0x074  accessed 2x, meaning unproven */
  char pad_076[0x2];
  int32_t field_078;                                 /* +0x078  accessed 3x, meaning unproven */
  char pad_07c[0x8];
  int32_t field_084;                                 /* +0x084  accessed 1x, meaning unproven */
  int32_t field_088;                                 /* +0x088  accessed 1x, meaning unproven */
  char field_08c;                                    /* +0x08c  accessed 1x, meaning unproven */
  char field_08d;                                    /* +0x08d  accessed 1x, meaning unproven */
  char field_08e;                                    /* +0x08e  accessed 2x, meaning unproven */
  char pad_08f[0x1];
  int16_t field_090;                                 /* +0x090  accessed 1x, meaning unproven */
  int16_t field_092;                                 /* +0x092  accessed 3x, meaning unproven */
  int32_t field_094;                                 /* +0x094  accessed 1x, meaning unproven */
  char field_098;                                    /* +0x098  accessed 8x, meaning unproven */
  char field_099;                                    /* +0x099  accessed 10x, meaning unproven */
  char pad_09a[0x3];
  char field_09d;                                    /* +0x09d  accessed 7x, meaning unproven */
  uint8_t field_09e;                                 /* +0x09e  accessed 12x, meaning unproven */
  char field_09f;                                    /* +0x09f  accessed 8x, meaning unproven */
  char field_0a0;                                    /* +0x0a0  accessed 12x, meaning unproven */
  char field_0a1;                                    /* +0x0a1  accessed 13x, meaning unproven */
  char field_0a2;                                    /* +0x0a2  accessed 2x, meaning unproven */
  char field_0a3;                                    /* +0x0a3  accessed 3x, meaning unproven */
  char field_0a4;                                    /* +0x0a4  accessed 1x, meaning unproven */
  char field_0a5;                                    /* +0x0a5  accessed 2x, meaning unproven */
  char field_0a6;                                    /* +0x0a6  accessed 7x, meaning unproven */
  char pad_0a7[0x1];
  int16_t field_0a8;                                 /* +0x0a8  accessed 8x, meaning unproven */
  char field_0aa;                                    /* +0x0aa  accessed 10x, meaning unproven */
  char field_0ab;                                    /* +0x0ab  accessed 3x, meaning unproven */
  int16_t field_0ac;                                 /* +0x0ac  accessed 4x, meaning unproven */
  char pad_0ae[0x3];
  char field_0b1;                                    /* +0x0b1  accessed 1x, meaning unproven */
  char pad_0b2[0x4];
  uint8_t field_0b6;                                 /* +0x0b6  accessed 1x, meaning unproven */
  char pad_0b7[0x1];
  int32_t field_0b8;                                 /* +0x0b8  accessed 9x, meaning unproven */
  int32_t field_0bc;                                 /* +0x0bc  accessed 1x, meaning unproven */
  int16_t field_0c0;                                 /* +0x0c0  accessed 2x, meaning unproven */
  int16_t field_0c2;                                 /* +0x0c2  accessed 1x, meaning unproven */
  char pad_0c4[0x1];
  char field_0c5;                                    /* +0x0c5  accessed 5x, meaning unproven */
  int16_t field_0c6;                                 /* +0x0c6  accessed 4x, meaning unproven */
  char pad_0c8[0x2];
  int16_t field_0ca;                                 /* +0x0ca  accessed 3x, meaning unproven */
  char pad_0cc[0x4];
  int32_t field_0d0;                                 /* +0x0d0  accessed 2x, meaning unproven */
  float field_0d4;                                   /* +0x0d4  accessed 1x, meaning unproven */
  int32_t field_0d8;                                 /* +0x0d8  accessed 5x, meaning unproven */
  int32_t field_0dc;                                 /* +0x0dc  accessed 1x, meaning unproven */
  char field_0e0;                                    /* +0x0e0  accessed 1x, meaning unproven */
  char pad_0e1[0x3];
  int32_t field_0e4;                                 /* +0x0e4  accessed 2x, meaning unproven */
  int32_t field_0e8;                                 /* +0x0e8  accessed 1x, meaning unproven */
  int32_t field_0ec;                                 /* +0x0ec  accessed 1x, meaning unproven */
  char field_0f0;                                    /* +0x0f0  accessed 1x, meaning unproven */
  char pad_0f1[0x3];
  float field_0f4;                                   /* +0x0f4  accessed 1x, meaning unproven */
  char field_0f8;                                    /* +0x0f8  accessed 2x, meaning unproven */
  char pad_0f9[0x1];
  int16_t field_0fa;                                 /* +0x0fa  accessed 2x, meaning unproven */
  int16_t field_0fc;                                 /* +0x0fc  accessed 2x, meaning unproven */
  char field_0fe;                                    /* +0x0fe  accessed 1x, meaning unproven */
  char pad_0ff[0x1];
  int32_t field_100;                                 /* +0x100  accessed 1x, meaning unproven */
  int32_t field_104;                                 /* +0x104  accessed 1x, meaning unproven */
  int32_t field_108;                                 /* +0x108  accessed 1x, meaning unproven */
  int32_t field_10c;                                 /* +0x10c  accessed 1x, meaning unproven */
  char field_110;                                    /* +0x110  accessed 2x, meaning unproven */
  char pad_111[0xf];
  float field_120;                                   /* +0x120  accessed 1x, meaning unproven */
  float field_124;                                   /* +0x124  accessed 1x, meaning unproven */
  float field_128;                                   /* +0x128  accessed 3x, meaning unproven */
  float field_12c;                                   /* +0x12c  accessed 1x, meaning unproven */
  float field_130;                                   /* +0x130  accessed 1x, meaning unproven */
  float field_134;                                   /* +0x134  accessed 1x, meaning unproven */
  char pad_138[0xc];
  int32_t field_144;                                 /* +0x144  accessed 1x, meaning unproven */
  uint16_t field_148;                                /* +0x148  accessed 1x, meaning unproven */
  char pad_14a[0xe];
  int32_t field_158;                                 /* +0x158  accessed 10x, meaning unproven */
  char field_15c;                                    /* +0x15c  accessed 3x, meaning unproven */
  char field_15d;                                    /* +0x15d  accessed 2x, meaning unproven */
  int16_t field_15e;                                 /* +0x15e  accessed 6x, meaning unproven */
  char field_160;                                    /* +0x160  accessed 17x, meaning unproven */
  char field_161;                                    /* +0x161  accessed 2x, meaning unproven */
  char field_162;                                    /* +0x162  accessed 2x, meaning unproven */
  char pad_163[0x1];
  int32_t field_164;                                 /* +0x164  accessed 1x, meaning unproven */
  int32_t field_168;                                 /* +0x168  accessed 1x, meaning unproven */
  int32_t field_16c;                                 /* +0x16c  accessed 1x, meaning unproven */
  int32_t field_170;                                 /* +0x170  accessed 1x, meaning unproven */
  float input_facing_vector[3];                       /* +0x174  FLD [ESI+0x17c] @0x3e4fd = .k, so base 0x174 */
  float input_aiming_vector[3];                       /* +0x180  FLD [ESI+0x180/184/188] @0x3e411/3e407/3e3ee */
  float input_looking_vector[3];                      /* +0x18c  FLD [ESI+0x190/194] @0x3e467/0x3e44e */
  float field_198;                                   /* +0x198  accessed 2x, meaning unproven */
  float field_19c;                                   /* +0x19c  accessed 2x, meaning unproven */
  float field_1a0;                                   /* +0x1a0  accessed 2x, meaning unproven */
  float field_1a4;                                   /* +0x1a4  accessed 1x, meaning unproven */
  float field_1a8;                                   /* +0x1a8  accessed 1x, meaning unproven */
  float field_1ac;                                   /* +0x1ac  accessed 1x, meaning unproven */
  int32_t field_1b0;                                 /* +0x1b0  accessed 1x, meaning unproven */
  char field_1b4;                                    /* +0x1b4  accessed 1x, meaning unproven */
  char field_1b5;                                    /* +0x1b5  accessed 1x, meaning unproven */
  char pad_1b6[0x2];
  int32_t field_1b8;                                 /* +0x1b8  accessed 1x, meaning unproven */
  char pad_1bc[0x4];
  float field_1c0;                                   /* +0x1c0  pain boost / damage stun decay */
  int32_t field_1c4;                                 /* +0x1c4  accessed 1x, meaning unproven */
  char field_1c8;                                    /* +0x1c8  accessed 1x, meaning unproven */
  char field_1c9;                                    /* +0x1c9  accessed 1x, meaning unproven */
  char field_1ca;                                    /* +0x1ca  accessed 3x, meaning unproven */
  char field_1cb;                                    /* +0x1cb  accessed 5x, meaning unproven */
  char field_1cc;                                    /* +0x1cc  accessed 1x, meaning unproven */
  char pad_1cd[0x3];
  int32_t field_1d0;                                 /* +0x1d0  accessed 3x, meaning unproven */
  int16_t field_1d4;                                 /* +0x1d4  accessed 1x, meaning unproven */
  char pad_1d6[0x6];
  int32_t field_1dc;                                 /* +0x1dc  accessed 6x, meaning unproven */
  int32_t field_1e0;                                 /* +0x1e0  accessed 2x, meaning unproven */
  int16_t field_1e4;                                 /* +0x1e4  accessed 6x, meaning unproven */
  char pad_1e6[0x2];
  int32_t field_1e8;                                 /* +0x1e8  accessed 6x, meaning unproven */
  char pad_1ec[0x1];
  char field_1ed;                                    /* +0x1ed  accessed 1x, meaning unproven */
  char pad_1ee[0x8];
  char field_1f6;                                    /* +0x1f6  accessed 1x, meaning unproven */
  char pad_1f7[0x1];
  char field_1f8;                                    /* +0x1f8  accessed 1x, meaning unproven */
  char pad_1f9[0x3];
  char field_1fc;                                    /* +0x1fc  accessed 1x, meaning unproven */
  char pad_1fd[0x3];
  char field_200;                                    /* +0x200  accessed 1x, meaning unproven */
  char pad_201[0x1];
  char field_202;                                    /* +0x202  accessed 1x, meaning unproven */
  char pad_203[0x42];
  char field_245;                                    /* +0x245  accessed 1x, meaning unproven */
  char pad_246[0x22];
  int16_t target_target_type;                         /* +0x268  MOVSX EAX,word [ESI+0x268] @0x3033c */
  char pad_26a[0x2];
  int32_t field_26c;                                 /* +0x26c  accessed 1x, meaning unproven */
  int32_t target_target_prop_index;                   /* +0x270  CMP dword [ESI+0x270],-1 @0x38535 */
  char field_274;                                    /* +0x274  accessed 1x, meaning unproven */
  char pad_275[0x3];
  int32_t field_278;                                 /* +0x278  accessed 1x, meaning unproven */
  char field_27c;                                    /* +0x27c  accessed 1x, meaning unproven */
  char pad_27d[0x3];
  int16_t danger_zone_danger_type;                    /* +0x280  CMP word [ESI+0x280],0 @0x3239c; [EBX+0x280] @0x484e8 */
  char pad_282[0x2];
  int16_t field_284;                                 /* +0x284  accessed 1x, meaning unproven */
  char field_286;                                    /* +0x286  accessed 1x, meaning unproven */
  char field_287;                                    /* +0x287  accessed 2x, meaning unproven */
  char field_288;                                    /* +0x288  accessed 5x, meaning unproven */
  char pad_289[0x1];
  char field_28a;                                    /* +0x28a  accessed 1x, meaning unproven */
  char pad_28b[0x1];
  int32_t danger_zone_object_index;                   /* +0x28c  CMP dword [EBX+0x28c],-1 @0x484f2 */
  char pad_290[0x4];
  float field_294;                                   /* +0x294  accessed 9x, meaning unproven */
  char pad_298[0x18];
  float field_2b0;                                   /* +0x2b0  accessed 1x, meaning unproven */
  float field_2b4;                                   /* +0x2b4  accessed 1x, meaning unproven */
  float field_2b8;                                   /* +0x2b8  accessed 1x, meaning unproven */
  char pad_2bc[0xc];
  float field_2c8;                                   /* +0x2c8  accessed 1x, meaning unproven */
  float field_2cc;                                   /* +0x2cc  accessed 1x, meaning unproven */
  float field_2d0;                                   /* +0x2d0  accessed 1x, meaning unproven */
  float field_2d4;                                   /* +0x2d4  accessed 2x, meaning unproven */
  float field_2d8;                                   /* +0x2d8  accessed 3x, meaning unproven */
  float field_2dc;                                   /* +0x2dc  accessed 2x, meaning unproven */
  float field_2e0;                                   /* +0x2e0  accessed 2x, meaning unproven */
  float field_2e4;                                   /* +0x2e4  accessed 2x, meaning unproven */
  char pad_2e8[0x4];
  char field_2ec;                                    /* +0x2ec  accessed 1x, meaning unproven */
  char field_2ed;                                    /* +0x2ed  accessed 1x, meaning unproven */
  int16_t field_2ee;                                 /* +0x2ee  accessed 2x, meaning unproven */
  char field_2f0;                                    /* +0x2f0  accessed 1x, meaning unproven */
  char pad_2f1[0x3];
  int32_t field_2f4;                                 /* +0x2f4  accessed 3x, meaning unproven */
  char field_2f8;                                    /* +0x2f8  accessed 2x, meaning unproven */
  char pad_2f9[0x3];
  int32_t field_2fc;                                 /* +0x2fc  accessed 1x, meaning unproven */
  int32_t field_300;                                 /* +0x300  accessed 1x, meaning unproven */
  int32_t field_304;                                 /* +0x304  accessed 1x, meaning unproven */
  int16_t stimuli_panic_type;                         /* +0x308  CMP word [ESI+0x308],0 @0x1c61a */
  char pad_30a[0x2];
  int32_t stimuli_panic_prop_index;                   /* +0x30c  MOV EAX,[ESI+0x30c] @0x1c624 */
  int16_t field_310;                                 /* +0x310  accessed 13x, meaning unproven */
  int16_t field_312;                                 /* +0x312  accessed 2x, meaning unproven */
  char field_314;                                    /* +0x314  accessed 1x, meaning unproven */
  char pad_315[0x3];
  int32_t field_318;                                 /* +0x318  accessed 1x, meaning unproven */
  int32_t field_31c;                                 /* +0x31c  accessed 1x, meaning unproven */
  int32_t field_320;                                 /* +0x320  accessed 1x, meaning unproven */
  int32_t field_324;                                 /* +0x324  accessed 2x, meaning unproven */
  int32_t field_328;                                 /* +0x328  accessed 1x, meaning unproven */
  char field_32c;                                    /* +0x32c  accessed 1x, meaning unproven */
  char pad_32d[0x3];
  int32_t field_330;                                 /* +0x330  accessed 1x, meaning unproven */
  int32_t field_334;                                 /* +0x334  accessed 1x, meaning unproven */
  int32_t field_338;                                 /* +0x338  accessed 1x, meaning unproven */
  int16_t field_33c;                                 /* +0x33c  accessed 1x, meaning unproven */
  char pad_33e[0x2];
  int32_t field_340;                                 /* +0x340  accessed 3x, meaning unproven */
  int16_t field_344;                                 /* +0x344  accessed 1x, meaning unproven */
  char pad_346[0x2];
  char field_348;                                    /* +0x348  accessed 1x, meaning unproven */
  char pad_349[0x1];
  int16_t field_34a;                                 /* +0x34a  accessed 3x, meaning unproven */
  int32_t field_34c;                                 /* +0x34c  accessed 3x, meaning unproven */
  char pad_350[0x4];
  float field_354;                                   /* +0x354  accessed 3x, meaning unproven */
  char field_358;                                    /* +0x358  accessed 2x, meaning unproven */
  char pad_359[0x1];
  int16_t field_35a;                                 /* +0x35a  accessed 1x, meaning unproven */
  char pad_35c[0x4];
  int16_t field_360;                                 /* +0x360  accessed 1x, meaning unproven */
  char field_362;                                    /* +0x362  accessed 16x, meaning unproven */
  char field_363;                                    /* +0x363  accessed 4x, meaning unproven */
  int16_t field_364;                                 /* +0x364  accessed 4x, meaning unproven */
  int16_t field_366;                                 /* +0x366  accessed 3x, meaning unproven */
  int16_t field_368;                                 /* +0x368  accessed 2x, meaning unproven */
  char pad_36a[0x2];
  int32_t field_36c;                                 /* +0x36c  accessed 1x, meaning unproven */
  int32_t field_370;                                 /* +0x370  accessed 1x, meaning unproven */
  char field_374;                                    /* +0x374  accessed 1x, meaning unproven */
  char pad_375[0x1];
  uint8_t field_376;                                 /* +0x376  accessed 1x, meaning unproven */
  char field_377;                                    /* +0x377  accessed 2x, meaning unproven */
  char field_378;                                    /* +0x378  accessed 2x, meaning unproven */
  char field_379;                                    /* +0x379  accessed 2x, meaning unproven */
  char pad_37a[0x2];
  int32_t field_37c;                                 /* +0x37c  accessed 1x, meaning unproven */
  int32_t field_380;                                 /* +0x380  accessed 1x, meaning unproven */
  int32_t field_384;                                 /* +0x384  accessed 1x, meaning unproven */
  int32_t field_388;                                 /* +0x388  accessed 1x, meaning unproven */
  char field_38c;                                    /* +0x38c  accessed 3x, meaning unproven */
  char pad_38d[0x3];
  int32_t field_390;                                 /* +0x390  accessed 1x, meaning unproven */
  int32_t field_394;                                 /* +0x394  accessed 1x, meaning unproven */
  int32_t field_398;                                 /* +0x398  accessed 1x, meaning unproven */
  int32_t field_39c;                                 /* +0x39c  accessed 1x, meaning unproven */
  int32_t field_3a0;                                 /* +0x3a0  accessed 2x, meaning unproven */
  int32_t field_3a4;                                 /* +0x3a4  accessed 1x, meaning unproven */
  int16_t field_3a8;                                 /* +0x3a8  accessed 2x, meaning unproven */
  char pad_3aa[0x2];
  int32_t field_3ac;                                 /* +0x3ac  accessed 3x, meaning unproven */
  int32_t field_3b0;                                 /* +0x3b0  accessed 1x, meaning unproven */
  float field_3b4;                                   /* +0x3b4  accessed 1x, meaning unproven */
  int16_t firing_positions_current_position_index;    /* +0x3b8  MOVSX EDX,word [ESI+0x3b8] @0x5b463 */
  char field_3ba;                                    /* +0x3ba  accessed 7x, meaning unproven */
  char field_3bb;                                    /* +0x3bb  accessed 1x, meaning unproven */
  char field_3bc;                                    /* +0x3bc  accessed 2x, meaning unproven */
  char field_3bd;                                    /* +0x3bd  accessed 3x, meaning unproven */
  char pad_3be[0x2];
  int32_t field_3c0;                                 /* +0x3c0  accessed 1x, meaning unproven */
  int16_t field_3c4;                                 /* +0x3c4  accessed 5x, meaning unproven */
  int16_t field_3c6;                                 /* +0x3c6  discarded-firing-position ring cursor, MOVSX word @0x24c09/0x24c17/0x24c26 */
  /* +0x3c8  four-entry discarded-firing-position ring. Stride 4 comes from the
   * `index += 4` byte walk in actor_clear_discarded_firing_positions and the
   * `% 4` cursor wrap in actor_discard_firing_position; the record boundary itself is unproven,
   * only the two written halves are named. */
  struct {
    char field_00;                                   /* +0x00   the param_3 flag stored alongside the index */
    char pad_01[0x1];
    int16_t field_02;                                /* +0x02   firing-position index, reset to NONE */
  } field_3c8[4];
  char field_3d8;                                    /* +0x3d8  accessed 1x, meaning unproven */
  char field_3d9;                                    /* +0x3d9  latched copy of the ring's param_3 flag */
  char pad_3da[0x2];
  int32_t field_3dc;                                 /* +0x3dc  accessed 1x, meaning unproven */
  int32_t field_3e0;                                 /* +0x3e0  accessed 1x, meaning unproven */
  int32_t field_3e4;                                 /* +0x3e4  accessed 1x, meaning unproven */
  int16_t field_3e8;                                 /* +0x3e8  accessed 30x, meaning unproven */
  char pad_3ea[0x2];
  int16_t field_3ec;                                 /* +0x3ec  accessed 21x, meaning unproven */
  char pad_3ee[0x2];
  int32_t field_3f0;                                 /* +0x3f0  accessed 6x, meaning unproven */
  int32_t field_3f4;                                 /* +0x3f4  accessed 2x, meaning unproven */
  int32_t field_3f8;                                 /* +0x3f8  accessed 3x, meaning unproven */
  int16_t field_3fc;                                 /* +0x3fc  accessed 14x, meaning unproven */
  char pad_3fe[0x2];
  union {
    struct {
      int16_t field_400;                             /* +0x400  accessed 3x, meaning unproven */
      char field_402;                                /* +0x402  accessed 1x, meaning unproven */
      char pad_403[0x1];
      int16_t field_404;                             /* +0x404 */
      char pad_406[0x2];
      float field_408;                               /* +0x408  accessed 2x, meaning unproven */
      float field_40c;                               /* +0x40c  accessed 1x, meaning unproven */
      int32_t field_410;                             /* +0x410  accessed 1x, meaning unproven */
      int32_t field_414;                             /* +0x414  accessed 3x, meaning unproven */
    };
    path_destination_t pending_destination;          /* +0x400 */
  };
  int16_t field_418;                                 /* +0x418  accessed 1x, meaning unproven */
  char pad_41a[0x2];
  int32_t field_41c;                                 /* +0x41c  accessed 2x, meaning unproven */
  int32_t field_420;                                 /* +0x420  accessed 2x, meaning unproven */
  char field_424;                                    /* +0x424  accessed 6x, meaning unproven */
  char field_425;                                    /* +0x425  accessed 6x, meaning unproven */
  char field_426;                                    /* +0x426  accessed 10x, meaning unproven */
  char field_427;                                    /* +0x427  accessed 8x, meaning unproven */
  char field_428;                                    /* +0x428  accessed 6x, meaning unproven */
  char field_429;                                    /* +0x429  accessed 2x, meaning unproven */
  char field_42a;                                    /* +0x42a  accessed 1x, meaning unproven */
  char pad_42b[0x1];
  int16_t field_42c;                                 /* +0x42c  accessed 1x, meaning unproven */
  int16_t field_42e;                                 /* +0x42e  accessed 2x, meaning unproven */
  char field_430;                                    /* +0x430  accessed 2x, meaning unproven */
  char pad_431[0x3];
  int32_t field_434;                                 /* +0x434  accessed 2x, meaning unproven */
  int32_t field_438;                                 /* +0x438  accessed 2x, meaning unproven */
  int32_t field_43c;                                 /* +0x43c  accessed 2x, meaning unproven */
  char field_440;                                    /* +0x440  accessed 1x, meaning unproven */
  char field_441;                                    /* +0x441  accessed 1x, meaning unproven */
  char field_442;                                    /* +0x442  accessed 1x, meaning unproven */
  char pad_443[0x1];
  float field_444;                                   /* +0x444  accessed 1x, meaning unproven */
  float field_448;                                   /* +0x448  accessed 1x, meaning unproven */
  float field_44c;                                   /* +0x44c  accessed 1x, meaning unproven */
  float field_450;                                   /* +0x450  accessed 1x, meaning unproven */
  char field_454;                                    /* +0x454  accessed 12x, meaning unproven */
  char field_455;                                    /* +0x455  accessed 1x, meaning unproven */
  char field_456;                                    /* +0x456  accessed 2x, meaning unproven */
  char field_457;                                    /* +0x457  accessed 1x, meaning unproven */
  int32_t field_458;                                 /* +0x458  accessed 1x, meaning unproven */
  char field_45c;                                    /* +0x45c  accessed 1x, meaning unproven */
  char field_45d;                                    /* +0x45d  accessed 2x, meaning unproven */
  char pad_45e[0xe];
  union {
    struct {
      int16_t field_46c;                             /* +0x46c  accessed 2x, meaning unproven */
      char field_46e;                                /* +0x46e  accessed 1x, meaning unproven */
      char pad_46f[0x1];
      int16_t field_470;                             /* +0x470  accessed 2x, meaning unproven */
      char pad_472[0x2];
      float field_474;
      float field_478;
      int32_t field_47c;                             /* +0x47c  accessed 1x, meaning unproven */
      int32_t control_path_destination_orders_ignore_target_object_index;/* +0x480  CMP dword [ESI+0x480],-1 @0x2d16b (NONE sentinel) */
    };
    path_destination_t active_destination;           /* +0x46c */
  };
  char field_484;                                    /* +0x484  accessed 6x, meaning unproven */
  char pad_485[0x3];
  float field_488;                                   /* +0x488  accessed 1x, meaning unproven */
  float field_48c;                                   /* +0x48c  accessed 1x, meaning unproven */
  float field_490;                                   /* +0x490  accessed 1x, meaning unproven */
  int32_t field_494;                                 /* +0x494  accessed 2x, meaning unproven */
  char pad_498[0x8];
  int32_t field_4a0;                                 /* +0x4a0  path step counter */
  char field_4a4;                                    /* +0x4a4  accessed 1x, meaning unproven */
  char pad_4a5[0x3];
  char field_4a8;                                    /* +0x4a8  accessed 3x, meaning unproven */
  char pad_4a9[0x13];
  float field_4bc;                                   /* +0x4bc  accessed 2x, meaning unproven */
  char field_4c0;                                    /* +0x4c0  accessed 1x, meaning unproven */
  int8_t field_4c1;                                  /* +0x4c1  accessed 2x, meaning unproven */
  int8_t field_4c2;                                  /* +0x4c2  accessed 3x, meaning unproven */
  char pad_4c3[0x41];
  char field_504;                                    /* +0x504  accessed 10x, meaning unproven */
  char field_505;                                    /* +0x505  accessed 1x, meaning unproven */
  char field_506;                                    /* +0x506  accessed 8x, meaning unproven */
  char field_507;                                    /* +0x507  accessed 1x, meaning unproven */
  char pad_508[0x2];
  int16_t field_50a;                                 /* +0x50a  accessed 1x, meaning unproven */
  float field_50c;                                   /* +0x50c  accessed 3x, meaning unproven */
  float field_510;                                   /* +0x510  accessed 3x, meaning unproven */
  float field_514;                                   /* +0x514  accessed 3x, meaning unproven */
  int32_t field_518;                                 /* +0x518  accessed 1x, meaning unproven */
  int32_t field_51c;                                 /* +0x51c  accessed 1x, meaning unproven */
  int32_t field_520;                                 /* +0x520  accessed 1x, meaning unproven */
  float field_524;                                   /* +0x524  accessed 1x, meaning unproven */
  float field_528;                                   /* +0x528  accessed 1x, meaning unproven */
  float field_52c;                                   /* +0x52c  accessed 1x, meaning unproven */
  char field_530;                                    /* +0x530  accessed 5x, meaning unproven */
  char pad_531[0x13];
  int16_t control_secondary_look_type;                /* +0x544  CMP word [ESI+0x544],0 @0x6443d */
  int16_t secondary_look_priority;                   /* +0x546  NTSC writes priority and consumes secondary mode priority as int16_t; name_source: halocea */
  int16_t secondary_look_timer;                      /* +0x548  NTSC writes tick_count, decrements while positive, and expires at zero as int16_t; name_source: halocea */
  char pad_54a[0x2];
  int16_t control_secondary_look_direction_type;      /* +0x54c  CMP word [ESI+0x54c],1 @0x64447 */
  char pad_54e[0x2];
  int32_t control_secondary_look_direction_prop_index;/* +0x550  CMP dword [ESI+0x550],EDI @0x64451 */
  int32_t field_554;                                 /* +0x554  accessed 1x, meaning unproven */
  int32_t field_558;                                 /* +0x558  accessed 1x, meaning unproven */
  char control_idle_major_active;                     /* +0x55c  MOV AL,byte [ESI+0x55c] @0x64479, @0x299d7 */
  char field_55d;                                    /* +0x55d  accessed 2x, meaning unproven */
  char field_55e;                                    /* +0x55e  accessed 4x, meaning unproven */
  char control_idle_minor_active;                     /* +0x55f  MOV AL,byte [ESI+0x55f] @0x644b5 */
  int32_t field_560;                                 /* +0x560  accessed 4x, meaning unproven */
  int32_t control_idle_major_timer;                   /* +0x564  MOV EAX,[ESI+0x564] @0x299e4 */
  int32_t control_idle_minor_timer;                   /* +0x568  MOV EAX,[ESI+0x568]; TEST; JG @0x29c49..0x29c54 */
  int16_t control_idle_major_direction_type;          /* +0x56c  CMP word [ESI+0x56c],1 @0x64483 */
  char pad_56e[0x2];
  int32_t control_idle_major_direction_prop_index;    /* +0x570  CMP dword [ESI+0x570],EDI @0x6448d */
  float field_574;                                   /* +0x574  accessed 1x, meaning unproven */
  float field_578;                                   /* +0x578  accessed 1x, meaning unproven */
  int16_t control_idle_minor_direction_type;          /* +0x57c  CMP word [ESI+0x57c],1 @0x644bf */
  char pad_57e[0x2];
  int32_t control_idle_minor_direction_prop_index;    /* +0x580  CMP dword [ESI+0x580],EDI @0x644c9 */
  float field_584;                                   /* +0x584  accessed 1x, meaning unproven */
  float field_588;                                   /* +0x588  accessed 1x, meaning unproven */
  char field_58c;                                    /* +0x58c  accessed 5x, meaning unproven */
  char field_58d;                                    /* +0x58d  accessed 4x, meaning unproven */
  char field_58e;                                    /* +0x58e  accessed 2x, meaning unproven */
  char field_58f;                                    /* +0x58f  accessed 1x, meaning unproven */
  char field_590;                                    /* +0x590  accessed 4x, meaning unproven */
  char field_591;                                    /* +0x591  accessed 6x, meaning unproven */
  char pad_592[0x6];
  float field_598;                                   /* +0x598  accessed 1x, meaning unproven */
  float field_59c;                                   /* +0x59c  accessed 1x, meaning unproven */
  float field_5a0;                                   /* +0x5a0  accessed 1x, meaning unproven */
  float control_desired_facing_vector[3];             /* +0x5a4  LEA EDI,[ESI+0x5a4] @0x2906b */
  float control_desired_aiming_vector[3];             /* +0x5b0  LEA EBX,[ESI+0x5b0] @0x290d8 */
  float control_desired_looking_vector[3];            /* +0x5bc  LEA EBX,[ESI+0x5bc] @0x2913d */
  char pad_5c8[0x10];
  int16_t field_5d8;                                 /* +0x5d8  accessed 1x, meaning unproven */
  char pad_5da[0x2];
  char field_5dc;                                    /* +0x5dc  accessed 1x, meaning unproven */
  char pad_5dd[0x13];
  int16_t field_5f0;                                 /* +0x5f0  accessed 1x, meaning unproven */
  int16_t control_fire_state;                         /* +0x5f2  MOVSX from word [EBX+0x5f2] @0x237d7, 5-case jump table */
  int16_t field_5f4;                                 /* +0x5f4  accessed 1x, meaning unproven */
  int16_t field_5f6;                                 /* +0x5f6  accessed 1x, meaning unproven */
  int16_t field_5f8;                                 /* +0x5f8  accessed 1x, meaning unproven */
  int16_t field_5fa;                                 /* +0x5fa  accessed 1x, meaning unproven */
  char field_5fc;                                    /* +0x5fc  accessed 2x, meaning unproven */
  char pad_5fd[0x3];
  char field_600;                                    /* +0x600  accessed 2x, meaning unproven */
  char field_601;                                    /* +0x601  accessed 2x, meaning unproven */
  char field_602;                                    /* +0x602  accessed 1x, meaning unproven */
  char field_603;                                    /* +0x603  accessed 2x, meaning unproven */
  char field_604;                                    /* +0x604  accessed 4x, meaning unproven */
  char pad_605[0x3];
  float field_608;                                   /* +0x608  accessed 3x, meaning unproven */
  int16_t control_current_fire_target_type;           /* +0x60c  CMP word [ESI+0x60c],1 @0x22032; ESI from datum_get on ACTOR_TABLE_PTR @0x22013 */
  char pad_60e[0x2];
  int32_t control_current_fire_target_prop_index;     /* +0x610  MOV [EBX+0x610],EAX after CMP EAX,-1 @0x22f52-0x22f55 */
  float field_614;                                   /* +0x614  accessed 1x, meaning unproven */
  float field_618;                                   /* +0x618  accessed 1x, meaning unproven */
  int32_t field_61c;                                 /* +0x61c  accessed 1x, meaning unproven */
  char pad_620[0x8];
  char field_628;                                    /* +0x628  accessed 1x, meaning unproven */
  char pad_629[0x3];
  int32_t field_62c;                                 /* +0x62c  accessed 1x, meaning unproven */
  float field_630;                                   /* +0x630  accessed 1x, meaning unproven */
  float field_634;                                   /* +0x634  accessed 1x, meaning unproven */
  float field_638;                                   /* +0x638  accessed 1x, meaning unproven */
  int32_t field_63c;                                 /* +0x63c  accessed 1x, meaning unproven */
  uint16_t field_640;                                /* +0x640  accessed 1x, meaning unproven */
  char pad_642[0x2];
  int32_t field_644;                                 /* +0x644  accessed 1x, meaning unproven */
  float field_648;                                   /* +0x648  accessed 1x, meaning unproven */
  float field_64c;                                   /* +0x64c  accessed 2x, meaning unproven */
  float field_650;                                   /* +0x650  accessed 2x, meaning unproven */
  float field_654;                                   /* +0x654  accessed 2x, meaning unproven */
  char pad_658[0xc];
  float field_664;                                   /* +0x664  accessed 2x, meaning unproven */
  int16_t field_668;                                 /* +0x668  accessed 2x, meaning unproven */
  int16_t field_66a;                                 /* +0x66a  accessed 2x, meaning unproven */
  int16_t field_66c;                                 /* +0x66c  accessed 2x, meaning unproven */
  char pad_66e[0x2];
  float field_670;                                   /* +0x670  accessed 1x, meaning unproven */
  float field_674;                                   /* +0x674  accessed 1x, meaning unproven */
  float field_678;                                   /* +0x678  accessed 1x, meaning unproven */
  float field_67c;                                   /* +0x67c  accessed 1x, meaning unproven */
  float field_680;                                   /* +0x680  accessed 1x, meaning unproven */
  float field_684;                                   /* +0x684  accessed 1x, meaning unproven */
  char pad_688[0x4];
  float control_burst_aim_vector[3];                  /* +0x68c  LEA EDI,[EBX+0x68c] @0x23d1a */
  float field_698;                                   /* +0x698  accessed 2x, meaning unproven */
  float field_69c;                                   /* +0x69c  FLD [EDX+0x69c] @0x3f92c/0x3f93f, meaning unproven */
  char field_6a0;                                    /* +0x6a0  accessed 1x, meaning unproven */
  uint8_t field_6a1;                                 /* +0x6a1  accessed 1x, meaning unproven */
  char pad_6a2[0x2];
  int32_t field_6a4;                                 /* +0x6a4  accessed 1x, meaning unproven */
  float field_6a8;                                   /* +0x6a8  accessed 1x, meaning unproven */
  float field_6ac;                                   /* +0x6ac  accessed 1x, meaning unproven */
  char pad_6b0[0x4];
  int32_t field_6b4;                                 /* +0x6b4  accessed 3x, meaning unproven */
  int32_t field_6b8;                                 /* +0x6b8  accessed 1x, meaning unproven */
  float field_6bc;                                   /* +0x6bc  accessed 2x, meaning unproven */
  float field_6c0;                                   /* +0x6c0  accessed 2x, meaning unproven */
  float field_6c4;                                   /* +0x6c4  accessed 2x, meaning unproven */
  float field_6c8;                                   /* +0x6c8  accessed 2x, meaning unproven */
  char field_6cc;                                    /* +0x6cc  accessed 1x, meaning unproven */
  char pad_6cd[0x1];
  int16_t field_6ce;                                 /* +0x6ce  accessed 1x, meaning unproven */
  char pad_6d0[0x4];
  int16_t field_6d4;                                 /* +0x6d4  accessed 2x, meaning unproven */
  char pad_6d6[0x2];
  int32_t field_6d8;                                 /* +0x6d8  accessed 1x, meaning unproven */
  int16_t field_6dc;                                 /* +0x6dc  accessed 1x, meaning unproven */
  char pad_6de[0x2];
  float field_6e0;                                   /* +0x6e0  accessed 2x, meaning unproven */
  float field_6e4;                                   /* +0x6e4  accessed 2x, meaning unproven */
  float field_6e8;                                   /* +0x6e8  accessed 2x, meaning unproven */
  int32_t field_6ec;                                 /* +0x6ec  accessed 1x, meaning unproven */
  int32_t field_6f0;                                 /* +0x6f0  accessed 1x, meaning unproven */
  int32_t field_6f4;                                 /* +0x6f4  accessed 1x, meaning unproven */
  int16_t field_6f8;                                 /* +0x6f8  accessed 2x, meaning unproven */
  char pad_6fa[0x2];
  float output_facing_vector[3];                      /* +0x6fc  LEA EDI,[ESI+0x6fc] @0x2a0c8 */
  float output_aiming_vector[3];                      /* +0x708  LEA EDI,[ESI+0x708] @0x2a17d */
  float output_looking_vector[3];                     /* +0x714  LEA EDI,[ESI+0x714] (k/j at 0x71c/0x718 @0x2a1ec) */
  char pad_720[0x4];
} actor_t;
cs(actor_t, 0x724);
co(actor_t, salt,                                          0x000);
co(actor_t, meta_swarm_cache_index,                        0x028);
co(actor_t, state_action,                                  0x06c);
co(actor_t, input_facing_vector,                           0x174);
co(actor_t, input_aiming_vector,                           0x180);
co(actor_t, input_looking_vector,                          0x18c);
co(actor_t, target_target_type,                            0x268);
co(actor_t, target_target_prop_index,                      0x270);
co(actor_t, danger_zone_danger_type,                       0x280);
co(actor_t, danger_zone_object_index,                      0x28c);
co(actor_t, stimuli_panic_type,                            0x308);
co(actor_t, stimuli_panic_prop_index,                      0x30c);
co(actor_t, firing_positions_current_position_index,       0x3b8);
co(actor_t, control_path_destination_orders_ignore_target_object_index, 0x480);
co(actor_t, field_4a0,                                         0x4a0);
co(actor_t, control_secondary_look_type,                   0x544);
co(actor_t, secondary_look_priority,                     0x546);
co(actor_t, secondary_look_timer,                        0x548);
co(actor_t, control_secondary_look_direction_type,         0x54c);
co(actor_t, control_secondary_look_direction_prop_index,   0x550);
co(actor_t, control_idle_major_active,                     0x55c);
co(actor_t, control_idle_minor_active,                     0x55f);
co(actor_t, control_idle_major_timer,                      0x564);
co(actor_t, control_idle_minor_timer,                      0x568);
co(actor_t, control_idle_major_direction_type,             0x56c);
co(actor_t, control_idle_major_direction_prop_index,       0x570);
co(actor_t, control_idle_minor_direction_type,             0x57c);
co(actor_t, control_idle_minor_direction_prop_index,       0x580);
co(actor_t, control_desired_facing_vector,                 0x5a4);
co(actor_t, control_desired_aiming_vector,                 0x5b0);
co(actor_t, control_desired_looking_vector,                0x5bc);
co(actor_t, control_fire_state,                            0x5f2);
co(actor_t, control_current_fire_target_type,              0x60c);
co(actor_t, control_current_fire_target_prop_index,        0x610);
co(actor_t, control_burst_aim_vector,                      0x68c);
co(actor_t, output_facing_vector,                          0x6fc);
co(actor_t, output_aiming_vector,                          0x708);
co(actor_t, output_looking_vector,                         0x714);
#pragma pack()

/* ---------------------------------------------------------------------------
 * tag_block — the engine's ubiquitous tag-data block header: an element count
 * plus a pointer to the element array. Consumed everywhere via
 * tag_block_get_element(block, index, element_size). The 12-byte size is not
 * asserted directly here (no standalone allocation observed) but is proven by
 * the three consecutive blocks in encounter_definition (halo/ai/encounters.h):
 * squads@0x80,
 * platoons@0x8c, firing_positions@0x98 are exactly 0xc apart, so sizeof must
 * be 0xc for those co() offsets to hold.
 * ------------------------------------------------------------------------- */
typedef struct tag_block {
    int32_t  count;      /* +0x00: element count (evidence: "encounter_definition->squads.count" assert, encounters.c:0x5a4) */
    void    *address;    /* +0x04: element array base (block ptr passed to tag_block_get_element) */
    int32_t  field_08;   /* +0x08: block definition ptr; no runtime access observed */
} tag_block;
cs(tag_block, 0xc);

/// Contrail instance datum (contrail_data).  Offsets from render_contrail
/// (0x188010) and render_contrails (0x1887b0); names from PAL 2342
/// render_contrails.c.  Total size unverified.
typedef struct {
  uint8_t pad_00[4];                         ///< offset=0x00
  int32_t definition_index;                  ///< offset=0x04
  int32_t object_index;                      ///< offset=0x08
  int16_t attachment_index;                  ///< offset=0x0c
  uint8_t pad_0e[2];                         ///< offset=0x0e
  real    density;                           ///< offset=0x10
  int16_t sequence_index;                    ///< offset=0x14
  int16_t frame_index;                       ///< offset=0x16
  real    texture_offset_u;                  ///< offset=0x18
  real    texture_offset_v;                  ///< offset=0x1c
  uint8_t pad_20[0xc];                       ///< offset=0x20
  int16_t contrail_point_counts[4];          ///< offset=0x2c
  int32_t first_contrail_point_indices[4];   ///< offset=0x34
} contrail_datum_t;

/// Contrail point datum (contrail_point_data).  Offsets from render_contrail
/// (0x188010).  Total size unverified.
typedef struct {
  uint8_t   pad_00[2];                 ///< offset=0x00
  uint8_t   flags;                     ///< offset=0x02 (bit 1: transitioning)
  int8_t    state_index;               ///< offset=0x03
  real      time;                      ///< offset=0x04
  uint8_t   pad_08[4];                 ///< offset=0x08
  real      density;                   ///< offset=0x0c
  uint8_t   pad_10[0xc];               ///< offset=0x10
  vector3_t position;                  ///< offset=0x1c
  uint8_t   pad_28[0xc];               ///< offset=0x28
  int32_t   next_contrail_point_index; ///< offset=0x34
} contrail_point_datum_t;

/// size=0x68. contrail_definition.states element (tag_block_get_element
/// size 0x68 in render_contrail, 0x188010).
typedef struct {
  uint8_t         pad_00[0x40];      ///< offset=0x00
  real            width;             ///< offset=0x40
  real_argb_color color_lower_bound; ///< offset=0x44
  real_argb_color color_upper_bound; ///< offset=0x54
  uint32_t        scale_flags;       ///< offset=0x64 (0x10 width, 0x20 color)
} contrail_point_state_t;
cs(contrail_point_state_t, 0x68);

/// Embedded contrail shader; only framebuffer_fade_mode is observed here.
typedef struct {
  uint8_t  pad_00[0x2c];          ///< offset=0x00
  uint16_t framebuffer_fade_mode; ///< offset=0x2c
  uint8_t  pad_2e[0x86];          ///< offset=0x2e
} contrail_shader_t;
cs(contrail_shader_t, 0xb4);

/// 'cont' tag definition.  Offsets from render_contrail (0x188010),
/// contrail_fade (0x187f80) and render_contrails (0x1887b0).  Total size
/// unverified.
typedef struct {
  uint16_t          flags;               ///< offset=0x00 (1 first unfaded, 2 last unfaded, 0x40 fades slowly)
  uint16_t          scale_flags;         ///< offset=0x02 (0x40 repeats_u, 0x80 repeats_v)
  uint8_t           pad_04[0x14];        ///< offset=0x04
  int16_t           render_type;         ///< offset=0x18
  uint8_t           pad_1a[2];           ///< offset=0x1a
  real              texture_repeats_u;   ///< offset=0x1c
  real              texture_repeats_v;   ///< offset=0x20
  uint8_t           pad_24[0x18];        ///< offset=0x24
  int32_t           bitmap_index;        ///< offset=0x3c (bitmap tag_reference index)
  uint8_t           pad_40[0x44];        ///< offset=0x40
  contrail_shader_t shader;              ///< offset=0x84
  tag_block         states;              ///< offset=0x138
} contrail_definition_t;
co(tag_block, count,   0x00);
co(tag_block, address, 0x04);

/*
 * Collision BSP header and winged-edge elements. Offsets and strides are the
 * tag_block_get_element sites in collision_bsp.c / decals.c:
 *   header +0x0c planes, +0x3c surfaces (stride 0xc), +0x48 edges (0x18),
 *   +0x54 vertices (0x10). Surface[+0]=plane, [+4]=first_edge, [+8]=flags.
 *   Edge six dwords: start/end vertex, forward/reverse edge, left/right surface.
 */
typedef struct collision_bsp_t {
  char      pad_00[0x0c];
  tag_block planes;                 ///< offset=0x0c
  char      pad_18[0x24];
  tag_block surfaces;               ///< offset=0x3c
  tag_block edges;                  ///< offset=0x48
  tag_block vertices;               ///< offset=0x54
} collision_bsp_t;
cs(collision_bsp_t, 0x60);
co(collision_bsp_t, planes, 0x0c);
co(collision_bsp_t, surfaces, 0x3c);
co(collision_bsp_t, edges, 0x48);
co(collision_bsp_t, vertices, 0x54);

typedef struct collision_surface_t {
  int32_t plane;                    ///< offset=0x00
  int32_t first_edge;               ///< offset=0x04
  uint8_t flags;                    ///< offset=0x08
  uint8_t pad_09[3];
} collision_surface_t;
cs(collision_surface_t, 0x0c);
co(collision_surface_t, plane, 0x00);
co(collision_surface_t, first_edge, 0x04);
co(collision_surface_t, flags, 0x08);

typedef struct collision_edge_t {
  int32_t start_vertex;             ///< offset=0x00
  int32_t end_vertex;               ///< offset=0x04
  int32_t forward_edge;             ///< offset=0x08
  int32_t reverse_edge;             ///< offset=0x0c
  int32_t left_surface;             ///< offset=0x10
  int32_t right_surface;            ///< offset=0x14
} collision_edge_t;
cs(collision_edge_t, 0x18);
co(collision_edge_t, start_vertex, 0x00);
co(collision_edge_t, end_vertex, 0x04);
co(collision_edge_t, forward_edge, 0x08);
co(collision_edge_t, reverse_edge, 0x0c);
co(collision_edge_t, left_surface, 0x10);
co(collision_edge_t, right_surface, 0x14);

typedef struct collision_vertex_t {
  real    point[3];                 ///< offset=0x00
  int32_t field_0c;                 ///< offset=0x0c
} collision_vertex_t;
cs(collision_vertex_t, 0x10);
co(collision_vertex_t, point, 0x00);
co(collision_vertex_t, field_0c, 0x0c);

/// size=0x68
/* Structure-BSP cluster element. Only the runtime-decal range is proven. */
typedef struct {
  char    pad_00[0x0c];
  int16_t field_0c;
  uint16_t field_0e;
  char    pad_10[0x58];
} structure_bsp_cluster_t;
cs(structure_bsp_cluster_t, 0x68);
co(structure_bsp_cluster_t, field_0c, 0x0c);
co(structure_bsp_cluster_t, field_0e, 0x0e);

/// size=0x10
/* Structure runtime-decal element. */
typedef struct {
  char   pad_00[0x0c];
  uint8_t field_0c;
  int8_t pad_0d;
  int8_t field_0e;
  int8_t field_0f;
} structure_runtime_decal_t;
cs(structure_runtime_decal_t, 0x10);
co(structure_runtime_decal_t, field_0c, 0x0c);
co(structure_runtime_decal_t, field_0e, 0x0e);
co(structure_runtime_decal_t, field_0f, 0x0f);

/// size=0x264
/* Runtime structure-BSP tag blocks used by structure_decals_update. */
typedef struct {
  char pad_00[0x134];
  tag_block clusters;
  char pad_140[0x118];
  tag_block runtime_decals;
} structure_bsp_t;
cs(structure_bsp_t, 0x264);
co(structure_bsp_t, clusters, 0x134);
co(structure_bsp_t, runtime_decals, 0x258);

/// size=0x04
/* Global structure decal runtime state allocated at 0x4d8ec8. */
typedef struct {
  uint8_t field_00;
  char    pad_01[3];
} structure_decals_globals_t;
cs(structure_decals_globals_t, 0x04);
co(structure_decals_globals_t, field_00, 0x00);

/* effects/decals.c asserts: NUMBER_OF_DECAL_LAYERS, NUMBER_OF_DECAL_TYPES,
 * MAXIMUM_CLUSTERS_PER_STRUCTURE, MAXIMUM_DECAL_SURFACE_QUEUE_SIZE,
 * _decal_locked_bit, geometry->decal_surface_count. */
#define MAXIMUM_CLUSTERS_PER_STRUCTURE 512
#define NUMBER_OF_DECAL_LAYERS 5
#define NUMBER_OF_DECAL_TYPES 4
#define MAXIMUM_DECALS 2048
#define MAXIMUM_DECAL_SURFACE_QUEUE_SIZE 1024
#define _decal_locked_bit 0
#define _decal_permanent_bit 1

/// size=0x38  pool stride from game_state_data_new("decals", 0x800, 0x38)
typedef struct decal_datum_t {
  int16_t  datum_salt;             ///< offset=0x00
  int16_t  flags;                  ///< offset=0x02  T1: decal->flags / _decal_locked_bit
  int16_t  cluster_index;          ///< offset=0x04  T1: decal->cluster_index
  int16_t  layer;                  ///< offset=0x06  T1: decal->layer
  real     position[3];            ///< offset=0x08
  int32_t  birth_time;             ///< offset=0x14
  uint8_t  color_index;            ///< offset=0x18
  uint8_t  pad_19;                 ///< offset=0x19
  uint8_t  field_1a;               ///< offset=0x1a
  uint8_t  sprite_index;           ///< offset=0x1b
  real     lifetime;               ///< offset=0x1c
  real     decay_time;             ///< offset=0x20
  uint32_t color;                  ///< offset=0x24
  uint8_t  alpha;                  ///< offset=0x28
  uint8_t  pad_29;                 ///< offset=0x29
  int16_t  primitive_count;        ///< offset=0x2a
  int32_t  definition_index;       ///< offset=0x2c  T1: decal->definition_index
  int32_t  previous_decal_index;   ///< offset=0x30
  int32_t  next_decal_index;       ///< offset=0x34
} decal_datum_t;
cs(decal_datum_t, 0x38);
co(decal_datum_t, flags, 0x02);
co(decal_datum_t, cluster_index, 0x04);
co(decal_datum_t, layer, 0x06);
co(decal_datum_t, position, 0x08);
co(decal_datum_t, birth_time, 0x14);
co(decal_datum_t, color_index, 0x18);
co(decal_datum_t, lifetime, 0x1c);
co(decal_datum_t, decay_time, 0x20);
co(decal_datum_t, color, 0x24);
co(decal_datum_t, alpha, 0x28);
co(decal_datum_t, primitive_count, 0x2a);
co(decal_datum_t, definition_index, 0x2c);
co(decal_datum_t, previous_decal_index, 0x30);
co(decal_datum_t, next_decal_index, 0x34);

/// size=0x280c  from game_state_malloc("decal globals", 0, 0x280c)
typedef struct decal_globals_t {
  int32_t first_decal_index[NUMBER_OF_DECAL_LAYERS][MAXIMUM_CLUSTERS_PER_STRUCTURE]; ///< offset=0x00
  int32_t first_disconnected_decal_index; ///< offset=0x2800  T1
  int32_t locked_count;                   ///< offset=0x2804
  int32_t permanent_count;                ///< offset=0x2808  T1: decal_globals->permanent_count
} decal_globals_t;
cs(decal_globals_t, 0x280c);
co(decal_globals_t, first_decal_index, 0x00);
co(decal_globals_t, first_disconnected_decal_index, 0x2800);
co(decal_globals_t, locked_count, 0x2804);
co(decal_globals_t, permanent_count, 0x2808);

/// size=0x18  FUN_0009a5a0 vertex stride
typedef struct decal_geometry_vertex_t {
  real    position[3]; ///< offset=0x00
  real    uv[2];       ///< offset=0x0c
  boolean clipped;     ///< offset=0x14
  uint8_t pad_15[3];   ///< offset=0x15
} decal_geometry_vertex_t;
cs(decal_geometry_vertex_t, 0x18);
co(decal_geometry_vertex_t, uv, 0x0c);
co(decal_geometry_vertex_t, clipped, 0x14);

/// size=0x7804  bound at 0x44dfd8 by decal_new_from_collision
typedef struct decal_geometry_scratch_t {
  decal_geometry_vertex_t vertices[MAXIMUM_DECAL_SURFACE_QUEUE_SIZE];
  int16_t vertex_count;                                                ///< offset=0x6000
  int16_t surface_vertex_counts[MAXIMUM_DECAL_SURFACE_QUEUE_SIZE];     ///< offset=0x6002
  int16_t decal_surface_count;                                         ///< offset=0x6802  T1
  int32_t surfaces[MAXIMUM_DECAL_SURFACE_QUEUE_SIZE];                  ///< offset=0x6804
} decal_geometry_scratch_t;
cs(decal_geometry_scratch_t, 0x7804);
co(decal_geometry_scratch_t, vertex_count, 0x6000);
co(decal_geometry_scratch_t, surface_vertex_counts, 0x6002);
co(decal_geometry_scratch_t, decal_surface_count, 0x6802);
co(decal_geometry_scratch_t, surfaces, 0x6804);

/// size=0x8c  g_decal_projection[35] overlay used by FUN_0009a300
typedef struct decal_projection_t {
  real    basis[13];       ///< offset=0x00
  real    bounds[4];       ///< offset=0x34
  real    normal[3];       ///< offset=0x44
  real    field_50;        ///< offset=0x50
  int16_t projection;      ///< offset=0x54
  uint8_t sign;            ///< offset=0x56
  uint8_t pad_57;          ///< offset=0x57
  real    corner[4][2];    ///< offset=0x58
  real    field_78;        ///< offset=0x78
  real    field_7c;        ///< offset=0x7c
  real    field_80;        ///< offset=0x80
  real    field_84;        ///< offset=0x84
  real    field_88;        ///< offset=0x88
} decal_projection_t;
cs(decal_projection_t, 0x8c);
co(decal_projection_t, bounds, 0x34);
co(decal_projection_t, normal, 0x44);
co(decal_projection_t, projection, 0x54);
co(decal_projection_t, sign, 0x56);
co(decal_projection_t, corner, 0x58);
co(decal_projection_t, field_88, 0x88);

/// size=0x10  staged rasterizer vertex
typedef struct decal_staged_vertex_t {
  real    position[3];
  int16_t uv[2];
} decal_staged_vertex_t;
cs(decal_staged_vertex_t, 0x10);
co(decal_staged_vertex_t, uv, 0x0c);

typedef struct decal_cached_quad_t {
  decal_staged_vertex_t vertices[4];
} decal_cached_quad_t;
cs(decal_cached_quad_t, 0x40);

/// size=0x10  per-type table at 0x269d80, indexed by decal tag type 0..3
typedef struct decal_type_parameters_t {
  real    field_00;   ///< offset=0x00  conformal-angle scale
  real    field_04;   ///< offset=0x04  deviant-angle scale
  real    field_08;   ///< offset=0x08  wrap-sphere radius scale
  uint8_t field_0c;   ///< offset=0x0c
  uint8_t pad_0d[3];  ///< offset=0x0d
} decal_type_parameters_t;
cs(decal_type_parameters_t, 0x10);
co(decal_type_parameters_t, field_04, 0x04);
co(decal_type_parameters_t, field_08, 0x08);
co(decal_type_parameters_t, field_0c, 0x0c);

/* -------------------------------------------------------------------------
 * tag_reference -- 0x10-byte element of a tag-reference tag block.
 *
 * Name: the engine's own diagnostic string names the accessor,
 * "tag_reference_set() is not supported with a cache file active"
 * (read out of cachebeta.xbe).
 * Size: 0x10 is the element-size argument at every tag_block_get_element call
 * over such a block in game_engine.c (12 sites, e.g. :1249, :1251, :1253).
 * ------------------------------------------------------------------------- */
typedef struct {
  char    pad_00[0xc];  /* +0x00  group tag / name / name length: never observed accessed here */
  int32_t tag_index;    /* +0x0c  datum index handed to the tag-load helper at
                         *        .text:0013DDA0 (game_engine.c:1178-1183) and
                         *        compared against tag indices (:1255, :9175) */
} tag_reference;
cs(tag_reference, 0x10);
co(tag_reference, tag_index, 0x0c);

/// size=0x3C0
/* Scenario overlay for the structure-BSP tag block used by runtime decals. */
typedef struct {
  char pad_00[0x3b4];
  tag_block structure_bsps;
} scenario_structure_bsps_t;
cs(scenario_structure_bsps_t, 0x3c0);
co(scenario_structure_bsps_t, structure_bsps, 0x3b4);

/* -------------------------------------------------------------------------
 * netgame_flag -- 0x94-byte element of the scenario netgame-flags tag block
 * at scenario + 0x378.
 *
 * Name: kb.json already names the readers of this block
 * (find_netgame_flag / find_netgame_flags / game_engine_validate_map_netgame_flags).
 * Size: 0x94 is the element-size argument at every tag_block_get_element call
 * over scenario+0x378 (game_engine.c:66, :1821, :1827, :1859, :4759, :4773).
 * ------------------------------------------------------------------------- */
typedef struct {
  float   position_x;   /* +0x00  world point fed to the LOS/collision test as a
                         *        candidate position (game_engine.c:4789-4791) */
  float   position_y;   /* +0x04 */
  float   position_z;   /* +0x08 */
  float   facing;       /* +0x0c  yaw: added to a camera angle and differenced
                         *        against another flag's (game_engine.c:4849) */
  int16_t type;         /* +0x10  flag type, compared against the game-type code
                         *        (0=ctf, 2, 3, 7, 8=hill; :1822, :7974, :8740) */
  int16_t team_index;   /* +0x12  named by the engine's own error strings --
                         *        "NETGAME MAP FAILURE: duplicate ctf flag
                         *        [team %d]" formats exactly this field
                         *        (game_engine.c:1830 via :5271) */
  char    pad_14[0x80]; /* +0x14  never observed accessed */
} netgame_flag;
cs(netgame_flag, 0x94);
co(netgame_flag, position_x, 0x00);
co(netgame_flag, position_y, 0x04);
co(netgame_flag, position_z, 0x08);
co(netgame_flag, facing,     0x0c);
co(netgame_flag, type,       0x10);
co(netgame_flag, team_index, 0x12);

/* -------------------------------------------------------------------------
 * object_placement_data -- descriptor filled by object_placement_data_new and
 * consumed by object_new.
 *
 * Name: from the engine's own constructor, object_placement_data_new.
 * Size: 0x88 -- the initializer at .text:0013FC20 writes 0x88 bytes, and every
 * caller in this TU declares the buffer as [0x88].
 * ------------------------------------------------------------------------- */
typedef struct {
  char  pad_00[0x18];   /* +0x00  never observed accessed */
  float position_x;     /* +0x18  object_new copies this to object_data+0x0C
                         *        (MOV ECX,[ESI+0x18] in the object_new
                         *        disassembly), i.e. the spawn position. Some
                         *        callers copy the three words with int casts
                         *        (bit copies), which is why the float type is
                         *        taken from object_new, not from the casts. */
  float position_y;     /* +0x1c */
  float position_z;     /* +0x20 */
  char  pad_24[0x10];   /* +0x24  never observed accessed */
  vector3_t forward;    /* +0x34  object_placement_data_new default {1,0,0} */
  vector3_t up;         /* +0x40  object_placement_data_new default {0,0,1} */
  char  pad_4c[0x3c];   /* +0x4c */
} object_placement_data;
cs(object_placement_data, 0x88);
co(object_placement_data, position_x, 0x18);
co(object_placement_data, position_y, 0x1c);
co(object_placement_data, position_z, 0x20);
co(object_placement_data, forward,    0x34);
co(object_placement_data, up,         0x40);
/* -------------------------------------------------------------------------
 * game_globals_multiplayer_element -- 0xa0-byte element of the tag block at
 * game_globals + 0x164 (the multiplayer information block).
 *
 * The name is locational, not from a string: the binary has no symbol naming
 * this element type, so it describes where the block lives rather than
 * claiming what the element means.
 * Size: 0xa0 is the element-size argument at every tag_block_get_element call
 * over game_globals+0x164 (game_engine.c:953, :969, :1245, :3034, :8049).
 * ------------------------------------------------------------------------- */
typedef struct {
  char    pad_00[0xc];          /* +0x00  never observed accessed */
  int32_t flag_definition_index; /* +0x0c  the value kb.json's
                                  *        get_flag_definition_index returns
                                  *        (game_engine.c:954) */
  char    pad_10[0x28];         /* +0x10  never observed accessed */
  int32_t field_38;             /* +0x38  read as a shader tag index
                                 *        (game_engine.c:8052) */
  char    pad_3c[0x1c];         /* +0x3c  never observed accessed */
  int32_t ball_definition_index; /* +0x58  the value kb.json's
                                  *        get_ball_definition_index returns
                                  *        (game_engine.c:970) */
  char    pad_5c[0x44];         /* +0x5c  never observed accessed */
} game_globals_multiplayer_element;
cs(game_globals_multiplayer_element, 0xa0);
co(game_globals_multiplayer_element, flag_definition_index, 0x0c);
co(game_globals_multiplayer_element, field_38,              0x38);
co(game_globals_multiplayer_element, ball_definition_index, 0x58);

/* -------------------------------------------------------------------------
 * draw_string_emit_proc — per-glyph blitter passed into the draw-string
 * clipping loop (FUN_0019c1b0, text/draw_string.c).
 *
 * Ten cdecl arguments; ADD ESP,0x28 after CALL [EBP+8] @0019c3a0 fixes the
 * count, and the two concrete implementations in the same translation unit
 * (FUN_0019b3c0 / FUN_0019b430) fix the widths: slots 5/6 are the clipped
 * destination and 9/10 the clipped extent, all int16_t; slots 7/8 are the
 * source-rectangle offsets produced by the clip, int32_t.
 *
 * This lives here rather than in the .c because kb.json declarations are
 * emitted into build/generated/decl.h and thunks.c, and the generator cannot
 * parse an inline function-pointer parameter -- it needs a plain type name.
 * ------------------------------------------------------------------------- */
typedef void (*draw_string_emit_proc)(void *state, void *font_table,
                                      void *glyph, int color, short dest_x,
                                      short dest_y, int src_x, int src_y,
                                      short width, short height);

/* Both selection sorts test the comparator result with TEST AL,AL -- one byte,
 * not a full dword (0x91d22 in FUN_00091cf0, 0x91d7c in FUN_00091d50). The
 * return type must therefore be byte-wide: declaring `int` makes clang emit
 * TEST EAX,EAX, which also sees whatever the callee left in the upper 24 bits
 * of EAX and flips the "is greater" decision, silently reordering the sorted
 * array. */
typedef bool (*profile_sort16_compare_proc)(uint16_t a, uint16_t b);
typedef bool (*profile_sort32_compare_proc)(int32_t a, int32_t b);

/* -------------------------------------------------------------------------
 * collision_test_result -- 0x50-byte record filled by the world collision
 * entry points in physics/collision_bsp.c (FUN_0014e7d0, FUN_0014e940).
 *
 * Widths are taken from the store instructions at 0x14e7f9..0x14e872 and
 * 0x14e8b9..0x14e8ef: +0x00, +0x08, +0x10, +0x34 and +0x4e are 16-bit stores
 * (MOV word ptr), +0x4c/+0x4d are byte stores, everything else is a dword.
 *
 * +0x38..+0x41 are not touched by those two entry points but ARE written by
 * FUN_0014dce0 (0x14dce0), which fills the same record through a raw char*:
 * +0x38 dword object handle, +0x3c/+0x3e/+0x40 16-bit indices (all three set
 * to -1 on the model path at 0x14df19/0x14df20/0x14df53, and copied from the
 * FUN_0014cb00 output +0x02/+0x00/+0x04 on the bsp path).  Only +0x36 and
 * +0x42 remain unobserved.
 * ------------------------------------------------------------------------- */
typedef struct collision_test_result {
    int16_t field_00;      /* +0x00: -1 when no hit, 2 on a bsp surface hit */
    int16_t pad_02;        /* +0x02 */
    int32_t field_04;      /* +0x04: leading surface/leaf index */
    int16_t field_08;      /* +0x08: cluster index for field_04 (MOVSX) */
    int16_t pad_0a;        /* +0x0a */
    int32_t field_0c;      /* +0x0c: trailing leaf index */
    int16_t field_10;      /* +0x10: cluster index for field_0c (MOVSX) */
    int16_t pad_12;        /* +0x12 */
    float   t;             /* +0x14: hit fraction along the sweep */
    float   position[3];   /* +0x18: point + t * delta */
    float   normal[3];     /* +0x24 */
    float   field_30;      /* +0x30 */
    int16_t field_34;      /* +0x34 */
    int16_t pad_36;        /* +0x36 */
    int32_t field_38;      /* +0x38: object handle (FUN_0014dce0) */
    int16_t field_3c;      /* +0x3c */
    int16_t field_3e;      /* +0x3e */
    int16_t field_40;      /* +0x40 */
    int16_t pad_42;        /* +0x42 */
    int32_t field_44;      /* +0x44 */
    int32_t field_48;      /* +0x48 */
    char    field_4c;      /* +0x4c */
    char    field_4d;      /* +0x4d */
    int16_t field_4e;      /* +0x4e */
} collision_test_result;
cs(collision_test_result, 0x50);
co(collision_test_result, t,        0x14);
co(collision_test_result, position, 0x18);
co(collision_test_result, normal,   0x24);
co(collision_test_result, field_38, 0x38);
co(collision_test_result, field_3c, 0x3c);
co(collision_test_result, field_3e, 0x3e);
co(collision_test_result, field_40, 0x40);
co(collision_test_result, field_44, 0x44);
co(collision_test_result, field_4e, 0x4e);

/* CRT qsort/_shortsort comparator: two cdecl record pointers, int result. */
typedef int(__cdecl *qsort_compar_proc)(const void *, const void *);

/* -------------------------------------------------------------------------
 * sound_cache_sound -- PARTIAL. Per-sound record managed by the Xbox hardware
 * sound cache (cache/xbox_sound_cache.c).
 *
 * Only +0x2c, +0x30 and +0x34 have been observed, all dword stores in
 * sound_cache_sound_new (0x1bdf41..0x1bdf4f). +0x30 is named from the assert
 * text "sound->cache_base_address==NULL" at 0x1bdf2a; +0x2c and +0x34 have no
 * naming evidence. Total size is UNKNOWN, so there is no cs() assert and
 * everything below +0x2c is unexamined rather than proven unused.
 * ------------------------------------------------------------------------- */
typedef struct sound_cache_sound {
    char  pad_00[0x2c];        /* +0x00: never observed accessed */
    int32_t field_2c;          /* +0x2c: set to -1 on new */
    void *cache_base_address;  /* +0x30: NULL while not resident */
    void *field_34;            /* +0x34: dword handed in by the creator */
} sound_cache_sound;
co(sound_cache_sound, field_2c,           0x2c);
co(sound_cache_sound, cache_base_address, 0x30);
co(sound_cache_sound, field_34,           0x34);

/* -------------------------------------------------------------------------
 * transport_address -- PARTIAL. Bungie.net transport-layer network address
 * (bungie_net/network/transport_address.c).
 *
 * Field names are taken verbatim from the assert text
 * "IPV4_ADDRESS_LENGTH == a->address_length" at 0x266060 / 0x266034
 * (transport_address_equivalent, 0x81a90).
 *
 * transport_address_equivalent compares the two records with
 * csmemcmp(a, b, max(a->address_length, b->address_length)) starting at
 * offset 0, so the address bytes occupy the front of the record; only the
 * first IPV4_ADDRESS_LENGTH (4) of them are ever compared at runtime. The
 * 16-byte span is inferred from address_length sitting at +0x10, not proven
 * as the declared address width. Total size is UNKNOWN, so there is no cs()
 * assert and everything past +0x14 is unexamined rather than proven unused.
 * ------------------------------------------------------------------------- */
typedef struct transport_address {
    uint8_t  address[0x10];   /* +0x00: address bytes, compared as a block */
    uint16_t address_length;  /* +0x10: asserted == IPV4_ADDRESS_LENGTH */
    uint16_t port;            /* +0x12: compared as a 16-bit value */
} transport_address;
co(transport_address, address_length, 0x10);
co(transport_address, port,           0x12);

/* transport_endpoint -- 8-byte Winsock endpoint (debug_malloc(8) at 0x82d70).
 *
 * type is named from the assert "ep->type == _transport_type_udp" (0x847ef).
 * type is signed: count_endpoints_in_set (0x82df0) does MOVSX EAX,byte [ESI+5].
 * socket / flags / status widths come from dword / byte / word operand sizes. */
#define _transport_type_udp 0x11
#define _transport_type_tcp 0x12

typedef struct transport_endpoint {
    int32_t socket;   ///< offset=0x00  INVALID_SOCKET = -1
    uint8_t flags;    ///< offset=0x04  bit0 connected, bit1 listening, bit3 in-set, bit4 nbio
    int8_t  type;     ///< offset=0x05  _transport_type_udp / _transport_type_tcp
    int16_t status;   ///< offset=0x06
} transport_endpoint;
cs(transport_endpoint, 8);
co(transport_endpoint, socket, 0x00);
co(transport_endpoint, flags,  0x04);
co(transport_endpoint, type,   0x05);
co(transport_endpoint, status, 0x06);

/* transport_endpoint_set -- 0x118-byte fd_set wrapper (debug_malloc(0x118)
 * in create_endpoint_set 0x82310). ep_array / max_endpoints are T1 from
 * asserts "set && set->ep_array" and "max_endpoints > 0". */
typedef struct transport_endpoint_set {
    int32_t fd_count;                 ///< offset=0x00
    int32_t fd_array[0x40];           ///< offset=0x04
    transport_endpoint **ep_array;    ///< offset=0x104
    int32_t max_endpoints;            ///< offset=0x108
    int32_t field_10c;                ///< offset=0x10c  high-water index, inited -1
    int32_t field_110;                ///< offset=0x110  rewind cursor
    int32_t field_114;                ///< offset=0x114  dirty; qsort in poll
} transport_endpoint_set;
cs(transport_endpoint_set, 0x118);
co(transport_endpoint_set, fd_count,      0x00);
co(transport_endpoint_set, fd_array,      0x04);
co(transport_endpoint_set, ep_array,      0x104);
co(transport_endpoint_set, max_endpoints, 0x108);
co(transport_endpoint_set, field_10c,     0x10c);
co(transport_endpoint_set, field_110,     0x110);
co(transport_endpoint_set, field_114,     0x114);

/* Connect-worker table at 0x3350a0, 64 slots of 8 bytes (walked to 0x3352a0
 * by endpoint_pool_cleanup). "thread" is T1 from the 0x82cf0 assert. */
typedef struct transport_connect_thread_slot {
    void    *thread;     ///< offset=0x00
    uint8_t  cleanup;    ///< offset=0x04
    uint8_t  pad_05[3];  ///< offset=0x05  never observed accessed
} transport_connect_thread_slot;
cs(transport_connect_thread_slot, 8);
co(transport_connect_thread_slot, thread,  0x00);
co(transport_connect_thread_slot, cleanup, 0x04);

/* Async connect request, debug_malloc(0x28) at 0x841b0. ep / thread are T1
 * from "input->ep" / "input->thread". Address blob is 6 dwords (REP MOVSD). */
typedef struct transport_connect_request {
    transport_endpoint *ep;       ///< offset=0x00
    uint32_t            address[6]; ///< offset=0x04
    void               *thread;   ///< offset=0x1c
    int                *mutex;    ///< offset=0x20
    uint8_t             cancelled; ///< offset=0x24
    uint8_t             pad_25[3]; ///< offset=0x25
} transport_connect_request;
cs(transport_connect_request, 0x28);
co(transport_connect_request, ep,        0x00);
co(transport_connect_request, address,   0x04);
co(transport_connect_request, thread,    0x1c);
co(transport_connect_request, mutex,     0x20);
co(transport_connect_request, cancelled, 0x24);

/* Callback handed to hs_object_iterate_names_containing (0xc9b10).  The three
 * call sites (0xc9b90 -> 0xc9990, 0xc9bb0 -> 0xc9a20, 0xca140 -> 0xca110) each
 * PUSH the routine's address as the single stack argument, and 0xc9b10 invokes
 * it with the 16-bit object-name index it just matched (PUSH EDI after
 * MOVSWL EAX,DI).  Named type rather than an inline function-pointer parameter
 * because the kb.json thunk generator cannot spell one. */
typedef void (*hs_object_name_iterator_t)(int16_t index);

/* Static (non-dynamic) geometry buffers consumed by rasterizer_draw (0x15e650)
 * and its rasterizer_draw_*_static_* variants in
 * rasterizer_xbox_draw_primitives.c.  Offsets read off the draw routines'
 * disassembly; both are passed by pointer and only read. */
typedef struct vertex_buffer {
    int16_t type;             ///< offset=0x00 rasterizer vertex type index
    uint16_t pad_02;          ///< offset=0x02
    int32_t count;            ///< offset=0x04 vertex count
    int32_t field_08;         ///< offset=0x08
    void   *vertices;         ///< offset=0x0c
    void   *hardware_format;  ///< offset=0x10 D3D vertex buffer
} vertex_buffer;
cs(vertex_buffer, 0x14);
co(vertex_buffer, type,            0x00);
co(vertex_buffer, count,           0x04);
co(vertex_buffer, field_08,        0x08);
co(vertex_buffer, vertices,        0x0c);
co(vertex_buffer, hardware_format, 0x10);

typedef struct triangle_buffer {
    int16_t type;             ///< offset=0x00
    uint16_t pad_02;          ///< offset=0x02
    int32_t field_04;         ///< offset=0x04
    int32_t field_08;         ///< offset=0x08
    void   *hardware_format;  ///< offset=0x0c D3D index buffer
} triangle_buffer;
cs(triangle_buffer, 0x10);
co(triangle_buffer, type,            0x00);
co(triangle_buffer, field_04,        0x04);
co(triangle_buffer, field_08,        0x08);
co(triangle_buffer, hardware_format, 0x0c);

/* Glow widget types (objects/widgets/glow.c; code split across
 * src/halo/objects/objects.c and src/halo/objects/widgets/glow.c, hence
 * shared here). Rendered from recovery/evidence/{object_marker,glow_particle,
 * glow_datum,glow_definition}.json; fix the artifact, not this copy. */
/// size=0x6C  (glow_datum marker stride: IMUL EAX,EAX,0x6c @0x133b69 in get_particle_world_position)
/// stride=0x6C  (IMUL EAX,EAX,0x6c @0x133b69; widget+idx*0x6c+0x44 in glow_render)
/// Recovered layout - evidence artifact: recovery/evidence/object_marker.json
/// matrix is a real_matrix4x3 (scale, forward, left, up, position) at +0x38; only forward/up/position are observed accessed here.
/// Offsets 0x00-0x3b are not accessed by glow code and stay padding until another consumer proves them.
/// evidence: get_particle_world_position @0x1339a0
/// evidence: glow_render @0x133520
/// evidence: PAL reference source/objects/objects.h struct object_marker {short node_index; real_matrix4x3 node_matrix; real_matrix4x3 matrix;} (names only)
typedef struct object_marker {
    uint8_t pad_00[60];       ///< offset=0x00  declared padding
    float matrix_forward[3];  ///< offset=0x3C  FMUL [EAX+0x44..0x4c] rel. glow_datum = marker+0x3c (cross product @0x133b7a-0x133bf0); name: PAL-2342 (T2) (real_matrix4x3.forward)
    uint8_t pad_48[12];       ///< offset=0x48  declared padding
    float matrix_up[3];       ///< offset=0x54  dword copy from EAX+0x5c = marker+0x54 @0x133b91 and cross product operands; name: PAL-2342 (T2) (real_matrix4x3.up)
    float matrix_position[3]; ///< offset=0x60  dword copy from EAX+0x68 = marker+0x60 @0x133b72; glow_trailing_particle_new copies widget+0x68; name: PAL-2342 (T2) (real_matrix4x3.position)
} object_marker;
cs(object_marker, 0x6C);
co(object_marker, pad_00, 0x00);
co(object_marker, matrix_forward, 0x3C);
co(object_marker, pad_48, 0x48);
co(object_marker, matrix_up, 0x54);
co(object_marker, matrix_position, 0x60);

/// size=0x64  (data_new("glow particles", 0x200, 0x64) @0x13377f in glow_initialize (0x133750))
/// Recovered layout - evidence artifact: recovery/evidence/glow_particle.json
/// Field names come from the PAL 2342 reference (T2) unless an evidence line cites a 2276 string; every offset and width is from 2276 disassembly.
/// color is PAL real_argb_color: alpha +0x0c, red +0x10, green +0x14, blue +0x18 (glow_normal_particle_update_color stores alpha=1.0 via MOV [EDI+0xc] and rgb via FSTP [EDI+0x10..0x18]).
/// evidence: glow_initialize @0x133750 (pool element size)
/// evidence: glow_trailing_particle_update_color @0x1330f0, glow_trailing_particle_update_size @0x133170, glow_trailing_particle_update_velocity @0x1331d0, glow_trailing_particle_update_position @0x133260 (trailing particle fields)
/// evidence: glow_normal_particle_update_color @0x133300 (color/fade), get_particle_world_position @0x1339a0 (marker index, t, position, angle, distance)
/// evidence: glow_delete @0x1330a0 (index, next), glow_update @0x1345b0 (flags, previous)
/// evidence: PAL reference /mnt/g/dev/halo-pal-2342 source/objects/widgets/glow.c struct glow_particle (names only; layout re-proven here)
typedef struct glow_particle {
    int16_t datum_salt;          ///< offset=0x00  standard data_t element prefix (pool from data_new @0x13377f); not accessed in the glow cluster
    int16_t parent_marker_index; ///< offset=0x02  MOV word [EDI+0x2],AX @0x133a2b stores PIN(marker_index,...); MOVSX EAX,word [EDI+0x2] @0x133c5e; name: PAL-2342 (T2)
    int32_t index;               ///< offset=0x04  datum index passed to datum_delete(glow particle pool) in glow_delete; name: PAL-2342 (T2)
    float initial_angle;         ///< offset=0x08  FADD [EDI+0x8] @0x134005 (angle = rate*t + initial_angle); name: PAL-2342 (T2)
    float color_alpha;           ///< offset=0x0C  MOV dword [EDI+0xc],1.0f @0x133391; name: PAL-2342 (T2)
    float color_red;             ///< offset=0x10  FSTP [EDI+0x10] @0x133363; name: PAL-2342 (T2)
    float color_green;           ///< offset=0x14  FSTP [EDI+0x14] @0x13337a; name: PAL-2342 (T2)
    float color_blue;            ///< offset=0x18  FSTP [EDI+0x18] @0x133394; name: PAL-2342 (T2)
    float distance_to_object;    ///< offset=0x1C  FMUL [EDI+0x1c] @0x13401f (orbit radius); name: PAL-2342 (T2)
    float initial_size;          ///< offset=0x20  FMUL [ESI+0x20] @0x1331c1; name: PAL-2342 (T2)
    float present_size;          ///< offset=0x24  FSTP [ESI+0x24] @0x1331c4; name: PAL-2342 (T2)
    float t;                     ///< offset=0x28  FCOMP [EDI+0x28] @0x1339c7 against marker times; spline parameter (assert "t>= t0 && t <= t3" @0x29aae4); name: PAL-2342 (T2)
    float position[3];           ///< offset=0x2C  FADD/FSTP [ESI+0x2c/0x30/0x34] @0x13327d-0x133298; spline output pointer LEA ESI,[EDI+0x2c] @0x133f8d; name: PAL-2342 (T2)
    float initial_velocity[3];   ///< offset=0x38  FMUL [ESI+0x38/0x3c/0x40] @0x133226-0x133234; name: PAL-2342 (T2)
    float present_velocity[3];   ///< offset=0x44  FSTP [ESI+0x44/0x48/0x4c] @0x133229-0x133237; FMUL [ESI+0x44] @0x133277; name: PAL-2342 (T2)
    int16_t ticks_in_existence;  ///< offset=0x50  MOVSX EDX,word [ESI+0x50] @0x133110; name: PAL-2342 (T2)
    int16_t lifetime;            ///< offset=0x52  MOVSX EAX,word [ESI+0x52] @0x133114; name: PAL-2342 (T2)
    uint32_t flags;              ///< offset=0x54  bit tests (&2 trailing, &1 reverse) in glow_update / glow_normal_particle_update_position, dword OR-store of bit 0; name: PAL-2342 (T2)
    float fade;                  ///< offset=0x58  FST [ESI+0x58] @0x133130; FSTP [EDI+0x58] @0x133428; name: PAL-2342 (T2)
    struct glow_particle *next;  ///< offset=0x5C  next-link read before datum_delete in glow_delete @0x1330c1; list walk in glow_render; name: PAL-2342 (T2)
    struct glow_particle *previous; ///< offset=0x60  previous-link read when unlinking an expired trailing particle in glow_update; name: PAL-2342 (T2)
} glow_particle;
cs(glow_particle, 0x64);
co(glow_particle, datum_salt, 0x00);
co(glow_particle, parent_marker_index, 0x02);
co(glow_particle, index, 0x04);
co(glow_particle, initial_angle, 0x08);
co(glow_particle, color_alpha, 0x0C);
co(glow_particle, color_red, 0x10);
co(glow_particle, color_green, 0x14);
co(glow_particle, color_blue, 0x18);
co(glow_particle, distance_to_object, 0x1C);
co(glow_particle, initial_size, 0x20);
co(glow_particle, present_size, 0x24);
co(glow_particle, t, 0x28);
co(glow_particle, position, 0x2C);
co(glow_particle, initial_velocity, 0x38);
co(glow_particle, present_velocity, 0x44);
co(glow_particle, ticks_in_existence, 0x50);
co(glow_particle, lifetime, 0x52);
co(glow_particle, flags, 0x54);
co(glow_particle, fade, 0x58);
co(glow_particle, next, 0x5C);
co(glow_particle, previous, 0x60);

/// size=0x25C  (data_new("glow", 8, 0x25c) @0x133759 in glow_initialize (0x133750))
/// Recovered layout - evidence artifact: recovery/evidence/glow_datum.json
/// 0x08..0x223 is object_marker markers[5] (5 * 0x6c, see object_marker.json). The schema has no struct-typed field kind, so the region is recorded as one gap here and the placed struct embeds object_marker markers[5]; the co() on markers is the check.
/// marker count 5 = (0x224 - 0x08) / 0x6c; PAL MAXIMUM_GLOW_MARKERS = 5.
/// evidence: glow_initialize @0x133750
/// evidence: get_particle_world_position @0x1339a0
/// evidence: glow_delete @0x1330a0
/// evidence: glow_render @0x133520
/// evidence: glow_trailing_particle_update_color/..._update_size/glow_trailing_particle_update_velocity/..._update_position/glow_normal_particle_update_color (definition_index +0x224)
/// evidence: PAL reference source/objects/widgets/glow.c struct glow_datum (names only; layout re-proven here)
/// evidence: glow_update @0x1345b0 (tail_particle +0x254: remove @0x1349d0, append @0x134a5c/0x134a70)
typedef struct glow_datum {
    int16_t datum_salt;                                     ///< offset=0x00  standard data_t element prefix (pool from data_new @0x133759)
    uint8_t pad_02[2];                                      ///< offset=0x02  declared padding
    int16_t number_of_markers;                              ///< offset=0x04  assert "glow->number_of_markers > 1" @0x29ab88 (glow.c:0x43b); MOVSX EDX,word [ESI+0x4] @0x1339ad
    uint8_t pad_06[2];                                      ///< offset=0x06  declared padding
    object_marker markers[5];                                    ///< offset=0x08  5 * 0x6c (see recovery/evidence/object_marker.json)
    int32_t definition_index;                               ///< offset=0x224  tag_get('glw!', [EAX+0x224]) @0x1330f4; name: PAL-2342 (T2)
    uint8_t pad_228[2];                                     ///< offset=0x228  declared padding
    int16_t marker_order[5];                                ///< offset=0x22A  MOVSX EAX,word [ECX] with ECX = widget+first*2+0x22a @0x133b2d/0x133b66; name: PAL-2342 (T2)
    float total_time;                                       ///< offset=0x234  FDIV [EBX+0x234] @0x133406; name: PAL-2342 (T2)
    float marker_time_index[5];                             ///< offset=0x238  FLD [ESI+ECX*4+0x238] @0x1339c0; name: PAL-2342 (T2)
    uint16_t number_of_particles;                           ///< offset=0x24C  zero-extended word load (XOR ECX,ECX; MOV CX,[..+0x24c]) @0x133554 in glow_render; name: PAL-2342 (T2)
    uint8_t pad_24e[2];                                     ///< offset=0x24E  declared padding
    glow_particle *head_particle;                           ///< offset=0x250  list head read in glow_delete/glow_render (widget+0x250); name: PAL-2342 (T2)
    glow_particle *tail_particle;                           ///< offset=0x254  list tail: MOV [EBX+0x254],EAX @0x1349d0 when removing the last particle in glow_update (also @0x1332dc in the uncalled glow_trailing_particle_age copy); read @0x134a5c/0x134a70 to append; name: PAL-2342 (T2)
    int16_t accumulated_trailing_particle_generation_ticks; ///< offset=0x258  word add of game_time_get() into widget+0x258 in glow_update; name: PAL-2342 (T2)
    uint8_t pad_25a[2];                                     ///< offset=0x25A  declared padding
} glow_datum;
cs(glow_datum, 0x25C);
co(glow_datum, datum_salt, 0x00);
co(glow_datum, pad_02, 0x02);
co(glow_datum, number_of_markers, 0x04);
co(glow_datum, pad_06, 0x06);
co(glow_datum, markers, 0x08);
co(glow_datum, definition_index, 0x224);
co(glow_datum, pad_228, 0x228);
co(glow_datum, marker_order, 0x22A);
co(glow_datum, total_time, 0x234);
co(glow_datum, marker_time_index, 0x238);
co(glow_datum, number_of_particles, 0x24C);
co(glow_datum, pad_24e, 0x24E);
co(glow_datum, head_particle, 0x250);
co(glow_datum, tail_particle, 0x254);
co(glow_datum, accumulated_trailing_particle_generation_ticks, 0x258);
co(glow_datum, pad_25a, 0x25A);

/// Recovered layout - evidence artifact: recovery/evidence/glow_definition.json
/// Partial: only offsets proven by the listed functions are typed; the rest stay padding for structize split to refine.
/// color bounds are PAL real_argb_color (alpha first); the alpha slots +0xb4/+0xc4 are not observed accessed and stay padding.
/// size is omitted: tag definitions have no pool/allocation size in this code.
/// evidence: glow_trailing_particle_update_color @0x1330f0, glow_trailing_particle_update_size @0x133170, glow_trailing_particle_update_velocity @0x1331d0 (flags bits 3/4/5)
/// evidence: glow_normal_particle_update_color @0x133300 (color attachment, color bounds, rate, edge fade, flags bit 0)
/// evidence: PAL reference source/objects/widgets/glow.c struct glow_definition (names only; size 0x154 there, unproven here)
typedef struct glow_definition {
    uint8_t pad_00[40];              ///< offset=0x00  declared padding
    uint32_t flags;                  ///< offset=0x28  TEST byte [EAX+0x28],0x8/0x10/0x20 @0x13310b/0x13318b/0x1331ee, TEST byte [ESI+0x28],0x1 @0x1333a0; name: PAL-2342 (T2)
    uint8_t pad_2c[132];             ///< offset=0x2C  declared padding
    uint16_t color_attachment_index; ///< offset=0xB0  XOR EAX,EAX; MOV AX,[ESI+0xb0]; CMP AX,0xffff @0x133318-0x133324 (zero-extended); name: PAL-2342 (T2)
    uint8_t pad_b2[6];               ///< offset=0xB2  declared padding
    float color_lower_bound_rgb[3];  ///< offset=0xB8  FSUB/FADD [ESI+0xb8/0xbc/0xc0] @0x133355-0x13338b; name: PAL-2342 (T2) (color_lower_bound.red..blue)
    uint8_t pad_c4[4];               ///< offset=0xC4  declared padding
    float color_upper_bound_rgb[3];  ///< offset=0xC8  FLD [ESI+0xc8/0xcc/0xd0] @0x13334a-0x13337d; name: PAL-2342 (T2) (color_upper_bound.red..blue)
    uint8_t pad_d4[32];              ///< offset=0xD4  declared padding
    float color_rate_of_change;      ///< offset=0xF4  FMUL [ESI+0xf4] @0x1333b2; name: PAL-2342 (T2)
    float percentage_edge_fade;      ///< offset=0xF8  FLD [ESI+0xf8]; FMUL 0.5 @0x13340c; name: PAL-2342 (T2)
} glow_definition;
/* size unproven - largest observed access at 0xFC */
co(glow_definition, pad_00, 0x00);
co(glow_definition, flags, 0x28);
co(glow_definition, pad_2c, 0x2C);
co(glow_definition, color_attachment_index, 0xB0);
co(glow_definition, pad_b2, 0xB2);
co(glow_definition, color_lower_bound_rgb, 0xB8);
co(glow_definition, pad_c4, 0xC4);
co(glow_definition, color_upper_bound_rgb, 0xC8);
co(glow_definition, pad_d4, 0xD4);
co(glow_definition, color_rate_of_change, 0xF4);
co(glow_definition, percentage_edge_fade, 0xF8);

/* glow_definition.flags (+0x28) bits.  Names: PAL-2342 glow.c
 * glow_definition_flags (T2); each bit is matched to the 2276 site that tests
 * it with the behavior PAL gives it. */
enum {
  _glow_definition_modify_particle_color_bit = 0,             /* glow_normal_particle_update_color, glow_normal_particle_new */
  _glow_definition_particles_move_backwards_bit = 1,          /* glow_particles_initialize: sets particle moving_backwards */
  _glow_definition_particles_move_in_both_directions_bit = 2, /* glow_particles_initialize: alternates moving_backwards */
  _glow_definition_trailing_particles_fade_over_time_bit = 3, /* glow_trailing_particle_update_color */
  _glow_definition_trailing_particles_shrink_over_time_bit = 4, /* glow_trailing_particle_update_size */
  _glow_definition_trailing_particles_slow_over_time_bit = 5  /* glow_trailing_particle_update_velocity */
};

/* glow_particle.flags (+0x54) bits.  Names: PAL-2342 glow.c
 * glow_particle_flags (T2). */
enum {
  _glow_particle_moving_backwards_bit = 0, /* glow_normal_particle_update_position steps t down */
  _glow_particle_trailing_bit = 1          /* set by glow_trailing_particle_new; glow_update tests it (0x134893) */
};

/* ---- RAD Bink (Xbox, 2001) --------------------------------------------------
 * Layouts and names: PAL-2342 libs/binkxbox/{bink.h,binkio.h,radcb.h} (T2),
 * the public RAD SDK names for the public fields.  Every offset named below was
 * re-checked against a 2276 access in bink.obj (BinkClose 0x231220, BinkWait
 * 0x22f1e0, BinkGetSummary 0x22f480, BinkGetRealtime 0x22f650, BinkNextFrame
 * 0x230ff0, GotoFrame 0x2302d0, dosilence 0x22e000, endframe 0x22f180).
 * pad_ spans were not observed in 2276 code. */
struct BINK;
struct BINKIO;
struct BINKSND;
struct radcb_handler;

/* RADCB callback record embedded in BINKIO and BINK (0x18 bytes). */
struct radcb_callback;
typedef unsigned long(__stdcall *radcb_callback_proc)(struct radcb_callback *callback, unsigned long iteration);
typedef struct radcb_callback {
  struct radcb_callback *next;
  void *mutex;
  struct radcb_callback *priority_next;
  unsigned long priority;
  radcb_callback_proc get_priority;
  radcb_callback_proc dispatch;
} radcb_callback;
cs(radcb_callback, 0x18);

typedef long(__stdcall *bink_io_open_proc)(struct BINKIO *io, const char *name, unsigned long flags);
typedef unsigned long(__stdcall *bink_io_read_header_proc)(struct BINKIO *io, long offset, void *destination, unsigned long size);
typedef unsigned long(__stdcall *bink_io_read_frame_proc)(struct BINKIO *io, unsigned long frame, long offset, void *destination, unsigned long size);
typedef unsigned long(__stdcall *bink_io_buffer_size_proc)(struct BINKIO *io, unsigned long size);
typedef void(__stdcall *bink_io_set_info_proc)(struct BINKIO *io, void *buffer, unsigned long size, unsigned long file_size, unsigned long simulate);
typedef unsigned long(__stdcall *bink_io_idle_proc)(struct BINKIO *io);
typedef void(__stdcall *bink_io_callback_proc)(struct BINKIO *io);
typedef long(__stdcall *bink_io_try_suspend_proc)(struct BINKIO *io);

/* The status block is written by the RADCB IO thread; the RAD SDK declares it
 * volatile, and BinkNextFrame's back-to-back Working=1/Working=0 stores
 * (0x231006/0x231010) are only emitted for a volatile field. */
typedef struct BINKIO {
  bink_io_read_header_proc ReadHeader;
  bink_io_read_frame_proc ReadFrame;
  bink_io_buffer_size_proc GetBufferSize;
  bink_io_set_info_proc SetInfo;
  bink_io_idle_proc Idle;
  bink_io_callback_proc Close;
  struct BINK *bink;
  volatile unsigned long ReadError;
  volatile unsigned long DoingARead;
  volatile unsigned long BytesRead;
  volatile unsigned long Working;
  volatile unsigned long TotalTime;
  volatile unsigned long ForegroundTime;
  volatile unsigned long IdleTime;
  volatile unsigned long ThreadTime;
  volatile unsigned long BufSize;
  volatile unsigned long BufHighUsed;
  volatile unsigned long CurBufSize;
  volatile unsigned long CurBufUsed;
  unsigned char pad_4c[0x80]; /* backend iodata */
  bink_io_callback_proc suspend_callback;
  bink_io_try_suspend_proc try_suspend_callback;
  bink_io_callback_proc resume_callback;
  bink_io_callback_proc idle_on_callback;
  radcb_callback callback;
  unsigned char pad_f4[8];
} BINKIO;
cs(BINKIO, 0xfc);
co(BINKIO, Idle, 0x10);
co(BINKIO, Close, 0x14);
co(BINKIO, bink, 0x18);
co(BINKIO, BytesRead, 0x24);
co(BINKIO, Working, 0x28);
co(BINKIO, BufSize, 0x3c);
co(BINKIO, CurBufUsed, 0x48);
co(BINKIO, suspend_callback, 0xcc);
co(BINKIO, callback, 0xdc);

typedef long(__stdcall *bink_sound_ready_proc)(struct BINKSND *sound);
typedef long(__stdcall *bink_sound_lock_proc)(struct BINKSND *sound, unsigned char **destination, unsigned long *bytes);
typedef long(__stdcall *bink_sound_unlock_proc)(struct BINKSND *sound, unsigned long bytes);
typedef void(__stdcall *bink_sound_volume_proc)(struct BINKSND *sound, long volume);
typedef void(__stdcall *bink_sound_pan_proc)(struct BINKSND *sound, long pan);
typedef long(__stdcall *bink_sound_pause_proc)(struct BINKSND *sound, long pause);
typedef long(__stdcall *bink_sound_on_off_proc)(struct BINKSND *sound, long on);
typedef void(__stdcall *bink_sound_close_proc)(struct BINKSND *sound);
typedef void(__stdcall *bink_sound_mix_bins_proc)(struct BINKSND *sound, unsigned long bins);
typedef long(__stdcall *bink_sound_open_proc)(struct BINKSND *sound, unsigned long frequency, long bits, long channels, unsigned long flags, struct BINK *bink);
typedef bink_sound_open_proc(__stdcall *bink_sound_system_open_proc)(unsigned long parameter);

typedef struct BINKSND {
  bink_sound_ready_proc Ready;
  bink_sound_lock_proc Lock;
  bink_sound_unlock_proc Unlock;
  bink_sound_volume_proc Volume;
  bink_sound_pan_proc Pan;
  bink_sound_pause_proc Pause;
  bink_sound_on_off_proc SetOnOff;
  bink_sound_close_proc Close;
  bink_sound_mix_bins_proc MixBins;
  unsigned long BestSizeIn16;
  unsigned long SoundDroppedOut;
  long OnOff;
  unsigned long Latency;
  unsigned long VideoScale;
  unsigned long Frequency;
  long Bits;
  long Channels;
  unsigned char pad_44[0x80]; /* DirectSound backend state */
} BINKSND;
cs(BINKSND, 0xc4);
co(BINKSND, Close, 0x1c);
co(BINKSND, SoundDroppedOut, 0x28);
co(BINKSND, VideoScale, 0x34);
co(BINKSND, Channels, 0x40);

typedef struct BINKRECT {
  long Left;
  long Top;
  long Width;
  long Height;
} BINKRECT;
cs(BINKRECT, 0x10);

typedef struct BINK {
  unsigned long Width;
  unsigned long Height;
  unsigned long Frames;
  unsigned long FrameNum;
  unsigned long LastFrameNum;
  unsigned long FrameRate;
  unsigned long FrameRateDiv;
  unsigned long ReadError;
  unsigned long OpenFlags;
  unsigned long BinkType;
  unsigned long Size;
  unsigned long FrameSize;
  unsigned long SndSize;
  BINKRECT FrameRects[8];
  long NumRects;
  unsigned long field_b8;  /* plane-pair index, XORed with 1 per decode (0x22efd9) */
  void *field_bc[2];       /* decode planes indexed by field_b8; [0] is the
                            * BinkOpen plane allocation base BinkClose frees */
  void *field_c4[2];       /* alpha planes (OpenFlags 0x100000), indexed as field_bc */
  long dirty_width;
  long dirty_height;
  unsigned long field_d4;  /* ((Width+1)/2+7)&~7; dirty_width is twice this */
  unsigned long field_d8;  /* ((Height+1)/2+7)&~7; dirty_height is twice this */
  unsigned char *dirty_mask;
  long dirty_pitch;
  unsigned long field_e4;  /* dirty_mask length; BinkOpen stores a 0 terminator there */
  unsigned long field_e8;  /* header dword 3; compframe allocation size */
  unsigned long InternalFrames;
  long NumTracks;
  unsigned long Highest1SecRate;
  unsigned long Highest1SecFrame;
  long Paused;
  unsigned char pad_100[4];
  unsigned char *compframe;
  void *preloadptr;
  unsigned long *frameoffsets;
  BINKIO io;
  void *iobuffer;
  unsigned long iosize;
  unsigned long field_214; /* header Width before copy-mode doubling */
  unsigned long field_218; /* header Height before copy-mode doubling */
  long trackindex;
  unsigned long *tracksizes;
  unsigned long *tracktypes;
  unsigned long *trackIDs;
  unsigned char pad_22c[4];
  unsigned long playedframes;
  unsigned long firstframetime;
  unsigned long field_238; /* BinkDoFrame start timestamp */
  unsigned long startblittime;
  unsigned long starttime;
  unsigned long startframe;
  unsigned long resynctime;
  unsigned long longestframetime;
  unsigned long slowestframetime;
  unsigned long slowestframe;
  unsigned long slowest2frametime;
  unsigned long slowest2frame;
  long SoundOn;
  long VideoOn;
  unsigned long totalmem;
  unsigned long timevdecomp;
  unsigned long timeadecomp;
  unsigned long timeblit;
  unsigned long timeopen;
  unsigned long fileframerate;
  unsigned long fileframeratediv;
  unsigned long runtimeframes;
  unsigned long runtimemoveamt;
  unsigned long *rtframetimes;
  unsigned long *rtadecomptimes;
  unsigned long *rtvdecomptimes;
  unsigned long *rtblittimes;
  unsigned long *rtreadtimes;
  unsigned long *rtidlereadtimes;
  unsigned long *rtthreadreadtimes;
  unsigned long lastblitflags;
  unsigned long lastdecompframe;
  unsigned long sndbufsize;
  unsigned char *sndbuf;
  unsigned char *sndend;
  unsigned char *sndwritepos;
  unsigned char *sndreadpos;
  void *sndcomp;
  unsigned long sndamt;
  long sndconvert8;
  BINKSND sound;
  unsigned long skippedlastblit;
  unsigned long skippedblits;
  unsigned long soundskips;
  long sndendframe;
  unsigned long sndprime;
  unsigned char pad_3a8[4];
  unsigned long field_3ac[9]; /* sizes from FUN_00236210, then pushmalloc'd buffers */
  unsigned long field_3d0;    /* consecutive late BinkCopyToBuffer count */
  unsigned long big_sound_skip_adj;
  unsigned long big_sound_skip_reduce;
  unsigned char pad_3dc[0xc];
  radcb_callback sound_callback;
  unsigned char pad_400[8];
} BINK;
cs(BINK, 0x408);
co(BINK, OpenFlags, 0x20);
co(BINK, NumRects, 0xb4);
co(BINK, field_b8, 0xb8);
co(BINK, field_bc, 0xbc);
co(BINK, field_c4, 0xc4);
co(BINK, field_d4, 0xd4);
co(BINK, dirty_mask, 0xdc);
co(BINK, field_e4, 0xe4);
co(BINK, field_e8, 0xe8);
co(BINK, InternalFrames, 0xec);
co(BINK, Highest1SecFrame, 0xf8);
co(BINK, compframe, 0x104);
co(BINK, field_214, 0x214);
co(BINK, tracksizes, 0x220);
co(BINK, trackIDs, 0x228);
co(BINK, field_238, 0x238);
co(BINK, resynctime, 0x248);
co(BINK, timeopen, 0x278);
co(BINK, runtimemoveamt, 0x288);
co(BINK, sndprime, 0x3a4);
co(BINK, field_3ac, 0x3ac);
co(BINK, field_3d0, 0x3d0);
co(BINK, Paused, 0xfc);
co(BINK, preloadptr, 0x108);
co(BINK, io, 0x110);
co(BINK, iobuffer, 0x20c);
co(BINK, trackindex, 0x21c);
co(BINK, playedframes, 0x230);
co(BINK, startblittime, 0x23c);
co(BINK, longestframetime, 0x24c);
co(BINK, SoundOn, 0x260);
co(BINK, totalmem, 0x268);
co(BINK, timeblit, 0x274);
co(BINK, runtimeframes, 0x284);
co(BINK, rtframetimes, 0x28c);
co(BINK, rtthreadreadtimes, 0x2a4);
co(BINK, sndbuf, 0x2b4);
co(BINK, sndcomp, 0x2c4);
co(BINK, sound, 0x2d0);
co(BINK, skippedblits, 0x398);
co(BINK, sndendframe, 0x3a0);
co(BINK, big_sound_skip_adj, 0x3d4);
co(BINK, sound_callback, 0x3e8);

/* BinkGetSummary output; BinkGetSummary clears exactly 0x1f dwords. */
typedef struct BINKSUMMARY {
  unsigned long Width;
  unsigned long Height;
  unsigned long TotalTime;
  unsigned long FileFrameRate;
  unsigned long FileFrameRateDiv;
  unsigned long FrameRate;
  unsigned long FrameRateDiv;
  unsigned long TotalOpenTime;
  unsigned long TotalFrames;
  unsigned long TotalPlayedFrames;
  unsigned long SkippedFrames;
  unsigned long SkippedBlits;
  unsigned long SoundSkips;
  unsigned long TotalBlitTime;
  unsigned long TotalReadTime;
  unsigned long TotalVideoDecompTime;
  unsigned long TotalAudioDecompTime;
  unsigned long TotalIdleReadTime;
  unsigned long TotalBackReadTime;
  unsigned long TotalReadSpeed;
  unsigned long SlowestFrameTime;
  unsigned long Slowest2FrameTime;
  unsigned long SlowestFrameNum;
  unsigned long Slowest2FrameNum;
  unsigned long AverageDataRate;
  unsigned long AverageFrameSize;
  unsigned long HighestMemAmount;
  unsigned long TotalIOMemory;
  unsigned long HighestIOUsed;
  unsigned long Highest1SecRate;
  unsigned long Highest1SecFrame;
} BINKSUMMARY;
cs(BINKSUMMARY, 0x7c);
co(BINKSUMMARY, TotalReadSpeed, 0x4c);
co(BINKSUMMARY, HighestMemAmount, 0x68);

/* BinkGetRealtime output. */
typedef struct BINKREALTIME {
  unsigned long FrameNum;
  unsigned long FrameRate;
  unsigned long FrameRateDiv;
  unsigned long Frames;
  unsigned long FramesTime;
  unsigned long FramesVideoDecompTime;
  unsigned long FramesAudioDecompTime;
  unsigned long FramesReadTime;
  unsigned long FramesIdleReadTime;
  unsigned long FramesThreadReadTime;
  unsigned long FramesBlitTime;
  unsigned long ReadBufferSize;
  unsigned long ReadBufferUsed;
  unsigned long FramesDataRate;
} BINKREALTIME;
cs(BINKREALTIME, 0x38);
co(BINKREALTIME, FramesBlitTime, 0x28);
co(BINKREALTIME, FramesDataRate, 0x34);

#endif /* TYPES_H */
