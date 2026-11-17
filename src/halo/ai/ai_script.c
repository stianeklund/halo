#include "encounters.h"
#include "../../common.h"

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
/* 0x00057330 — command-list progress status for one actor's current point.
 *
 * Not an independently-callable routine in the original: the body appears
 * TWICE in the XBE — inlined at 0x57432-0x57491 inside
 * ai_scripting_command_list_status's non-swarm arm (reading the point pair
 * directly out of the actor record at +0xa4/+0xa8), and out-of-line at
 * 0x57330 for the swarm arm.  The out-of-line copy uses MSVC's custom
 * calling convention for a file-static function: param 1 arrives in AX
 * (MOVSX EAX,AX at the entry) and param 2 in ESI, while the remaining three
 * parameters are still pushed by the caller (PUSH 0; PUSH ECX; PUSH EDX;
 * ADD ESP,0xc at 0x5754e-0x57563) and never read — the body has zero
 * ESP-relative loads.  They are live in the source signature but dead in the
 * generated code, so they are kept here as named-but-unused parameters.
 *
 * KNOWN VC71 GAP (the entire remaining 11 instructions of 0x57380's 188):
 * VC71 reproduces the AX/ESI convention and the inline/out-of-line split
 * exactly, but it ALSO dead-argument-eliminates the three unused stack
 * parameters, so our call site emits neither the three PUSHes nor the
 * ADD ESP,0xc.  Losing those args also drops `object` (EBX) and
 * `object_index` (ESI) out of the reference's -0x10 / -0xc spill slots
 * (SUB ESP,0x10 instead of 0x18) and removes the EBX reload after the swarm
 * search loop.  Measured non-fixes: `volatile` on the three parameters, on
 * `object`, on `object_index`, on `status`, on `overall`; defining the helper
 * after its caller behind a prototype; reordering the parameters.  Only
 * `--opt "/O2 /Oy"` closes part of it (84.7% for this function) and that is
 * rejected: it costs the two sibling functions 4.0pp and 8.5pp, so the TU was
 * not built with /Oy.
 *
 * Return value (Confirmed from the two exits):
 *   1                                  — index past the end of the point
 *                                        block, or the element lookup failed
 *   ((~flags & 0x10) | 0x20) >> 4      — 3 when bit 4 of the flags byte is
 *                                        clear, 2 when it is set (the
 *                                        binary's branch-free form of a
 *                                        2-vs-3 if/else)
 * The caller only ever tests the result against zero and takes the maximum
 * over every object, so 1 < 2 < 3 is an ordered status.
 *
 * `point` is a 5-byte-or-larger record whose byte 0 is an index into the
 * command-list's point tag_block (scenario+0x438, stride 0x60, sub-block at
 * +0x30 with stride 0x20) and whose byte 4 carries the flags.  In the inlined
 * copy those are actor+0xa4 / actor+0xa8; in the swarm copy they are
 * swarm_component+0x1c / +0x20.
 *
 * 0x57330 / ai_script.obj */
short ai_scripting_command_list_status_internal(int16_t scenario_index,
                                                void *record, int field_1a8_val,
                                                int child_handle, int reserved)
{
  const unsigned char *point;
  char *list;
  short status;

  (void)field_1a8_val;
  (void)child_handle;
  (void)reserved;

  point = (const unsigned char *)record;

  list = (char *)tag_block_get_element((char *)global_scenario_get() + 0x438,
                                       scenario_index, 0x60);
  /* One status variable set down an else-if chain and returned once
   * (measured, VC71).  The reference's `movb ..,%cl; notb %cl; movzbl
   * %cl,%eax; andl $0x10; orl $0x20; shrl $0x4` is VC71's branch-free
   * lowering of the 2-vs-3 if/else below, not a source expression.  Spelled
   * as that expression, as a ternary, or as early returns, VC71 sinks the
   * status-1 block below the success path and emits `movw $1,%ax` or
   * `xorl; movb %al`: 84.2% -> 98.2%. */
  if ((int)point[0] >= *(int *)(list + 0x30)) {
    status = 1;
  } else if (tag_block_get_element(list + 0x30, point[0], 0x20) ==
             (void *)0) {
    status = 1;
  } else if (point[4] & 0x10) {
    status = 2;
  } else {
    status = 3;
  }
  return status;
}

