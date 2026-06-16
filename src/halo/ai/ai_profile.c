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
