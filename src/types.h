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
 * (rasterizer_xbox_screen_effect.c) because FUN_001700d0 returns it by value,
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

/// size=0x8. PAL 2342 real_point2d. x at +0x00 / y at +0x04 from
/// draw_bitmap_in_rect (0xe3e80): positions[0].x = (real)rect->x0 FST'd to
/// EBP-0x44 and positions[0].y = (real)rect->y0 to EBP-0x40.
typedef struct {
  real x;                          ///< offset=0x00
  real y;                          ///< offset=0x04
} real_point2d;
cs(real_point2d, 0x8);
co(real_point2d, x, 0x0);
co(real_point2d, y, 0x4);

typedef struct real_point3d {
  real x;                          ///< offset=0x00
  real y;                          ///< offset=0x04
  real z;                          ///< offset=0x08
} real_point3d;
cs(real_point3d, 0xc);
co(real_point3d, x, 0x0);
co(real_point3d, y, 0x4);
co(real_point3d, z, 0x8);

typedef struct real_euler_angles2d {
  real yaw;
  real pitch;
} real_euler_angles2d;
cs(real_euler_angles2d, 0x08);
co(real_euler_angles2d, pitch, 0x04);

typedef struct real_euler_angles3d {
  real yaw;
  real pitch;
  real roll;
} real_euler_angles3d;
cs(real_euler_angles3d, 0x0c);
co(real_euler_angles3d, pitch, 0x04);
co(real_euler_angles3d, roll, 0x08);

typedef struct real_vector3d {
  real i;                          ///< offset=0x00
  real j;                          ///< offset=0x04
  real k;                          ///< offset=0x08
} real_vector3d;
cs(real_vector3d, 0xc);
co(real_vector3d, i, 0x0);
co(real_vector3d, j, 0x4);
co(real_vector3d, k, 0x8);

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

/* Scripted-camera state at 0x2ee5a0 (camera_scripting.c), written by the
 * scripted_camera_* setters 0x84fe0..0x85350 and read by
 * scripted_camera_update 0x853c0. Offsets/widths from those accesses;
 * evidence: recovery/evidence/scripted_camera_globals.json. */
typedef struct scripted_camera_globals {
  boolean enabled;                 ///< offset=0x00
  boolean first_update;            ///< offset=0x01
  int16_t mode;                    ///< offset=0x02
  int16_t camera_point_index;      ///< offset=0x04
  uint8_t pad_06[2];               ///< offset=0x06  never observed accessed
  real timer;                      ///< offset=0x08
  real_point3d point;              ///< offset=0x0c
  real_vector3d forward;           ///< offset=0x18
  real_vector3d up;                ///< offset=0x24
  real field_of_view;              ///< offset=0x30
  int32_t relative_object_index;   ///< offset=0x34
  int32_t animation_graph_index;   ///< offset=0x38
  int16_t animation_index;         ///< offset=0x3c
  uint8_t pad_3e[2];               ///< offset=0x3e  never observed accessed
} scripted_camera_globals_t;
cs(scripted_camera_globals_t, 0x40);
co(scripted_camera_globals_t, first_update, 0x01);
co(scripted_camera_globals_t, mode, 0x02);
co(scripted_camera_globals_t, camera_point_index, 0x04);
co(scripted_camera_globals_t, timer, 0x08);
co(scripted_camera_globals_t, point, 0x0c);
co(scripted_camera_globals_t, forward, 0x18);
co(scripted_camera_globals_t, up, 0x24);
co(scripted_camera_globals_t, field_of_view, 0x30);
co(scripted_camera_globals_t, relative_object_index, 0x34);
co(scripted_camera_globals_t, animation_graph_index, 0x38);
co(scripted_camera_globals_t, animation_index, 0x3c);

/* Default scripted-camera field of view (70 degrees; 0x3f9c61aa). */
#define SCRIPTED_CAMERA_DEFAULT_FIELD_OF_VIEW 1.22173047f

#define __int16 short
#define __int8 char

#pragma pack(1)

/// size=0x0C.  Legacy name for real_point3d (same x/y/z floats); one type so
/// a real_matrix4x3 position copies straight into a real_point3d.
typedef real_point3d vector3_t;

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
  wchar_t game_name[16];          ///< offset=0x00
  int32_t map_version;            ///< offset=0x20
  char map_name[0x80];            ///< offset=0x24
  game_variant_t game_variant;    ///< offset=0xa4
  char pad_10c[1];                ///< offset=0x10c
  int8_t minimum_players;          ///< offset=0x10d
  int8_t maximum_player_count;    ///< offset=0x10e
  uint8_t maximum_teams;            ///< offset=0x10f
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
co(network_game_blob_t, game_name,               0x00);
co(network_game_blob_t, map_version,             0x20);
co(network_game_blob_t, map_name,                0x24);
co(network_game_blob_t, game_variant,            0xa4);
co(network_game_blob_t, minimum_players,          0x10d);
co(network_game_blob_t, maximum_player_count,    0x10e);
co(network_game_blob_t, maximum_teams,           0x10f);
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
  vector3_t position;       ///< offset=0x0C
  vector3_t translational_velocity; ///< offset=0x18
  vector3_t forward;        ///< offset=0x24
  vector3_t up;             ///< offset=0x30
  vector3_t angular_velocity; ///< offset=0x3C
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
  union {
    int16_t unk_104;          ///< offset=0x68  .text:00032344                 movsx   eax, word ptr [eax+68h] team-related index
    int16_t owner_team_index; ///< offset=0x68  object.owner_team_index (PAL 2342 props.c prop_add; read @0x642a9)
  };
  int16_t unk_106;          ///< offset=0x6A
  int16_t unk_108;          ///< offset=0x6C  .text:000A8741                 cmp     [eax+6Ch], si
  int16_t unk_110;          ///< offset=0x6E  .text:0003EC0B                 cmp     word ptr [edi+6Eh], 64h
  union {
    uint32_t unk_112;         ///< offset=0x70  .text:00143FFA                 mov     [edi+70h], edx
    int32_t owner_player_index; ///< offset=0x70  object.owner_player_index (PAL 2342 props.c prop_add; CMP -1 @0x64343)
  };
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
  float unk_152;            ///< offset=0x98  .text:00136EB1                 fld     dword ptr [esi+98h]  float (FLD/FCOMP vs 1.0f, stores 0x3f800000) shield
  float unk_156;            ///< offset=0x9C  .text:0001FA9E                 fld     dword ptr [edi+9Ch]
  uint32_t unk_160;         ///< offset=0xA0  .text:00137F25                 cmp     dword ptr [ebx+0A0h], 0FFFFFFFFh   datum_handle?
  float unk_164;            ///< offset=0xA4  .text:00138865                 fld     dword ptr [esi+0A4h]
  float unk_168;            ///< offset=0xA8  .text:001387AF                 fld     dword ptr [esi+0A8h]
  uint32_t unk_172;         ///< offset=0xAC  .text:00143FB0                 mov     [edi+0ACh], eax
  uint32_t unk_176;         ///< offset=0xB0  .text:0013877C                 mov     eax, [esi+0B0h]   datum_handle?

  // 32-bit flags?
  int16_t unk_180;          ///< offset=0xB4  .text:00138775                 mov     [esi+0B4h], ax
  union {
    int8_t unk_182;           ///< offset=0xB6  .text:00018832                 or      byte ptr [eax+0B6h], 40h
    uint8_t damage_flags;     ///< offset=0xB6  object.damage_flags; bit 2 = _object_dead_bit (PAL 2342; SHR 2/AND 1 @0x642fc)
  };
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

/* ai_information_data — the 8-byte payload of an information packet;
 * ai_communication_event (0x46f10) copies it as two dwords from its seventh
 * argument, or csmemsets it to zero when that argument is NULL. */
typedef union {
  struct {
    int16_t team1_index;              ///< offset=0x00
    int16_t team2_index;              ///< offset=0x02
    boolean broken;                   ///< offset=0x04
  } allegiance;
  int32_t prop_index;                 ///< offset=0x00  target-knowledge information
  char raw[8];
} ai_information_data_t;
cs(ai_information_data_t, 0x8);

/* ai_information_packet — the AI side of a unit speech item (unit+0x348 is
 * the packet of the unit's current speech item).  Field names are T2 (PAL
 * 2342 ai.h); every offset below is confirmed against 2276 code:
 * ai_communication_finished (0x46530) fills a packet at EBP-0x40 with
 * +0x00 dword, +0x04/+0x06/+0x08 words = -1, +0x0a byte = 1, +0x0c/+0x0e/
 * +0x14 words = 0 and csmemset(+0x18, 0, 8); ai_communication_started
 * (0x44fd0) reads +0x06 and +0x0a; ai_communication_notify (0x45290) reads
 * +0x06, +0x0c, +0x14 (==1 allegiance) and +0x18/+0x1a/+0x1c.  size=0x20 */
typedef struct {
  int32_t target_unit_index;          ///< offset=0x00
  int16_t communication_type;         ///< offset=0x04
  int16_t dialogue_type_index;        ///< offset=0x06
  int16_t damage_category;            ///< offset=0x08
  boolean updated_dialogue_timers;    ///< offset=0x0a
  char pad_0b[1];                     ///< offset=0x0b
  int16_t look_priority;              ///< offset=0x0c
  int16_t look_type;                  ///< offset=0x0e
  int32_t look_unit_index;            ///< offset=0x10  unit or object handle, per look_type
  int16_t information_type;           ///< offset=0x14
  char pad_16[2];                     ///< offset=0x16
  ai_information_data_t information_data; ///< offset=0x18
} ai_information_packet_t;
cs(ai_information_packet_t, 0x20);

co(ai_information_packet_t, dialogue_type_index,     0x06);
co(ai_information_packet_t, updated_dialogue_timers, 0x0a);
co(ai_information_packet_t, look_unit_index,         0x10);
co(ai_information_packet_t, information_type,        0x14);
co(ai_information_packet_t, information_data,        0x18);

/* AI profiling meters at 0x5abaa0 (ai_profile).  Header byte names come from
 * the hs-global table entries that point at them (ai_profile_disable 0x5abaa0
 * ... ai_show_sound_distance 0x5abaab); render_spray (+0x02) is the word
 * ai_profile_change_render_spray steps.  ai_profile_initialize clears the
 * whole 0xeec block; the 28 meters (0x88 stride) start at +0x0c. */
enum { AI_METER_HISTORY_TICKS = 60, NUMBER_OF_AI_METERS = 28 };
enum { _ai_meter_collisions = 21 };
typedef struct {
  int16_t accumulator;
  int16_t current_value;
  real average;
  int32_t history_sum;
  int16_t history_next_index;
  int16_t history_count;
  int16_t history[AI_METER_HISTORY_TICKS];
} ai_meter_t;
cs(ai_meter_t, 0x88);
co(ai_meter_t, history_next_index, 0x0c);
co(ai_meter_t, history, 0x10);
typedef struct {
  boolean disabled;
  boolean move_actors_randomly;
  int16_t render_spray;
  boolean show;
  boolean show_stats;
  boolean show_actors;
  boolean show_swarms;
  boolean show_paths;
  boolean show_line_of_sight;
  boolean show_prop_types;
  boolean show_sound_distance;
  ai_meter_t meters[NUMBER_OF_AI_METERS];
} ai_profile_globals_t;
cs(ai_profile_globals_t, 0xeec);
co(ai_profile_globals_t, render_spray, 0x02);
co(ai_profile_globals_t, show, 0x04);
co(ai_profile_globals_t, show_sound_distance, 0x0b);
co(ai_profile_globals_t, meters, 0x0c);

/* AI globals: game_state_malloc("ai globals", NULL, 0x8dc) in ai_initialize
 * (0x3f677), pointer kept at 0x632574.  +0x10 is the dialogue-trigger switch
 * written by ai_globals_dialogue_triggers_enabled (0x3f7b0) and tested first
 * by ai_communication_finished/_event; ai_communication_event (0x46f10) also
 * walks three two-entry (per communication team) time arrays from +0x14.
 * The other bytes are read and written by ai.c/actions.c/actors.c but not
 * yet typed. */
typedef struct {
  uint8_t field_00[0x10];                  ///< offset=0x00
  uint8_t dialogue_triggers_enabled;       ///< offset=0x10
  uint8_t pad_11[0x3];                     ///< offset=0x11
  int32_t last_chatter_time[2];            ///< offset=0x14
  int32_t last_talk_time[2];               ///< offset=0x1c
  int32_t last_shout_time[2];              ///< offset=0x24
  uint8_t field_2c[0x8b0];                 ///< offset=0x2c
} ai_globals_t;
cs(ai_globals_t, 0x8dc);
co(ai_globals_t, dialogue_triggers_enabled, 0x10);
co(ai_globals_t, last_chatter_time, 0x14);
co(ai_globals_t, last_talk_time, 0x1c);
co(ai_globals_t, last_shout_time, 0x24);

/* ai_communication dialogue/reply tables. */
/* Dialogue table entry (0x257e48, stride 0x28 per the LEA [EAX+EAX*4] /
 * [EDI*8+0x257e48] indexing).  +0x00 and +0x02 are read by
 * ai_communication_started (0x44fd0) and ai_communication_notify (0x45290). */
typedef struct {
  int16_t communication_type;         /* +0x00 */
  int16_t communication_priority;     /* +0x02 */
  int16_t vocalization_type;          /* +0x04 */
  int16_t animation_type;             /* +0x06 */
  int16_t protagonist_type;           /* +0x08 */
  int16_t protagonist_look_priority;  /* +0x0a */
  int16_t recipient_look_direction;   /* +0x0c */
  int16_t recipient_look_priority;    /* +0x0e */
  real weight;                        /* +0x10 */
  real repeat_delay;                  /* +0x14 */
  int16_t flags;                      /* +0x18 */
  int16_t required_group;             /* +0x1a */
  int16_t required_hostility;         /* +0x1c */
  int16_t required_enemy_status;      /* +0x1e */
  int16_t required_subject_race;      /* +0x20 */
  int16_t required_cause_race;        /* +0x22 */
  int16_t required_damage;            /* +0x24 */
  char pad_26[2];                     /* +0x26 */
} dialogue_usage_t;
cs(dialogue_usage_t, 0x28);
/* Reply table entry (0x258eb0, stride 0x24).  Every field below is read by
 * ai_communication_find_actor_to_reply_to_player (0x460e0) or
 * ai_communication_finished (0x46530) at the listed offset. */
typedef struct {
  int16_t original_vocalization_type; /* +0x00 */
  int16_t original_damage_category;   /* +0x02  -1 = any */
  int16_t protagonist_type;           /* +0x04 */
  int16_t vocalization_type;          /* +0x06 */
  int16_t animation_type;             /* +0x08 */
  int16_t communication_priority;     /* +0x0a */
  uint16_t flags;                     /* +0x0c  bit 0: allowed during scripted dialog */
  char pad_0e[2];                     /* +0x0e */
  real chance;                        /* +0x10 */
  real player_chance;                 /* +0x14 */
  real delay_time;                    /* +0x18  seconds */
  real repeat_delay;                  /* +0x1c */
  bool (*reply_filter)(int original_unit_index, void *information,
                       int reply_actor_index); /* +0x20  TEST AL on return */
} reply_usage_t;
cs(reply_usage_t, 0x24);

/* Per-(entry, team) timer record in the dialogue (0x331f0c) and reply
 * (0x331f14) status tables; initialize_for_new_map sets both dwords to -1. */
typedef struct {
  int32_t last_time_spoken;           /* +0x00 */
  int32_t disable_until_time;         /* +0x04 */
} dialogue_event_status_t;
cs(dialogue_event_status_t, 0x8);
/* communication_timer_tolerances element:
 * [near_player][chatter, talk, -, shout, minimum], seconds. */
typedef real communication_timer_tolerance_t[2][5];

/* unit_speech_item — one queued/current unit speech (unit+0x338 holds the
 * current item).  Names T2 (PAL 2342 units.h); offsets confirmed by the
 * EBP-0x50 record ai_communication_finished (0x46530) passes to unit_speak
 * (0x1a6ef0): +0x00/+0x02 words, +0x04 dword, +0x08/+0x0a/+0x0c words,
 * packet at +0x10.  size=0x30 */
typedef struct {
  int16_t priority;                   ///< offset=0x00
  int16_t vocalization_type;          ///< offset=0x02
  int32_t sound_definition_index;     ///< offset=0x04
  int16_t delay_time;                 ///< offset=0x08
  int16_t ai_notification_delay;      ///< offset=0x0a
  int16_t pause_time;                 ///< offset=0x0c
  char pad_0e[2];                     ///< offset=0x0e
  ai_information_packet_t ai;         ///< offset=0x10
} unit_speech_item_t;
cs(unit_speech_item_t, 0x30);
co(unit_speech_item_t, sound_definition_index, 0x04);
co(unit_speech_item_t, pause_time,             0x0c);
co(unit_speech_item_t, ai,                     0x10);

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
  vector3_t desired_facing_vector;   ///< offset=0x1D4 .text:001AF62B                 lea     ecx, [esi+1D4h]
  vector3_t unk_480;                  ///< offset=0x1E0 .text:001AF63E                 lea     edx, [esi+1E0h]
  vector3_t unk_492;                  ///< offset=0x1EC .text:001AF678                 lea     eax, [esi+1ECh]
  vector3_t unk_504;                  ///< offset=0x1F8 .text:001AF7E5                 fld     dword ptr [esi+1F8h]
  vector3_t unk_516;                  ///< offset=0x204 .text:001AF651                 lea     eax, [esi+204h]
  vector3_t unk_528;                  ///< offset=0x210 .text:001AF68B                 add     esi, 210h
  vector3_t unk_540;                  ///< offset=0x21C .text:001AF82F                 fld     dword ptr [esi+21Ch]
  vector3_t throttle;                 ///< offset=0x228 .text:001B39A3                 lea     edx, [ebx+228h]
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
  real seat_power[2];                 ///< offset=0x2E8 .text:001A8078/001A8085 fld [esi+2E8h]/[esi+2ECh]; vehicle_update seat-power gates
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
  char pad_3e[2];                     ///< offset=0x3e
  datum_index field_40;               ///< offset=0x40  object index returned by player_aim_projectile (0xa6450); -1 at player_new
  int32_t field_44;                   ///< offset=0x44  game_time_get() at that aim (0xa6458)
  char pad_48[0x20];                  ///< offset=0x48
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
co(player_data_t, field_40,                 0x40);
co(player_data_t, field_44,                 0x44);
co(player_data_t, field_68,                 0x68);
co(player_data_t, speed_multiplier,         0x6c);
co(player_data_t, target_player_index,      0x88);
co(player_data_t, race_score,               0xc2);
co(player_data_t, field_c4,                 0xc4);
co(player_data_t, quit_at,                  0xcc);
co(player_data_t, quitting,                 0xd1);

/// size=0x38.  One candidate-target record of aim_assist.c (__FILE__ string at
/// 0x26b08c).  Built by FUN_000a5ac0 (0xa5ac0), collected by FUN_000a5d70,
/// sorted by compare_targets, copied out whole by FUN_000a6030.  Field meanings
/// beyond object_index are behavior-only (T3), so they stay field_<hex>.
typedef struct {
  datum_index object_index;  ///< offset=0x00  a5ac0 stores its object handle arg; a6470/a6130 return it
  real_point3d field_04;     ///< offset=0x04  point written via EDI = record+4 around the FUN_000a5920 call (0xa5ad2); a5830 line-of-sight target
  real_vector3d field_10;    ///< offset=0x10  field_04 minus the query point (0xa5add-0xa5afc)
  real_vector3d field_1c;    ///< offset=0x1c  copy of field_10, normalized in place (normalize3d 0xa5b10)
  real field_28;             ///< offset=0x28  normalize3d return (length of field_10); compare_targets key
  real field_2c;             ///< offset=0x2c  acos of clamped dot(field_1c, direction); compare_targets key
  real field_30;             ///< offset=0x30  product of two FUN_000a5590 terms; compare_targets key
  real field_34;             ///< offset=0x34  product of two FUN_000a5590 terms; compare_targets key
} aim_assist_record_t;
cs(aim_assist_record_t, 0x38);
co(aim_assist_record_t, object_index, 0x00);
co(aim_assist_record_t, field_04,     0x04);
co(aim_assist_record_t, field_10,     0x10);
co(aim_assist_record_t, field_1c,     0x1c);
co(aim_assist_record_t, field_28,     0x28);
co(aim_assist_record_t, field_2c,     0x2c);
co(aim_assist_record_t, field_30,     0x30);
co(aim_assist_record_t, field_34,     0x34);

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

