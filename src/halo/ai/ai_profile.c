/* ai_profile.c — AI difficulty/profile subsystem.
 *
 * Corresponds to addresses 0x540b0-0x56320 in the XBE (ai_profile.obj TU).
 * Source path confirmed via __FILE__ string @ 0x25c0ac:
 *   c:\halo\SOURCE\ai\ai_profile.c
 *
 * Script-command strings observed:
 *   0x27e61c  "ai_profile_random"
 *   0x27e630  "ai_profile_disable"
 *
 * Functions are lifted incrementally below. Declarations come from the
 * force-included generated header (src/common.h -> build/generated/decl.h).
 */

/* ---------------------------------------------------------------------------
 * Subsystem lifecycle hooks (0x540b0-0x540e0).
 *
 * Each is a single `ret` in the delinked reference (16-byte aligned). The AI
 * profile data lives entirely in the scenario tag, so there is no per-subsystem
 * or per-map allocation to set up or tear down: all four hooks are empty.
 * Mirrors the identical empty quartet in ai_communication.obj
 * (ai_communication_initialize/_dispose/_initialize_for_new_map/
 *  _dispose_from_old_map at 0x42a30/0x42b80/0x42b90/0x42ca0).
 * ------------------------------------------------------------------------- */

/* ai_profile_initialize (0x540b0) — no global state to construct. */
void FUN_000540b0(void)
{
}

/* ai_profile_dispose (0x540c0) — no global state to release. */
void ai_profile_dispose(void)
{
}

/* ai_profile_initialize_for_new_map (0x540d0) — profiles come from the
 * scenario tag, loaded by the tag system; nothing to do per map. */
void FUN_000540d0(void)
{
}

/* ai_profile_dispose_from_old_map (0x540e0) — nothing allocated per map. */
void ai_profile_dispose_from_old_map(void)
{
}

/* ---------------------------------------------------------------------------
 * ai_index_reference: a packed 32-bit handle naming an ai_profile entry in the
 * scenario tag (block at scenario+0x42c, element size 0xb0) plus an optional
 * sub-reference. Bit layout:
 *     bits  0-15 : ai_profile block index
 *     bits 16-23 : sub-index (into element+0x80 [stride 0xe8] or
 *                             element+0x8c [stride 0xac])
 *     bits 30-31 : selector  0=profile only, 1=+0x8c sub, 2=+0x80 sub, 3=error
 * String form is "<profile>" or "<profile>/<sub>"; the keyword "none" maps to
 * the all-ones sentinel (-1). These helpers live in ai_profile.obj but assert
 * against ai_script.c line numbers.
 * ------------------------------------------------------------------------- */

/* FUN_000540f0 — parse a name string ("profile" or "profile/sub") into a
 * packed ai_index_reference. Returns true and writes the packed value (or the
 * -1 sentinel for "none"/not-found) to *out_value; returns false only when the
 * parse fails (name too long, profile/sub not found). 0x40 obj / 0x540f0 XBE.
 * Asserts (ai_script.c:0x57) that name and out_value are non-NULL. */
bool FUN_000540f0(void *scenario, const char *name, int *out_value)
{
  char prefix[32];           /* [ebp-0x24], 0x20 bytes */
  int result;                /* [ebp-0x4], accumulator (starts -1) */
  const char *slash;
  int prefix_index;
  int sub_index;
  int profile_index;

  result = -1;
  if (name == 0 || out_value == 0) {
    display_assert("ai_string && ai_index_reference",
                   "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x57, 1);
    system_exit(-1);
  }

  /* "none" -> success with the -1 sentinel */
  if (crt_stricmp(name, (const char *)0x254384) == 0) {
    *out_value = -1;
    return 1;
  }

  slash = strrchr(name, '/');
  if (slash == 0) {
    /* no '/': whole name is a profile name */
    profile_index = FUN_00053e20(scenario, name);
    if (profile_index != -1) {
      result = profile_index & 0xffff;
    }
  } else {
    int len = (int)(slash - name);
    if (len <= 0x1f) {
      void *element;

      csstrncpy(prefix, name, len);
      prefix[len] = '\0';
      prefix_index = FUN_00053e20(scenario, prefix);
      if (prefix_index != -1) {
        /* fetch the ai_profile element, then resolve the part after '/' */
        element = tag_block_get_element((char *)scenario + 0x42c,
                                        prefix_index, 0xb0);
        sub_index = FUN_00053e80(element, slash + 1);
        if (sub_index != -1) {
          /* selector 2: element+0x80 sub-block */
          result = (((sub_index & 0xff) | 0xffff8000) << 16)
                   | (prefix_index & 0xffff);
        } else {
          sub_index = FUN_00053ee0(element, slash + 1);
          if (sub_index != -1) {
            /* selector 1: element+0x8c sub-block */
            result = (((sub_index & 0xff) | 0x4000) << 16)
                     | (prefix_index & 0xffff);
          }
        }
      }
    }
  }

  *out_value = result;
  return result != -1;
}

/* FUN_00054220 — format a packed ai_index_reference back into its name string
 * ("profile", "profile/sub", "none", or "<error>"). Writes at most
 * buffer_size bytes. 0x170 obj / 0x54220 XBE. */
void FUN_00054220(unsigned int combined_index, void *scenario,
                  char *buffer, int buffer_size)
{
  void *element;
  unsigned int selector;
  unsigned char sub_index;

  if (combined_index == 0xffffffff) {
    csstrncpy(buffer, (const char *)0x254384, buffer_size);
    return;
  }

  element = tag_block_get_element((char *)scenario + 0x42c,
                                  combined_index & 0xffff, 0xb0);
  selector = combined_index >> 0x1e;
  sub_index = (unsigned char)(combined_index >> 16);

  /* NOTE: a switch (not an if-else-if cascade) is required to match MSVC's
   * CMP-chain selector dispatch; clang lowers the equivalent if-else-if to a
   * DEC-chain (sub/dec/dec), costing ~10pp VC71. See lift-learnings.md §19. */
  switch (selector) {
  case 0:
    /* profile name only */
    snprintf(buffer, buffer_size, (const char *)0x257984, element);
    return;
  case 1: {
    /* profile + element+0x8c sub-block (stride 0xac) */
    void *sub = tag_block_get_element((char *)element + 0x8c, sub_index, 0xac);
    snprintf(buffer, buffer_size, (const char *)0x253d30, element, sub);
    return;
  }
  case 2: {
    /* profile + element+0x80 sub-block (stride 0xe8) */
    void *sub = tag_block_get_element((char *)element + 0x80, sub_index, 0xe8);
    snprintf(buffer, buffer_size, (const char *)0x253d30, element, sub);
    return;
  }
  default:
    /* selector 3: invalid */
    csstrncpy(buffer, (const char *)0x253b58, buffer_size);
  }
}

/* FUN_00054310 — decode a packed ai_index_reference into the 3-int iterator
 * record out[0..2]: out[0]=profile index, out[1]=sub-key, out[2]=sub-bound.
 * On any invalid input out[0] is set to -1. 0x260 obj / 0x54310 XBE.
 * Asserts (ai_script.c:0xbf) that out is non-NULL. */
void FUN_00054310(unsigned int combined_index, int *out)
{
  void *ai_globals;
  void *element;
  int profile_index;
  unsigned int selector;
  int sub_index;

  ai_globals = FUN_0018e3b0();
  if (out == 0) {
    display_assert("iterator", "c:\\halo\\SOURCE\\ai\\ai_script.c", 0xbf, 1);
    system_exit(-1);
  }

  profile_index = combined_index & 0xffff;
  out[0] = profile_index;

  if (ai_globals == 0
      || *(char *)((char *)*(void **)0x632574 + 1) == 0
      || profile_index < 0
      || profile_index >= *(int *)((char *)ai_globals + 0x42c)) {
    out[0] = -1;
    return;
  }

  element = tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                  profile_index & 0xffff, 0xb0);
  selector = combined_index >> 0x1e;

  switch (selector) {
  case 0:
    out[1] = 0;
    out[2] = *(int *)((char *)element + 0x8c) - 1;
    return;

  case 1:
  case 2:
    sub_index = (int)(unsigned char)(combined_index >> 16);
    if (selector == 2) {
      if (sub_index < 0 || sub_index >= *(int *)((char *)element + 0x80)) {
        out[1] = -1;
      } else {
        void *sub = tag_block_get_element((char *)element + 0x80,
                                          sub_index, 0xe8);
        out[1] = *(short *)((char *)sub + 0x22);
      }
    } else {
      out[1] = sub_index;
    }
    break;

  default:
    out[0] = -1;
    return;
  }

  if (out[1] < 0 || out[1] >= *(int *)((char *)element + 0x8c)) {
    out[0] = -1;
    return;
  }
  out[2] = out[1];
}

