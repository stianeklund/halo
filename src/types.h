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

/// size=0x68
/* First field is a 2-byte scalar: every variant-default initializer in the
 * original zeroes a local copy as MOV word [base],DX then REP STOSD from
 * base+2 (0x19 dwords) + STOSW - MSVC's member-wise {0} zeroing of a struct
 * whose first member is 16-bit. */
typedef struct {
  int16_t unk_0;
  char    unk_2[0x66];
} game_variant_t;

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
  char unk_0[0xd4];
} player_data_t;

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
  uint16_t action_flags;         ///< offset=0x08 (player_control_set_action_flags)
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
/// player_control_get_facing. Bungie calls the parameter "input" -- recovered
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
/// The action player_control_get_facing builds from a player control slot and
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

/// size=0x54
typedef struct {
  vector3_t         unk_0;                  ///< offset=0x00
  vector3_t         unk_12;                 ///< offset=0x0c
  vector3_t         unk_24;                 ///< offset=0x18
  uint8_t           unk_36;                 ///< offset=0x24
  char              unk_37[3];              ///< offset=0x25
  float             vertical_field_of_view; ///< offset=0x28
  viewport_bounds_t viewport_bounds;        ///< offset=0x2c
  viewport_bounds_t unk_52;                 ///< offset=0x34
  float             z_near;                 ///< offset=0x3c
  float             z_far;                  ///< offset=0x40
  char              unk_68[16];             ///< offset=0x44
} camera_t;

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

/* ai_firing_pos_entry_t — one slot in the firing-position candidate buffer
 * built by FUN_00041420 and consumed by ai_test_line_of_fire.
 * Entry stride = 0x28 bytes; buffer holds up to 0x20 entries.
 *
 * Note: vec_b[3] as declared occupies +0x10..+0x18, but the binary only ever
 * writes two elements (vec_b[0] and vec_b[1] = 0.0f) via FUN_000413c0.
 * scalar_a at +0x18 shares the same offset as vec_b[2] — the name
 * distinguishes its role (height_offset from biped_get_camera_height_and_offset).
 * Layout confirmed from FUN_000413c0 disasm stores at 0x41402–0x4141a. */
typedef struct {
    bool       occupied;   /* +0x00: 0 = candidate; 1 = selected winner */
    bool       is_sphere;  /* +0x01: 0 = segment test; 1 = sphere test  */
    int16_t    _pad;       /* +0x02: unused                              */
    float      vec_a[3];   /* +0x04: biped eye position (from biped_get_camera_height_and_offset) */
    float      vec_b[2];   /* +0x10: line direction or zero for sphere   */
    float      scalar_a;   /* +0x18: height_offset (biped camera height) */
    int        handle_a;   /* +0x1c: actor handle (return from prop_get_active_by_unit_index / local_10[0]) */
    int        handle_b;   /* +0x20: object/unit handle (EDI at call to FUN_000413c0) */
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
 * Cross-reference: the prose block above FUN_0003dc20 in halo/ai/actors.c
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
  int32_t field_1c0;                                 /* +0x1c0  accessed 1x, meaning unproven */
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
   * `% 4` cursor wrap in FUN_00024be0; the record boundary itself is unproven,
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
  int16_t field_400;                                 /* +0x400  accessed 3x, meaning unproven */
  char field_402;                                    /* +0x402  accessed 1x, meaning unproven */
  char pad_403[0x5];
  float field_408;                                   /* +0x408  accessed 2x, meaning unproven */
  float field_40c;                                   /* +0x40c  accessed 1x, meaning unproven */
  int32_t field_410;                                 /* +0x410  accessed 1x, meaning unproven */
  int32_t field_414;                                 /* +0x414  accessed 3x, meaning unproven */
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
  int16_t field_46c;                                 /* +0x46c  accessed 2x, meaning unproven */
  char field_46e;                                    /* +0x46e  accessed 1x, meaning unproven */
  char pad_46f[0x1];
  int16_t field_470;                                 /* +0x470  accessed 2x, meaning unproven */
  char pad_472[0xa];
  int32_t field_47c;                                 /* +0x47c  accessed 1x, meaning unproven */
  int32_t control_path_destination_orders_ignore_target_object_index;/* +0x480  CMP dword [ESI+0x480],-1 @0x2d16b (NONE sentinel) */
  char field_484;                                    /* +0x484  accessed 6x, meaning unproven */
  char pad_485[0x3];
  float field_488;                                   /* +0x488  accessed 1x, meaning unproven */
  float field_48c;                                   /* +0x48c  accessed 1x, meaning unproven */
  float field_490;                                   /* +0x490  accessed 1x, meaning unproven */
  int32_t field_494;                                 /* +0x494  accessed 2x, meaning unproven */
  char pad_498[0xc];
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
  int16_t field_546;                                 /* +0x546  accessed 4x, meaning unproven */
  int16_t field_548;                                 /* +0x548  accessed 6x, meaning unproven */
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
  char pad_69c[0x4];
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
co(actor_t, control_secondary_look_type,                   0x544);
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
co(tag_block, count,   0x00);
co(tag_block, address, 0x04);

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

/* -------------------------------------------------------------------------
 * collision_test_result -- 0x50-byte record filled by the world collision
 * entry points in physics/collision_bsp.c (FUN_0014e7d0, FUN_0014e940).
 *
 * Widths are taken from the store instructions at 0x14e7f9..0x14e872 and
 * 0x14e8b9..0x14e8ef: +0x00, +0x08, +0x10, +0x34 and +0x4e are 16-bit stores
 * (MOV word ptr), +0x4c/+0x4d are byte stores, everything else is a dword.
 * 0x36..0x43 is never touched by either function, so it stays pad_.
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
    char    pad_36[0xe];   /* +0x36 */
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

#endif /* TYPES_H */