#define NUMBER_OF_CTF_TEAMS 2

/* CTF engine globals (0x456b74). Size 0x30 is the csmemset length in
 * ctf_initialize_for_new_map (0xb05c0). */
/// size=0x30
typedef struct {
  void *flags[NUMBER_OF_CTF_TEAMS];            ///< offset=0x00 netgame flag elements (tag_block_get_element, 0x94 stride) in 0xb05c0
  int32_t weapon_indices[NUMBER_OF_CTF_TEAMS]; ///< offset=0x08 flag object handles ("NONE != weapon_index") in 0xb05c0
  int32_t scores[NUMBER_OF_CTF_TEAMS];         ///< offset=0x10 team scores (score text, 0xb0e50 compare)
  int32_t score_to_win;                        ///< offset=0x18 variant score_limit copy in 0xb05c0
  boolean flag_warnings[NUMBER_OF_CTF_TEAMS];  ///< offset=0x1c byte flags tested in 0xb0e50
  uint8_t pad_1e[2];                           ///< offset=0x1e
  int32_t flag_warning_ticks[NUMBER_OF_CTF_TEAMS]; ///< offset=0x20 counters in 0xb0e50
  int32_t flag_swap_timer;                     ///< offset=0x28 variant field_50 countdown in 0xb0c10
  int32_t next_flag_failure_time;              ///< offset=0x2c game-time deadline in FUN_000b00c0
} ctf_globals_t;
cs(ctf_globals_t, 0x30);
co(ctf_globals_t, weapon_indices,         0x08);
co(ctf_globals_t, scores,                 0x10);
co(ctf_globals_t, score_to_win,           0x18);
co(ctf_globals_t, flag_warnings,          0x1c);
co(ctf_globals_t, flag_warning_ticks,     0x20);
co(ctf_globals_t, flag_swap_timer,        0x28);
co(ctf_globals_t, next_flag_failure_time, 0x2c);

#define MAXIMUM_ODDBALLS 16

/* Oddball engine globals (0x456e08). Size 0x104 is the csmemset length in
 * oddball_engine_initialize_for_new_map (0xb2f00). */
/// size=0x104
typedef struct {
  int32_t score_to_win;                        ///< offset=0x00 variant score_limit copy in 0xb2f00
  int32_t team_score[MAXIMUM_ODDBALLS];        ///< offset=0x04
  int32_t individual_score[MAXIMUM_ODDBALLS];  ///< offset=0x44
  int32_t ball_spawn_timer[MAXIMUM_ODDBALLS];  ///< offset=0x84 countdowns in 0xb2f00/0xb33a0
  int32_t current_ball_owner[MAXIMUM_ODDBALLS]; ///< offset=0xc4 player handles, NONE-filled in 0xb2f00
} oddball_globals_t;
cs(oddball_globals_t, 0x104);
co(oddball_globals_t, team_score,         0x04);
co(oddball_globals_t, individual_score,   0x44);
co(oddball_globals_t, ball_spawn_timer,   0x84);
co(oddball_globals_t, current_ball_owner, 0xc4);

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
  real    field_0x34;            ///< offset=0x34
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
co(player_control_t, field_0x34,             0x34);
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

/* Actor iterator (actor_iterator_new/_next, 0x1c bytes): an encounter data
 * iterator, then the "encounterless list done" (+0x10) and active-only
 * (+0x11) flags, the current actor index (+0x14) and the next one (+0x18). */
typedef struct {
  data_iter_t encounter_iterator;
  boolean iterated_encounterless_list;
  boolean active_only;
  uint8_t pad_12[2];
  int32_t index;
  int32_t next_index;
} actor_iterator_t;
cs(actor_iterator_t, 0x1c);
co(actor_iterator_t, iterated_encounterless_list, 0x10);
co(actor_iterator_t, active_only, 0x11);
co(actor_iterator_t, index, 0x14);
co(actor_iterator_t, next_index, 0x18);

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

/// size=0x24. Three basis rows; the 3x3 matrix operand of the transpose
/// (0x1099f0), multiply (0x109c70) and to-quaternion (0x10a330) helpers.
typedef struct {
  real_vector3d forward; ///< offset=0x00
  real_vector3d left;    ///< offset=0x0c
  real_vector3d up;      ///< offset=0x18
} real_matrix3x3;
cs(real_matrix3x3, 0x24);

/// Per-object record of the object render and shadow passes. Names: PAL 2342
/// render_objects.c struct object_render_data (T2). Offsets: FUN_0018c100
/// branches on shadow (+0x08) and writes no_planar_fog (+0x09);
/// render_object_shadow_end (0x18b990) reads shadow_matrix (+0x0c, its
/// forward/left/up/position at +0x10/+0x1c/+0x28/+0x34) and
/// shadow_bounding_radius (+0x40). Size beyond +0x44 is unproven in 2276.
typedef struct object_render_data {
  datum_index object_index;          ///< offset=0x00
  void *lighting;                    ///< offset=0x04
  boolean shadow;                    ///< offset=0x08
  boolean no_planar_fog;             ///< offset=0x09
  byte pad_0a[2];                    ///< offset=0x0a
  real_matrix4x3 shadow_matrix;      ///< offset=0x0c
  real shadow_bounding_radius;       ///< offset=0x40
} object_render_data;
co(object_render_data, shadow, 0x08);
co(object_render_data, no_planar_fog, 0x09);
co(object_render_data, shadow_matrix, 0x0c);
co(object_render_data, shadow_bounding_radius, 0x40);

/// size=0x10. Vector part then scalar; identity is (0,0,0,1) (0x28cae8).
typedef struct {
  real_vector3d v; ///< offset=0x00
  real w;          ///< offset=0x0c
} real_quaternion;
cs(real_quaternion, 0x10);
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

/// size=0xC. global_real_rgb_white (0x2ee708) points at {1,1,1}; lightning_submit
/// (0x135510) multiplies red/green/blue from +0x0/+0x4/+0x8 and indexes
/// render_animation colors with stride 0xC (LEA EAX,[EAX+EAX*2]; [ECX+EAX*4-0xC]).
typedef struct {
  real red;   ///< offset=0x00
  real green; ///< offset=0x04
  real blue;  ///< offset=0x08
} real_rgb_color;
cs(real_rgb_color, 0xc);
co(real_rgb_color, green, 0x04);
co(real_rgb_color, blue, 0x08);

/// rasterizer_debug_options (0x3256b8). Prefix only. Names and offsets follow
/// PAL 2342 rasterizer_debug_options.h (T2), whose offsets match every field
/// read here in 2276: CMP word [0x3256ba] (statistics_mode), [0x3256c4]
/// (draw_models), [0x3256ca] (draw_environment_shadows), [0x3256d4] (fog),
/// [0x3256f6]/[0x3256f7] (shadows_convolution/shadows_debug).
typedef struct {
  byte pad_00[2];                   ///< offset=0x00
  int16_t statistics_mode;          ///< offset=0x02
  byte pad_04[8];                   ///< offset=0x04
  boolean draw_models;              ///< offset=0x0c
  boolean draw_transparent_models;  ///< offset=0x0d
  byte pad_0e[4];                   ///< offset=0x0e
  boolean draw_environment_shadows; ///< offset=0x12
  byte pad_13[9];                   ///< offset=0x13
  boolean fog;                      ///< offset=0x1c
  byte pad_1d[0x21];                ///< offset=0x1d
  boolean shadows_convolution;      ///< offset=0x3e
  boolean shadows_debug;            ///< offset=0x3f
} rasterizer_debug_options_t;
co(rasterizer_debug_options_t, statistics_mode, 0x02);
co(rasterizer_debug_options_t, draw_models, 0x0c);
co(rasterizer_debug_options_t, draw_environment_shadows, 0x12);
co(rasterizer_debug_options_t, fog, 0x1c);
co(rasterizer_debug_options_t, shadows_convolution, 0x3e);
co(rasterizer_debug_options_t, shadows_debug, 0x3f);

/// global_window_parameters (0x5a5bc0), PAL 2342 struct
/// rasterizer_window_begin_parameters. Prefix only: the render target the
/// window draws to (CMP word [0x5a5bc0],0 against
/// _rasterizer_target_render_primary), window_index (named by the
/// rasterizer_lights.c assert string at +0x02) and the render camera at +0x08,
/// whose position/forward rasterizer_lights.c reads at +0x08/+0x14 and whose
/// viewport_bounds rasterizer_set_target reads at 0x5a5bf4..0x5a5bfa.
typedef struct {
  int16_t rasterizer_target; ///< offset=0x00
  int16_t window_index;      ///< offset=0x02
  byte pad_04[4];            ///< offset=0x04
  camera_t camera;           ///< offset=0x08
} rasterizer_window_begin_parameters;
co(rasterizer_window_begin_parameters, window_index, 0x02);
co(rasterizer_window_begin_parameters, camera, 0x08);

/// rasterizer_set_target / rasterizer_set_target_as_texture targets. Names
/// follow PAL 2342 (T2); 2276 confirms the order: each case of
/// rasterizer_set_target (0x158140) binds the surface whose IDirect3D* error
/// string in FUN_00157010 names it, and case 6 asserts
/// "mipmap_index>=0 && mipmap_index<RASTERIZER_TARGET_WATER_MAX_MIPMAP_LEVELS".
enum rasterizer_target {
  _rasterizer_target_render_primary = 0,
  _rasterizer_target_render_secondary,
  _rasterizer_target_shadow_primary,
  _rasterizer_target_shadow_secondary,
  _rasterizer_target_sun_glow_primary,
  _rasterizer_target_sun_glow_secondary,
  _rasterizer_target_water_bumpmap,
  _rasterizer_target_render_primary_copy,
  NUMBER_OF_RASTERIZER_TARGETS
};

/// global_d3d_surface_water element count (FUN_00157010's
/// GetSurfaceLevel loop runs mip 0..3).
enum {
  RASTERIZER_TARGET_WATER_MAX_MIPMAP_LEVELS = 4
};

/// global_frame_parameters (0x5a5e18), PAL 2342 struct
/// rasterizer_frame_begin_parameters. Prefix only: FUN_00157940
/// (_rasterizer_frame_begin) stores parameters->game_time_sec here, and the
/// texture-animation callers pass it as the animation time.
typedef struct {
  real game_time_sec; ///< offset=0x00
} rasterizer_frame_begin_parameters;

/// rasterizer_frame_statistics (0x5a5400), PAL 2342 struct
/// rasterizer_frame_statistics_globals. 0x170 bytes: FUN_0017eb90 clears it
/// with csmemset(&rasterizer_frame_statistics, 0, 0x170). The named counters
/// are the ones rasterizer_frame_statistics_update (0x17ef00) prints as
/// "shadows (%d)|t%d|t%d|t%d" (+0x30..+0x3c) and "model shadows (%d)"
/// (+0xf4..+0x100); the first column is bumped once per shadow by
/// FUN_00172a30 / FUN_00172590, the other three by the draw paths.
typedef struct {
  real frames_per_second;                 ///< offset=0x00
  byte pad_04[0x2c];                      ///< offset=0x04
  uint32_t shadow_count;                  ///< offset=0x30
  uint32_t shadow_vertex_count;           ///< offset=0x34
  uint32_t shadow_triangle_count;         ///< offset=0x38
  uint32_t shadow_draw_count;             ///< offset=0x3c
  byte pad_40[0xb4];                      ///< offset=0x40
  uint32_t model_shadow_count;            ///< offset=0xf4
  uint32_t model_shadow_vertex_count;     ///< offset=0xf8
  uint32_t model_shadow_triangle_count;   ///< offset=0xfc
  uint32_t model_shadow_draw_count;       ///< offset=0x100
  byte pad_104[0x6c];                     ///< offset=0x104
} rasterizer_frame_statistics_globals;
cs(rasterizer_frame_statistics_globals, 0x170);
co(rasterizer_frame_statistics_globals, shadow_count, 0x30);
co(rasterizer_frame_statistics_globals, shadow_draw_count, 0x3c);
co(rasterizer_frame_statistics_globals, model_shadow_count, 0xf4);
co(rasterizer_frame_statistics_globals, model_shadow_draw_count, 0x100);

/// global_pixel_shader (0x5a5ac0), PAL 2342 struct pixel_shader_definition:
/// the 0xf0-byte pixel-shader state block handed to rasterizer_set_pixel_shader
/// (csmemset(&global_pixel_shader, 0, 0xf0) in
/// _rasterizer_environment_shadow_draw).
typedef struct {
  uint32_t alpha_inputs[8];           ///< offset=0x00
  uint32_t final_combiner_inputs_abcd; ///< offset=0x20
  uint32_t final_combiner_inputs_efg; ///< offset=0x24
  uint32_t constant_0[8];             ///< offset=0x28
  uint32_t constant_1[8];             ///< offset=0x48
  uint32_t alpha_outputs[8];          ///< offset=0x68
  uint32_t rgb_inputs[8];             ///< offset=0x88
  uint32_t compare_mode;              ///< offset=0xa8
  uint32_t final_combiner_constant_0; ///< offset=0xac
  uint32_t final_combiner_constant_1; ///< offset=0xb0
  uint32_t rgb_outputs[8];            ///< offset=0xb4
  uint32_t combiner_count;            ///< offset=0xd4
  uint32_t texture_modes;             ///< offset=0xd8
  byte pad_dc[0x14];                  ///< offset=0xdc
} pixel_shader_definition;
cs(pixel_shader_definition, 0xf0);
co(pixel_shader_definition, constant_1, 0x48);
co(pixel_shader_definition, rgb_outputs, 0xb4);
co(pixel_shader_definition, texture_modes, 0xd8);

/// rasterizer_environment_shadows_globals (0x47e46c), PAL 2342
/// rasterizer_xbox_shadows.c. shadow_color is passed to
/// real_rgb_color_to_pixel32 (PUSH 0x47e46c at 0x173317); object_bounding_radius
/// and shadow_matrix are stored by FUN_00172a30; local_parameters by
/// FUN_00172590; shadow_setup is the once-per-shadow latch of
/// _rasterizer_environment_shadow_draw; shadow_used is tested by
/// _rasterizer_environment_shadow_end.
typedef struct environment_shadows_globals {
  real_rgb_color shadow_color;    ///< offset=0x00
  real object_bounding_radius;    ///< offset=0x0c
  real_matrix4x3 shadow_matrix;   ///< offset=0x10
  void *local_parameters;         ///< offset=0x44
  boolean shadow_setup;           ///< offset=0x48
  boolean shadow_used;            ///< offset=0x49
} environment_shadows_globals;
co(environment_shadows_globals, object_bounding_radius, 0x0c);
co(environment_shadows_globals, shadow_matrix, 0x10);
co(environment_shadows_globals, local_parameters, 0x44);
co(environment_shadows_globals, shadow_setup, 0x48);
co(environment_shadows_globals, shadow_used, 0x49);
co(camera_t, field_00, 0x00);
co(camera_t, field_0c, 0x0c);
co(camera_t, field_18, 0x18);
co(camera_t, vertical_field_of_view, 0x28);
co(camera_t, viewport_bounds, 0x2c);
co(camera_t, z_near, 0x3c);
co(camera_t, z_far, 0x40);
co(camera_t, field_44, 0x44);

/// size=0x14. One screen-space vertex handed to rasterizer_sprites_render
/// (0x17cfa0). Offsets from draw_bitmap_in_rect (0xe3e80): position copied to
/// [ECX-8]/[ECX-4], texture coordinates FSTP'd to [ECX]/[ECX+4], color to
/// [ECX+8], stride ADD ECX,0x14. Names from PAL 2342 dynamic_screen_vertex.
typedef struct {
  real_point2d position;            ///< offset=0x00 screen x, y
  real_point2d texture_coordinates; ///< offset=0x08 u, v
  uint32_t color;                   ///< offset=0x10 pixel32 argb
} dynamic_screen_vertex_t;
cs(dynamic_screen_vertex_t, 0x14);
co(dynamic_screen_vertex_t, position, 0x00);
co(dynamic_screen_vertex_t, texture_coordinates, 0x08);
co(dynamic_screen_vertex_t, color, 0x10);

/// size=0x8c (csmemset size in draw_bitmap_in_rect 0xe3e80). Parameter block
/// for rasterizer_sprites_render (0x17cfa0). Every named offset is a store
/// observed in draw_bitmap_in_rect (params base EBP-0xec); names follow PAL
/// 2342 rasterizer_dynamic_screen_geometry_parameters. pad_ bytes were not
/// observed accessed there.
typedef struct {
  void *meter_parameters;              ///< offset=0x00
  uint8_t pad_04[4];                   ///< offset=0x04
  uint8_t map_wrapped[2];              ///< offset=0x08
  uint8_t pad_0a[2];                   ///< offset=0x0a
  void *map[3];                        ///< offset=0x0c bitmap_data pointers
  uint8_t map_anchor_screen[2];        ///< offset=0x18
  uint8_t pad_1a[2];                   ///< offset=0x1a
  real_point2d *map_offset[2];         ///< offset=0x1c
  uint8_t pad_24[4];                   ///< offset=0x24
  real_vector2d map_scale[3];          ///< offset=0x28
  real_vector2d map_texture_scale[3];  ///< offset=0x40
  real *map_tint[2];                   ///< offset=0x58 -> real[3] rgb
  uint8_t pad_60[4];                   ///< offset=0x60
  real_argb_color plasma_fade;         ///< offset=0x64
  uint8_t doing_plasma_effect;         ///< offset=0x74
  uint8_t pad_75[3];                   ///< offset=0x75
  real *map_fade[3];                   ///< offset=0x78
  int16_t map0_to_1_blend_function;    ///< offset=0x84
  int16_t map1_to_2_blend_function;    ///< offset=0x86
  int16_t framebuffer_blend_function;  ///< offset=0x88
  uint8_t point_sampled;               ///< offset=0x8a
  uint8_t pad_8b[1];                   ///< offset=0x8b
} rasterizer_dynamic_screen_geometry_parameters_t;
cs(rasterizer_dynamic_screen_geometry_parameters_t, 0x8c);
co(rasterizer_dynamic_screen_geometry_parameters_t, map_wrapped, 0x08);
co(rasterizer_dynamic_screen_geometry_parameters_t, map, 0x0c);
co(rasterizer_dynamic_screen_geometry_parameters_t, map_anchor_screen, 0x18);
co(rasterizer_dynamic_screen_geometry_parameters_t, map_offset, 0x1c);
co(rasterizer_dynamic_screen_geometry_parameters_t, map_scale, 0x28);
co(rasterizer_dynamic_screen_geometry_parameters_t, map_texture_scale, 0x40);
co(rasterizer_dynamic_screen_geometry_parameters_t, map_tint, 0x58);
co(rasterizer_dynamic_screen_geometry_parameters_t, plasma_fade, 0x64);
co(rasterizer_dynamic_screen_geometry_parameters_t, doing_plasma_effect, 0x74);
co(rasterizer_dynamic_screen_geometry_parameters_t, map_fade, 0x78);
co(rasterizer_dynamic_screen_geometry_parameters_t, map0_to_1_blend_function, 0x84);
co(rasterizer_dynamic_screen_geometry_parameters_t, framebuffer_blend_function, 0x88);
co(rasterizer_dynamic_screen_geometry_parameters_t, point_sampled, 0x8a);

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

/* Tag block header; evidence comment lives at its former site below. */
typedef struct tag_block {
    int32_t  count;      /* +0x00: element count (evidence: "encounter_definition->squads.count" assert, encounters.c:0x5a4) */
    void    *address;    /* +0x04: element array base (block ptr passed to tag_block_get_element) */
    int32_t  field_08;   /* +0x08: block definition ptr; no runtime access observed */
} tag_block;
cs(tag_block, 0xc);

/* 'unit' tag definition (struct unit_definition = _object_definition +
 * _unit_definition).  Only the fields read by 2276 code so far are
 * modelled; the leading bytes and the total size are not verified.  Placed
 * after tag_block because it embeds one.
 *   +0x284 ai_danger_radius — prop_add @0x64307 copies it (MOV EDX,[ECX+0x284])
 *          into prop->suicide_radius.
 *   +0x2e4 seats — tag block of 0x11c-byte seat elements; indexed by the
 *          unit's seat index (unit +0x2a0): director_desired_perspective
 *          (0x864b0) hands &seats to tag_block_get_element with size 0x11c. */