/* 0x00057380 — ai_scripting_command_list_status.
 *
 * Walks every object named by a packed ai_index reference
 * (object_list_get_first/object_list_get_next child-object iterator, single-int iterator
 * state at [EBP-0x18]) and returns the MAXIMUM per-object status:
 *   0 — no actor, or the actor is neither running a command list nor
 *       recently finished one
 *   1 — command list finished (index past the end), or the actor finished
 *       a command list within the last 0x96 ticks
 *   2 — running, point flag bit 4 set
 *   3 — running, point flag bit 4 clear
 *
 * An object carries its actor either directly (object+0x1a4, asserted NOT a
 * swarm actor) or through a swarm (object+0x1a8, asserted to BE a swarm
 * actor).  In the swarm case the actor's swarm record (+0x28) is searched
 * linearly for this object handle among its 0x18-based handle array; the
 * matching slot's component handle (parallel array at +0x58) supplies the
 * command-list point, and only components with flag bit 3 set report status.
 *
 * Asserts (Confirmed, strings at 0x253380 / 0x257098, file 0x25c394):
 *   "!actor->meta.swarm" ai_script.c:0xa80
 *   "actor->meta.swarm"  ai_script.c:0xa8d
 *
 * Frame: PUSH EBP; MOV EBP,ESP; SUB ESP,0x18 — six dword locals, no buffers.
 * EBX is pushed only once the loop is entered (0x573b3), which is why the
 * empty-iteration exit at 0x575c5 has a shorter epilogue and returns AX=DI.
 *
 * 0x57380 / ai_script.obj */
short ai_scripting_command_list_status(int ai_index)
{
  int iterator;
  int game_time;
  int object_index;
  char *object;
  char *actor;
  char *swarm;
  char *component;
  short index;
  short count;
  short overall;
  short status;

  overall = 0;
  game_time = game_time_get();
  for (object_index = object_list_get_first(ai_index, &iterator); object_index != -1;
       object_index = object_list_get_next(ai_index, &iterator)) {
    object = (char *)object_try_and_get_and_verify_type(object_index, 3);
    if (object == (char *)0)
      continue;
    status = 0;
    if (*(int *)(object + 0x1a4) != -1) {
      actor = (char *)datum_get(*(data_t **)0x6325a4, *(int *)(object + 0x1a4));
      assert_halt_msg_at("!actor->meta.swarm",
                         "c:\\halo\\SOURCE\\ai\\ai_script.c", 0xa80,
                         actor[6] == 0);
      if (*(short *)(actor + 0x6c) == 0xb) {
        status = ai_scripting_command_list_status_internal(
          *(short *)(actor + 0x9c), (void *)(actor + 0xa4),
          *(int *)(object + 0x1a4), object_index, 0);
        if (status != 0)
          goto accumulate;
      }
      goto recently_finished;
    }
    if (*(int *)(object + 0x1a8) == -1)
      goto accumulate;
    actor = (char *)datum_get(*(data_t **)0x6325a4, *(int *)(object + 0x1a8));
    assert_halt_msg_at("actor->meta.swarm", "c:\\halo\\SOURCE\\ai\\ai_script.c",
                       0xa8d, actor[6] != 0);
    if (*(short *)(actor + 0x6c) != 0xb || *(int *)(actor + 0x28) == -1)
      goto recently_finished;
    swarm = (char *)datum_get(*(data_t **)0x6325a0, *(int *)(actor + 0x28));
    count = *(short *)(swarm + 2);
    /* Loop spelling is match-critical (measured, VC71): the count>0 guard plus
     * an infinite loop with TWO breaks is what makes both loop exits converge
     * on one block, so the component lookup stays the loop's fall-through and
     * the redundant `CMP CX,DX; JGE` at 0x57522 survives.  Spelled as
     * `while (index < count && arr[index] != object_index)` or as a do/while,
     * VC71 proves index==count on the natural exit, forwards that edge
     * straight to the tail and sinks the whole component block past the
     * epilogue: 88.8% -> 75.8%. */
    index = 0;
    if (count > 0) {
      for (;;) {
        if (*(int *)(swarm + index * 4 + 0x18) == object_index)
          break;
        index++;
        if (index >= count)
          break;
      }
    }
    if (index >= count)
      goto recently_finished;
    component = (char *)datum_get(*(data_t **)0x63259c,
                                  *(int *)(swarm + index * 4 + 0x58));
    if ((component[2] & 8) == 0)
      goto recently_finished;
    status = ai_scripting_command_list_status_internal(
      *(short *)(actor + 0x9c), (void *)(component + 0x1c),
      *(int *)(object + 0x1a8), object_index, 0);
    if (status != 0)
      goto accumulate;
  recently_finished:
    /* Both arms share this tail in the reference (one copy at 0x5756e reached
     * by five jumps); writing it once with the gotos above is what keeps VC71
     * from duplicating the tail, the loop latch AND the epilogue. */
    if (*(int *)(actor + 0x94) != -1 &&
        *(int *)(actor + 0x94) + 0x96 >= game_time)
      status = 1;
  accumulate:
    overall = (overall > status) ? overall : status;
  }
  return overall;
}

