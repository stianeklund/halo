/* hud_weapon.c — weapon HUD interface (0xd8af0-0xd91f0)
 *
 * Lifted from Halo CE Xbox (cachebeta.xbe).  Owns the weapon-HUD globals
 * buffer (0x1e4 bytes, pointer stored at 0x46bd24) and renders the per-player
 * weapon / crosshair overlays.  Original source: hud_weapon.c.
 */

#include "x87_math.h"

/* Pointer to the weapon-HUD globals buffer (0x1e4 bytes), allocated by
 * FUN_000d8af0 and stored at the fixed global 0x46bd24. */
#define weapon_hud_globals (*(void **)0x46bd24)

/* FUN_000d8af0 (0xd8af0) — allocate the weapon-HUD globals buffer from the
 * game-state heap and stash it at 0x46bd24.  Asserts on allocation failure. */
void FUN_000d8af0(void)
{
  weapon_hud_globals = game_state_malloc("hud weapon interface", 0, 0x1e4);
  if (weapon_hud_globals == 0) {
    display_assert("weapon_hud_globals",
                   "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x6b, 1);
    system_exit(-1);
  }
}

/* FUN_000d8b30 (0xd8b30) — reset the weapon-HUD globals buffer to all-0xff
 * (NONE handles) on new-map initialisation. */
void FUN_000d8b30(void)
{
  if (weapon_hud_globals == 0) {
    display_assert("weapon_hud_globals",
                   "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x73, 1);
    system_exit(-1);
  }
  csmemset(weapon_hud_globals, -1, 0x1e4);
}

/* FUN_000d8b70 (0xd8b70) — old-map teardown hook for the weapon HUD.  The
 * original is an empty body (single RET). */
void FUN_000d8b70(void)
{
}

/* FUN_000d8b80 (0xd8b80) — dispose hook for the weapon HUD.  The original is
 * an empty body (single RET). */
void FUN_000d8b80(void)
{
}

/* FUN_000d8b90 (0xd8b90) — set (nonzero) or clear (zero) bit 0 of the weapon
 * HUD globals flags word at +0x1e0. */
void FUN_000d8b90(char show)
{
  char *g;
  unsigned int value;

  g = (char *)weapon_hud_globals;
  value = *(unsigned int *)(g + 0x1e0);
  if (show != 0) {
    *(unsigned int *)(g + 0x1e0) = value | 1;
    return;
  }
  *(unsigned int *)(g + 0x1e0) = value & ~1u;
}

/* FUN_000d8bc0 (0xd8bc0) — per-local-player weapon-HUD state accessor.
 * Returns &globals[local_player_index] at stride 0x28.  Index in ESI. */
void *FUN_000d8bc0(int16_t local_player_index /* @<esi> */)
{
  if (local_player_index >= 0 && local_player_index < 4) {
    if (weapon_hud_globals == 0) {
      display_assert("weapon_hud_globals",
                     "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x1af, 1);
      system_exit(-1);
    }
    return (char *)weapon_hud_globals + local_player_index * 0x28;
  }
  display_assert(
      "local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
      "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x1ae, 1);
  system_exit(-1);
}

/* FUN_000d8c30 (0xd8c30) — per-local-player accessor into a second globals
 * region: &globals[local_player_index+2] at stride 0x50.  Index in ESI. */
void *FUN_000d8c30(int16_t local_player_index /* @<esi> */)
{
  if (local_player_index >= 0 && local_player_index < 4) {
    if (weapon_hud_globals == 0) {
      display_assert("weapon_hud_globals",
                     "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x1b8, 1);
      system_exit(-1);
    }
    return (char *)weapon_hud_globals + (local_player_index + 2) * 0x50;
  }
  display_assert(
      "local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
      "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x1b7, 1);
  system_exit(-1);
}

/* FUN_000d8ca0 (0xd8ca0) — refresh the tracked weapon object for a local
 * player's HUD.  EAX = current object handle (gate: skip when NONE),
 * ESI = local player index.  Looks up the player record and re-verifies the
 * unit object it references (type mask 3). */
void FUN_000d8ca0(int object_handle /* @<eax> */,
                  int16_t local_player_index /* @<esi> */)
{
  int player_index;
  void *player_datum;

  if (object_handle != -1) {
    player_index = local_player_get_player_index(local_player_index);
    if (player_index == -1) {
      object_try_and_get_and_verify_type(player_index, 3);
      return;
    }
    player_index = local_player_get_player_index(local_player_index);
    player_datum = datum_get(*(data_t **)0x5aa6d4, player_index);
    object_try_and_get_and_verify_type(*(int *)((char *)player_datum + 0x34), 3);
  }
}

/* FUN_000d8fd0 (0xd8fd0) — return the filename portion of a path (text after
 * the last '\\'), or the whole string when there is no separator. */
char *FUN_000d8fd0(char *path)
{
  char *sep;

  sep = strrchr(path, '\\');
  if (sep != 0) {
    return sep + 1;
  }
  return path;
}