typedef struct unit_definition_t {
  char pad_000[0x17c];
  uint32_t flags;                         /* +0x17c (bit 20 special) */
  char pad_180[0x284 - 0x180];
  real ai_danger_radius;                  /* +0x284 */
  char pad_288[0x2e4 - 0x288];
  tag_block seats;                        /* +0x2e4 */
} unit_definition_t;
co(unit_definition_t, flags, 0x17c);
co(unit_definition_t, ai_danger_radius, 0x284);
co(unit_definition_t, seats, 0x2e4);

/* Biped tag definition: the unit definition followed by the biped block.
 *   +0x2f4 biped_flags — bit 2 flying, bit 6 climbs anything
 *          (biped_accelerate, 0x1a4a70). */
typedef struct biped_definition_t {
  unit_definition_t unit;                 /* +0x000 */
  char pad_2f0[0x2f4 - 0x2f0];
  uint32_t biped_flags;                   /* +0x2f4 */
} biped_definition_t;
co(biped_definition_t, biped_flags, 0x2f4);

/* Unit seat tag-block element, 0x11c bytes (director_desired_perspective's
 * tag_block_get_element size).  Only the leading flags dword is read here:
 * bit 4 makes the director use the following camera, bit 6 picks a
 * perspective from the unit's enter/exit animation state. */
typedef struct {
  uint32_t flags;
  uint8_t pad_04[0x118];
} unit_seat_t;
cs(unit_seat_t, 0x11c);

enum {
  _unit_seat_third_person_camera_bit = 4,
  _unit_seat_third_person_on_enter_bit = 6,
  /* first_person_camera_for_unit_and_vector (0x88d50) tests the sign of
   * the seat's first flags byte to pick the "primary trigger" marker. */
  _unit_seat_first_person_camera_bit = 7
};

/* object_get_and_verify_type type masks: bit n = object type n. */
enum {
  _object_type_biped = 0,
  _object_type_vehicle = 1,
  _object_mask_unit = (1 << _object_type_biped) | (1 << _object_type_vehicle),
  _object_mask_vehicle = 1 << _object_type_vehicle,
  _object_mask_all = -1
};

/* unit_data_t.unk_595 (+0x253) animation state values tested by
 * director_desired_perspective. */
enum {
  _unit_state_entering_seat = 0x1a,
  _unit_state_exiting_seat = 0x1b
};

/* 'antr' animation-graph tag: only the node and animation blocks at
 * +0x68/+0x74 are proven (scripted_camera_set_animation 0x85000 tests
 * nodes.count == 1, then walks animations with element size 0xb4). */
typedef struct {
  uint8_t pad_00[0x68];                    ///< offset=0x00  not accessed here
  tag_block nodes;                         ///< offset=0x68
  tag_block animations;                    ///< offset=0x74
} animation_graph_t;
#define ANIMATION_GRAPH_TAG 0x616e7472 /* 'antr' */
co(animation_graph_t, nodes, 0x68);
co(animation_graph_t, animations, 0x74);

/* animation_graph_t::animations element; stride 0xb4 from the
 * tag_block_get_element calls in 0x85000 / 0x853c0. */
typedef struct {
  char name[0x20];                         ///< offset=0x00  crt_stricmp key
  int16_t field_20;                      ///< offset=0x20
  int16_t frame_count;                   ///< offset=0x22
  int16_t frame_size;                    ///< offset=0x24
  int16_t field_26;                      ///< offset=0x26
  int32_t field_28;                      ///< offset=0x28
  int16_t field_2c;                      ///< offset=0x2c
  int16_t field_2e;                      ///< offset=0x2e
  uint8_t pad_30[0xa];
  uint8_t field_3a;                      ///< offset=0x3a
  uint8_t pad_3b[0x7];
  uint16_t field_42;                     ///< offset=0x42
  uint8_t pad_44[0x10];
  void *field_54;                        ///< offset=0x54
  uint8_t pad_58[0x5c];
} animation_t;
cs(animation_t, 0xb4);
co(animation_t, field_20, 0x20);
co(animation_t, frame_count, 0x22);
co(animation_t, frame_size, 0x24);
co(animation_t, field_26, 0x26);
co(animation_t, field_28, 0x28);
co(animation_t, field_2c, 0x2c);
co(animation_t, field_2e, 0x2e);
co(animation_t, field_3a, 0x3a);
co(animation_t, field_42, 0x42);
co(animation_t, field_54, 0x54);

/* scenario_t::cutscene_camera_points element; stride 0x68 from
 * scripted_camera_set (0x85180). */
typedef struct {
  uint8_t pad_00[0x28];                    ///< offset=0x00  not accessed here
  real_point3d position;                   ///< offset=0x28
  real orientation[3];                     ///< offset=0x34  euler angles
  real field_of_view;                      ///< offset=0x40
  uint8_t pad_44[0x24];                    ///< offset=0x44  not accessed here
} scenario_cutscene_camera_point_t;
cs(scenario_cutscene_camera_point_t, 0x68);
co(scenario_cutscene_camera_point_t, position, 0x28);
co(scenario_cutscene_camera_point_t, orientation, 0x34);
co(scenario_cutscene_camera_point_t, field_of_view, 0x40);

/* Scenario tag; size unproven (only offsets up to 0x5b0 accessed). */
typedef struct
{
  _BYTE unk_0[60];   ///< offset=0x00
  _WORD type;        ///< offset=0x3C
  _BYTE unk_62[174]; ///< offset=0x3E
  int   unk_236;     ///< offset=0xEC
  _BYTE unk_240[0x114];                ///< offset=0xF0
  struct tag_block object_names;       ///< offset=0x204  element 0x24 (scenario_object_name_t)
  uint8_t pad_210[0x21c];              ///< offset=0x210
  struct tag_block ai_encounters;      ///< offset=0x42c  element 0xb0 (encounter_definition)
  uint8_t pad_438[0xb8];               ///< offset=0x438
  struct tag_block cutscene_camera_points; ///< offset=0x4F0  element 0x68, scripted_camera_set
  uint8_t pad_4fc[0xa8];                   ///< offset=0x4fc
  struct tag_block structure_bsp_references; ///< offset=0x5a4  element 0x20 (scenario_structure_bsp_reference_t), 0x18e480
} scenario_t;
co(scenario_t, cutscene_camera_points, 0x4f0);
co(scenario_t, object_names, 0x204);
co(scenario_t, ai_encounters, 0x42c);

/* scenario_t::object_names element (stride 0x24); the name is at +0, read
 * as a string by the debug printers (e.g. ai_communication_started). */
typedef struct {
  char name[0x20];
  uint8_t pad_20[4];
} scenario_object_name_t;
cs(scenario_object_name_t, 0x24);

/* encounter_definition::squads element (stride 0xe8); the name is at +0,
 * printed by ai_communication_started. */
typedef struct {
  char name[0x20];
  uint8_t pad_20[0xc8];
} squad_definition_t;
cs(squad_definition_t, 0xe8);
co(scenario_t, structure_bsp_references, 0x5a4);

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
  union {
    struct {
      real_point3d point;                            /* +0x04 */
      int32_t surface_index;                         /* +0x10 */
    } raw;
    int16_t firing_position_index;                   /* +0x04 */
    int16_t move_position_index;                     /* +0x04 */
    struct {
      int32_t prop_index;                            /* +0x04 */
      float accept_radius;                           /* +0x08 */
    } prop;
    struct {
      int16_t position_index;                        /* +0x04 */
      char pad_06[0x2];                              /* +0x06 */
      float field_08;                                /* +0x08 */
      float field_0c;                                /* +0x0c */
      int32_t dest_node;                             /* +0x10 */
    };
  };
  int32_t orders_ignore_target_object_index;         /* +0x14 */
} path_destination_t;
cs(path_destination_t, 0x18);
co(path_destination_t, mode, 0x00);
co(path_destination_t, field_02, 0x02);
co(path_destination_t, raw.point, 0x04);
co(path_destination_t, raw.surface_index, 0x10);
co(path_destination_t, firing_position_index, 0x04);
co(path_destination_t, move_position_index, 0x04);
co(path_destination_t, prop.prop_index, 0x04);
co(path_destination_t, prop.accept_radius, 0x08);
co(path_destination_t, position_index, 0x04);
co(path_destination_t, field_08, 0x08);
co(path_destination_t, field_0c, 0x0c);
co(path_destination_t, dest_node, 0x10);
co(path_destination_t, orders_ignore_target_object_index, 0x14);
/* prop_t — struct prop_datum, an element of the "prop" data_t pool
 * (0x138 = 312 bytes; props_initialize @0x64100 allocates 0x300 x 0x138).
 *
 * Historical field names and grouping came from PAL 2342 source/ai/props.h
 * `struct prop_datum`. This is historical provenance, not naming authority.
 * The following offsets have reported 2276 evidence, cross-checked against the
 * stores in prop_add (0x64170) and prop_setup_orphan (0x647c0):
 *   +0x04/+0x08/+0x0c/+0x18/+0x1c/+0x66/+0x6a/+0x6c/+0x70/+0x74/+0x7c/+0x8c/
 *   +0xa0/+0xb0/+0xb4/+0xb8 initialised by prop_add; +0x24 = 4
 *   (_prop_state_uninspected_orphan), +0x3a = 900 (30 s), +0x40 =
 *   body_position(+0xbc) - last_perceived_body_position(+0x80), +0xd4
 *   velocity and +0x123 quantized_speed written by prop_setup_orphan.
 *   Asserts in props.c name owner_actor_index, orphan_prop_index and
 *   parent_prop_index (both at +0x0c).  actor_move_to_prop reads +0x18 and
 *   +0x110. Fields not listed here were historically PAL-placed; their widths
 *   and semantics require independent verification. Neutralizing an unused
 *   name preserves storage and does not validate the historical layout.
 */
/* prop_t.state values — PAL 2342 source/ai/props.h (T2).  2276 evidence:
 * prop_setup_orphan stores 4; prop_new_unacknowledged skips 4..5 (orphans);
 * actor_perception_become_acknowledged tests 2..3 and stores 3. */
