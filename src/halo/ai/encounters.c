/* encounters.c — AI encounter management.
 *
 * Covers encounters.obj (XBE address ranges ~0x58fa0–0x597f0 and
 * ~0x5d420–0x5dfb0).  __FILE__ = c:\halo\SOURCE\ai\encounters.c
 * (confirmed via display_assert strings throughout).
 *
 * Globals:
 *   encounter_data  = *(data_t**)0x5ab270  ("encounter", 0x80 × 0x6c)
 *   pursuit_data    = *(data_t**)0x5ab26c  ("ai pursuit", 0x100 × 0x28)
 *   actor_data      = *(data_t**)0x6325a4  ("actor", 0x100 × 0x724)
 *   ai_globals      = *(char**)0x632574
 *     +0x1  : ai_active byte (guard for all encounter entry points)
 *     +0x8  : encounterless list head (int handle)
 *
 * Actor meta offsets (confirmed from disassembly of 0x59740/0x597f0):
 *   actor+0x08 = flag used to compute initial action mask (char)
 *   actor+0x09 = encounterless flag (char)
 *   actor+0x0a = sub-state byte cleared on leave (Uncertain)
 *   actor+0x10 = action mask (uint16_t), set to 0x5a if actor+0x8 != 0
 *   actor+0x2c = next_encounterless handle (int, linked list)
 *   actor+0x34 = encounter_index (int, -1 = NONE)
 *   actor+0x3a = squad_index (int16_t, -1 = NONE)
 *   actor+0x3c = platoon_index (int16_t, -1 = NONE)
 *
 * Ported: FUN_00058a40 (ai_magically_see_players), encounters_dispose (stub),
 * encounter_compute_activation_cluster_bit_vector (dispose pools),
 * encounterless_attach_actor (encounter_enter), encounterless_detach_actor
 * (encounter_leave), encounters_create_for_new_map (tally reset), FUN_0005de80
 * (encounter_update), encounter lifecycle stubs (0x5df80–0x5dfb0).
 */

#include "../../common.h"


/* 0x00054020 — encounter_get_platoon_ptr (FUN_00054020).
 *
 * Returns a pointer to the platoon record for a given encounter and relative
 * platoon index. The platoon record lives in a flat array at *(char**)0x5ab274
 * with each element being 0x10 (16) bytes wide.
 *
 * Lookup process:
 *   1. Assert platoon_index is in [0, MAXIMUM_PLATOONS_PER_ENCOUNTER) and
 *      less than encounter->platoon_count (encounter+0xa).
 *   2. Compute absolute platoon index:
 *        platoon_absolute_index = encounter->platoon_start_index
 * (encounter+0x8)
 *                               + platoon_index
 *   3. Assert platoon_absolute_index is in [0, MAXIMUM_PLATOONS_PER_MAP).
 *   4. Return *(char**)0x5ab274 + platoon_absolute_index * 0x10.
 *
 * Constants (from assert strings + binary):
 *   MAXIMUM_PLATOONS_PER_ENCOUNTER = 0x20 (32)
 *   MAXIMUM_PLATOONS_PER_MAP       = 0x100 (256)
 *   platoon record size            = 0x10 (16 bytes)
 *
 * Source file: c:\halo\source\ai\encounters.h (inline, line 0xea / 0xed).
 * Inferred: the assert filepath is encounters.h, confirming this is an
 * inline helper compiled into encounters.obj. */
char *FUN_00054020(char *encounter, short platoon_index)
{
  short platoon_absolute_index;

  if (platoon_index < 0 || platoon_index >= 0x20 ||
      platoon_index >= *(short *)(encounter + 0xa)) {
    display_assert(
      "platoon_index>=0 && platoon_index<MAXIMUM_PLATOONS_PER_ENCOUNTER"
      " && platoon_index<encounter->platoon_count",
      "c:\\halo\\source\\ai\\encounters.h", 0xea, 1);
    system_exit(-1);
  }
  platoon_absolute_index = *(short *)(encounter + 0x8) + platoon_index;
  if (platoon_absolute_index < 0 || platoon_absolute_index >= 0x100) {
    display_assert("platoon_absolute_index>=0 && "
                   "platoon_absolute_index<MAXIMUM_PLATOONS_PER_MAP",
                   "c:\\halo\\source\\ai\\encounters.h", 0xed, 1);
    system_exit(-1);
  }
  return *(char **)0x5ab274 + (int)platoon_absolute_index * 0x10;
}

/*
 * FUN_00056320 — migrate AI from one encounter to another (ai_migrate).
 *
 * If either debug trace flag (0x5aca57 = ai_trace_detail or 0x5aca59 =
 * ai_trace) is set, logs the migration via:
 *   "[thread]: ai_migrate [encounter1_name] [encounter2_name]"
 * using FUN_00054220 to format each encounter handle into a name buffer via
 * global_scenario_get(), then hs_runtime_get_executing_thread_name() for the
 * thread prefix. The two buffers (local_404, local_204) are pre-pushed before
 * hs_runtime_get_executing_thread_name to match MSVC's argument batching.
 *
 * Finally calls FUN_00055dd0(encounter_handle_1@<eax>, encounter_handle_2, 0,
 * 0) to perform the actual migration.
 *
 * 0x56320 / encounters.obj
 */
void FUN_00056320(int encounter_handle_1, int encounter_handle_2)
{
  char local_404[512];
  char local_204[512];
  scenario_t *uVar1;

  if (*(char *)0x5aca57 || *(char *)0x5aca59) {
    uVar1 = global_scenario_get();
    FUN_00054220(encounter_handle_1, uVar1, local_404, 0x200);
    uVar1 = global_scenario_get();
    FUN_00054220(encounter_handle_2, uVar1, local_204, 0x200);
    error(2, "%s: ai_migrate %s %s", hs_runtime_get_executing_thread_name(),
          local_404, local_204);
  }
  FUN_00055dd0(encounter_handle_1, encounter_handle_2, 0, 0);
}

/*
 * FUN_000563c0 — set squad/actor-variant for an actor given an encounter.
 *
 * Verifies the actor object (type mask 3), retrieves its primary actor handle
 * from [+0x1a4] (fallback [+0x1a8]), then looks up the actor's squad and
 * variant tags. Calls FUN_000559a0 to find the best matching squad index
 * given the encounter_handle. If a valid squad is found and conditions allow,
 * updates the actor's squad assignment via FUN_0003baa0, and if do_migrate is
 * set, migrates the actor via FUN_00036dc0.
 *
 * actor_datum@<eax>: actor object datum handle.
 *
 * 0x563c0 / encounters.obj
 */
void FUN_000563c0(int actor_datum, unsigned int encounter_handle,
                  char do_migrate, int param_3)
{
  char *obj;
  int iVar4;
  void *actr_tag;
  void *actv_tag;
  char *aptr;
  char cVar5;
  int16_t squad_result;

  obj = (char *)object_get_and_verify_type(actor_datum, 3);
  iVar4 = *(int *)(obj + 0x1a4);
  if (iVar4 == -1)
    iVar4 = *(int *)(obj + 0x1a8);
  if (iVar4 == -1 || encounter_handle == 0xffffffff)
    return;

  aptr = (char *)datum_get(actor_data, iVar4);
  actr_tag = tag_get(0x61637472, *(int *)(aptr + 0x58));
  actv_tag = tag_get(0x61637476, *(int *)(aptr + 0x5c));
  cVar5 =
    (char)(((*(unsigned int *)(aptr + 0x34) ^ (encounter_handle & 0xffff)) &
            0xffff) == 0 ?
             1 :
             0);

  squad_result = FUN_000559a0(encounter_handle, *(int *)(aptr + 0x34),
                              *(int16_t *)(aptr + 0x3a), actr_tag, actv_tag,
                              cVar5, (const void *)0x25c8ec);
  if (squad_result == (int16_t)-1)
    return;

  if (cVar5 == 0 || squad_result != *(int16_t *)(aptr + 0x3a)) {
    FUN_0003baa0(iVar4, (int16_t)(encounter_handle & 0xffff), squad_result);
    if (do_migrate != 0)
      FUN_00036dc0(iVar4, param_3, 0);
  }
}

/*
 * FUN_00056790 — debug-logged wrapper for game_allegiance_remove.
 *
 * If AI trace flag (0x5aca59) is set, logs the removal using the MSVC
 * pre-push optimization (team_a/team_b pushed before thread_name call).
 * If both params are not -1, calls game_allegiance_remove.
 *
 * 0x56790 / encounters.obj
 */
void FUN_00056790(int16_t param_1, int16_t param_2)
{
  if (*(char *)0x5aca59) {
    error(2, "%s: ai_allegiance_remove %d %d",
          hs_runtime_get_executing_thread_name(), (int)param_1, (int)param_2);
  }
  if (param_1 != (int16_t)-1 && param_2 != (int16_t)-1)
    game_allegiance_remove(param_1, param_2);
}

/*
 * FUN_000567e0 — check that two teams are both allied and friendly.
 * Returns true iff param_1 != -1 AND param_2 != -1 AND game_team_is_ally
 * AND game_allegiance_get_team_is_friendly both return true.
 * 0x567e0 / encounters.obj
 */
bool FUN_000567e0(int16_t param_1, int16_t param_2)
{
  if (param_1 != (int16_t)-1 && param_2 != (int16_t)-1 &&
      game_team_is_ally(param_1, param_2) &&
      game_allegiance_get_team_is_friendly(param_1, param_2))
    return 1;
  return 0;
}

/*
 * FUN_00056880 — count actors in encounters with squad_type==9 and the
 * given actor handle. Iterates all encounters with flag=1, checks each
 * actor's field_0x6c (squad type) and field_0x9c (actor handle).
 * Returns the count of matching actors.
 *
 * 0x56880 / encounters.obj
 */
short FUN_00056880(int param_1)
{
  char local_20[28];
  int iVar1;
  short sVar2;

  sVar2 = 0;
  encounter_iterator_next(local_20, 1);
  iVar1 = FUN_00059b50(local_20);
  while (iVar1 != 0) {
    if (*(short *)((char *)iVar1 + 0x6c) == 9 &&
        *(int *)((char *)iVar1 + 0x9c) == param_1)
      sVar2 = (short)(sVar2 + 1);
    iVar1 = FUN_00059b50(local_20);
  }
  return sVar2;
}

/*
 * FUN_000568e0 — make all actors in an encounter exit their vehicles.
 *
 * If AI trace (0x5aca59) is set, logs via pre-push pattern:
 *   "[thread]: ai_exit_vehicle [encounter_name]"
 * Then iterates actors in the encounter via FUN_00054680/FUN_00054750.
 * For each actor whose field_0x158 != -1 and unit handle (field_0x18) != -1,
 * calls unit_try_and_exit_seat on the unit handle.
 *
 * 0x568e0 / encounters.obj
 */
void FUN_000568e0(int param_1)
{
  char local_11c[256];
  char local_1c[24];
  void *uVar1;
  int iVar2;
  int unit_handle;

  if (*(char *)0x5aca59) {
    uVar1 = global_scenario_get();
    FUN_00054220(param_1, uVar1, local_11c, 0x100);
    error(2, "%s: ai_exit_vehicle %s", hs_runtime_get_executing_thread_name(),
          local_11c);
  }
  FUN_00054680(param_1, local_1c);
  iVar2 = FUN_00054750(local_1c);
  while (iVar2 != 0) {
    if (*(int *)((char *)iVar2 + 0x158) != -1) {
      unit_handle = *(int *)((char *)iVar2 + 0x18);
      if (unit_handle != -1)
        unit_try_and_exit_seat(unit_handle);
    }
    iVar2 = FUN_00054750(local_1c);
  }
}

/*
 * FUN_00056980 — set braindead state for all actors in an encounter.
 *
 * If AI trace (0x5aca59) is set, logs via pre-push pattern:
 *   "[thread]: ai_braindead [encounter_name] [true|false]"
 * If param_1 != -1, iterates actors via FUN_00054680/FUN_00054750 and
 * calls actor_braindead(actor_handle, param_2) for each actor. The actor
 * handle is at local_1c+0x10 (offset 0x10 into the iterator state).
 * param_2 = 0 means un-braindead; non-zero means braindead.
 *
 * 0x56980 / encounters.obj
 */
void FUN_00056980(int param_1, char param_2)
{
  char local_11c[256];
  char local_1c[16];
  void *uVar1;
  int iVar3;

  if (*(char *)0x5aca59) {
    uVar1 = global_scenario_get();
    FUN_00054220(param_1, uVar1, local_11c, 0x100);
    error(2, "%s: ai_braindead %s %s", hs_runtime_get_executing_thread_name(),
          local_11c, param_2 ? (void *)0x25c530 : (void *)0x25c52c);
  }
  if (param_1 != -1) {
    FUN_00054680(param_1, local_1c);
    iVar3 = FUN_00054750(local_1c);
    while (iVar3 != 0) {
      actor_braindead(*(int *)(local_1c + 0x10), param_2);
      iVar3 = FUN_00054750(local_1c);
    }
  }
}

/*
 * FUN_00056b20 — set/clear the ai_disregard bit (0x400) on actors by unit.
 * Iterates units in the encounter via FUN_000ce450/FUN_000ce320.
 * For each biped/vehicle (type mask 3), sets or clears field_0x1b4 bit 0x400.
 * param_2 != 0: set (disregard on); param_2 == 0: clear (disregard off).
 * Logs "[thread]: ai_disregard <some guys> [true|false]" if trace is on.
 * 0x56b20 / encounters.obj
 */
void FUN_00056b20(int param_1, char param_2)
{
  int local_8;
  int iVar1;
  const void *puVar2;

  iVar1 = FUN_000ce450(param_1, &local_8);
  if (*(char *)0x5aca59) {
    puVar2 = param_2 ? (const void *)0x25c530 : (const void *)0x25c52c;
    error(2, "%s: ai_disregard <some guys> %s",
          hs_runtime_get_executing_thread_name(), puVar2);
  }
  while (iVar1 != -1) {
    iVar1 = (int)object_try_and_get_and_verify_type(iVar1, 3);
    if (iVar1 != 0) {
      if (param_2 == '\0')
        *(unsigned int *)((char *)iVar1 + 0x1b4) &= ~0x400u;
      else
        *(unsigned int *)((char *)iVar1 + 0x1b4) |= 0x400u;
    }
    iVar1 = FUN_000ce320(param_1, &local_8);
  }
}

/*
 * FUN_00056bc0 — set/clear the ai_prefer_target bit (0x800) on actors by unit.
 * Identical to FUN_00056b20 but uses bit 0x800 instead of 0x400.
 * Logs "[thread]: ai_prefer_target <some guys> [true|false]" if trace is on.
 * 0x56bc0 / encounters.obj
 */
void FUN_00056bc0(int param_1, char param_2)
{
  int local_8;
  int iVar1;
  const void *puVar2;

  iVar1 = FUN_000ce450(param_1, &local_8);
  if (*(char *)0x5aca59) {
    puVar2 = param_2 ? (const void *)0x25c530 : (const void *)0x25c52c;
    error(2, "%s: ai_prefer_target <some guys> %s",
          hs_runtime_get_executing_thread_name(), puVar2);
  }
  while (iVar1 != -1) {
    iVar1 = (int)object_try_and_get_and_verify_type(iVar1, 3);
    if (iVar1 != 0) {
      if (param_2 == '\0')
        *(unsigned int *)((char *)iVar1 + 0x1b4) &= ~0x800u;
      else
        *(unsigned int *)((char *)iVar1 + 0x1b4) |= 0x800u;
    }
    iVar1 = FUN_000ce320(param_1, &local_8);
  }
}

/*
 * FUN_00056e40 — clear the target-selection field (0x1d4) for all actors.
 * Logs "[thread]: ai_try_to_fight_nothing [encounter]", then iterates
 * encounter actors and clears field_0x1d4 (short) to 0.
 * 0x56e40 / encounters.obj
 */
void FUN_00056e40(int param_1)
{
  char local_11c[256];
  char local_1c[24];
  void *uVar1;
  int iVar2;

  if (*(char *)0x5aca59) {
    uVar1 = global_scenario_get();
    FUN_00054220(param_1, uVar1, local_11c, 0x100);
    error(2, "%s: ai_try_to_fight_nothing %s",
          hs_runtime_get_executing_thread_name(), local_11c);
  }
  FUN_00054680(param_1, local_1c);
  iVar2 = FUN_00054750(local_1c);
  while (iVar2 != 0) {
    *(short *)((char *)iVar2 + 0x1d4) = 0;
    iVar2 = FUN_00054750(local_1c);
  }
}

/*
 * FUN_00056ed0 — set all actors to target a given encounter (ai_try_to_fight).
 * Logs "[thread]: ai_try_to_fight [enc1] [enc2]", then iterates encounter
 * actors in param_1 and sets field_0x1d4=1, field_0x1d8=param_2.
 * 0x56ed0 / encounters.obj
 */
