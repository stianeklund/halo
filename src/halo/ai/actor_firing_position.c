/* actor_firing_position.c — AI firing-position group selection.
 *
 * Corresponds to actor_firing_position.obj. Source path confirmed via
 * __FILE__ assert xref: c:\halo\SOURCE\ai\actor_firing_position.c
 * (the assert at 0x24b3d stamps that literal).
 *
 * Ported:
 *   FUN_00024370 (0x24370) — score one candidate firing position: resolve the
 *     actor's 'actr' tag, then either accumulate a penalty (no candidate) or
 *     test 3D path availability to the candidate and either credit it via
 *     FUN_00024000 or mark it rejected.
 *   actor_get_firing_position_group (0x24a60) — map an actor plus a
 *     group-selector and a searching-state override onto one of the squad
 *     definition's firing-position group indices, returned as the int stored
 *     at squad + 0x54 + group*4.
 *
 * Layout used here (all offsets read directly from the listing):
 *   actor_t  +0x034  uint32  encounter index, -1 == NONE (CMP dword,-1)
 *            +0x03a  int16   squad index      (MOVSX word)
 *            +0x098  char    "searching" state flag (TEST CL,CL)
 *            +0x374  char    "currently defending" flag
 *   scenario +0x42c  tag_block encounters, element stride 0xb0
 *   encounter_definition +0x80 squads tag_block, element stride 0xe8
 *   squad    +0x54 .. +0x6c  array of 7 firing-position group indices (int)
 *
 * The binary returns early for selector 1 and selector 5, loading
 * [squad+0x68] and [squad+0x6c] directly. Those are 0x54 + 5*4 and
 * 0x54 + 6*4 — i.e. MSVC tail-duplicated the common indexed load with the
 * constant folded in, not two distinct source statements. The single-tail
 * shape below is the source form.
 */

#include "../../common.h"
#include "encounters.h"

/* Bound from the assert's own range check: CMP SI,7 / JL. The identifier is
 * verbatim from the assert predicate string in the XBE. */
#define NUMBER_OF_FIRING_POSITION_GROUPS 7

/* FUN_00024370 (0x24370) — score one candidate firing position for an actor.
 *
 * Confirmed from the listing at 0x24370:
 *   MOV EAX,[0x6325a4] (actor_data); PUSH EBX (actor_handle) -> datum_get.
 *   EDI = the actor datum. PUSH [EDI+0x58]; PUSH 0x61637472 ('actr');
 *   CALL tag_get — the result is discarded, the call is made for its side
 *   effect. The single ADD ESP,0x10 at 0x2439e pays for BOTH calls' pushes,
 *   so it is not evidence of a 4-argument tag_get.
 *
 *   The addend at 0x243b4 is FADD DWORD PTR [0x254cc0]; that address holds
 *   0x41700000 == 15.0f — VC71's literal pool for the same 15.0f pushed to
 *   FUN_00024000 at 0x2440d. It is a source literal, not a game global.
 *
 *   `dist` occupies the (dead) third parameter's home slot in the original:
 *   MOV DWORD PTR [EBP+0x10],0 / LEA EDX,[EBP+0x10] while ESI still holds
 *   firing_position. It is a distinct local — Ghidra's
 *   `fVar = (float)in_stack_0000000c` is the param-slot-reuse artifact.
 *
 *   path_3d_available's third parameter is declared int in kb.json but here
 *   carries a float bit pattern (MOV EDX,[EBP+0x10]; PUSH EDX reads back the
 *   float actor_path_3d_available wrote). Reinterpret the bits; converting
 *   would truncate.
 *
 *   Both `*(float **)firing_position` loads are re-read from memory in the
 *   original (MOV EAX,[ESI] at 0x243c9, MOV ECX,[ESI] at 0x243e4), so the
 *   expression appears twice here rather than being cached.
 *
 *   Return is 32 bits wide (MOV EAX,1 twice, MOVZX EAX,BYTE PTR [ESI+0x30]
 *   once), so the return type is int-width, not char.
 *
 * Offsets used (raw; struct identities not yet proven):
 *   actor           +0x058 int      tag index handed to tag_get('actr')
 *                   +0x12c float[3] actor position (path start point)
 *   eval_state      +0x014 char     "keep rejected positions" gate
 *                   +0x044 char     master gate
 *                   +0x660 float    rejected-position accumulator
 *   firing_position +0x000 float *  candidate position
 *                   +0x030 char     usable flag (also the return value)
 *                   +0x031 char     rejected flag
 */
