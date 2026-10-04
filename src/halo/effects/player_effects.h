/* player_effects layouts and globals for src/halo/effects/player_effects.c.
 *
 * Repo-local header: the original header path is not binary-proven.  Kept out
 * of types.h because struct definitions there move unrelated VC71 code.
 * Evidence tables: recovery/evidence/player_effect_t.json and
 * recovery/evidence/player_effect_globals_t.json. */

#ifndef HALO_EFFECTS_PLAYER_EFFECTS_H
#define HALO_EFFECTS_PLAYER_EFFECTS_H

#include "../../types.h"

/* One local player's effect state; player_effect_get (0xa2690) returns
 * player_effect_globals + index * 0xec.  +0x18..+0x4f, +0x50..+0x83 and
 * +0x84..+0xcb are filled by whole-block copies (0xa2ab0, 0xa3890, 0xa2ba0). */
typedef struct player_effect_t {
  real field_00[3];         ///< offset=0x00
  real field_0c[3];         ///< offset=0x0C
  int16_t field_18;         ///< offset=0x18  unknown_flash_type_table index
  int16_t field_1a;         ///< offset=0x1A
  uint8_t pad_1c[0xc];      ///< offset=0x1C  never observed accessed
  real field_28;            ///< offset=0x28
  uint16_t field_2c;        ///< offset=0x2C  transition function
  uint8_t pad_2e[0xe];      ///< offset=0x2E  never observed accessed
  real field_3c;            ///< offset=0x3C
  real_argb_color field_40; ///< offset=0x40
  real field_50;            ///< offset=0x50
  uint16_t field_54;        ///< offset=0x54  transition function
  uint8_t pad_56[2];        ///< offset=0x56  never observed accessed
  real field_58;            ///< offset=0x58
  real field_5c;            ///< offset=0x5C
  real field_60;            ///< offset=0x60
  real field_64;            ///< offset=0x64
  real field_68;            ///< offset=0x68
  uint8_t pad_6c[0x18];     ///< offset=0x6C  never observed accessed
  real field_84;            ///< offset=0x84
  uint16_t field_88;        ///< offset=0x88  transition function
  uint8_t pad_8a[2];        ///< offset=0x8A  never observed accessed
  real field_8c;            ///< offset=0x8C
  real field_90;            ///< offset=0x90
  uint8_t pad_94[0xc];      ///< offset=0x94  never observed accessed
  uint16_t field_a0;        ///< offset=0xA0
  uint8_t pad_a2[2];        ///< offset=0xA2  never observed accessed
  real field_a4;            ///< offset=0xA4
  real field_a8;            ///< offset=0xA8
  real field_ac;            ///< offset=0xAC
  uint8_t pad_b0[0x1c];     ///< offset=0xB0  never observed accessed
  real field_cc[4];         ///< offset=0xCC  csmemset 0x10 as one block
  int16_t field_dc;         ///< offset=0xDC
  int16_t field_de;         ///< offset=0xDE  ticks
  int16_t field_e0;         ///< offset=0xE0  ticks
  int16_t field_e2;         ///< offset=0xE2  ticks
  uint8_t field_e4[4];      ///< offset=0xE4
  uint8_t field_e8;         ///< offset=0xE8  bits 0..2
  uint8_t pad_e9[3];        ///< offset=0xE9  never observed accessed
} player_effect_t;
cs(player_effect_t, 0xec);
co(player_effect_t, field_00, 0x00);
co(player_effect_t, field_0c, 0x0C);
co(player_effect_t, field_18, 0x18);
co(player_effect_t, field_1a, 0x1A);
co(player_effect_t, pad_1c, 0x1C);
co(player_effect_t, field_28, 0x28);
co(player_effect_t, field_2c, 0x2C);
co(player_effect_t, pad_2e, 0x2E);
co(player_effect_t, field_3c, 0x3C);
co(player_effect_t, field_40, 0x40);
co(player_effect_t, field_50, 0x50);
co(player_effect_t, field_54, 0x54);
co(player_effect_t, pad_56, 0x56);
co(player_effect_t, field_58, 0x58);
co(player_effect_t, field_5c, 0x5C);
co(player_effect_t, field_60, 0x60);
co(player_effect_t, field_64, 0x64);
co(player_effect_t, field_68, 0x68);
co(player_effect_t, pad_6c, 0x6C);
co(player_effect_t, field_84, 0x84);
co(player_effect_t, field_88, 0x88);
co(player_effect_t, pad_8a, 0x8A);
co(player_effect_t, field_8c, 0x8C);
co(player_effect_t, field_90, 0x90);
co(player_effect_t, pad_94, 0x94);
co(player_effect_t, field_a0, 0xA0);
co(player_effect_t, pad_a2, 0xA2);
co(player_effect_t, field_a4, 0xA4);
co(player_effect_t, field_a8, 0xA8);
co(player_effect_t, field_ac, 0xAC);
co(player_effect_t, pad_b0, 0xB0);
co(player_effect_t, field_cc, 0xCC);
co(player_effect_t, field_dc, 0xDC);
co(player_effect_t, field_de, 0xDE);
co(player_effect_t, field_e0, 0xE0);
co(player_effect_t, field_e2, 0xE2);
co(player_effect_t, field_e4, 0xE4);
co(player_effect_t, field_e8, 0xE8);
co(player_effect_t, pad_e9, 0xE9);