enum prop_state {
  _prop_state_unacknowledged = 0,
  _prop_state_becoming_acknowledged = 1,
  _prop_state_becoming_unacknowledged = 2,
  _prop_state_acknowledged = 3,
  _prop_state_uninspected_orphan = 4,
  _prop_state_inspected_orphan = 5
};
typedef struct prop_t {
  int16_t identifier;                     /* +0x000 datum salt */
  char pad_002[0x2];
  int32_t owner_actor_index;              /* +0x004 */
  int32_t next_prop_index;                /* +0x008 actor prop-chain link */
  union {
    int32_t orphan_prop_index;            /* +0x00c */
    int32_t parent_prop_index;            /* +0x00c */
  };
  int16_t type;                           /* +0x010 */
  int16_t team_index;                     /* +0x012 */
  boolean swarm;                          /* +0x014 */
  char pad_015[0x3];
  int32_t unit_index;                     /* +0x018 */
  int32_t actor_index;                    /* +0x01c */
  real suicide_radius;                    /* +0x020 */
  int16_t state;                          /* +0x024 enum prop_state */
  int16_t timer;                          /* +0x026 */
  int32_t swarm_unit_selected_time;       /* +0x028 */
  real awareness;                         /* +0x02c */
  int16_t perception;                     /* +0x030 */
  int16_t visibility;                     /* +0x032 */
  int16_t audibility;                     /* +0x034 */
  int16_t ineffability;                   /* +0x036 */
  int16_t line_of_sight;                  /* +0x038 */
  int16_t orphan_lifespan_ticks;          /* +0x03a */
  int16_t orphan_inspection_ticks;        /* +0x03c */
  char pad_03e[0x2];
  real_vector3d orphan_hint_vector;       /* +0x040 */
  int16_t field_4c;                       /* +0x04c; historical name in provenance ledger */
  boolean orphan_corpse_cheated;          /* +0x04e */
  char pad_04f[0x1];
  real target_weight;                     /* +0x050 */
  real look_interest;                     /* +0x054 */
  real field_58;                         /* +0x058 */
  int32_t field_5c;                      /* +0x05c */
  boolean enemy;                          /* +0x060 */
  boolean ally;                           /* +0x061 */
  boolean field_62;                      /* +0x062 */
  boolean in_use;                         /* +0x063 */
  boolean refresh_stimuli;                /* +0x064 */
  char pad_065[0x1];
  int16_t unit_effect;                    /* +0x066 */
  int16_t field_68;                      /* +0x068 */
  int16_t required_ticks;                 /* +0x06a */
  int16_t field_6c;                      /* +0x06c */
  char pad_06e[0x2];
  real field_70;                         /* +0x070 */
  boolean field_74;                      /* +0x074 */
  char pad_075[0x1];
  int16_t dead_ticks;                     /* +0x076 */
  int16_t field_78;                      /* +0x078 */
  char pad_07a[0x2];
  int32_t last_perceived_time;            /* +0x07c */
  real_point3d last_perceived_body_position; /* +0x080 */
  int32_t last_visible_time;              /* +0x08c */
  real_point3d last_visible_head_position; /* +0x090 */
  int16_t field_9c;                      /* +0x09c; word comparison @0x2fc6c */
  char pad_09e[0x2];
  int32_t last_unreachable_time;          /* +0x0a0 */
  boolean unopposable_enemy;              /* +0x0a4 */
  char pad_0a5[0x1];
  int16_t unopposable_casualties_inflicted; /* +0x0a6 */
  int16_t field_a8;                      /* +0x0a8 */
  int16_t field_aa;                      /* +0x0aa; word clear @0x2fcba */
  int16_t field_ac;                      /* +0x0ac; word clear @0x2fcc8 */
  int16_t field_ae;                      /* +0x0ae; word clear @0x2fcc1 */
  int16_t ticks_since_definitely_located; /* +0x0b0 */
  char pad_0b2[0x2];
  int32_t definite_knowledge_source_actor; /* +0x0b4 */
  boolean definitely_located;             /* +0x0b8 */
  boolean tried_to_uncover;               /* +0x0b9 */
  boolean tried_to_search;                /* +0x0ba */
  boolean abandoned_search;               /* +0x0bb */
  real_point3d body_position;             /* +0x0bc */
  real_point3d center_of_mass;            /* +0x0c8 */
  real_vector3d velocity;                 /* +0x0d4 */
  real_vector3d actor_to_prop;            /* +0x0e0 */
  int32_t pathfinding_surface_index;      /* +0x0ec */
  real_point3d pathfinding_point;         /* +0x0f0 */
  int32_t body_location_leaf_index;       /* +0x0fc struct location */
  int16_t body_location_cluster_index;    /* +0x100 */
  int16_t field_102;                     /* +0x102 */
  real_point3d head_position;             /* +0x104 */
  int32_t vehicle_index;                  /* +0x110 */
  int32_t attached_to_unit_index;         /* +0x114 */
  boolean underwater;                     /* +0x118 */
  char pad_119[0x3];
  real distance;                          /* +0x11c */
  char lighting;                          /* +0x120 */
  char quantized_distance;                /* +0x121 */
  char quantized_facing;                  /* +0x122 */
  char quantized_speed;                   /* +0x123 */
  char quantized_closing_speed;           /* +0x124 */
  char child_units_attached;              /* +0x125 */
  boolean delay_requirement_decision;     /* +0x126 */
  boolean dead;                           /* +0x127 */
  boolean really_dead;                    /* +0x128 */
  boolean just_killed;                    /* +0x129 */
  boolean just_became_visible;            /* +0x12a */
  boolean noncombat;                      /* +0x12b */
  boolean in_combat;                      /* +0x12c */
  boolean fighting;                       /* +0x12d */
  boolean player;                         /* +0x12e */
  boolean shooting;                       /* +0x12f */
  boolean flying;                         /* +0x130 */
  boolean active_camouflage;              /* +0x131 */
  boolean flashlight;                     /* +0x132 */
  boolean ignore;                         /* +0x133 */
  boolean preferred_target;               /* +0x134 */
  boolean vehicle_gunner;                 /* +0x135 */
  boolean dangerous_vehicle_driver;       /* +0x136 */
  char pad_137[0x1];
} prop_t;
cs(prop_t, 0x138);
co(prop_t, owner_actor_index, 0x004);
co(prop_t, next_prop_index, 0x008);
co(prop_t, orphan_prop_index, 0x00c);
co(prop_t, team_index, 0x012);
co(prop_t, unit_index, 0x018);
co(prop_t, actor_index, 0x01c);
co(prop_t, state, 0x024);
co(prop_t, orphan_lifespan_ticks, 0x03a);
co(prop_t, orphan_hint_vector, 0x040);
co(prop_t, orphan_corpse_cheated, 0x04e);
co(prop_t, enemy, 0x060);
co(prop_t, unit_effect, 0x066);
co(prop_t, field_6c, 0x06c);
co(prop_t, field_70, 0x070);
co(prop_t, dead_ticks, 0x076);
co(prop_t, last_perceived_time, 0x07c);
co(prop_t, last_perceived_body_position, 0x080);
co(prop_t, last_visible_time, 0x08c);
co(prop_t, last_unreachable_time, 0x0a0);
co(prop_t, ticks_since_definitely_located, 0x0b0);
co(prop_t, definite_knowledge_source_actor, 0x0b4);
co(prop_t, definitely_located, 0x0b8);
co(prop_t, tried_to_uncover, 0x0b9);
co(prop_t, body_position, 0x0bc);
co(prop_t, velocity, 0x0d4);
co(prop_t, vehicle_index, 0x110);
co(prop_t, distance, 0x11c);
co(prop_t, quantized_speed, 0x123);
co(prop_t, dead, 0x127);
co(prop_t, really_dead, 0x128);
co(prop_t, player, 0x12e);

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
  union {
    int16_t field_004;                                 /* +0x004  accessed 5x, meaning unproven */
    int16_t meta_type;                                 /* +0x004  actor->meta.type (PAL 2342 actors.h actor_meta_data; read by prop_add @0x6439b) */
  };
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
  union {
    int32_t field_018;                                 /* +0x018  accessed 55x, meaning unproven */
    int32_t meta_unit_index;                           /* +0x018  actor->meta.unit_index */
  };
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
  union {
    int16_t field_03e;                                 /* +0x03e  accessed 5x, meaning unproven */
    int16_t meta_team_index;                           /* +0x03e  actor->meta.team_index (PAL 2342; game_team_is_* arg in prop_add @0x642b7) */
  };
  char field_040;                                    /* +0x040  accessed 4x, meaning unproven */
  char pad_041[0x3];
  int32_t field_044;                                 /* +0x044  accessed 2x, meaning unproven */
  int16_t field_048;                                 /* +0x048  accessed 2x, meaning unproven */
  int16_t field_04a;                                 /* +0x04a  accessed 4x, meaning unproven */
  char field_04c;                                    /* +0x04c  accessed 9x, meaning unproven */
  char pad_04d[0x1];
  int16_t field_04e;                                 /* +0x04e  accessed 5x, meaning unproven */
  union {
    int32_t field_050;                                 /* +0x050  accessed 4x, meaning unproven */
    int32_t meta_first_prop_index;                     /* +0x050  actor->meta.first_prop_index: prop chain head (PAL 2342; written by prop_add @0x643b8) */
  };
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
  int32_t field_07c;                                 /* +0x07c  dword MOV/INC @0x30391/0x30396, meaning unproven */
  int32_t field_080;                                 /* +0x080  dword MOV @0x3039e/0x303d8, meaning unproven */
  int32_t field_084;                                /* +0x084  accessed 1x, meaning unproven */
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
  union {
    struct {
      float field_120;                                   /* +0x120  accessed 1x, meaning unproven */
      float field_124;                                   /* +0x124  accessed 1x, meaning unproven */
      float field_128;                                   /* +0x128  accessed 3x, meaning unproven */
    };
    real_point3d head_position;                      /* +0x120  actor->input.position.head_position (PAL 2342); weapon_aim origin @0x23422 */
  };
  union {
    struct {
      float field_12c;                               /* +0x12c  accessed 1x, meaning unproven */
      float field_130;                               /* +0x130  accessed 1x, meaning unproven */
      float field_134;                               /* +0x134  accessed 1x, meaning unproven */
    };
    real_point3d body_position;                      /* +0x12c  actor->input.position.body_position */
  };
  char pad_138[0xc];
  int32_t field_144;                                 /* +0x144  accessed 1x, meaning unproven */
  uint16_t field_148;                                /* +0x148  accessed 1x, meaning unproven */
  char pad_14a[0xe];
  union {
    int32_t field_158;                                 /* +0x158  accessed 10x, meaning unproven */
    int32_t vehicle_index;                             /* +0x158  actor->input.vehicle_index */
  };
  union {
    char field_15c;                                    /* +0x15c  accessed 3x, meaning unproven */
    boolean input_in_midair;                           /* +0x15c  actor->input.in_midair (PAL 2342); _firing_not_in_midair test @0x234ad */
  };
  union {
    char field_15d;                                    /* +0x15d  accessed 2x, meaning unproven */
    boolean input_underwater;                          /* +0x15d  actor->input.underwater (PAL 2342); _firing_underwater test @0x2354a */
  };
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
  char pad_1d6[0x2];
  uint32_t field_1d8;                                /* +0x1d8  prop_status_refresh @0x33562: -1 test, low word vs encounter, >>30 selector, byte +0x1da */
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
  char danger_zone_noticed_danger;                    /* +0x287  assert text 0x254e08 @0x24d86 */
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
  char field_375;                                    /* +0x375  MOV byte [ESI+0x375],1 @0x314a4, meaning unproven */
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
  real_point3d field_3dc;                            /* +0x3dc  12-byte struct copy of the discarded firing position's */
                                                     /*         float point (fld [elt+0/4/8] @0x2666c); meaning unproven */
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
  char pad_45e[0x2];
  real_point3d orders_combat_target_point;          /* +0x460  actor->orders.combat.target_point (PAL 2342); copied to +0x610 @0x22fb0 */
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
  union {
    char field_504;                                    /* +0x504  accessed 10x, meaning unproven */
    boolean control_moving;                            /* +0x504  actor->control.moving (PAL 2342); _firing_not_stationary test @0x23526 */
  };
  char field_505;                                    /* +0x505  accessed 1x, meaning unproven */
  char field_506;                                    /* +0x506  accessed 8x, meaning unproven */
  char field_507;                                    /* +0x507  accessed 1x, meaning unproven */
  boolean control_crouching;                         /* +0x508  actor->control.crouching (PAL 2342); _firing_not_crouching test @0x234e3 */
  char pad_509[0x1];
  int16_t field_50a;                                 /* +0x50a  accessed 1x, meaning unproven */
  float field_50c;                                   /* +0x50c  accessed 3x, meaning unproven */
  float field_510;                                   /* +0x510  accessed 3x, meaning unproven */
  float field_514;                                   /* +0x514  accessed 3x, meaning unproven */
  int32_t field_518;                                 /* +0x518  accessed 1x, meaning unproven */
  int32_t field_51c;                                 /* +0x51c  accessed 1x, meaning unproven */
  int32_t field_520;                                 /* +0x520  accessed 1x, meaning unproven */
  union {
    struct {
      float field_524;                               /* +0x524  accessed 1x, meaning unproven */
      float field_528;                               /* +0x528  accessed 1x, meaning unproven */
      float field_52c;                               /* +0x52c  accessed 1x, meaning unproven */
    };
    real_vector3d control_moving_forced_aim_direction;/* +0x524  actor->control.moving_forced_aim_direction */
  };
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
  char pad_592[0x2];
  float control_face_exactly_oversteer_angle;        /* +0x594  actor->control.face_exactly_oversteer_angle */
  float field_598;                                   /* +0x598  accessed 1x, meaning unproven */
  float field_59c;                                   /* +0x59c  accessed 1x, meaning unproven */
  float field_5a0;                                   /* +0x5a0  accessed 1x, meaning unproven */
  float control_desired_facing_vector[3];             /* +0x5a4  LEA EDI,[ESI+0x5a4] @0x2906b */
  float control_desired_aiming_vector[3];             /* +0x5b0  LEA EBX,[ESI+0x5b0] @0x290d8 */
  float control_desired_looking_vector[3];            /* +0x5bc  LEA EBX,[ESI+0x5bc] @0x2913d */
  union {
    char pad_5c8[0x10];
    uint8_t control_vector_avoidance_clear_times[8][2];/* +0x5c8  actor->control.vector_avoidance_clear_times */
  };
  union {
    int16_t field_5d8;                                 /* +0x5d8  accessed 1x, meaning unproven */
    int16_t control_vector_avoidance_current_direction;/* +0x5d8  actor->control.vector_avoidance_current_direction */
  };
  char pad_5da[0x2];
  char field_5dc;                                    /* +0x5dc  accessed 1x, meaning unproven */
  char pad_5dd[0x13];
  union {
    int16_t field_5f0;                                 /* +0x5f0  accessed 1x, meaning unproven */
    int16_t control_vector_avoidance_sharp_turn_timer; /* +0x5f0  actor->control.vector_avoidance_sharp_turn_timer */
  };
  int16_t control_fire_state;                         /* +0x5f2  MOVSX from word [EBX+0x5f2] @0x237d7, 5-case jump table */
  union {
    int16_t field_5f4;                                 /* +0x5f4  accessed 1x, meaning unproven */
    int16_t control_fire_state_timer;                  /* +0x5f4  actor->control.fire_state_timer (PAL 2342); decremented @0x22e7f */
  };
  union {
    int16_t field_5f6;                                 /* +0x5f6  accessed 1x, meaning unproven */
    int16_t control_burst_disable_timer;               /* +0x5f6  actor->control.burst_disable_timer (PAL 2342); _firing_disabled test @0x23471 */
  };
  union {
    int16_t field_5f8;                                 /* +0x5f8  accessed 1x, meaning unproven */
    int16_t control_trigger_delay_timer;               /* +0x5f8  actor->control.trigger_delay_timer (PAL 2342); set from 30/rof @0x23f63 */
  };
  union {
    int16_t field_5fa;                                 /* +0x5fa  accessed 1x, meaning unproven */
    int16_t control_blocked_communication_timer;       /* +0x5fa  actor->control.blocked_communication_timer (PAL 2342); compared to 0x2d @0x23e3b */
  };
  union {
    struct {
      char field_5fc;                                    /* +0x5fc  accessed 2x, meaning unproven */
      char pad_5fd[0x3];
    };
    struct {
      int16_t control_special_fire_delay;           /* +0x5fc  actor->control.special_fire_delay (PAL 2342); word store @0x23199 */
      int16_t control_special_fire_deny_attempts;   /* +0x5fe  actor->control.special_fire_deny_attempts (PAL 2342); word store @0x231dc */
    };
  };
  char field_600;                                    /* +0x600  accessed 2x, meaning unproven */
  char field_601;                                    /* +0x601  accessed 2x, meaning unproven */
  union {
    char field_602;                                    /* +0x602  accessed 1x, meaning unproven */
    boolean control_overcharging_weapon;               /* +0x602  actor->control.overcharging_weapon (PAL 2342); set @0x23231 */
  };
  union {
    char field_603;                                    /* +0x603  accessed 2x, meaning unproven */
    boolean control_fire_burst_secondary;              /* +0x603  actor->control.fire_burst_secondary (PAL 2342); weapon_aim trigger arg @0x23d2a */
  };
  union {
    char field_604;                                    /* +0x604  accessed 4x, meaning unproven */
    boolean control_next_burst_secondary;              /* +0x604  actor->control.next_burst_secondary (PAL 2342); set @0x231f0 */
  };
  char pad_605[0x3];
  union {
    float field_608;                                   /* +0x608  accessed 3x, meaning unproven */
    real control_weapon_maximum_range;                 /* +0x608  actor->control.weapon_maximum_range (PAL 2342); stored @0x22ff4 */
  };
  int16_t control_current_fire_target_type;           /* +0x60c  CMP word [ESI+0x60c],1 @0x22032; ESI from datum_get on ACTOR_TABLE_PTR @0x22013 */
  char pad_60e[0x2];
  union {
    struct {
      int32_t control_current_fire_target_prop_index;     /* +0x610  MOV [EBX+0x610],EAX after CMP EAX,-1 @0x22f52-0x22f55 */
      float field_614;                                   /* +0x614  accessed 1x, meaning unproven */
      float field_618;                                   /* +0x618  accessed 1x, meaning unproven */
    };
    real_point3d control_current_fire_target_manual_point; /* +0x610  actor->control.current_fire_target_manual_point (PAL 2342); 3-dword copy @0x22fb0 */
  };
  union {
    int32_t field_61c;                                 /* +0x61c  accessed 1x, meaning unproven */
    int32_t control_current_fire_target_timer;         /* +0x61c  actor->control.current_fire_target_timer (PAL 2342); INC @0x22ed5, % 10 @0x23353 */
  };
  boolean control_current_fire_target_visible;       /* +0x620  actor->control.current_fire_target_visible (PAL 2342); store @0x235ee */
  boolean control_current_fire_target_underwater;    /* +0x621  PAL 2342 name; store @0x232a5 */
  boolean control_current_fire_target_superballistic; /* +0x622  PAL 2342 name; weapon_aim arg 5 @0x2341a */
  boolean control_current_fire_target_bombardment;   /* +0x623  PAL 2342 name; store @0x2340a */
  boolean control_current_fire_target_outside_active_area; /* +0x624  PAL 2342 name; pvs bit test @0x232e3 */
  char pad_625[0x1];
  int16_t control_current_fire_target_line_of_sight; /* +0x626  PAL 2342 name; word store @0x23291, ai_test_line_of_sight result @0x23391 */
  union {
    char field_628;                                    /* +0x628  accessed 1x, meaning unproven */
    boolean control_aiming_at_fire_target;             /* +0x628  actor->control.aiming_at_fire_target (PAL 2342); store @0x22fcd/0x23633 */
  };
  char pad_629[0x3];
  union {
    struct {
      int32_t field_62c;                                 /* +0x62c  accessed 1x, meaning unproven */
      float field_630;                                   /* +0x630  accessed 1x, meaning unproven */
      float field_634;                                   /* +0x634  accessed 1x, meaning unproven */
    };
    real_point3d control_current_fire_target_position; /* +0x62c  PAL 2342 name; weapon_aim target @0x2341b */
  };
  union {
    float field_638;                                   /* +0x638  accessed 1x, meaning unproven */
    real control_current_fire_target_range;            /* +0x638  PAL 2342 name; prop->distance copy @0x2326d */
  };
  union {
    struct {
      int32_t field_63c;                                 /* +0x63c  accessed 1x, meaning unproven */
      uint16_t field_640;                                /* +0x640  accessed 1x, meaning unproven */
      char pad_642[0x2];
      int32_t field_644;                                 /* +0x644  accessed 1x, meaning unproven */
    };
    real_vector3d control_current_fire_target_aim_vector; /* +0x63c  PAL 2342 name; weapon_aim out @0x23416 */
  };
  union {
    float field_648;                                   /* +0x648  accessed 1x, meaning unproven */
    real control_current_fire_target_distance;         /* +0x648  PAL 2342 name; weapon_aim out @0x23401 */
  };
  union {
    struct {
      float field_64c;                                   /* +0x64c  accessed 2x, meaning unproven */
      float field_650;                                   /* +0x650  accessed 2x, meaning unproven */
      float field_654;                                   /* +0x654  accessed 2x, meaning unproven */
    };
    real_point3d control_burst_initial_position;      /* +0x64c  PAL 2342 name; copied to +0x658 @0x23973 */
  };
  real_point3d control_burst_origin;                /* +0x658  PAL 2342 name; LEA EDI,[EBX+0x658] @0x2397d */
  union {
    struct {
      float field_664;                                   /* +0x664  accessed 2x, meaning unproven */
      int16_t field_668;                                 /* +0x668  accessed 2x, meaning unproven */
      int16_t field_66a;                                 /* +0x66a  accessed 2x, meaning unproven */
      int16_t field_66c;                                 /* +0x66c  accessed 2x, meaning unproven */
      char pad_66e[0x2];
    };
    real_vector3d control_burst_relative_position;    /* +0x664  PAL 2342 name; FSTP dword x3 @0x23b9c-0x23bc0 */
  };
  union {
    struct {
      float field_670;                                   /* +0x670  accessed 1x, meaning unproven */
      float field_674;                                   /* +0x674  accessed 1x, meaning unproven */
      float field_678;                                   /* +0x678  accessed 1x, meaning unproven */
    };
    real_vector3d control_burst_adjustment;           /* +0x670  PAL 2342 name; FLD x3 @0x23b8a-0x23bb4 */
  };
  union {
    struct {
      float field_67c;                                   /* +0x67c  accessed 1x, meaning unproven */
      float field_680;                                   /* +0x680  accessed 1x, meaning unproven */
      float field_684;                                   /* +0x684  accessed 1x, meaning unproven */
    };
    real_point3d control_burst_target;                /* +0x67c  PAL 2342 name; LEA ESI,[EBX+0x67c] @0x23b90 */
  };
  boolean control_burst_aim_by_vector;              /* +0x688  actor->control.burst_aim_by_vector (PAL 2342); store @0x2392b/0x23dc9 */
  char pad_689[0x3];
  float control_burst_aim_vector[3];                  /* +0x68c  LEA EDI,[EBX+0x68c] @0x23d1a */
  float field_698;                                   /* +0x698  accessed 2x, meaning unproven */
  float field_69c;                                   /* +0x69c  FLD [EDX+0x69c] @0x3f92c/0x3f93f, meaning unproven */
  char field_6a0;                                    /* +0x6a0  accessed 1x, meaning unproven */
  uint8_t field_6a1;                                 /* +0x6a1  accessed 1x, meaning unproven */
  char pad_6a2[0x2];
  int32_t field_6a4;                                 /* +0x6a4  accessed 1x, meaning unproven */
  float field_6a8;                                   /* +0x6a8  accessed 1x, meaning unproven */
  float field_6ac;                                   /* +0x6ac  accessed 1x, meaning unproven */
  float field_6b0;                                   /* +0x6b0  MOV [EDI+0x8] (EDI=actor+0x6a8) @0x22b8e, meaning unproven */
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
co(actor_t, body_position,                                 0x12c);
co(actor_t, input_facing_vector,                           0x174);
co(actor_t, input_aiming_vector,                           0x180);
co(actor_t, input_looking_vector,                          0x18c);
co(actor_t, target_target_type,                            0x268);
co(actor_t, target_target_prop_index,                      0x270);
co(actor_t, danger_zone_danger_type,                       0x280);
co(actor_t, danger_zone_object_index,                      0x28c);
co(actor_t, danger_zone_noticed_danger,                    0x287);
co(actor_t, stimuli_panic_type,                            0x308);
co(actor_t, stimuli_panic_prop_index,                      0x30c);
co(actor_t, firing_positions_current_position_index,       0x3b8);
co(actor_t, field_3dc,                                     0x3dc);
co(actor_t, control_path_destination_orders_ignore_target_object_index, 0x480);
co(actor_t, field_4a0,                                         0x4a0);
co(actor_t, control_moving_forced_aim_direction,           0x524);
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
co(actor_t, control_face_exactly_oversteer_angle,          0x594);
co(actor_t, control_desired_facing_vector,                 0x5a4);
co(actor_t, control_desired_aiming_vector,                 0x5b0);
co(actor_t, control_desired_looking_vector,                0x5bc);
co(actor_t, control_fire_state,                            0x5f2);
co(actor_t, control_current_fire_target_type,              0x60c);
co(actor_t, control_current_fire_target_prop_index,        0x610);
co(actor_t, control_burst_aim_vector,                      0x68c);
co(actor_t, output_facing_vector,                          0x6fc);
co(actor_t, output_aiming_vector,                          0x708);
co(actor_t, meta_unit_index,                               0x018);
co(actor_t, meta_type,                                     0x004);
co(actor_t, meta_team_index,                               0x03e);
co(actor_t, meta_first_prop_index,                         0x050);
co(actor_t, head_position,                                  0x120);
co(actor_t, input_in_midair,                                0x15c);
co(actor_t, input_underwater,                               0x15d);
co(actor_t, orders_combat_target_point,                     0x460);
co(actor_t, control_moving,                                 0x504);
co(actor_t, control_crouching,                              0x508);
co(actor_t, control_fire_state_timer,                       0x5f4);
co(actor_t, control_blocked_communication_timer,            0x5fa);
co(actor_t, control_special_fire_delay,                     0x5fc);
co(actor_t, control_special_fire_deny_attempts,             0x5fe);
co(actor_t, control_overcharging_weapon,                    0x602);
co(actor_t, control_next_burst_secondary,                   0x604);
co(actor_t, control_weapon_maximum_range,                   0x608);
co(actor_t, control_current_fire_target_manual_point,       0x610);
co(actor_t, control_current_fire_target_timer,              0x61c);
co(actor_t, control_current_fire_target_visible,            0x620);
co(actor_t, control_current_fire_target_outside_active_area, 0x624);
co(actor_t, control_current_fire_target_line_of_sight,      0x626);
co(actor_t, control_aiming_at_fire_target,                  0x628);
co(actor_t, control_current_fire_target_position,           0x62c);
co(actor_t, control_current_fire_target_range,              0x638);
co(actor_t, control_current_fire_target_aim_vector,         0x63c);
co(actor_t, control_current_fire_target_distance,           0x648);
co(actor_t, control_burst_initial_position,                 0x64c);
co(actor_t, control_burst_origin,                           0x658);
co(actor_t, control_burst_relative_position,                0x664);
co(actor_t, control_burst_adjustment,                       0x670);
co(actor_t, control_burst_target,                           0x67c);
co(actor_t, control_burst_aim_by_vector,                    0x688);
co(actor_t, vehicle_index,                                 0x158);
co(actor_t, control_vector_avoidance_clear_times,          0x5c8);
co(actor_t, control_vector_avoidance_current_direction,    0x5d8);
co(actor_t, control_vector_avoidance_sharp_turn_timer,     0x5f0);
co(actor_t, output_looking_vector,                         0x714);
#pragma pack()

#define MAXIMUM_NUMBER_OF_AVOIDANCE_OBJECTS 1024

typedef struct vehicle_avoidance_cylinder_t {
  int32_t object_index;            ///< offset=0x00
  real_point3d base;               ///< offset=0x04
  float height;                    ///< offset=0x10
  float width;                     ///< offset=0x14
} vehicle_avoidance_cylinder_t;
cs(vehicle_avoidance_cylinder_t, 0x18);
co(vehicle_avoidance_cylinder_t, object_index, 0x00);
co(vehicle_avoidance_cylinder_t, base, 0x04);
co(vehicle_avoidance_cylinder_t, height, 0x10);
co(vehicle_avoidance_cylinder_t, width, 0x14);

typedef struct vector_avoidance_data_t {
  const void *structure;                                                      ///< offset=0x00
  const void *bsp;                                                            ///< offset=0x04
  int32_t object_index;                                                       ///< offset=0x08
  real_point3d origin;                                                        ///< offset=0x0c
  real_vector3d forward;                                                      ///< offset=0x18
  real_vector3d left;                                                         ///< offset=0x24
  real_vector3d up;                                                           ///< offset=0x30
  int16_t avoidance_object_count;                                             ///< offset=0x3c
  char pad_3e[2];                                                             ///< offset=0x3e
  vehicle_avoidance_cylinder_t avoidance_objects[MAXIMUM_NUMBER_OF_AVOIDANCE_OBJECTS]; ///< offset=0x40
  float avoid_width;                                                          ///< offset=0x6040
  float avoid_distance;                                                       ///< offset=0x6044
} vector_avoidance_data_t;
cs(vector_avoidance_data_t, 0x6048);
co(vector_avoidance_data_t, structure, 0x00);
co(vector_avoidance_data_t, bsp, 0x04);
co(vector_avoidance_data_t, object_index, 0x08);
co(vector_avoidance_data_t, origin, 0x0c);
co(vector_avoidance_data_t, forward, 0x18);
co(vector_avoidance_data_t, left, 0x24);
co(vector_avoidance_data_t, up, 0x30);
co(vector_avoidance_data_t, avoidance_object_count, 0x3c);
co(vector_avoidance_data_t, avoidance_objects, 0x40);
co(vector_avoidance_data_t, avoid_width, 0x6040);
co(vector_avoidance_data_t, avoid_distance, 0x6044);

typedef struct vector_avoidance_ray_t {
  float length;                    ///< offset=0x00
  real_vector3d offset;            ///< offset=0x04
  real_vector3d divergence;        ///< offset=0x10
} vector_avoidance_ray_t;
cs(vector_avoidance_ray_t, 0x1c);
co(vector_avoidance_ray_t, length, 0x00);
co(vector_avoidance_ray_t, offset, 0x04);
co(vector_avoidance_ray_t, divergence, 0x10);

