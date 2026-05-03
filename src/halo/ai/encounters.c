/* 0x005ddc0 — encounter tally reset pass.
 * Iterates ALL encounter records (first_pass=false in binary — no active-enemy
 * filter at +0xd). For each encounter: checks whether the active encounter
 * index changed OR the enemy-visible flag (+0x20 bit 0, inverted) is clear.
 * If either condition holds, calls FUN_0005d910 to reset that encounter's
 * vote tallies.
 *
 * Stack layout (0x18 frame):
 *   EBP-0x18..EBP-0x09 = data_iter_t iter (iter.datum_handle at EBP-0x10)
 *   EBP-0x08 = encounter_handle copy (local_c)
 *   EBP-0x04 = first-pass flag (local_8), false on entry
 *
 * Note: the binary uses a shared inner-loop pattern with encounter_update
 * (FUN_0005de80), controlled by a first_pass flag at EBP-0x4.  When
 * first_pass=0 (this function) the +0xd filter is skipped and every
 * encounter is visited.  When first_pass=1 (encounter_update) only
 * encounters with +0xd != 0 are visited. */
void FUN_0005ddc0(void)
{
  data_iter_t iter;
  void *encounter;
  int encounter_handle;
  int encounter_handle_copy;
  scenario_t *scenario;
  void *tag_elem;

  scenario = global_scenario_get();
  if (*(char *)((char *)ai_globals + 1) != '\0') {
    data_iterator_new(&iter, encounter_data);
  }
  do {
    if (*(char *)((char *)ai_globals + 1) == '\0') {
      return;
    }
    encounter = data_iterator_next(&iter);
    encounter_handle = (int)iter.datum_handle;
    encounter_handle_copy = encounter_handle;
    if (encounter == NULL) {
      return;
    }
    tag_elem = tag_block_get_element((char *)scenario + 0x42c,
                                     encounter_handle & 0xffff, 0xb0);
    if (((encounter_active_index ^ encounter_handle_copy) & 0xffff) == 0 ||
        (~*(uint8_t *)((char *)tag_elem + 0x20) & 1) != 0) {
      FUN_0005d910(encounter_handle_copy, -1, -1);
    }
  } while (1);
}

/* 0x005de80 — per-tick encounter update.
 * Every 30 ticks flushes AI perception state (FUN_0005d890) and actor-update
 * globals (FUN_0005a6e0). Then iterates all encounters with the active-enemy
 * flag, increments encounter_update_counter, and time-slices the full
 * per-encounter pipeline to (encounter_index % 15 == game_tick % 15).
 *
 * Stack layout (0x18 frame):
 *   EBP-0x18..EBP-0x09 = data_iter_t iter (iter.datum_handle at EBP-0x10)
 *   EBP-0x08 = encounter_handle copy (local_c)
 *   EBP-0x04 = first-pass flag (local_8), true on entry */
void FUN_0005de80(void)
{
  data_iter_t iter;
  void *encounter;
  int encounter_handle;
  int encounter_handle_copy;
  int tick;
  int tick_mod15;

  tick = game_time_get();
  if (tick % 0x1e == 0) {
    FUN_0005d890();
    FUN_0005a6e0();
  }
  /* ESI = tick % 15, used for the time-slice comparison below */
  tick_mod15 = tick % 0xf;
  if (*(char *)((char *)ai_globals + 1) != '\0') {
    data_iterator_new(&iter, encounter_data);
  }
  do {
    if (*(char *)((char *)ai_globals + 1) == '\0') {
      return;
    }
    do {
      encounter = data_iterator_next(&iter);
      if (encounter == NULL)
        break;
    } while (*(char *)((char *)encounter + 0xd) == '\0');

    encounter_handle = (int)iter.datum_handle; /* local_14 */
    encounter_handle_copy = encounter_handle; /* local_c */
    if (encounter == NULL) {
      return;
    }
    encounter_update_counter++;
    /* Time-slice: update only if this encounter's slot index matches current
     * tick mod 15 */
    if ((int16_t)((unsigned int)(encounter_handle & 0xffff) % 0xf) ==
        (int16_t)tick_mod15) {
      FUN_0005d420(encounter_handle);
      FUN_0005acf0(encounter_handle_copy);
      FUN_0005c680(encounter_handle_copy);
      FUN_0005ae70(encounter_handle_copy);
      FUN_0005c940(encounter_handle_copy);
      FUN_0005ca80(encounter_handle_copy);
      FUN_0005dc00(encounter_handle_copy);
    }
  } while (1);
}

/* encounters.c — AI encounter management.
 *
 * Corresponds to encounters.obj (XBE address range ~0x5d420–0x5dfb0).
 * __FILE__ = c:\halo\SOURCE\ai\encounters.c (confirmed via display_assert
 * strings).
 *
 * Ported: encounter lifecycle stubs (0x5df80, 0x5df90, 0x5dfa0, 0x5dfb0),
 *         encounter_tally_reset (0x5ddc0), encounter_update (0x5de80).
 * Deferred: encounter_tally_votes (0x5d420) — too complex for this pass.
 */

#include "../../common.h"

/* 0x005df80 — encounter_initialize stub.
 * Called from ai_initialize. No work to do at this level. */
void FUN_0005df80(void)
{
  return;
}

/* 0x005df90 — encounter_dispose stub.
 * Called from ai_dispose. No teardown needed at this level. */
void FUN_0005df90(void)
{
  return;
}

/* 0x005dfa0 — encounter_initialize_for_new_map stub.
 * Called from ai_initialize_for_new_map. */
void FUN_0005dfa0(void)
{
  return;
}

/* 0x005dfb0 — encounter_dispose_from_old_map stub.
 * Called from FUN_00041e80 and ai_dispose_from_old_map. */
void FUN_0005dfb0(void)
{
  return;
}