void FUN_00056ed0(int param_1, int param_2)
{
  char local_21c[256];
  char local_11c[256];
  char local_1c[24];
  void *uVar1;
  int iVar2;

  if (*(char *)0x5aca59) {
    uVar1 = global_scenario_get();
    FUN_00054220(param_1, uVar1, local_21c, 0x100);
    uVar1 = global_scenario_get();
    FUN_00054220(param_2, uVar1, local_11c, 0x100);
    error(2, "%s: ai_try_to_fight %s %s",
          hs_runtime_get_executing_thread_name(), local_21c, local_11c);
  }
  FUN_00054680(param_1, local_1c);
  iVar2 = FUN_00054750(local_1c);
  while (iVar2 != 0) {
    *(short *)((char *)iVar2 + 0x1d4) = 1;
    *(int *)((char *)iVar2 + 0x1d8) = param_2;
    iVar2 = FUN_00054750(local_1c);
  }
}

/*
 * FUN_00056fa0 — set target-selection field to 2 (ai_try_to_fight_player).
 * Logs "[thread]: ai_try_to_fight_player [encounter]", then iterates
 * encounter actors and sets field_0x1d4 (short) to 2.
 * 0x56fa0 / encounters.obj
 */
void FUN_00056fa0(int param_1)
{
  char local_11c[256];
  char local_1c[24];
  void *uVar1;
  int iVar2;

  if (*(char *)0x5aca59) {
    uVar1 = global_scenario_get();
    FUN_00054220(param_1, uVar1, local_11c, 0x100);
    error(2, "%s: ai_try_to_fight_player %s",
          hs_runtime_get_executing_thread_name(), local_11c);
  }
  FUN_00054680(param_1, local_1c);
  iVar2 = FUN_00054750(local_1c);
  while (iVar2 != 0) {
    *(short *)((char *)iVar2 + 0x1d4) = 2;
    iVar2 = FUN_00054750(local_1c);
  }
}

/*
 * FUN_00057030 — enable or disable charge for all actors in an encounter.
 * Logs "[thread]: ai_allow_charge [encounter] [true|false]", then sets
 * field_0x1cb (bool) in each actor to (param_2 == 0) — i.e., 1 when
 * charge is being disallowed, 0 when it is being allowed.
 * 0x57030 / encounters.obj
 */
void FUN_00057030(int param_1, char param_2)
{
  char local_11c[256];
  char local_1c[24];
  void *uVar1;
  int iVar3;

  if (*(char *)0x5aca59) {
    uVar1 = global_scenario_get();
    FUN_00054220(param_1, uVar1, local_11c, 0x100);
    error(2, "%s: ai_allow_charge %s %s",
          hs_runtime_get_executing_thread_name(), local_11c,
          param_2 ? (const char *)0x25c530 : (const char *)0x25c52c);
  }
  FUN_00054680(param_1, local_1c);
  iVar3 = FUN_00054750(local_1c);
  while (iVar3 != 0) {
    *(unsigned char *)((char *)iVar3 + 0x1cb) = (param_2 == '\0') ? 1 : 0;
    iVar3 = FUN_00054750(local_1c);
  }
}

/*
 * FUN_00057230 — advance command list for all actors in an encounter.
 * Logs "[thread]: ai_command_list_advance [encounter]", then iterates
 * encounter actors via FUN_00054680/FUN_00054750 and calls
 * FUN_00017090(actor_handle) for each. Actor handle is at local_1c+0x10.
 * 0x57230 / encounters.obj
 */
void FUN_00057230(int param_1)
{
  char local_11c[256];
  char local_1c[16];
  void *uVar1;
  int iVar2;

  if (*(char *)0x5aca59) {
    uVar1 = global_scenario_get();
    FUN_00054220(param_1, uVar1, local_11c, 0x100);
    error(2, "%s: ai_command_list_advance %s",
          hs_runtime_get_executing_thread_name(), local_11c);
  }
  FUN_00054680(param_1, local_1c);
  iVar2 = FUN_00054750(local_1c);
  while (iVar2 != 0) {
    FUN_00017090(*(int *)(local_1c + 0x10));
    iVar2 = FUN_00054750(local_1c);
  }
}

/*
 * FUN_000572c0 — advance command list for the actor attached to a unit.
 * Logs "[thread]: ai_command_list_advance_by_unit <some unit>". If
 * param_1 != -1 and the object has an actor at field_0x1a4 (or 0x1a8),
 * calls FUN_00017090 on that actor handle.
 * 0x572c0 / encounters.obj
 */
void FUN_000572c0(int param_1)
{
  int iVar2;

  if (*(char *)0x5aca59)
    error(2, "%s: ai_command_list_advance_by_unit <some unit>",
          hs_runtime_get_executing_thread_name());

  if (param_1 != -1) {
    iVar2 = (int)object_try_and_get_and_verify_type(param_1, 3);
    if (iVar2 != 0) {
      if (*(int *)((char *)iVar2 + 0x1a4) != -1) {
        FUN_00017090(*(int *)((char *)iVar2 + 0x1a4));
        return;
      }
      if (*(int *)((char *)iVar2 + 0x1a8) != -1)
        FUN_00017090(*(int *)((char *)iVar2 + 0x1a8));
    }
  }
}

/* 0x00058a40 — ai_magically_see_players (FUN_00058a40).
 *
 * Forces all active players to be "magically seen" by the encounter
 * specified by combined_handle.  This overrides normal AI perception rules
 * so that every actor in the encounter immediately knows where all players
 * are, regardless of line-of-sight.
 *
 * Iff the AI trace flag at 0x5aca59 is non-zero, formats the encounter name
 * into a 256-byte stack buffer via FUN_00054220 then logs:
 *   "[scenario_tag_name]: ai_magically_see_players [encounter_name]"
 * via console_printf (channel 2).
 *
 * Then, iff combined_handle != -1, walks the player data pool
 * (*(data_t**)0x5aa6d4) using data_iterator_new / data_iterator_next and
 * calls FUN_00055110(combined_handle, player+0x34) for each live player.
 *
 * Confirmed:
 *   - ESI = param_1 throughout (callee-saved, loaded at 0x58a51).
 *   - global_scenario_get() at 0x58a62 takes 0 args; return in EAX.
 *   - Pre-push pattern: 0x100 and local_114 pushed before global_scenario_get
 *     for subsequent FUN_00054220 call; ADD ESP,0x10 at 0x58a74 cleans 4 args.
 *   - Pre-push pattern: local_114 pushed before
 * hs_runtime_get_executing_thread_name (0-arg) as 4th arg to console_printf;
 * ADD ESP,0x10 at 0x58a8a cleans 4 dwords.
 *   - console_printf(2, fmt, cb980_result, name_buf): first %s = scenario
 *     tag name from hs_runtime_get_executing_thread_name, second %s = encounter
 * name in name_buf.
 *   - MOV EDX,[0x005aa6d4] dereferences player_data before data_iterator_new.
 *   - player+0x34 is the field passed as arg2 to FUN_00055110.
 */
void FUN_00058a40(int combined_handle)
{
  char name_buf[256];
  char iter_buf[16];
  char *player;

  if (*(char *)0x5aca59 != '\0') {
    FUN_00054220((unsigned int)combined_handle, (void *)global_scenario_get(),
                 name_buf, 0x100);
    console_printf(2, "%s: ai_magically_see_players %s",
                   (const char *)hs_runtime_get_executing_thread_name(),
                   name_buf);
  }
  if (combined_handle != -1) {
    data_iterator_new((data_iter_t *)iter_buf, *(data_t **)0x5aa6d4);
    player = (char *)data_iterator_next((data_iter_t *)iter_buf);
    while (player != (char *)0) {
      FUN_00055110(combined_handle, *(int *)(player + 0x34));
      player = (char *)data_iterator_next((data_iter_t *)iter_buf);
    }
  }
}

/* 0x00058fa0 — encounter_dispose stub.
 * Called from ai_dispose (0x3f6f0). No teardown needed at this level.
 * Binary: single RET instruction. */
void encounters_dispose(void)
{
  return;
}

/* 0x00058fb0 — encounters_dispose_from_old_map.
 * Called from ai_dispose_from_old_map (0x3f720) and ai_handle_editing.
 * Invalidates the encounter_data and pursuit_data pools via
 * data_make_invalid.  Uses raw address loads (MOV r32,[0x5ab270] /
 * PUSH r32 / CALL 0x119550) — the pool pointers are passed by value.
 *
 * Confirmed: ADD ESP,0x8 after two CALL 0x119550 instructions (combined
 * stack cleanup for both calls). */
void encounter_compute_activation_cluster_bit_vector(void)
{
  data_make_invalid(*(data_t **)0x5ab270); /* encounter_data */
  data_make_invalid(*(data_t **)0x5ab26c); /* pursuit_data */
}

/* 0x59480 — Remove actor from encounter membership.
 * Unlinks the actor from the encounter's member linked list (rooted at
 * encounter+0x14, chained via actor+0x2c). When flag==0, also decrements
 * original counts on encounter, squad, leader, and platoon. Clears the
 * actor's encounter/squad/platoon fields and marks the encounter dirty. */
void encounter_detach_actor(int actor_handle, char flag)
{
  char *actor;
  char *encounter;
  int *piVar;
  int iVar;
  char *squad;
  char *platoon;

  if (*(char *)(*(char **)0x632574 + 1) == '\0')
    return;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(int *)(actor + 0x34) == -1)
    return;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, *(int *)(actor + 0x34));
  piVar = (int *)(encounter + 0x14);
  iVar = *(int *)(encounter + 0x14);
  while (iVar != actor_handle) {
    if (iVar == -1) {
      display_assert("*actor_index_reference!=NONE",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x220, 1);
      system_exit(-1);
    }
    iVar = (int)datum_get(*(data_t **)0x6325a4, *piVar);
    piVar = (int *)(iVar + 0x2c);
    iVar = *piVar;
  }
  *piVar = *(int *)(actor + 0x2c);

  if (flag == '\0') {
    squad = encounter_get_squad(encounter, *(int16_t *)(actor + 0x3a));

    if (*(int16_t *)(encounter + 0x18) < 1) {
      display_assert("encounter->original_count > 0",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x22c, 1);
      system_exit(-1);
    }
    (*(int16_t *)(encounter + 0x18))--;

    if (*(char *)(actor + 0x1c) != '\0') {
      if (*(int16_t *)(encounter + 0x1c) < 1) {
        display_assert("encounter->unique_leader_count > 0",
                       "c:\\halo\\SOURCE\\ai\\encounters.c", 0x231, 1);
        system_exit(-1);
      }
      (*(int16_t *)(encounter + 0x1c))--;
    }

    if (*(int16_t *)(squad + 0x16) < 1) {
      display_assert("squad->original_count > 0",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x235, 1);
      system_exit(-1);
    }
    (*(int16_t *)(squad + 0x16))--;

    if (*(int16_t *)(actor + 0x3c) != -1) {
      platoon = FUN_00054020(encounter, *(int16_t *)(actor + 0x3c));
      if (*(int16_t *)(platoon + 4) < 1) {
        display_assert("platoon->original_count > 0",
                       "c:\\halo\\SOURCE\\ai\\encounters.c", 0x23d, 1);
        system_exit(-1);
      }
      (*(int16_t *)(platoon + 4))--;
    }
  }

  *(int *)(actor + 0x2c) = -1;
  *(int *)(actor + 0x34) = -1;
  *(int16_t *)(actor + 0x3c) = -1;
  *(int16_t *)(actor + 0x3a) = -1;
  *(char *)(encounter + 0x28) = 1;
}

/* 0x59630 — Validate encounter/actor linkage for a unit and optionally set
 * the encounter's team index from the unit. If the unit has a secondary actor
 * (field 0x1a8), validates that actor's encounter matches. Otherwise validates
 * the primary actor (field 0x1a4) encounter and unit linkage. When
 * game_engine_running returns false and encounter->team is zero, copies the
 * unit's team and notifies via ai_update_team_status if members exist. */
void encounter_attach_unit(int encounter_index, int unit_index)
{
  char *encounter;
  char *unit;
  char *actor;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_index);
  unit = (char *)object_get_and_verify_type(unit_index, 3);

  if (*(int *)(unit + 0x1a8) != -1) {
    actor = (char *)datum_get(*(data_t **)0x6325a4, *(int *)(unit + 0x1a8));
    if (*(int *)(actor + 0x34) != encounter_index) {
      display_assert("actor->meta.encounter_index == encounter_index",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x25b, 1);
      system_exit(-1);
    }
  } else {
    actor = (char *)datum_get(*(data_t **)0x6325a4, *(int *)(unit + 0x1a4));
    if (*(int *)(actor + 0x34) != encounter_index) {
      display_assert("actor->meta.encounter_index == encounter_index",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x261, 1);
      system_exit(-1);
    }
    if (*(int *)(actor + 0x18) != unit_index) {
      display_assert("actor->meta.unit_index == unit_index",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x262, 1);
      system_exit(-1);
    }
  }

  if (!game_engine_running() && *(int16_t *)(encounter + 2) == 0) {
    *(int16_t *)(encounter + 2) = *(int16_t *)(unit + 0x68);
    if (*(int *)(encounter + 0x14) != -1)
      ai_update_team_status();
  }
}

/* 0x00059740 — encounter_enter (add actor to encounterless list).
 * Places an actor into the "encounterless" linked list maintained in
 * ai_globals.  Guards on ai_globals->field_1 (ai_active byte).
 *
 * Preconditions (asserted in binary):
 *   actor->meta.encounter_index == NONE  (actor+0x34 == -1)
 *   actor->meta.encounterless == 0       (actor+0x9 == 0)
 *
 * Side effects:
 *   actor+0x2c = old ai_globals->field_8 (prepend to encounterless list)
 *   ai_globals->field_8 = actor_handle   (new list head)
 *   actor+0x9 = 1                        (mark as encounterless)
 *   actor+0x10 = 0x5a if actor+0x8 != 0 else 0  (initial action mask)
 *   Calls actor_flush_position_indices(actor_handle) to reset action state.
 *
 * NEG/SBB/AND pattern at 0x597d1–0x597da:
 *   DL = actor+0x8; NEG DL sets CF if DL != 0; SBB EDX,EDX → EDX=-1 or 0;
 *   AND EDX,0x5a → EDX = 0x5a (if flag) or 0 (if not). */
void encounterless_attach_actor(int actor_handle)
{
  char *actor;
  char *ai_globals;

  ai_globals = *(char **)0x632574;
  if (*(char *)(ai_globals + 1) == '\0') {
    return;
  }
  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);

  /* Assert actor is not already in an encounter */
  if (*(int *)(actor + 0x34) != -1) {
    display_assert("actor->meta.encounter_index==NONE",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x2f6, 1);
    system_exit(-1);
  }

  /* Assert actor is not already encounterless */
  if (*(char *)(actor + 9) != '\0') {
    display_assert("!actor->meta.encounterless",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x2f7, 1);
    system_exit(-1);
  }

  /* Prepend actor to the encounterless list */
  *(int *)(actor + 0x2c) = *(int *)(ai_globals + 8);
  *(int *)(ai_globals + 8) = actor_handle;

  /* Set encounterless flag */
  *(char *)(actor + 9) = 1;

  /* Compute initial action mask: 0x5a if actor->field_8 is non-zero, else 0.
   * Binary: NEG [ESI+0x8] / SBB EDX,EDX / AND EDX,0x5a / MOV [ESI+0x10],DX */
  *(uint16_t *)(actor + 0x10) =
    (uint16_t)(-(*(char *)(actor + 8) != '\0') & 0x5a);

  actor_flush_position_indices(actor_handle);
}

/* 0x000597f0 — encounter_leave (remove actor from encounterless list).
 * Splices the actor out of the encounterless linked list.
 * Guards on ai_globals->field_1 (ai_active byte).
 *
 * Preconditions (asserted in binary):
 *   actor->meta.encounterless != 0    (actor+0x9 != 0)
 *   actor->meta.encounter_index == NONE  (actor+0x34 == -1)
 *   actor->meta.squad_index == NONE   (actor+0x3a == -1)
 *   actor->meta.platoon_index == NONE (actor+0x3c == -1)
 *
 * List walk: starts at &ai_globals->field_8 (the head pointer slot),
 * advances through actor->field_0x2c chain until EAX == actor_handle.
 * Each iteration: datum_get(actor_data, *piVar3) to get next record,
 * then piVar3 = &that_actor->field_0x2c, EAX = *piVar3.
 *
 * Side effects after splice:
 *   *piVar3 = actor->field_0x2c    (link predecessor to successor)
 *   actor+0x9  = 0                 (clear encounterless flag)
 *   actor+0x2c = -1                (next = NONE)
 *   actor+0xa  = 0                 (Uncertain: sub-state byte cleared)
 *
 * Confirmed: ESI = &ai_globals->field_8 at 0x598c0 (ADD ESI,0x8 after
 * MOV ESI,[0x632574]).  Inner loop uses ESI as int* pointer-to-next-slot. */
