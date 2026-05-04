
/* 0x1c270 — encounter_get_squad: return pointer to a squad record by index.
 *
 * Validates squad_index is in [0, MAXIMUM_SQUADS_PER_ENCOUNTER) and also
 * less than encounter->squad_count (at encounter+0x6).  Computes the
 * absolute squad index as encounter->squad_base (encounter+0x4) +
 * squad_index, validates it is in [0, MAXIMUM_SQUADS_PER_MAP), then
 * returns squad_array + absolute_index * 0x20.
 *
 * __FILE__ = c:\halo\source\ai\encounters.h (inline helper defined there,
 * compiled as an out-of-line instance here).
 * MAXIMUM_SQUADS_PER_ENCOUNTER = 0x40; MAXIMUM_SQUADS_PER_MAP = 0x400.
 * squad_array (0x5ab278) is a flat game_state_malloc block of 0x8000 bytes
 * (0x400 * 0x20), initialized by FUN_00058eb0.
 *
 * Confirmed: encounter+0x4 = squad_base (int16_t), encounter+0x6 =
 *   squad_count (int16_t).  Each squad record is 0x20 bytes.
 * Confirmed: squad_array pointer at [0x5ab278] (MOVSX EAX,SI; SHL EAX,5;
 *   ADD EAX,[0x5ab278]).
 */
char *FUN_0001c270(char *encounter, int16_t squad_index)
{
  int16_t squad_absolute;

  if (squad_index < 0 || squad_index >= 0x40 ||
      squad_index >= *(int16_t *)(encounter + 6)) {
    display_assert(
      "squad_index>=0 && squad_index<MAXIMUM_SQUADS_PER_ENCOUNTER && "
      "squad_index<encounter->squad_count",
      "c:\\halo\\source\\ai\\encounters.h", 0xdc, 1);
    system_exit(-1);
  }
  squad_absolute = *(int16_t *)(encounter + 4) + squad_index;
  if (squad_absolute < 0 || squad_absolute >= 0x400) {
    display_assert(
      "squad_absolute_index>=0 && squad_absolute_index<MAXIMUM_SQUADS_PER_MAP",
      "c:\\halo\\source\\ai\\encounters.h", 0xdf, 1);
    system_exit(-1);
  }
  return *(char **)0x5ab278 + (int16_t)squad_absolute * 0x20;
}

char *FUN_000210f0(int actor_handle)
{
  int weapon_handle = FUN_0003b270(actor_handle);
  if (weapon_handle != -1) {
    int *obj = (int *)object_get_and_verify_type(weapon_handle, 4);
    return (char *)tag_get(0x77656170, *obj);
  }
  return 0;
}