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

  if (selector == 0) {
    /* profile name only */
    snprintf(buffer, buffer_size, (const char *)0x257984, element);
  } else if (selector == 1) {
    /* profile + element+0x8c sub-block (stride 0xac) */
    void *sub = tag_block_get_element((char *)element + 0x8c, sub_index, 0xac);
    snprintf(buffer, buffer_size, (const char *)0x253d30, element, sub);
  } else if (selector == 2) {
    /* profile + element+0x80 sub-block (stride 0xe8) */
    void *sub = tag_block_get_element((char *)element + 0x80, sub_index, 0xe8);
    snprintf(buffer, buffer_size, (const char *)0x253d30, element, sub);
  } else {
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

  if (encounter_handle == -1) {
    return -1;
  }

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
