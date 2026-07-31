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

// OBJE -> UNIT -> VEHI
/// size=0x47C
typedef struct {
  unit_data_t unit;       ///< offset=0x000
  uint16_t unk_1060;      ///< offset=0x424 .text:001B578E                 mov     [esi+424h], bx
  uint16_t unk_1062;      ///< offset=0x426 .text:001B9819                 cmp     word ptr [ebx+426h], 0
  uint8_t unk_1064;       ///< offset=0x428 .text:001A2020                 cmp     byte ptr [ebx+428h], 1Eh
  uint8_t unk_1065;       ///< offset=0x429 .text:001B57A2                 mov     [esi+429h], bl
  uint8_t unk_1066;       ///< offset=0x42A .text:001B57A8                 mov     [esi+42Ah], bl
  uint8_t unk_1067;       ///< offset=0x42B .text:001B572C                 mov     al, [esi+42Bh]
  float unk_1068;         ///< offset=0x42C .text:001B6025                 fld     dword ptr [esi+42Ch]
  float unk_1072;         ///< offset=0x430 .text:001B7B31                 fld     dword ptr [esi+430h]
  float unk_1076;         ///< offset=0x434 .text:001B602B                 fsub    dword ptr [esi+434h]
  float unk_1080;         ///< offset=0x438 .text:001B5BA9                 fld     dword ptr [esi+438h]
  float unk_1084;         ///< offset=0x43C .text:001B604C                 fadd    dword ptr [esi+43Ch]
  float unk_1088;         ///< offset=0x440 .text:001B608F                 fadd    dword ptr [esi+440h]
  float unk_1092;         ///< offset=0x444 .text:0002EAA8                 fld     dword ptr [edi+444h]
  float unk_1096;         ///< offset=0x448 .text:001B6860                 fcomp   dword ptr [edi+448h]
  uint8_t unk_1100[8];    ///< offset=0x44C .text:001B5786                 lea     ecx, [esi+44Ch] & .text:001B5C28                 mov     cl, [edi+esi+44Ch]
  vector3_t unk_1108;     ///< offset=0x454 .text:001B5631                 lea     eax, [esi+454h]
  float unk_1120;         ///< offset=0x460 .text:0015225F                 fadd    dword ptr [edi+460h]
  float unk_1124;         ///< offset=0x464 .text:0015226E                 fadd    dword ptr [edi+464h]
  float unk_1128;         ///< offset=0x468 .text:0015227D                 fadd    dword ptr [edi+468h]
  float unk_1132;         ///< offset=0x46C .text:0015228C                 fadd    dword ptr [edi+46Ch]
  float unk_1136;         ///< offset=0x470 .text:0015229B                 fadd    dword ptr [edi+470h]
  float unk_1140;         ///< offset=0x474 .text:001522AA                 fadd    dword ptr [edi+474h]
  uint32_t unk_1144;      ///< offset=0x478 .text:001B80DA                 test    [ebx+478h], edx
} vehicle_data_t;

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

// OBJE -> PROJ
/// size=0x228
typedef struct {
  object_data_t object;   ///< offset=0x000
  char unk_420[0x38];     ///< offset=0x1A4
  uint32_t unk_476;       ///< offset=0x1DC .text:000F7CBE                 mov     ecx, [eax+1DCh]
  uint16_t unk_480;       ///< offset=0x1E0 .text:000F7E4B                 cmp     si, [eax+1E0h]   type of some sort, also see projectile_collision
  uint16_t unk_482;       ///< offset=0x1E2 .text:000F8D84                 mov     [esi+1E2h], bx
  datum_handle_t unk_484; ///< offset=0x1E4 .text:000F8D90                 mov     [esi+1E4h], eax
  datum_handle_t unk_488; ///< offset=0x1E8 .text:000F7D44                 mov     [eax+1E8h], ecx
  uint32_t unk_492;       ///< offset=0x1EC .text:000F9CAC                 mov     eax, [ebx+1ECh]  index into [ebx+eax*4+0FCh]
  float unk_496;          ///< offset=0x1F0 .text:000F8A91                 fmul    dword ptr [edi+1F0h]
  float unk_500;          ///< offset=0x1F4 .text:000F8DEC                 fstp    dword ptr [esi+1F4h]
  float unk_504;          ///< offset=0x1F8 .text:000F9DBD                 fld     dword ptr [ebx+1F8h]
  float unk_508;          ///< offset=0x1FC .text:000F8E15                 fstp    dword ptr [esi+1FCh]
  float unk_512;          ///< offset=0x200 .text:000F7F66                 fld     dword ptr [esi+200h]
  float unk_516;          ///< offset=0x204 .text:000F8702                 mov     dword ptr [esi+204h], 3F800000h
  float unk_520;          ///< offset=0x208 .text:000F86AB                 fstp    dword ptr [esi+208h]
  float unk_524;          ///< offset=0x20C .text:000F8677                 fstp    dword ptr [esi+20Ch]
  float unk_528;          ///< offset=0x210 .text:000FA304                 fcomp   dword ptr [ebx+210h]
  float unk_532;          ///< offset=0x214 .text:000F85E7                 fstp    dword ptr [ecx+214h]
  float unk_536;          ///< offset=0x218 .text:000F85F2                 fstp    dword ptr [ecx+218h]
  float unk_540;          ///< offset=0x21C .text:000F85FB                 fstp    dword ptr [ecx+21Ch]
  float unk_544;          ///< offset=0x220 .text:000F8605                 fstp    dword ptr [ecx+220h]
  float unk_548;          ///< offset=0x224 .text:000F860D                 fstp    dword ptr [ecx+224h]
} projectile_data_t;

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

#endif /* HALO_OBJECTS_OBJECTS_H */