void encounterless_detach_actor(int actor_handle)
{
  char *actor;
  char *ai_globals;
  char *node;
  int *piVar3;
  int iVar2;

  ai_globals = *(char **)0x632574;
  if (*(char *)(ai_globals + 1) == '\0') {
    return;
  }
  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);

  /* Assert actor is encounterless */
  if (*(char *)(actor + 9) == '\0') {
    display_assert("actor->meta.encounterless",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x30e, 1);
    system_exit(-1);
  }

  /* Assert no encounter, squad, or platoon membership */
  if (*(int *)(actor + 0x34) != -1) {
    display_assert("actor->meta.encounter_index == NONE",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x30f, 1);
    system_exit(-1);
  }
  if (*(int16_t *)(actor + 0x3a) != -1) {
    display_assert("actor->meta.squad_index == NONE",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x310, 1);
    system_exit(-1);
  }
  if (*(int16_t *)(actor + 0x3c) != -1) {
    display_assert("actor->meta.platoon_index == NONE",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x311, 1);
    system_exit(-1);
  }

  /* Walk encounterless list to find the slot pointing to actor_handle.
   * piVar3 starts at the head pointer slot (&ai_globals->field_8). */
  piVar3 = (int *)(ai_globals + 8);
  iVar2 = *piVar3;
  while (iVar2 != actor_handle) {
    if (iVar2 == -1) {
      display_assert("*actor_index_reference!=NONE",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x31c, 1);
      system_exit(-1);
    }
    node = (char *)datum_get(*(data_t **)0x6325a4, *piVar3);
    piVar3 = (int *)(node + 0x2c);
    iVar2 = *piVar3;
  }

  /* Splice out: link predecessor to our successor */
  *piVar3 = *(int *)(actor + 0x2c);

  /* Clear actor's encounterless state */
  *(char *)(actor + 9) = 0;
  *(int *)(actor + 0x2c) = -1;
  *(char *)(actor + 0xa) = 0; /* Uncertain: sub-state byte */
}

/* 0x59930 — Find encounter index by name.
 * Searches the scenario encounter block for the first entry whose name matches
 * the given string (strncmp up to 0x20 bytes). Returns -1 if not found. */
int encounter_get_by_name(char *name)
{
  int *encounters;
  int i;
  void *elem;

  encounters = (int *)global_scenario_get();
  if (encounters != 0) {
    encounters = (int *)((char *)encounters + 0x42c);
    i = 0;
    if (*encounters > 0) {
      do {
        elem = tag_block_get_element(encounters, i, 0xb0);
        if (csstrncmp((char *)elem, name, 0x20) == 0)
          return i;
        i++;
      } while (i < *encounters);
      return -1;
    }
  }
  return -1;
}

/* 0x00059a00 — encounter_clump_iter_new.
 * Initialises a 3-slot int iterator for walking an encounter's clump member
 * list.  Guards on ai_active (ai_globals+1).
 *
 * iter[0] = clump_handle         (the encounter handle or -1 for encounterless)
 * iter[1] = -1                   (current member handle; filled by _next)
 * iter[2] = first_member_handle  (from encounter+0x14, or ai_globals+8 when
 *                                  clump_handle == -1)
 *
 * Call-site verification (callers read iter[1] to compare against
 * actor_handle): PUSH [EBP+0xc]  (param_2 = clump_handle)  → datum_get arg2 YES
 *   PUSH [0x5ab270] (encounter_data)           → datum_get arg1  YES
 *   MOV ECX,[EAX+0x14]                         → encounter->first_member  YES
 *   MOV [ESI+0x8],ECX                          → iter[2]         YES
 *
 * Store-offset table (from disasm MOV [ESI+N]):
 *   ESI+0x0 : param_2 (clump_handle)
 *   ESI+0x4 : 0xffffffff (-1)
 *   ESI+0x8 : encounter->field_0x14 OR ai_globals->field_8 */
void encounter_actor_iterator_new(int *iter, int clump_handle)
{
  char *encounter;
  char *ai_globals;

  ai_globals = *(char **)0x632574;
  if (*(char *)(ai_globals + 1) == '\0') {
    return;
  }
  iter[0] = clump_handle;
  iter[1] = -1;
  if (clump_handle == -1) {
    /* No encounter: seed with encounterless list head */
    iter[2] = *(int *)(ai_globals + 8);
    return;
  }
  encounter = (char *)datum_get(*(data_t **)0x5ab270, clump_handle);
  iter[2] = *(int *)(encounter + 0x14);
}

/* 0x00059a50 — encounter_clump_iter_next.
 * Advances the encounter-clump member iterator and returns a pointer to the
 * next actor record, or NULL when the list is exhausted.
 *
 * iter[1] (current) ← iter[2] (next handle)
 * iter[2]           ← actor+0x2c  (link to following member)
 * returns datum_get(actor_data, iter[1]) or 0 if iter[1] == -1
 *
 * Call-site verification (PUSH ECX/PUSH EDX before CALL 0x119320):
 *   PUSH ECX (handle from iter[2]) → datum_get arg2  YES
 *   PUSH EDX ([0x6325a4])          → datum_get arg1  YES
 *   MOV ECX,[EAX+0x2c]             → actor->next_member  YES
 *   MOV [ESI+0x8],ECX              → iter[2]         YES */
int encounter_actor_iterator_next(int *iter)
{
  int handle;
  char *actor;

  if (*(char *)(*(char **)0x632574 + 1) == '\0') {
    return 0;
  }
  handle = iter[2];
  iter[1] = handle;
  if (handle == -1) {
    return 0;
  }
  actor = (char *)datum_get(*(data_t **)0x6325a4, handle);
  iter[2] = *(int *)(actor + 0x2c);
  return (int)actor;
}

/* 0x00059b10 — actor_iterator_new (extended AI actor iterator init).
 * Initialises a 0x1c-byte extended AI actor iterator.  The first 0x10
 * bytes are a standard data_iter_t (initialised by data_iterator_new on
 * encounter_data at 0x5ab270).  The extra fields are:
 *   iter+0x10 : phase byte  (0=scanning data table, 1=scanning encounterless
 * list) iter+0x11 : filter_flag (0=all actors, 1=player-actors with actor+0x8
 * != 0) iter+0x14 : current datum handle (-1 = none) iter+0x18 : next
 * linked-list handle (-1 = end)
 *
 * Call-site verification (PUSH ECX / PUSH ESI before CALL 0x1197b0):
 *   PUSH ECX ([0x5ab270] = encounter_data) → data_iterator_new arg2 (data)  YES
 *   PUSH ESI (iter = param_1)              → data_iterator_new arg1 (iter)  YES
 *
 * Store-offset table (from disasm MOV [ESI+N]):
 *   ESI+0x10 : 0x0   (phase byte — data-table phase)
 *   ESI+0x18 : -1    (next linked-list handle)
 *   ESI+0x14 : -1    (current handle)
 *   ESI+0x11 : DL    (param_2 = filter_flag) */
void encounter_iterator_next(void *iter, char flag)
{
  char *p = (char *)iter;

  if (*(char *)(*(char **)0x632574 + 1) == '\0') {
    return;
  }
  data_iterator_new((data_iter_t *)iter, *(data_t **)0x5ab270);
  *(char *)(p + 0x10) = 0;
  *(int *)(p + 0x18) = -1;
  *(int *)(p + 0x14) = -1;
  *(char *)(p + 0x11) = flag;
}

/* 0x00059b50 — actor_iterator_next (extended AI actor iterator advance).
 * Returns the next actor record pointer, or NULL when done.  Two-phase:
 *
 * Phase 0 — data table (iter+0x18 == -1 initially):
 *   data_iterator_next advances through the encounter data pool.  For each
 *   encounter record returned, if filter_flag or encounter->field_0xd != 0,
 *   load iter+0x18 from encounter->field_0x14 (the member-chain head).
 *   Then fall into phase 1.
 *   If data_iterator_next returns NULL and iter+0x10 == 0, transition to
 *   phase 1 by seeding iter+0x18 from ai_globals->field_8 and setting
 *   iter+0x10 = 1.
 *
 * Phase 1 — encounterless linked list (iter+0x18 != -1):
 *   Walk via datum_get(actor_data, iter+0x18)->field_0x2c.
 *   If filter_flag==1 and actor+0x8==0: skip (continue with next link).
 *   Stores current handle at iter+0x14.
 *
 * Returns actor record pointer (datum_get result) or 0.
 *
 * Call-site verification (PUSH EAX/PUSH ECX before CALL 0x119320 at 0x59bc3):
 *   PUSH EAX (iter+0x18 current handle) → datum_get arg2 (handle)  YES
 *   PUSH ECX ([0x6325a4] = actor_data)  → datum_get arg1 (data)    YES
 *
 * Key disasm confirmations:
 *   0x59b6a: CMP [ESI+0x18],-1   — check next handle in linked list
 *   0x59b71: PUSH ESI / CALL 0x119810 — data_iterator_next(iter)
 *   0x59b80: TEST [ESI+0x11]     — filter_flag
 *   0x59b84: TEST [EAX+0xd]      — encounter->field_0xd
 *   0x59b8e: MOV ECX,[EAX+0x14] — encounter->first_member_handle
 *   0x59ba0: MOV EDX,[0x632574]  — ai_globals
 *   0x59ba6: MOV EAX,[EDX+0x8]  — ai_globals->encounterless_head
 *   0x59bac: MOV [ESI+0x10],1   — set phase=1 */
int FUN_00059b50(void *iter)
{
  char *p = (char *)iter;
  char *actor;
  char *encounter;
  int handle;

  if (*(char *)(*(char **)0x632574 + 1) == '\0') {
    return 0;
  }

  for (;;) {
    if (*(int *)(p + 0x18) != -1) {
      /* Phase 1: walk linked list of actors */
      goto walk_list;
    }

    /* Phase 0: advance data table */
    encounter = (char *)data_iterator_next((data_iter_t *)iter);
    if (encounter != NULL) {
      /* If filter_flag==0, or this encounter has field_0xd set, load chain */
      if (*(char *)(p + 0x11) == '\0' || *(char *)(encounter + 0xd) != '\0') {
        *(int *)(p + 0x18) = *(int *)(encounter + 0x14);
      }
      /* Re-check iter+0x18 */
      if (*(int *)(p + 0x18) == -1) {
        continue;
      }
      goto walk_list;
    }

    /* data table exhausted — transition to encounterless list if not done */
    if (*(char *)(p + 0x10) != '\0') {
      /* Phase 1 already seeded: jump straight to list walk */
      goto walk_list;
    }
    *(int *)(p + 0x18) = *(int *)(*(char **)0x632574 + 8);
    *(char *)(p + 0x10) = 1;

  walk_list:
    handle = *(int *)(p + 0x18);
    *(int *)(p + 0x14) = handle;
    if (handle == -1) {
      return 0;
    }
    actor = (char *)datum_get(*(data_t **)0x6325a4, handle);
    *(int *)(p + 0x18) = *(int *)(actor + 0x2c);

    if (*(char *)(p + 0x11) == '\0') {
      /* No filter — return this actor */
      return (int)actor;
    }
    /* filter_flag: only return if actor+0x8 != 0 */
    if (*(char *)(actor + 8) != '\0') {
      return (int)actor;
    }
    /* Skip: loop within walk_list, matching asm JMP 0x59bb0.
     * The original returns 0 when the chain ends mid-filter rather than
     * advancing to the next encounter in the same call. */
    goto walk_list;
  }
}

/* 0x00059bf0 — encounter_drain_pursuit_list (FUN_00059bf0).
 *
 * Drains the linked pursuit list attached to the given encounter.
 * The encounter record keeps the head of the pursuit list at +0x38 (int
 * handle). Each pursuit record's next-handle lives at pursuit+0x24.
 *
 * Steps:
 *   1. datum_get(encounter_data, encounter_handle) → encounter ptr (EDI).
 *   2. Load head handle from encounter+0x38 (ESI).
 *   3. While head != -1:
 *        a. datum_get(pursuit_data, head) → pursuit ptr.
 *        b. Advance encounter+0x38 = pursuit+0x24 (next handle).
 *        c. datum_delete(pursuit_data, head) — free the pursuit record.
 *        d. Reload head from encounter+0x38.
 *
 * Confirmed:
 *   - encounter_handle received in EAX (@<eax>).
 *   - encounter_data  = *(data_t**)0x5ab270.
 *   - pursuit_data    = *(data_t**)0x5ab26c.
 *   - encounter+0x38  = pursuit list head (int handle, -1 = empty).
 *   - pursuit+0x24    = next pursuit handle in list.
 */
void FUN_00059bf0(int encounter_handle /* @<eax> */)
{
  char *encounter;
  char *pursuit;
  int pursuit_handle;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
  pursuit_handle = *(int *)(encounter + 0x38);
  while (pursuit_handle != -1) {
    pursuit = (char *)datum_get(*(data_t **)0x5ab26c, pursuit_handle);
    *(int *)(encounter + 0x38) = *(int *)(pursuit + 0x24);
    datum_delete(*(data_t **)0x5ab26c, pursuit_handle);
    pursuit_handle = *(int *)(encounter + 0x38);
  }
}

/* 0x5a050 — squad_initialize_starting_locations (FUN_0005a050).
 * Initializes the starting-location bitfield for a squad. Retrieves the
 * encounter datum and scenario squad definition, then fills the bitfield
 * (at squad_record + 4) with all-ones via csmemset. Finally iterates each
 * starting location element and sets bit i if element+0x13 flag bit 0 is set.
 *
 * Confirmed:
 *   - squad_index via EAX (@<eax>), encounter_handle via ECX (@<ecx>).
 *   - datum_get(encounter_data, encounter_handle) at 0x5a062.
 *   - global_scenario_get() at 0x5a078 (0 args).
 *   - tag_block_get_element(scenario+0x42c, enc_index, 0xb0) at 0x5a083.
 *   - encounter_get_squad(encounter, squad_index) at 0x5a08c → squad record.
 *   - tag_block_get_element(enc_def+0x80, squad_index, 0xe8) at 0x5a0a3.
 *   - Starting-location tag_block at squad_def+0xd0; element size 0x1c.
 *   - csmemset(squad+4, 0xff, ((count+31)>>5)<<2) at 0x5a0c4.
 *   - Loop counter is sign-extended to short (MOVSX ESI,AX at 0x5a105).
 */
void FUN_0005a050(int squad_index /* @<eax> */,
                  int encounter_handle /* @<ecx> */)
{
  char *encounter;
  char *scenario;
  char *encounter_def;
  char *squad_base;
  char *squad_def;
  int *count_ptr;
  int i;
  int loop_var;
  char *element;
  unsigned int *word_ptr;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
  scenario = (char *)global_scenario_get();
  encounter_def = (char *)tag_block_get_element(
    scenario + 0x42c, encounter_handle & 0xffff, 0xb0);
  squad_base = (char *)encounter_get_squad(encounter, (short)squad_index);
  squad_def = (char *)tag_block_get_element(encounter_def + 0x80,
                                            (short)squad_index, 0xe8);

  count_ptr = (int *)(squad_def + 0xd0);
  csmemset(squad_base + 4, -1, ((*count_ptr + 0x1f) >> 5) << 2);

  i = 0;
  loop_var = 0;
  if (0 < *count_ptr) {
    do {
      element = (char *)tag_block_get_element(count_ptr, i, 0x1c);
      if ((*(unsigned char *)(element + 0x13) & 1) != 0) {
        word_ptr = (unsigned int *)(squad_base + 4 + (i >> 5) * 4);
        *word_ptr = *word_ptr | (1u << (i & 0x1f));
      }
      loop_var = loop_var + 1;
      i = (int)(short)loop_var;
    } while (i < *count_ptr);
  }
}

/* 0x5a120 — encounter_initialize_from_definition (FUN_0005a120).
 * Allocates a new encounter record from the encounter data pool, initializes
 * its fields from the scenario encounter definition, then iterates squads and
 * platoons to set up per-squad and per-platoon state. Updates the running
 * squad_counter and platoon_counter accumulators.
 *
 * Confirmed:
 *   - squad_counter passed via EAX (@<eax>), encounter_def via [EBP+0x8],
 *     platoon_counter via [EBP+0xc].
 *   - data_new_at_index(DAT_005ab270) at 0x5a12f; datum_get at 0x5a14d.
 *   - Squad count from encounter_def+0x80 (tag_block); max 0x40 squads.
 *   - Squad accumulator max 0x400 (MAXIMUM_SQUADS_PER_MAP).
 *   - Platoon count from encounter_def+0x8c (tag_block); max 0x20 platoons.
 *   - Platoon accumulator max 0x100 (MAXIMUM_PLATOONS_PER_MAP).
 *   - encounter_get_squad(encounter, squad_index) for squad records.
 *   - FUN_00054020(encounter, platoon_index) for platoon records.
 *   - FUN_0005a050(squad_index @EAX, encounter_handle @ECX) initializes
 *     squad starting locations from the definition.
 *   - _ftol2 at 0x5a289 = (short)(squad_def->field_0x50 * 30.0f).
 *   - tag_block_get_element sizes: 0xe8 for squads, 0xac for platoons.
 */