/* 0x00058cc0 — ai_go_to_vehicle_override script command entry point.
 * Uses the same trace path and 0x100-byte name buffer as
 * ai_scripting_go_to_vehicle, but forwards allow_type9 = 1 to
 * ai_scripting_go_to_vehicle_internal.  The three parameters are stack
 * arguments at [EBP+8], [EBP+0xc], and [EBP+0x10]. */
void ai_scripting_go_to_vehicle_override(unsigned int ai_index,
                                         int vehicle_handle,
                                         const char *seat_substring)
{
  char local_104[0x100];

  if (*(char *)0x5aca59 != '\0') {
    ai_index_to_string(ai_index, (void *)global_scenario_get(), local_104,
                       0x100);
    error(2, "%s: ai_go_to_vehicle_override %s 0x%04X %s",
          hs_runtime_get_executing_thread_name(), local_104,
          vehicle_handle & 0xffff, seat_substring);
  }
  ai_scripting_go_to_vehicle_internal(ai_index, vehicle_handle,
                                      (int)seat_substring, 1);
}

/* 0x00058d40 — "ai_scripting_renew" HS script command.
 *
 * Refreshes every actor named by a packed ai_index_reference:
 *   1. binarizes the unit's two desired-movement scalars (+0x88/+0x8c) into
 *      0.0f / 1.0f at +0x90/+0x94, and
 *   2. tops the unit's grenades back up to a random count drawn from the
 *      actor_variant tag's [min,max] range, if the variant carries a grenade
 *      type at all.
 *
 * Name is Confirmed from the format string at 0x25d1fc ("%s: ai_scripting_renew
 * %s").
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
void ai_scripting_renew(int handle)
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
    error(2, "%s: ai_renew %s",
          hs_runtime_get_executing_thread_name(), local_11c);
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
        wanted =
          seed_random_range((unsigned int *)get_global_random_seed_address(),
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

/*
 * ai_scripting_free — free (detach) all actors from an encounter (ai_free).
 * Logs "[thread]: ai_free [encounter]", then for each actor in the
 * encounter asserts encounter_index != NONE, then calls
 * actor_flush_position_indices, encounter_detach_actor(handle, 0), and
 * encounterless_attach_actor. Finally calls encounters_update_dirty_status.
 * Actor handle is at local_1c+0x10 (iterator offset).
 * 0x575d0 / encounters.obj
 */
void ai_scripting_free(int param_1)
{
  char local_21c[512];
  char local_1c[24];
  void *uVar1;
  int iVar2;

  if (*(char *)0x5aca59) {
    uVar1 = global_scenario_get();
    ai_index_to_string(param_1, uVar1, local_21c, 0x200);
    error(2, "%s: ai_free %s", hs_runtime_get_executing_thread_name(),
          local_21c);
  }
  if (param_1 != -1) {
    ai_index_actor_iterator_new(param_1, local_1c);
    iVar2 = ai_index_actor_iterator_next(local_1c);
    while (iVar2 != 0) {
      if (*(int *)((char *)iVar2 + 0x34) == -1) {
        display_assert("actor->meta.encounter_index != NONE",
                       "c:\\halo\\SOURCE\\ai\\ai_script.c", 0xad1, 1);
        system_exit(-1);
      }
      actor_flush_position_indices(*(int *)(local_1c + 0x10));
      encounter_detach_actor(*(int *)(local_1c + 0x10), 0);
      encounterless_attach_actor(*(int *)(local_1c + 0x10));
      iVar2 = ai_index_actor_iterator_next(local_1c);
    }
    encounters_update_dirty_status();
  }
}