/* Block at player_effect_globals+0x3c4, written only by the
 * scripted_player_effect_* functions (0xa2dc0, 0xa28e0, 0xa2df0, 0xa2e40). */
typedef struct scripted_player_effect_t {
  real field_00[3];  ///< offset=0x00  (0x3c4)
  real field_0c[3];  ///< offset=0x0C  (0x3d0)
  real field_18;     ///< offset=0x18  (0x3dc)
  int16_t field_1c;  ///< offset=0x1C  (0x3e0) ticks remaining
  int16_t field_1e;  ///< offset=0x1E  (0x3e2) ticks total
} scripted_player_effect_t;
cs(scripted_player_effect_t, 0x20);
co(scripted_player_effect_t, field_00, 0x00);
co(scripted_player_effect_t, field_0c, 0x0C);
co(scripted_player_effect_t, field_18, 0x18);
co(scripted_player_effect_t, field_1c, 0x1C);
co(scripted_player_effect_t, field_1e, 0x1E);

/* game_state_malloc("player effects", 0, 0x3ec) at 0xa2700; name from the
 * assert string "player_effect_globals" (0x26ae7c). */
typedef struct player_effect_globals_t {
  player_effect_t effects[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS]; ///< offset=0x000
  real_rgb_color field_3b0;           ///< offset=0x3B0  screen fade colour
  int32_t field_3bc;                  ///< offset=0x3BC  game time
  int16_t field_3c0;                  ///< offset=0x3C0  ticks, -1 = none
  char field_3c2;                     ///< offset=0x3C2
  uint8_t pad_3c3[1];                 ///< offset=0x3C3  never observed accessed
  scripted_player_effect_t scripted;  ///< offset=0x3C4
  uint32_t field_3e4;                 ///< offset=0x3E4  bits 0, 1
  int32_t field_3e8;                  ///< offset=0x3E8  game time
} player_effect_globals_t;
cs(player_effect_globals_t, 0x3ec);
co(player_effect_globals_t, effects, 0x000);
co(player_effect_globals_t, field_3b0, 0x3B0);
co(player_effect_globals_t, field_3bc, 0x3BC);
co(player_effect_globals_t, field_3c0, 0x3C0);
co(player_effect_globals_t, field_3c2, 0x3C2);
co(player_effect_globals_t, pad_3c3, 0x3C3);
co(player_effect_globals_t, scripted, 0x3C4);
co(player_effect_globals_t, field_3e4, 0x3E4);
co(player_effect_globals_t, field_3e8, 0x3E8);

/* player_effect_globals (0x4557ec) is declared char * by kb; typed view. */
#define player_effects ((player_effect_globals_t *)player_effect_globals)

/* Pointer to the static identity real_matrix4x3 at 0x28ca80 (scale 1,
 * forward/left/up basis, zero position). */
#define global_identity4x3 (*(real_matrix4x3 **)0x31fc60)

/* int16 table indexed by player_effect_t.field_18; the entry is the
 * screen_flash type (0xa31d5) and zero disables the flash (0xa2af0). */
#define unknown_flash_type_table ((int16_t *)0x2ef7e0)

#endif