typedef struct actor_debug_info_t {
  /* Historical PAL names are recorded in the provenance ledger. Widths
   * retained for compatibility; neutral names do not establish target proof. */
  int32_t field_00;                          ///< offset=0x0000
  int32_t field_04;                          ///< offset=0x0004
  int16_t firing_decision;                   ///< offset=0x0008  word store @0x23fcd (actor_combat_update)
  char pad_000a[0x2];                        ///< offset=0x000a
  real shooting_rof;                         ///< offset=0x000c  FST @0x23f3f
  real_point3d burst_last_known_position;    ///< offset=0x0010
  real_point3d burst_tracked_position;       ///< offset=0x001c
  real_vector3d burst_lead_vector;           ///< offset=0x0028
  int32_t burst_alignment_time;              ///< offset=0x0034  game_time_get() @0x23642
  boolean burst_alignment_aligned;           ///< offset=0x0038
  boolean burst_alignment_aligned_immediately; ///< offset=0x0039
  char pad_003a[0x2];                        ///< offset=0x003a
  real_vector3d burst_alignment_weapon_vector; ///< offset=0x003c
  real_vector3d burst_alignment_aim_vector;  ///< offset=0x0048
  real burst_alignment_threshold;            ///< offset=0x0054
  real burst_alignment_alignment;            ///< offset=0x0058
  char pad_005c[0x140];                      ///< offset=0x005c
  uint32_t timestamp;                                                           ///< offset=0x019c
  vector_avoidance_data_t avoidance_data;                                       ///< offset=0x01a0
  int16_t avoidance_type[9];                                                    ///< offset=0x61e8
  char pad_61fa[2];                                                             ///< offset=0x61fa
  float collision_t[9];                                                         ///< offset=0x61fc
  real_point3d ray_origin[9];                                                   ///< offset=0x6220
  real_vector3d ray_direction[9];                                               ///< offset=0x628c
  int16_t avoidance_types_2[8][2];                                              ///< offset=0x62f8
  float avoid_t[8][2];                                                          ///< offset=0x6318
  real_point3d probe_origin[8][2];                                              ///< offset=0x6358
  real_vector3d probe_dir[8][2];                                                ///< offset=0x6418
  float weights[8];                                                             ///< offset=0x64d8
  uint32_t pad_64f8;                                                            ///< offset=0x64f8
  float best_weight;                                                            ///< offset=0x64fc
  int16_t best_avoidance_direction;                                             ///< offset=0x6500
  char pad_6502[2];                                                             ///< offset=0x6502
  float movement_direction_approximation;                                       ///< offset=0x6504
  float movement_approximate_weight;                                            ///< offset=0x6508
  float sign_no_danger;                                                         ///< offset=0x650c
  float forward_dot;                                                            ///< offset=0x6510
  float sign_too_far_cosangle;                                                  ///< offset=0x6514
  float sign_rotated;                                                           ///< offset=0x6518
  float maximum_sense_emergency;                                                ///< offset=0x651c
  float rotation_angle;                                                         ///< offset=0x6520
  real_vector3d forward;                                                        ///< offset=0x6524
  real_vector3d requested_facing;                                               ///< offset=0x6530
  int16_t debug_mode;                                                           ///< offset=0x653c
  char pad_653e[2];                                                             ///< offset=0x653e
  real_vector3d rotation;                                                       ///< offset=0x6540
  float emergency;                                                              ///< offset=0x654c
  uint8_t direction_chosen;                                                     ///< offset=0x6550
  uint8_t has_emergency_velocity;                                               ///< offset=0x6551
  char pad_6552[2];                                                             ///< offset=0x6552
  float velocity_weight;                                                        ///< offset=0x6554
  float angular_speed;                                                          ///< offset=0x6558
  real_vector3d avoidance_vector;                                               ///< offset=0x655c
  float velocity_approximate_weight;                                            ///< offset=0x6568
  char pad_656c[0x10];                                                          ///< offset=0x656c
} actor_debug_info_t;
cs(actor_debug_info_t, 0x657c);
co(actor_debug_info_t, firing_decision, 0x08);
co(actor_debug_info_t, burst_alignment_time, 0x34);
co(actor_debug_info_t, burst_alignment_alignment, 0x58);
co(actor_debug_info_t, timestamp, 0x19c);
co(actor_debug_info_t, avoidance_data, 0x1a0);
co(actor_debug_info_t, avoidance_type, 0x61e8);
co(actor_debug_info_t, collision_t, 0x61fc);
co(actor_debug_info_t, ray_origin, 0x6220);
co(actor_debug_info_t, ray_direction, 0x628c);
co(actor_debug_info_t, avoidance_types_2, 0x62f8);
co(actor_debug_info_t, avoid_t, 0x6318);
co(actor_debug_info_t, probe_origin, 0x6358);
co(actor_debug_info_t, probe_dir, 0x6418);
co(actor_debug_info_t, weights, 0x64d8);
co(actor_debug_info_t, best_weight, 0x64fc);
co(actor_debug_info_t, best_avoidance_direction, 0x6500);
co(actor_debug_info_t, movement_direction_approximation, 0x6504);
co(actor_debug_info_t, movement_approximate_weight, 0x6508);
co(actor_debug_info_t, sign_no_danger, 0x650c);
co(actor_debug_info_t, forward_dot, 0x6510);
co(actor_debug_info_t, sign_too_far_cosangle, 0x6514);
co(actor_debug_info_t, sign_rotated, 0x6518);
co(actor_debug_info_t, maximum_sense_emergency, 0x651c);
co(actor_debug_info_t, rotation_angle, 0x6520);
co(actor_debug_info_t, forward, 0x6524);
co(actor_debug_info_t, requested_facing, 0x6530);
co(actor_debug_info_t, debug_mode, 0x653c);
co(actor_debug_info_t, rotation, 0x6540);
co(actor_debug_info_t, emergency, 0x654c);
co(actor_debug_info_t, direction_chosen, 0x6550);
co(actor_debug_info_t, has_emergency_velocity, 0x6551);
co(actor_debug_info_t, velocity_weight, 0x6554);
co(actor_debug_info_t, angular_speed, 0x6558);
co(actor_debug_info_t, avoidance_vector, 0x655c);
co(actor_debug_info_t, velocity_approximate_weight, 0x6568);

typedef struct object_datum_t {
  int32_t definition_index;                             ///< offset=0x00
  uint32_t flags;                                       ///< offset=0x04
  int32_t magic_number;                                 ///< offset=0x08
  real_point3d position;                                ///< offset=0x0c
  real_vector3d translational_velocity;                 ///< offset=0x18
  real_vector3d forward;                                ///< offset=0x24
  real_vector3d up;                                     ///< offset=0x30
  real_vector3d angular_velocity;                       ///< offset=0x3c
  char pad_48[0x88 - 0x48];                             ///< offset=0x48
  real vitality[4];                                     ///< offset=0x88, indexed by OBJECT_VITALITY_*
  char pad_98[0xb6 - 0x98];                             ///< offset=0x98
  uint8_t damage_flags;                                 ///< offset=0xb6 (bit 2 dead)
  char pad_b7[0xcc - 0xb7];                             ///< offset=0xb7
  int32_t parent_object_index;                          ///< offset=0xcc
  char pad_d0[0x1a4 - 0xd0];                            ///< offset=0xd0
} object_datum_t;
cs(object_datum_t, 0x1a4);
co(object_datum_t, vitality, 0x88);
co(object_datum_t, damage_flags, 0xb6);
#define OBJECT_VITALITY_MAXIMUM_BODY   0
#define OBJECT_VITALITY_MAXIMUM_SHIELD 1
#define OBJECT_VITALITY_BODY           2
#define OBJECT_VITALITY_SHIELD         3
co(object_datum_t, parent_object_index, 0xcc);

/* Biped object datum.  Only the biped flags dword is modelled; the span
 * between the object header and it is unobserved.
 *   +0x424 flags — biped_accelerate (0x1a4a70) ORs 3 (airborne | slipping). */
typedef struct biped_datum_t {
  object_datum_t object;                                ///< offset=0x00
  char pad_1a4[0x424 - 0x1a4];                          ///< offset=0x1a4
  uint32_t flags;                                       ///< offset=0x424
} biped_datum_t;
co(biped_datum_t, flags, 0x424);
co(object_datum_t, definition_index, 0x00);
co(object_datum_t, position, 0x0c);
co(object_datum_t, translational_velocity, 0x18);
co(object_datum_t, forward, 0x24);
co(object_datum_t, up, 0x30);
co(object_datum_t, angular_velocity, 0x3c);

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
/* tag_block is defined above scenario_t (it embeds one). */

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
/*
 * The four blocks below +0x3c are proven by the pill sweep
 * (collision_bsp_test_pill 0x149680 / bsp2d_test_pill_recursive 0x149570):
 * +0x00 bsp3d nodes (stride 0xc), +0x18 leaves (8), +0x24 bsp2d references (8),
 * +0x30 bsp2d nodes (0x14). Names follow the CEA test_pill_data sweep (T2).
 */
typedef struct collision_bsp_t {
  tag_block bsp3d_nodes;            ///< offset=0x00
  tag_block planes;                 ///< offset=0x0c
  tag_block leaves;                 ///< offset=0x18
  tag_block bsp2d_references;       ///< offset=0x24
  tag_block bsp2d_nodes;            ///< offset=0x30
  tag_block surfaces;               ///< offset=0x3c
  tag_block edges;                  ///< offset=0x48
  tag_block vertices;               ///< offset=0x54
} collision_bsp_t;
cs(collision_bsp_t, 0x60);
co(collision_bsp_t, bsp3d_nodes, 0x00);
co(collision_bsp_t, planes, 0x0c);
co(collision_bsp_t, leaves, 0x18);
co(collision_bsp_t, bsp2d_references, 0x24);
co(collision_bsp_t, bsp2d_nodes, 0x30);
co(collision_bsp_t, surfaces, 0x3c);
co(collision_bsp_t, edges, 0x48);
co(collision_bsp_t, vertices, 0x54);

typedef struct collision_bsp_test_vector_result_t {
  float t;                          ///< offset=0x00
  const real_plane3d *plane;        ///< offset=0x04
  int32_t surface_index;            ///< offset=0x08
  int32_t plane_designator;         ///< offset=0x0c
  uint8_t flags;                    ///< offset=0x10
  uint8_t breakable_surface_index;  ///< offset=0x11
  int16_t material_index;           ///< offset=0x12
  int32_t leaf_count;               ///< offset=0x14
  int32_t leaf_indices[256];        ///< offset=0x18
} collision_bsp_test_vector_result_t;
cs(collision_bsp_test_vector_result_t, 0x418);
co(collision_bsp_test_vector_result_t, t, 0x00);
co(collision_bsp_test_vector_result_t, plane, 0x04);
co(collision_bsp_test_vector_result_t, surface_index, 0x08);
co(collision_bsp_test_vector_result_t, plane_designator, 0x0c);
co(collision_bsp_test_vector_result_t, flags, 0x10);
co(collision_bsp_test_vector_result_t, breakable_surface_index, 0x11);
co(collision_bsp_test_vector_result_t, material_index, 0x12);
co(collision_bsp_test_vector_result_t, leaf_count, 0x14);
co(collision_bsp_test_vector_result_t, leaf_indices, 0x18);

typedef struct collision_surface_t {
  int32_t plane;                    ///< offset=0x00
  int32_t first_edge;               ///< offset=0x04
  uint8_t flags;                    ///< offset=0x08
  uint8_t pad_09[1];
  int16_t material_index;           ///< offset=0x0a (word copy into the pill result +0x1a; CEA name, T2)
} collision_surface_t;
cs(collision_surface_t, 0x0c);
co(collision_surface_t, plane, 0x00);
co(collision_surface_t, first_edge, 0x04);
co(collision_surface_t, flags, 0x08);
co(collision_surface_t, material_index, 0x0a);

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

/* bsp3d node (stride 0xc): plane index, then back/front child. A negative
 * child is a leaf (bit 31 flag; -1 = none). collision_bsp_test_pill 0x149680. */
typedef struct bsp3d_node_t {
  int32_t plane;                    ///< offset=0x00
  int32_t children[2];              ///< offset=0x04 (0 = back, 1 = front)
} bsp3d_node_t;
cs(bsp3d_node_t, 0x0c);
co(bsp3d_node_t, plane, 0x00);
co(bsp3d_node_t, children, 0x04);

/* Collision leaf (stride 8): MOVSX word +0x02 count, dword +0x04 first. */
typedef struct collision_leaf_t {
  uint8_t pad_00[2];
  int16_t bsp2d_reference_count;    ///< offset=0x02
  int32_t first_bsp2d_reference;    ///< offset=0x04
} collision_leaf_t;
cs(collision_leaf_t, 0x08);
co(collision_leaf_t, bsp2d_reference_count, 0x02);
co(collision_leaf_t, first_bsp2d_reference, 0x04);

/* bsp2d reference (stride 8): signed plane index (bit 31 = flipped plane) and
 * the root of the leaf's 2D BSP. */
typedef struct bsp2d_reference_t {
  int32_t plane;                    ///< offset=0x00
  int32_t bsp2d_root;               ///< offset=0x04
} bsp2d_reference_t;
cs(bsp2d_reference_t, 0x08);
co(bsp2d_reference_t, plane, 0x00);
co(bsp2d_reference_t, bsp2d_root, 0x04);

/* bsp2d node (stride 0x14): 2D line (i, j, d), then back/front child. A
 * negative child is a surface index (bit 31 flag). */
typedef struct bsp2d_node_t {
  real    plane[3];                 ///< offset=0x00
  int32_t children[2];              ///< offset=0x0c (0 = back, 1 = front)
} bsp2d_node_t;
cs(bsp2d_node_t, 0x14);
co(bsp2d_node_t, plane, 0x00);
co(bsp2d_node_t, children, 0x0c);

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

// OBJE -> UNIT -> VEHI
/* Vehicle object datum. Size 0x47c is the datum size (short at +0x8) of the
 * vehicle object_type_definition at 0x323de8. Every field below has an
 * observed access (cited); names follow the field's use in vehicles.obj. */
#define OBJECT_MASK_VEHICLE 0x2 /* 1 << vehicle object type */
typedef struct vehicle_data_t {
  unit_data_t unit;               ///< offset=0x000
  uint16_t flags;                 ///< offset=0x424 .text:001B578E mov [esi+424h], bx
  int16_t stop_time;              ///< offset=0x426 .text:001B9819 cmp word ptr [ebx+426h], 0
  uint8_t airborne_ticks;         ///< offset=0x428 .text:001A2020 cmp byte ptr [ebx+428h], 1Eh
  uint8_t upending_type;          ///< offset=0x429 .text:001B57A2 mov [esi+429h], bl
  uint8_t upending_ticks;         ///< offset=0x42a .text:001B57A8 mov [esi+42Ah], bl
  uint8_t on_ground_ticks;        ///< offset=0x42b .text:001B572C mov al, [esi+42Bh]
  real speed;                     ///< offset=0x42c .text:001B6025 fld [esi+42Ch]
  real slide;                     ///< offset=0x430 .text:001B7B31 fld [esi+430h]
  real turn;                      ///< offset=0x434 .text:001B602B fsub [esi+434h]
  real wheel;                     ///< offset=0x438 .text:001B5BA9 fld [esi+438h]
  real left_tread;                ///< offset=0x43c .text:001B604C fadd [esi+43Ch]
  real right_tread;               ///< offset=0x440 .text:001B608F fadd [esi+440h]
  real hover;                     ///< offset=0x444 .text:0002EAA8 fld [edi+444h]
  real thrust;                    ///< offset=0x448 .text:001B6860 fcomp [edi+448h]
  uint8_t suspension[8];          ///< offset=0x44c .text:001B5C28 mov cl, [edi+esi+44Ch]
  real_point3d hover_position;    ///< offset=0x454 .text:001B5631 lea eax, [esi+454h]
  real field_460[6];              ///< offset=0x460 .text:0015225F..001522AA fadd [edi+460h..474h]
  uint32_t stuck_mass_point_flags; ///< offset=0x478 .text:001B80DA test [ebx+478h], edx
} vehicle_data_t;
cs(vehicle_data_t, 0x47c);
co(vehicle_data_t, flags, 0x424);
co(vehicle_data_t, speed, 0x42c);
co(vehicle_data_t, turn, 0x434);
co(vehicle_data_t, left_tread, 0x43c);
co(vehicle_data_t, right_tread, 0x440);
co(vehicle_data_t, hover_position, 0x454);
co(vehicle_data_t, stuck_mass_point_flags, 0x478);

/* 'vehi' tag definition. The total size is not verified. +0x80 is the
 * object definition's physics reference (its tag_index at +0x8c is passed to
 * tag_get('phys') by every vehicles.obj physics routine); +0x2f4 is the
 * vehicle type switched on by vehicle_update's jump table at 0x1b9870;
 * +0x310 is the tread/wheel wrap divisor used by fmod in 0x1b5ff0/0x1b6140. */
typedef struct vehicle_definition_t {
  char pad_000[0x80];                 ///< offset=0x000
  tag_reference physics;              ///< offset=0x080
  char pad_090[0x2f0 - 0x90];         ///< offset=0x090
  uint32_t flags;                     ///< offset=0x2f0
  int16_t vehicle_type;               ///< offset=0x2f4
  char pad_2f6[2];                    ///< offset=0x2f6
  real field_2f8[4];                  ///< offset=0x2f8 speed-seek block (vehicle_update)
  real field_308;                     ///< offset=0x308 turn-seek block (vehicle_update)
  real field_30c;                     ///< offset=0x30c
  real wheel_circumference;           ///< offset=0x310
  real field_314;                     ///< offset=0x314 angular-rate scale (0x1b67c2, 0x1b6857)
  char pad_318[0x364 - 0x318];        ///< offset=0x318
  real field_364;                     ///< offset=0x364 pitch angle, FSIN/FCOS at 0x1b66e4
  char pad_368[0x3e0 - 0x368];        ///< offset=0x368
  tag_reference effect;               ///< offset=0x3e0 thruster-wash effect (index read at +0x3ec by 0x1b6e20/0x1b7020)
} vehicle_definition_t;
co(vehicle_definition_t, physics, 0x80);
co(vehicle_definition_t, flags, 0x2f0);
co(vehicle_definition_t, vehicle_type, 0x2f4);
co(vehicle_definition_t, wheel_circumference, 0x310);
co(vehicle_definition_t, field_314, 0x314);
co(vehicle_definition_t, field_364, 0x364);
co(vehicle_definition_t, effect, 0x3e0);
co(tag_reference, tag_index, 0x0c);

/// size=0x1ac. global_rasterizer_data (0x476204): element 0 of the game
/// globals rasterizer_data block, set by rasterizer_initialize_for_new_map
/// (tag_block_get_element(game_globals + 0x134, 0, 0x1ac)); the binary's
/// "global_rasterizer_data" assert string names the pointer. Names follow
/// PAL 2342 struct game_globals_rasterizer_data (T2); every named field is a
/// tag_index read in 2276: +0x1c / +0x4c (_rasterizer_environment_shadow_draw
/// stages 2 / 1), +0x2c / +0x3c (environment fog), +0xb8 + type * 0x10
/// (rasterizer_xbox.c default textures), +0x128 / +0x138 (rasterizer_sprites.c).
typedef struct {
  byte pad_00[0x10];                             ///< offset=0x00
  tag_reference vector_normalization;            ///< offset=0x10
  tag_reference atmospheric_fog_density;         ///< offset=0x20
  tag_reference planar_fog_density;              ///< offset=0x30
  tag_reference linear_corner_fade;              ///< offset=0x40
  byte pad_50[0x5c];                             ///< offset=0x50
  tag_reference default_textures[3];             ///< offset=0xac
  byte pad_dc[0x40];                             ///< offset=0xdc
  tag_reference screen_effect_video_scanline_map; ///< offset=0x11c
  tag_reference screen_effect_video_noise_map;   ///< offset=0x12c
  byte pad_13c[0x70];                            ///< offset=0x13c
} game_globals_rasterizer_data;
cs(game_globals_rasterizer_data, 0x1ac);
co(game_globals_rasterizer_data, vector_normalization, 0x10);
co(game_globals_rasterizer_data, linear_corner_fade, 0x40);
co(game_globals_rasterizer_data, default_textures, 0xac);
co(game_globals_rasterizer_data, screen_effect_video_scanline_map, 0x11c);