/*
 * ai_scripting_vehicle_encounter (0x57d00) — ai_vehicle_encounter script
 * command.  Binds a unit (biped/vehicle, object type mask 3) to an
 * encounter+squad by writing the resolved encounter index to unit+0x2e4 and the
 * squad index to unit+0x2e6 (both int16, -1 = NONE).  Before overwriting, if
 * the unit already carries an encounter index, every actor of that encounter
 * whose field_0x158 points at this unit is re-bound via
 * actor_change_encounter(actor_handle, encounter_index, squad_index).
 *
 * param_2 is a combined ai index: low 16 bits = signed encounter index into
 * the scenario encounters block (scenario+0x42c, element stride 0xb0), byte 2
 * = squad *name* index (selector 1) or direct squad index (selector 2), bits
 * 30..31 = selector.  Selector 0 means "encounter only" (squad 0); selector 3
 * is unreachable (display_assert at ai_script.c:0xc52 then system_exit(-1)).
 *
 * Confirmed: frame is a direct SUB ESP,0x220 (no _chkstk); the trace name
 * buffer is 512 bytes at EBP-0x220.
 * Confirmed: element sizes 0xb0 (scenario encounters block) and 0xe8
 * (encounter_definition->squads) are literal immediates in the original.
 * Confirmed: [EBP-0x1c] is actor_iter[1] — the actor handle produced by
 * encounter_actor_iterator_next — NOT an independent local (buffer-alias
 * hazard); it is re-read on every iteration.
 * Confirmed: unit+0x2e4/+0x2e6 are 16-bit loads/stores (MOV AX,word / MOV
 * word,CX); byte 2 of param_2 is a MOVZX byte load compared against the
 * uint16 at squad+0x22.
 * 0x57d00 / encounters.obj */
void ai_scripting_vehicle_encounter(int param_1, int param_2)
{
  int out_squad;
  int saved_encounter_index;
  char *unit;
  int out_encounter;
  encounter_definition *encounter_def;
  int actor_iter[3];
  char name_buf[512];
  char *scenario;
  unsigned int selector;
  short encounter_index;
  int squad_index;
  int i;
  void *squad;
  int actor;

  if (*(char *)0x5aca59 != '\0') {
    ai_index_to_string(param_2, (void *)global_scenario_get(), name_buf, 0x200);
    error(2, "%s: ai_vehicle_encounter <some unit> %s",
          hs_runtime_get_executing_thread_name(), name_buf);
  }
  if (param_1 == -1) {
    return;
  }

  unit = (char *)object_get_and_verify_type(param_1, 3);
  out_encounter = -1;
  out_squad = -1;
  if (param_2 == -1) {
    goto store_result;
  }

  scenario = (char *)global_scenario_get();
  encounter_index = (short)param_2;
  if (encounter_index < 0) {
    goto store_result;
  }
  saved_encounter_index = (int)encounter_index;
  if (saved_encounter_index >= *(int *)(scenario + 0x42c)) {
    goto store_result;
  }

  squad_index = 0;
  encounter_def = (encounter_definition *)tag_block_get_element(
    (char *)global_scenario_get() + 0x42c, (unsigned short)param_2, 0xb0);

  selector = (unsigned int)param_2 >> 0x1e;
  switch (selector) {
  case 0:
    /* Encounter only: squad 0. */
    break;
  case 1:
    /* Byte 2 is a squad *name* index; scan the squads block for the element
     * whose name index (squad+0x22) matches. */
    if (encounter_def->squads.count > 0) {
      i = 0;
      do {
        squad = tag_block_get_element(&encounter_def->squads, i, 0xe8);
        if (*(unsigned short *)((char *)squad + 0x22) ==
            (unsigned short)*(unsigned char *)((char *)&param_2 + 2)) {
          break;
        }
        squad_index = squad_index + 1;
        i = (int)(short)squad_index;
      } while (i < encounter_def->squads.count);
    }
    if ((int)(short)squad_index >= encounter_def->squads.count) {
      /* No match: fall through with squad 0 (matches the original). */
      squad_index = 0;
    }
    break;
  case 2:
    /* Byte 2 is a direct squad index. */
    squad_index = (int)*(unsigned char *)((char *)&param_2 + 2);
    break;
  default:
    display_assert("!\"unreachable\"", "c:\\halo\\SOURCE\\ai\\ai_script.c",
                   0xc52, 1);
    system_exit(-1);
    break;
  }

  if ((short)squad_index < 0) {
    goto store_result;
  }
  if ((int)(short)squad_index < encounter_def->squads.count) {
    out_encounter = param_2;
    out_squad = squad_index;
    if (encounter_index != -1 && (short)squad_index != -1 &&
        *(short *)(unit + 0x2e4) != -1) {
      /* Re-bind the actors of the unit's PREVIOUS encounter that reference
       * this unit.  actor_iter[1] is the current actor handle. */
      encounter_actor_iterator_new(actor_iter, (int)*(short *)(unit + 0x2e4));
      actor = encounter_actor_iterator_next(actor_iter);
      while (actor != 0) {
        if (*(int *)(actor + 0x158) == param_1) {
          actor_change_encounter(actor_iter[1], saved_encounter_index,
                                 (short)squad_index);
        }
        actor = encounter_actor_iterator_next(actor_iter);
      }
    }
  }

store_result:
  *(short *)(unit + 0x2e4) = (short)out_encounter;
  *(short *)(unit + 0x2e6) = (short)out_squad;
}
