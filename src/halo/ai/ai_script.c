/* ai_script.c — HS ("hsc") script-callable AI commands.
 *
 * Covers ai_script.obj.  __FILE__ = c:\halo\SOURCE\ai\ai_script.c, proven by
 * the display_assert strings inside this unit's iterator helpers
 * (ai_index_actor_iterator_new asserts "c:\halo\SOURCE\ai\ai_script.c":0x180,
 * ai_index_actor_iterator_next the same file at :0x1ba).
 *
 * Globals (raw pointer-cast idiom, matching the sibling AI translation units):
 *   0x5aca59 (char) : ai script trace/spew flag — when set, every scripted AI
 *                     command echoes "<thread>: <command> <ai-index>" through
 *                     error(2, ...).
 */
#include "../../common.h"

/* 0x00058d40 — "ai_renew" HS script command.
 *
 * Refreshes every actor named by a packed ai_index_reference:
 *   1. binarizes the unit's two desired-movement scalars (+0x88/+0x8c) into
 *      0.0f / 1.0f at +0x90/+0x94, and
 *   2. tops the unit's grenades back up to a random count drawn from the
 *      actor_variant tag's [min,max] range, if the variant carries a grenade
 *      type at all.
 *
 * Name is Confirmed from the format string at 0x25d1fc ("%s: ai_renew %s").
 *
 * Signature: the HS thunk at 0xc0970 does MOV EDX,dword ptr [EAX]; PUSH EDX
 * and cleans with cdecl, so this takes exactly ONE stack dword.  Ghidra models
 * it `void __cdecl FUN_00058d40(void)` with a stray `uint in_stack_00000004`
 * — that IS the parameter.  kb.json's `void FUN_00058d40(int handle)` is
 * authoritative and must not be widened.
 *
 * Frame (0x58d40): PUSH EBP; MOV EBP,ESP; SUB ESP,0x118.  No _chkstk.
 * Exactly two locals:
 *   [EBP-0x118] char[256] ai-index name scratch (size Confirmed by PUSH 0x100)
 *   [EBP-0x18]  char[24]  ai_index actor iterator (Confirmed: the iterator is
 *               6 dwords — iter[0..2] filters plus a 3-dword embedded
 *               encounter_actor_iterator at iter+3; same 24-byte local as the
 *               encounters.obj siblings 0x568e0 / 0x56980)
 *
 * Struct offsets touched (all raw, unproven beyond the accesses themselves):
 *   actor record  +0x18 int     unit object handle (-1 = none)
 *                 +0x5c int     actor_variant tag index
 *   actor_variant +0x180 int16  grenade type (-1 = variant throws none)
 *                 +0x1d0 int16  grenade count minimum
 *                 +0x1d2 int16  grenade count maximum (range is [min,max+1))
 *   unit object   +0x88/+0x8c float inputs, +0x90/+0x94 float outputs
 *
 * FCOM sense (verified against disassembly, not the decompiler):
 *   FLD [ECX+0x88]; FCOMP [0.0f]; FNSTSW AX; TEST AH,0x41; JNE -> FLD 0.0f
 *   AH & 0x41 is C3|C0, so the jump is taken when ST(0) < 0.0f, == 0.0f, or
 *   unordered.  Only a strictly positive input stores 1.0f — hence
 *   `(x > 0.0f) ? 1.0f : 0.0f`, NOT a clamp and NOT `x < 0.0f`.
 *   Operand ORDER is load-bearing, not cosmetic: the reference puts x in ST(0)
 *   (FLD x; FCOMP 0.0).  Writing the mathematically identical `0.0f < x`
 *   makes VC71 emit FLD 0.0; FCOMP x; TEST AH,0x5; JP instead, which trips
 *   [FCOM-WARN] and cost 3.4pp (measured: 94.0% -> 97.4% VC71).
 *
 * unit_set_grenade_count returns int16_t; the result is discarded here in the
 * original (no store, no test after CALL 0x1aaa90).  That is intentional.
 *
 * 0x58d40 / ai_script.obj
 */
void ai_renew(int handle)
{
  char local_11c[256];
  char local_1c[24];
  void *variant;
  void *unit;
  int actor;
  short wanted;
  short count;

  if (*(char *)0x5aca59) {
    ai_index_to_string((unsigned int)handle, global_scenario_get(), local_11c,
                       0x100);
    error(2, "%s: ai_renew %s", hs_runtime_get_executing_thread_name(),
          local_11c);
  }

  ai_index_actor_iterator_new((unsigned int)handle, local_1c);
  actor = ai_index_actor_iterator_next(local_1c);
  while (actor != 0) {
    if (*(int *)(actor + 0x18) != -1) {
      variant = tag_get(0x61637476, *(int *)(actor + 0x5c)); /* 'actv' */
      unit = object_get_and_verify_type(*(int *)(actor + 0x18), 3);

      *(float *)((char *)unit + 0x90) =
        (*(float *)((char *)unit + 0x88) > 0.0f) ? 1.0f : 0.0f;
      *(float *)((char *)unit + 0x94) =
        (*(float *)((char *)unit + 0x8c) > 0.0f) ? 1.0f : 0.0f;

      if (*(short *)((char *)variant + 0x180) != -1) {
        wanted = random_range((unsigned int *)get_global_random_seed_address(),
                              *(short *)((char *)variant + 0x1d0),
                              *(short *)((char *)variant + 0x1d2) + 1);
        count = unit_get_grenade_count(
          *(int *)(actor + 0x18),
          unit_get_current_grenade_type(*(int *)(actor + 0x18)));
        if (count < wanted) {
          unit_set_grenade_count(*(int *)(actor + 0x18),
                                 *(short *)((char *)variant + 0x180),
                                 wanted - count);
        }
      }
    }
    actor = ai_index_actor_iterator_next(local_1c);
  }
}