/* scenario_t::structure_bsp_references element; stride 0x20 from
 * global_structure_bsp_tag_index_get (0x18e480), which reads +0x1c. */
typedef struct {
  uint8_t pad_00[0x10];                    ///< offset=0x00  not accessed here
  tag_reference structure_bsp;             ///< offset=0x10
} scenario_structure_bsp_reference_t;
cs(scenario_structure_bsp_reference_t, 0x20);
co(scenario_structure_bsp_reference_t, structure_bsp, 0x10);

/* ---------------------------------------------------------------------------
 * Virtual keyboard ('vcky' tag + runtime globals). Field names from
 * interface/virtual_keyboard.c (T2); layout checked against the 2276 binary:
 *  - virtual_keyboard_get_character (0xf5800) reads the seven character words
 *    at +0x02..+0x0e of a 0x50-stride key (MOVZX key; LEA *5; SHL 4).
 *  - virtual_keyboard_render_internal (0xf5900) reads definition +0x0c/+0x1c/
 *    +0x2c (tag_reference.tag_index of the three references), +0x34 (keys
 *    block address) and key +0x1c/+0x2c/+0x3c/+0x4c (background tag indices).
 * ------------------------------------------------------------------------- */
/// size=0x50
typedef struct {
  char pad_00[2];                          /* +0x00 keycode; not read in 2276 lifts */
  wchar_t character;                       /* +0x02 */
  wchar_t shift_character;                 /* +0x04 */
  wchar_t caps_character;                  /* +0x06 */
  wchar_t symbols_character;               /* +0x08 */
  wchar_t shift_caps_character;            /* +0x0a */
  wchar_t shift_symbols_character;         /* +0x0c */
  wchar_t caps_symbols_character;          /* +0x0e */
  tag_reference unselected_background_bitmap_tag; /* +0x10 */
  tag_reference selected_background_bitmap_tag;   /* +0x20 */
  tag_reference active_background_bitmap_tag;     /* +0x30 */
  tag_reference sticky_background_bitmap_tag;     /* +0x40 */
} virtual_keyboard_key_t;
cs(virtual_keyboard_key_t, 0x50);
co(virtual_keyboard_key_t, character, 0x02);
co(virtual_keyboard_key_t, caps_symbols_character, 0x0e);
co(virtual_keyboard_key_t, unselected_background_bitmap_tag, 0x10);
co(virtual_keyboard_key_t, sticky_background_bitmap_tag, 0x40);

/// size=0x3c
typedef struct {
  tag_reference font_tag;                           /* +0x00 */
  tag_reference background_bitmap_tag;              /* +0x10 */
  tag_reference special_key_labels_string_list_tag; /* +0x20 */
  tag_block keys;                                   /* +0x30 virtual_keyboard_key_t[] */
} virtual_keyboard_definition_t;
cs(virtual_keyboard_definition_t, 0x3c);
co(virtual_keyboard_definition_t, background_bitmap_tag, 0x10);
co(virtual_keyboard_definition_t, special_key_labels_string_list_tag, 0x20);
co(virtual_keyboard_definition_t, keys, 0x30);

/* Runtime state at 0x46cef0 (virtual_keyboard_globals, 0x68 bytes). Every field is read or written by a lifted virtual_keyboard_* body in
 * src/halo/items/items.c. */
/// size=0x68
typedef struct {
  boolean active;                           /* +0x00 */
  boolean shift_active;                     /* +0x01 */
  boolean caps_active;                      /* +0x02 */
  boolean symbols_active;                   /* +0x03 */
  virtual_keyboard_definition_t *keyboard;  /* +0x04 */
  int16_t row;                              /* +0x08 */
  int16_t column;                           /* +0x0a */
  uint16_t buffer_size;                     /* +0x0c bytes (MOVZX loads) */
  int16_t last_event;                       /* +0x0e */
  int16_t last_key;                         /* +0x10 */
  int16_t number_of_event_repeats;          /* +0x12 */
  uint16_t caption_index;                   /* +0x14 zero-extended at 0xf5a43 */
  boolean last_exit_saved_text;             /* +0x16 */
  boolean first_key_replaces_buffer;        /* +0x17 */
  wchar_t *text_buffer;                     /* +0x18 */
  wchar_t *cursor;                          /* +0x1c */
  uint32_t time_of_last_event;              /* +0x20 */
  int32_t caret_bitmap_index;               /* +0x24 */
  wchar_t saved_text[32];                   /* +0x28 */
} virtual_keyboard_globals_t;
cs(virtual_keyboard_globals_t, 0x68);
co(virtual_keyboard_globals_t, keyboard, 0x04);
co(virtual_keyboard_globals_t, last_event, 0x0e);
co(virtual_keyboard_globals_t, caption_index, 0x14);
co(virtual_keyboard_globals_t, first_key_replaces_buffer, 0x17);
co(virtual_keyboard_globals_t, caret_bitmap_index, 0x24);
co(virtual_keyboard_globals_t, saved_text, 0x28);


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
  char  pad_00[0x4];    /* +0x00  never observed accessed */
  uint32_t field_04;    /* +0x04  trigger_create_projectiles ORs bit 2
                         *        (OR EAX,2 at .text:000FDBF2) */
  char  pad_08[0x10];   /* +0x08  never observed accessed */
  float position_x;     /* +0x18  object_new copies this to object_data+0x0C
                         *        (MOV ECX,[ESI+0x18] in the object_new
                         *        disassembly), i.e. the spawn position. Some
                         *        callers copy the three words with int casts
                         *        (bit copies), which is why the float type is
                         *        taken from object_new, not from the casts. */
  float position_y;     /* +0x1c */
  float position_z;     /* +0x20 */
  char  pad_24[0x4];    /* +0x24  never observed accessed */
  float field_28[3];    /* +0x28  trigger_create_projectiles stores
                         *        forward * speed (.text:000FDBB5-000FDBD3) */
  vector3_t forward;    /* +0x34  object_placement_data_new default {1,0,0} */
  vector3_t up;         /* +0x40  object_placement_data_new default {0,0,1} */
  char  pad_4c[0x3c];   /* +0x4c */
} object_placement_data;
cs(object_placement_data, 0x88);
co(object_placement_data, field_04,   0x04);
co(object_placement_data, position_x, 0x18);
co(object_placement_data, field_28,   0x28);
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
 * 16-byte span is inferred from address_length sitting at +0x10. The 24-byte
 * size and final dword are proven by FUN_00084520's bind-address initializer
 * at 0x845dc-0x845fb. See recovery/evidence/transport_address.json.
 * ------------------------------------------------------------------------- */
#define IPV4_ADDRESS_LENGTH 4 /* T1: assert text; stored as 4 at 0x84702 */
#define IPV4_LOOPBACK_ADDRESS 0x7f000001 /* 127.0.0.1; compared in network_game_server_add_new_client */

typedef struct transport_address {
    /* ipv4_address is T1: assert text "address->address.ipv4_address"
     * (network_client_manager.c 0x2d5). The union layout and the words/bytes
     * names come from the PAL 2342 reference (transport.h), T2. Our binary
     * agrees: FUN_00084520 zeroes a transport_address with a dword store at
     * +0x00, so the first member is 32 bits wide, and transport_address.c
     * reads the block as 8 words (IPv6) or 4 bytes (IPv4). */
    union {
        uint32_t ipv4_address;
        uint16_t words[8];
        uint8_t  bytes[0x10];
    } address;                /* +0x00: address bytes, compared as a block */
    uint16_t address_length;  /* +0x10: asserted == IPV4_ADDRESS_LENGTH */
    uint16_t port;            /* +0x12: compared as a 16-bit value */
    uint32_t field_14;        /* +0x14: dword cleared at FUN_00084520:0x845f8 */
} transport_address;
cs(transport_address, 0x18);
co(transport_address, address,        0x00);
co(transport_address, address_length, 0x10);
co(transport_address, port,           0x12);
co(transport_address, field_14,       0x14);

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

/* transport_endpoint.flags bit numbers. Order from the PAL 2342 reference
 * (transport_endpoint.h), T2. Our binary agrees: "!endpoint_connected(ep)"
 * (0x266d58, T1) tests bit 0 at 0x84633; a read error clears bit 2
 * (AND 0xfb, 0x846b7); a lost connection clears bits 0 and 2 (AND 0xfa,
 * 0x846a8). */
enum {
    _transport_endpoint_connected_bit = 0,
    _transport_endpoint_listening_bit,
    _transport_endpoint_readable_bit,
    _transport_endpoint_in_set_bit,
    _transport_endpoint_nonblocking_bit
};

/* Transport-layer result/error codes.
 *
 * Confirmed: the member NAMES are exact -- each one is the .rdata string the
 * matching case returns (0x26611c-0x266438), and the leading underscore is
 * Bungie's usual enum-constant spelling, so the original almost certainly
 * stringized the constant rather than hand-typing a parallel literal table.
 * Confirmed: the VALUES are exact -- the dispatch biases the selector by +23
 * and indexes a 24-entry jump table at 0x81d4c, so table index i selects
 * selector (i - 23): index 0 is _transport_result_connect_in_progress (-23)
 * and index 0x17 is _transport_error_none (0).
 * Uncertain: the enum's TYPE name is not recoverable from the binary (no
 * assert or string names it), so this stays an anonymous enum rather than
 * inventing one. */
enum {
    _transport_error_none = 0,
    _transport_error_unknown = -1,
    _transport_error_endpoint_io = -2,
    _transport_error_connection_lost = -3,
    _transport_result_operation_would_block = -4,
    _transport_error_not_initialized = -5,
    _transport_result_already_initialized = -6,
    _transport_error_bad_input_parameters = -7,
    _transport_error_dns_lookup_failure = -8,
    _transport_error_out_of_memory = -9,
    _transport_error_seg_fault = -10,
    _transport_error_buffers_full = -11,
    _transport_error_bad_endpoint = -12,
    _transport_result_poll_timeout = -13,
    _transport_error_bind_endpoint = -14,
    _transport_error_address_unknown = -15,
    _transport_error_connect_failed = -16,
    _transport_error_listen_failed = -17,
    _transport_error_options_failed = -18,
    _transport_error_endpoint_not_in_set = -19,
    _transport_error_endpoint_set_full = -20,
    _transport_error_poll_error = -21,
    _transport_result_dns_lookup_in_progress = -22,
    _transport_result_connect_in_progress = -23
};

/* Standard Winsock values used by the transport layer. Each one appears as
 * an immediate in FUN_00084520: socket(AF_INET, SOCK_DGRAM, 0) at 0x845c3,
 * and the error switch over WSAGetLastError() is based at 0x2733 (10035). */
#define INVALID_SOCKET  (-1)
#define SOCKET_ERROR    (-1)
#define AF_INET         2
#define SOCK_DGRAM      2
#define WSAEWOULDBLOCK  10035
#define WSAENETRESET    10052
#define WSAECONNABORTED 10053
#define WSAECONNRESET   10054
#define WSAENOTCONN     10057
#define WSAESHUTDOWN    10058
#define WSAETIMEDOUT    10060

/* Winsock sockaddr_in (16 bytes). FUN_00084520 reads sin_port at -0x12 and
 * sin_addr at -0x10 of a 16-byte frame slot based at -0x14. */
typedef struct in_addr {
    uint32_t s_addr;
} in_addr;

typedef struct sockaddr_in {
    int16_t  sin_family;    ///< offset=0x00
    uint16_t sin_port;      ///< offset=0x02  network byte order
    in_addr  sin_addr;      ///< offset=0x04  network byte order
    uint8_t  sin_zero[8];   ///< offset=0x08
} sockaddr_in;
cs(sockaddr_in, 0x10);
co(sockaddr_in, sin_port, 0x02);
co(sockaddr_in, sin_addr, 0x04);

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
/// The evidence-table schema cannot express the nested matrix, so this layout
/// is maintained here by hand; the artifact keeps the per-field evidence.
/// matrix: lightning_submit (0x135510) passes marker+0x38 as the real_matrix4x3
/// to matrix4x3_transform_vector (via lightning_offset_marker_position) and
/// copies matrix.position from +0x60; glow reads forward (+0x3c) and up (+0x54).
typedef struct object_marker {
    uint8_t pad_00[0x38];  ///< offset=0x00  node_index / node_matrix: not accessed by recovered code
    real_matrix4x3 matrix; ///< offset=0x38  name: T2
} object_marker;
cs(object_marker, 0x6C);
co(object_marker, pad_00, 0x00);
co(object_marker, matrix, 0x38);

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

/* render (0x506540).  Prefix only: the fields below are the ones a 2276 access
 * proves; everything past camera.forward is not yet recovered, so there is no
 * size assert.  Names: T2. */
typedef struct {
  int32_t frame_index;        ///< offset=0x00  INC dword [0x506540] in render.c
  uint8_t pad_04[4];          ///< offset=0x04
  int16_t local_player_index; ///< offset=0x08  CMP [0x506548] in hud.c; assert "render.local_player_index" (event_manager.c)
  uint8_t pad_0a[6];          ///< offset=0x0a
  struct {
    real_point3d position;    ///< offset=0x10
    real_vector3d forward;    ///< offset=0x1c  FMUL [0x50655c/60/64] cross product in lightning_submit (0x135847)
  } camera;
} render_globals_t;
co(render_globals_t, frame_index, 0x00);
co(render_globals_t, local_player_index, 0x08);
co(render_globals_t, camera, 0x10);

/* rasterizer_globals (0x325650).  Prefix only; later fields not yet recovered.
 * Names: T2. */
typedef struct {
  boolean initialized;            ///< offset=0x00  MOV byte [0x325650],1 @0x15790e
  uint8_t pad_01[1];              ///< offset=0x01
  int16_t current_lock_operation; ///< offset=0x02  MOV word [0x325652],0xc / 0 around lightning geometry (0x135ac6, 0x135ede)
} rasterizer_globals_t;
co(rasterizer_globals_t, current_lock_operation, 0x02);

/* rasterizer_globals.current_lock_operation values (T2). */
enum {
  _rasterizer_lock_none = 0,
  _rasterizer_lock_lightning = 12 /* lightning_submit (0x135ac6) */
};

/// size=0xB4. Element size of the lightning 'elec' shaders block
/// (tag_block_get_element(..., 0xb4) @0x135e8a); global_shader_effect_additive
/// (0x326a78) is its fallback.  No field is accessed by the recovered code.
typedef struct {
  uint8_t pad_00[0xb4]; ///< offset=0x00
} shader_effect_definition;
cs(shader_effect_definition, 0xb4);

/* Per-object function values handed to widget submit functions.  Prefix only.
 * Both arrays are indexed by an object function source 1..4 (value - 1). */
typedef struct {
  real_rgb_color *colors; ///< offset=0x00  [ECX+EAX*0xc-0xc] in lightning_submit (0x135bab)
  real *values;           ///< offset=0x04  FLD [ECX+EDX*4-4] in lightning_submit (0x135615)
} render_animation;
co(render_animation, colors, 0x00);
co(render_animation, values, 0x04);

/* Object function source references (T2): 0 is none, 1..4 are functions a..d. */
enum {
  _object_function_reference_a = 1,
  _object_function_reference_d = 4
};

/* 'bitm' tag definition.  Prefix only (no size assert): lightning_submit takes
 * tag_get('bitm', ...) + 0x60 as the bitmap_data block and reads element 0 with
 * element size 0x30 (ADD EAX,0x60; tag_block_get_element(...,0,0x30) @0x1355b0).
 * Names: T2. */
typedef struct {
  uint8_t pad_00[0x60];  ///< offset=0x00
  tag_block bitmap_data; ///< offset=0x60
} bitmap_group;
co(bitmap_group, bitmap_data, 0x60);

#define SIZEOF_BITMAP_DATA 0x30 /* bitmap_data element size (tag_block_get_element @0x1355b7) */

/* Lightning widgets (objects/widgets/lightning.c).  Names: T2. */
/// size=0x4. lightning_globals (0x46f024); the data array pointer is stored by
/// lightnings_initialize (MOV [0x46f024],EAX @0x135334).
typedef struct {
  data_t *lightning_data; ///< offset=0x00
} lightning_globals_t;
cs(lightning_globals_t, 0x4);

/// size=0x8. Element size passed to game_state_data_new @0x135320.
typedef struct {
  int16_t datum_salt;      ///< offset=0x00  standard data_t element prefix
  uint8_t pad_02[2];       ///< offset=0x02
  int32_t definition_index; ///< offset=0x04  MOV [EAX+0x4],EDX in lightning_new (0x1353e2); tag_get('elec', [EAX+0x4]) in lightning_submit (0x135544)
} lightning_datum_t;
cs(lightning_datum_t, 0x8);
co(lightning_datum_t, definition_index, 0x04);

#define MAXIMUM_LIGHTNINGS 256 /* game_state_data_new count @0x135322 */

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

/* physics_variables.c speed-update parameters (0x154540 reads +0x0/+0x4/+0x8/
 * +0xc as floats: FMUL [EDX] / [EDX+0x4] / [EDX+0x8] / [EDX+0xc]).  Field
 * names from the PAL 2342 reference source (physics_variables.h). */
struct physics_variable_speed_parameters {
  real positive_scale;             ///< offset=0x00
  real negative_scale;             ///< offset=0x04
  real acceleration;               ///< offset=0x08
  real deceleration;               ///< offset=0x0c
};
cs(struct physics_variable_speed_parameters, 0x10);
co(struct physics_variable_speed_parameters, deceleration, 0xc);

/* 'phys' tag definition, size 0x80 (PAL 2342 physics_definitions.h).  Only
 * binary-observed fields are named: center_of_mass negated component-wise by
 * physics_instance_new (FLD [ESI+0xc/+0x10/+0x14] at 0x150a74-0x150a7e);
 * mass_points walked by FUN_001508b0 (count at +0x74, element size 0x80);
 * radius (+0x0, FLD [EBX] / FCOMP 0 at 0x1542a3) and powered_mass_points
 * count (+0x68, CMP [EBX+0x68] at 0x1542eb) read by physics_update; mass
 * (+0x8) multiplied across two instances and FSQRT'd at 0x151ef7 by
 * physics_compute_vehicle_collision. */
struct physics_definition {
  real radius;                     ///< offset=0x00
  real field_04;                   ///< offset=0x04 (moment scale, FMUL [EDI+4] @153cae)
  real mass;                       ///< offset=0x08
  real_point3d center_of_mass;     ///< offset=0x0c
  byte pad_18[0x4];                ///< offset=0x18
  real gravity_scale;              ///< offset=0x1c
  real ground_friction;            ///< offset=0x20
  real ground_depth;               ///< offset=0x24
  real ground_damp_fraction;       ///< offset=0x28
  real ground_normal_k1;           ///< offset=0x2c
  real ground_normal_k0;           ///< offset=0x30
  byte pad_34[0x4];                ///< offset=0x34
  real water_friction;             ///< offset=0x38
  real water_depth;                ///< offset=0x3c
  real water_density;              ///< offset=0x40
  byte pad_44[0x4];                ///< offset=0x44
  real air_friction;               ///< offset=0x48
  byte pad_4c[0x4];                ///< offset=0x4c
  real xx_moment;                  ///< offset=0x50 (FMUL [EAX+0x50] @1b8b28)
  real yy_moment;                  ///< offset=0x54 (FMUL [EAX+0x54] @1b8afc)
  real zz_moment;                  ///< offset=0x58 (FMUL [EAX+0x58] @1b8854)
  tag_block field_5c;              ///< offset=0x5c (0x24-byte 3x3 elements)
  tag_block powered_mass_points;   ///< offset=0x68
  tag_block mass_points;           ///< offset=0x74
};
cs(struct physics_definition, 0x80);
co(struct physics_definition, gravity_scale, 0x1c);
co(struct physics_definition, ground_friction, 0x20);
co(struct physics_definition, ground_normal_k0, 0x30);
co(struct physics_definition, water_friction, 0x38);
co(struct physics_definition, water_density, 0x40);
co(struct physics_definition, air_friction, 0x48);
co(struct physics_definition, field_5c, 0x5c);
co(struct physics_definition, field_04, 0x04);
co(struct physics_definition, mass, 0x08);
co(struct physics_definition, center_of_mass, 0x0c);
co(struct physics_definition, powered_mass_points, 0x68);
co(struct physics_definition, mass_points, 0x74);