int FUN_00024370(int actor_handle, char *eval_state, char *firing_position)
{
  char *actor;
  float dist;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag_get(0x61637472 /* 'actr' */, *(int *)(actor + 0x58));

  if (*(char *)(eval_state + 0x44) != '\0') {
    if (firing_position == (char *)0) {
      *(float *)(eval_state + 0x660) += 15.0f;
      return 1;
    }

    dist = 0.0f;
    if (actor_path_3d_available(actor_handle, *(float **)firing_position,
                                &dist) != '\0' &&
        path_3d_available((int)scenario_get(), (int *)(actor + 0x12c),
                          *(int *)&dist, *(int **)firing_position,
                          (unsigned char *)0, (float *)0) != '\0') {
      FUN_00024000(eval_state, 15.0f, 0x19, firing_position);
    } else {
      *(char *)(firing_position + 0x31) = 1;
      if (*(char *)(eval_state + 0x14) == '\0')
        *(char *)(firing_position + 0x30) = '\0';
    }
  }

  if (firing_position == (char *)0)
    return 1;
  return *(unsigned char *)(firing_position + 0x30);
}

int actor_get_firing_position_group(int actor_handle, short param_2,
                                    int param_3)
{
  actor_t *actor;
  encounter_definition *encounter;
  char *squad;
  char searching;
  short group_index;

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  if (actor->field_034 == 0xffffffff)
    return 0;

  encounter = (encounter_definition *)tag_block_get_element(
    (char *)global_scenario_get() + 0x42c, actor->field_034 & 0xffff, 0xb0);
  squad =
    (char *)tag_block_get_element(&encounter->squads, actor->field_03a, 0xe8);

  /* The override is compared 16 bits wide in the original (CMP with a word
   * operand); do not widen it to a full int compare. */
  searching = actor->field_098;
  if ((short)param_3 == 1)
    searching = 1;
  else if ((short)param_3 == 2)
    searching = 0;

  if (param_2 == 1) {
    group_index = 5;
  } else if (param_2 == 4) {
    /* Branchless in the original: NEG AL / SBB EAX,EAX / AND EAX,3 / ADD EAX,2.
     * Written either as `5 : 2` or as `2 + (cond ? 3 : 0)`, VC71 emits the
     * identical sequence (both measured at 88.9%), so the simpler form stands.
     */
    group_index = (short)(actor->field_374 != 0 ? 5 : 2);
  } else if (param_2 == 5) {
    group_index = 6;
  } else if (actor->field_374 != 0) {
    group_index = (short)(3 + (searching != 0));
  } else {
    group_index = (short)(searching != 0);
  }

  /* Signed range check: TEST SI,SI / JL then CMP SI,7 / JL. */
  if (group_index < 0 || group_index >= NUMBER_OF_FIRING_POSITION_GROUPS) {
    display_assert("(index >= 0) && (index < NUMBER_OF_FIRING_POSITION_GROUPS)",
                   "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x584, 1);
    system_exit(-1);
  }

  return *(int *)(squad + 0x54 + group_index * 4);
}

/* actor_clear_discarded_firing_positions (0x24b80) — reset the actor's memory
 * of firing positions it has already discarded.
 *
 * Confirmed from the listing at 0x24b80 (95 bytes, bare PUSH EBP/MOV EBP,ESP
 * frame, no locals, no FPU, one CALL):
 *   MOV EAX,[EBP+0x8]; MOV ECX,[0x6325a4]; PUSH EAX; PUSH ECX; CALL datum_get;
 *   ADD ESP,0x8  ->  datum_get(actor_data, actor_handle).
 *   MOV word ptr [EAX+0x3c6],0 — a 16-bit store, not a byte or a dword.
 *   LEA ECX,[EAX+0x3ca]; MOV EDX,4; {MOV word ptr [ECX],0xffff; ADD ECX,0x4;
 *   DEC EDX; JNZ} — four int16 NONE stores on a 4-byte stride, touching only
 *   the low half of each 4-byte slot. See the actor_t comment at +0x3ca: the
 *   record boundary is unproven, so the table is walked by explicit byte
 *   stride rather than indexed through a made-up element type.
 *
 *   The tail is a short-circuit chain of byte compares, in this exact order:
 *   CL=[EAX+0x3d8] (zero -> return); CL=[EBP+0xc] (zero -> take the store);
 *   CL=[EAX+0x3d9] (zero -> return, else store). Reordering the && / || here
 *   changes the branch layout, so the shape below is deliberate.
 *
 *   param2 is declared int in kb.json but only its low byte is ever read
 *   (MOV CL,[EBP+0xc]); both in-repo call sites pass 0 or 1. The (char) cast
 *   reproduces the byte compare without touching the immutable declaration.
 */
void actor_clear_discarded_firing_positions(int actor_handle, int param2)
{
  actor_t *actor;
  int16_t *index;
  int count;

  actor = (actor_t *)datum_get(actor_data, actor_handle);

  actor->field_3c6 = 0;

  index = &actor->field_3ca[0];
  count = 4;
  do {
    *index = (int16_t)NONE;
    index = (int16_t *)((char *)index + 4);
    count--;
  } while (count != 0);

  if (actor->field_3d8 != '\0' &&
      ((char)param2 == '\0' || actor->field_3d9 != '\0')) {
    actor->field_3d8 = 0;
  }
}