/* FUN_00054430 — iterator step over the encounters named by an
 * ai_index_reference record (the out[3] produced by FUN_00054310). Returns the
 * current encounter's squad pointer and advances out[1]; returns NULL when the
 * record is exhausted (out[0]==-1 or out[1]>out[2]). 0x380 obj / 0x54430 XBE.
 * Asserts (ai_script.c:0x109) that iter is non-NULL. */
void *FUN_00054430(int *iter)
{
  void *result;
  char *encounter;
  int handle;

  result = 0;

  if (iter == 0) {
    display_assert("iterator", "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x109, 1);
    system_exit(-1);
  }

  handle = iter[0];
  if (handle != -1 && iter[1] <= iter[2]) {
    encounter = (char *)datum_get(*(data_t **)0x5ab270, handle);
    encounter = FUN_00054020(encounter, (short)(unsigned short)iter[1]);
    iter[1] = iter[1] + 1;
    result = encounter;
  }

  return result;
}

/* ---------------------------------------------------------------------------
 * ai_index_reference iterators. Two record layouts:
 *   Layout A (encounter/squad iterator, FUN_000544a0 begin / FUN_000545a0 next),
 *     5 ints: [0]=profile index, [1]=encounter-key filter (-1=wildcard),
 *     [2]=cursor scratch, [3]=loop cursor, [4]=loop bound (inclusive).
 *   Layout B (actor iterator, FUN_00054680 begin / FUN_00054750 next), 6 ints:
 *     [0]=clump handle, [1]=squad filter (-1=wildcard), [2]=platoon filter
 *     (-1=wildcard), [3..5]=embedded encounter_actor_iterator state.
 * ------------------------------------------------------------------------- */

/* FUN_000544a0 — begin an encounter/squad iterator over the squads named by a
 * packed ai_index_reference (Layout A). On invalid input iter[0] = -1.
 * 0x3f0 obj / 0x544a0 XBE. Asserts (ai_script.c:0x11a) iter non-NULL. */
void FUN_000544a0(unsigned int combined_index, void *iter_arg)
{
  int *iter;
  void *ai_globals;
  void *element;
  int profile_index;
  unsigned int selector;
  short sub_index;

  iter = (int *)iter_arg;
  ai_globals = FUN_0018e3b0();
  if (iter == 0) {
    display_assert("iterator", "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x11a, 1);
    system_exit(-1);
  }

  profile_index = combined_index & 0xffff;
  iter[0] = profile_index;

  if (ai_globals == 0
      || *(char *)((char *)*(void **)0x632574 + 1) == 0
      || profile_index < 0
      || profile_index >= *(int *)((char *)ai_globals + 0x42c)) {
    iter[0] = -1;
    return;
  }

  element = tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                  profile_index & 0xffff, 0xb0);
  selector = combined_index >> 0x1e;

  if (selector > 1) {
    if (selector != 2) {
      iter[0] = -1;
      return;
    }

    sub_index = (short)((unsigned char *)&combined_index)[2];
    if (sub_index < 0 || sub_index >= *(int *)((char *)element + 0x80)) {
      iter[0] = -1;
      return;
    }
    iter[2] = -1;
    iter[4] = sub_index;
    iter[3] = sub_index;
    iter[1] = -1;
    return;
  }

  iter[2] = -1;
  iter[3] = 0;
  iter[4] = *(int *)((char *)element + 0x80) - 1;
  if (selector == 0) {
    iter[1] = -1;
  } else {
    iter[1] = ((unsigned char *)&combined_index)[2];
  }
}

/* FUN_000545a0 — step an encounter/squad iterator (Layout A record from
 * FUN_000544a0). Scans element+0x80 sub-blocks (stride 0xe8) from iter[3] to
 * iter[4], skipping any whose field+0x22 != the iter[1] filter (-1 matches
 * all). Returns the squad pointer for a hit, NULL when exhausted.
 * 0x4f0 obj / 0x545a0 XBE. Asserts (ai_script.c:0x15f) iter non-NULL. */
int FUN_000545a0(void *iter_arg)
{
  int *iter;
  void *encounter;
  void *element;
  char *sub_base;
  void *sub;
  int profile_index;
  int result;

  iter = (int *)iter_arg;
  result = 0;
  if (iter == 0) {
    display_assert("iterator", "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x15f, 1);
    system_exit(-1);
  }

  profile_index = iter[0];
  if (profile_index != -1) {
    encounter = datum_get(*(data_t **)0x5ab270, profile_index);
    element = tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                    iter[0] & 0xffff, 0xb0);

    if (iter[3] <= iter[4]) {
      sub_base = (char *)element + 0x80;
      do {
        iter[2] = iter[3];
        iter[3] = iter[3] + 1;
        sub = tag_block_get_element(sub_base, iter[2], 0xe8);
        if (iter[1] == -1) {
          goto found;
        }
        if ((int)*(short *)((char *)sub + 0x22) == iter[1]) {
          goto found;
        }
      } while (iter[3] <= iter[4]);
    }
  }

  return result;

found:
  return (int)encounter_get_squad((char *)encounter,
                                  (short)(unsigned short)iter[2]);
}

/* FUN_00054680 — begin an actor iterator over the actors named by a packed
 * ai_index_reference (Layout B). On invalid input iter[0] = -1.
 * 0x5d0 obj / 0x54680 XBE. Asserts (ai_script.c:0x180) iter non-NULL. */
void FUN_00054680(unsigned int combined_index, void *iter_arg)
{
  int *iter;
  void *ai_globals;
  int profile_index;
  unsigned int selector;

  iter = (int *)iter_arg;
  ai_globals = FUN_0018e3b0();
  if (iter == 0) {
    display_assert("iterator", "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x180, 1);
    system_exit(-1);
  }

  profile_index = combined_index & 0xffff;
  iter[0] = profile_index;

  if (ai_globals == 0
      || *(char *)((char *)*(void **)0x632574 + 1) == 0
      || profile_index < 0
      || profile_index >= *(int *)((char *)ai_globals + 0x42c)) {
    iter[0] = -1;
    return;
  }

  /* element fetched for validation side effects; result not read afterward
   * (eax is overwritten by the selector ladder in the original). */
  tag_block_get_element((char *)global_scenario_get() + 0x42c,
                        profile_index & 0xffff, 0xb0);

  selector = combined_index >> 0x1e;
  iter[2] = -1;
  iter[1] = -1;

  switch (selector) {
  case 0:
    /* both filters wildcard */
    break;
  case 1:
    iter[2] = (unsigned char)(combined_index >> 16);
    break;
  case 2:
    iter[1] = (unsigned char)(combined_index >> 16);
    break;
  default:
    iter[0] = -1;
    return;
  }

  if (iter[0] != -1) {
    encounter_actor_iterator_new(iter + 3, iter[0]);
  }
}

/* FUN_00054750 — step an actor iterator (Layout B record from FUN_00054680).
 * Pulls the next actor from the embedded encounter_actor_iterator (iter+3),
 * skipping any whose squad (field+0x3a) or platoon (field+0x3c) does not match
 * the iter[1]/iter[2] filters (-1 matches all). Returns the actor, NULL when
 * exhausted. 0x6a0 obj / 0x54750 XBE. Asserts (ai_script.c:0x1ba) iter non-NULL. */
int FUN_00054750(void *iter_arg)
{
  int *iter;
  void *actor;

  iter = (int *)iter_arg;
  if (iter == 0) {
    display_assert("iterator", "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x1ba, 1);
    system_exit(-1);
  }

  for (;;) {
    actor = (void *)encounter_actor_iterator_next(iter + 3);
    if (actor == 0) {
      break;
    }
    if (iter[1] == -1 || iter[1] == (int)*(short *)((char *)actor + 0x3a)) {
      if (iter[2] == -1) {
        break;
      }
      if (iter[2] == (int)*(short *)((char *)actor + 0x3c)) {
        break;
      }
    }
  }

  return (int)actor;
}

/* FUN_000547c0 — relay all actors named by an ai_index_reference (plus each
 * actor's unit and child chain) through FUN_000ce2b0, keyed by the resource
 * handle from FUN_000ce200. Returns that handle, or -1 on bad input / no
 * resource. 0x710 obj / 0x547c0 XBE. */
int FUN_000547c0(int encounter_handle)
{
  int iter[6];               /* [ebp-0x18], Layout B */
  int resource;
  void *actor;
  int child;

  if (encounter_handle != -1) {
    resource = FUN_000ce200();
    if (resource != -1) {
      FUN_00054680((unsigned int)encounter_handle, iter);
      actor = (void *)FUN_00054750(iter);
      if (actor != 0) {
        do {
          if (*(int *)((char *)actor + 0x18) != -1) {
            FUN_000ce2b0(resource, *(int *)((char *)actor + 0x18));
          }

          child = *(int *)((char *)actor + 0x24);
          while (child != -1) {
            void *object = object_get_and_verify_type(child, 3);
            FUN_000ce2b0(resource, child);
            child = *(int *)((char *)object + 0x1ac);
          }

          actor = (void *)FUN_00054750(iter);
        } while (actor != 0);
      }
    }
    return resource;
  }

  return -1;
}

/* ---------------------------------------------------------------------------
 * ai_attach / ai_detach / ai_place script-command implementations.
 * The verbose AI-spew flag at 0x5aca59 gates a diagnostic error(2, ...) trace
 * at each entry; the AI-enabled gate at *(0x632574)+1 gates the actual work in
 * the attach path. (FUN_00054a80 keeps its legacy kb name
 * ai_profile_change_render_spray; behaviorally it is ai_attach over children.)
 * ------------------------------------------------------------------------- */

/* FUN_00054860 — ai_attach: create one actor (from the ai_profile squad named
 * by ai_ref) and attach it to the unit object unit_handle. No-ops if AI is
 * disabled or either handle is the -1 sentinel. 0x7b0 obj / 0x54860 XBE. */
void FUN_00054860(int unit_handle, unsigned int ai_ref)
{
  char buffer[0x100];          /* [ebp-0x10c] */
  void *scenario;
  void *element;
  void *squad;
  void *variant;
  void *actv;
  int *actr_tag;
  int profile_index;
  int selector;
  int sub_index;
  int i;

  scenario = global_scenario_get();

  if (*(char *)0x5aca59 != 0) {
    FUN_00054220(ai_ref, scenario, buffer, 0x100);
    error(2, (const char *)0x25c460,
          hs_runtime_get_executing_thread_name(),
          unit_handle & 0xffff, buffer);
  }

  if (*(char *)((char *)*(void **)0x632574 + 1) == 0)
    return;
  if (unit_handle == -1 || ai_ref == 0xffffffff)
    return;

  profile_index = ai_ref & 0xffff;
  if (profile_index < 0)
    return;
  if (profile_index >= *(int *)((char *)scenario + 0x42c))
    return;

  element = tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                  profile_index & 0xffff, 0xb0);
  selector = ai_ref >> 0x1e;
  sub_index = 0;

  if (selector == 2) {
    sub_index = *(unsigned char *)((char *)&ai_ref + 2);
    if (sub_index < 0)
      goto bad_squad;
  } else if (selector == 1) {
    i = 0;
    if (*(int *)((char *)element + 0x80) > 0) {
      do {
        void *sq = tag_block_get_element((char *)element + 0x80, i, 0xe8);
        if (*(short *)((char *)sq + 0x22)
            == *(unsigned char *)((char *)&ai_ref + 2)) {
          sub_index = i;
          if (sub_index < 0)
            goto bad_squad;
          break;
        }
        i++;
      } while (i < *(int *)((char *)element + 0x80));
    }
  }

  if (sub_index < *(int *)((char *)element + 0x80)) {
    squad = tag_block_get_element((char *)element + 0x80, sub_index, 0xe8);
    if (*(short *)((char *)squad + 0x20) != -1) {
      variant = tag_block_get_element((char *)scenario + 0x420,
                                      *(short *)((char *)squad + 0x20), 0x10);
      if (*(int *)((char *)variant + 0xc) != -1) {
        actv = tag_get(0x61637476, *(int *)((char *)variant + 0xc));
        if (*(int *)((char *)actv + 0x10) != -1) {
          actr_tag = (int *)tag_get(0x61637472, *(int *)((char *)actv + 0x10));
          actor_create_for_unit(
              (char)((*(unsigned int *)actr_tag >> 0x1a) & 0x101),
              unit_handle,
              *(int *)((char *)variant + 0xc),
              profile_index,
              sub_index,
              0,
              -1,
              (char)((*(unsigned int *)((char *)element + 0x20) >> 4) & 0x101),
              (short)*(unsigned short *)((char *)squad + 0x24),
              (short)*(unsigned short *)((char *)squad + 0x26),
              0xffff,
              0);
          encounters_update_dirty_status();
          return;
        }
      }
    }

    error(2, (const char *)0x25c408, element, squad);
    return;
  }

bad_squad:
  error(2, (const char *)0x25c3c0, element);
}

/* FUN_00054a80 (ai_profile_change_render_spray) — ai_attach over the children
 * of a parent object: iterates every child (FUN_000ce450/FUN_000ce320) and
 * attaches the same ai_ref to each. 0x9d0 obj / 0x54a80 XBE. */
void ai_profile_change_render_spray(int parent_handle, unsigned int ai_ref)
{
  int iter_state;
  int child;

  child = FUN_000ce450(parent_handle, &iter_state);
  if (child == -1)
    return;
  do {
    FUN_00054860(child, ai_ref);
    child = FUN_000ce320(parent_handle, &iter_state);
  } while (child != -1);
}

/* FUN_00054ac0 — ai_detach: detach (delete the attached actor of) one unit
 * object. No-op if the handle is -1 or it has no attached actor (object+0x1a4
 * == -1). 0xa10 obj / 0x54ac0 XBE. */
void FUN_00054ac0(int unit_handle)
{
  void *object;
  int actor_handle;

  if (*(char *)0x5aca59 != 0) {
    error(2, "%s: ai_detach unit 0x%04X",
          hs_runtime_get_executing_thread_name(),
          unit_handle & 0xffff);
  }

  if (unit_handle == -1)
    return;

  object = object_get_and_verify_type(unit_handle, 3);
  actor_handle = *(int *)((char *)object + 0x1a4);
  if (actor_handle == -1)
    return;

  actor_delete(actor_handle, 0);
}

/* FUN_00054b20 — ai_detach over the children of a parent object: iterates each
 * child (FUN_000ce450/FUN_000ce320) and detaches its attached actor (inlines
 * FUN_00054ac0). 0xa70 obj / 0x54b20 XBE. */
void FUN_00054b20(int parent_handle)
{
  int iter_state;
  int child;
  void *object;
  int actor_handle;

  child = FUN_000ce450(parent_handle, &iter_state);
  if (child == -1)
    return;

  do {
    if (*(char *)0x5aca59 != 0) {
      error(2, "%s: ai_detach unit 0x%04X",
            hs_runtime_get_executing_thread_name(),
            child & 0xffff);
    }
    if (child != -1) {
      object = object_get_and_verify_type(child, 3);
      actor_handle = *(int *)((char *)object + 0x1a4);
      if (actor_handle != -1)
        actor_delete(actor_handle, 0);
    }
    child = FUN_000ce320(parent_handle, &iter_state);
  } while (child != -1);
}

/* FUN_00054bb0 — ai_place: place (spawn) the encounter named by an
 * ai_index_reference via encounter_create. The two sub-arguments are the
 * sub-index for the matching selector and -1 otherwise. 0xb00 obj/0x54bb0 XBE. */
void FUN_00054bb0(unsigned int ai_ref)
{
  char buffer[0x100];
  int selector;
  unsigned char sub_byte;
  int arg_a;
  int arg_b;

  if (*(char *)0x5aca59 != 0) {
    FUN_00054220(ai_ref, global_scenario_get(), buffer, 0x100);
    error(2, (const char *)0x25c49c,
          hs_runtime_get_executing_thread_name(), buffer);
  }

  if (ai_ref == 0xffffffff)
    return;

  sub_byte = *(unsigned char *)((char *)&ai_ref + 2);
  selector = ai_ref >> 0x1e;

  arg_b = (selector == 2) ? sub_byte : -1;
  arg_a = ((ai_ref >> 0x1e) == 1) ? sub_byte : -1;

  encounter_create(ai_ref & 0xffff, (short)arg_a, (short)arg_b);
}

/* ---------------------------------------------------------------------------
 * ai_index_reference count accessor (the "how many of X" query).
 *
 * FUN_00055350 resolves a packed ai_index_reference to a tag record and reports
 * one of three count_type quantities about it:
 *     count_type 0 -> the record's "start"/min index
 *     count_type 1 -> the record's "end"/max index
 *     count_type 2 -> the span (end - start), clamped to >= 0
 * The record is located by the reference's selector (top 2 bits):
 *     selector 0 -> the ai_profile element itself (offsets +0x2a/+0x2c/+0x34)
 *     selector 1 -> a platoon record (encounter_get_platoon, offs +0x4/+6/+8/+c)
 *     selector 2 -> a squad record  (encounter_get_squad,    offs +0x16..+0x1c)
 * In addition to the EAX result the dispatcher returns two record fields through
 * the optional out parameters: *out_min receives a fixed record field (squad/
 * platoon "name" word at +0x16/+0x4, or the profile's +0x18) and *out_handle
 * receives a record dword (squad/platoon +0x1c/+0xc, or the profile's +0x34).
 * count_type arrives in EDI as an int16 (compared via di / sign-extended via
 * movsx). 0x12a0 obj / 0x55350 XBE. Asserts (ai_script.c) on bad count_type and
 * on the unreachable selector/count_type defaults. */
int FUN_00055350(unsigned int ai_ref, int *out_min, int *out_handle,
                 int count_type /* @<edi> */)
{
  int ret_val;      /* [ebp-0x4], EAX result */
  int handle_val;   /* [ebp-0x8], flows to *out_handle */
  int min_val;      /* EBX, flows to *out_min */
  void *scenario;
  void *element;
  char *record;
  int profile_index;
  unsigned int selector;
  short sub_index;

  ret_val = 0;
  handle_val = 0;
  min_val = 0;

  if ((short)count_type < 0 || (short)count_type >= 3) {
    display_assert("(count_type >= 0) && (count_type < NUMBER_OF_AI_COUNT_TYPES)",
                   "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x405, 1);
    system_exit(-1);
  }

  if (ai_ref == 0xffffffff)
    goto done;

  scenario = global_scenario_get();
  selector = ai_ref >> 0x1e;

  switch (selector) {
  case 0:
    profile_index = ai_ref & 0xffff;
    if (profile_index < 0
        || profile_index >= *(int *)((char *)scenario + 0x42c))
      goto done;
    element = datum_get(*(data_t **)0x5ab270, profile_index);

    switch ((short)count_type) {
    case 0:
      ret_val = *(short *)((char *)element + 0x2a);
      break;
    case 1:
      ret_val = *(short *)((char *)element + 0x2c);
      break;
    case 2:
      ret_val = (int)*(short *)((char *)element + 0x2a)
                - (int)*(short *)((char *)element + 0x2c);
      ret_val = (ret_val < 0) ? 0 : ret_val;
      break;
    default:
      display_assert("!\"unreachable\"",
                     "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x424, 1);
      system_exit(-1);
    }
    min_val = *(short *)((char *)element + 0x18);
    handle_val = *(int *)((char *)element + 0x34);
    break;

  case 1:
    profile_index = ai_ref & 0xffff;
    if (profile_index < 0
        || profile_index >= *(int *)((char *)scenario + 0x42c))
      goto done;
    element = datum_get(*(data_t **)0x5ab270, profile_index);
    sub_index = ((unsigned char *)&ai_ref)[2];
    if (sub_index < 0 || sub_index >= *(short *)((char *)element + 0xa))
      goto done;
    record = FUN_00054020((char *)element, sub_index);

    switch ((short)count_type) {
    case 0:
      min_val = *(short *)(record + 0x4);
      ret_val = *(short *)(record + 0x6);
      handle_val = *(int *)(record + 0xc);
      break;
    case 1:
      min_val = *(short *)(record + 0x4);
      ret_val = *(short *)(record + 0x8);
      handle_val = *(int *)(record + 0xc);
      break;
    case 2:
      min_val = *(short *)(record + 0x4);
      ret_val = (int)*(short *)(record + 0x6)
                - (int)*(short *)(record + 0x8);
      ret_val = (ret_val < 0) ? 0 : ret_val;
      handle_val = *(int *)(record + 0xc);
      break;
    default:
      display_assert("!\"unreachable\"",
                     "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x448, 1);
      system_exit(-1);
      min_val = *(short *)(record + 0x4);
      handle_val = *(int *)(record + 0xc);
    }
    break;

  case 2:
    profile_index = ai_ref & 0xffff;
    if (profile_index < 0
        || profile_index >= *(int *)((char *)scenario + 0x42c))
      goto done;
    element = datum_get(*(data_t **)0x5ab270, profile_index);
    sub_index = ((unsigned char *)&ai_ref)[2];
    if (sub_index < 0 || sub_index >= *(short *)((char *)element + 0x6))
      goto done;
    record = encounter_get_squad((char *)element, sub_index);

    switch ((short)count_type) {
    case 0:
      min_val = *(short *)(record + 0x16);
      ret_val = *(short *)(record + 0x18);
      handle_val = *(int *)(record + 0x1c);
      break;
    case 1:
      min_val = *(short *)(record + 0x16);
      ret_val = *(short *)(record + 0x1a);
      handle_val = *(int *)(record + 0x1c);
      break;
    case 2:
      min_val = *(short *)(record + 0x16);
      ret_val = (int)*(short *)(record + 0x18)
                - (int)*(short *)(record + 0x1a);
      ret_val = (ret_val < 0) ? 0 : ret_val;
      handle_val = *(int *)(record + 0x1c);
      break;
    default:
      display_assert("!\"unreachable\"",
                     "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x46d, 1);
      system_exit(-1);
      min_val = *(short *)(record + 0x16);
      handle_val = *(int *)(record + 0x1c);
    }
    break;

  default:
    display_assert("!\"unreachable\"",
                   "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x477, 1);
    system_exit(-1);
  }

done:
  if (out_min != 0)
    *out_min = min_val;
  if (out_handle != 0)
    *out_handle = handle_val;
  return ret_val;
}

/* FUN_00055620 — count_type 1 ("end"/max) accessor wrapper. 0x1570 obj. */
int FUN_00055620(unsigned int ai_ref)
{
  return FUN_00055350(ai_ref, 0, 0, 1);
}

/* FUN_00055640 — count_type 2 ("span") accessor wrapper. 0x1590 obj. */
int FUN_00055640(unsigned int ai_ref)
{
  return FUN_00055350(ai_ref, 0, 0, 2);
}

/* FUN_00055660 — count_type 0 ("start"/min) accessor wrapper. 0x15b0 obj. */
int FUN_00055660(unsigned int ai_ref)
{
  return FUN_00055350(ai_ref, 0, 0, 0);
}

/* FUN_00055680 — ratio accessor: count_type 0 result divided by the record's
 * *out_min field (numerator = EAX, denominator = the value written through
 * out_min). Returns 0.0f when the denominator is non-positive. 0x15d0 obj. */
float FUN_00055680(unsigned int ai_ref)
{
  int denom;
  int numer;
  union { int i; float f; } zero;

  zero.i = 0;
  numer = FUN_00055350(ai_ref, &denom, 0, 0);
  if (denom > 0)
    return (float)numer / (float)denom;
  return zero.f;
}

/* FUN_000556c0 — float-field accessor: count_type 0 resolves the record and
 * writes its handle dword through out_handle; that dword is reinterpreted as a
 * float (bit pattern, not a numeric conversion). 0x1610 obj. */
float FUN_000556c0(unsigned int ai_ref)
{
  union { int i; float f; } out;

  out.i = 0;
  FUN_00055350(ai_ref, 0, &out.i, 0);
  return out.f;
}

/* FUN_000556f0 — predicate: returns true if the ai_index_reference names any
 * encounter whose first byte is zero. Walks the encounter iterator built by
 * FUN_00054310/FUN_00054430; returns false if exhausted without a match.
 * 0x1640 obj. */
bool FUN_000556f0(unsigned int ai_ref)
{
  int iter[3];
  char *encounter;

  if (ai_ref == 0xffffffff)
    return 0;

  FUN_00054310(ai_ref, iter);
  encounter = (char *)FUN_00054430(iter);
  if (encounter == 0)
    return 0;

  do {
    if (*encounter == 0)
      return 1;
    encounter = (char *)FUN_00054430(iter);
  } while (encounter != 0);

  return 0;
}

/* ---------------------------------------------------------------------------
 * ai_kill / ai_kill_silent / ai_erase / ai_erase_all / ai_spawn_actor +
 * ai_debug select commands. Each public command emits a verbose diagnostic
 * trace gated by the AI-spew flag at 0x5aca59, then performs its work.
 * ------------------------------------------------------------------------- */

/* FUN_00054c40 — kill every actor named by the packed ai_index_reference in
 * EAX, forwarding by_player to actor_kill. Walks the Layout B actor iterator;
 * the live actor handle is the embedded iterator's current handle (iter[4]).
 * No-op when ai_ref is the -1 sentinel. 0x990 obj / 0x54c40 XBE. */
void FUN_00054c40(unsigned int ai_ref /* @<eax> */, char by_player)
{
  int iter[6];                 /* [ebp-0x18], Layout B actor iterator */

  if (ai_ref == 0xffffffff)
    return;

  FUN_00054680(ai_ref, iter);
  if (FUN_00054750(iter) != 0) {
    do {
      actor_kill(iter[4], by_player, 0);
    } while (FUN_00054750(iter) != 0);
  }
}

/* FUN_00054ca0 — ai_kill: kill the actors named by ai_ref (by_player = 0).
 * 0x9f0 obj / 0x54ca0 XBE. */
void FUN_00054ca0(unsigned int ai_ref)
{
  char buffer[0x100];          /* [ebp-0x100] */

  if (*(char *)0x5aca59 != 0) {
    FUN_00054220(ai_ref, global_scenario_get(), buffer, 0x100);
    error(2, (const char *)0x25c4ac,
          hs_runtime_get_executing_thread_name(), buffer);
  }

  FUN_00054c40(ai_ref, 0);
}

/* FUN_00054d00 — ai_kill_silent: kill the actors named by ai_ref silently
 * (by_player = 1). 0xa50 obj / 0x54d00 XBE. */
void FUN_00054d00(unsigned int ai_ref)
{
  char buffer[0x100];          /* [ebp-0x100] */

  if (*(char *)0x5aca59 != 0) {
    FUN_00054220(ai_ref, global_scenario_get(), buffer, 0x100);
    error(2, (const char *)0x25c4bc,
          hs_runtime_get_executing_thread_name(), buffer);
  }

  FUN_00054c40(ai_ref, 1);
}

/* FUN_00054d60 — ai_erase: erase the encounter/squad named by ai_ref. The
 * selector picks which sub-index (squad vs platoon) is passed; a non-matching
 * selector passes the -1 wildcard. 0xab0 obj / 0x54d60 XBE. */
void FUN_00054d60(unsigned int ai_ref)
{
  char buffer[0x100];          /* [ebp-0x100] */
  unsigned char sub_byte;      /* [ebp+0xa] -> dl */
  int sub_a;
  int sub_b;

  if (*(char *)0x5aca59 != 0) {
    FUN_00054220(ai_ref, global_scenario_get(), buffer, 0x100);
    error(2, (const char *)0x25c4d4,
          hs_runtime_get_executing_thread_name(), buffer);
  }

  if (ai_ref == 0xffffffff)
    return;

  sub_byte = *(unsigned char *)((char *)&ai_ref + 2);

  sub_b = ((ai_ref >> 0x1e) == 2) ? sub_byte : -1;
  sub_a = ((ai_ref >> 0x1e) == 1) ? sub_byte : -1;

  ai_erase(ai_ref & 0xffff, sub_a, sub_b, 0);
}

/* FUN_00054df0 — ai_erase_all: erase every encounter (wildcard).
 * 0xb40 obj / 0x54df0 XBE. */
void FUN_00054df0(void)
{
  if (*(char *)0x5aca59 != 0) {
    error(2, (const char *)0x25c4e4,
          hs_runtime_get_executing_thread_name());
  }

  ai_erase(-1, -1, -1, 0);
}

/* FUN_00054e20 — ai debug: select all actors (wildcard). Calls
 * ai_debug_select_actor(-1, -1) when AI is enabled (*(0x632574)+1).
 * 0xd70 obj / 0x54e20 XBE. */
void FUN_00054e20(void)
{
  if (*(char *)((char *)*(void **)0x632574 + 1) != 0)
    ai_debug_select_actor(-1, -1);
}

/* FUN_00054e40 — ai debug: select an encounter. The -1 sentinel selects the
 * current/all encounter; otherwise the low 16 bits index it. Gated by
 * *(0x632574)+1 (AI enabled). 0xd90 obj / 0x54e40 XBE. */
void FUN_00054e40(int encounter_ref)
{
  if (*(char *)((char *)*(void **)0x632574 + 1) == 0)
    return;

  if (encounter_ref == -1)
    ai_debug_select_encounter(-1);
  else
    ai_debug_select_encounter(encounter_ref & 0xffff);
}

/* FUN_00054e80 — ai_spawn_actor: spawn the actor(s) named by ai_ref. For a
 * direct squad reference (selector 2) the sub-index byte is the squad index;
 * for a profile reference (selector 1) it scans element+0x80 (squad sub-block,
 * stride 0xe8) for the squad whose +0x22 key matches the sub-index byte. On a
 * hit, encounter_spawn_actor(profile_index, squad_index). Gated by
 * *(0x632574)+1. 0xdd0 obj / 0x54e80 XBE. */
void FUN_00054e80(unsigned int ai_ref)
{
  char buffer[0x100];          /* [ebp-0x104] */
  void *element;
  void *sq;
  int profile_index;           /* [ebp-0x4] */
  int sub_byte;
  short squad_index;
  short i;

  if (*(char *)0x5aca59 != 0) {
    FUN_00054220(ai_ref, global_scenario_get(), buffer, 0x100);
    error(2, (const char *)0x25c4f8,
          hs_runtime_get_executing_thread_name(), buffer);
  }

  if (*(char *)((char *)*(void **)0x632574 + 1) == 0)
    return;
  if (ai_ref == 0xffffffff)
    return;

  profile_index = ai_ref & 0xffff;

  if ((ai_ref >> 0x1e) == 2) {
    squad_index = (short)*(unsigned char *)((char *)&ai_ref + 2);
    if (squad_index != -1)
      goto spawn;
    /* fall through to the selector==1 test (which fails) */
  }

  if ((ai_ref >> 0x1e) != 1)
    return;

  element = tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                  profile_index & 0xffff, 0xb0);
  if (*(int *)((char *)element + 0x80) <= 0)
    return;

  sub_byte = (int)((ai_ref >> 0x10) & 0xff);
  squad_index = -1;
  i = 0;
  do {
    sq = tag_block_get_element((char *)element + 0x80, (int)i, 0xe8);
    if ((int)*(short *)((char *)sq + 0x22) == sub_byte) {
      squad_index = i;
      break;
    }
    i++;
  } while ((int)i < *(int *)((char *)element + 0x80));

  if (squad_index == -1)
    return;

spawn:
  encounter_spawn_actor(profile_index, (int)squad_index);
}

/* ---------------------------------------------------------------------------
 * AI script-command interface: per-profile setters and timer/effect drivers.
 * Each resolves a packed ai_index_reference to its named profile (for the
 * optional trace spew), then walks the referenced encounters/squads/actors to
 * apply the effect. The on/off token comes from the string pair 0x25c530 /
 * 0x25c52c. Structurally identical to the ai_braindead family in encounters.obj.
 * ------------------------------------------------------------------------- */

/* FUN_00054f90 — ai_set_respawn: toggle the respawn flag on the named
 * encounter. 0xee0 obj / 0x54f90 XBE. */
void FUN_00054f90(unsigned int combined_index, char flag)
{
  char name[256];          /* [ebp-0x100] */

  if (*(char *)0x5aca59) {
    FUN_00054220(combined_index, global_scenario_get(), name, 0x100);
    error(2, (const char *)0x25c510, hs_runtime_get_executing_thread_name(),
          name, flag ? (const char *)0x25c530 : (const char *)0x25c52c);
  }
  if (combined_index != 0xffffffff) {
    encounter_set_respawn(combined_index & 0xffff, flag);
  }
}

/* FUN_00055010 — ai_set_deaf: toggle the deaf flag on the named encounter.
 * 0xf60 obj / 0x55010 XBE. */
void FUN_00055010(unsigned int combined_index, char flag)
{
  char name[256];          /* [ebp-0x100] */

  if (*(char *)0x5aca59) {
    FUN_00054220(combined_index, global_scenario_get(), name, 0x100);
    error(2, (const char *)0x25c534, hs_runtime_get_executing_thread_name(),
          name, flag ? (const char *)0x25c530 : (const char *)0x25c52c);
  }
  if (combined_index != 0xffffffff) {
    encounter_set_deaf(combined_index & 0xffff, flag);
  }
}

/* FUN_00055090 — ai_set_blind: toggle the blind flag on the named encounter.
 * 0xfe0 obj / 0x55090 XBE. */
void FUN_00055090(unsigned int combined_index, char flag)
{
  char name[256];          /* [ebp-0x100] */

  if (*(char *)0x5aca59) {
    FUN_00054220(combined_index, global_scenario_get(), name, 0x100);
    error(2, (const char *)0x25c54c, hs_runtime_get_executing_thread_name(),
          name, flag ? (const char *)0x25c530 : (const char *)0x25c52c);
  }
  if (combined_index != 0xffffffff) {
    encounter_set_blind(combined_index & 0xffff, flag);
  }
}

/* FUN_00055110 — ai_magically_see_unit: make every actor named by
 * combined_handle "magically see" unit_handle. For each actor: force its
 * encounter active (actor+0x34), then look up the unit's slot (FUN_00064b40)
 * and apply unit-effect 3 (actor_handle_unit_effect). 0x1060 obj / 0x55110. */
void FUN_00055110(unsigned int combined_handle, int unit_handle)
{
  char name[256];          /* [ebp-0x118] */
  int iter[6];             /* [ebp-0x18], Layout B actor iterator */
  void *actor;
  int encounter_handle;
  int slot;

  if (*(char *)0x5aca59) {
    FUN_00054220(combined_handle, global_scenario_get(), name, 0x100);
    error(2, (const char *)0x25c564, hs_runtime_get_executing_thread_name(),
          name, unit_handle & 0xffff);
  }

  if (combined_handle == 0xffffffff || unit_handle == -1) {
    return;
  }

  FUN_00054680(combined_handle, iter);
  actor = (void *)FUN_00054750(iter);
  while (actor != 0) {
    encounter_handle = *(int *)((char *)actor + 0x34);
    if (encounter_handle != -1) {
      encounter_force_activate(encounter_handle);
    }
    slot = FUN_00064b40(iter[4], unit_handle, 1, 0);
    if (slot != -1) {
      actor_handle_unit_effect(iter[4], slot, 3);
    }
    actor = (void *)FUN_00054750(iter);
  }
}

/* FUN_000551e0 — ai_magically_see: make all actors named by combined_handle
 * see every unit in unit_group. Iterates the unit group via
 * FUN_000ce450/FUN_000ce320 and relays each unit through FUN_00055110.
 * 0x1130 obj / 0x551e0 XBE. */
void FUN_000551e0(unsigned int combined_handle, int unit_group)
{
  int unit;
  int state;               /* [ebp-0x4] */

  unit = FUN_000ce450(unit_group, &state);
  if (unit == -1) {
    return;
  }
  do {
    FUN_00055110(combined_handle, unit);
    unit = FUN_000ce320(unit_group, &state);
  } while (unit != -1);
}

/* FUN_00055220 — ai_timer_start: set the timer-running flag (squad+0x11 = 1)
 * on every squad named by combined_index. 0x1170 obj / 0x55220 XBE. */
void FUN_00055220(unsigned int combined_index)
{
  char name[256];          /* [ebp-0x114] */
  int iter[5];             /* [ebp-0x14], Layout A squad iterator */
  void *squad;

  if (*(char *)0x5aca59) {
    FUN_00054220(combined_index, global_scenario_get(), name, 0x100);
    error(2, (const char *)0x25c588, hs_runtime_get_executing_thread_name(),
          name);
  }

  if (combined_index == 0xffffffff) {
    return;
  }

  FUN_000544a0(combined_index, iter);
  squad = (void *)FUN_000545a0(iter);
  while (squad != 0) {
    *(char *)((char *)squad + 0x11) = 1;
    squad = (void *)FUN_000545a0(iter);
  }
}

/* FUN_000552b0 — ai_timer_expire: expire the timer on every squad named by
 * combined_index (encounter_squad_timer_expire(iter[0], iter[2])).
 * 0x1200 obj / 0x552b0 XBE. */
void FUN_000552b0(unsigned int combined_index)
{
  char name[256];          /* [ebp-0x114] */
  int iter[5];             /* [ebp-0x14], Layout A squad iterator */
  void *squad;

  if (*(char *)0x5aca59) {
    FUN_00054220(combined_index, global_scenario_get(), name, 0x100);
    error(2, (const char *)0x25c5a0, hs_runtime_get_executing_thread_name(),
          name);
  }

  if (combined_index == 0xffffffff) {
    return;
  }

  FUN_000544a0(combined_index, iter);
  squad = (void *)FUN_000545a0(iter);
  while (squad != 0) {
    encounter_squad_timer_expire(iter[0], (short)iter[2]);
    squad = (void *)FUN_000545a0(iter);
  }
}

/* ---------------------------------------------------------------------------
 * ai_attack / ai_defend / ai_maneuver / ai_maneuver_enable: per-encounter mode
 * setters. Each resolves the packed ai_index_reference, then walks the
 * FUN_00054310/FUN_00054430 encounter iterator poking a single mode byte on
 * every named encounter. Verbose trace gated by 0x5aca59.
 * ------------------------------------------------------------------------- */

/* FUN_00055750 — ai_attack: clear encounter[0] (attack mode) on every named
 * encounter. 0x16a0 obj / 0x55750 XBE. */
void FUN_00055750(unsigned int combined_index)
{
  char name[256];          /* [ebp-0x10c] */
  int iter[3];             /* [ebp-0xc] */
  char *encounter;

  if (*(char *)0x5aca59) {
    FUN_00054220(combined_index, global_scenario_get(), name, 0x100);
    error(2, (const char *)0x25c5f8, hs_runtime_get_executing_thread_name(),
          name);
  }

  if (combined_index == 0xffffffff) {
    return;
  }

  FUN_00054310(combined_index, iter);
  encounter = (char *)FUN_00054430(iter);
  if (encounter == 0) {
    return;
  }
  do {
    encounter[0] = 0;
    encounter = (char *)FUN_00054430(iter);
  } while (encounter != 0);
}

/* FUN_000557e0 — ai_defend: set encounter[0] (defend mode) on every named
 * encounter. 0x1730 obj / 0x557e0 XBE. */
void FUN_000557e0(unsigned int combined_index)
{
  char name[256];          /* [ebp-0x10c] */
  int iter[3];             /* [ebp-0xc] */
  char *encounter;

  if (*(char *)0x5aca59) {
    FUN_00054220(combined_index, global_scenario_get(), name, 0x100);
    error(2, (const char *)0x25c60c, hs_runtime_get_executing_thread_name(),
          name);
  }

  if (combined_index == 0xffffffff) {
    return;
  }

  FUN_00054310(combined_index, iter);
  encounter = (char *)FUN_00054430(iter);
  if (encounter == 0) {
    return;
  }
  do {
    encounter[0] = 1;
    encounter = (char *)FUN_00054430(iter);
  } while (encounter != 0);
}

/* FUN_00055870 — ai_maneuver: set encounter[1] (maneuver flag) on every named
 * encounter. 0x17c0 obj / 0x55870 XBE. */
void FUN_00055870(unsigned int combined_index)
{
  char name[256];          /* [ebp-0x10c] */
  int iter[3];             /* [ebp-0xc] */
  char *encounter;

  if (*(char *)0x5aca59) {
    FUN_00054220(combined_index, global_scenario_get(), name, 0x100);
    error(2, (const char *)0x25c620, hs_runtime_get_executing_thread_name(),
          name);
  }

  if (combined_index == 0xffffffff) {
    return;
  }

  FUN_00054310(combined_index, iter);
  encounter = (char *)FUN_00054430(iter);
  if (encounter == 0) {
    return;
  }
  do {
    encounter[1] = 1;
    encounter = (char *)FUN_00054430(iter);
  } while (encounter != 0);
}

/* FUN_00055900 — ai_maneuver_enable: enable/disable maneuvering on every named
 * encounter. Stores the inverse of the enable flag into encounter[2] (a
 * "maneuver disabled" byte): enable -> 0, disable -> 1. 0x1850 obj / 0x55900. */
void FUN_00055900(unsigned int combined_index, char flag)
{
  char name[256];          /* [ebp-0x10c] */
  int iter[3];             /* [ebp-0xc] */
  char *encounter;
  char disabled;

  if (*(char *)0x5aca59) {
    FUN_00054220(combined_index, global_scenario_get(), name, 0x100);
    error(2, (const char *)0x25c634, hs_runtime_get_executing_thread_name(),
          name, flag ? (const char *)0x25c530 : (const char *)0x25c52c);
  }

  if (combined_index == 0xffffffff) {
    return;
  }

  FUN_00054310(combined_index, iter);
  encounter = (char *)FUN_00054430(iter);
  if (encounter == 0) {
    return;
  }
  disabled = (flag == 0);
  do {
    encounter[2] = disabled;
    encounter = (char *)FUN_00054430(iter);
  } while (encounter != 0);
}

/* FUN_000559a0 — squad-migration core (ai_migrate per-squad mapper). 0x18f0 obj
 * / 0x559a0 XBE. Given one SOURCE squad (squad_index, with its resolved 'actr'
 * tag actr_tag and 'actv' tag actv_tag, plus the source profile index
 * field_0x34), finds the best matching squad in the DESTINATION encounter
 * (encounter_handle, @<eax>) and returns its squad index. Iterates the dest
 * encounter's squads (FUN_000544a0/FUN_000545a0) and records, in priority
 * order, the first dest squad that matches on five levels:
 *   slot1 ([ebp-0x8])  self: match_flag set and dest squad index == squad_index
 *   slot2 ([ebp-0xc])  same-variant: dest squad's 'actv' tag == actv_tag
 *   slot3 ([ebp-0x10]) same-actor:   dest squad's 'actr' tag == actr_tag
 *   slot4 ([ebp-0x14]) same-type:    actr_tag[+0x14] (actor type) matches
 *   slot5 ([ebp-0x18]) fallback:     first dest squad
 * Resolution walks slots 1..5; the first set slot wins, emits a verbose trace
 * (gated by *(char*)0x5aca57) and is range-checked against the dest squads
 * count. If no slot is set but the dest has >=1 squad, returns 0; if the dest
 * has no squads, returns -1. */
int16_t FUN_000559a0(unsigned int encounter_handle /* @<eax> */, int field_0x34,
                     int16_t squad_index, void *actr_tag, void *actv_tag,
                     char match_flag, const void *debug_str)
{
  int iter[5];               /* [ebp-0x34] FUN_000544a0/FUN_000545a0 record */
  void *scenario;            /* [ebp-0x1c] */
  void *dest_element;        /* [ebp-0x20] dest profile element             */
  void *dest_squads;         /* [ebp-0x4]  dest squads tag_block            */
  int slot1;                 /* [ebp-0x8]  self                             */
  int slot2;                 /* [ebp-0xc]  same-variant                     */
  int slot3;                 /* [ebp-0x10] same-actor                       */
  int slot4;                 /* [ebp-0x14] same-type                        */
  int slot5;                 /* [ebp-0x18] fallback                         */
  int cur;                   /* esi: current dest squad index               */
  void *squad;               /* FUN_000545a0 return                         */
  void *element;             /* per-squad tag element                       */
  void *actr_element;        /* actor-palette element                       */
  void *cand_actv;           /* edi: this dest squad's 'actv' tag           */
  void *cand_actr;           /* ebx: this dest squad's 'actr' tag           */
  short variant_index;       /* ax: dest squad's actor-variant index        */
  int found;                 /* ebx in resolution: chosen squad index       */

  scenario = global_scenario_get();
  dest_element = tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                       encounter_handle & 0xffff, 0xb0);
  slot1 = -1;
  slot2 = -1;
  slot3 = -1;
  slot4 = -1;
  slot5 = -1;
  FUN_000544a0(encounter_handle, iter);
  squad = (void *)FUN_000545a0(iter);
  if (squad == 0)
    goto no_squad_match;

  dest_squads = (char *)dest_element + 0x80;
  do {
    cur = iter[2];
    element = tag_block_get_element(dest_squads, cur, 0xe8);
    variant_index = *(short *)((char *)element + 0x20);
    cand_actv = 0;
    cand_actr = 0;
    if (variant_index >= 0
        && (int)variant_index < *(int *)((char *)scenario + 0x420)) {
      actr_element = tag_block_get_element((char *)scenario + 0x420,
                                           (int)variant_index, 0x10);
      if (*(int *)((char *)actr_element + 0xc) != -1
          && tag_get_group_tag(*(int *)((char *)actr_element + 0xc))
                 == 0x61637476) {
        cand_actv = tag_get(0x61637476, *(int *)((char *)actr_element + 0xc));
        if (*(int *)((char *)cand_actv + 0x10) != -1)
          cand_actr = tag_get(0x61637472, *(int *)((char *)cand_actv + 0x10));
      }
    }
    cur = iter[2];

    if ((short)slot1 == (short)-1 && match_flag != 0
        && (int)squad_index == cur)
      slot1 = cur;
    if ((short)slot2 == (short)-1 && actv_tag != 0 && cand_actv != 0
        && actv_tag == cand_actv)
      slot2 = cur;
    if ((short)slot3 == (short)-1 && actr_tag != 0 && cand_actr != 0
        && actr_tag == cand_actr)
      slot3 = cur;
    if ((short)slot4 == (short)-1 && actr_tag != 0 && cand_actr != 0
        && *(short *)((char *)actr_tag + 0x14)
               == *(short *)((char *)cand_actr + 0x14))
      slot4 = cur;
    if ((short)slot5 == (short)-1)
      slot5 = cur;

    squad = (void *)FUN_000545a0(iter);
  } while (squad != 0);

  /* --- resolution: first set slot, in priority order, wins --------------- */
  found = slot1;
  if ((short)found != (short)0xffff) {
    if (*(char *)0x5aca57)
      error(2, (const char *)0x25c76c /* "%s unchanged" */, debug_str);
    goto validate;
  }

  found = slot2;
  if ((short)found != (short)0xffff) {
    if (*(char *)0x5aca57) {
      void *dst_sq;
      void *name;
      dst_sq = tag_block_get_element(dest_squads, (int)(short)found, 0xe8);
      name = (void *)0x253b58; /* "<error>" */
      if (field_0x34 != -1 && (short)squad_index != (short)0xffff) {
        void *src_prof;
        void *src_sq;
        short si;
        src_prof = tag_block_get_element(
            (char *)global_scenario_get() + 0x42c, field_0x34 & 0xffff, 0xb0);
        src_sq = tag_block_get_element((char *)src_prof + 0x80,
                                       (int)squad_index, 0xe8);
        si = *(short *)((char *)src_sq + 0x20);
        if (si >= 0 && (int)si < *(int *)((char *)scenario + 0x420)) {
          void *src_actr;
          src_actr = tag_block_get_element((char *)scenario + 0x420,
                                           (int)si, 0x10);
          if (*(int *)((char *)src_actr + 0xc) != -1)
            name = (void *)tag_get_name(*(int *)((char *)src_actr + 0xc));
        }
      }
      error(2, (const char *)0x25c74c /* "%s -> %s (same-variant %s)" */,
            debug_str, dst_sq, name);
    }
    goto validate;
  }

  found = slot3;
  if ((short)found != (short)0xffff) {
    if (*(char *)0x5aca57) {
      void *dst_sq;
      void *name;
      dst_sq = tag_block_get_element(dest_squads, (int)(short)found, 0xe8);
      if (actv_tag == 0)
        name = (void *)0x253b58; /* "<error>" */
      else if (*(int *)((char *)actv_tag + 0x10) == -1)
        name = (void *)0x25ad08; /* "<none>" */
      else
        name = (void *)tag_get_name(*(int *)((char *)actv_tag + 0x10));
      error(2, (const char *)0x25c6d0 /* "%s -> %s (same-actor %s)" */,
            debug_str, dst_sq, name);
    }
    goto validate;
  }

  found = slot4;
  if ((short)found != (short)0xffff) {
    if (*(char *)0x5aca57) {
      void *dst_sq;
      const char *type_name;
      dst_sq = tag_block_get_element(dest_squads, (int)(short)found, 0xe8);
      type_name = FUN_0003a760(*(short *)((char *)actr_tag + 0x14));
      error(2, (const char *)0x25c6b4 /* "%s -> %s (same-type %s)" */,
            debug_str, dst_sq, type_name);
    }
    goto validate;
  }

  found = slot5;
  if ((short)found != (short)0xffff) {
    if (*(char *)0x5aca57) {
      void *dst_sq;
      dst_sq = tag_block_get_element(dest_squads, (int)(short)found, 0xe8);
      error(2, (const char *)0x25c68c /* "%s -> %s (no matching types ...)" */,
            debug_str, dst_sq);
    }
    goto validate;
  }

no_squad_match:
  dest_squads = (char *)dest_element + 0x80;
  if (*(int *)((char *)dest_element + 0x80) <= 0)
    return -1;
  found = 0;
  if (*(char *)0x5aca57) {
    void *dst_sq;
    dst_sq = tag_block_get_element(dest_squads, 0, 0xe8);
    error(2, (const char *)0x25c654 /* "%s -> %s (no matching squads ...)" */,
          debug_str, dst_sq);
  }
  goto count_check;

validate:
  if ((short)found == (short)0xffff)
    return (int16_t)(short)found;
  if ((short)found >= 0) {
  count_check:
    if ((int)(short)found < *(int *)dest_squads)
      return (int16_t)(short)found;
  }
  display_assert("(found_squad_index >= 0) && (found_squad_index < "
                 "target_encounter_definition->squads.count)",
                 "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x5fc, 1);
  system_exit(-1);
  return (int16_t)(short)found;
}

/* ai_migrate (FUN_00055dd0) — migrate all AI of a source encounter into a
 * destination encounter. 0x1d20 obj / 0x55dd0 XBE (largest in the TU). Both
 * args are packed ai_index_references (only the low 16 bits matter). Builds a
 * source-squad -> dest-squad map (FUN_000559a0 migration core per squad),
 * re-attaches live actors, rewrites pending creation records and BSP-resident
 * actors, then refreshes team status and dirty flags. match_flag =
 * (source==dest) skips identity mappings. encounter_handle (source) is @<eax>;
 * param_3/param_4 drive an optional per-actor FUN_00036dc0 (encounters.c passes
 * 0,0 so that path is inert). */
void FUN_00055dd0(int encounter_handle /* @<eax> */, int dest_encounter,
                  int param_3, int param_4)
{
  short target_squad_indices[64]; /* [ebp-0xc8], 0x80 bytes, init 0xffff */
  int squad_iter[5];              /* [ebp-0x34], Layout A (FUN_000544a0) */
  int actor_iter[3];              /* [ebp-0x2c], FUN_00059a00 (iter[1]=handle) */
  char enc_iter[0x1c];            /* [ebp-0x3c], FUN_00059b10 */
  void *scenario;                 /* [ebp-0x40] */
  void *src_datum;                /* [ebp-0xc]  source encounter datum */
  void *dst_datum;                /* [ebp-0x1c] dest encounter datum   */
  void *src_element;              /* [ebp-0x44] source profile element  */
  void *dst_element;              /* [ebp-0x8]  dest profile element    */
  int src_index;                  /* [ebp-0x10] / [ebp-0x18] */
  int dst_index;                  /* [ebp-0x14] */
  char match_flag;                /* [ebp-0x4]  */
  void *squad;                    /* loop-1 squad pointer (esi) */
  int src_squad;                  /* [ebp-0x2c] iter cursor (ebx) */
  void *sub_element;              /* [ebp-0x48] */
  int actr_tag;                   /* [ebp-0x20] */
  void *actv_tag;                 /* esi in loop 1 */
  void *actv_element;
  void *actor;                    /* loop-2/4 actor record */
  char *record;                   /* loop-3 pending record (ebx) */
  short cur_squad;                /* si */
  short mapped;                   /* target_squad_indices[cur_squad] */

  if (encounter_handle == -1)
    return;
  if (dest_encounter == -1)
    return;

  src_index = encounter_handle & 0xffff;
  dst_index = dest_encounter & 0xffff;
  if (src_index == -1 || dst_index == -1)
    return;

  scenario = global_scenario_get();
  src_datum = datum_get(*(data_t **)0x5ab270, src_index);
  dst_datum = datum_get(*(data_t **)0x5ab270, dst_index);
  src_element = tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                      src_index & 0xffff, 0xb0);
  dst_element = tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                      dst_index & 0xffff, 0xb0);
  csmemset(target_squad_indices, -1, 0x80);
  match_flag = (char)(src_index == dst_index);

  /* --- Loop 1: build the source-squad -> dest-squad map ----------------- */
  FUN_000544a0((unsigned int)encounter_handle, squad_iter);
  squad = (void *)FUN_000545a0(squad_iter);
  while (squad != 0) {
    src_squad = squad_iter[2];
    if (src_squad < 0 || src_squad >= 0x40) {
      display_assert("(source_iterator.squad_index >= 0) && "
                     "(source_iterator.squad_index < MAXIMUM_SQUADS_PER_ENCOUNTER)",
                     "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x623, 1);
      system_exit(-1);
    }

    if (*(short *)((char *)squad + 0x18) > 0
        || *(char *)((char *)src_datum + 0x1e) != 0) {
      sub_element = tag_block_get_element((char *)src_element + 0x80,
                                          src_squad, 0xe8);
      actr_tag = 0;
      actv_tag = 0;
      if ((short)*(short *)((char *)sub_element + 0x20) >= 0
          && (int)*(short *)((char *)sub_element + 0x20)
                 < *(int *)((char *)scenario + 0x420)) {
        void *actr_element =
            tag_block_get_element((char *)scenario + 0x420,
                                  (int)*(short *)((char *)sub_element + 0x20),
                                  0x10);
        if (*(int *)((char *)actr_element + 0xc) != -1
            && tag_get_group_tag(*(int *)((char *)actr_element + 0xc))
                   == 0x61637476) {
          actv_element = tag_get(0x61637476,
                                 *(int *)((char *)actr_element + 0xc));
          if (*(int *)((char *)actv_element + 0x10) != -1) {
            actr_tag = (int)tag_get(0x61637472,
                                    *(int *)((char *)actv_element + 0x10));
            actv_tag = actv_element;
          }
        }
      }

      crt_sprintf((char *)0x5ab100, (const char *)0x25c864 /* "squad %s" */,
                  sub_element);
      target_squad_indices[src_squad] =
          FUN_000559a0((unsigned int)dest_encounter, src_index,
                       (int16_t)src_squad, (void *)actr_tag, actv_tag,
                       match_flag, (const void *)0x5ab100);
    }

    squad = (void *)FUN_000545a0(squad_iter);
  }

  /* --- Loop 2: re-attach live actors of the source encounter ------------ */
  encounter_actor_iterator_new(actor_iter, src_index);
  actor = (void *)encounter_actor_iterator_next(actor_iter);
  while (actor != 0) {
    cur_squad = *(short *)((char *)actor + 0x3a);
    if (cur_squad < 0 || cur_squad >= 0x40) {
      display_assert("(current_squad_index >= 0) && "
                     "(current_squad_index < MAXIMUM_SQUADS_PER_ENCOUNTER)",
                     "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x651, 1);
      system_exit(-1);
    }
    mapped = target_squad_indices[cur_squad];
    if (mapped != (short)0xffff && !(match_flag && mapped == cur_squad)) {
      if (mapped < 0
          || (int)mapped >= *(int *)((char *)dst_element + 0x80)) {
        display_assert("(target_squad_indices[current_squad_index] >= 0) && "
                       "(target_squad_indices[current_squad_index] < "
                       "target_encounter_definition->squads.count)",
                       "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x65d, 1);
        system_exit(-1);
      }
      FUN_0003baa0(actor_iter[1], dst_index, (int16_t)mapped);
      if ((char)param_3 != 0)
        FUN_00036dc0(actor_iter[1], (char)param_4, 0);
    }
    actor = (void *)encounter_actor_iterator_next(actor_iter);
  }

  /* --- Loop 3: rewrite pending records (only if source is active) ------- */
  if (*(char *)((char *)src_datum + 0x1e) != 0) {
    encounter_iterator_next(enc_iter, 0);
    record = (char *)FUN_00059b50(enc_iter);
    while (record != 0) {
      if ((*(int *)(record + 0x44) & 0xffff) == src_index) {
        cur_squad = *(short *)(record + 0x48);
        if (cur_squad < 0 || cur_squad >= 0x40) {
          display_assert("(current_squad_index >= 0) && "
                         "(current_squad_index < MAXIMUM_SQUADS_PER_ENCOUNTER)",
                         "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x677, 1);
          system_exit(-1);
        }
        mapped = target_squad_indices[cur_squad];
        if (mapped != (short)0xffff
            && !(match_flag && mapped == cur_squad)) {
          if (mapped < 0
              || (int)mapped >= *(int *)((char *)dst_element + 0x80)) {
            display_assert("(target_squad_indices[current_squad_index] >= 0) && "
                           "(target_squad_indices[current_squad_index] < "
                           "target_encounter_definition->squads.count)",
                           "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x683, 1);
            system_exit(-1);
          }
          *(int *)(record + 0x44) = dst_index;
          *(short *)(record + 0x48) = target_squad_indices[cur_squad];
        }
      }
      record = (char *)FUN_00059b50(enc_iter);
    }

    if (match_flag == 0) {
      if ((src_index & 0xc0000000) == 0)
        *(char *)((char *)src_datum + 0x1e) = 0;
      *(char *)((char *)dst_datum + 0x1e) = 1;
    }
  }

  /* --- Loop 4: BSP-resident actors across all encounters ---------------- */
  encounter_actor_iterator_new(actor_iter, -1);
  actor = (void *)encounter_actor_iterator_next(actor_iter);
  while (actor != 0) {
    if ((*(int *)((char *)actor + 0x30) & 0xffff) == src_index) {
      cur_squad = *(short *)((char *)actor + 0x38);
      if (cur_squad < 0 || cur_squad >= 0x40) {
        display_assert("(source_iterator.squad_index >= 0) && "
                       "(source_iterator.squad_index < MAXIMUM_SQUADS_PER_ENCOUNTER)",
                       "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x6a4, 1);
        system_exit(-1);
      }
      mapped = target_squad_indices[cur_squad];
      if (mapped != (short)0xffff && !(match_flag && mapped == cur_squad)) {
        if (mapped < 0
            || (int)mapped >= *(int *)((char *)dst_element + 0x80)) {
          display_assert("(target_squad_indices[current_squad_index] >= 0) && "
                         "(target_squad_indices[current_squad_index] < "
                         "target_encounter_definition->squads.count)",
                         "c:\\halo\\SOURCE\\ai\\ai_script.c", 0x6b0, 1);
          system_exit(-1);
        }
        *(int *)((char *)actor + 0x30) = dst_index;
        *(short *)((char *)actor + 0x38) = target_squad_indices[cur_squad];
        if (match_flag == 0
            && *(short *)((char *)dst_element + 0x7e)
                   == global_structure_bsp_index_get()) {
          encounterless_detach_actor(actor_iter[1]);
          encounter_attach_actor(actor_iter[1],
                                 *(int *)((char *)actor + 0x30),
                                 *(short *)((char *)actor + 0x38), 1);
        }
      }
    }
    actor = (void *)encounter_actor_iterator_next(actor_iter);
  }

  /* --- Tail: refresh team status + dirty flags -------------------------- */
  if (*(short *)((char *)src_datum + 2) != *(short *)((char *)dst_datum + 2))
    ai_update_team_status();
  encounters_update_dirty_status();
}