/* 'phys' mass-point block element, size 0x80 (tag_block_get_element(...,
 * 0x80) in FUN_001508b0 / physics_test_vector).  position +0x38 and radius
 * +0x68 are the sphere_test_vector3d operands at 0x150bcb/0x150bd3; forward
 * +0x44 / up +0x50 are the matrix_transform_vector inputs in FUN_001508b0.
 * Names from the PAL 2342 reference (physics_definitions.h). */
struct mass_point_definition {
  byte pad_00[0x20];               ///< offset=0x00
  int16_t powered_mass_point_index; ///< offset=0x20
  byte pad_22[0xa];                ///< offset=0x22
  real mass;                       ///< offset=0x2c
  byte pad_30[0x4];                ///< offset=0x30
  real density;                    ///< offset=0x34
  real_point3d position;           ///< offset=0x38
  real_vector3d forward;           ///< offset=0x44
  real_vector3d up;                ///< offset=0x50
  int16_t friction_type;           ///< offset=0x5c
  byte pad_5e[0x2];                ///< offset=0x5e
  real friction_parallel_scale;    ///< offset=0x60
  real friction_perpendicular_scale; ///< offset=0x64
  real radius;                     ///< offset=0x68
  byte pad_6c[0x14];               ///< offset=0x6c
};
cs(struct mass_point_definition, 0x80);
co(struct mass_point_definition, powered_mass_point_index, 0x20);
co(struct mass_point_definition, mass, 0x2c);
co(struct mass_point_definition, density, 0x34);
co(struct mass_point_definition, friction_type, 0x5c);
co(struct mass_point_definition, friction_perpendicular_scale, 0x64);
co(struct mass_point_definition, position, 0x38);
co(struct mass_point_definition, radius, 0x68);

/* physics_test_vector (0x150b60) result: t at +0x0 (MOV [ESI],0x7f7fffff),
 * hit plane at +0x4 (normal +0x4..+0xc, d FSTP [ESI+0x10]). */
struct physics_test_vector_result {
  real t;                          ///< offset=0x00
  real_plane3d plane;              ///< offset=0x04
};
cs(struct physics_test_vector_result, 0x14);
co(struct physics_test_vector_result, plane, 0x04);

/* 'pphy' point-physics tag definition, size 0x40 (PAL 2342
 * point_physics.h).  Every named field is read and lerped by
 * point_physics_definition_interpolate (0x1548c0): flags copied as a dword
 * at 0x154988, then +0x20/+0x8/+0xc/+0x4/+0x24/+0x28/+0x2c/+0x30 as floats;
 * +0x4 is also the point_physics_definition_get_mass (0x1548a0) operand. */
struct point_physics_definition {
  uint32_t flags;                        ///< offset=0x00
  real runtime_mass_over_radius_cubed;   ///< offset=0x04
  real runtime_water_buoyancy_scale;     ///< offset=0x08
  real runtime_air_buoyancy_scale;       ///< offset=0x0c
  byte pad_10[0x10];                     ///< offset=0x10
  real density;                          ///< offset=0x20
  real air_friction;                     ///< offset=0x24
  real water_friction;                   ///< offset=0x28
  real contact_friction;                 ///< offset=0x2c
  real elasticity;                       ///< offset=0x30
  byte pad_34[0xc];                      ///< offset=0x34
};
cs(struct point_physics_definition, 0x40);
co(struct point_physics_definition, density, 0x20);
co(struct point_physics_definition, elasticity, 0x30);

/* Collision-test result filled by FUN_0014df70 (collision_test_vector),
 * size 0x50 (PAL 2342 collisions.h; point_physics_update frame places it at
 * [EBP-0x94] with the next local at [EBP-0x44]).  Offsets observed in
 * point_physics_update (0x154a50): type word at +0x0 compared with 0/2,
 * location copied as two dwords from +0xc/+0x10 when leaf_index != -1,
 * t at +0x14, point at +0x18, plane normal at +0x24, material_type word at
 * +0x34. */
struct collision_location {
  int32_t leaf_index;              ///< offset=0x00
  int32_t cluster_index;           ///< offset=0x04 (short + pad in PAL; copied as a dword)
};
cs(struct collision_location, 0x8);

struct collision_result {
  int16_t type;                          ///< offset=0x00
  byte pad_02[0xa];                      ///< offset=0x02
  struct collision_location location;    ///< offset=0x0c
  real t;                                ///< offset=0x14
  real_point3d point;                    ///< offset=0x18
  real_plane3d plane;                    ///< offset=0x24
  int16_t material_type;                 ///< offset=0x34
  byte pad_36[0x1a];                     ///< offset=0x36
};
cs(struct collision_result, 0x50);
co(struct collision_result, location, 0x0c);
co(struct collision_result, t, 0x14);
co(struct collision_result, point, 0x18);
co(struct collision_result, plane, 0x24);
co(struct collision_result, material_type, 0x34);

/* Per-mass-point friction split (PAL 2342 friction_datum.h), size 0x24:
 * friction_evaluate (0x150dd0) copies +0x0 into +0xc, zeroes +0x18..+0x20,
 * and rebuilds +0x0 = +0xc + +0x18 after scaling. */
struct friction_datum {
  real_vector3d friction;          ///< offset=0x00
  real_vector3d parallel;          ///< offset=0x0c
  real_vector3d perpendicular;     ///< offset=0x18
};
cs(struct friction_datum, 0x24);
co(struct friction_datum, parallel, 0x0c);
co(struct friction_datum, perpendicular, 0x18);

/* Per-object powered mass point state (PAL 2342 powered_mass_point_datum.h),
 * size 0x60: physics_update (0x154270) indexes it with LEA EAX,[EAX*3] /
 * SHL EAX,5 and rebuilds rotation_matrix (+0x2c) from the rotation
 * quaternion (+0x1c) via 0x1093b0, then transposes it (0x109120). */
struct powered_mass_point_datum {
  real ground_friction_velocity;   ///< offset=0x00
  real water_friction_velocity;    ///< offset=0x04
  real air_friction_velocity;      ///< offset=0x08
  real water_lift_ratio;           ///< offset=0x0c
  real air_lift_ratio;             ///< offset=0x10
  real thrust_fraction;            ///< offset=0x14
  real antigrav_fraction;          ///< offset=0x18
  real rotation[4];                ///< offset=0x1c (quaternion)
  real_matrix4x3 rotation_matrix;  ///< offset=0x2c
};
cs(struct powered_mass_point_datum, 0x60);
co(struct powered_mass_point_datum, water_lift_ratio, 0x0c);
co(struct powered_mass_point_datum, antigrav_fraction, 0x18);
co(struct powered_mass_point_datum, rotation, 0x1c);
co(struct powered_mass_point_datum, rotation_matrix, 0x2c);

/* 'phys' powered-mass-point block element (PAL 2342 physics.c, static
 * struct), size 0x80: tag_block_get_element(&physics->powered_mass_points,
 * index, 0x80) at 0x150f82 in physics_compute_new.  flags +0x20 (TEST byte
 * 0x1..0x40), antigrav_strength +0x24, antigrav_height +0x2c,
 * antigrav_damp_fraction +0x30 and the pin_fraction k1/k0 pair +0x34/+0x38
 * are the physics_compute_new operands at 0x151718-0x1517f5. */
struct powered_mass_point_definition {
  byte pad_00[0x20];               ///< offset=0x00
  uint32_t flags;                  ///< offset=0x20
  real antigrav_strength;          ///< offset=0x24
  byte pad_28[0x4];                ///< offset=0x28
  real antigrav_height;            ///< offset=0x2c
  real antigrav_damp_fraction;     ///< offset=0x30
  real antigrav_normal_k1;         ///< offset=0x34
  real antigrav_normal_k0;         ///< offset=0x38
  byte pad_3c[0x44];               ///< offset=0x3c
};
cs(struct powered_mass_point_definition, 0x80);
co(struct powered_mass_point_definition, flags, 0x20);
co(struct powered_mass_point_definition, antigrav_height, 0x2c);
co(struct powered_mass_point_definition, antigrav_normal_k0, 0x38);

/* Per-object mass point state (PAL 2342 mass_point_datum.h), size 0x130:
 * physics_compute_new (0x150ed0) memsets count*0x130 and strides EDI by
 * 0x130.  All named offsets are FLD/FSTP/LEA operands in that function;
 * compute_ground_plane (0x150c80) writes ground_plane/material/depth and
 * flag 0x4. */
struct mass_point_datum {
  uint32_t flags;                  ///< offset=0x00
  real_point3d position;           ///< offset=0x04
  real_vector3d forward;           ///< offset=0x10
  byte pad_1c[0xc];                ///< offset=0x1c
  real_vector3d up;                ///< offset=0x28
  byte location[0x8];              ///< offset=0x34 (scenario location)
  real_vector3d radius;            ///< offset=0x3c
  real_vector3d velocity;          ///< offset=0x48
  real_vector3d velocity_relative_to_ground; ///< offset=0x54
  real_plane3d ground_plane;       ///< offset=0x60
  int16_t ground_material_type;    ///< offset=0x70
  byte pad_72[0x2];                ///< offset=0x72
  real ground_depth;               ///< offset=0x74
  byte pad_78[0x4];                ///< offset=0x78
  real water_depth;                ///< offset=0x7c
  real normal_force_magnitude;     ///< offset=0x80
  real_vector3d normal_force;      ///< offset=0x84
  struct friction_datum ground_friction; ///< offset=0x90
  real water_pressure_magnitude;   ///< offset=0xb4
  real_vector3d water_pressure;    ///< offset=0xb8
  struct friction_datum water_friction; ///< offset=0xc4
  struct friction_datum air_friction; ///< offset=0xe8
  real_vector3d powered_force;     ///< offset=0x10c
  real_vector3d force;             ///< offset=0x118
  real_vector3d torque;            ///< offset=0x124
};
cs(struct mass_point_datum, 0x130);
co(struct mass_point_datum, location, 0x34);
co(struct mass_point_datum, ground_plane, 0x60);
co(struct mass_point_datum, ground_depth, 0x74);
co(struct mass_point_datum, water_depth, 0x7c);
co(struct mass_point_datum, ground_friction, 0x90);
co(struct mass_point_datum, water_friction, 0xc4);
co(struct mass_point_datum, air_friction, 0xe8);
co(struct mass_point_datum, powered_force, 0x10c);
co(struct mass_point_datum, torque, 0x124);

/* Game-globals material block element (PAL 2342
 * material_effect_definitions.h), size 0x374 (element size in
 * FUN_0018e500).  Only the five physics scales read by physics_compute_new
 * at 0x151102-0x1511b6 are named. */
struct material_definition {
  byte pad_00[0x94];               ///< offset=0x00
  real physics_ground_friction_scale;          ///< offset=0x94
  real physics_ground_friction_normal_k1_scale; ///< offset=0x98
  real physics_ground_friction_normal_k0_scale; ///< offset=0x9c
  real physics_ground_depth_scale;             ///< offset=0xa0
  real physics_ground_damp_fraction_scale;     ///< offset=0xa4
  byte pad_a8[0x2cc];              ///< offset=0xa8
};
cs(struct material_definition, 0x374);
co(struct material_definition, physics_ground_friction_scale, 0x94);
co(struct material_definition, physics_ground_damp_fraction_scale, 0xa4);

/* damage_data (PAL 2342 damage.h), size 0x54: the stack block filled by
 * damage_data_new (0x136750) in physics_compute_biped_collision, which then
 * writes flags +0x4 (OR 1), owner player/object/team +0x8/+0xc/+0x10,
 * origin +0x1c, epicenter +0x28, direction +0x34 and scale +0x40
 * (0x151d56-0x151dbc, 0x151e48-0x151ea0).  Names from PAL; the rest is
 * unobserved here. */
struct damage_data {
  byte pad_00[0x4];                ///< offset=0x00
  uint32_t flags;                  ///< offset=0x04
  int32_t owner_player_index;      ///< offset=0x08
  int32_t owner_object_index;      ///< offset=0x0c
  int16_t owner_team_index;        ///< offset=0x10
  byte pad_12[0xa];                ///< offset=0x12
  real_point3d origin;             ///< offset=0x1c
  real_point3d epicenter;          ///< offset=0x28
  real_vector3d direction;         ///< offset=0x34
  real scale;                      ///< offset=0x40
  byte pad_44[0x10];               ///< offset=0x44
};
cs(struct damage_data, 0x54);
co(struct damage_data, owner_team_index, 0x10);
co(struct damage_data, origin, 0x1c);
co(struct damage_data, direction, 0x34);
co(struct damage_data, scale, 0x40);

/* Transient physics instance built by physics_instance_new (0x1509c0):
 * object index at +0x0 (MOV [ESI],EDI), 'phys' definition at +0x4, and the
 * object's world matrix at +0x8 (scale store 0x3f800000 at 0x150a12). */
struct physics_instance {
  int32_t object_index;                      ///< offset=0x00
  const struct physics_definition *physics;  ///< offset=0x04
  real_matrix4x3 world_matrix;               ///< offset=0x08
};
cs(struct physics_instance, 0x3c);
co(struct physics_instance, physics, 0x04);
co(struct physics_instance, world_matrix, 0x08);

/* Per-tick camera input passed to the camera update procs: +0 local player
 * index (read as a word), +4 tick length read by scripted_camera_update. */
typedef struct {
  int16_t local_player_index;              ///< offset=0x00
  uint8_t pad_02[2];                       ///< offset=0x02  never observed accessed
  real seconds_elapsed;                    ///< offset=0x04
} camera_control_t;
cs(camera_control_t, 0x8);
co(camera_control_t, seconds_elapsed, 0x04);

/* Bored (attract-mode) camera state, walked by bored_camera_update (0x84ae0):
 * +0 last update time, +4 countdown, +8 shot counter (all 32-bit). */
typedef struct {
  uint32_t last_update_milliseconds;       ///< offset=0x00
  int32_t timer_milliseconds;              ///< offset=0x04
  int32_t boredom_count;                   ///< offset=0x08
} bored_camera_t;
co(bored_camera_t, timer_milliseconds, 0x04);
co(bored_camera_t, boredom_count, 0x08);

/* Unit camera tag block; only the camera-track block at +0x4c is proven
 * (bored_camera_update reads its count, then fetches element 0 of 0x1c). */
typedef struct {
  uint8_t pad_00[0x4c];                    ///< offset=0x00  not accessed here
  tag_block unit_camera_tracks;            ///< offset=0x4c
} unit_camera_t;
co(unit_camera_t, unit_camera_tracks, 0x4c);

/* player_control_get_unit_camera_info (0xb6740) output, 0x18 bytes on the
 * caller's stack in bored_camera_update; +8 is the unit camera tag whose
 * track block sits at +0x4c. */
typedef struct {
  int32_t unit_index;                      ///< offset=0x00  0x89d6c dword read
  int16_t seat_index;                      ///< offset=0x04  0x89d4a word read
  uint8_t pad_06[2];                       ///< offset=0x06
  unit_camera_t *camera;                   ///< offset=0x08
  real_point3d position;                   ///< offset=0x0c
} player_control_unit_camera_info_t;
cs(player_control_unit_camera_info_t, 0x18);
co(player_control_unit_camera_info_t, camera, 0x08);
co(player_control_unit_camera_info_t, position, 0x0c);

/* Camera command: the result block filled by the per-mode camera functions
 * (bored_camera, first_person_camera, ...) and copied whole by
 * observer_update_command (26 dwords = 0x68 bytes) into the observer at +0x8.
 * The validator's "Invalid camera command." format string labels the members:
 * F=forward U=up P=position O=offset D=depth V=velocity FOV=field_of_view
 * T=timer FL=flags. The five bytes at +0x4c and floats at +0x54 are walked in
 * lockstep by observer_update_command (5 iterations); their meaning is
 * unproven. */
typedef struct {
  uint32_t flags;                          ///< offset=0x00
  real_point3d position;                   ///< offset=0x04
  real_vector3d offset;                    ///< offset=0x10
  real depth;                              ///< offset=0x1c
  real field_of_view;                      ///< offset=0x20
  real_vector3d forward;                   ///< offset=0x24
  real_vector3d up;                        ///< offset=0x30
  real_vector3d velocity;                  ///< offset=0x3c
  real timer;                              ///< offset=0x48
  uint8_t field_4c[5];                     ///< offset=0x4c
  char pad_51[3];                          ///< offset=0x51
  real field_54[5];                        ///< offset=0x54
} camera_command_t;
cs(camera_command_t, 0x68);
co(camera_command_t, flags, 0x00);
co(camera_command_t, position, 0x04);
co(camera_command_t, offset, 0x10);
co(camera_command_t, depth, 0x1c);
co(camera_command_t, field_of_view, 0x20);
co(camera_command_t, forward, 0x24);
co(camera_command_t, up, 0x30);
co(camera_command_t, velocity, 0x3c);
co(camera_command_t, timer, 0x48);

/* camera_command_t.flags bits.  Bit 0 gates the "Invalid camera command."
 * validation (observer_set_camera 0x8acb0, first_person_camera_for_unit_and_
 * vector 0x88d50); observer_set_camera raises bit 3 on an observer's first
 * command. */
enum {
  _observer_command_valid_bit = 0,
  _observer_command_force_time_bit = 3
};

/* Per-local-player observer, 0x29c bytes (observer_update strides 0x29c from
 * 0x33571c).  observer_result_initialize writes OBSERVER_SIGNATURE at +0x0
 * and +0x298 and seeds the +0x70/+0x71 bytes; observer_update checks both
 * signatures and the +0x70 byte under the "observer->header_signature",
 * "!observer->updated_for_frame" asserts; observer_set_camera stores the
 * command pointer at +0x4 and tests/sets +0x71; observer_update_command
 * copies the 0x68-byte command to +0x8; observer_get_camera returns +0x74. */
typedef struct {
  int32_t header_signature;                ///< offset=0x00
  camera_command_t *pending_command;       ///< offset=0x04
  camera_command_t last_command;           ///< offset=0x08
  uint8_t updated_for_frame;               ///< offset=0x70
  uint8_t first_command;                   ///< offset=0x71
  uint8_t pad_72[2];                       ///< offset=0x72
  uint8_t field_74[0x224];                 ///< offset=0x74 camera result + integrator state
  int32_t trailer_signature;               ///< offset=0x298
} observer_t;
cs(observer_t, 0x29c);
co(observer_t, pending_command, 0x04);
co(observer_t, last_command, 0x08);
co(observer_t, updated_for_frame, 0x70);
co(observer_t, first_command, 0x71);
co(observer_t, field_74, 0x74);
co(observer_t, trailer_signature, 0x298);

#define OBSERVER_SIGNATURE 0x72616421 /* 'rad!' */

/* Observer globals at 0x335718: observer_update stores its delta time at
 * +0x0, the four observers follow at 0x33571c. */
typedef struct {
  real dtime;
  observer_t local_players[4];
} observer_globals_t;
cs(observer_globals_t, 0xa74);
co(observer_globals_t, local_players, 0x04);
/* Camera state and input layouts re-proven from the 2276 update routines.
 * Field names are T2 from PAL 2342; recovery/evidence/camera_*.json records
 * the target-build accesses. */
typedef struct {
  int16_t local_player_index;
  uint8_t active;
  uint8_t pad_03;
  real seconds_elapsed;
  real_euler_angles3d facing_delta;
  real_vector3d translation;
  real wheel_delta;
} camera_action_t;
cs(camera_action_t, 0x24);
co(camera_action_t, active, 0x02);
co(camera_action_t, seconds_elapsed, 0x04);
co(camera_action_t, facing_delta, 0x08);
co(camera_action_t, translation, 0x14);
co(camera_action_t, wheel_delta, 0x20);

typedef struct {
  uint8_t initialized;
  uint8_t confined;
  uint8_t crouched;
  uint8_t zoomed;
  int16_t zoom_level;
  uint8_t pad_06[2];
  int32_t unit_index;
  int16_t seat_index;
  uint8_t pad_0e[2];
  real_euler_angles2d facing_offset;
  real distance_scale;
} following_camera_t;
cs(following_camera_t, 0x1c);
co(following_camera_t, crouched, 0x02);
co(following_camera_t, unit_index, 0x08);
co(following_camera_t, seat_index, 0x0c);
co(following_camera_t, facing_offset, 0x10);
co(following_camera_t, distance_scale, 0x18);