void FUN_0005a120(short *squad_counter /* @<eax> */, void *encounter_def,
                  short *platoon_counter)
{
  int encounter_handle;
  char *encounter;
  char *squad_record;
  char *squad_def;
  char *platoon_record;
  char *platoon_def;
  short squad_count;
  short platoon_count;
  int i;
  short sVar;
  void *platoon_block;

  encounter_handle = data_new_at_index(*(data_t **)0x5ab270);
  if (encounter_handle == -1) {
    return;
  }
  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);

  *(short *)(encounter + 0x2) = *(short *)((char *)encounter_def + 0x24);
  *(int *)(encounter + 0x14) = -1;
  *(int *)(encounter + 0x38) = -1;
  *(unsigned char *)(encounter + 0x40) =
    (unsigned char)((*(unsigned int *)((char *)encounter_def + 0x20) >> 2) & 1);
  *(unsigned char *)(encounter + 0x41) =
    (unsigned char)((*(unsigned int *)((char *)encounter_def + 0x20) >> 3) & 1);
  *(unsigned char *)(encounter + 0x3c) =
    (unsigned char)((*(unsigned int *)((char *)encounter_def + 0x20) >> 1) & 1);
  *(short *)(encounter + 0x3e) = 0;
  *(char *)(encounter + 0x46) = 0;
  *(char *)(encounter + 0x45) = 0;
  *(int *)(encounter + 0x50) = -1;
  *(char *)(encounter + 0x44) = 0;
  *(int *)(encounter + 0x54) = -1;
  *(int *)(encounter + 0x58) = -1;
  *(char *)(encounter + 0x42) = 1;
  *(int *)(encounter + 0x5c) = -1;
  *(short *)(encounter + 0x20) = 0;
  *(int *)(encounter + 0x10) = -1;

  if (*(int *)((char *)encounter_def + 0x80) > 0x40) {
    display_assert(
      "encounter_definition->squads.count <= MAXIMUM_SQUADS_PER_ENCOUNTER",
      "c:\\halo\\SOURCE\\ai\\encounters.c", 0x5a4, 1);
    system_exit(-1);
  }

  squad_count = *(short *)((char *)encounter_def + 0x80);
  *(short *)(encounter + 0x6) = squad_count;
  *(short *)(encounter + 0x4) = *squad_counter;
  *squad_counter = *squad_counter + squad_count;

  if (*squad_counter > 0x400) {
    display_assert(csprintf((char *)0x5ab100,
                            "overflowed MAXIMUM_SQUADS_PER_MAP (%d)", 0x400),
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x5a8, 1);
    system_exit(-1);
  }

  i = 0;
  if (*(short *)(encounter + 0x6) > 0) {
    do {
      squad_record = (char *)encounter_get_squad(encounter, (short)i);
      squad_def = (char *)tag_block_get_element((char *)encounter_def + 0x80,
                                                (int)(short)i, 0xe8);
      *(char *)(squad_record + 0x11) = 0;
      if ((*(unsigned char *)(squad_def + 0x28) & 8) == 0) {
        *(short *)(squad_record + 0x12) =
          (short)(*(float *)(squad_def + 0x50) * 30.0f);
      } else {
        *(short *)(squad_record + 0x12) = 999;
      }
      *(unsigned char *)(squad_record + 0x10) =
        (unsigned char)((*(unsigned int *)(squad_def + 0x28) >> 5) & 1);
      FUN_0005a050(i /* @<eax> */, encounter_handle /* @<ecx> */);
      if (*(short *)(squad_def + 0x86) > 0 ||
          *(short *)(squad_def + 0x84) > 0) {
        sVar = 999;
        if (*(short *)(squad_def + 0x88) != 0) {
          sVar = *(short *)(squad_def + 0x88);
        }
        *(short *)(squad_record + 0xc) = sVar;
      }
      i = i + 1;
    } while ((short)i < *(short *)(encounter + 0x6));
  }

  if (*(int *)((char *)encounter_def + 0x8c) > 0x20) {
    display_assert(
      "encounter_definition->platoons.count <= MAXIMUM_PLATOONS_PER_ENCOUNTER",
      "c:\\halo\\SOURCE\\ai\\encounters.c", 0x5cb, 1);
    system_exit(-1);
  }

  platoon_count = *(short *)((char *)encounter_def + 0x8c);
  *(short *)(encounter + 0xa) = platoon_count;
  *(short *)(encounter + 0x8) = *platoon_counter;
  *platoon_counter = *platoon_counter + platoon_count;

  if (*platoon_counter > 0x100) {
    display_assert(csprintf((char *)0x5ab100,
                            "overflowed MAXIMUM_PLATOONS_PER_MAP (%d)", 0x100),
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x5cf, 1);
    system_exit(-1);
  }

  platoon_block = (void *)((char *)encounter_def + 0x8c);
  i = 0;
  if (*(short *)(encounter + 0xa) > 0) {
    do {
      platoon_record = (char *)FUN_00054020(encounter, (short)i);
      platoon_def =
        (char *)tag_block_get_element(platoon_block, (int)(short)i, 0xac);
      i = i + 1;
      *(unsigned char *)platoon_record =
        (unsigned char)((*(unsigned int *)(platoon_def + 0x20) >> 2) & 1);
    } while ((short)i < *(short *)(encounter + 0xa));
  }
}

/* FUN_0005a3b0 (0x5a3b0) — Look up actor type from squad definition.
 *
 * Reads the squad's scenario_squad index from squad_def+0x20 (int16_t),
 * bounds-checks it against the scenario block at +0x420 (count at first
 * dword), calls tag_block_get_element(scenario+0x420, index, 0x10),
 * follows +0xc → tag_get('actv', ...) → +0x10 → tag_get('actr', ...),
 * and returns the int16_t at actr_tag+0x14.
 * Returns 14 (0xe) on any failure (out-of-range, -1 ref, etc.).
 *
 * Confirmed: squad_def+0x20 (int16_t) at 0x5a3bc. Default ESI=0xe at 0x5a3c3.
 * Confirmed: count at *(int*)(scenario+0x420); block ptr = scenario+0x420.
 * Confirmed: tag_block_get_element(block, index, 0x10) at 0x5a3e0.
 * Confirmed: +0xc actv-ref, +0x10 actr-ref, +0x14 return field.
 */
short FUN_0005a3b0(void *squad_def)
{
  char *p;
  int16_t squad_index;

  p = (char *)global_scenario_get();
  squad_index = *(int16_t *)((char *)squad_def + 0x20);
  if (squad_index >= 0 && (int)squad_index < *(int *)(p + 0x420)) {
    p = (char *)tag_block_get_element(p + 0x420, (int)squad_index, 0x10);
    if (*(int *)(p + 0xc) != -1) {
      p = (char *)tag_get(0x61637476, *(int *)(p + 0xc));
      if (*(int *)(p + 0x10) != -1) {
        p = (char *)tag_get(0x61637472, *(int *)(p + 0x10));
        return *(int16_t *)(p + 0x14);
      }
    }
  }
  return 0xe;
}

/* 0x5a4e0 — encounter_activate.
 * Activates an encounter if its BSP requirement is satisfied (enc_def+0x7e is
 * NONE or matches the current global_structure_bsp_index).  When the encounter
 * is not already active (encounter+0xd == 0), walks the encounter's member
 * list and calls actor_set_active(actor_handle, 1) to activate each actor. Then
 * stamps encounter+0x10 with game_time_get() and sets encounter+0xd = 1.
 *
 * param: encounter_index via @<eax> register arg.
 * returns: encounter+0xd (activation status byte).
 *
 * Confirmed: encounter_index in EAX (MOV ESI,EAX at 0x5a4e8).
 * Confirmed: datum_get(encounter_data, encounter_index) at 0x5a4f1.
 * Confirmed: tag_block_get_element(scenario+0x42c, index&0xffff, 0xb0) at
 * 0x5a514. Confirmed: enc_def+0x7e (int16_t) checked against -1 and
 * *(int16_t*)0x326a0c. Confirmed: encounter_actor_iterator_new(iter,
 * encounter_index) at 0x5a53b; iter at EBP-0xc. Confirmed: inline member-list
 * walk using actor+0x2c (next member link). Confirmed:
 * actor_set_active(actor_handle, 1) at 0x5a576 per member. Confirmed:
 * game_time_get() at 0x5a581; stored to encounter+0x10. Confirmed:
 * encounter+0xd set to 1 at 0x5a589.
 */
char FUN_0005a4e0(int encounter_index /* @<eax> */)
{
  char *encounter;
  char *enc_def;
  int iter[3];
  int next_handle;
  int actor_handle;
  char *actor;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_index);
  enc_def =
    (char *)tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                  (int)(encounter_index & 0xffff), 0xb0);

  if (*(short *)(enc_def + 0x7e) == -1 ||
      *(short *)(enc_def + 0x7e) == *(short *)0x326a0c) {
    if (*(char *)(encounter + 0xd) == '\0') {
      encounter_actor_iterator_new(iter, encounter_index);
      next_handle = iter[2];
      while (*(char *)(*(char **)0x632574 + 1) != '\0' && next_handle != -1) {
        actor_handle = next_handle;
        actor = (char *)datum_get(*(data_t **)0x6325a4, next_handle);
        next_handle = *(int *)(actor + 0x2c);
        actor_set_active(actor_handle, 1);
      }
    }
    *(int *)(encounter + 0x10) = game_time_get();
    *(char *)(encounter + 0xd) = 1;
  }

  return *(char *)(encounter + 0xd);
}

/* 0x5a640 — encounter_deactivate.
 * Deactivates an encounter (encounter_handle via @<eax>).
 * Clears encounter+0xd (active flag), walks the encounter's actor member
 * list and calls actor_set_active(handle, 0) for each actor to make them
 * dormant.
 *
 * Confirmed:
 *   - in_EAX used as encounter_handle (comparison vs -1 and datum_get arg).
 *   - datum_get(encounter_data, encounter_handle) at top.
 *   - FUN_00059bf0(encounter_handle @<eax>) called unconditionally.
 *   - actor loop: ai_globals+8 (encounterless head) if handle == -1,
 *     else encounter+0x14; advance via actor+0x2c.
 *   - actor_set_active(handle, 0) via FUN_0003d5f0 for each active actor.
 *   - actor_verify_activation(handle) via FUN_0003aca0 for each actor.
 */
void FUN_0005a640(int encounter_handle /* @<eax> */)
{
  char *encounter;
  char *ai_globals;
  char *actor;
  int actor_handle;
  int next_handle;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
  *(char *)(encounter + 0xd) = 0;
  FUN_00059bf0(encounter_handle /* @<eax> */);

  ai_globals = *(char **)0x632574;
  if (*(char *)(ai_globals + 1) != '\0') {
    if (encounter_handle == -1) {
      actor_handle = *(int *)(ai_globals + 8);
    } else {
      encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
      actor_handle = *(int *)(encounter + 0x14);
    }
    while (*(char *)(*(char **)0x632574 + 1) != '\0' && actor_handle != -1) {
      actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
      next_handle = *(int *)(actor + 0x2c);
      if (*(char *)(actor + 8) != '\0') {
        actor_set_active(actor_handle, 0);
      }
      actor_verify_activation(actor_handle);
      actor_handle = next_handle;
    }
  }
}

/* 0x5a6e0 — encounter_update_visibility.
 * Called every tick (from FUN_0005de80 / encounter_update) to refresh:
 *   1. Encounterless actor PVS visibility and activation timers.
 *   2. Per-encounter activation state based on cluster visibility.
 *
 * Phase 1 — encounterless actor loop:
 *   Iterates actors chained from ai_globals+8 (head) via actor+0x2c (next).
 *   For each actor:
 *     - Asserts actor+0x9 (encounterless flag).
 *     - If actor+0x6 == 0 (no vehicle): gets unit root parent, reads
 *       object+0x4c (cluster_index), checks PVS bit; writes actor+0x12.
 *     - Else: sets actor+0x12 = 1, then walks vehicle units or squad actor
 *       list to check if any are visible; clears actor+0x12 if one is found.
 *     - Activation decision at 0x5a870:
 *       if NOT (actor+0x12 && !g_ai_override && !actor+0xa &&
 * !game_in_editor()) → set actor+0x10 = 0x5a and activate. else if actor+0x10 >
 * 30: decrement by 30. else: zero actor+0x10 and deactivate.
 *     - Calls actor_verify_activation(local_c) each iteration.
 *     - Advances via actor+0x2c.
 *
 * Phase 2 — encounter loop:
 *   Iterates all encounters via data_iterator_new/next on encounter_data.
 *   For each encounter:
 *     - Reads combined visibility flags (encounter+0xc, game_in_editor,
 *       encounter+0x3e timer, g_ai_override).
 *     - Gets scenario encounter def; checks def+0x7e (bsp_index) vs current.
 *     - If bsp match (or -1): builds encounter cluster bit-vector via
 *       FUN_00058fd0, intersects with pvs via bit_vector_and.
 *       If any visibility → activate via FUN_0005a4e0(@<eax>).
 *       Else: goto deactivate path.
 *     - Deactivate path: if encounter+0xd (active): decrement encounter+0xe
 *       timer, or zero timer and deactivate via FUN_0005a640(@<eax>).
 *     - Activate path: check encounter+0x20 squads for pending actors;
 *       set encounter+0xe=0 and call FUN_0005a4e0 or FUN_0005a640.
 *
 * Confirmed:
 *   - CALL 0x18e3c0 (scenario_get) → [EBP-0x10].
 *   - CALL 0xba6c0 (players_get_combined_pvs) → [EBP-0xc].
 *   - [EBP-0x8] = local_c = encounterless actor list head (ai_globals+8).
 *   - datum_get(actor_data=0x6325a4, local_c) at 0x5a712.
 *   - assert "actor->meta.encounterless" at 0x5a72b/0x5a9a7 (lines
 * 0x8ba/0x720).
 *   - actor+0x6 branches at 0x5a750: JZ 0x5a82b (no vehicle = solo unit path).
 *   - actor+0x28 = -1 check at 0x5a756/0x5a760 selects vehicle vs squad path.
 *   - actor+0x12 written at 0x5a825 (0) / 0x5a84d,0x5a86d (1 or computed).
 *   - actor+0xa (sub-state byte) read at 0x5a870.
 *   - activation gate OR at 0x5a88c: JZ 0x5a8d4 (deactivate path).
 *   - actor+0x10 timer: 0x5a writes at 0x5a8cb; dec at 0x5a8e0; zero at
 * 0x5a8ec.
 *   - actor_set_active (0x3d5f0) called with (local_c, 1 or 0) at 0x5a934.
 *   - actor_verify_activation (0x3aca0) called at 0x5a940.
 *   - advance: local_c = actor+0x2c at 0x5a945.
 *   - encounter loop: data_iterator_new([EBP-0x20], 0x5ab270) at 0x5a957.
 *   - [EBP-0x18] = iter.datum_handle = encounter_handle.
 *   - cluster bv build: FUN_00058fd0(enc_hdl,1,0x200,pvs,local_64) at 0x5a9e4.
 *   - cluster bv intersect: bit_vector_and(*(uint16_t*)(scenario+0x134), pvs,
 * local_64, 0) at 0x5a9fd.
 *   - encounter+0xe = 0x96 on activate-trigger at 0x5aa09.
 *   - encounter+0x20 = squad count, encounter+0x22[i*2] = squad datum indices.
 *   - datum_get(encounter_data, squad_idx) → squad+0xe checked at 0x5aa58.
 */
void FUN_0005a6e0(void)
{
  char *scenario;
  char *pvs;
  char *ai_globals;
  char *actor;
  char *group_rec;
  char *obj;
  char *enc_def;
  data_iter_t iter;
  char cluster_bv[64];
  int encounter_handle;
  int actor_handle;
  int unit_handle;
  char *unit_obj;
  int root;
  char *actor_ptr;
  char *squad_rec;
  char *enc;
  int i;
  int16_t cluster_index;
  int16_t squad_i;
  int16_t squad_count;
  int16_t enc_timer;
  char enc_active;
  char in_editor;
  char override_flag;
  char sub_state;
  char any_squad_active;
  char vis_result;

  scenario = (char *)scenario_get();
  pvs = (char *)players_get_combined_pvs();
  ai_globals = *(char **)0x632574;
  actor_handle = *(int *)(ai_globals + 8);

  if (actor_handle == -1)
    goto LAB_encounters;

  /* Phase 1: encounterless actor visibility / activation update */
LAB_actor_loop:
  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(char *)(actor + 9) == '\0') {
    display_assert("actor->meta.encounterless",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x8ba, 1);
    system_exit(-1);
  }

  /* Compute visibility into actor+0x12 */
  if (*(char *)(actor + 6) != '\0') {
    /* Actor is in a vehicle or group: start with not-visible */
    *(char *)(actor + 0x12) = 1;
    if (*(int *)(actor + 0x28) == -1) {
      /* Walk vehicle unit list: actor+0x24 = head, unit+0x1ac = next */
      unit_handle = *(int *)(actor + 0x24);
      while (unit_handle != -1) {
        unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);
        root = object_get_root_parent(unit_handle);
        obj = (char *)object_get_and_verify_type(root, 0xffffffff);
        cluster_index = *(int16_t *)(obj + 0x4c);
        if (cluster_index != -1 &&
            (*(unsigned int *)(pvs + ((int)cluster_index >> 5) * 4) &
             (1 << ((unsigned char)cluster_index & 0x1f))) != 0)
          goto LAB_0005a825;
        unit_handle = *(int *)(unit_obj + 0x1ac);
      }
    } else {
      /* Group path: get group record from swarm data, iterate unit handles */
      group_rec =
        (char *)datum_get(*(data_t **)0x6325a0, *(int *)(actor + 0x28));
      squad_i = 0;
      if (0 < *(int16_t *)(group_rec + 2)) {
        do {
          root = object_get_root_parent(
            *(int *)(group_rec + 0x18 + (int)squad_i * 4));
          obj = (char *)object_get_and_verify_type(root, 0xffffffff);
          cluster_index = *(int16_t *)(obj + 0x4c);
          if (cluster_index != -1 &&
              (*(unsigned int *)(pvs + ((int)cluster_index >> 5) * 4) &
               (1 << ((unsigned char)cluster_index & 0x1f))) != 0)
            goto LAB_0005a825;
          squad_i++;
        } while (squad_i < *(int16_t *)(group_rec + 2));
      }
    }
    goto LAB_0005a870;
  }

  /* Solo unit path (actor+6 == 0): get root parent and check cluster vs PVS */
  root = object_get_root_parent(*(int *)(actor + 0x18));
  obj = (char *)object_get_and_verify_type(root, 0xffffffff);
  cluster_index = *(int16_t *)(obj + 0x4c);
  if (cluster_index == -1) {
    *(char *)(actor + 0x12) = 1;
  } else {
    /* NEG+SBB+INC pattern: 1 - (pvs_bit != 0) */
    *(char *)(actor + 0x12) =
      (char)(1 -
             (((1 << ((unsigned char)cluster_index & 0x1f)) &
               *(unsigned int *)(pvs + ((int)cluster_index >> 5) * 4)) != 0));
  }
  goto LAB_0005a870;