typedef struct {
  real_point3d position;
  real_euler_angles2d facing;
  real distance;
  real field_of_view;
  real timer;
  int32_t player_index;
  int32_t current_player_index;
  int32_t unit_index;
  real switch_timer;
} dead_camera_t;
cs(dead_camera_t, 0x30);
co(dead_camera_t, facing, 0x0c);
co(dead_camera_t, distance, 0x14);
co(dead_camera_t, field_of_view, 0x18);
co(dead_camera_t, timer, 0x1c);
co(dead_camera_t, player_index, 0x20);
co(dead_camera_t, current_player_index, 0x24);
co(dead_camera_t, unit_index, 0x28);
co(dead_camera_t, switch_timer, 0x2c);

typedef struct {
  real_point3d position;
  real_euler_angles2d facing;
  real roll;
  real field_of_view;
} flying_camera_t;
cs(flying_camera_t, 0x1c);
co(flying_camera_t, facing, 0x0c);
co(flying_camera_t, roll, 0x14);
co(flying_camera_t, field_of_view, 0x18);

typedef struct {
  real_euler_angles2d facing;
  real distance;
} orbiting_camera_t;
cs(orbiting_camera_t, 0x0c);
co(orbiting_camera_t, distance, 0x08);

/* Director debug variable: four {value, velocity, delta} triples at director
 * +0xc8 (director_process_variables walks them with a 0xc stride). */
typedef struct {
  real value;
  real velocity;
  real delta;
} director_variable_instance_t;
cs(director_variable_instance_t, 0x0c);
co(director_variable_instance_t, velocity, 0x04);
co(director_variable_instance_t, delta, 0x08);

/* Director debug variable definition (0x1c-byte .data table entries read by
 * director_process_variables). */
typedef struct {
  int16_t negative_bit;
  int16_t positive_bit;
  int16_t reset_bit;
  uint8_t pad_06[2];
  real scale;
  real initial_value;
  real minimum;
  real maximum;
  uint8_t has_hyper_scale;
  uint8_t pad_19[3];
} director_variable_definition_t;
cs(director_variable_definition_t, 0x1c);
co(director_variable_definition_t, scale, 0x08);
co(director_variable_definition_t, initial_value, 0x0c);
co(director_variable_definition_t, minimum, 0x10);
co(director_variable_definition_t, maximum, 0x14);
co(director_variable_definition_t, has_hyper_scale, 0x18);

/* Per-local-player camera director, 0xf8 bytes (stride of every
 * index * 0xf8 access in director.obj).
 *   +0x04 camera_change_pause  1.0f written by FUN_000865a0 when asked
 *   +0x08 camera_proc          the active camera update function; holds the
 *                              ORIGINAL entry address (see director.c)
 *   +0x0c camera_data          per-mode camera block (following/first-person
 *                              camera_new take &camera_data)
 *   +0x4c bored_time / +0x50 bored  dword + byte pair read together
 *   +0x51 inhibited_facing     director_inhibit_facing / _inhibited_facing
 *   +0x52 inhibited_input      director_set_local_player_context /
 *                              director_inhibited_input
 *   +0x54 seat_state           perspective word stored by
 *                              director_choose_game_perspective
 *   +0x56 perspective          cache written by director_get_perspective */
typedef struct {
  int16_t camera_mode_index;
  uint8_t pad_02[2];
  real camera_change_pause;
  int32_t camera_proc;
  uint8_t camera_data[0x40];
  int32_t bored_time;
  uint8_t bored;
  uint8_t inhibited_facing;
  uint8_t inhibited_input;
  uint8_t pad_53[1];
  int16_t seat_state;
  int16_t perspective;
  camera_command_t command;
  uint8_t debug_controls;
  uint8_t pad_c1[3];
  real debug_input_scale;
  director_variable_instance_t debug_variables[4];
} camera_director_t;
cs(camera_director_t, 0xf8);
co(camera_director_t, camera_change_pause, 0x04);
co(camera_director_t, camera_proc, 0x08);
co(camera_director_t, camera_data, 0x0c);
co(camera_director_t, bored_time, 0x4c);
co(camera_director_t, bored, 0x50);
co(camera_director_t, inhibited_facing, 0x51);
co(camera_director_t, inhibited_input, 0x52);
co(camera_director_t, seat_state, 0x54);
co(camera_director_t, perspective, 0x56);
co(camera_director_t, command, 0x58);
co(camera_director_t, debug_controls, 0xc0);
co(camera_director_t, debug_input_scale, 0xc4);
co(camera_director_t, debug_variables, 0xc8);

/* Director globals at 0x3352a8: the frame delta director_update writes
 * (0x3352a8), the director game mode word (0x3352ac, compared/stored by
 * director_set_game_mode), the re-dispatch flag byte it raises (0x3352ae),
 * then the four local-player directors (0x3352b0). */
typedef struct {
  real dtime;
  int16_t game_mode;
  uint8_t initialize_camera;
  uint8_t pad_07[1];
  camera_director_t local_players[4];
} director_globals_t;
cs(director_globals_t, 0x3e8);
co(director_globals_t, game_mode, 0x04);
co(director_globals_t, initialize_camera, 0x06);
co(director_globals_t, local_players, 0x08);

/* "director scripting" game-state block (game_state_malloc size 4); only the
 * first byte is ever read or written. */
typedef struct {
  uint8_t camera_scripted;
  uint8_t pad_01[3];
} director_scripting_t;
cs(director_scripting_t, 0x4);

/* Editor (flying) camera globals at 0x335698, 0x7c bytes.  Offsets from the
 * director.obj editor_camera_* functions: +0x01 use_roll byte (FUN_00087ac0),
 * +0x02 initialized byte and +0x04 focus (editor_camera_set_focus /
 * _set_position), +0x18 live camera pointer, +0x2c mode word
 * (editor_camera_set_mode), +0x38 two persisted 0x20-byte camera slots. */
typedef struct {
  flying_camera_t camera;
  uint8_t saved;
  uint8_t pad_1d[3];
} editor_camera_persisted_camera_t;
cs(editor_camera_persisted_camera_t, 0x20);
co(editor_camera_persisted_camera_t, saved, 0x1c);

typedef struct {
  uint8_t scripted;
  uint8_t use_roll;
  uint8_t initialized;
  uint8_t pad_03[1];
  real_point3d focus_position;
  real_euler_angles2d focus_angles;
  flying_camera_t *camera;
  uint8_t reset_all;
  uint8_t pad_1d[3];
  real_vector3d unit_offset;
  int16_t mode;
  uint8_t pad_2e[4];
  uint8_t last_scripted;
  uint8_t pad_33[5];
  editor_camera_persisted_camera_t persisted_cameras[2];
  uint32_t speed_step;
} editor_camera_globals_t;
cs(editor_camera_globals_t, 0x7c);
co(editor_camera_globals_t, use_roll, 0x01);
co(editor_camera_globals_t, initialized, 0x02);
co(editor_camera_globals_t, focus_position, 0x04);
co(editor_camera_globals_t, focus_angles, 0x10);
co(editor_camera_globals_t, camera, 0x18);
co(editor_camera_globals_t, reset_all, 0x1c);
co(editor_camera_globals_t, unit_offset, 0x20);
co(editor_camera_globals_t, mode, 0x2c);
co(editor_camera_globals_t, last_scripted, 0x32);
co(editor_camera_globals_t, persisted_cameras, 0x38);
co(editor_camera_globals_t, speed_step, 0x78);

/* translate_funcs[mode][direction] (0x2ee67c): per editor-camera-mode
 * {_translate_from, _translate_to} callbacks, named by the asserts in
 * editor_camera_set_mode. */
typedef void (*editor_camera_translate_function)(flying_camera_t *camera);
enum {
  _translate_from = 0,
  _translate_to = 1,
  NUMBER_OF_EDITOR_CAMERA_TRANSLATIONS = 2,
  NUMBER_OF_EDITOR_CAMERA_MODES = 2
};

co(camera_command_t, field_4c, 0x4c);
co(camera_command_t, field_54, 0x54);
/* ---------------------------------------------------------------------------
 * UI widget runtime instance (interface/ui_widget.c). size=0x58 (PAL 2342
 * verify_widget_instance_size). Offsets are re-proven from the 2276 list
 * navigation functions (0xe6ab0/0xe6cb0), the tab functions (0xe53e0/0xe5440)
 * and event_handler_dispatch (0xe6ed0); names are PAL 2342 (T2). pad_ bytes
 * were not observed accessed by those functions. The list_* fields are the
 * PAL `parameters.list` union arm (text boxes overlay 0x3c..0x43).
 * ------------------------------------------------------------------------- */
typedef struct widget_instance_t {
  int32_t definition_tag_index;         ///< offset=0x00 tag_get('DeLa', [ESI])
  void *field_04;                       ///< offset=0x04 definition+4 @0xe7b54
  int16_t local_player_index;           ///< offset=0x08 MOV DX,[ESI+8] zero-extended
  int16_t horizontal_offset;            ///< offset=0x0a ADD [ESI+0xa],CX @0xe7112
  int16_t vertical_offset;              ///< offset=0x0c ADD [ESI+0xc],CX @0xe711a
  int16_t type;                         ///< offset=0x0e 2 spinner list, 3 column list
  uint8_t visible;                      ///< offset=0x10 TEST AL @0xe6484
  uint8_t field_11;                    ///< offset=0x11
  uint8_t field_12;                    ///< offset=0x12
  uint8_t field_13;                    ///< offset=0x13
  uint8_t pad_14[0x1];
  uint8_t field_15;                    ///< offset=0x15
  uint8_t pad_16[0x2];
  int32_t field_18;                    ///< offset=0x18
  int32_t field_1c;                    ///< offset=0x1c
  int32_t field_20;                    ///< offset=0x20
  real alpha_modifier;                  ///< offset=0x24 parent-chain product @0xe645f
  struct widget_instance_t *previous;   ///< offset=0x28
  struct widget_instance_t *next;       ///< offset=0x2c
  struct widget_instance_t *parent;     ///< offset=0x30
  struct widget_instance_t *child;      ///< offset=0x34 first child
  struct widget_instance_t *focused_child; ///< offset=0x38
  int16_t list_selected_index;          ///< offset=0x3c
  int16_t list_last_tab_direction;      ///< offset=0x3e set to +15 / -15
  void *list_items;                     ///< offset=0x40
  uint16_t list_number_of_items;        ///< offset=0x44 MOVZX / JBE (unsigned)
  uint8_t pad_46[0x2];
  struct widget_instance_t *field_48;  ///< offset=0x48
  wchar_t *list_item_text;              ///< offset=0x4c read @0xe66dd (PAL item_text)
  int16_t field_50;                     ///< offset=0x50 read/write @0xe73c0 callers
  uint8_t pad_52[0x4];
  int16_t field_56;                     ///< offset=0x56
} widget_instance_t;
cs(widget_instance_t, 0x58);
co(widget_instance_t, local_player_index, 0x08);
co(widget_instance_t, type, 0x0e);
co(widget_instance_t, visible, 0x10);
co(widget_instance_t, alpha_modifier, 0x24);
co(widget_instance_t, list_item_text, 0x4c);
co(widget_instance_t, previous, 0x28);
co(widget_instance_t, next, 0x2c);
co(widget_instance_t, parent, 0x30);
co(widget_instance_t, child, 0x34);
co(widget_instance_t, focused_child, 0x38);
co(widget_instance_t, list_selected_index, 0x3c);
co(widget_instance_t, list_last_tab_direction, 0x3e);
co(widget_instance_t, list_items, 0x40);
co(widget_instance_t, list_number_of_items, 0x44);
co(widget_instance_t, field_50, 0x50);
co(widget_instance_t, field_04, 0x04);
co(widget_instance_t, field_20, 0x20);
co(widget_instance_t, field_1c, 0x1c);
co(widget_instance_t, field_18, 0x18);
co(widget_instance_t, field_15, 0x15);
co(widget_instance_t, field_13, 0x13);
co(widget_instance_t, field_12, 0x12);
co(widget_instance_t, field_11, 0x11);
co(widget_instance_t, field_56, 0x56);
co(widget_instance_t, field_48, 0x48);

#define UI_WIDGET_TYPE_SPINNER_LIST 2
#define UI_WIDGET_TYPE_COLUMN_LIST 3

/* ui_widget_definition ('DeLa' tag data). size=0x3ec per PAL 2342; only the
 * fields read by the 2276 functions named above are broken out. */
typedef struct {
  int16_t type;                         ///< offset=0x00 CMP word [EDI],2 @0xeaa47
  uint8_t pad_02[0x22];                 ///< offset=0x02
  viewport_bounds_t bounds;             ///< offset=0x24 dword pair @0xe6731/0xe6737
  int32_t flags;                        ///< offset=0x2c TEST byte [EAX+0x2c],1 @0xe5414
  uint8_t pad_30[0x14];                 ///< offset=0x30
  int32_t field_44;                     ///< offset=0x44 read @0xe73c0
  int32_t field_48;                     ///< offset=0x48 read @0xe73c0
  void *field_4c;                        ///< offset=0x4c read @0xe73c0
  uint8_t pad_50[0x4];                  ///< offset=0x50
  tag_block event_handlers;             ///< offset=0x54 count read @0xe540a
  tag_block search_and_replace_functions; ///< offset=0x60 count/address @0xe6663/0xe6680
  uint8_t pad_6c[0x80];                 ///< offset=0x6c
  tag_reference text_label_string_list; ///< offset=0xec index read @0xe65f6
  tag_reference text_font;              ///< offset=0xfc index read @0xe66ee
  real_argb_color text_color;           ///< offset=0x10c copied @0xe6796
  int16_t justification;                ///< offset=0x11c range 0..2 @0xe6701
  uint16_t text_box_flags;              ///< offset=0x11e TEST byte,4 @0xe67e6
  uint8_t pad_120[0x30];                ///< offset=0x120
  int32_t list_flags;                   ///< offset=0x150 TEST byte [EBX+0x150],2 @0xe6c17
  tag_reference list_header_bitmap;     ///< offset=0x154 index read @0xe64bb
  tag_reference list_footer_bitmap;     ///< offset=0x164 index read @0xe655a
  viewport_bounds_t list_header_bounds; ///< offset=0x174 @0xe64fd
  viewport_bounds_t list_footer_bounds; ///< offset=0x17c @0xe6598
  uint8_t pad_184[0x150];               ///< offset=0x184
  tag_block conditional_widgets;        ///< offset=0x2d4 count/address @0xe72df/0xe72f0
  uint8_t pad_2e0[0x100];               ///< offset=0x2e0
  tag_block child_widgets;              ///< offset=0x3e0 count read @0xe6b82
} ui_widget_definition_t;
cs(ui_widget_definition_t, 0x3ec);
co(ui_widget_definition_t, type, 0x00);
co(ui_widget_definition_t, bounds, 0x24);
co(ui_widget_definition_t, flags, 0x2c);
co(ui_widget_definition_t, field_44, 0x44);
co(ui_widget_definition_t, field_48, 0x48);
co(ui_widget_definition_t, field_4c, 0x4c);
co(ui_widget_definition_t, search_and_replace_functions, 0x60);
co(ui_widget_definition_t, text_label_string_list, 0xec);
co(ui_widget_definition_t, text_font, 0xfc);
co(ui_widget_definition_t, text_color, 0x10c);
co(ui_widget_definition_t, justification, 0x11c);
co(ui_widget_definition_t, text_box_flags, 0x11e);
co(ui_widget_definition_t, list_header_bitmap, 0x154);
co(ui_widget_definition_t, list_footer_bitmap, 0x164);
co(ui_widget_definition_t, list_header_bounds, 0x174);
co(ui_widget_definition_t, list_footer_bounds, 0x17c);
co(ui_widget_definition_t, event_handlers, 0x54);
co(ui_widget_definition_t, list_flags, 0x150);
co(ui_widget_definition_t, conditional_widgets, 0x2d4);
co(ui_widget_definition_t, child_widgets, 0x3e0);

#define UI_WIDGET_PASS_UNHANDLED_EVENTS_TO_CHILDREN_FLAG 0x1
#define UI_TEXT_BOX_FLASHING_TEXT_FLAG 0x4

/* ui_widget_search_and_replace_reference, size=0x22 (ADD EDX,0x22 @0xe66ca;
 * replace_function word read at +0x20 @0xe6692). Names PAL 2342 (T2). */
typedef struct {
  char search_string[32];               ///< offset=0x00
  uint16_t replace_function;            ///< offset=0x20
} ui_widget_search_and_replace_reference_t;
cs(ui_widget_search_and_replace_reference_t, 0x22);
co(ui_widget_search_and_replace_reference_t, replace_function, 0x20);
#define UI_LIST_ITEMS_GENERATED_FROM_STRING_LIST_TAG_FLAG 0x2

/* ui_widget_event_handler_reference, size=0x48 (stride at 0xe7ea3). */
typedef struct {
  int32_t flags;                        ///< offset=0x00
  int16_t event_type;                   ///< offset=0x04
  uint16_t function;                    ///< offset=0x06 zero-extended @0xe6f3d
  tag_reference widget_tag;             ///< offset=0x08 index read at +0x14
  tag_reference sound_effect;           ///< offset=0x18 index read at +0x24
  char script[32];                      ///< offset=0x28
} ui_widget_event_handler_reference_t;
cs(ui_widget_event_handler_reference_t, 0x48);
co(ui_widget_event_handler_reference_t, function, 0x06);
co(ui_widget_event_handler_reference_t, widget_tag, 0x08);
co(ui_widget_event_handler_reference_t, sound_effect, 0x18);
co(ui_widget_event_handler_reference_t, script, 0x28);

#define UI_EVENT_HANDLER_CLOSE_CURRENT_WIDGET_FLAG 0x001
#define UI_EVENT_HANDLER_CLOSE_OTHER_WIDGET_FLAG 0x002
#define UI_EVENT_HANDLER_CLOSE_ALL_WIDGETS_FLAG 0x004
#define UI_EVENT_HANDLER_OPEN_WIDGET_FLAG 0x008
#define UI_EVENT_HANDLER_RELOAD_WIDGET_FLAG 0x020
#define UI_EVENT_HANDLER_GIVE_FOCUS_TO_WIDGET_FLAG 0x040
#define UI_EVENT_HANDLER_RUN_FUNCTION_FLAG 0x080
#define UI_EVENT_HANDLER_REPLACE_WITH_OTHER_WIDGET_FLAG 0x100
#define UI_EVENT_HANDLER_GO_BACK_TO_PREVIOUS_WIDGET_FLAG 0x200
#define UI_EVENT_HANDLER_RUN_SCENARIO_SCRIPT_FLAG 0x400
#define UI_EVENT_HANDLER_LOOK_FOR_CONDITIONAL_WIDGET_ON_FAILURE_FLAG 0x800

/* ui_widget_conditional_reference, size=0x50 (ADD ESI,0x50 @0xe7349). */
typedef struct {
  tag_reference widget_tag;             ///< offset=0x00 index read at +0x0c
  char name[32];                        ///< offset=0x10
  int32_t flags;                        ///< offset=0x30 TEST byte [EAX+0x30],1
  uint8_t pad_34[0x1c];                 ///< offset=0x34
} ui_widget_conditional_reference_t;
cs(ui_widget_conditional_reference_t, 0x50);
co(ui_widget_conditional_reference_t, flags, 0x30);

#define UI_CONDITIONAL_WIDGET_LOAD_IF_FUNCTION_FAILS_FLAG 0x1

/* widget_stack_data / widget_stack_node (push_widget 0xe46f0 allocates 0x10,
 * copies 3 dwords, links next at +0xc; go_back_to_previous reads the fields
 * at EBP-0x10/-0xc/-0x8/-0x6). Names PAL 2342 (T2). */
typedef struct {
  int32_t previous_widget_tag;              ///< offset=0x00
  int32_t focused_child_parent_widget_tag;  ///< offset=0x04
  int16_t focused_child_index;              ///< offset=0x08
  int16_t local_player_index;               ///< offset=0x0a
} widget_stack_data_t;
cs(widget_stack_data_t, 0xc);
co(widget_stack_data_t, focused_child_index, 0x08);
co(widget_stack_data_t, local_player_index, 0x0a);

typedef struct widget_stack_node_t {
  widget_stack_data_t data;                 ///< offset=0x00
  struct widget_stack_node_t *next;         ///< offset=0x0c
} widget_stack_node_t;
cs(widget_stack_node_t, 0x10);
co(widget_stack_node_t, next, 0x0c);

#endif /* TYPES_H */