LAB_0005a825:
  *(char *)(actor + 0x12) = 0;

LAB_0005a870:
  /* Activation decision: short-circuit OR, last test inverted.
   * ref: je visible, jne override, jne sub_state, je !editor → deactivate;
   * fall=activate. SETZ; OR override; OR sub_state; then if !editor →
   * deactivate. */
  sub_state = *(char *)(actor + 0xa);
  in_editor = game_in_editor();
  if (*(char *)(actor + 0x12) == '\0')
    goto LAB_activate; /* visible → activate */
  if (*(char *)0x5ac9c9 != '\0')
    goto LAB_activate; /* override → activate */
  if (sub_state != '\0')
    goto LAB_activate; /* sub_state → activate */
  if (in_editor == '\0')
    goto LAB_0005a8d4; /* !editor → deactivate */
  /* else fall through to activate */

LAB_activate:
  /* Activate: re-fetch actor to assert encounterless, set timer, activate.
   * Disasm 0x5a891: datum_get(actor_data, actor_handle) → ESI; check ESI+9;
   * MOV [ESI+0x10], 0x5a; PUSH 1; PUSH actor_handle; JMP actor_set_active. */
  actor_ptr = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(char *)(actor_ptr + 9) == '\0') {
    display_assert("actor->meta.encounterless",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x720, 1);
    system_exit(-1);
  }
  *(int16_t *)(actor_ptr + 0x10) = 0x5a;
  actor_set_active(actor_handle, 1);
  goto LAB_advance;

LAB_0005a8d4:
  /* Deactivate or tick down timer */
  if (*(int16_t *)(actor + 0x10) > 0x1e) {
    *(int16_t *)(actor + 0x10) = (int16_t)(*(int16_t *)(actor + 0x10) - 0x1e);
    goto LAB_advance;
  }
  *(int16_t *)(actor + 0x10) = 0;
  /* Re-fetch actor to assert encounterless before deactivating */
  actor_ptr = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(char *)(actor_ptr + 9) == '\0') {
    display_assert("actor->meta.encounterless",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x72e, 1);
    system_exit(-1);
  }
  *(int16_t *)(actor_ptr + 0x10) = 0;
  actor_set_active(actor_handle, 0);

LAB_advance:
  actor_verify_activation(actor_handle);
  actor_handle = *(int *)(actor + 0x2c);
  if (actor_handle != -1)
    goto LAB_actor_loop;

  /* Phase 2: encounter loop */
LAB_encounters:
  data_iterator_new(&iter, *(data_t **)0x5ab270);
  enc = (char *)data_iterator_next(&iter);
  for (;;) {
    if (enc == NULL)
      return;

    encounter_handle = (int)iter.datum_handle;

    /* Gather visibility flags */
    enc_active = *(char *)(enc + 0xc);
    in_editor = game_in_editor();
    override_flag = *(char *)0x5ac9c9;
    enc_timer = *(int16_t *)(enc + 0x3e);

    enc_def =
      (char *)tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                    (int)(encounter_handle & 0xffff), 0xb0);

    if (*(int16_t *)(enc_def + 0x7e) == -1 ||
        *(int16_t *)(enc_def + 0x7e) == *(int16_t *)0x326a0c) {
      /* BSP matches: compute cluster visibility */
      FUN_00058fd0(encounter_handle, 1, 0x200, (int)pvs, cluster_bv);
      vis_result = bit_vector_and(*(int16_t *)(scenario + 0x134), (int)pvs,
                                  (int)cluster_bv, 0);
      if (!((enc_active == '\0' && in_editor == '\0') &&
            (enc_timer < 1 && override_flag == '\0') && vis_result == '\0')) {
        /* Trigger activation */
        *(int16_t *)(enc + 0xe) = 0x96;
        FUN_0005a4e0(encounter_handle /* @<eax> */);
        goto LAB_enc_next;
      }
    }

    /* Deactivate path */
    if (*(char *)(enc + 0xd) == '\0' || *(int16_t *)(enc + 0xe) < 0x1f) {
      /* Check squads for any active members */
      squad_count = *(int16_t *)(enc + 0x20);
      any_squad_active = '\0';
      for (i = 0; i < (int)squad_count; i++) {
        squad_rec = (char *)datum_get(*(data_t **)0x5ab270,
                                      (int)*(int16_t *)(enc + 0x22 + i * 2));
        if (*(int16_t *)(squad_rec + 0xe) > 0) {
          any_squad_active = '\x01';
        }
      }
      *(int16_t *)(enc + 0xe) = 0;
      if (any_squad_active == '\0') {
        FUN_0005a640(encounter_handle /* @<eax> */);
      } else {
        FUN_0005a4e0(encounter_handle /* @<eax> */);
      }
    } else {
      *(int16_t *)(enc + 0xe) = *(int16_t *)(enc + 0xe) - 0x1e;
    }

  LAB_enc_next:
    enc = (char *)data_iterator_next(&iter);
  }
}

/* 0x0005aab0 — encounter_clear_active_props (encounter_stand_down).
 *
 * Called when an encounter loses all visible enemies (encounter+0x45 must be
 * 0 on entry — asserted at line 0x97f).  Prepares the encounter for a fresh
 * activation cycle:
 *
 *   1. Assert !encounter->enemy_visible (encounter+0x45 == 0).
 *   2. Set encounter+0x42 = 1 (reset/active flag).
 *   3. Clear encounter+0x4c (uint16 tally).
 *   4. Call FUN_00059bf0(encounter_handle) to drain the pursuit list.
 *   5. Walk all actors in this encounter (linked list at encounter+0x14,
 *      chained via actor+0x2c).  For each actor:
 *        - Iterate its props via FUN_00064540 / FUN_00064570.
 *        - For each prop whose state (prop+0x24) is 4 or 5 AND whose
 *          enemy-visible flag (prop+0x60) is set AND whose prop_handle is
 *          not the actor's orphan_prop (actor+0x270):
 *            a. Assert prop->parent_prop_index (prop+0xc) != NONE.
 *            b. Follow the parent prop via datum_get(prop_data, prop+0xc).
 *            c. Assert parent_prop->orphan_prop_index == current prop_handle.
 *            d. Clear parent_prop->orphan_prop_index to NONE.
 *            e. Call FUN_0003b410(actor_handle, prop_handle, NONE).
 *            f. Call prop_iterator_next(actor_handle, prop_handle).
 *
 * Confirmed:
 *   EDI  = encounter_handle (param, EBP+0x8).
 *   ESI  = encounter ptr after first datum_get; later reused for
 * prop/parent_prop. EBX  = outer actor_handle loop variable, advanced to
 * next_actor_handle (actor+0x2c) at both loop-back paths. EBP-0x4 = actor ptr
 * (stored at 0x5ab63 after datum_get). EBP-0xc = prop_iter[2] (2-slot int
 * array: [0]=current handle read at 0x5aba6/0x5ac20/0x5ac2f, [1]=next handle
 * written by FUN_00064540/FUN_00064570). FUN_00059bf0: @<eax> register
 * convention (encounter_handle in EAX at 0x5aaf4). prop_data  =
 * *(data_t**)0x5ab23c. actor_data = *(data_t**)0x6325a4. assert strings confirm
 * file "c:\\halo\\SOURCE\\ai\\encounters.c" lines 0x97f/0x99a/0x99f.
 */
void encounter_stand_down(int encounter_handle)
{
  char *encounter;
  char *ai_globals;
  char *actor;
  char *prop;
  char *parent_prop;
  int actor_handle;
  int cur_actor_handle;
  int next_actor_handle;
  int prop_iter[2]; /* 2-slot iterator: [0]=current prop handle, [1]=next */

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
  if (*(char *)(encounter + 0x45) != '\0') {
    display_assert("!encounter->enemy_visible",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x97f, 1);
    system_exit(-1);
  }
  *(char *)(encounter + 0x42) = 1;
  *(short *)(encounter + 0x4c) = 0;
  FUN_00059bf0(encounter_handle /* @<eax> */);

  ai_globals = *(char **)0x632574;
  if (*(char *)(ai_globals + 1) != '\0') {
    if (encounter_handle == -1) {
      actor_handle = *(int *)(ai_globals + 8);
    } else {
      encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
      actor_handle = *(int *)(encounter + 0x14);
    }
  }

  /* Outer actor loop: walks the actor linked list.
   * actor_handle (EBX) = current actor for the outer-loop check.
   * cur_actor_handle (EDI) = snapshot of actor_handle at outer-loop entry,
   * used for all calls inside the inner loop. */
  for (;;) {
    ai_globals = *(char **)0x632574;
    if (*(char *)(ai_globals + 1) == '\0')
      break;
    if (actor_handle == -1)
      break;

    /* Snapshot current actor_handle into cur_actor_handle (= EDI in
     * binary). This is what gets passed to FUN_0003b410 and
     * prop_iterator_next. */
    cur_actor_handle = actor_handle;
    actor = (char *)datum_get(*(data_t **)0x6325a4, cur_actor_handle);
    next_actor_handle = *(int *)(actor + 0x2c);

    /* Init prop iterator and get first prop data ptr.
     * prop_iter[0] (EBP-0xc) = current prop_handle (index).
     * prop_iter[1] (EBP-0x8) = next prop_handle (chain link).
     * prop (return value of FUN_00064570) = prop data ptr. */
    FUN_00064540(prop_iter, cur_actor_handle);
    prop = (char *)FUN_00064570(prop_iter);

    /* The binary's inner while condition is a comma expression:
     *   while (actor_handle = next_actor_handle, prop != NULL)
     * The assignment runs unconditionally before the condition test,
     * advancing the outer loop even when the actor has zero props. */
    actor_handle = next_actor_handle;
    while (prop != NULL) {
      if (*(short *)(prop + 0x24) > 3 && *(short *)(prop + 0x24) < 6 &&
          *(char *)(prop + 0x60) != '\0' &&
          prop_iter[0] != *(int *)(actor + 0x270)) {
        if (*(int *)(prop + 0xc) == -1) {
          display_assert("prop->parent_prop_index != NONE",
                         "c:\\halo\\SOURCE\\ai\\encounters.c", 0x99a, 1);
          system_exit(-1);
        }
        parent_prop =
          (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(prop + 0xc));
        if (*(int *)(parent_prop + 0xc) != prop_iter[0]) {
          display_assert(
            "parent_prop->orphan_prop_index == prop_iterator.index",
            "c:\\halo\\SOURCE\\ai\\encounters.c", 0x99f, 1);
          system_exit(-1);
        }
        *(int *)(parent_prop + 0xc) = -1;
        FUN_0003b410(cur_actor_handle, prop_iter[0], -1);
        prop_iterator_next(cur_actor_handle, prop_iter[0]);
      }
      prop = (char *)FUN_00064570(prop_iter);
    }
  }
}

/* 0x5acf0 — encounter_update_timers.
 * Updates encounter timers each tick (called every 15 ticks from
 * encounter_update). Three timer groups: +0x50 (int):  incremented by 15 each
 * call if +0x45 == 0 and != -1; reset to 0 if +0x45 != 0. +0x54 (int):
 * incremented by 15 each call if +0x44 == 0 and != -1; reset to 0 if +0x44 !=
 * 0. +0x4a (short): decremented by 15 if both +0x47 and +0x48 are non-zero and
 * current value > 15; otherwise zeroed.
 *
 * Confirmed:
 *   - 1 register arg (EAX = encounter_handle); PUSH EAX, PUSH [0x5ab270]
 *     before CALL datum_get at 0x5acf8.
 *   - XOR EDX,EDX at 0x5ad00 used as zero source throughout.
 *   - CMP ECX,-0x1 at 0x5ad11/0x5ad29 gates the "disabled" (-1) case.
 *   - XOR ECX,ECX; MOV CX,[EAX+0x4a] at 0x5ad3e/0x5ad40 — zero-extends
 *     the short into ECX for the signed compare CMP CX,0xf.
 */
void FUN_0005acf0(int encounter_handle)
{
  char *encounter;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
  if (*(char *)(encounter + 0x45) != '\0') {
    *(int *)(encounter + 0x50) = 0;
  } else {
    if (*(int *)(encounter + 0x50) != -1) {
      *(int *)(encounter + 0x50) = *(int *)(encounter + 0x50) + 15;
    }
  }
  if (*(char *)(encounter + 0x44) != '\0') {
    *(int *)(encounter + 0x54) = 0;
  } else {
    if (*(int *)(encounter + 0x54) != -1) {
      *(int *)(encounter + 0x54) = *(int *)(encounter + 0x54) + 15;
    }
  }
  if (*(char *)(encounter + 0x47) != '\0' &&
      *(char *)(encounter + 0x48) != '\0') {
    if (*(short *)(encounter + 0x4a) > 15) {
      *(short *)(encounter + 0x4a) = *(short *)(encounter + 0x4a) - 15;
      return;
    }
    *(short *)(encounter + 0x4a) = 0;
  }
}

/* 0x5adc0 — encounter_squad_delay_timer_finished.
 * Called when a squad's delay timer expires (count < 0x10 ticks).
 * Resets the squad's delay counter to 0, then optionally triggers
 * ai_magically_see_players on the squad (if squad_def flag 0x10 is set),
 * and logs a debug message if the debug flag (0x5aca4b) is set.
 *
 * param_1 = encounter_handle (int, full datum handle)
 * param_2 = squad_index (int16_t, index within encounter)
 *
 * Confirmed:
 *   - 2 cdecl stack args; ADD ESP,0x8 at callers (0x1cb00, 0x5af13, 0x5af0e).
 *   - datum_get(*(data_t**)0x5ab270, encounter_handle) → ESI=encounter record.
 *   - AND EDI,0xffff at 0x5add8 → encounter_handle masked before scenario
 * lookup.
 *   - global_scenario_get() at 0x5ade6 (0 args); +0x42c+[EDI] element (size
 * 0xb0).
 *   - encounter_get_squad(encounter, param_2) → squad record; EBX=squad
 * pointer.
 *   - tag_block_get_element(EBX+0x80, (int16_t)param_2, 0xe8) → squad_def.
 *   - MOV word ptr [ECX+0x12],0 at 0x5ae1e clears squad delay counter.
 *   - Bit 0x10 of squad_def+0x28 gates FUN_00058a40 call
 * (ai_magically_see_players).
 *   - Handle for FUN_00058a40: ((squad_index & 0xff | 0xffff8000) << 16) |
 * (encounter_handle & 0xffff).
 *   - ADD ESP,0x20 at 0x5ae27 cleans up 8 dwords (first tag_block 3 +
 * encounter_get_squad 2 + second tag_block 3).
 *   - ADD ESP,0x4 at 0x5ae4c cleans FUN_00058a40 arg.
 *   - ADD ESP,0x10 at 0x5ae67 cleans console_printf args.
 *
 * Store-offset table (squad record writes):
 *   squad_record+0x12 | 0 (int16_t zero) | MOV word ptr [ECX+0x12],0x0 at
 * 0x5ae1e
 */
void encounter_squad_timer_expire(int encounter_handle, int16_t squad_index)
{
  char *encounter;
  char *squad;
  char *squad_record;
  char *squad_def;
  int handle;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
  squad = (char *)tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                        (int)(encounter_handle & 0xffff), 0xb0);
  squad_record = (char *)encounter_get_squad(encounter, squad_index);
  squad_def = (char *)tag_block_get_element((char *)(squad + 0x80),
                                            (int)squad_index, 0xe8);
  *(int16_t *)(squad_record + 0x12) = 0;
  if ((*(unsigned char *)(squad_def + 0x28) & 0x10) != 0) {
    handle =
      (int)(((unsigned int)(((int)squad_index & 0xff) | 0xffff8000U) << 16) |
            (unsigned int)(encounter_handle & 0xffff));
    FUN_00058a40(handle);
  }
  if (*(char *)0x5aca4b != '\0') {
    console_printf(0, "%s/%s: delay timer finished", squad, squad_def);
  }
}

/* 0x5ae70 — encounter_update_squad_delay_timers.
 * Iterates all squads in an encounter and manages their delay timers.
 * For each squad with a positive delay counter (squad+0x12):
 *   - If the delay is not yet started (squad+0x11 == 0): starts the timer
 *     when either bit 2 of squad_def+0x28 is set OR encounter+0x2e > 0.
 *     Logs "%s/%s: delay timer started (%.1f sec)" when debug flag is set.
 *   - If the delay is running and >= 0x10 ticks: decrements by 15.
 *   - If the delay is running and < 0x10 ticks: fires
 * encounter_squad_timer_expire to complete the squad spawn. Squads with bit 3
 * of squad_def+0x28 set are skipped entirely.
 *
 * Confirmed: PUSH [EBP+8] before CALL 0x5ae70 in FUN_0005de80 (0x5df42).
 * Confirmed: ADD ESP,0xc after global_scenario_get + tag_block_get_element
 *   (pre-positioned args pattern: 0xb0, ESI pushed before global_scenario_get).
 * Confirmed: encounter_squad_timer_expire(encounter_handle, squad_index) — 2
 * cdecl args. Confirmed: float at iVar5+0x50 promoted to double via FSTP [ESP].
 */
void FUN_0005ae70(int encounter_handle)
{
  char *encounter;
  char *scenario;
  char *squad;
  char *squad_def;
  int16_t squad_count;
  int16_t squad_index;
  int16_t delay;
  char started;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
  scenario = (char *)global_scenario_get();
  squad = (char *)tag_block_get_element(
    (char *)(scenario + 0x42c), (int)(int16_t)(encounter_handle & 0xffff),
    0xb0);
  squad_index = 0;
  squad_count = *(int16_t *)(encounter + 0x6);
  if (squad_count <= 0) {
    return;
  }
  do {
    char *sq = (char *)encounter_get_squad(encounter, squad_index);
    squad_def = (char *)tag_block_get_element((char *)(squad + 0x80),
                                              (int)squad_index, 0xe8);
    delay = *(int16_t *)(sq + 0x12);
    if (delay > 0 && (*(unsigned int *)(squad_def + 0x28) & 8) == 0) {
      if (*(char *)(sq + 0x11) == '\0') {
        if ((*(unsigned int *)(squad_def + 0x28) & 4) == 0 &&
            *(int16_t *)(encounter + 0x2e) < 1) {
          started = '\0';
        } else {
          started = '\x01';
        }
        *(char *)(sq + 0x11) = started;
        if (started != '\0' && *(char *)0x5aca4b != '\0') {
          console_printf(0, "%s/%s: delay timer started (%.1f sec)", squad,
                         squad_def, (double)*(float *)(squad_def + 0x50));
        }
      } else if (delay < 0x10) {
        encounter_squad_timer_expire(encounter_handle, squad_index);
      } else {
        *(int16_t *)(sq + 0x12) = delay - 15;
      }
    }
    squad_index = squad_index + 1;
  } while (squad_index < *(int16_t *)(encounter + 0x6));
}

/* 0x5af70 — encounter_evaluate_rule (FUN_0005af70).
 *
 * Evaluates an encounter platoon rule condition. The rule structure is a
 * short[2]: rule[0] = type (0–9), rule[1] = platoon index override.
 *
 * If rule[1] is a valid platoon index, uses that platoon's stats (strength,
 * total, survivors); otherwise uses the encounter-level stats.
 *
 * Rule types:
 *   0: always false (default)
 *   1: strength < 0.75
 *   2: strength < 0.50
 *   3: strength < 0.25
 *   4: survivors < total
 *   5: survivors*4/3 <= total
 *   6: survivors*2 <= total
 *   7: survivors*4 <= total
 *   8: survivors <= 1
 *   9: survivors == 0
 *
 * If the global debug flag at 0x5aca4c is set and the rule triggers, a
 * diagnostic message is printed via console_printf.
 *
 * Confirmed:
 *   - EAX = encounter_handle (datum index), EDI = rule pointer (short*).
 *   - datum_get(*(data_t**)0x5ab270, encounter_handle) → encounter record.
 *   - FUN_00054020(encounter, platoon_index) → platoon record.
 *   - Switch table at 0x5b1b4 (10 entries), debug switch at 0x5b1dc (9
 * entries).
 *   - Float constants: 0x25afcc=0.75f, 0x253398=0.5f, 0x25337c=0.25f.
 */
bool FUN_0005af70(int encounter_handle /* @<eax> */, void *rule /* @<edi> */)
{
  char *encounter;
  char *platoon;
  short platoon_index;
  short total;
  short survivors;
  float strength;
  bool result;
  short *rule_ptr;

  rule_ptr = (short *)rule;
  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
  platoon_index = rule_ptr[1];
  result = 0;

  if (platoon_index < 0 || platoon_index >= *(short *)(encounter + 0xa)) {
    /* Use encounter-level stats. */
    total = *(short *)(encounter + 0x18);
    survivors = *(short *)(encounter + 0x2a);
    strength = *(float *)(encounter + 0x34);
  } else {
    /* Use platoon-level stats. */
    platoon = FUN_00054020(encounter, platoon_index);
    strength = *(float *)(platoon + 0xc);
    total = *(short *)(platoon + 0x4);
    survivors = *(short *)(platoon + 0x6);
  }

  if (total <= 0) {
    goto done;
  }

  switch (rule_ptr[0]) {
  case 1:
    if (strength < 0.75f) {
      result = 1;
      break;
    }
    goto case_default;
  case 2:
    if (strength < 0.5f) {
      result = 1;
      break;
    }
    goto case_default;
  case 3:
    if (strength < 0.25f) {
      result = 1;
      break;
    }
  case_default:
  default:
    result = 0;
    break;
  case 4:
    result = (survivors < total);
    break;
  case 5:
    result = ((int)survivors * 4 / 3 <= (int)total);
    break;
  case 6:
    result = ((int)survivors * 2 <= (int)total);
    break;
  case 7:
    result = ((int)survivors * 4 <= (int)total);
    break;
  case 8:
    result = (survivors <= 1);
    break;
  case 9:
    result = (survivors == 0);
    break;
  }

done:
  if (*(char *)0x5aca4c != '\0' && result != 0) {
    switch (rule_ptr[0] - 1) {
    case 0:
      console_printf(0, "strength %.2f < 75%%", (double)strength);
      return result;
    case 1:
      console_printf(0, "strength %.2f < 50%%", (double)strength);
      return result;
    case 2:
      console_printf(0, "strength %.2f < 25%%", (double)strength);
      return result;
    case 3:
      console_printf(0, "survivors %d < total %d", (int)survivors, (int)total);
      return result;
    case 4:
      console_printf(0, "survivors %d <= 25%% of total %d", (int)survivors,
                     (int)total);
      return result;
    case 5:
      console_printf(0, "survivors %d <= 50%% of total %d", (int)survivors,
                     (int)total);
      return result;
    case 6:
      console_printf(0, "survivors %d <= 75%% of total %d", (int)survivors,
                     (int)total);
      return result;
    case 7:
      console_printf(0, "survivors %d <= 1", (int)survivors);
      return result;
    case 8:
      console_printf(0, "survivors %d = 0", (int)survivors);
      break;
    }
  }
  return result;
}

/* 0x5b200 — encounters_initialize_for_new_map.
 * Resets encounter and pursuit data pools, zeroes squad and platoon arrays,
 * then iterates scenario encounter definitions calling FUN_0005a120 to
 * initialize each encounter record. */
void encounters_initialize_for_new_map(void)
{
  char *scenario;
  short i;
  short squad_counter;
  short platoon_counter;
  void *encounter_def;

  scenario = (char *)global_scenario_get();
  squad_counter = 0;
  platoon_counter = 0;
  data_delete_all(*(data_t **)0x5ab270);
  data_delete_all(*(data_t **)0x5ab26c);
  csmemset(*(void **)0x5ab278, 0, 0x8000);
  csmemset(*(void **)0x5ab274, 0, 0x1000);
  if (*(int *)(scenario + 0x42c) > 0) {
    for (i = 0; (int)i < *(int *)(scenario + 0x42c); i++) {
      encounter_def =
        tag_block_get_element((void *)(scenario + 0x42c), (int)i, 0xb0);
      FUN_0005a120(&squad_counter /* @<eax> */, encounter_def,
                   &platoon_counter);
    }
  }
}

/* 0x0005b2a0 — encounter_increment_unit_tally (encounters_unit_died).
 *
 * For each active encounter whose team is friendly to the given unit's team,
 * increments the encounter's live-unit tally counter (encounter+0x4c) if the
 * encounter is in an eligible state.
 *
 * Algorithm:
 *   1. object_get_and_verify_type(unit_handle, 3) → unit ptr (EDI).
 *   2. Check unit+0x68 (int16_t team index) != -1; return immediately if so.
 *   3. If ai_globals+1 (ai_active) is set: init encounter data iterator.
 *   4. Outer loop: guard ai_active at each iteration.
 *   5. Inner do-while: call data_iterator_next; skip if NULL or first pass
 *      (flag==0); skip encounters where encounter+0xd (active flag) == 0.
 *   6. Copy iter.datum_handle → encounter_handle (read but unused in this fn).
 *   7. If encounter == NULL: return.
 *   8. game_allegiance_get_team_is_friendly(encounter+0x2 team, unit+0x68 team)
 *      AND encounter+0x43 != 0 (some condition met)
 *      AND encounter+0x42 == 0 (not in reset state)
 *      AND encounter+0x47 == 0 (no exclusion flag):
 *        → increment encounter+0x4c (int16_t tally).
 *
 * Confirmed from disassembly (0x5b2a0–0x5b364):
 *   - Unit ptr in EDI; ESI = encounter ptr from data_iterator_next.
 *   - [EBP-0x18] = data_iter_t (0x10 bytes); [EBP-0x4] = first-pass flag.
 *   - [EBP-0x10] = iter.datum_handle (EBP-0x18+0x8); copied to [EBP-0x8].
 *   - PUSH ECX (unit team zero-extended) / PUSH EDX (encounter team
 *     zero-extended) → game_allegiance_get_team_is_friendly(enc_team,
 *     unit_team).
 *   - INC word ptr [ESI+0x4c] at 0x5b359 = ++encounter->tally.
 *   - encounter+0x42 = reset/active flag (set to 1 by
 * encounter_clear_active_props).
 *   - encounter+0x43 = condition-met flag.
 *   - encounter+0x47 = exclusion flag.
 *   - encounter+0x4c = uint16_t live-unit tally (cleared by
 * encounter_clear_active_props).
 *
 * Caller: ai_handle_death (ai.obj) — unit-update routine.
 */
void encounters_unit_died(int unit_handle)
{
  data_iter_t iter;
  char *unit;
  char *encounter;
  char flag;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  if (*(short *)(unit + 0x68) == -1) {
    return;
  }
  if (*(char *)(*(char **)0x632574 + 1) != '\0') {
    data_iterator_new(&iter, *(data_t **)0x5ab270);
    flag = '\x01';
  }
  for (;;) {
    if (*(char *)(*(char **)0x632574 + 1) == '\0') {
      return;
    }
    do {
      encounter = (char *)data_iterator_next(&iter);
      if (encounter == NULL || flag == '\0')
        break;
    } while (*(char *)(encounter + 0xd) == '\0');
    if (encounter == NULL) {
      return;
    }
    if (game_allegiance_get_team_is_friendly(*(short *)(encounter + 0x2),
                                             *(short *)(unit + 0x68)) &&
        *(char *)(encounter + 0x43) != '\0' &&
        *(char *)(encounter + 0x42) == '\0' &&
        *(char *)(encounter + 0x47) == '\0') {
      (*(short *)(encounter + 0x4c))++;
    }
  }
}

/* 0x5c940 — encounter_update_platoon_rules.
 * For each platoon in the encounter, evaluates two rule conditions:
 *   1. Maneuvering rule (platoon_def+0x3c): if platoon[1]==0 (not yet set),
 *      calls FUN_0005af70 to evaluate; stores result in platoon[1].
 *   2. Defend/attack rule (platoon_def+0x30): if platoon[2]!=0 or
 * platoon[1]==0, computes the expected attacking flag from platoon_def+0x20 bit
 * 2 (inverted), and if it differs from platoon[0], calls FUN_0005af70 to
 * confirm, then sets platoon[0] to the new value. Logs state transitions via
 * console_printf when DAT_005aca4b is set.
 *
 * Confirmed:
 *   - cdecl, 1 stack arg (encounter_handle); no ADD ESP after RET.
 *   - datum_get(*(data_t**)0x5ab270, encounter_handle) → encounter record.
 *   - PUSH 0xb0 / PUSH ESI / CALL global_scenario_get / ADD EAX,0x42c / PUSH
 * EAX / CALL tag_block_get_element / ADD ESP,0xc → encounter def element at
 *     global_scenario_get()+0x42c[encounter_handle&0xffff] with
 * element_size=0xb0.
 *   - Outer loop on platoon_count = *(int16_t*)(encounter+0xa), via
 * FUN_00054020.
 *   - Inner tag_block_get_element: enc_def+0x8c block, loop_index,
 * element_size=0xac.
 *   - First FUN_0005af70 call: EAX=encounter_handle, EDI=platoon_def+0x3c.
 *   - Second FUN_0005af70 call: EAX=encounter_handle, EDI=platoon_def+0x30.
 *   - BL = ~(*(uint32_t*)(platoon_def+0x20) >> 2) & 1 (attacking flag).
 *   - Loop counter stored/restored via [EBP-0xc] / DI (16-bit); EBX=[EBP-0x10]
 *     (encounter) restored at bottom; ESI=platoon record from FUN_00054020.
 *
 * Call-site verification (FUN_0005de80 @ 0x5df4b):
 *   arg1 | PUSH EAX ([EBP-0x8] = encounter_handle) | encounter_handle | YES
 *
 * Store-offset table (platoon record writes):
 *   platoon[0] — attacking flag (byte), written at 0x5ca2f from BL
 *   platoon[1] — maneuvering enabled (byte), written at 0x5c9dc from AL
 * (FUN_0005af70 result)
 */
void FUN_0005c940(int encounter_handle)
{
  char *encounter;
  char *enc_def_elt;
  char *platoon;
  char *platoon_def;
  unsigned int flags;
  char new_flag;
  char result;
  const char *label;
  int i;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
  enc_def_elt = (char *)tag_block_get_element(
    (char *)global_scenario_get() + 0x42c,
    (int)(short)(encounter_handle & 0xffff), 0xb0);
  i = 0;
  if (*(short *)(encounter + 0xa) <= 0) {
    return;
  }
  do {
    platoon = (char *)FUN_00054020(encounter, (short)i);
    if (*(short *)(platoon + 6) > 0) {
      platoon_def =
        (char *)tag_block_get_element(enc_def_elt + 0x8c, (int)(short)i, 0xac);

      /* Maneuvering rule: evaluate once (platoon[1] is the latch). */
      if (platoon[1] == '\0') {
        result = (char)FUN_0005af70(encounter_handle /* @<eax> */,
                                    platoon_def + 0x3c /* @<edi> */);
        platoon[1] = result;
        if (result != '\0' && *(char *)0x5aca4b != '\0') {
          console_printf(0, "%s/%s triggered maneuvering rule", enc_def_elt,
                         platoon_def);
        }
      }

      /* Defend/attack rule: re-evaluate when platoon[2]!=0 or platoon[1]==0. */
      if (platoon[2] != '\0' || platoon[1] == '\0') {
        flags = *(unsigned int *)(platoon_def + 0x20);
        new_flag = (char)(~(flags >> 2) & 1u);
        if (platoon[0] != new_flag) {
          result = (char)FUN_0005af70(encounter_handle /* @<eax> */,
                                      platoon_def + 0x30 /* @<edi> */);
          if (result != '\0') {
            platoon[0] = new_flag;
            if (*(char *)0x5aca4b != '\0') {
              label = "defending";
              if (new_flag == '\0') {
                label = "attacking";
              }
              console_printf(0, "%s/%s triggered %s rule", enc_def_elt,
                             platoon_def, label);
            }
          }
        }
      }
    }
    i = i + 1;
  } while ((short)i < *(short *)(encounter + 0xa));
}

/* 0x5d200 — Add actor to encounter/squad/platoon membership.
 *
 * Assigns an actor to a specific encounter, squad, and platoon slot.
 * Updates all linked-list bookkeeping (encounter member list, squad count,
 * platoon count, leader count), optionally changes the actor's team, and
 * activates/deactivates the actor via encounter state flags.
 *
 * Preconditions (asserted in binary):
 *   actor->field_0x34 == -1  (actor not already in an encounter)
 *
 * Parameters:
 *   actor_handle    — datum index of the actor to add
 *   encounter_index — datum index of the target encounter
 *   squad_index     — squad slot within the encounter (int16_t)
 *   flag            — non-zero: force team assignment (suppress actor-team
 * change; if encounter->field_0x2a != 0, warns and forces anyway)
 *
 * Side effects:
 *   actor->field_0x30  = -1          (clear meta field)
 *   actor->field_0x38  = 0xffff      (clear meta short)
 *   actor->field_0x2c  = old encounter->field_0x14  (prepend to member list)
 *   encounter->field_0x14 = actor_handle
 *   actor->field_0x3c  = platoon_index (or -1 if out of range)
 *   actor->field_0x34  = encounter_index
 *   actor->field_0x3a  = squad_index
 *   encounter->field_0xe = 0x96 if actor->field_0x8 && !actor->field_0x13
 *   encounter->field_0x28 = 1  (dirty flag)
 *
 * Confirmed: 4 cdecl args, ADD ESP,0x10 at caller 0x3bb04.
 * Confirmed: assert string "actor->meta.encounter_index==NONE" at 0x5d2bd,
 *   file "c:\halo\SOURCE\ai\encounters.c", line 0x28a.
 * Confirmed: FUN_0005a4e0 reads encounter_index from EAX (@<eax> register arg).
 * Confirmed: EBX=encounter_index preserved across all inner calls.
 * Confirmed: ESI=actor record, EDI=encounter record throughout.
 */
void encounter_attach_actor(int actor_handle, int encounter_index,
                            int16_t squad_index, int flag)
{
  char *actor;
  char *encounter;
  char *enc_def;
  char *squad_def;
  char *squad;
  char *platoon;
  int platoon_index;
  short platoon_index_s;
  int unit_index;

  if (*(char *)(*(char **)0x632574 + 1) == '\0')
    return;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  tag_get(0x61637472,
          *(int *)(actor + 0x58)); /* 'actr' tag — result discarded */
  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_index);

  /* Build enc_def: scenario encounter block element */
  enc_def =
    (char *)tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                  (int)(encounter_index & 0xffff), 0xb0);

  /* Squad record from encounter runtime data */
  squad = (char *)encounter_get_squad(encounter, (int)squad_index);

  /* Squad definition element from tag block */
  squad_def = (char *)tag_block_get_element(enc_def + 0x80,
                                            (int)(int16_t)squad_index, 0xe8);

  /* platoon_index from squad def field +0x22 */
  platoon_index_s = *(short *)(squad_def + 0x22);
  platoon_index = (int)platoon_index_s;

  /* Assert actor is not already in an encounter */
  if (*(int *)(actor + 0x34) != -1) {
    display_assert("actor->meta.encounter_index==NONE",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x28a, 1);
    system_exit(-1);
  }

  /* Clear actor meta fields */
  *(int *)(actor + 0x30) = -1;
  *(short *)(actor + 0x38) = (short)-1; /* 0xffff */

  /* Prepend actor to encounter member list (rooted at encounter+0x14) */
  *(int *)(actor + 0x2c) = *(int *)(encounter + 0x14);
  *(int *)(encounter + 0x14) = actor_handle;

  /* Clamp platoon_index: validate against enc_def->field_0x8c count */
  if (platoon_index_s < 0 || platoon_index >= *(int *)(enc_def + 0x8c)) {
    platoon_index = -1;
    platoon_index_s = -1;
  }

  /* Store encounter membership fields on actor */
  *(short *)(actor + 0x3c) = (short)platoon_index; /* platoon_index */
  *(int *)(actor + 0x34) = encounter_index;
  *(short *)(actor + 0x3a) = squad_index;

  /* If actor is active (field_0x8) and not already activated (field_0x13):
   * mark encounter as pending (field_0xe = 0x96) and check activation. */
  if (*(char *)(actor + 0x8) != '\0' && *(char *)(actor + 0x13) == '\0') {
    /* Re-fetch encounter record for the field_0xe write */
    encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_index);
    *(short *)(encounter + 0xe) = 0x96;
    if ((char)FUN_0005a4e0(encounter_index /* @<eax> */) != '\0')
      goto LAB_0005d365;
  }

  actor_set_active(actor_handle, *(char *)(encounter + 0xd));
  if (*(char *)(encounter + 0xd) != '\0')
    actor_set_dormant(actor_handle, 0);

LAB_0005d365:
  /* If actor has a unit, validate linkage */
  unit_index = *(int *)(actor + 0x18);
  if (unit_index != -1)
    encounter_attach_unit(encounter_index, unit_index);

  /* Team change logic */
  if (*(short *)(actor + 0x3e) != *(short *)(encounter + 2)) {
    if (flag == 0) {
      /* Actor drives its own team assignment */
      actor_set_team(actor_handle,
                     (int)(unsigned short)*(short *)(encounter + 2));
    } else if (*(short *)(encounter + 0x2a) == 0) {
      /* Encounter has no fixed team: adopt actor's team */
      *(short *)(encounter + 2) = *(short *)(actor + 0x3e);
      ai_update_team_status();
    } else {
      /* Team conflict: warn, then force actor to encounter team */
      console_printf(
        2,
        "WARNING: actor changing to encounter %s/%s is being forced "
        "to change teams",
        enc_def, squad_def);
      actor_set_team(actor_handle,
                     (int)(unsigned short)*(short *)(encounter + 2));
    }
  }

  /* Increment actor count on encounter and squad */
  *(short *)(encounter + 0x18) = *(short *)(encounter + 0x18) + 1;
  *(short *)(squad + 0x16) = *(short *)(squad + 0x16) + 1;

  /* Increment leader count if actor is a leader */
  if (*(char *)(actor + 0x1c) != '\0')
    *(short *)(encounter + 0x1c) = *(short *)(encounter + 0x1c) + 1;

  /* Platoon membership */
  if ((short)platoon_index != -1) {
    platoon = (char *)FUN_00054020(encounter, platoon_index);
    *(char *)(actor + 0x1c9) = *platoon;
    *(char *)(actor + 0x374) = *platoon;
    *(short *)(platoon + 4) = *(short *)(platoon + 4) + 1;
  }

  /* Mark encounter dirty */
  *(char *)(encounter + 0x28) = 1;
}

/* 0x5d420 — encounter_update_status.
 *
 * Recomputes per-encounter, per-squad, and per-platoon actor-count statistics
 * for the encounter identified by encounter_handle.  Called after every actor
 * attach/detach so the encounter's cached counts stay consistent.
 *
 * Algorithm:
 *   1. Resolve encounter ptr via datum_get(encounter_data, encounter_handle).
 *   2. Reset counters: encounter+0x44 (enemy_alive), +0x45 (enemy_visible),
 *      +0x30, +0x2e, +0x2c, +0x2a (short tallies), +0x34 (float vitality sum);
 *      and per-squad (+0x18, +0x1a, +0x1c) and per-platoon (+0x6, +0x8, +0xc).
 *   3. Walk the actor linked list (encounter+0x14 chained via actor+0x2c).
 *      For each actor compute its contribution weight:
 *        - if actor+0x18 == -1 (no live unit): weight = actor+0x1e / actor+0x20
 *        - else: weight = 1, vitality = *(float*)(unit+0x90)
 *      Then accumulate per-squad, per-platoon (if actor+0x3c != -1), and
 *      encounter-level counters.  Also calls FUN_0003b120/actor_is_fighting
 * with the actor handle for dead/fleeing status. Sets encounter
 * enemy-visible/alive flags from unit state when actor+0x270 != -1.
 *   4. If no longer active (enemy gone), calls encounter_stand_down or
 * FUN_0005bbe0 depending on encounter state.
 *   5. Finalise: compute vitality ratio = sum_vitality / actor_count - 0.001f,
 *      clamped to 0.0f, for encounter and each squad/platoon.
 *   6. Clear encounter+0x28 (dirty flag).
 *
 * Confirmed:
 *   ESI = encounter ptr (callee-saved, loaded at 0x5d439).
 *   EDI = loop counter / reused during actor walk.
 *   EBX = integer weight during actor loop (sVar13).
 *   FPU: FILD/FIDIV/FSTP at 0x5d569-0x5d572 for integer-ratio weight.
 *   FDIVR at 0x5d7c4, 0x5d806, 0x5d84e for final vitality ratio.
 *   FSUB [0x255ef8] (0.001f) at 0x5d7c7, 0x5d809, 0x5d851.
 *   FCOMP [0x2533c0] (0.0f) + TEST AH,0x41 clamp pattern.
 *   assert string "!encounter->enemy_visible && !encounter->enemy_alive"
 *     at 0x5d754, file "c:\halo\SOURCE\ai\encounters.c" line 0x86c.
 */
void encounter_update_status(int encounter_handle)
{
  char *encounter;
  char *actor;
  char *squad;
  char *platoon;
  char *unit;
  int actor_handle;
  int next_actor_handle;
  int i;
  short weight;
  float vitality;
  float fVar1;
  unsigned char bVar6;
  char not_same_team; /* bVar2: set if game_team_is_ally returns false */
  char has_reinforcements; /* bVar3: set if actor+0x1e4 > 0 */
  char saw_enemy_primary; /* bVar4: actor+0x8c != 0 */
  char saw_enemy_secondary; /* bVar5: actor+0x8d != 0 */

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);

  not_same_team = 0;
  has_reinforcements = 0;
  saw_enemy_primary = 0;
  saw_enemy_secondary = 0;

  /* Reset encounter-level status fields */
  *(char *)(encounter + 0x44) = 0;
  *(char *)(encounter + 0x45) = 0;
  *(short *)(encounter + 0x30) = 0;
  *(short *)(encounter + 0x2e) = 0;
  *(short *)(encounter + 0x2c) = 0;
  *(short *)(encounter + 0x2a) = 0;
  *(int *)(encounter + 0x34) = 0;

  /* Reset per-squad counters */
  i = 0;
  if (0 < *(short *)(encounter + 6)) {
    do {
      squad = encounter_get_squad(encounter, (short)i);
      i = i + 1;
      *(short *)(squad + 0x1a) = 0;
      *(short *)(squad + 0x18) = 0;
      *(int *)(squad + 0x1c) = 0;
    } while ((short)i < *(short *)(encounter + 6));
  }

  /* Reset per-platoon counters */
  i = 0;
  if (0 < *(short *)(encounter + 10)) {
    do {
      platoon = FUN_00054020(encounter, (short)i);
      i = i + 1;
      *(short *)(platoon + 8) = 0;
      *(short *)(platoon + 6) = 0;
      *(int *)(platoon + 0xc) = 0;
    } while ((short)i < *(short *)(encounter + 10));
  }

  /* Determine starting actor handle for the linked-list walk */
  if (*(char *)(*(char **)0x632574 + 1) != '\0') {
    if (encounter_handle == -1) {
      actor_handle = *(int *)(*(char **)0x632574 + 8);
    } else {
      actor_handle =
        *(int *)((char *)datum_get(*(data_t **)0x5ab270, encounter_handle) +
                 0x14);
    }
  }

  /* Walk the actor linked list */
  while (*(char *)(*(char **)0x632574 + 1) != '\0' && actor_handle != -1) {
    actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
    next_actor_handle = *(int *)(actor + 0x2c);

    squad = encounter_get_squad(encounter, *(short *)(actor + 0x3a));

    /* Compute per-actor weight and vitality */
    if (*(int *)(actor + 0x18) != -1) {
      /* Live unit: weight = 1, vitality from unit health field */
      unit = (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);
      vitality = *(float *)(unit + 0x90);
      weight = 1;
    } else {
      /* No live unit: use spawn fraction as weight */
      weight = *(short *)(actor + 0x1e);
      vitality = (float)(int)weight / (float)(int)*(short *)(actor + 0x20);
    }

    /* Accumulate platoon counters if actor belongs to a platoon */
    if (*(short *)(actor + 0x3c) != -1) {
      platoon = FUN_00054020(encounter, *(short *)(actor + 0x3c));
      *(short *)(platoon + 6) = *(short *)(platoon + 6) + weight;
      bVar6 = *(unsigned char *)(actor + 6);
      *(float *)(platoon + 0xc) = vitality + *(float *)(platoon + 0xc);
      *(short *)(platoon + 8) =
        *(short *)(platoon + 8) +
        (short)((unsigned short)bVar6 * (unsigned short)weight);
    }

    /* Accumulate squad counters */
    *(short *)(squad + 0x18) = *(short *)(squad + 0x18) + weight;
    bVar6 = *(unsigned char *)(actor + 6);
    *(float *)(squad + 0x1c) = vitality + *(float *)(squad + 0x1c);
    *(short *)(squad + 0x1a) =
      *(short *)(squad + 0x1a) +
      (short)((unsigned short)bVar6 * (unsigned short)weight);

    /* Accumulate encounter counters */
    *(short *)(encounter + 0x2a) = *(short *)(encounter + 0x2a) + weight;
    *(short *)(encounter + 0x2c) =
      *(short *)(encounter + 0x2c) +
      (short)((unsigned short)*(unsigned char *)(actor + 6) *
              (unsigned short)weight);

    bVar6 = (unsigned char)FUN_0003b120(actor_handle);
    *(short *)(encounter + 0x2e) =
      *(short *)(encounter + 0x2e) +
      (short)((unsigned short)bVar6 * (unsigned short)weight);

    bVar6 = (unsigned char)actor_is_fighting(actor_handle);
    *(float *)(encounter + 0x34) = vitality + *(float *)(encounter + 0x34);
    *(short *)(encounter + 0x30) =
      *(short *)(encounter + 0x30) +
      (short)((unsigned short)bVar6 * (unsigned short)weight);

    /* Enemy visible / alive flags from actor's linked unit */
    if (*(int *)(actor + 0x270) != -1) {
      unit = (char *)datum_get(*(data_t **)0x5ab23c, *(int *)(actor + 0x270));
      *(char *)(encounter + 0x43) = 1;

      if (!game_team_is_ally(*(short *)(actor + 0x3e),
                             *(short *)(unit + 0x12))) {
        not_same_team = 1;
      }

      FUN_0003b120(actor_handle);

      if (*(char *)(actor + 0x8c) != '\0') {
        saw_enemy_primary = 1;
      }
      if (*(char *)(actor + 0x8d) != '\0') {
        saw_enemy_secondary = 1;
      }

      if (*(short *)(actor + 0x6e) >= 7) {
        *(char *)(encounter + 0x45) = 1;
      } else {
        if (*(short *)(unit + 0x24) < 2 || 3 < *(short *)(unit + 0x24)) {
          bVar6 = *(unsigned char *)((char *)object_get_and_verify_type(
                                       *(int *)(unit + 0x18), 3) +
                                     0xb6) &
                  4;
        } else {
          bVar6 = *(unsigned char *)(unit + 0x127);
        }
        if (bVar6 != 0) {
          goto skip_enemy_alive;
        }
      }

      *(char *)(encounter + 0x44) = 1;
    }

  skip_enemy_alive:
    actor_handle = next_actor_handle;
    if (0 < *(short *)(actor + 0x1e4)) {
      has_reinforcements = 1;
    }
  }

  /* Apply not-same-team flag */
  if (not_same_team) {
    *(char *)(encounter + 0x46) = 0;
  }

  /* State machine: determine encounter activity */
  if (*(char *)(encounter + 0x45) == '\0' &&
      (*(int *)(encounter + 0x50) == -1 || 0x3b < *(int *)(encounter + 0x50))) {
    if ((*(char *)(encounter + 0x44) == '\0' &&
         (*(int *)(encounter + 0x54) == -1 ||
          0x3b < *(int *)(encounter + 0x54))) ||
        (*(int *)(encounter + 0x50) == -1 ||
         0x1c1 < *(int *)(encounter + 0x50))) {
      if (*(char *)(encounter + 0x42) != '\0') {
        /* Encounter was previously active -- transition out */
        *(char *)(encounter + 0x47) = 0;
        *(short *)(encounter + 0x1a) = *(short *)(encounter + 0x2a);
        *(int *)(encounter + 0x58) = game_time_get();
        *(short *)(encounter + 0x4c) = 0;

        if (*(char *)(encounter + 0x43) == '\0') {
          if (*(char *)(encounter + 0x45) != '\0' ||
              *(char *)(encounter + 0x44) != '\0') {
            display_assert(
              "!encounter->enemy_visible && !encounter->enemy_alive",
              "c:\\halo\\SOURCE\\ai\\encounters.c", 0x86c, 1);
            system_exit(-1);
          }
          *(int *)(encounter + 0x50) = -1;
          *(int *)(encounter + 0x54) = -1;
        }
      } else {
        if (*(char *)(encounter + 0x47) != '\0') {
          *(char *)(encounter + 0x48) = !has_reinforcements;
          if (*(short *)(encounter + 0x4a) != 0) {
            goto done;
          }
        } else {
          if (saw_enemy_primary && saw_enemy_secondary) {
            FUN_0005bbe0(encounter_handle);
            goto done;
          }
        }
        encounter_stand_down(encounter_handle);
      }
    } else {
      /* Still active */
      *(char *)(encounter + 0x42) = 0;
      *(char *)(encounter + 0x47) = 0;
    }
  } else {
    /* Still active */
    *(char *)(encounter + 0x42) = 0;
    *(char *)(encounter + 0x47) = 0;
  }

done:
  /* Finalise encounter vitality ratio */
  if (0 < *(short *)(encounter + 0x18)) {
    fVar1 =
      *(float *)(encounter + 0x34) / (float)(int)*(short *)(encounter + 0x18) -
      *(float *)0x255ef8;
    if (fVar1 < *(float *)0x2533c0) {
      fVar1 = *(float *)0x2533c0;
    }
    *(float *)(encounter + 0x34) = fVar1;
  }

  /* Finalise per-squad vitality ratios */
  i = 0;
  if (0 < *(short *)(encounter + 6)) {
    do {
      squad = encounter_get_squad(encounter, (short)i);
      fVar1 = *(float *)(squad + 0x1c) / (float)(int)*(short *)(squad + 0x16) -
              *(float *)0x255ef8;
      if (fVar1 < *(float *)0x2533c0) {
        fVar1 = *(float *)0x2533c0;
      }
      i = i + 1;
      *(float *)(squad + 0x1c) = fVar1;
    } while ((short)i < *(short *)(encounter + 6));
  }

  /* Finalise per-platoon vitality ratios */
  i = 0;
  if (0 < *(short *)(encounter + 10)) {
    do {
      platoon = FUN_00054020(encounter, (short)i);
      fVar1 = *(float *)(platoon + 0xc) / (float)(int)*(short *)(platoon + 4) -
              *(float *)0x255ef8;
      if (fVar1 < *(float *)0x2533c0) {
        fVar1 = *(float *)0x2533c0;
      }
      i = i + 1;
      *(float *)(platoon + 0xc) = fVar1;
    } while ((short)i < *(short *)(encounter + 10));
  }

  /* Clear the dirty / recycle-pending flag */
  *(char *)(encounter + 0x28) = 0;
}

/* 0x5d890 — Iterate all encounters; for each dirty encounter whose
 * flag at +0x28 is set, calls encounter_update_status
 * (encounter_finalize/recycle).
 *
 * Uses the same guarded iterator pattern as encounters_create_for_new_map:
 *   - Guards on ai_globals+1 before init and at every loop iteration.
 *   - Inner do-while skips encounters whose dirty flag (+0xd) is clear,
 *     unless flag==0 (first call after init, which forces one iteration).
 *   - encounter_handle is read from iter.datum_handle (EBP-0x10 in
 *     disassembly, confirmed offset 0x08 of data_iter_t).
 *   - Decompiler aliasing bug: showed local_14 (EBP-0x14) for the handle;
 *     disassembly clearly reads EBP-0x10 = iter.datum_handle.
 *
 * Call-site verification (PUSH ECX at 0x5d8ff → CALL 0x5d420):
 *   arg1 | EBP-0x10 = iter.datum_handle | encounter_handle | match YES
 *
 * Store-offset table (none: no struct init, only reads):
 *   encounter+0x0d — dirty flag, read in inner loop continue condition
 *   encounter+0x28 — recycle-pending flag, gates encounter_update_status call
 */
void encounters_update_dirty_status(void)
{
  data_iter_t iter;
  int encounter_handle;
  char *encounter;
  char flag;

  if (*(char *)(*(int *)0x632574 + 1) != '\0') {
    data_iterator_new(&iter, *(data_t **)0x5ab270);
    flag = '\0';
  }
  for (;;) {
    if (*(char *)(*(int *)0x632574 + 1) == '\0')
      return;
    do {
      encounter = (char *)data_iterator_next(&iter);
      if (encounter == NULL || flag == '\0')
        break;
    } while (*(char *)(encounter + 0xd) == '\0');
    encounter_handle = (int)iter.datum_handle;
    if (encounter == NULL)
      return;
    if (*(char *)(encounter + 0x28) != '\0') {
      encounter_update_status(encounter_handle);
    }
  }
}

/* 0x0005d910 — Place actors for an encounter or specific squad/platoon.
 * param_2: platoon index (-1 = all), param_3: squad index (-1 = all).
 * Resolves difficulty-based spawn counts, applies spawn-type delays,
 * calls encounter_get_actor_starting_location per actor slot, then finalises
 * via encounter_update_status and FUN_0005a6e0. */
void encounter_create(int encounter_handle, short param_2, short param_3)
{
  char *scenario;
  char *encounter_def;
  char *squad_def;
  char *name;
  char *encounter;
  char do_all;
  int i;
  int count;
  int delay;
  int should_spawn;
  int leader_count;
  int squad_state;
  int spawn_type;
  int j;
  int16_t difficulty;

  if (*(char *)(*(int *)0x632574 + 1) == 0) {
    return;
  }

  scenario = (char *)global_scenario_get();
  encounter_def = (char *)tag_block_get_element(
    (void *)(scenario + 0x42c), encounter_handle & 0xffff, 0xb0);
  do_all = (param_2 == -1 && param_3 == -1);

  if (*(char *)0x5aca52) {
    if (param_2 == -1 && param_3 == -1) {
      console_printf(0, "ai_place %s", encounter_def);
    } else if (param_2 != -1) {
      name = (char *)tag_block_get_element((void *)(encounter_def + 0x8c),
                                           param_2, 0xac);
      console_printf(0, "ai_place %s/%s", encounter_def, name);
    } else {
      name = (char *)tag_block_get_element((void *)(encounter_def + 0x80),
                                           param_3, 0xe8);
      console_printf(0, "ai_place %s/%s", encounter_def, name);
    }
  }

  for (i = 0; (int)(int16_t)i < *(int *)(encounter_def + 0x80); i++) {
    squad_def = (char *)tag_block_get_element((void *)(encounter_def + 0x80),
                                              (int16_t)i, 0xe8);

    if (!do_all && (int16_t)i != (int16_t)param_3 &&
        (*(int16_t *)(squad_def + 0x22) == -1 ||
         *(int16_t *)(squad_def + 0x22) != (int16_t)param_2)) {
      continue;
    }

    delay = 0;

    difficulty = game_difficulty_level_get();
    switch (difficulty) {
    case 0:
    case 1:
      count = *(uint16_t *)(squad_def + 0x7c);
      break;
    case 2:
      count = ((int16_t)(*(int16_t *)(squad_def + 0x7e) +
                         *(int16_t *)(squad_def + 0x7c))) /
              2;
      break;
    case 3:
      count = *(uint16_t *)(squad_def + 0x7e);
      break;
    default:
      assert_halt(0);
      break;
    }

    squad_state = (int)FUN_0005a3b0((void *)squad_def);
    spawn_type = *(int16_t *)(squad_def + 0x2c);

    switch (spawn_type) {
    case 0:
      encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
      should_spawn = 0;
      leader_count = 0;
      if (squad_state == 7) {
        leader_count = (int)*(int16_t *)(encounter + 0x1c);
        if (leader_count == 0) {
          should_spawn = (*(int16_t *)(encounter + 0x18) + (int16_t)count >= 4);
        } else if (leader_count == 1) {
          should_spawn =
            (*(int16_t *)(encounter + 0x18) + (int16_t)count >= 10);
        }
      }
      if (*(char *)0x5aca52) {
        console_printf(
          0, "%s/%s: %d current %d leaders, create %d -> %s", encounter_def,
          squad_def, (int)*(int16_t *)(encounter + 0x18), leader_count,
          (int16_t)count, should_spawn ? "new leader" : "no leader");
      }
      if (!should_spawn) {
        break;
      }
      /* fall through to case 2 */
    case 2:
      if (squad_state == 7) {
        delay = (int)random_range(
                  (unsigned int *)get_global_random_seed_address(), 0, 2) +
                100;
      }
      break;
    case 3:
      if (squad_state == 7) {
        delay = 100;
      }
      break;
    case 4:
      if (squad_state == 7) {
        delay = 101;
      }
      break;
    default:
      break;
    }

    if ((int16_t)count > 0) {
      for (j = 0; j < (int16_t)count; j++) {
        encounter_get_actor_starting_location((int16_t)i, delay, 0,
                                              encounter_handle);
        delay = 0;
      }
    }
  }

  encounter_update_status(encounter_handle);
  FUN_0005a6e0();
}

/* 0x0005dc00 — Sync actor state from encounter and platoon definitions.
 *
 * For a given encounter handle, walks the linked list of actors belonging to
 * that encounter (rooted at encounter+0x14, chained via actor+0x2c).
 * For each actor the function:
 *   - Copies encounter-level flag bytes into the actor record
 *     (encounter+0x42 -> actor+0x1c8, encounter+0x60 -> actor+0x1ca).
 *   - Conditionally resets the actor's aggression/combat state fields
 *     (actor+0x1e4 = 0, actor+0x1e8 = -1) when encounter+0x47 == 0.
 *   - If the actor has a valid platoon index (actor+0x3c != -1), looks up the
 *     platoon definition and copies platoon+0x00 into actor+0x1c9.
 *   - If the platoon lookup result indicates a valid squad assignment
 *     (result[1] != 0 and result[2] == 0), reads the target squad index from
 *     squad_def+0x4e, bounds-checks it against the encounter's squad count,
 *     and calls FUN_0003baa0 to move the actor to that squad, then calls
 *     FUN_00036dc0 to update the actor's firing state from platoon flags.
 * After the actor loop, calls encounters_update_dirty_status for encounter
 * cleanup.
 *
 * Confirmed:
 *   - cdecl, 1 stack arg (encounter_handle), RET (no stack fixup).
 *   - actor linked list: encounter+0x14 = head; actor+0x2c = next handle.
 *   - Loop guard: *(char*)(ai_globals+1) != 0 && handle != -1.
 *   - Batch ADD ESP,0x18 cleanup for FUN_0003baa0 + FUN_00036dc0 (3+3 args).
 *   - EDI = encounter record ptr, restored from [EBP-0x8] on loop-back
 * (0x5dc72).
 *   - [EBP-0x10] saves actor_handle for use as arg1 in
 * FUN_0003baa0/FUN_00036dc0.
 *
 * Call-site: FUN_0005de80 @ 0x5df5e: PUSH EDX ([EBP-0x8] = encounter_handle).
 */
void FUN_0005dc00(int encounter_handle)
{
  char *encounter;
  char *scenario;
  char *encounter_def;
  char *actor;
  char *platoon_def;
  char *squad_def;
  char *platoon_entry;
  int actor_handle;
  int saved_actor_handle;
  int next_handle;
  int squad_count;
  int16_t squad_target_idx;
  char uVar9;
  char bVar2;
  unsigned int flags_dword;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);

  scenario = (char *)global_scenario_get();
  encounter_def = (char *)tag_block_get_element(
    (void *)(scenario + 0x42c), encounter_handle & 0xffff, 0xb0);

  actor_handle = -1;
  if (*(char *)(*(int *)0x632574 + 1) != '\0') {
    if (encounter_handle == -1) {
      actor_handle = *(int *)(*(int *)0x632574 + 8);
    } else {
      actor_handle = *(int *)(encounter + 0x14);
    }
  }

  for (;;) {
    if (*(char *)(*(int *)0x632574 + 1) == '\0')
      break;
    if (actor_handle == -1)
      break;

    saved_actor_handle = actor_handle;
    actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);

    next_handle = *(int *)(actor + 0x2c);
    *(char *)(actor + 0x1c8) = *(char *)(encounter + 0x42);
    *(char *)(actor + 0x1ca) = *(char *)(encounter + 0x60);

    if (*(char *)(encounter + 0x47) == '\0') {
      *(int16_t *)(actor + 0x1e4) = 0;
      *(int *)(actor + 0x1e8) = -1;
    }

    uVar9 = '\0';
    bVar2 = 0;
    if (*(int16_t *)(actor + 0x3c) != -1) {
      platoon_entry =
        (char *)FUN_00054020(encounter, (int)*(int16_t *)(actor + 0x3c));
      uVar9 = platoon_entry[0];
      if (platoon_entry[1] != '\0' && platoon_entry[2] == '\0') {
        bVar2 = 1;
      }
    }
    *(char *)(actor + 0x1c9) = uVar9;

    if (bVar2) {
      squad_def = (char *)tag_block_get_element(
        (void *)(encounter_def + 0x80), (int)*(int16_t *)(actor + 0x3a), 0xe8);
      platoon_def = (char *)tag_block_get_element(
        (void *)(encounter_def + 0x8c), (int)*(int16_t *)(actor + 0x3c), 0xac);
      squad_target_idx = *(int16_t *)(squad_def + 0x4e);
      squad_count = *(int *)(encounter_def + 0x80);
      if ((int)squad_target_idx >= 0 && (int)squad_target_idx < squad_count) {
        FUN_0003baa0(saved_actor_handle, encounter_handle, squad_target_idx);
        flags_dword = *(unsigned int *)(platoon_def + 0x20);
        FUN_00036dc0(saved_actor_handle,
                     (char)((int)(flags_dword >> 1) & (int)0xffffff01u),
                     (char)(*(unsigned char *)(platoon_def + 0x20) & 1u));
      }
    }

    actor_handle = next_handle;
  }

  encounters_update_dirty_status();
}

/* 0x5ddc0 — Iterate all encounters and reset tallies for those matching
 * the current BSP or with the "not-automatically-recycled" flag cleared.
 * Uses a data iterator over encounter_data; for each encounter whose
 * dirty flag (+0xd) is set, fetches the encounter's tag definition and
 * calls encounter_create to reset vote tallies. */
void encounters_create_for_new_map(void)
{
  char *scenario;
  data_iter_t iter;
  int encounter_handle;
  char *encounter;
  char *encounter_def;
  char flag;

  scenario = (char *)global_scenario_get();
  if (*(char *)(*(int *)0x632574 + 1) != '\0') {
    data_iterator_new(&iter, *(data_t **)0x5ab270);
    flag = '\0';
  }
  for (;;) {
    if (*(char *)(*(int *)0x632574 + 1) == '\0')
      return;
    do {
      encounter = (char *)data_iterator_next(&iter);
      if (encounter == NULL || flag == '\0')
        break;
    } while (*(char *)(encounter + 0xd) == '\0');
    encounter_handle = (int)iter.datum_handle;
    if (encounter == NULL)
      return;
    encounter_def = (char *)tag_block_get_element(
      scenario + 0x42c, encounter_handle & 0xffff, 0xb0);
    if (((*(int *)0x5ac9f4 ^ encounter_handle) & 0xffff) == 0 ||
        (~*(unsigned char *)(encounter_def + 0x20) & 1) != 0) {
      encounter_create(encounter_handle, -1, -1);
    }
  }
}

/* 0x5de80 — Per-tick encounter update. Every 30 ticks calls
 * encounters_update_dirty_status and FUN_0005a6e0. Then iterates all
 * encounters; for each dirty encounter whose handle index mod 15 matches the
 * current tick mod 15, runs the full suite of encounter update functions
 * (tally, perception, squad management, etc.). */
void FUN_0005de80(void)
{
  int tick;
  int tick_mod15;
  data_iter_t iter;
  int encounter_handle;
  char *encounter;
  char flag;

  tick = game_time_get();
  if (tick % 30 == 0) {
    encounters_update_dirty_status();
    FUN_0005a6e0();
  }
  tick_mod15 = tick % 15;
  if (*(char *)(*(int *)0x632574 + 1) != '\0') {
    data_iterator_new(&iter, *(data_t **)0x5ab270);
    flag = 1;
  }
  for (;;) {
    if (*(char *)(*(int *)0x632574 + 1) == '\0')
      return;
    do {
      encounter = (char *)data_iterator_next(&iter);
      if (encounter == NULL || flag == '\0')
        break;
    } while (*(char *)(encounter + 0xd) == '\0');
    encounter_handle = (int)iter.datum_handle;
    if (encounter == NULL)
      return;
    (*(short *)0x5abb34)++;
    if ((short)((encounter_handle & 0xffff) % 15) == (short)tick_mod15) {
      encounter_update_status(encounter_handle);
      FUN_0005acf0(encounter_handle);
      FUN_0005c680(encounter_handle);
      FUN_0005ae70(encounter_handle);
      FUN_0005c940(encounter_handle);
      FUN_0005ca80(encounter_handle);
      FUN_0005dc00(encounter_handle);
    }
  }
}

/* 0x0005df80 — encounter_initialize stub.
 * Binary: single RET. No initialization needed at this level. */
void FUN_0005df80(void)
{
}

/* 0x0005df90 — encounter_dispose stub.
 * Binary: single RET. No teardown needed at this level. */
void FUN_0005df90(void)
{
}

/* 0x0005dfa0 — encounter_initialize_for_new_map stub.
 * Binary: single RET. Map-level init is handled elsewhere. */
void FUN_0005dfa0(void)
{
}

/* 0x0005dfb0 — encounter_dispose_from_old_map stub.
 * Binary: single RET. Map-level dispose is handled elsewhere. */
void FUN_0005dfb0(void)
{
}

/* Deferred functions (not yet ported — thunked from XBE):
 *   FUN_0005de80  — encounter_update (needs FUN_0005acf0 @<eax> audit)
 *   encounters_create_for_new_map  — encounter_tally_reset_pass (shared loop
 * pattern)
 */
