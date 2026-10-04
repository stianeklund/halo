#ifdef HALO_RNG_TRACE
#include "halo/math/rng_trace.h"
#endif
#include "../../x87_math.h"
#include "model_animations.h"
#line 1
/* FUN_00120250 (0x120250) — Allocate a rectangle in a texture page's packed
 * bitmap layout.
 *
 * kb.json maps this address into model_animations.obj by link-time object
 * grouping; the assert's __FILE__ string
 * ("c:\halo\SOURCE\memory\texture_page.c") confirms the real source TU is the
 * texture-page allocator, not animations.
 *
 * Confirmed: cdecl, 4 args (page ptr, width int16, height int16, immediate
 * bool). Confirmed: returns int — the new datum handle on success, -1 on
 * failure (EBX seeded to -1 at 0x120259, returned via MOV EAX,EBX at
 * 0x120339, or OR EAX,-1 directly at 0x12032f/0x120336).
 * Confirmed: page+0x0 = contains_unsorted_textures (bool, from the assert
 * string "immediate || texture_page->contains_unsorted_textures").
 * Confirmed: page+0x8/+0xa = page width/height (signed int16, JG comparisons
 * at 0x120292/0x1202a6). Confirmed: page+0x10 = used-area accumulator (int,
 * added to at 0x120304/0x120308, restored on rollback at 0x120320/0x120324).
 * Confirmed: page+0x18 = data_t* handle table, passed to data_new_at_index
 * (0x1202cb) and datum_delete (0x120327). Byte offsets on `page` match the
 * existing FUN_0011fef0(page, ...) call site in bitmap_utilities.c (page+8,
 * page+0xa, page+0x18), so `page` stays an untyped pointer per that
 * precedent rather than a newly invented struct.
 * Confirmed: FUN_0011fef0 return entry+0x8/+0xa store width/height,
 * entry+0x2 stores (page->contains_unsorted_textures == 0) (SETZ at
 * 0x1202f8).
 * Confirmed: FUN_0011ff70(page) returns bool in AL (TEST AL,AL at 0x120316);
 * kb.json's prior `void FUN_0011ff70(void)` decl mismatched the call site's
 * push+cleanup=1 (hazard §7_GETTER_SWALLOWED) and has been corrected.
 * Confirmed: on immediate-commit failure, the used-area add is undone and
 * the just-allocated datum is deleted via datum_delete(page->data, handle)
 * before returning -1 (0x120320-0x12032d).
 */
int FUN_00120250(void *page, short width, short height, bool immediate)
{
  char *pg;
  void *entry;
  int area;
  int handle;

  pg = (char *)page;

  texture_page_verify(page);

  if (!immediate && *pg == 0) {
    display_assert("immediate || texture_page->contains_unsorted_textures",
                   "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x60, 1);
    system_exit(-1);
  }

  if (width <= *(short *)(pg + 8) && height <= *(short *)(pg + 0xa)) {
    area = (int)width * (int)height;
    if (*(int *)(pg + 0x10) + area <
        (int)*(short *)(pg + 0xa) * (int)*(short *)(pg + 8)) {
      handle = data_new_at_index(*(data_t **)(pg + 0x18));
      if (handle != -1) {
        entry = FUN_0011fef0(pg, handle);
        *(short *)((char *)entry + 8) = width;
        *(short *)((char *)entry + 0xa) = height;
        *(bool *)((char *)entry + 2) = (bool)(*pg == 0);
        *(int *)(pg + 0x10) += area;
        if (immediate) {
          if (!FUN_0011ff70(pg)) {
            *(int *)(pg + 0x10) -= area;
            datum_delete(*(data_t **)(pg + 0x18), handle);
            return -1;
          }
        }
        return handle;
      }
    }
  }
  return -1;
}

/* FUN_00120340 (0x120340) — Reclaim unsorted texture-page entries and
 * trigger a resort.
 *
 * kb.json maps this address into model_animations.obj by link-time object
 * grouping; the assert's __FILE__ string
 * ("c:\halo\SOURCE\memory\texture_page.c") confirms the real source TU is the
 * texture-page allocator, same as FUN_00120250/FUN_00120400 above/below.
 *
 * Confirmed: cdecl, 1 arg (page ptr, matches [EBP+8] loaded once into ESI at
 * 0x120345). Confirmed: void return (plain RET at 0x1203f2).
 * Confirmed: calls texture_page_verify() unconditionally first, same as
 * FUN_00120250/FUN_00120400.
 * Confirmed: asserts page->contains_unsorted_textures (pg+0x0, byte) is
 * nonzero — display_assert("texture_page->contains_unsorted_textures",
 * "c:\halo\SOURCE\memory\texture_page.c", 0x8a, true) then system_exit(-1) on
 * failure (0x12034e-0x12036b), same message text as FUN_00120400's guard but
 * a different line number (0x8a vs 0xaa) since it's a different call site in
 * the same source file.
 * Confirmed: walks the 12-byte-stride entry array at
 * *(int*)(pg+0x18)+0x34, count *(short*)(*(int*)(pg+0x18)+0x2e) — the same
 * table/entry layout as FUN_00120250/FUN_00120400 (entry+0x0 salt, entry+0x2
 * bool, entry+0x8/+0xa width/height). The count/table pointer is reloaded
 * from *(pg+0x18) at the bottom of each iteration (0x1203b1-0x1203b8),
 * matching FUN_00120400's precedent of not trusting the table pointer across
 * loop iterations.
 * Confirmed: for every entry whose salt (entry+0x0) is nonzero AND whose
 * bool flag (entry+0x2) is zero (i.e. an allocated-but-unsorted entry),
 * subtracts that entry's area (height*width, read as entry+0xa * entry+0x8
 * per the decompiled expression order which mirrors the ECX=width/EAX=height
 * load order at 0x12038e-0x120396) from the page's used-area accumulator
 * (pg+0x10) and deletes the datum via datum_delete(*(data_t**)(pg+0x18),
 * (int)i) — the loop index i is sign-extended (MOVSX EDX,BX at 0x12039c)
 * before being passed as the handle, and the delete uses the *current*
 * (pre-increment) index (0x120399-0x1203ae).
 * Confirmed: after the loop, clears *pg = 0 (0x1203bf), then calls
 * FUN_0011ff70(pg) (bool result in AL, same corrected decl as
 * FUN_00120250/FUN_00120400) and asserts the result is nonzero —
 * display_assert("resort_succeeded",
 * "c:\halo\SOURCE\memory\texture_page.c", 0x9d, true) then system_exit(-1)
 * on failure (0x1203cf-0x1203e9).
 */
void FUN_00120340(void *page)
{
  char *pg;
  short *entry;
  short count;
  short i;

  pg = (char *)page;

  texture_page_verify(page);

  if (*pg == 0) {
    display_assert("texture_page->contains_unsorted_textures",
                   "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x8a, 1);
    system_exit(-1);
  }

  entry = *(short **)(*(int *)(pg + 0x18) + 0x34);
  count = *(short *)(*(int *)(pg + 0x18) + 0x2e);
  i = 0;
  if (0 < count) {
    do {
      if (*entry != 0 && *((char *)entry + 2) == 0) {
        *(int *)(pg + 0x10) -= (int)entry[5] * (int)entry[4];
        datum_delete(*(data_t **)(pg + 0x18), (int)i);
      }
      i = i + 1;
      entry = entry + 6;
      count = *(short *)(*(int *)(pg + 0x18) + 0x2e);
    } while (i < count);
  }
  *pg = 0;

  if (!FUN_0011ff70(pg)) {
    display_assert("resort_succeeded",
                   "c:\\halo\\SOURCE\\memory\\texture_page.c", 0x9d, 1);
    system_exit(-1);
  }
}

/* FUN_00120400 (0x120400) — Flush a texture page's unsorted-entry marks and
 * clear the page-level dirty flag.
 *
 * kb.json maps this address into model_animations.obj by link-time object
 * grouping; the assert's __FILE__ string
 * ("c:\halo\SOURCE\memory\texture_page.c") confirms the real source TU is the
 * texture-page allocator, same as FUN_00120250 immediately above.
 *
 * Confirmed: cdecl, 1 arg (page ptr). Confirmed: void return.
 * Confirmed: calls texture_page_verify() unconditionally first, same as
 * FUN_00120250 (0x120407).
 * Confirmed: asserts page->contains_unsorted_textures (pg+0x0, byte) is
 * nonzero — display_assert("texture_page->contains_unsorted_textures",
 * "c:\halo\SOURCE\memory\texture_page.c", 0xaa, true) then system_exit(-1) on
 * failure (0x120411-0x12042f). Unlike FUN_00120250's `!immediate && *pg==0`
 * guard, this function has no alternate condition — it unconditionally
 * requires *pg != 0.
 * Confirmed: calls FUN_0011ff70(page) (bool result in AL) at 0x120432; the
 * entire body below is gated on that result being nonzero (TEST AL,AL / JZ
 * 0x12046b skips straight to the epilogue, leaving *pg untouched).
 * Confirmed: on success, walks the 12-byte-stride entry array at
 * *(int*)(pg+0x18)+0x34, count *(short*)(*(int*)(pg+0x18)+0x2e) — the same
 * table/entry base FUN_00120250 populates via FUN_0011fef0 (entry+0x2 is the
 * per-entry bool FUN_00120250 sets from page->contains_unsorted_textures).
 * The table pointer *(int*)(pg+0x18) is reloaded every iteration
 * (0x12045a), matching the disassembly exactly. For every entry whose first
 * short field is nonzero, sets entry+0x2 (byte) = 1 (0x120450-0x120456).
 * Confirmed: after the loop, clears *pg = 0 (0x120467).
 */
void FUN_00120400(void *page)
{
  char *pg;
  char *table;
  short *entry;
  short count;
  short i;

  pg = (char *)page;

  texture_page_verify(page);

  if (*pg == 0) {
    display_assert("texture_page->contains_unsorted_textures",
                   "c:\\halo\\SOURCE\\memory\\texture_page.c", 0xaa, 1);
    system_exit(-1);
  }

  if (FUN_0011ff70(pg)) {
    table = *(char **)(pg + 0x18);
    entry = *(short **)(table + 0x34);
    count = *(short *)(table + 0x2e);
    i = 0;
    if (0 < count) {
      do {
        table = *(char **)(pg + 0x18);
        if (*entry != 0) {
          *(unsigned char *)(entry + 1) = 1;
        }
        i = i + 1;
        entry = entry + 6;
        count = *(short *)(table + 0x2e);
      } while (i < count);
    }
    *pg = 0;
  }
}

/* FUN_00120470 (0x120470) — Free a texture-page rectangle allocated by
 * FUN_00120250, then commit the page.
 *
 * kb.json maps this address into model_animations.obj by link-time object
 * grouping; same texture-page allocator TU as FUN_00120250/FUN_00120400
 * immediately above (page+0x18 = data_t* handle table, matching precedent).
 *
 * Confirmed: cdecl, 2 args (page ptr, datum handle int). Confirmed: void
 * return.
 * Confirmed: calls texture_page_verify() unconditionally first, same as
 * FUN_00120250/FUN_00120400 (0x120477).
 * Confirmed: CALL datum_delete(*(data_t**)(page+0x18), handle) at 0x120484 —
 * ECX (page+0x18 deref) pushed last = first arg, EAX (param_2) pushed first =
 * second arg, matching datum_delete(data_t *data, int datum_handle).
 * Confirmed: CALL FUN_0011ff70(page) at 0x12048a with page (ESI) as the sole
 * pushed arg; its bool return in AL is discarded here — no TEST/branch on EAX
 * follows, unlike FUN_00120400's gated use of the same call.
 * Confirmed: single ADD ESP,0xc at 0x12048f cleans the cumulative 3 pushes
 * from both calls (2 for datum_delete + 1 for FUN_0011ff70), not 3 args to
 * FUN_0011ff70 — the call_site_audit's §7_GETTER_SWALLOWED hint is a false
 * positive against the disassembly, which shows exactly one PUSH ESI
 * (0x120489) immediately before CALL 0x0011ff70.
 */
void FUN_00120470(void *page, int handle)
{
  char *pg;

  pg = (char *)page;

  texture_page_verify(page);

  datum_delete(*(data_t **)(pg + 0x18), handle);

  FUN_0011ff70(pg);
}

/* FUN_001204a0 (0x1204a0) — Try to resize a texture page to new dimensions,
 * rolling back on failure to commit.
 *
 * kb.json maps this address into model_animations.obj by link-time object
 * grouping; same texture-page allocator TU as FUN_00120250/FUN_00120340/
 * FUN_00120400/FUN_00120470 immediately above (unconditional
 * texture_page_verify() first, and FUN_0011ff70(page) to commit — same
 * corrected bool-returning decl already used by those functions).
 *
 * Confirmed: cdecl, 3 args (page ptr at [EBP+8]->ESI, new width int16 at
 * [EBP+0xc], new height int16 at [EBP+0x10]). Confirmed: returns bool in AL
 * (MOV AL,1 at 0x1204d5 vs XOR AL,AL at 0x1204ed).
 * Confirmed: page+0x8/+0xa = page width/height (int16), same offsets
 * FUN_00120250 compares against.
 * Confirmed: saves the old width/height (MOV DI,[ESI+8] / MOV BX,[ESI+0xa]
 * at 0x1204b6/0x1204ba) before overwriting them with the new values
 * (0x1204bf/0x1204c3), then calls FUN_0011ff70(page) to attempt to commit
 * the new size (0x1204c7). If that succeeds (TEST AL,AL / JZ 0x1204d1
 * not taken), returns true with the new dimensions left in place.
 * Confirmed: on failure (JZ 0x1204d1 taken), restores the saved old
 * width/height (0x1204db/0x1204df) and calls FUN_0011ff70(page) again to
 * re-commit the reverted size (0x1204e3), discarding that second call's
 * result (no TEST/branch on it — matches FUN_00120470's precedent of an
 * unchecked FUN_0011ff70 return), then returns false.
 */
bool FUN_001204a0(void *page, short width, short height)
{
  char *pg;
  short old_width;
  short old_height;

  pg = (char *)page;

  texture_page_verify(page);

  old_width = *(short *)(pg + 8);
  old_height = *(short *)(pg + 0xa);

  *(short *)(pg + 8) = width;
  *(short *)(pg + 0xa) = height;

  if (FUN_0011ff70(pg)) {
    return true;
  }

  *(short *)(pg + 8) = old_width;
  *(short *)(pg + 0xa) = old_height;

  FUN_0011ff70(pg);

  return false;
}

/* FUN_00120500 (0x120500) — Get a pointer to a specific animation frame's data.
 *
 * Given an animation structure and a frame index, returns a pointer to the
 * frame data for that frame. If compression is active (flag bit 0 at
 * animation+0x3a set, and model_animation_compression is nonzero), returns a pointer
 * offset by the compressed data offset (animation+0x88). Otherwise,
 * returns a pointer offset by frame_size * frame_index.
 *
 * The frame data itself lives in tag_data at animation+0xa0, resolved
 * via tag_data_get_pointer.
 *
 * Confirmed: cdecl, 2 args (animation ptr, frame_index short).
 * Confirmed: CALL tag_data_get_pointer(animation+0xa0, 0, 0) at 0x12052b.
 * Confirmed: Assert "frame_index>=0 && frame_index<animation->frame_count" at
 * 0x120555. Confirmed: CALL display_assert at 0x120555, system_exit(-1) at
 * 0x12055c. Confirmed: Compressed path returns ESI + [EDI+0x88] at 0x12056b.
 * Confirmed: Uncompressed path returns ESI + MOVSX([EDI+0x24]) * MOVSX(BX) at
 * 0x120578-0x120582.
 */
void *FUN_00120500(void *animation, short frame_index)
{
  int compressed;
  char *data;
  char *anim;

  anim = (char *)animation;

  if (((((animation_t *)anim)->field_3a & 1) != 0) && model_animation_compression != '\0') {
    compressed = 1;
  } else {
    compressed = 0;
  }

  data = (char *)tag_data_get_pointer(anim + 0xa0, 0, 0);

  if (frame_index < 0 || frame_index >= ((animation_t *)anim)->frame_count) {
    display_assert("frame_index>=0 && frame_index<animation->frame_count",
                   "c:\\halo\\SOURCE\\models\\model_animation_definitions.c",
                   0x47a, 1);
    system_exit(-1);
  }

  if (compressed) {
    return (void *)(data + *(int *)(anim + 0x88));
  }
  return (void *)(data + (int)((animation_t *)anim)->frame_size * (int)frame_index);
}

/* FUN_00120590 (0x120590) — Resolve a pointer to a specific animation frame's
 * raw data block via tag_data_get_pointer.
 *
 * Given an animation structure pointer, a frame_index, and the per-frame
 * stride (frame_size), bounds-checks frame_index against animation->frame_count
 * (int16 at +0x22), then calls tag_data_get_pointer on the tag_data block at
 * animation+0x48 with offset = frame_index * frame_size and size = frame_size,
 * returning that pointer to the caller.
 *
 * Confirmed void-EAX-return (lift-silent-bugs §16): the original leaves
 * tag_data_get_pointer's result in EAX across the epilogue (no instruction
 * between the CALL at 0x1205d7 and RET touches EAX) and its only caller,
 * animation_frame_get_xy_translation (0x120ee0), reads EAX immediately after
 * `CALL 0x00120590` (MOV EDX,[EAX] / MOV EAX,[EAX+4] at 0x120ef9/0x120f00) as
 * the returned frame-data pointer. Must NOT be declared void.
 *
 * Confirmed: cdecl, 3 args (animation ptr, frame_index short, frame_size
 * short). Confirmed: assert "frame_index>=0 &&
 * frame_index<animation->frame_count" at 0x1205a7. Confirmed: MOVSX
 * EAX,[EBP+0x10] (frame_size); MOVSX ECX,SI (frame_index); IMUL ECX,EAX; PUSH
 * EAX; PUSH ECX; ADD EDI,0x48; PUSH EDI; CALL tag_data_get_pointer at 0x1205d7.
 * Source: c:\halo\SOURCE\models\model_animation_definitions.c, line 0x48e
 * (1166).
 */
void *FUN_00120590(void *animation, short frame_index, short frame_size)
{
  char *anim;
  int offset;
  int size;

  anim = (char *)animation;

  if (frame_index < 0 || frame_index >= ((animation_t *)anim)->frame_count) {
    display_assert("frame_index>=0 && frame_index<animation->frame_count",
                   "c:\\halo\\SOURCE\\models\\model_animation_definitions.c",
                   0x48e, 1);
    system_exit(-1);
  }

  size = (int)frame_size;
  offset = (int)frame_index * size;
  return tag_data_get_pointer(anim + 0x48, offset, size);
}

/* FUN_001205f0 (0x1205f0) — look up a string in an indexed string table.
 * Returns "#<invalid>" if the index is out of range or the entry is NULL. */
const char *FUN_001205f0(void *string_table, int16_t index)
{
  int16_t *table = (int16_t *)string_table;
  const char *result;

  if (index >= *table || (result = *(const char **)(*(int32_t *)(table + 2) +
                                                    index * 8)) == NULL) {
    result = "#<invalid>";
  }
  return result;
}

/* animation_is_compressed (0x120620)
 *
 * Name: PAL 2342 model_animations.c animation_is_compressed (T2), whose
 * match_assert("...model_animations.c", 38, animation) matches the
 * "animation" assert at line 0x26 here.
 * Confirmed: register arg — TEST ESI,ESI at entry, no prologue, no stack
 *   args; every caller loads ESI before the CALL.
 * Confirmed: TEST byte [ESI+0x3a],1 (compressed flag); then the byte
 *   model_animation_compression (0x322600) or
 *   dword [ESI+0x88] == 0.  Returns via
 *   MOV EAX,1 / XOR EAX,EAX.
 */
char FUN_00120620(int animation)
{
  if (animation == 0) {
    display_assert("animation", "c:\\halo\\SOURCE\\models\\model_animations.c",
                   0x26, 1);
    system_exit(-1);
  }

  return (((animation_t *)animation)->field_3a & 1) &&
         (model_animation_compression != '\0' || *(int *)(animation + 0x88) == 0);
}

/* build_damage_animation_index (0x120670) — flatten a (damage_type,
 * damage_direction, damage_part) triple into a single animation index.
 *
 * Confirmed: cdecl, 3 stack args, each tested as a 16-bit value (TEST DI,DI /
 * CMP DI,0x4 at 0x120679-0x120682, and likewise for BX and SI). Confirmed
 * bounds from the assert strings: damage_type and damage_direction in [0,4),
 * damage_part in [0,0xb). Assert lines 0x37/0x38/0x39 of
 * c:\halo\SOURCE\models\model_animations.c, each followed by
 * system_exit(-1). Confirmed index form: LEA EAX,[EBX+EDI*4] (damage_direction
 * + damage_type*4), IMUL EAX,EAX,0xb, ADD EAX,ESI (damage_part).
 */
short build_damage_animation_index(short damage_type, short damage_direction,
                                   short damage_part)
{
  if (damage_type < 0 || damage_type >= 4) {
    display_assert(
      "damage_type>=0 && damage_type<NUMBER_OF_ANIMATION_DAMAGE_TYPES",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x37, true);
    system_exit(-1);
  }
  if (damage_direction < 0 || damage_direction >= 4) {
    display_assert("damage_direction>=0 && "
                   "damage_direction<NUMBER_OF_ANIMATION_DAMAGE_DIRECTIONS",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x38, true);
    system_exit(-1);
  }
  if (damage_part < 0 || damage_part >= 0xb) {
    display_assert("damage_part>=0 && damage_part<NUMBER_OF_DAMAGE_PARTS",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x39, true);
    system_exit(-1);
  }
  return (short)((damage_direction + damage_type * 4) * 0xb + damage_part);
}

/* animation_get_x_offsets (0x120710) — Accumulate the first float of every
 * frame's translation record across the whole animation, and separately
 * capture that running total at one specific frame (tick_index).
 *
 * anim_entry+0x22 = frame_count (int16; same field asserted
 * "animation->frame_count" by FUN_00120500/FUN_00120590 above). anim_entry+
 * 0x26 = translation-record discriminator (int16; same field
 * animation_frame_get_xy_translation below checks against 1), selecting a
 * per-frame stride of 2/3/4 floats (0x8/0xc/0x10 bytes); any other value
 * skips the accumulate and pointer advance for that frame. anim_entry+0x34 =
 * tick_index (int16; confirmed by unit_get_melee_range_and_ticks in units.c,
 * which stores this same anim_tag+0x34 into *out_tick_count). anim_entry+
 * 0x54 = float* to the raw per-frame translation stream — accessed only
 * here, meaning otherwise unproven (not the tag_data_get_pointer path
 * FUN_00120590 uses via +0x48).
 *
 * Confirmed: cdecl, 3 args. Confirmed: FLD [0x2533c0] (the global 0.0f
 * constant used throughout this codebase, e.g. actor_combat.c) initializes
 * the accumulator unconditionally at 0x120717, before the frame_count>0
 * test. Confirmed: the discriminator (+0x26) and tick_index (+0x34) are each
 * read from memory exactly ONCE, at 0x120735/0x120739, only on the
 * frame_count>0 path, and held in registers (EBX/CX) for every loop
 * iteration — not re-read per frame. Confirmed: do-while loop
 * 0x120740-0x12076a; the running accumulator is snapshotted into local_8 via
 * FST (no pop) at 0x120763 when frame_index==tick_index, checked AFTER that
 * frame's accumulate/advance. Confirmed: damage_time_out (param_3,
 * [EBP+0x10]) receives the final accumulator via FSTP at 0x120776 (or a bare
 * FSTP ST0 pop-and-discard at 0x12077a if NULL); tick_out (param_2,
 * [EBP+0xc]) receives local_8 via a plain MOV dword copy at
 * 0x120783/0x120786 (not an FPU store — matches the original exactly).
 */
void animation_get_x_offsets(int anim_entry, int tick_out, int damage_time_out)
{
  char *anim;
  float *data;
  int16_t frame_count;
  int16_t translation_type;
  int16_t tick_index;
  int16_t frame_index;
  float accum;
  float tick_value;

  anim = (char *)anim_entry;
  data = (float *)((animation_t *)anim)->field_54;
  frame_index = 0;
  tick_value = 0.0f;
  accum = *(float *)0x2533c0;
  frame_count = ((animation_t *)anim)->frame_count;

  if (frame_count > 0) {
    translation_type = ((animation_t *)anim)->field_26;
    tick_index = *(int16_t *)(anim + 0x34);
    do {
      switch (translation_type) {
      case 1:
        accum = accum + *data;
        data += 2;
        break;
      case 2:
        accum = accum + *data;
        data += 3;
        break;
      case 3:
        accum = accum + *data;
        data += 4;
        break;
      }
      if (frame_index == tick_index) {
        tick_value = accum;
      }
      frame_index = frame_index + 1;
    } while (frame_index < frame_count);
  }

  if (damage_time_out != 0) {
    *(float *)damage_time_out = accum;
  }
  if (tick_out != 0) {
    *(float *)tick_out = tick_value;
  }
}

/* animation_set_frame_size (0x120790) — Compute and store the per-frame byte
 * stride for a compressed animation from its per-node data-presence flags.
 *
 * If animation is NULL, calls display_assert("animation", ...) then
 * system_exit(-1) (does not return). Otherwise, if animation->node_count
 * (int16 at +0x2c) is positive, walks node_count bits, indexed by
 * word = bit_index >> 5 across three per-node flag bitmaps stored as 32-bit
 * words at +0x6c, +0x5c, +0x7c: each set bit in the +0x6c bitmap adds 8
 * (compressed rotation), each set bit in +0x5c adds 12 (compressed
 * translation), each set bit in +0x7c adds 4 (compressed scale). The
 * accumulated total is stored to animation->frame_size (int16 at +0x24).
 *
 * Confirmed: cdecl, 1 arg (animation ptr) via [EBP+0x8] (ESI).
 * Confirmed: NULL check at 0x12079c (TEST ESI,ESI / JNZ); CALL display_assert
 * ("animation","c:\halo\SOURCE\models\model_animations.c",0x7b,true) at
 * 0x1207ac, then CALL system_exit(-1) at 0x1207b3 (no return).
 * Confirmed: loop guard is signed `0 < *(short *)(anim+0x2c)` at 0x1207bb-
 * 0x1207c0; loop count MOVZX'd (unsigned) into EBX at 0x1207c3.
 * Confirmed: accumulator (EDI) is a 32-bit register, zeroed once in the
 * prologue (XOR EDI,EDI at 0x120798) before the NULL check, added to via
 * 32-bit ADD (0x1207e7/0x1207f0/0x1207f9), and only narrowed to int16 at the
 * final store (MOV word ptr [ESI+0x24],DI at 0x120801).
 * Confirmed: per-iteration bit test order in disassembly is +0x6c (+8), then
 * +0x5c (+0xc), then +0x7c (+4) (0x1207e1-0x1207f9).
 */
void animation_set_frame_size(void *animation)
{
  char *anim;
  int frame_size;
  int node_count;
  int i;
  int word_index;
  unsigned int bit;

  anim = (char *)animation;
  frame_size = 0;

  if (anim == NULL) {
    display_assert("animation", "c:\\halo\\SOURCE\\models\\model_animations.c",
                   0x7b, 1);
    system_exit(-1);
  }

  if (0 < ((animation_t *)anim)->field_2c) {
    node_count = (int)*(unsigned short *)(anim + 0x2c);
    i = 0;
    do {
      bit = 1u << (i & 0x1f);
      word_index = i >> 5;
      if ((*(unsigned int *)(anim + 0x6c + word_index * 4) & bit) != 0) {
        frame_size = frame_size + 8;
      }
      if ((*(unsigned int *)(anim + 0x5c + word_index * 4) & bit) != 0) {
        frame_size = frame_size + 0xc;
      }
      if ((*(unsigned int *)(anim + 0x7c + word_index * 4) & bit) != 0) {
        frame_size = frame_size + 4;
      }
      i = i + 1;
      node_count = node_count - 1;
    } while (node_count != 0);
  }

  ((animation_t *)anim)->frame_size = (short)frame_size;
}

/* quaternion_decompress_8byte (0x120810) — Convert 4 packed int16 values to
 * normalized floats.
 *
 * Reads 4 consecutive short values from src and writes 4 floats to dest,
 * each multiplied by (1.0f / 32767.0f) to normalize from [-32767,32767]
 * to approximately [-1.0, 1.0]. Used to decompress quaternion rotation
 * components stored as 16-bit integers in animation frame data.
 *
 * Confirmed: cdecl, 2 args (src shorts ptr, dest floats ptr).
 * Confirmed: Leaf function, no callees.
 * Confirmed: Multiplies by float constant at 0x290dd8 = 1.0f/32767.0f.
 * Confirmed: 4 iterations via MOVSX+FILD+FMUL+FSTP pattern in disassembly.
 */
void quaternion_decompress_8byte(short *src, float *dest)
{
  dest[0] = (float)(int)src[0] * (1.0f / 32767.0f);
  dest[1] = (float)(int)src[1] * (1.0f / 32767.0f);
  dest[2] = (float)(int)src[2] * (1.0f / 32767.0f);
  dest[3] = (float)(int)src[3] * (1.0f / 32767.0f);
}

/* quaternion_decompress_6byte (0x120870) — Decompress 3 packed uint16s into 4
 * normalized floats.
 *
 * Extracts 4 values from 3 consecutive unsigned shorts (48 bits total) by
 * interleaved bit manipulation, sign-extends each to int, converts to float,
 * and multiplies by (1.0f / 32767.0f). Used to decompress compressed
 * quaternion rotation data in animation frames (compressed path in
 * FUN_00121d60).
 *
 * Confirmed: cdecl, 2 args (compressed_data ushort ptr, dest floats ptr).
 * Confirmed: Leaf function, no callees.
 * Confirmed: Multiplies by float constant at 0x290dd8 = 1.0f/32767.0f.
 * Confirmed: 4 outputs via interleaved bit extraction + MOVSX + FILD + FMUL +
 * FSTP. Confirmed: Bit operations verified against disassembly at
 * 0x120870-0x12092a.
 */
void quaternion_decompress_6byte(void *compressed_data, float *dest)
{
  unsigned short *src;
  unsigned short w0, w1, w2;
  short s0, s1, s2, s3;

  src = (unsigned short *)compressed_data;
  w0 = src[0];
  w1 = src[1];
  w2 = src[2];

  s0 = (short)((w0 >> 12) | (w0 & 0xFFF0));
  s1 = (short)(((w1 >> 4) & 0xFF0) | (w0 & 0xF) | (w0 << 12));
  s2 = (short)((((w2 >> 4) & 0xF00) | (w1 & 0xF0)) >> 4 | (w1 << 8));
  s3 = (short)(((w2 >> 8) & 0xF) | (w2 << 4));

  dest[0] = (float)(int)s0 * (1.0f / 32767.0f);
  dest[1] = (float)(int)s1 * (1.0f / 32767.0f);
  dest[2] = (float)(int)s2 * (1.0f / 32767.0f);
  dest[3] = (float)(int)s3 * (1.0f / 32767.0f);
}

/* quaternion_decompress_6byte_renormalized (0x120930) — Decompress 3 packed
 * uint16s into 4 floats via quaternion_decompress_6byte, then renormalize
 * dest in place via sphere_intersects_rectangle3d (misnamed at 0x10ca30 —
 * see its kb.json decl `float *quaternion`; it is the same renormalize call
 * inlined right after quaternion_decompress_6byte at 0x121e89/0x121e8f in
 * FUN_00121d60).
 *
 * Confirmed: cdecl, 2 args (compressed_data ptr, dest float ptr), void
 * return, leaf wrapper (no locals beyond saved ESI).
 * Confirmed: CALL quaternion_decompress_6byte at 0x12093c — PUSH ESI(dest)
 * then PUSH EAX(compressed_data), matching the callee's (compressed_data,
 * dest) parameter order.
 * Confirmed: CALL sphere_intersects_rectangle3d at 0x120942 — PUSH ESI(dest).
 * The single ADD ESP,0xc at 0x120947 is the combined cleanup for both calls
 * (8 bytes + 4 bytes), not a 3-arg call.
 */
void quaternion_decompress_6byte_renormalized(void *compressed_data,
                                              float *dest)
{
  quaternion_decompress_6byte(compressed_data, dest);
  sphere_intersects_rectangle3d(dest);
}

/* quaternion_compress_8byte (0x120950) — Compress 4 quaternion floats into 4
 * packed int16 values.
 *
 * Inverse of quaternion_decompress_8byte: each component is scaled by
 * 32767.0f and truncated to int (_ftol2), then stored as a 16-bit word.
 *
 * Confirmed: cdecl, 2 args ([EBP+8] quaternion float ptr in ESI, [EBP+0xc]
 * output ptr in EDI), void return, RET with no immediate.
 * Confirmed: FLD [ESI+0/4/8/0xc] * float at 0x26a600 (= 32767.0f, the same
 * constant used by quaternion_compress_6byte) then CALL _ftol2 (0x1d9068)
 * per component, in order 0,1,2,3.
 * Confirmed: stores are MOV word ptr [EDI+0/2/4/6],AX.
 */
void quaternion_compress_8byte(float *quaternion, short *compressed_data)
{
  compressed_data[0] = (short)(int)(quaternion[0] * 32767.0f);
  compressed_data[1] = (short)(int)(quaternion[1] * 32767.0f);
  compressed_data[2] = (short)(int)(quaternion[2] * 32767.0f);
  compressed_data[3] = (short)(int)(quaternion[3] * 32767.0f);
}

/* quaternion_compress_6byte (0x1209b0) — Compress 4 quaternion floats into 3
 * packed uint16s (48 bits, 12 bits per component).
 *
 * Inverse of quaternion_decompress_6byte: each component is scaled by
 * 32767.0f, truncated to int (_ftol2), and the top 12 bits of each 16-bit
 * result are interleaved across the 3 output words.
 *
 * Confirmed: cdecl, 2 args ([EBP+8] quaternion float ptr in EDI, [EBP+0xc]
 * output ushort ptr), void return, RET with no immediate.
 * Confirmed: float constant at 0x26a600 = 32767.0f (same constant clamped
 * against in decals.c / rasterizer.c).
 * Confirmed: conversion order is q[1], q[2], q[3], q[0] — the results land in
 * ESI, EBX, [EBP+8] and EAX respectively (0x1209b9-0x1209f2).
 * Confirmed: only callee is _ftol2 (0x1d9068), written as an (int) cast.
 * Confirmed: packing verified against 0x1209f7-0x120a33 — all three stores
 * are 16-bit (MOV word ptr [ECX+0/2/4]), so the wider 32-bit intermediates
 * (SHL ESI,4 / OR EDX,EAX) are truncated.
 */
void quaternion_compress_6byte(float *quaternion,
                               unsigned short *compressed_data)
{
  unsigned short v1, v2, v3, v0;

  v1 = (unsigned short)(int)(quaternion[1] * 32767.0f);
  v2 = (unsigned short)(int)(quaternion[2] * 32767.0f);
  v3 = (unsigned short)(int)(quaternion[3] * 32767.0f);
  v0 = (unsigned short)(int)(quaternion[0] * 32767.0f);

  compressed_data[0] = (unsigned short)((v0 & 0xFFF0) | (v1 >> 12));
  compressed_data[1] = (unsigned short)(((v1 & 0xFFF0) << 4) | (v2 >> 8));
  compressed_data[2] = (unsigned short)(((v2 & 0xF0) << 8) | (v3 >> 4));
}

/* animation_graph_node_matrices_from_orientations (0x120a40) — Compose the
 * per-node world matrices of an 'antr' animation graph's node hierarchy from a
 * per-node local orientation array and a root (forward/up/position) basis.
 *
 * Breadth-first walk of the node tree held in the tag_block at antr_tag+0x68
 * (element stride 0x40). A 64-entry int16 work queue holds node indices;
 * read_index consumes, write_index appends first-child (+0x20) and
 * next-sibling (+0x22) links, both terminated by -1. The parent matrix of a
 * non-root node comes from node_matrices[node->field_24] (stride 0x34); node 0
 * uses the root basis built by matrix4x3_from_forward_up_position.
 *
 * Confirmed: cdecl, 6 stack args at [EBP+8..+0x1c]; frame SUB ESP,0xf4 =
 *            64*2 queue + 13*4 base matrix + 13*4 scratch matrix + 3 dwords.
 * Confirmed: CALL 0x1ba140 tag_get(0x616e7472, [EBP+8]) — PUSH EAX then
 *            PUSH 0x616e7472; PUSH ESI at 0x120a4c is a register save, not an
 *            argument, and ADD ESP,0x18 at 0x120a77 is the COMBINED cleanup
 *            for tag_get (8) + matrix4x3_from_forward_up_position (16).
 * Confirmed: CALL 0x10a110 argument order from push order
 *            (0x120a5e..0x120a69): [EBP+0x1c], [EBP+0x18], [EBP+0x14],
 *            LEA [EBP-0x74] — so out=base_matrix, position=[EBP+0x14],
 *            forward=[EBP+0x18], up=[EBP+0x1c] per the kb.json decl.
 * Confirmed: CALL 0x109500 (rotation_matrix, node_orientations + index*0x20);
 *            CALL 0x109850 matrix4x3_multiply pushes ESI(node_matrices +
 *            index*0x34), EDX(rotation_matrix), EDI(parent_matrix), so
 *            a=parent_matrix, b=rotation_matrix, out=node_matrices[index].
 *            ADD ESP,0x14 at 0x120b01 is the COMBINED cleanup for both (8+12).
 * Confirmed: node index is int16 and is sign-extended (MOVSX ESI,DI at
 *            0x120aab) before both the *0x20 and *0x34 scalings.
 * Confirmed: -1 sentinel lives in DI (OR EDI,0xffffffff at 0x120afe) and is
 *            also the value pushed to system_exit on the assert paths.
 * Confirmed: loop latch is CMP word ptr [EBP-0x8],SI at 0x120b80 — a 16-bit
 *            compare of read_index against write_index.
 * Confirmed: asserts at 0x120b21 (line 0x4e2) and 0x120b5f (line 0x4e8),
 *            both "write_index<MAXIMUM_NODES_PER_MODEL" with halt=true.
 */
void animation_graph_node_matrices_from_orientations(
  int animation_graph_tag_index, float *node_matrices, float *node_orientations,
  float *position, float *forward, float *up)
{
  short node_indices[64];
  float base_matrix[13];
  float rotation_matrix[13];
  int *node_block;
  int read_index;
  int write_index;
  char *antr_tag;
  char *node;
  short node_index;
  float *parent_matrix;

  antr_tag = (char *)tag_get(0x616e7472, animation_graph_tag_index);
  matrix4x3_from_forward_up_position(base_matrix, position, forward, up);
  read_index = 0;
  node_block = (int *)(antr_tag + 0x68);
  if (*node_block > 0) {
    write_index = 1;
    node_indices[0] = 0;
    do {
      node_index = node_indices[(short)read_index];
      read_index = read_index + 1;
      node = (char *)tag_block_get_element(node_block, (int)node_index, 0x40);
      if (node_index == 0) {
        parent_matrix = base_matrix;
      } else {
        parent_matrix =
          (float *)((char *)node_matrices + *(short *)(node + 0x24) * 0x34);
      }
      FUN_00109500(rotation_matrix, (float *)((char *)node_orientations +
                                              (int)node_index * 0x20));
      matrix4x3_multiply(
        parent_matrix, rotation_matrix,
        (float *)((char *)node_matrices + (int)node_index * 0x34));
      if (*(short *)(node + 0x20) != -1) {
        if ((short)write_index >= 0x40) {
          display_assert("write_index<MAXIMUM_NODES_PER_MODEL",
                         "c:\\halo\\SOURCE\\models\\model_animations.c", 0x4e2,
                         1);
          system_exit(-1);
        }
        node_indices[(short)write_index] = *(short *)(node + 0x20);
        write_index = write_index + 1;
      }
      if (*(short *)(node + 0x22) != -1) {
        if ((short)write_index >= 0x40) {
          display_assert("write_index<MAXIMUM_NODES_PER_MODEL",
                         "c:\\halo\\SOURCE\\models\\model_animations.c", 0x4e8,
                         1);
          system_exit(-1);
        }
        node_indices[(short)write_index] = *(short *)(node + 0x22);
        write_index = write_index + 1;
      }
    } while ((short)read_index != (short)write_index);
  }
}

/* interpolate_node_orientations (0x120ba0) -- blend each node's orientation
 * from original toward target in place: fraction = (frame_index+1)/frame_count.
 * Node records are 0x20 bytes: +0x00 rotation quaternion, +0x10 translation,
 * +0x1c scale (layout from the loop's FMUL/FSTP offsets; same stride as
 * overlay_animation_apply).  Asserts are model_animations.c 0x4fd/0x4fe with
 * the XBE's own strings.  quaternions_interpolate_and_normalize (0x10cb60) is
 * called (original, target, fraction, target) -- pushes at 0x120c46..0x120c50.
 * Shape: PAL 2342 model_animations.c interpolate_node_orientations (T2). */
void interpolate_node_orientations(int16_t node_count,
                                   void *original_node_orientations,
                                   void *target_node_orientations,
                                   int16_t frame_index, int16_t frame_count)
{
  real fraction;
  real inverse_fraction;
  int16_t node_index;

  fraction = (real)(frame_index + 1) / (real)frame_count;
  inverse_fraction = *(float *)0x2533c8 - fraction;
  assert_halt_msg_at("frame_count>0",
                     "c:\\halo\\SOURCE\\models\\model_animations.c", 0x4fd,
                     frame_count > 0);
  assert_halt_msg_at("frame_index<frame_count",
                     "c:\\halo\\SOURCE\\models\\model_animations.c", 0x4fe,
                     frame_index < frame_count);

  for (node_index = 0; node_index < node_count; node_index++) {
    float *target = (float *)target_node_orientations + node_index * 8;
    float *original = (float *)original_node_orientations + node_index * 8;

    target[7] = inverse_fraction * original[7] + fraction * target[7];
    quaternions_interpolate_and_normalize(original, target, fraction, target);
    target[4] = inverse_fraction * original[4] + fraction * target[4];
    target[5] = inverse_fraction * original[5] + fraction * target[5];
    target[6] = inverse_fraction * original[6] + fraction * target[6];
  }
}

/* animation_graph_get_animation_by_name (0x120cb0) — Look up an animation by
 * name in an 'antr' (model_animations) tag's animation block.
 *
 * Walks the tag_block at antr+0x74 (element stride 0xb4) comparing `name`
 * case-insensitively against each element's name field at element+0x0, and
 * returns the matching int16 animation index, or -1 if no element matches.
 *
 * Confirmed: CALL 0x1ba140 tag_get(0x616e7472, animation_graph_tag_index)
 *            — PUSH EAX([EBP+8]) then PUSH 0x616e7472, ADD ESP,8.
 * Confirmed: ESI = tag_get result + 0x74 (the animation tag_block).
 * Confirmed: CALL 0x19b210 tag_block_get_element(ESI, MOVSX(DI), 0xb4)
 *            — pushes 0xb4, EAX(index), ESI(block).
 * Confirmed: CALL 0x1dd801 crt_stricmp(EBX([EBP+0xc]) = name, EAX = element).
 *            The single ADD ESP,0x14 at 0x120cea is the COMBINED cleanup for
 *            tag_block_get_element (12) + crt_stricmp (8) — not a 5-arg call.
 * Confirmed: counter lives in DI and is widened with MOVSX EAX,DI before both
 *            the element fetch and the loop compare — int16 semantics.
 * Confirmed: the block count is re-read from memory each iteration
 *            (MOV ECX,[ESI] at 0x120cf1); it is not hoisted into a register.
 * Confirmed: not-found path is OR AX,0xffff (-1); found path is MOV AX,DI.
 */
short animation_graph_get_animation_by_name(int animation_graph_tag_index,
                                            const char *name)
{
  char *antr_tag;
  int *animation_block;
  short animation_index;
  char *element;

  antr_tag = (char *)tag_get(0x616e7472, animation_graph_tag_index);
  animation_block = (int *)(antr_tag + 0x74);
  animation_index = 0;
  if (*animation_block > 0) {
    do {
      element = (char *)tag_block_get_element(animation_block,
                                              (int)animation_index, 0xb4);
      if (crt_stricmp(name, element) == 0) {
        return animation_index;
      }
      animation_index = animation_index + 1;
    } while ((int)animation_index < *animation_block);
  }
  return -1;
}

/* find_keyframe_index (0x120d10) — Binary search for a keyframe by frame index.
 *
 * Given a sorted array of keyframe frame indices and a target frame, returns
 * the keyframe index i such that:
 *   keyframe_frame_indices[i] <= target_frame_index <
 * keyframe_frame_indices[i+1]
 *
 * Uses binary search with lo/hi bounds. The keyframe_count parameter is passed
 * in EDI (register arg @<edi>).
 *
 * Asserts:
 *   keyframe_count > 1
 *   keyframe_frame_indices != NULL
 *   keyframe_frame_indices[0] > 0
 *   target_frame_index >= 0 && target_frame_index <
 * keyframe_frame_indices[keyframe_count-1] Infinite loop killer at 200
 * iterations Post-search: keyframe_index >= 0 && keyframe_index <
 * keyframe_count-1 Post-search: target in range [keyframe_frame_indices[i],
 * keyframe_frame_indices[i+1])
 *
 * Confirmed: keyframe_count@<edi> register arg per disassembly.
 * Confirmed: Binary search with lo=local_8, hi=keyframe_count-1.
 * Confirmed: RETURNS uint masked to 0xffff (MOVZX EAX,AX pattern at return).
 */
short FUN_00120d10(unsigned short *keyframe_frame_indices,
                   short target_frame_index, short keyframe_count)
{
  int lo;
  int hi;
  int mid;
  short kfc;
  int kf_idx;
  int i;

  kfc = keyframe_count;
  if (kfc < 2) {
    display_assert("keyframe_count>1",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x536, 1);
    system_exit(-1);
  }
  if (keyframe_frame_indices == (void *)0) {
    display_assert("keyframe_frame_indices",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x537, 1);
    system_exit(-1);
  }
  if ((short)keyframe_frame_indices[0] < 1) {
    display_assert("keyframe_frame_indices[0]>0",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x538, 1);
    system_exit(-1);
  }
  if (target_frame_index < 0 ||
      (short)keyframe_frame_indices[kfc - 1] <= target_frame_index) {
    display_assert(
      "target_frame_index>=0 && "
      "target_frame_index<keyframe_frame_indices[keyframe_count-1]",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x539, 1);
    system_exit(-1);
  }

  lo = 0;
  hi = kfc - 1;
  i = 0;
  while (1) {
    mid = lo + hi;
    mid = mid >> 1;
    kf_idx = mid;

    if ((short)kf_idx < 0 || kfc <= (short)kf_idx) {
      display_assert("keyframe_index>=0 && keyframe_index<keyframe_count",
                     "c:\\halo\\SOURCE\\models\\model_animations.c", 0x53f, 1);
      system_exit(-1);
    }

    if ((short)(kf_idx + 1) >= kfc ||
        target_frame_index < (short)keyframe_frame_indices[kf_idx + 1]) {
      if (target_frame_index >= (short)keyframe_frame_indices[kf_idx]) {
        break;
      }
      hi = mid;
    } else {
      lo = mid;
    }

    i++;
    if (i > 199) {
      display_assert("++infinite_loop_killer<200",
                     "c:\\halo\\SOURCE\\models\\model_animations.c", 0x54c, 1);
      system_exit(-1);
    }
  }

  if ((short)kf_idx < 0 || kfc - 1 <= (short)kf_idx) {
    display_assert("keyframe_index>=0 && keyframe_index<keyframe_count-1",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x550, 1);
    system_exit(-1);
  }
  if (target_frame_index < (short)keyframe_frame_indices[kf_idx] ||
      (short)keyframe_frame_indices[kf_idx + 1] <= target_frame_index) {
    display_assert(
      "target_frame_index>=keyframe_frame_indices[keyframe_index] && "
      "target_frame_index<keyframe_frame_indices[keyframe_index+1]",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x551, 1);
    system_exit(-1);
  }

  return (short)kf_idx;
}

/* animation_frame_get_xy_translation (0x120ee0) — Fetch a frame's XY
 * translation offset, for animation types whose translation is applied via
 * this path (discriminator == 1), else return (0, 0).
 *
 * If *(short *)(animation+0x26) == 1, resolves the frame's raw data block
 * via FUN_00120590(animation, frame_index, 8) and copies the block's first
 * two dwords (x, y) into *out_translation. FUN_00120590 returns that pointer
 * in EAX (see its confirmed void-EAX-return note above); the two MOVs here
 * are a straight dword bit-copy, not FPU math, matching the original's plain
 * MOV/MOV pair. Otherwise zero-fills *out_translation.
 *
 * Confirmed: cdecl, 3 args (animation ptr [EBP+8], frame_index short
 * [EBP+0xc], out_translation float* [EBP+0x10]). Confirmed: CMP word ptr
 * [EAX+0x26],0x1 / JNZ 0x00120f0b at 0x120ee6-0x120eeb. Confirmed: frame_size
 * literal 8 pushed at 0x120ef0 (PUSH 0x8). Confirmed: taken path copies
 * dword [ret_ptr] -> out[0] and dword [ret_ptr+4] -> out[1] at
 * 0x120ef9-0x120f06. Confirmed: not-taken path stores dword 0 to both output
 * slots at 0x120f0e and 0x120f14.
 */
void animation_frame_get_xy_translation(void *animation, short frame_index,
                                        float *out_translation)
{
  char *anim = (char *)animation;
  void *frame_data;

  if (((animation_t *)anim)->field_26 == 1) {
    frame_data = FUN_00120590(animation, frame_index, 8);
    out_translation[0] = *(float *)frame_data;
    out_translation[1] = *(float *)((char *)frame_data + 4);
    return;
  }
  out_translation[0] = 0.0f;
  out_translation[1] = 0.0f;
}

/* model_animation_choose_random (0x120f20) — Choose a weighted random
 * animation.
 *
 * Gets the animation graph tag ('antr'), generates a random float [0,1) using
 * either the global or local random seed (based on update_kind), then walks the
 * animation chain starting at animation_index. For each animation element:
 * compares the random value against the weight threshold at element+0x44.
 * If random <= threshold, returns the current animation index. Otherwise
 * advances to the next animation via element+0x38.
 *
 * Confirmed: tag_get('antr', animation_graph_tag_index) at 0x120f2e.
 * Confirmed: update_kind==1 → get_global_random_seed_address() at 0x120f40.
 * Confirmed: update_kind==0 → random_math_get_local_seed_address() at 0x120f53.
 * Confirmed: random_math_real(seed) at 0x120f46/0x120f59.
 * Confirmed: tag_block_get_element(antr_tag+0x74, index, 0xb4) at 0x120f9e.
 * Confirmed: FCOMP [ECX+0x44] + JNP loop exit at 0x120fab-0x120fb3.
 * Confirmed: next animation at element+0x38 (int16_t) at 0x120fb5.
 * Confirmed: returns the signed 16-bit index in AX at 0x120fc0.
 */
int16_t model_animation_choose_random(int update_kind,
                                      int animation_graph_tag_index,
                                      int16_t animation_index)
{
  char *antr_tag;
  float random_value;
  char *element;

#ifdef HALO_RNG_TRACE
  if (update_kind == 1)
    RNG_TRACE_EX(RNG_TRACE_KIND_ANIM_CHOOSE,
                 (unsigned int)(unsigned short)animation_index,
                 __builtin_return_address(0));
#endif
#line 862
  antr_tag = (char *)tag_get(0x616e7472, animation_graph_tag_index);
  if (update_kind == 1) {
    random_value =
      random_math_real((unsigned int *)get_global_random_seed_address());
  } else {
    random_value = random_math_real(random_math_get_local_seed_address());
    if (update_kind != 0) {
      display_assert("(animation_update_kind_affects_game_state==render_or_affects_game_state) || (animation_update_kind_render_only==render_or_affects_game_state)",
                     "c:\\halo\\SOURCE\\models\\model_animations.c", 0x3f0, 1);
      system_exit(-1);
    }
  }
  while (animation_index != -1) {
    element = (char *)tag_block_get_element(antr_tag + 0x74,
                                            (int)animation_index, 0xb4);
    if (random_value <= *(float *)(element + 0x44))
      break;
    animation_index = *(int16_t *)(element + 0x38);
  }
  return animation_index;
}

/* |p1 - p0|; the IK solve below inlines this three times (FSQRT, then the
 * three x87-resident deltas are popped). */
/* Stores (x, y, z); VC71 evaluates the arguments right to left, so the IK
 * basis rebuild computes z first and keeps all three on the x87 stack. */
static __inline void anim_set_vector3d(vector3_t *v, float x, float y, float z)
{
  v->x = x;
  v->y = y;
  v->z = z;
}

static __inline float anim_distance3d(const vector3_t *p0, const vector3_t *p1)
{
  vector3_t delta;

  delta.x = p1->x - p0->x;
  delta.y = p1->y - p0->y;
  delta.z = p1->z - p0->z;
  return (float)x87_sqrtd(delta.x * delta.x + delta.y * delta.y +
                          delta.z * delta.z);
}

/* inverse_kinematics_adjust_matrices (0x120fd0) — Two-bone IK solve.
 *
 * Confirmed (disassembly): cdecl, 4 stack args, void return. All four args are
 * real_matrix4x3 pointers (0x34 bytes: rep movsd ECX=0xd at 0x121316); the
 * existing caller (objects.c) passes them as ints, so the kb decl keeps `int`.
 *   b = node_matrix_b (EBX), c = node_matrix_c (EDI), d = node_matrix_d.
 * Confirmed: len_bc = |c.pos - b.pos|, len_cd = |d.pos - c.pos|,
 *   len_ab = |b.pos - composed.pos|; axis = (composed.pos - b.pos) * (1/len_ab)
 *   (1.0f at 0x2533c8); bend = normalize(cross(axis, c.pos - b.pos)).
 * Confirmed: reach = (len_cd + len_bc) * 0.98f (0x291060); when reach < len_ab
 *   (FCOM + TEST AH,5 + JP) the target is pulled in to b.pos + axis * reach.
 * Confirmed: law-of-cosines split x = (len_ab^2 + len_bc^2 - len_cd^2) /
 *   (2 * len_ab), h = sqrt(len_bc^2 - x^2); b.forward = axis*x + perp*h,
 *   c.forward = axis*(len_ab - x) - perp*h; both bases re-orthonormalized with
 *   normalize3d (return value discarded, FSTP ST0); c.position = b.pos +
 *   b.forward * len_bc; finally *d = *composed.
 * Inferred: basis cross products are passed to anim_set_vector3d; VC71
 *   picks the FLD/FMUL operand order, which source order only partly steers.
 * Dormant (ported:false) body for the standalone build.
 */
void inverse_kinematics_adjust_matrices(float *composed_matrix,
                                        int node_matrix_b, int node_matrix_c,
                                        int node_matrix_d)
{
  real_matrix4x3 *composed = (real_matrix4x3 *)composed_matrix;
  real_matrix4x3 *b = (real_matrix4x3 *)node_matrix_b;
  real_matrix4x3 *c = (real_matrix4x3 *)node_matrix_c;
  real_matrix4x3 *d = (real_matrix4x3 *)node_matrix_d;
  vector3_t *c_position = &c->position;
  float len_bc, len_cd, len_ab;
  float inv_len_ab, reach, split, height;
  vector3_t *b_forward, *b_left, *b_up;
  vector3_t *c_forward, *c_left, *c_up;
  vector3_t bc_dir;
  vector3_t axis, bend, perp, offset, new_position;

  len_bc = anim_distance3d(&b->position, c_position);
  len_cd = anim_distance3d(c_position, &d->position);
  len_ab = anim_distance3d(&composed->position, &b->position);

  bc_dir.x = c_position->x - b->position.x;
  bc_dir.y = c->position.y - b->position.y;
  bc_dir.z = c->position.z - b->position.z;
  inv_len_ab = 1.0f / len_ab;
  axis.x = (composed->position.x - b->position.x) * inv_len_ab;
  axis.y = (composed->position.y - b->position.y) * inv_len_ab;
  axis.z = (composed->position.z - b->position.z) * inv_len_ab;
  bend.x = axis.y * bc_dir.z - axis.z * bc_dir.y;
  bend.y = axis.z * bc_dir.x - axis.x * bc_dir.z;
  bend.z = axis.x * bc_dir.y - axis.y * bc_dir.x;
  normalize3d((float *)&bend);
  perp.x = bend.y * axis.z - bend.z * axis.y;
  perp.y = bend.z * axis.x - bend.x * axis.z;
  perp.z = bend.x * axis.y - bend.y * axis.x;

  reach = (len_cd + len_bc) * 0.98f;
  if (reach < len_ab) {
    composed->position.x = axis.x * reach + b->position.x;
    composed->position.y = axis.y * reach + b->position.y;
    composed->position.z = axis.z * reach + b->position.z;
    len_ab = reach;
  }

  split = (len_ab * len_ab + len_bc * len_bc - len_cd * len_cd) /
          (len_ab + len_ab);
  len_ab = len_ab - split;
  height = (float)x87_sqrtd(len_bc * len_bc - split * split);
  b_forward = &b->forward;
  b_left = &b->left;
  b_up = &b->up;
  offset.x = perp.x * height;
  b_forward->x = split * axis.x + offset.x;
  offset.y = perp.y * height;
  b_forward->y = split * axis.y + offset.y;
  offset.z = perp.z * height;
  b_forward->z = split * axis.z + offset.z;
  normalize3d((float *)b_forward);

  anim_set_vector3d(b_up,
                    b_left->z * b_forward->y - b_left->y * b_forward->z,
                    b_left->x * b_forward->z - b_left->z * b_forward->x,
                    b_left->y * b_forward->x - b_forward->y * b_left->x);
  normalize3d((float *)b_up);
  anim_set_vector3d(b_left,
                    b_up->y * b_forward->z - b_up->z * b_forward->y,
                    b_up->z * b_forward->x - b_up->x * b_forward->z,
                    b_forward->y * b_up->x - b_forward->x * b_up->y);

  new_position.x = len_bc * b_forward->x + b->position.x;
  new_position.y = len_bc * b_forward->y + b->position.y;
  new_position.z = len_bc * b_forward->z + b->position.z;

  c_forward = &c->forward;
  c_left = &c->left;
  c_up = &c->up;
  c_forward->x = len_ab * axis.x - offset.x;
  c_forward->y = len_ab * axis.y - offset.y;
  c_forward->z = len_ab * axis.z - offset.z;
  normalize3d((float *)c_forward);

  anim_set_vector3d(c_up,
                    c_forward->y * c_left->z - c_forward->z * c_left->y,
                    c_forward->z * c_left->x - c_forward->x * c_left->z,
                    c_forward->x * c_left->y - c_forward->y * c_left->x);
  normalize3d((float *)c_up);
  anim_set_vector3d(c_left,
                    c_forward->z * c_up->y - c_forward->y * c_up->z,
                    c_forward->x * c_up->z - c_forward->z * c_up->x,
                    c_forward->y * c_up->x - c_forward->x * c_up->y);

  *c_position = new_position;
  *d = *composed;
}

/* floor: the original calls MSVC CRT floor (0x1d9c2b).
 * We provide a local implementation since we don't link the CRT math lib. */
static __declspec(noinline) double anim_floor(double x)
{
  int i = (int)x;
  return (double)((x < (double)i) ? (i - 1) : i);
}

/* animation_get_node_orientations (0x121640) — Interpolate keyframed
 * translation data for a single node in a compressed animation.
 *
 * Given an animation structure, a fractional frame index, a translation
 * keyframe count, a node index, and an output buffer, this function resolves
 * the two bracketing keyframes and either copies the exact keyframe data or
 * interpolates between them using points_interpolate (vec3 lerp).
 *
 * The animation's tag_data (at animation+0xa0) contains:
 *   +0x0c: offset to a per-component packed descriptor array (4 bytes each,
 *          low 12 bits = keyframe_count, high 4 bits = data_offset_index).
 *   +0x10: offset to keyframe_frame_indices (unsigned short array).
 *   +0x14: offset to default_translations (vec3 array, 12 bytes per node).
 *   +0x18: offset to keyframe_data (vec3 array, 12 bytes per keyframe).
 *
 * Three branches for the frame position:
 *   1. Before the first keyframe: interpolate between default_translation[node]
 *      and keyframe_data[0], with kf0_frame=0 and
 * kf1_frame=first_keyframe_frame.
 *   2. At the last keyframe: interpolate between keyframe_data[last] and
 *      default_translation[node], with kf0_frame=last_frame and
 * kf1_frame=last+1.
 *   3. Between two keyframes: binary-search via FUN_00120d10, then interpolate
 *      between the two bracketing keyframe entries.
 *
 * If frame == kf0_frame exactly, copies this_kf_data directly (no blend).
 *
 * Confirmed: cdecl, 5 args, void return.
 * Confirmed: CALL tag_data_get_pointer(animation+0xa0, *(int*)(animation+0x88),
 * 0) at 0x12165b. Confirmed: CALL floor() at 0x12175f (CRT 0x1d9c2b), result
 * truncated to short frame_index. Confirmed: CALL
 * FUN_00120d10(keyframe_frame_indices, frame_index) at 0x12182d with
 * keyframe_count in EDI. Confirmed: CALL points_interpolate(this_kf_data,
 * next_kf_data, blend, out) at 0x121928. Confirmed: Assert strings at 0x5f2,
 * 0x5f4, 0x609, 0x60a, 0x61e, 0x62f, 0x630. Confirmed: Before-first-keyframe
 * branch saves kf0_frame to kf1_frame before zeroing (MOV EBX,EDX at 0x1217fa).
 */
void animation_get_node_orientations(void *animation, float frame,
                                     unsigned short translation_count,
                                     short node_index, void *out_translation)
{
  char *anim;
  char *tag_data_base;
  unsigned int descriptor;
  unsigned short keyframe_count;
  int data_offset_index;
  char *default_translations;
  char *keyframe_data;
  unsigned short *keyframe_frame_indices;
  int frame_count_i;
  float frame_floor_f;
  short frame_index;
  int kf_count_i;
  unsigned short kf0_frame;
  unsigned short kf1_frame;
  char *this_kf_data;
  char *next_kf_data;
  short kf_idx;
  float this_frame_f;
  float blend;

  anim = (char *)animation;

  /* Resolve tag_data pointer */
  tag_data_base =
    (char *)tag_data_get_pointer(anim + 0xa0, *(int *)(anim + 0x88), 0);

  /* Read packed descriptor for this translation component */
  descriptor =
    *(unsigned int *)(tag_data_base + *(int *)(tag_data_base + 0x0c) +
                      (short)translation_count * 4);
  keyframe_count = (unsigned short)(descriptor & 0xfff);
  data_offset_index = (int)(short)(descriptor >> 0xc);

  /* Default translations and keyframe arrays are relative to tag_data_base */
  default_translations = tag_data_base + *(int *)(tag_data_base + 0x14);
  keyframe_data =
    tag_data_base + *(int *)(tag_data_base + 0x18) + data_offset_index * 0xc;
  keyframe_frame_indices =
    (unsigned short *)(tag_data_base + *(int *)(tag_data_base + 0x10) +
                       data_offset_index * 2);

  /* Assert: real_frame_index >= 0.0f */
  if (frame < 0.0f) {
    display_assert("real_frame_index>=0.0f",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x5f2, 1);
    system_exit(-1);
  }

  /* Assert: real_frame_index < (real)animation->frame_count */
  frame_count_i = (int)((animation_t *)anim)->frame_count;
  if (frame >= (float)frame_count_i) {
    display_assert("real_frame_index<(real)animation->frame_count",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x5f4, 1);
    system_exit(-1);
  }

  /* If keyframe_count == 0, return the default translation for this node */
  if (keyframe_count == 0) {
    char *def = default_translations + (int)node_index * 0xc;
    int *out = (int *)out_translation;
    out[0] = *(int *)(def);
    out[1] = *(int *)(def + 4);
    out[2] = *(int *)(def + 8);
    return;
  }

  /* Compute integer frame index from floor(frame) */
  frame_floor_f = (float)anim_floor((double)frame);
  /* 0x121764: FSTP dword / 0x12176a: FLD dword before FISTP. */
  HALO_FLT_ROUNDTRIP(frame_floor_f);
  frame_index = (short)(int)frame_floor_f;

  /* Assert: frame_index >= 0 && frame_index <=
   * keyframe_frame_indices[keyframe_count-1] */
  kf_count_i = (int)(short)keyframe_count;
  if (frame_index < 0 ||
      (int)frame_index >
        (int)(unsigned int)keyframe_frame_indices[kf_count_i - 1]) {
    display_assert(
      "frame_index>=0 && frame_index<=keyframe_frame_indices[keyframe_count-1]",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x609, 1);
    system_exit(-1);
  }

  /* Assert: keyframe_frame_indices[keyframe_count-1] == animation->frame_count
   * - 1 */
  if ((unsigned int)keyframe_frame_indices[kf_count_i - 1] !=
      (unsigned int)((int)((animation_t *)anim)->frame_count - 1)) {
    display_assert(
      "keyframe_frame_indices[keyframe_count-1]==animation->frame_count-1",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x60a, 1);
    system_exit(-1);
  }

  /* Determine which two keyframes bracket the current frame */
  kf0_frame = keyframe_frame_indices[0];

  if ((int)frame_index < (int)(unsigned int)kf0_frame) {
    /* Before the first keyframe: interpolate default -> first keyframe */
    this_kf_data = default_translations + (int)node_index * 0xc;
    kf1_frame = kf0_frame;
    kf0_frame = 0;
    next_kf_data = keyframe_data;
  } else {
    kf1_frame = keyframe_frame_indices[kf_count_i - 1];

    if ((int)frame_index == (int)(unsigned int)kf1_frame) {
      /* At the last keyframe: interpolate last keyframe -> default */
      this_kf_data = keyframe_data + (kf_count_i - 1) * 0xc;
      kf0_frame = kf1_frame;
      kf1_frame = kf1_frame + 1;
      next_kf_data = default_translations + (int)node_index * 0xc;
    } else {
      /* Between two keyframes: binary search */
      kf_idx = FUN_00120d10(keyframe_frame_indices, (short)(int)frame_floor_f,
                            keyframe_count);

      if (kf_idx < 0 || (int)kf_idx >= kf_count_i - 1) {
        display_assert("keyframe_index>=0 && keyframe_index<keyframe_count-1",
                       "c:\\halo\\SOURCE\\models\\model_animations.c", 0x61e,
                       1);
        system_exit(-1);
      }

      kf0_frame = keyframe_frame_indices[(int)kf_idx];
      kf1_frame = keyframe_frame_indices[(int)kf_idx + 1];
      this_kf_data = keyframe_data + (int)kf_idx * 0xc;
      next_kf_data = this_kf_data + 0xc;
    }
  }

  /* If frame == this_keyframe_frame exactly, copy directly */
  this_frame_f = (float)(int)(short)kf0_frame;
  if (frame == this_frame_f) {
    int *out = (int *)out_translation;
    out[0] = *(int *)(this_kf_data);
    out[1] = *(int *)(this_kf_data + 4);
    out[2] = *(int *)(this_kf_data + 8);
    return;
  }

  /* Compute blend factor and interpolate */
  blend = (frame - this_frame_f) /
          (float)((int)(short)kf1_frame - (int)(short)kf0_frame);

  /* Assert: real_frame_index >= (real)this_keyframe_frame_index */
  if (frame < this_frame_f) {
    display_assert("real_frame_index>=(real)this_keyframe_frame_index",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x62f, 1);
    system_exit(-1);
  }

  /* Assert: real_frame_index < (real)next_keyframe_frame_index */
  if (frame >= (float)(int)(short)kf1_frame) {
    display_assert("real_frame_index< (real)next_keyframe_frame_index",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x630, 1);
    system_exit(-1);
  }

  points_interpolate((float *)this_kf_data, (float *)next_kf_data, blend,
                     (float *)out_translation);
}

/* animation_get_keyframe_scale (0x121940) — Interpolate keyframed
 * scale data for a single node in a compressed animation.
 *
 * Scalar (single-float) sibling of animation_get_node_orientations. Resolves
 * the two bracketing keyframes for a given fractional frame and either copies
 * the exact keyframe scale or interpolates between two scales using
 * scalars_interpolate (scalar lerp).
 *
 * The animation's tag_data (at animation+0xa0) contains:
 *   +0x1c: offset to a per-component packed descriptor array (4 bytes each,
 *          low 12 bits = keyframe_count, high 4 bits = data_offset_index).
 *   +0x20: offset to keyframe_frame_indices (unsigned short array).
 *   +0x24: offset to default_scales (float array, 4 bytes per node).
 *   +0x28: offset to keyframe_data (float array, 4 bytes per keyframe).
 *
 * Branch structure mirrors animation_get_node_orientations:
 *   1. Before first keyframe: lerp default_scale[scale_count] -> keyframe[0]
 *      with kf0_frame=0, kf1_frame=first_keyframe_frame.
 *   2. At last keyframe: lerp keyframe[last] -> default_scale[scale_count]
 *      with kf0_frame=last_frame, kf1_frame=last_frame+1.
 *   3. Between keyframes: binary search via FUN_00120d10, then lerp the two
 *      bracketing keyframe entries.
 * If frame == kf0_frame exactly, copy this_kf_scale directly (no blend).
 *
 * Note: scale_count is a 16-bit selector indexing the descriptor array; it
 * is also used as the index into default_scales (default_scales[scale_count]).
 * After masking the descriptor, the local param_3 slot holds keyframe_count.
 *
 * Confirmed: cdecl, 5 args, void return (stack cleanup ADD ESP,0x10 at
 * 0x121c1c). Confirmed: CALL tag_data_get_pointer(animation+0xa0,
 * *(int*)(animation+0x88), 0) at 0x12195b. Confirmed: CALL anim_floor (CRT
 * 0x1d9c2b) at 0x121a4f via push double + FSTP [ESP]. Confirmed: CALL
 * FUN_00120d10(keyframe_frame_indices=EBX, target_frame=ECX,
 *            keyframe_count@<edi>=[EBP+0x10]) at 0x121b1d.
 * Confirmed: CALL scalars_interpolate(this_kf, next_kf, blend, out) at
 * 0x121c17. Confirmed: Assert lines 0x64a, 0x64c, 0x662, 0x663, 0x677, 0x688,
 * 0x689 (the 0x64e "keyframe_count>=0" assert is dead after the &0xfff mask and
 * was eliminated by the optimizer). Confirmed: Element size 4 bytes (float) —
 * LEA EDX+EAX*0x4 at 0x121a39. Confirmed: Frame indices array stride 2 bytes —
 * LEA ECX+EAX*0x2 at 0x121a44.
 */
void animation_get_keyframe_scale(void *animation, float frame,
                                  unsigned short scale_count, short node_index,
                                  void *out_scale)
{
  char *anim;
  char *tag_data_base;
  unsigned int descriptor;
  unsigned short keyframe_count;
  int data_offset_index;
  char *default_scales;
  char *keyframe_data;
  unsigned short *keyframe_frame_indices;
  int frame_count_i;
  float frame_floor_f;
  short frame_index;
  int kf_count_i;
  unsigned short kf0_frame;
  unsigned short kf1_frame;
  float this_kf_scale;
  float next_kf_scale;
  short kf_idx;
  float this_frame_f;
  float blend;
  (void)node_index;

  anim = (char *)animation;

  /* Resolve tag_data pointer */
  tag_data_base =
    (char *)tag_data_get_pointer(anim + 0xa0, *(int *)(anim + 0x88), 0);

  /* Read packed descriptor for this scale component */
  descriptor =
    *(unsigned int *)(tag_data_base + *(int *)(tag_data_base + 0x1c) +
                      (short)scale_count * 4);
  keyframe_count = (unsigned short)(descriptor & 0xfff);
  data_offset_index = (int)(short)(descriptor >> 0xc);

  /* Default scales and keyframe arrays are relative to tag_data_base */
  default_scales = tag_data_base + *(int *)(tag_data_base + 0x24);
  keyframe_data =
    tag_data_base + *(int *)(tag_data_base + 0x28) + data_offset_index * 4;
  keyframe_frame_indices =
    (unsigned short *)(tag_data_base + *(int *)(tag_data_base + 0x20) +
                       data_offset_index * 2);

  /* Assert: real_frame_index >= 0.0f */
  if (frame < 0.0f) {
    display_assert("real_frame_index>=0.0f",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x64a, 1);
    system_exit(-1);
  }

  /* Assert: real_frame_index < (real)animation->frame_count */
  frame_count_i = (int)((animation_t *)anim)->frame_count;
  if (frame >= (float)frame_count_i) {
    display_assert("real_frame_index<(real)animation->frame_count",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x64c, 1);
    system_exit(-1);
  }

  /* If keyframe_count == 0, return the default scale for this slot */
  if (keyframe_count == 0) {
    *(int *)out_scale = *(int *)(default_scales + (int)(short)scale_count * 4);
    return;
  }

  /* Compute integer frame index from floor(frame) */
  frame_floor_f = (float)anim_floor((double)frame);
  /* 0x121a54: FSTP dword / 0x121a5a: FLD dword before FISTP. */
  HALO_FLT_ROUNDTRIP(frame_floor_f);
  frame_index = (short)(int)frame_floor_f;

  /* Assert: frame_index >= 0 && frame_index <=
   * keyframe_frame_indices[keyframe_count-1] */
  kf_count_i = (int)(short)keyframe_count;
  if (frame_index < 0 ||
      (int)frame_index >
        (int)(unsigned int)keyframe_frame_indices[kf_count_i - 1]) {
    display_assert(
      "frame_index>=0 && frame_index<=keyframe_frame_indices[keyframe_count-1]",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x662, 1);
    system_exit(-1);
  }

  /* Assert: keyframe_frame_indices[keyframe_count-1] == animation->frame_count
   * - 1 */
  if ((unsigned int)keyframe_frame_indices[kf_count_i - 1] !=
      (unsigned int)((int)((animation_t *)anim)->frame_count - 1)) {
    display_assert(
      "keyframe_frame_indices[keyframe_count-1]==animation->frame_count-1",
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x663, 1);
    system_exit(-1);
  }

  /* Determine which two keyframes bracket the current frame */
  kf0_frame = keyframe_frame_indices[0];

  if ((int)frame_index < (int)(unsigned int)kf0_frame) {
    /* Before the first keyframe: interpolate default_scale -> first keyframe */
    this_kf_scale = *(float *)(default_scales + (int)(short)scale_count * 4);
    next_kf_scale = *(float *)keyframe_data;
    kf1_frame = kf0_frame;
    kf0_frame = 0;
  } else {
    kf1_frame = keyframe_frame_indices[kf_count_i - 1];

    if ((int)frame_index == (int)(unsigned int)kf1_frame) {
      /* At the last keyframe: interpolate last keyframe -> default_scale */
      this_kf_scale = *(float *)(keyframe_data + (kf_count_i - 1) * 4);
      next_kf_scale = *(float *)(default_scales + (int)(short)scale_count * 4);
      kf0_frame = kf1_frame;
      kf1_frame = kf1_frame + 1;
    } else {
      /* Between two keyframes: binary search */
      kf_idx = FUN_00120d10(keyframe_frame_indices, (short)(int)frame_floor_f,
                            keyframe_count);

      if (kf_idx < 0 || (int)kf_idx >= kf_count_i - 1) {
        display_assert("keyframe_index>=0 && keyframe_index<keyframe_count-1",
                       "c:\\halo\\SOURCE\\models\\model_animations.c", 0x677,
                       1);
        system_exit(-1);
      }

      kf0_frame = keyframe_frame_indices[(int)kf_idx];
      kf1_frame = keyframe_frame_indices[(int)kf_idx + 1];
      this_kf_scale = *(float *)(keyframe_data + (int)kf_idx * 4);
      next_kf_scale = *(float *)(keyframe_data + ((int)kf_idx + 1) * 4);
    }
  }

  /* If frame == this_keyframe_frame exactly, copy directly */
  this_frame_f = (float)(int)(short)kf0_frame;
  if (frame == this_frame_f) {
    *(float *)out_scale = this_kf_scale;
    return;
  }

  /* Compute blend factor and interpolate */
  blend = (frame - this_frame_f) /
          (float)((int)(short)kf1_frame - (int)(short)kf0_frame);

  /* Assert: real_frame_index >= (real)this_keyframe_frame_index */
  if (frame < this_frame_f) {
    display_assert("real_frame_index>=(real)this_keyframe_frame_index",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x688, 1);
    system_exit(-1);
  }

  /* Assert: real_frame_index < (real)next_keyframe_frame_index */
  if (frame >= (float)(int)(short)kf1_frame) {
    display_assert("real_frame_index< (real)next_keyframe_frame_index",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x689, 1);
    system_exit(-1);
  }

  scalars_interpolate(this_kf_scale, next_kf_scale, blend, (float *)out_scale);
}

/* animation_update_internal (0x121c30) — Advance an animation state by one
 * frame and classify the result.
 *
 * Confirmed: tag_get('antr', animation_graph_tag_index) at 0x121c3f; asserts
 * "state" (line 0x93) when state is NULL (0x121c4a-0x121c68).
 * Confirmed: animation element = tag_block_get_element(tag+0x74, state[0],
 * 0xb4) at 0x121c7d. When out_sound is non-NULL, writes -1 unless
 * element+0x3c != -1 and element+0x3e == state[1], in which case it writes
 * dword +0xc of tag_block_get_element(tag+0x54, element+0x3c, 0x14)
 * (0x121c8e-0x121cbe).
 * Confirmed: INC word [state+2] at 0x121cc4, then compares against the frame
 * count at element+0x22. Return codes: 4 = clamp to element+0x2e (bounded by
 * count-1), 3 = pick next animation via model_animation_choose_random with
 * element+0x42 and reset frame to 0, 2 = last frame with element+0x2e == 0,
 * 1 = frame equals element+0x34 or element+0x36, else 0.
 * Field meanings beyond these accesses are unknown. */
int animation_update_internal(int update_kind, int animation_graph_tag_index,
                              short *state, int *out_sound)
{
  char *antr_tag;
  char *animation;
  char *sound_element;
  short frame;
  short frame_count;
  short loop_frame;
  int clamped;

  antr_tag = (char *)tag_get(0x616e7472, animation_graph_tag_index);
  if (state == NULL) {
    display_assert("state", "c:\\halo\\SOURCE\\models\\model_animations.c",
                   0x93, 1);
    system_exit(-1);
  }
  animation =
    (char *)tag_block_get_element(antr_tag + 0x74, (int)state[0], 0xb4);
  if (out_sound != NULL) {
    if (*(short *)(animation + 0x3c) == -1 ||
        *(short *)(animation + 0x3e) != state[1]) {
      *out_sound = -1;
    } else {
      sound_element = (char *)tag_block_get_element(
        antr_tag + 0x54, (int)*(short *)(animation + 0x3c), 0x14);
      *out_sound = *(int *)(sound_element + 0xc);
    }
  }
  state[1]++;
  frame = state[1];
  frame_count = ((animation_t *)animation)->frame_count;
  if (frame >= frame_count) {
    loop_frame = ((animation_t *)animation)->field_2e;
    if (loop_frame > 0) {
      clamped = frame_count - 1;
      if ((int)loop_frame <= frame_count - 1)
        clamped = (int)loop_frame;
      state[1] = (short)clamped;
      return 4;
    }
    state[0] = model_animation_choose_random(
      update_kind, animation_graph_tag_index, *(int16_t *)(animation + 0x42));
    state[1] = 0;
    return 3;
  }
  if (frame + 1 == (int)frame_count && ((animation_t *)animation)->field_2e == 0)
    return 2;
  if (frame != *(short *)(animation + 0x34) &&
      frame != *(short *)(animation + 0x36))
    return 0;
  return 1;
}

/* FUN_00121d60 (0x121d60) — Decode a single animation frame into per-node
 * rotation/translation/scale data.
 *
 * For each node in the animation (animation+0x2c count), decodes rotation
 * (quaternion), translation (vec3), and scale (float) from either:
 *   - Compressed keyframed data (when flag bit 0 is set and compression is
 *     active), using
 * FUN_00121330/animation_get_node_orientations/animation_get_keyframe_scale
 * interpolators.
 *   - Uncompressed frame data via
 * quaternion_decompress_8byte/quaternion_decompress_6byte or raw memcpy from
 * default data (animation+0x98).
 *
 * Three bitmask arrays at animation offsets 0x5c, 0x6c, 0x7c (4 DWORDs each
 * for up to 128 nodes) indicate which nodes have animated rotation,
 * translation, and scale respectively. Bit=1 means animated (read from
 * frame data), bit=0 means static (read from default data).
 *
 * If the animation type (animation+0x20) is nonzero, or the mode_tag check
 * fails, falls back to FUN_00123aa0 which fills default node transforms.
 *
 * After the loop, two assertions verify that exactly the right amount of
 * frame data and default data was consumed.
 *
 * Confirmed: cdecl, 4 args, void return.
 * Confirmed: CALL FUN_00120500 at 0x121dca and 0x121fd5 (2 args: animation,
 * frame_index). Confirmed: CALL quaternion_decompress_8byte at 0x121e63 and
 * 0x121e9d (2 args: src_shorts, dest_floats). Confirmed: CALL
 * quaternion_decompress_6byte at 0x121e89 (2 args: compressed_data,
 * dest_floats). Confirmed: CALL sphere_intersects_rectangle3d at 0x121e8f (1
 * arg: quaternion). Confirmed: CALL FUN_00121330 at 0x121e51 (5 args:
 * animation, frame_float, count, node, out). Confirmed: CALL
 * animation_get_node_orientations at 0x121edc (5 args: animation, frame_float,
 * count, node, out). Confirmed: CALL animation_get_keyframe_scale
 * at 0x121f78 (5 args: animation, frame_float, count, node, out). Confirmed:
 * CALL FUN_00123aa0 at 0x12204a (2 args: mode_tag, out_node_data).
 */
void FUN_00121d60(void *mode_tag, void *animation, int animation_index,
                  void *out_node_data)
{
  int param_1;
  int param_2;
  int param_4;
  unsigned int uVar6;
  int iVar7;
  short sVar5;
  int iVar3;
  unsigned int local_1c;
  unsigned int local_20;
  unsigned int local_14;
  int local_10;
  int local_18;
  int local_24;
  int *local_c;
  int *local_8;
  int bVar2;
  int *puVar4;

  param_1 = (int)mode_tag;
  param_2 = (int)animation;
  param_4 = (int)out_node_data;
  uVar6 = 0;

  if (((animation_t *)param_2)->field_20 == 0 &&
      (param_1 == 0 ||
       (((((animation_t *)param_2)->field_28 == 0 ||
          ((animation_t *)param_2)->field_28 == *(int *)(param_1 + 4) ||
          *(int *)(param_1 + 4) == 0) &&
         *(int *)(param_1 + 0xb8) == (int)((animation_t *)param_2)->field_2c)))) {
    if (((((animation_t *)param_2)->field_3a & 1) == 0) ||
        (model_animation_compression == '\0' && *(int *)(param_2 + 0x88) != 0)) {
      bVar2 = 0;
    } else {
      bVar2 = 1;
    }

    local_8 = (int *)FUN_00120500(animation, (short)animation_index);
    local_c = *(int **)(param_2 + 0x98);
    local_10 = 0;
    local_18 = 0;
    local_24 = 0;
    if (0 < ((animation_t *)param_2)->field_2c) {
      do {
        sVar5 = (short)uVar6;
        iVar7 = sVar5 * 0x20 + param_4;
        if ((uVar6 & 0x1f) == 0) {
          iVar3 = (int)(sVar5 >> 5);
          local_1c = *(unsigned int *)(param_2 + 0x5c + iVar3 * 4);
          local_14 = *(unsigned int *)(param_2 + 0x6c + iVar3 * 4);
          local_20 = *(unsigned int *)(param_2 + 0x7c + iVar3 * 4);
        }
        if ((local_14 & 1) == 0) {
          if (bVar2) {
            quaternion_decompress_6byte(
              (void *)(local_8[1] + sVar5 * 6 + (int)local_8), (float *)iVar7);
            sphere_intersects_rectangle3d((float *)iVar7);
          } else {
            quaternion_decompress_8byte((short *)local_c, (float *)iVar7);
            local_c = (int *)((char *)local_c + 8);
          }
        } else if (bVar2) {
          FUN_00121330(animation, (float)(int)(short)animation_index,
                       (unsigned short)local_10, sVar5, (void *)iVar7);
          local_10 = local_10 + 1;
        } else {
          quaternion_decompress_8byte((short *)local_8, (float *)iVar7);
          local_8 = (int *)((char *)local_8 + 8);
        }
        local_14 = local_14 >> 1;

        if ((local_1c & 1) == 0) {
          if (bVar2) {
            puVar4 = (int *)(local_8[5] + sVar5 * 0xc + (int)local_8);
            *(int *)(iVar7 + 0x10) = puVar4[0];
            *(int *)(iVar7 + 0x14) = puVar4[1];
            *(int *)(iVar7 + 0x18) = puVar4[2];
          } else {
            *(int *)(iVar7 + 0x10) = local_c[0];
            *(int *)(iVar7 + 0x14) = local_c[1];
            *(int *)(iVar7 + 0x18) = local_c[2];
            local_c = (int *)((char *)local_c + 0xc);
          }
        } else if (bVar2) {
          animation_get_node_orientations(
            animation, (float)(int)(short)animation_index,
            (unsigned short)local_18, sVar5, (void *)(iVar7 + 0x10));
          local_18 = local_18 + 1;
        } else {
          *(int *)(iVar7 + 0x10) = local_8[0];
          *(int *)(iVar7 + 0x14) = local_8[1];
          *(int *)(iVar7 + 0x18) = local_8[2];
          local_8 = (int *)((char *)local_8 + 0xc);
        }
        local_1c = local_1c >> 1;

        if ((local_20 & 1) == 0) {
          if (bVar2) {
            *(int *)(iVar7 + 0x1c) = 0x3f800000;
          } else {
            *(int *)(iVar7 + 0x1c) = *local_c;
            local_c = (int *)((char *)local_c + 4);
          }
        } else if (bVar2) {
          animation_get_keyframe_scale(
            animation, (float)(int)(short)animation_index,
            (unsigned short)local_24, sVar5, (void *)(iVar7 + 0x1c));
          local_24 = local_24 + 1;
        } else {
          *(int *)(iVar7 + 0x1c) = *local_8;
          local_8 = (int *)((char *)local_8 + 4);
        }
        local_20 = local_20 >> 1;

        uVar6 = uVar6 + 1;
      } while ((short)uVar6 < ((animation_t *)param_2)->field_2c);
    }
    if (!bVar2) {
      iVar7 = (int)FUN_00120500(animation, (short)animation_index);
      if ((int)local_8 - iVar7 != (int)((animation_t *)param_2)->frame_size) {
        display_assert("compressed || (byte *)data-(byte "
                       "*)animation_get_frame_data(animation, "
                       "frame_index)==animation->frame_size",
                       "c:\\halo\\SOURCE\\models\\model_animations.c", 0x141,
                       1);
        system_exit(-1);
      }
      if ((int)local_c - *(int *)(param_2 + 0x98) != *(int *)(param_2 + 0x8c)) {
        display_assert("compressed || (byte *)default_data-(byte "
                       "*)animation_get_default_data(animation)==animation->"
                       "default_data.size",
                       "c:\\halo\\SOURCE\\models\\model_animations.c", 0x142,
                       1);
        system_exit(-1);
      }
    }
  } else {
    FUN_00123aa0(mode_tag, out_node_data);
  }
}

/* replacement_animation_apply (0x122060) — Apply a replacement (type 2)
 * animation's per-node rotation/translation/scale channels onto a node
 * transform array.
 *
 * Structural sibling of FUN_00121d60, restricted to animations whose type
 * (animation+0x20) is 2 and to a frame_index inside [0, animation+0x22).
 * For each node the three per-channel bitmask arrays are reloaded every 32
 * nodes and consumed one bit at a time:
 *   +0x5c[]: translation channel present
 *   +0x6c[]: rotation channel present
 *   +0x7c[]: scale channel present
 * When the animation is not compressed (animation_is_compressed == 0) the
 * values are read sequentially out of the frame data returned by FUN_00120500;
 * when it is compressed each present channel is evaluated from the keyframe
 * streams with a per-channel running component index.
 *
 * Output stride is 0x20 per node: +0x00 rotation quaternion (4 floats),
 * +0x10 translation (3 floats), +0x1c scale (1 float).
 *
 * Confirmed: cdecl, 3 args, void return (MOV ESP,EBP epilogue at 0x12222d).
 * Confirmed: CALL animation_is_compressed(animation@<esi>) at 0x12208f — no
 * stack args, result byte stored to [EBP+0xb]. Confirmed: CALL FUN_00120500 at
 * 0x122099 and 0x1221f5 (2 args: animation, frame_index). Confirmed: CALL
 * quaternion_decompress_8byte at 0x12211d (2 args: src_shorts, dest_floats).
 * Confirmed: CALL FUN_00121330 at 0x122108, animation_get_node_orientations
 * at 0x12215c, animation_get_keyframe_scale at 0x1221b7 — each 5
 * args pushed out,node,count,frame,animation with the float frame lowered as
 * PUSH <dummy>; FILD [EBP-0x20]; FSTP [ESP] (frame = (float)(int)frame_index).
 * Confirmed: node stride 0x20 via MOVSX EDI,BX; SHL EDI,0x5 at 0x1220ba.
 * Confirmed: mask block index is a 16-bit arithmetic shift (MOV AX,BX; SAR
 * AX,0x5; MOVSX EAX,AX at 0x1220c7). Confirmed: uncompressed advances are 8
 * bytes (rotation), 0xc bytes (translation), 4 bytes (scale). Confirmed:
 * assert line 0x187 at 0x12221b followed by system_exit(-1).
 */
void replacement_animation_apply(void *animation, short frame_index,
                                 void *node_data)
{
  char *anim;
  char compressed;
  int *data;
  int frame_data;
  int out_node;
  int block_index;
  int rotation_count;
  int translation_count;
  int scale_count;
  short node_index;
  unsigned int translation_flags;
  unsigned int rotation_flags;
  unsigned int scale_flags;

  anim = (char *)animation;

  if (((animation_t *)anim)->field_20 == 2) {
    node_index = 0;
    if (frame_index >= node_index && frame_index < ((animation_t *)anim)->frame_count) {
      compressed = FUN_00120620((int)anim);
      data = (int *)FUN_00120500(animation, frame_index);
      rotation_count = 0;
      translation_count = 0;
      scale_count = 0;

      if (0 < ((animation_t *)anim)->field_2c) {
        do {
          out_node = (int)node_data + (int)node_index * 0x20;
          if ((node_index & 0x1f) == 0) {
            block_index = (int)(short)(node_index >> 5);
            translation_flags =
              *(unsigned int *)(anim + block_index * 4 + 0x5c);
            rotation_flags = *(unsigned int *)(anim + block_index * 4 + 0x6c);
            scale_flags = *(unsigned int *)(anim + block_index * 4 + 0x7c);
          }

          if ((rotation_flags & 1) != 0) {
            if (compressed != 0) {
              FUN_00121330(animation, (float)(int)frame_index,
                           (unsigned short)rotation_count, node_index,
                           (void *)out_node);
              rotation_count = rotation_count + 1;
            } else {
              quaternion_decompress_8byte((short *)data, (float *)out_node);
              data = (int *)((char *)data + 8);
            }
          }
          rotation_flags = rotation_flags >> 1;

          if ((translation_flags & 1) != 0) {
            if (compressed != 0) {
              animation_get_node_orientations(
                animation, (float)(int)frame_index,
                (unsigned short)translation_count, node_index,
                (void *)(out_node + 0x10));
              translation_count = translation_count + 1;
            } else {
              *(int *)(out_node + 0x10) = data[0];
              *(int *)(out_node + 0x14) = data[1];
              *(int *)(out_node + 0x18) = data[2];
              data = (int *)((char *)data + 0xc);
            }
          }
          translation_flags = translation_flags >> 1;

          if ((scale_flags & 1) != 0) {
            if (compressed != 0) {
              animation_get_keyframe_scale(
                animation, (float)(int)frame_index, (unsigned short)scale_count,
                node_index, (void *)(out_node + 0x1c));
              scale_count = scale_count + 1;
            } else {
              *(int *)(out_node + 0x1c) = data[0];
              data = (int *)((char *)data + 4);
            }
          }
          scale_flags = scale_flags >> 1;

          node_index = node_index + 1;
        } while (node_index < ((animation_t *)anim)->field_2c);
      }

      if (compressed == 0) {
        frame_data = (int)FUN_00120500(animation, frame_index);
        if ((int)data - frame_data != (int)((animation_t *)anim)->frame_size) {
          display_assert("compressed || ((byte *)data-(byte "
                         "*)animation_get_frame_data(animation, "
                         "frame_index)==animation->frame_size)",
                         "c:\\halo\\SOURCE\\models\\model_animations.c", 0x187,
                         1);
          system_exit(-1);
        }
      }
    }
  }
}

/* overlay_animation_apply (0x122240) — Blend an overlay (type 1) animation's
 * per-node rotation/translation/scale channels onto a node transform array.
 *
 * Structural sibling of replacement_animation_apply (0x122060): same channel
 * bitmask walk, but the sampled values are COMBINED with the existing node
 * transform instead of overwriting it — rotation is quaternion-multiplied
 * (FUN_0010b9c0), translation is added, scale is multiplied.
 *
 * Restricted to animations whose type (animation+0x20) is 1 and to a
 * frame_index inside [0, animation+0x22). The three per-channel bitmask
 * arrays are reloaded every 32 nodes and consumed one bit at a time:
 *   +0x5c[]: translation channel present
 *   +0x6c[]: rotation channel present
 *   +0x7c[]: scale channel present
 *
 * Output stride is 0x20 per node: +0x00 rotation quaternion (4 floats),
 * +0x10 translation (3 floats), +0x1c scale (1 float).
 *
 * Confirmed: cdecl, 3 args, void return (MOV ESP,EBP epilogue at 0x122442).
 * Confirmed: frame_index is read as a 16-bit value (CMP DI,BX at 0x12225c;
 * MOVSX EAX,word ptr [EBP+0xc] at 0x1222d7).
 * Confirmed: CALL animation_is_compressed(animation@<esi>) at 0x12226f — no
 * stack args, result byte stored to [EBP+0xb]. Confirmed: CALL FUN_00120500 at
 * 0x122279 and 0x12240a (2 args: animation, frame_index). Confirmed: CALL
 * quaternion_decompress_8byte at 0x122300 (2 args: src_shorts, dest_floats;
 * last push EDX=data is arg0). Confirmed: CALL FUN_0010b9c0 at 0x122317 with
 * pushes EDI,EDI,LEA[EBP-0x40] — args (rotation, node, node).
 * Confirmed: CALL FUN_00121330 at 0x1222eb, animation_get_node_orientations
 * at 0x12234d, animation_get_keyframe_scale at 0x1223c3 — each 5
 * args pushed out,node,count,frame,animation with the float frame lowered as
 * PUSH <dummy>; FILD [EBP-0x24]; FSTP [ESP] (frame = (float)(int)frame_index).
 * Confirmed: node stride 0x20 via MOVSX EDI,BX; SHL EDI,0x5 at 0x12229a.
 * Confirmed: mask block index is a 16-bit arithmetic shift (MOV AX,BX; SAR
 * AX,0x5; MOVSX EAX,AX at 0x1222a7). Confirmed: uncompressed advances are 8
 * bytes (rotation), 0xc bytes (translation), 4 bytes (scale), each copied as
 * dwords (MOV pairs at 0x122363 and 0x1223d7).
 * Confirmed: accumulate order is FLD local; FADD [EDI+N] (0x12237a) and
 * FLD local; FMUL [EDI+0x1c] (0x1223e2).
 * Confirmed: assert line 0x1d6 at 0x122430 followed by system_exit(-1).
 */
void overlay_animation_apply(void *anim_entry, int frame, void *node_data)
{
  char *anim;
  char compressed;
  int *data;
  int frame_data;
  int out_node;
  int block_index;
  int rotation_count;
  int translation_count;
  int scale_count;
  short frame_index;
  short node_index;
  unsigned int translation_flags;
  unsigned int rotation_flags;
  unsigned int scale_flags;
  float rotation[4];
  float translation[3];
  float scale;

  anim = (char *)anim_entry;
  frame_index = (short)frame;

  if (((animation_t *)anim)->field_20 == 1) {
    node_index = 0;
    if (frame_index >= node_index && frame_index < ((animation_t *)anim)->frame_count) {
      compressed = FUN_00120620((int)anim);
      data = (int *)FUN_00120500(anim_entry, frame_index);
      rotation_count = 0;
      translation_count = 0;
      scale_count = 0;

      if (0 < ((animation_t *)anim)->field_2c) {
        do {
          out_node = (int)node_data + (int)node_index * 0x20;
          if ((node_index & 0x1f) == 0) {
            block_index = (int)(short)(node_index >> 5);
            translation_flags =
              *(unsigned int *)(anim + block_index * 4 + 0x5c);
            rotation_flags = *(unsigned int *)(anim + block_index * 4 + 0x6c);
            scale_flags = *(unsigned int *)(anim + block_index * 4 + 0x7c);
          }

          if ((rotation_flags & 1) != 0) {
            if (compressed != 0) {
              FUN_00121330(anim_entry, (float)(int)frame_index,
                           (unsigned short)rotation_count, node_index,
                           rotation);
              rotation_count = rotation_count + 1;
            } else {
              quaternion_decompress_8byte((short *)data, rotation);
              data = (int *)((char *)data + 8);
            }
            FUN_0010b9c0(rotation, (float *)out_node, (float *)out_node);
          }
          rotation_flags = rotation_flags >> 1;

          if ((translation_flags & 1) != 0) {
            if (compressed != 0) {
              animation_get_node_orientations(
                anim_entry, (float)(int)frame_index,
                (unsigned short)translation_count, node_index, translation);
              translation_count = translation_count + 1;
            } else {
              *(int *)&translation[0] = data[0];
              *(int *)&translation[1] = data[1];
              *(int *)&translation[2] = data[2];
              data = (int *)((char *)data + 0xc);
            }
            *(float *)(out_node + 0x10) =
              translation[0] + *(float *)(out_node + 0x10);
            *(float *)(out_node + 0x14) =
              translation[1] + *(float *)(out_node + 0x14);
            *(float *)(out_node + 0x18) =
              translation[2] + *(float *)(out_node + 0x18);
          }
          translation_flags = translation_flags >> 1;

          if ((scale_flags & 1) != 0) {
            if (compressed != 0) {
              animation_get_keyframe_scale(anim_entry, (float)(int)frame_index,
                                           (unsigned short)scale_count,
                                           node_index, &scale);
              scale_count = scale_count + 1;
            } else {
              *(int *)&scale = data[0];
              data = (int *)((char *)data + 4);
            }
            *(float *)(out_node + 0x1c) = scale * *(float *)(out_node + 0x1c);
          }
          scale_flags = scale_flags >> 1;

          node_index = node_index + 1;
        } while (node_index < ((animation_t *)anim)->field_2c);
      }

      if (compressed == 0) {
        frame_data = (int)FUN_00120500(anim_entry, frame_index);
        if ((int)data - frame_data != (int)((animation_t *)anim)->frame_size) {
          display_assert("compressed || ((byte *)data-(byte "
                         "*)animation_get_frame_data(animation, "
                         "frame_index)==animation->frame_size)",
                         "c:\\halo\\SOURCE\\models\\model_animations.c", 0x1d6,
                         1);
          system_exit(-1);
        }
      }
    }
  }
}

/* overlay_animation_apply_scaled (0x122450) -- overlay_animation_apply with
 * every per-node delta weighted by animation_scale: the rotation is first
 * interpolated from the identity quaternion (*(float **)0x31fc5c) by
 * animation_scale (FUN_0010ba90 = PAL quaternions_interpolate), then composed
 * onto the node (FUN_0010b9c0); translation += delta * scale; node scale *=
 * scale_delta * animation_scale + (1 - animation_scale).
 *
 * Confirmed params (callers 0x141e29, 0x1aff19, 0xdd98d..): [EBP+0xc] is a
 * 16-bit frame index (CMP DI,BX / CMP DI,[ESI+0x22]); [EBP+0x10] is a float
 * (FMUL [EBP+0x10], and first_person_weapon_update pushes it with FSTP
 * [ESP]); [EBP+0x14] is the node orientation array (stride 0x20).  The old
 * kb decl (int frame, void *in_node_data, void *out_node_data) was wrong.
 * Assert: model_animations.c 0x22a, same text as overlay_animation_apply.
 * Shape: PAL 2342 overlay_animation_apply_scaled (T2). */
void overlay_animation_apply_scaled(void *animation, int16_t frame_index,
                                    real animation_scale,
                                    void *node_orientations)
{
  char *anim;
  real inverse_animation_scale;
  char compressed;
  char *data;
  int rotation_index;
  int translation_index;
  int scale_index;
  unsigned int rotation_flags;
  unsigned int translation_flags;
  unsigned int scale_flags;
  int16_t node_index;
  float rotation[4];
  real_point3d translation;
  float scale;

  anim = (char *)animation;
  inverse_animation_scale = 1.0f - animation_scale;
  if (((animation_t *)anim)->field_20 == 1 && frame_index >= 0 &&
      frame_index < ((animation_t *)anim)->frame_count) {
    compressed = FUN_00120620((int)anim);
    data = (char *)FUN_00120500(animation, frame_index);
    rotation_index = 0;
    translation_index = 0;
    scale_index = 0;

    for (node_index = 0; node_index < ((animation_t *)anim)->field_2c; node_index++) {
      float *orientation = (float *)node_orientations + node_index * 8;

      if ((node_index & 0x1f) == 0) {
        short long_index = (short)(node_index >> 5);

        translation_flags = *(unsigned int *)(anim + long_index * 4 + 0x5c);
        rotation_flags = *(unsigned int *)(anim + long_index * 4 + 0x6c);
        scale_flags = *(unsigned int *)(anim + long_index * 4 + 0x7c);
      }

      if ((rotation_flags & 1) != 0) {
        if (compressed) {
          FUN_00121330(animation, (float)frame_index,
                       (unsigned short)rotation_index++, node_index, rotation);
        } else {
          quaternion_decompress_8byte((short *)data, rotation);
          data += 8;
        }
        FUN_0010ba90(global_identity_quaternion_ptr, rotation, animation_scale, rotation);
        FUN_0010b9c0(rotation, orientation, orientation);
      }
      rotation_flags >>= 1;

      if ((translation_flags & 1) != 0) {
        if (compressed) {
          animation_get_node_orientations(animation, (float)frame_index,
                                          (unsigned short)translation_index++,
                                          node_index, &translation);
        } else {
          translation = *(real_point3d *)data;
          data += 0xc;



        }
        orientation[4] += translation.x * animation_scale;
        orientation[5] += translation.y * animation_scale;
        orientation[6] += translation.z * animation_scale;
      }
      translation_flags >>= 1;

      if ((scale_flags & 1) != 0) {
        if (compressed) {
          animation_get_keyframe_scale(animation, (float)frame_index,
                                       (unsigned short)scale_index++,
                                       node_index, &scale);
        } else {
          scale = *(real *)data;
          data += 4;
        }
        orientation[7] *= scale * animation_scale + inverse_animation_scale;
      }
      scale_flags >>= 1;
    }

    if (!compressed && data - (char *)FUN_00120500(animation, frame_index) !=
                         ((animation_t *)anim)->frame_size) {
      display_assert("compressed || ((byte *)data-(byte "
                     "*)animation_get_frame_data(animation, "
                     "frame_index)==animation->frame_size)",
                     "c:\\halo\\SOURCE\\models\\model_animations.c", 0x22a, 1);
      system_exit(-1);
    }
  }
}

/* overlay_animation_apply_continuous (0x122690) -- overlay animation sampled
 * at a fractional frame: each node delta is interpolated between frame
 * floor(|real_frame_index|) and the next frame (wrapping to 0 after the last)
 * by fraction = fmod(real_frame_index, 1.0), then applied like
 * overlay_animation_apply (rotation composed via FUN_0010b9c0, translation
 * added, scale multiplied).
 *
 * Name: PAL 2342 overlay_animation_apply_continuous (T2), corroborated by the
 * XBE's own error string "### ERROR animation frame index out of bounds
 * A(%f,%x) -- tell Bernie!!" (0x291388) that PAL places in this function, and
 * by the two frame-size asserts on data / next_data (model_animations.c
 * 0x2b5 / 0x2b6).  Was FUN_00122690.
 *
 * Confirmed from 0x122690..0x122a48: fmod is the CRT __CIfmod (0x1daf7e) with
 * the double 1.0 at 0x2573d8; floor is CRT 0x1d9c2b (anim_floor here); the
 * float->int is a bare FISTP (fast_ftol).  animation_is_compressed
 * (animation_is_compressed) is expanded inline WITHOUT its null assert:
 * (anim+0x3a & 1) && (model_animation_compression || *(int *)(anim+0x88) == 0).
 * Compressed rotation samples at (real)frame_index; compressed translation
 * and scale sample at real_frame_index (PUSH [EBP+0xc] at 0x1228fe /
 * 0x122964). */
void overlay_animation_apply_continuous(void *animation, float real_frame_index,
                                        void *node_orientations)
{
#if defined(_MSC_VER) && !defined(__clang__)
  double __cdecl fmod(double, double);
#endif
  char *anim;
  real fraction;
  real frame_floor;
  int16_t frame_index;
  char compressed;
  int next_frame_index;
  char *data;
  char *next_data;
  int rotation_index;
  int translation_index;
  int scale_index;
  unsigned int rotation_flags;
  unsigned int translation_flags;
  unsigned int scale_flags;
  int16_t node_index;
  float rotation[4];
  float this_rotation[4];
  float next_rotation[4];
  float translation[3];

  anim = (char *)animation;
#if defined(_MSC_VER) && !defined(__clang__)
  fraction = (real)fmod((double)real_frame_index, 1.0);
#else
  fraction = x87_fmod(real_frame_index, 1.0);
#endif
  frame_floor = (real)anim_floor(fabs(real_frame_index));
  frame_index = (int16_t)x87_round_to_int(frame_floor);

  if (real_frame_index < 0.0f ||
      real_frame_index > (real)((animation_t *)anim)->frame_count) {
    error(2,
          "### ERROR animation frame index out of bounds A(%f,%x) -- tell "
          "Bernie!!",
          (double)real_frame_index, *(long *)&real_frame_index);
  }

  if (frame_index >= ((animation_t *)anim)->frame_count) {
    frame_index = (int16_t)(((animation_t *)anim)->frame_count - 1);
    fraction = 1.0f;
    real_frame_index = (real)frame_index;
  }

  if (((animation_t *)anim)->field_20 == 1) {
    compressed = (((animation_t *)anim)->field_3a & 1) &&
                 (model_animation_compression || *(int *)(anim + 0x88) == 0);
    next_frame_index =
      frame_index == ((animation_t *)anim)->frame_count - 1 ? 0 : frame_index + 1;
    data = (char *)FUN_00120500(animation, frame_index);
    next_data = (char *)FUN_00120500(animation, (short)next_frame_index);
    rotation_index = 0;
    translation_index = 0;
    scale_index = 0;

    for (node_index = 0; node_index < ((animation_t *)anim)->field_2c; node_index++) {
      float *orientation = (float *)node_orientations + node_index * 8;

      if ((node_index & 0x1f) == 0) {
        short long_index = (short)(node_index >> 5);

        translation_flags = *(unsigned int *)(anim + long_index * 4 + 0x5c);
        rotation_flags = *(unsigned int *)(anim + long_index * 4 + 0x6c);
        scale_flags = *(unsigned int *)(anim + long_index * 4 + 0x7c);
      }

      if ((rotation_flags & 1) != 0) {
        if (compressed) {
          FUN_00121330(animation, (float)frame_index,
                       (unsigned short)rotation_index++, node_index, rotation);
        } else {
          quaternion_decompress_8byte((short *)data, this_rotation);
          data += 8;
          quaternion_decompress_8byte((short *)next_data, next_rotation);
          next_data += 8;
          quaternions_interpolate_and_normalize(this_rotation, next_rotation,
                                                fraction, rotation);
        }
        FUN_0010b9c0(rotation, orientation, orientation);
      }
      rotation_flags >>= 1;

      if ((translation_flags & 1) != 0) {
        if (compressed) {
          animation_get_node_orientations(animation, real_frame_index,
                                          (unsigned short)translation_index++,
                                          node_index, translation);
        } else {
          float *this_translation = (float *)data;
          float *next_translation = (float *)next_data;

          data += 0xc;
          next_data += 0xc;
          points_interpolate(this_translation, next_translation, fraction,
                             translation);
        }
        orientation[4] += translation[0];
        orientation[5] += translation[1];
        orientation[6] += translation[2];
      }
      translation_flags >>= 1;

      if ((scale_flags & 1) != 0) {
        float scale;
        if (compressed) {
          animation_get_keyframe_scale(animation, real_frame_index,
                                       (unsigned short)scale_index++,
                                       node_index, &scale);
        } else {
          float this_scale;
          float next_scale;

          this_scale = *(real *)data;
          data += 4;
          next_scale = *(real *)next_data;
          next_data += 4;
          scalars_interpolate(this_scale, next_scale, fraction, &scale);
        }
        orientation[7] *= scale;
      }
      scale_flags >>= 1;
    }

    if (!compressed) {
      if (data - (char *)FUN_00120500(animation, frame_index) !=
          ((animation_t *)anim)->frame_size) {
        display_assert("compressed || ((byte *)data-(byte "
                       "*)animation_get_frame_data(animation, "
                       "frame_index)==animation->frame_size)",
                       "c:\\halo\\SOURCE\\models\\model_animations.c", 0x2b5,
                       1);
        system_exit(-1);
      }
      if (next_data -
            (char *)FUN_00120500(animation, (short)next_frame_index) !=
          ((animation_t *)anim)->frame_size) {
        display_assert("compressed || ((byte *)next_data-(byte "
                       "*)animation_get_frame_data(animation, "
                       "next_frame_index)==animation->frame_size)",
                       "c:\\halo\\SOURCE\\models\\model_animations.c", 0x2b6,
                       1);
        system_exit(-1);
      }
    }
  }
}

/* FUN_00123aa0 (0x123aa0) — Fill default node transforms from mode tag.
 *
 * Iterates over the nodes in a model mode tag (tag block at mode_tag+0xb8,
 * element size 0x9c). For each node, copies the default rotation quaternion
 * from element+0x34 (4 floats) and default translation from element+0x28
 * (3 floats) into the output node_data array (stride 0x20 per node).
 * Sets scale to 1.0f for each node.
 *
 * This is the fallback path used by FUN_00121d60 when the animation type is
 * nonzero or the mode_tag node count doesn't match the animation.
 *
 * Confirmed: cdecl, 2 args (mode_tag ptr, out_node_data ptr).
 * Confirmed: CALL tag_block_get_element(mode_tag+0xb8, index, 0x9c) at
 * 0x123ac7. Confirmed: Copies element+0x34..0x43 (rotation) to out+0x00..0x0F.
 * Confirmed: Copies element+0x28..0x33 (translation) to out+0x10..0x1B.
 * Confirmed: Sets out+0x1c = 0x3f800000 (1.0f scale).
 * Confirmed: Loop counter is short via MOVSX at 0x123b0c; compared to [EDI] at
 * 0x123b19.
 */
void FUN_00123aa0(void *mode_tag, void *out_node_data)
{
  int param_1;
  int param_2;
  short sVar1;
  int iVar4;
  char *element;
  int *out;

  param_1 = (int)mode_tag;
  param_2 = (int)out_node_data;
  iVar4 = 0;
  sVar1 = 0;

  if (0 < *(int *)(param_1 + 0xb8)) {
    do {
      element =
        (char *)tag_block_get_element((void *)(param_1 + 0xb8), iVar4, 0x9c);
      out = (int *)(param_2 + iVar4 * 0x20);

      /* rotation quaternion from element+0x34 */
      out[0] = *(int *)(element + 0x34);
      out[1] = *(int *)(element + 0x38);
      out[2] = *(int *)(element + 0x3c);
      out[3] = *(int *)(element + 0x40);

      /* translation from element+0x28 */
      out[4] = *(int *)(element + 0x28);
      out[5] = *(int *)(element + 0x2c);
      out[6] = *(int *)(element + 0x30);

      /* scale = 1.0f */
      out[7] = 0x3f800000;

      sVar1 = sVar1 + 1;
      iVar4 = (int)sVar1;
    } while (iVar4 < *(int *)(param_1 + 0xb8));
  }
}

/* model_get_node_matrices (0x123b30) — Build the world node matrices for a
 * 'mode' tag by walking its node tree breadth-first.
 *
 * Confirmed: cdecl, 5 stack args — [EBP+0x8] mode_tag, [EBP+0xc] node_matrices,
 *            [EBP+0x10] position, [EBP+0x14] forward, [EBP+0x18] up. The
 *            defaults loaded when an arg is NULL are the global basis pointers
 *            *(float**)0x31fc1c (position), 0x31fc3c (forward), 0x31fc44 (up),
 *            matching the kb.json names global_forward_vector_ptr /
 *            global_up_vector_ptr.
 * Confirmed: node tag_block is mode_tag+0xb8, element stride 0x9c — the same
 *            block/stride pair as FUN_00123aa0 and animation_get_root_matrix.
 * Confirmed: CALL 0x1094d0 component_vectors_from_normal3d pushes
 *            LEA[EBX+0x34], LEA[EBX+0x28], LEA[EBP-0x40] (0x123b86..0x123b8e)
 *            so out=local matrix, position=node+0x28, basis=node+0x34. The
 *            ADD ESP,0x18 at 0x123b94 is the COMBINED cleanup for
 *            tag_block_get_element (12) + this call (12), not a 6-arg call.
 * Confirmed: CALL 0x10a110 pushes EDX(up), ECX(forward), EAX(position),
 *            ESI(node_matrices) at 0x123bc7..0x123bca — out first per the
 *            kb.json decl. The NULL tests run up, forward, position (i.e.
 *            right-to-left argument evaluation).
 * Confirmed: CALL 0x109850 at 0x123bd6 pushes ESI, LEA[EBP-0x40], ESI so
 *            a=node_matrices, b=local matrix, out=node_matrices. ADD ESP,0x1c
 *            at 0x123bdb is the COMBINED cleanup for 0x10a110 (16) + this (12).
 * Confirmed: CALL 0x109850 at 0x123c1e pushes ESI(node_matrices +
 *            node_index*0x34), EDX(local matrix), ECX(node_matrices +
 *            parent*0x34) so a=parent matrix, b=local matrix,
 *            out=node_matrices[node_index]. Both indices are MOVSX-widened
 *            int16 before the *0x34 scaling.
 * Confirmed: assert at 0x123bf8 — "node->parent_node_index!=NONE",
 *            "c:\halo\SOURCE\models\models.c", line 0x28a, halt=1, followed by
 *            system_exit(-1). It fires when node+0x24 == -1 on a non-root node.
 * Confirmed: unlike animation_graph_node_matrices_from_orientations there is no
 *            block-count guard before the loop and no write_index bounds
 *            asserts — the do/while runs at least once.
 * Confirmed: loop latch is CMP word ptr [EBP-0x8],CX at 0x123c5b — a 16-bit
 *            compare of read_index against write_index.
 */
void model_get_node_matrices(void *mode_tag, float *node_matrices,
                             float *position, float *forward, float *up)
{
  short node_indices[64];
  float node_matrix[13];
  int *node_block;
  int read_index;
  int write_index;
  char *node;
  short node_index;

  read_index = 0;
  node_indices[0] = 0;
  write_index = 1;
  node_block = (int *)((char *)mode_tag + 0xb8);
  do {
    node_index = node_indices[(short)read_index];
    read_index = read_index + 1;
    node = (char *)tag_block_get_element(node_block, (int)node_index, 0x9c);
    component_vectors_from_normal3d(node_matrix, (float *)(node + 0x28),
                                    (float *)(node + 0x34));
    if (node_index == 0) {
      matrix4x3_from_forward_up_position(
        node_matrices, position != NULL ? position : *(float **)0x31fc1c,
        forward != NULL ? forward : *(float **)0x31fc3c,
        up != NULL ? up : *(float **)0x31fc44);
      matrix4x3_multiply(node_matrices, node_matrix, node_matrices);
    } else {
      if (*(short *)(node + 0x24) == -1) {
        display_assert("node->parent_node_index!=NONE",
                       "c:\\halo\\SOURCE\\models\\models.c", 0x28a, 1);
        system_exit(-1);
      }
      matrix4x3_multiply(
        (float *)((char *)node_matrices + *(short *)(node + 0x24) * 0x34),
        node_matrix, (float *)((char *)node_matrices + (int)node_index * 0x34));
    }
    if (*(short *)(node + 0x20) != -1) {
      node_indices[(short)write_index] = *(short *)(node + 0x20);
      write_index = write_index + 1;
    }
    if (*(short *)(node + 0x22) != -1) {
      node_indices[(short)write_index] = *(short *)(node + 0x22);
      write_index = write_index + 1;
    }
  } while ((short)read_index != (short)write_index);
}

/* FUN_00123c70 (0x123c70) — Build world node matrices from node orientation
 * data by walking the mode-tag node tree breadth-first. */
void FUN_00123c70(void *mode_tag, void *out_matrices, void *node_data,
                  float *position, float *forward, float *up)
{
  short node_indices[64];
  float node_matrix[13];
  float root_matrix[13];
  void *node_block;
  void *node;
  float *parent_matrix;
  short node_index;
  int read_index;
  int write_index;

  matrix4x3_from_forward_up_position(root_matrix, position, forward, up);
  read_index = 0;
  node_block = (char *)mode_tag + 0xb8;
  if (0 < *(int *)((char *)mode_tag + 0xb8)) {
    write_index = 1;
    node_indices[0] = 0;
    do {
      node_index = node_indices[(short)read_index];
      read_index = read_index + 1;
      node = tag_block_get_element(node_block, (int)node_index, 0x9c);
      if (node_index == 0) {
        parent_matrix = root_matrix;
      } else {
        parent_matrix = (float *)((char *)out_matrices +
                                  *(short *)((char *)node + 0x24) * 0x34);
      }
      FUN_00109500(node_matrix,
                   (float *)((char *)node_data + (int)node_index * 0x20));
      matrix4x3_multiply(
        parent_matrix, node_matrix,
        (float *)((char *)out_matrices + (int)node_index * 0x34));
      if (*(short *)((char *)node + 0x20) != -1) {
        node_indices[(short)write_index] = *(short *)((char *)node + 0x20);
        write_index = write_index + 1;
      }
      if (*(short *)((char *)node + 0x22) != -1) {
        node_indices[(short)write_index] = *(short *)((char *)node + 0x22);
        write_index = write_index + 1;
      }
    } while ((short)read_index != (short)write_index);
  }
}

/* FUN_00123d80 (0x123d80) — Binary-search a mode-tag block (+0xac, element
 * size 0x40) for an element whose name (element+0x0) matches marker_name
 * case-insensitively; returns the int16 index or -1. PAL calls this
 * model_find_marker.
 *
 * Confirmed: early-out to OR AX,0xffff when model_ref == -1, marker_name ==
 * NULL, or *marker_name == 0 (JZ at 0x123d8c/0x123d93/0x123d98).
 * Confirmed: CALL tag_get(0x6d6f6465 ('mode'), model_ref) at 0x123da0.
 * Confirmed: hi = (int16)(word [mode+0xac]) - 1 (MOV DI,word / DEC DI); lo in
 * BX starts at 0; loop runs while lo <= hi (CMP BX,DI / JLE, JGE entry test).
 * Confirmed: mid = (MOVSX lo + MOVSX hi) / 2 (CDQ/SUB/SAR), kept in SI.
 * Confirmed: CALL tag_block_get_element(mode+0xac, MOVSX mid, 0x40) at
 * 0x123de6, then CALL crt_stricmp(marker_name, element) at 0x123df0; the
 * single ADD ESP,0x14 is the combined cleanup of both calls (3 + 2 args).
 * Confirmed: result 0 returns mid (MOV AX,SI); <0 sets hi = mid-1, else
 * lo = mid+1. Block semantics beyond element size/name are unknown.
 */
int16_t FUN_00123d80(int model_ref, const char *marker_name)
{
  char *mode_tag;
  void *block;
  short lo;
  short hi;
  short mid;
  int result;

  if (model_ref != -1 && marker_name != NULL && *marker_name != '\0') {
    mode_tag = (char *)tag_get(0x6d6f6465, model_ref); /* 'mode' */
    block = (void *)(mode_tag + 0xac);
    lo = 0;
    hi = (short)(*(short *)block - 1);
    if (hi >= 0) {
      do {
        mid = (short)(((int)lo + (int)hi) / 2);
        result = crt_stricmp(marker_name, (const char *)tag_block_get_element(
                                            block, (int)mid, 0x40));
        if (result == 0) {
          return mid;
        }
        if (result < 0) {
          hi = (short)(mid - 1);
        } else {
          lo = (short)(mid + 1);
        }
      } while (lo <= hi);
    }
  }
  return -1;
}

/* model_get_default_inverse_matrix (0x123e20) — Get a node's default matrix
 * from a model mode tag.
 *
 * Confirmed: cdecl, 2 args (mode_tag ptr, node_index short).
 * Confirmed: CALL tag_block_get_element(mode_tag+0xb8, node_index, 0x9c) at
 * 0x123e37 — same tag block/element-size pair as FUN_00123aa0 above, so
 * element+0x68 is a field of that same 0x9c-byte node structure (past the
 * element+0x28 translation and element+0x34 rotation already confirmed
 * there). Confirmed: MOVSX at 0x123e23 sign-extends node_index before the
 * PUSH. Confirmed: return value is element+0x68 (ADD EAX,0x68 at 0x123e3f) —
 * no dereference, so this returns a pointer, not a copied value.
 */
float *model_get_default_inverse_matrix(void *mode_tag, short node_index)
{
  char *element;

  element = (char *)tag_block_get_element((void *)((char *)mode_tag + 0xb8),
                                          (int)node_index, 0x9c);
  return (float *)(element + 0x68);
}

/* FUN_00123e50 (0x123e50) — Find a mode-tag node's index by name.
 *
 * Confirmed: cdecl, 2 args (tag_index int, name char*). Confirmed: early-out
 * returns -1 without calling tag_get when tag_index == -1 (JZ at 0x123e5c).
 * Confirmed: CALL tag_get(0x6d6f6465 ('mode'), tag_index) at 0x123e64.
 * Confirmed: tag block at mode_tag+0xb8 — same tag block/element-size pair
 * (0x9c) as FUN_00123aa0 and model_get_default_inverse_matrix above, so this
 * walks the mode tag's node array. Confirmed: CALL tag_block_get_element(nodes,
 * index, 0x9c) at 0x123e87. Confirmed: CALL csstrcmp(element, name) at 0x123e8e
 * — the element pointer itself is passed as the string, so the node name field
 * is at offset 0x0 of the 0x9c-byte node struct. Confirmed: loop index is a
 * short — MOVSX EAX,DI at 0x123e9d re-sign-extends it from DI before the count
 * comparison, matching a `short` C loop variable stored in a 32-bit register.
 * Confirmed: on a csstrcmp match (== 0) the function returns the index via MOV
 * AX,DI at 0x123ead; the tag_index==-1, empty-block, and no-match paths all
 * fall through to OR AX,0xffff at 0x123ea6 and return -1.
 */
short FUN_00123e50(int tag_index, const char *name)
{
  void *mode_tag;
  char *element;
  short index;
  int count;

  if (tag_index != -1) {
    mode_tag = tag_get(0x6d6f6465, tag_index); /* 'mode' */
    count = *(int *)((char *)mode_tag + 0xb8);
    index = 0;
    if (0 < count) {
      do {
        element = (char *)tag_block_get_element(
          (void *)((char *)mode_tag + 0xb8), (int)index, 0x9c);
        if (csstrcmp(element, name) == 0) {
          return index;
        }
        index = index + 1;
      } while ((int)index < count);
    }
  }
  return -1;
}


typedef struct render_model_skinning_data {
  real_matrix4x3 *node_matrices;
  short node_matrix_count;
  short pad_06;
} render_model_skinning_data;

/* Opaque payloads copied whole (REP MOVSD, ECX=0x1d / 0xa at 0x1245f6 /
 * 0x124621). */
typedef struct render_model_lighting_bytes {
  unsigned char bytes[0x74];
} render_model_lighting_bytes;

typedef struct render_model_effect_bytes {
  unsigned char bytes[0x28];
} render_model_effect_bytes;

typedef struct render_model_begin_data {
  unsigned int geometry_flags;
  int unique_identifier;
  render_model_skinning_data skinning;
  render_model_lighting_bytes lighting;
  struct {
    void *colors;
    void *values;
  } animation;
  render_model_effect_bytes effect;
  float centroid[3];
  float radius;
  float base_map_scale[2];
} render_model_begin_data;

cs(render_model_skinning_data, 0x08);
cs(render_model_begin_data, 0xcc);
co(render_model_begin_data, skinning, 0x08);
co(render_model_begin_data, lighting, 0x10);
co(render_model_begin_data, animation, 0x84);
co(render_model_begin_data, effect, 0x8c);
co(render_model_begin_data, centroid, 0xb4);
co(render_model_begin_data, radius, 0xc0);
co(render_model_begin_data, base_map_scale, 0xc4);

/* render_model (0x123ed0) — Prepare model skinning and render state, then
 * dispatch the selected geometry detail level through render_model_parts.
 *
 * The 0xcc-byte begin record and 64-entry matrix workspace are fixed by the
 * 2276 frame stores at 0x1245e5..0x1246dd. PAL 2342 models.c supplies the T2
 * field names; all offsets, flags, constants, call targets and assert lines
 * below were checked against the 2276 disassembly.
 */
void render_model(int model_ref, float distance, void *node_matrices,
                  void *region_permutation_indices, void *change_colors,
                  void *function_values, int lighting, void *centroid,
                  int radius, void *model_effect, int object_handle,
                  int forced_shader_permutation_index, int flags)
{
  char *model;
  real_matrix4x3 relative_node_matrices[64];
  render_model_begin_data parameters;
  short detail_level;
  short node_index;
  unsigned int geometry_flags;

  model = (char *)tag_get(0x6d6f6465, model_ref);
  if (profile_global_enable && render_model_profile_section_active) {
    profile_enter_private(render_model_profile_section);
  }

  if (!lighting) {
    display_assert("lighting", "c:\\halo\\SOURCE\\models\\models.c", 0x52, 1);
    system_exit(-1);
  }

  if (*(int *)(model + 4) == 0x0769c097 &&
      (*(unsigned char *)((char *)global_scenario_get() + 0x3e) & 1)) {
    *(char *)0x5a5570 = 1;
  } else {
    *(char *)0x5a5570 = 0;
  }

  if (distance >= *(float *)(model + 8) || (flags & 2)) {
    if (!region_permutation_indices) {
      region_permutation_indices = (void *)0x46e898;
    }
    if (!model_effect) {
      model_effect = (void *)0x46e870;
    }
    if (!change_colors) {
      change_colors = (void *)0x46e840;
    }
    if (!function_values) {
      function_values = (void *)0x46e830;
    }
    if (!centroid) {
      centroid = (char *)node_matrices + 0x28;
    }

    if (node_matrices) {
      for (node_index = 0; node_index < *(int *)(model + 0xb8); node_index++) {
        char *node;

        node = (char *)tag_block_get_element((void *)(model + 0xb8), node_index,
                                             0x9c);
        matrix4x3_multiply((float *)((char *)node_matrices + node_index * 0x34),
                           (float *)(node + 0x68),
                           (float *)&relative_node_matrices[node_index]);
      }
    } else {
      for (node_index = 0; node_index < *(int *)(model + 0xb8); node_index++) {
        relative_node_matrices[node_index] = *(real_matrix4x3 *)0x5065b4;
      }
    }

    detail_level = 4;
    while (detail_level > 0 &&
           distance < *(float *)(model + 8 + detail_level * 4)) {
      detail_level--;
    }
    {
      short detail_level_override = rasterizer_debug_model_lod;

      if (detail_level_override != -1) {
        if (detail_level_override < 0) {
          detail_level = 0;
        } else if (detail_level_override > 4) {
          detail_level = 4;
        } else {
          detail_level = detail_level_override;
        }
      }
    }
    if (detail_level < 0 || detail_level >= 5) {
      display_assert(
        "geometry_detail_level_index>=0 && "
        "geometry_detail_level_index<NUMBER_OF_DETAIL_LEVELS_PER_MODEL",
        "c:\\halo\\SOURCE\\models\\models.c", 0xa9, 1);
      system_exit(-1);
    }

    if (!(flags & 2)) {
      if (render_model_nodes) {
        for (node_index = 0; node_index < *(int *)(model + 0xb8);
             node_index++) {
          char *node;
          short parent_node_index;

          node = (char *)tag_block_get_element((void *)(model + 0xb8),
                                               node_index, 0x9c);
          parent_node_index = *(short *)(node + 0x24);
          if (parent_node_index != -1) {
            FUN_00189270(
              1, (float *)((char *)node_matrices + node_index * 0x34 + 0x28),
              (float *)((char *)node_matrices + parent_node_index * 0x34 +
                        0x28),
              global_real_argb_white);
          }
          FUN_001894d0(1, (float *)((char *)node_matrices + node_index * 0x34),
                       0.05f);
        }
      }

      if (render_model_markers) {
        short marker_index;

        for (marker_index = 0; marker_index < *(int *)(model + 0xac);
             marker_index++) {
          char *marker;
          short instance_index;

          marker = (char *)tag_block_get_element((void *)(model + 0xac),
                                                 marker_index, 0x40);
          for (instance_index = 0; instance_index < *(int *)(marker + 0x34);
               instance_index++) {
            unsigned char *instance;

            instance = (unsigned char *)tag_block_get_element(
              (void *)(marker + 0x34), instance_index, 0x20);
            /* MOVSX of the permutation byte vs MOVZX of instance[1]. */
            if (*((char *)region_permutation_indices + instance[0]) ==
                instance[1]) {
              real_matrix4x3 marker_matrix;

              component_vectors_from_normal3d((float *)&marker_matrix,
                                              (float *)(instance + 4),
                                              (float *)(instance + 0x10));
              matrix4x3_multiply(
                (float *)((char *)node_matrices + instance[2] * 0x34),
                (float *)&marker_matrix, (float *)&marker_matrix);
              FUN_001894d0(0, (float *)&marker_matrix, 0.05f);
              FUN_00189cb0(0, &marker_matrix.position, marker,
                           (int)global_real_argb_white);
            }
          }
        }
      }

      if (render_model_vertex_counts || render_model_index_counts) {
        short maximum_detail_level;
        short vertex_count;
        short index_count;
        short region_index;
        char has_unstripped_parts;
        float screen_offset;

        maximum_detail_level = detail_level;
        vertex_count = 0;
        index_count = 0;
        has_unstripped_parts = 0;
        for (region_index = 0; region_index < *(int *)(model + 0xc4);
             region_index++) {
          char *region;
          char permutation_index;

          region = (char *)tag_block_get_element((void *)(model + 0xc4),
                                                 region_index, 0x4c);
          permutation_index =
            *(char *)((char *)region_permutation_indices + region_index);
          if (permutation_index != -1) {
            char *permutation;
            short actual_detail_level;
            short geometry_index;

            permutation = (char *)tag_block_get_element(
              (void *)(region + 0x40), permutation_index, 0x58);
            actual_detail_level = detail_level + 1;
            while (actual_detail_level < 5 &&
                   *(short *)(permutation + 0x40 + actual_detail_level * 2) ==
                     *(short *)(permutation + 0x40 + detail_level * 2)) {
              actual_detail_level++;
            }
            if (actual_detail_level <= 0) {
              display_assert("actual_detail_level_index > 0",
                             "c:\\halo\\SOURCE\\models\\models.c", 0xf7, 1);
              system_exit(-1);
            }
            actual_detail_level--;
            if (actual_detail_level < 0 || actual_detail_level >= 5) {
              display_assert("(actual_detail_level_index >= 0) && "
                             "(actual_detail_level_index < "
                             "NUMBER_OF_DETAIL_LEVELS_PER_MODEL)",
                             "c:\\halo\\SOURCE\\models\\models.c", 0xf9, 1);
              system_exit(-1);
            }
            if (maximum_detail_level < actual_detail_level) {
              maximum_detail_level = actual_detail_level;
            }

            geometry_index = *(short *)(permutation + 0x40 + detail_level * 2);
            if (geometry_index != -1) {
              char *geometry;
              short part_index;

              geometry = (char *)tag_block_get_element((void *)(model + 0xd0),
                                                       geometry_index, 0x30);
              for (part_index = 0; part_index < *(int *)(geometry + 0x24);
                   part_index++) {
                char *part;

                part = (char *)tag_block_get_element((void *)(geometry + 0x24),
                                                     part_index, 0x68);
                vertex_count += *(short *)(part + 0x58);
                switch (*(short *)(part + 0x44)) {
                case 0:
                  index_count += *(short *)(part + 0x48) * 3;
                  has_unstripped_parts = 1;
                  break;
                case 1:
                  index_count += (unsigned short)(*(short *)(part + 0x48) + 2);
                  break;
                default:
                  display_assert("!\"unreachable\"",
                                 "c:\\halo\\SOURCE\\models\\models.c", 0x114,
                                 1);
                  system_exit(-1);
                  break;
                }
              }
            }
          }
        }

        screen_offset =
          (float)fabs(*(float *)0x5065c0 * *(float *)centroid +
                      *(float *)0x5065d8 * *((float *)centroid + 2) +
                      *(float *)0x5065cc * *((float *)centroid + 1) +
                      *(float *)0x5065e4) /
          *(float *)0x50672c * distance * 0.5f;
        if (screen_offset > 0.0001f) {
          void *color;
          char text[256];

          {
            void *detail_colors[5];

            detail_colors[0] = *(void **)0x2ee6d8;
            detail_colors[1] = *(void **)0x2ee6d4;
            detail_colors[2] = *(void **)0x2ee6e0;
            detail_colors[3] = global_real_argb_orange;
            detail_colors[4] = *(void **)0x2ee6d0;
            color = detail_colors[maximum_detail_level];
          }
          if (has_unstripped_parts && (game_time_get() + model_ref) % 30 < 15) {
            color = global_real_argb_white;
          }

          csstrcpy(text, "");
          if (render_model_vertex_counts) {
            snprintf(text + csstrlen(text), 0x100 - csstrlen(text), "%d",
                     vertex_count);
            if (render_model_vertex_counts && render_model_index_counts) {
              snprintf(text + csstrlen(text), 0x100 - csstrlen(text), "/");
            }
          }
          if (render_model_index_counts) {
            snprintf(text + csstrlen(text), 0x100 - csstrlen(text), "%d",
                     index_count);
          }

          {
            float point[3];

            point[0] = *(float *)centroid;
            point[1] = *((float *)centroid + 1);
            point[2] = screen_offset + *((float *)centroid + 2);
            FUN_00189cb0(0, point, text, (int)color);
          }
        }
      }
    }

    parameters.unique_identifier = object_handle;
    parameters.lighting = *(render_model_lighting_bytes *)lighting;
    parameters.centroid[0] = *(float *)centroid;
    parameters.centroid[1] = *((float *)centroid + 1);
    parameters.centroid[2] = *((float *)centroid + 2);
    parameters.radius = *(float *)&radius;
    parameters.effect = *(render_model_effect_bytes *)model_effect;
    parameters.animation.colors = change_colors;
    parameters.animation.values = function_values;
    parameters.skinning.node_matrices = relative_node_matrices;
    parameters.skinning.node_matrix_count = *(short *)(model + 0xb8);
    parameters.base_map_scale[0] = *(float *)(model + 0x30);
    parameters.base_map_scale[1] = *(float *)(model + 0x34);

    geometry_flags = 0;
    if (flags & 1) {
      geometry_flags = 0x1f;
    }
    if (flags & 4) {
      geometry_flags |= 0x40;
    } else {
      geometry_flags &= ~0x40;
    }
    if (flags & 8) {
      geometry_flags |= 0x80;
    } else {
      geometry_flags &= ~0x80;
    }
    parameters.geometry_flags = geometry_flags;

    if (flags & 2) {
      FUN_0017ccc0((int)&parameters);
    } else {
      FUN_0017cbb0(&parameters, 0);
    }
    render_model_parts((int)model, (int)region_permutation_indices,
                       (int *)&parameters.skinning, object_handle, detail_level,
                       (short)forced_shader_permutation_index,
                       (unsigned char)flags);
    if (flags & 2) {
      rasterizer_environment_shadow_model_end();
    } else {
      rasterizer_model_end();
    }
  }

  *(char *)0x5a5570 = 0;
  if (profile_global_enable && render_model_profile_section_active) {
    profile_exit_private(render_model_profile_section);
  }
}

/* -----------------------------------------------------------------------
 * overlay_animation_apply_continuous_scaled — animation 1D overlay frame apply
 *
 * For animation type 1 (overlay), interpolates rotation, translation, and
 * scale between the current frame and next frame for each node.  The result
 * is blended into the node output buffer using blend_weight.
 *
 * Disassembly range: 0x122a50 – 0x122e43.
 * Source: c:\halo\SOURCE\models\model_animations.c
 * ----------------------------------------------------------------------- */
void overlay_animation_apply_continuous_scaled(int animation, float frame_pos, float blend_weight,
                  int node_output)
{
  short *data;
  short *next_data;
  unsigned short frame_count_u;
  int compressed;
  short *data_cursor;
  short *next_cursor;
  int frame_index;
  int next_frame_index;
  int node_output_ptr;
  float weight_complement;
  float frac;
  float floor_val;
  int frame_idx_int;
  int rotation_counter;
  int translation_counter;
  int scale_counter;
  unsigned int node_idx;
  unsigned int has_translation;
  unsigned int has_rotation;
  unsigned int has_scale;
  float rot_a[4];
  float rot_b[4];
  float interp_rot[4];
  float interp_trans[3];
  float interp_scale;
  int temp_scale_a;
  int temp_scale_b;

  weight_complement = *(float *)0x2533c8 - blend_weight;
#if defined(_MSC_VER) && !defined(__clang__)
  frac = (float)fmod((double)frame_pos, *(const double *)0x2573d8);
#else
  frac = (float)x87_fmod(frame_pos, *(const double *)0x2573d8);
#endif
  floor_val = (float)floor((double)frame_pos);
  frame_idx_int = x87_round_to_int(floor_val);

  if (frame_pos < *(float *)0x2533c0 ||
      (float)((animation_t *)animation)->frame_count < frame_pos) {
    error(
      2,
      "### ERROR animation frame index out of bounds B(%f,%x) -- tell Bernie!!",
      (double)frame_pos, *(int *)&frame_pos);
  }

  frame_count_u = (unsigned short)((animation_t *)animation)->frame_count;
  if ((short)frame_idx_int >= (short)frame_count_u) {
    frame_idx_int = (int)(unsigned short)(frame_count_u - 1);
    frac = 1.0f;
    frame_pos = (float)(int)(short)frame_idx_int;
  }

  if (((animation_t *)animation)->field_20 == 1) {
    if ((((animation_t *)animation)->field_3a & 1) == 0 ||
        (model_animation_compression == '\0' && *(int *)(animation + 0x88) != 0)) {
      compressed = 0;
    } else {
      compressed = 1;
    }

    frame_index = (int)(short)frame_idx_int;
    if (frame_index == (int)(short)frame_count_u - 1) {
      next_frame_index = 0;
    } else {
      next_frame_index = frame_index + 1;
    }

    data = (short *)FUN_00120500((void *)animation, (short)frame_idx_int);
    next_data =
      (short *)FUN_00120500((void *)animation, (short)next_frame_index);

    rotation_counter = 0;
    translation_counter = 0;
    scale_counter = 0;
    node_idx = 0;
    if (0 < ((animation_t *)animation)->field_2c) {
      do {
        node_output_ptr = (short)node_idx * 0x20 + node_output;
        if ((node_idx & 0x1f) == 0) {
          int bit_idx = (int)(short)((short)node_idx >> 5);
          has_translation = *(unsigned int *)(animation + 0x5c + bit_idx * 4);
          has_rotation = *(unsigned int *)(animation + 0x6c + bit_idx * 4);
          has_scale = *(unsigned int *)(animation + 0x7c + bit_idx * 4);
        }

        data_cursor = data;
        next_cursor = next_data;

        if ((has_rotation & 1) != 0) {
          if (compressed) {
            FUN_00121330((void *)animation, (float)frame_index,
                         (unsigned short)rotation_counter, (short)node_idx,
                         interp_rot);
            rotation_counter = rotation_counter + 1;
          } else {
            rot_a[0] = (float)data[0] * *(float *)0x290dd8;
            rot_a[1] = (float)data[1] * *(float *)0x290dd8;
            rot_a[2] = (float)data[2] * *(float *)0x290dd8;
            rot_a[3] = (float)data[3] * *(float *)0x290dd8;
            data += 4;
            rot_b[0] = (float)next_data[0] * *(float *)0x290dd8;
            rot_b[1] = (float)next_data[1] * *(float *)0x290dd8;
            rot_b[2] = (float)next_data[2] * *(float *)0x290dd8;
            rot_b[3] = (float)next_data[3] * *(float *)0x290dd8;
            next_data += 4;
            quaternions_interpolate_and_normalize(rot_a, rot_b, frac,
                                                  interp_rot);
          }
          quaternions_interpolate_and_normalize(*(float **)0x31fc5c, interp_rot,
                                                blend_weight, interp_rot);
          FUN_0010b9c0(interp_rot, (float *)node_output_ptr,
                       (float *)node_output_ptr);
          data_cursor = data;
        }
        has_rotation = has_rotation >> 1;
        next_cursor = next_data;
        data = data_cursor;

        if ((has_translation & 1) != 0) {
          if (compressed) {
            animation_get_node_orientations((void *)animation, frame_pos,
                                            (unsigned short)translation_counter,
                                            (short)node_idx, interp_trans);
            translation_counter = translation_counter + 1;
          } else {
            points_interpolate((float *)data_cursor, (float *)next_cursor, frac,
                               interp_trans);
            data = data_cursor + 6;
            next_data = next_cursor + 6;
          }
          *(float *)(node_output_ptr + 0x10) =
            interp_trans[0] * blend_weight + *(float *)(node_output_ptr + 0x10);
          *(float *)(node_output_ptr + 0x14) =
            interp_trans[1] * blend_weight + *(float *)(node_output_ptr + 0x14);
          *(float *)(node_output_ptr + 0x18) =
            interp_trans[2] * blend_weight + *(float *)(node_output_ptr + 0x18);
        }
        has_translation = has_translation >> 1;

        if ((has_scale & 1) != 0) {
          if (compressed) {
            animation_get_keyframe_scale(
              (void *)animation, frame_pos, (unsigned short)scale_counter,
              (short)node_idx, &interp_scale);
            scale_counter = scale_counter + 1;
          } else {
            temp_scale_a = *(int *)data;
            temp_scale_b = *(int *)next_data;
            data = data + 2;
            next_data = next_data + 2;
            scalars_interpolate(*(float *)&temp_scale_a,
                                *(float *)&temp_scale_b, frac, &interp_scale);
          }
          *(float *)(node_output_ptr + 0x1c) =
            (interp_scale * blend_weight + weight_complement) *
            *(float *)(node_output_ptr + 0x1c);
        }
        has_scale = has_scale >> 1;
        node_idx = node_idx + 1;
      } while ((short)node_idx < ((animation_t *)animation)->field_2c);
    }

    if (!compressed) {
      int check_base;
      check_base = (int)FUN_00120500((void *)animation, (short)frame_idx_int);
      if ((int)data - check_base != (int)((animation_t *)animation)->frame_size) {
        display_assert(
          "compressed || ((byte *)data-(byte *)animation_get_frame_data"
          "(animation, frame_index)==animation->frame_size)",
          "c:\\halo\\SOURCE\\models\\model_animations.c", 0x334, 1);
        system_exit(-1);
      }
      check_base =
        (int)FUN_00120500((void *)animation, (short)next_frame_index);
      if ((int)next_data - check_base != (int)((animation_t *)animation)->frame_size) {
        display_assert("compressed || ((byte *)next_data-(byte *)"
                       "animation_get_frame_data(animation, next_frame_index)"
                       "==animation->frame_size)",
                       "c:\\halo\\SOURCE\\models\\model_animations.c", 0x335,
                       1);
        system_exit(-1);
      }
    }
  }
}

/* -----------------------------------------------------------------------
 * aiming_screen_apply — animation 2D blend
 *
 * Performs 2D bilinear interpolation of 4 corner animation frames,
 * blending rotation (quaternion slerp) and translation for each node
 * based on direction and throttle parameters.
 *
 * Disassembly range: 0x122e50 – 0x123462.
 * Source: c:\halo\SOURCE\models\model_animations.c
 * ----------------------------------------------------------------------- */
void aiming_screen_apply(int animation, float *blend_params, float direction,
                  float throttle, int node_output)
{
  int direction_count;
  int throttle_count;
  int throttle_count_s;
  short dir_count_s;
  char is_compressed;
  float yaw_range;
  float ratio;
  int dir_frame;
  float dir_frac;
  unsigned short neg_dir_offset;
  unsigned short neg_thr_offset;
  int thr_frame;
  float thr_frac;
  short thr_s;
  short dir_s;
  int frame_00;
  int frame_10;
  int frame_01;
  int frame_11;
  short *data_00;
  short *data_10;
  short *data_01;
  short *data_11;
  int rotation_counter;
  unsigned int node_idx;
  int node_out_ptr;
  unsigned int has_translation;
  unsigned int has_rotation;
  int translation_counter;
  volatile float dir_complement;
  volatile float thr_complement;
  float rot_00[4];
  float rot_10[4];
  float rot_01[4];
  float rot_11[4];
  float blend_a[4];
  float blend_b[4];
  float final_rot[4];
  float trans_00[3];
  float trans_10[3];
  float trans_01[3];
  float trans_11[3];
  int temp_int;

  direction_count = (unsigned short)(*(short *)(blend_params + 2) +
                                     *(short *)((int)blend_params + 10)) +
                    1;
  throttle_count = (unsigned short)(*(short *)(blend_params + 5) +
                                    *(short *)((int)blend_params + 0x16)) +
                   1;

  if (((animation_t *)animation)->field_20 != 1) {
    return;
  }

  throttle_count_s = (int)(short)throttle_count;
  dir_count_s = (short)direction_count;
  if (dir_count_s * throttle_count_s > (int)((animation_t *)animation)->frame_count) {
    return;
  }

  is_compressed = FUN_00120620(animation);

  /* Direction axis */
  if (direction >= *(float *)0x2533c0) {
    yaw_range = blend_params[1];
  } else {
    yaw_range = blend_params[0];
  }
  if (yaw_range == *(float *)0x2533c0) {
    ratio = 0.0f;
  } else {
    ratio = direction / yaw_range;
  }

  dir_frame = (int)ratio;
  /* 0x122ed8: ratio is narrowed before fmod and frame conversion. */
  HALO_FLT_ROUNDTRIP(ratio);
  dir_frac = (float)x87_fmod(ratio, 1.0);
  /* 0x122ef5 stores before the negative check; later uses reload float32. */
  HALO_FLT_ROUNDTRIP(dir_frac);
  if (dir_frac < *(float *)0x2533c0) {
    dir_frame = dir_frame - 1;
    dir_frac = dir_frac + *(float *)0x2533c8;
  }

  neg_dir_offset = *(unsigned short *)((int)blend_params + 10);
  if ((short)dir_frame >= (short)neg_dir_offset) {
    dir_frame = (int)(unsigned short)(neg_dir_offset - 1);
    dir_frac = 1.0f;
  }

  neg_dir_offset = *(unsigned short *)(blend_params + 2);
  if ((int)(short)dir_frame < -(int)(short)neg_dir_offset) {
    dir_frame = -(int)(unsigned int)neg_dir_offset;
    dir_frac = 0.0f;
  }
  dir_frame = dir_frame + (unsigned int)neg_dir_offset;

  if (dir_frac < *(float *)0x2533c0 ||
      !(dir_frac < *(float *)0x2533c8 || dir_frac == *(float *)0x2533c8)) {
    csprintf((char *)0x5ab100, "d0==%f direction(%f) yaw_delta(%f,%f)",
             (double)dir_frac, (double)direction, (double)blend_params[0],
             (double)blend_params[1]);
    display_assert((char *)0x5ab100,
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0x365, 1);
    system_exit(-1);
  }

  /* Throttle axis */
  if (throttle >= *(float *)0x2533c0) {
    yaw_range = blend_params[4];
  } else {
    yaw_range = blend_params[3];
  }
  if (yaw_range == *(float *)0x2533c0) {
    ratio = 0.0f;
  } else {
    ratio = throttle / yaw_range;
  }

  thr_frame = (int)ratio;
  /* 0x122ff0: ratio is narrowed before fmod and frame conversion. */
  HALO_FLT_ROUNDTRIP(ratio);
  thr_frac = (float)x87_fmod(ratio, 1.0);
  /* 0x12300e stores before the negative check; later uses reload float32. */
  HALO_FLT_ROUNDTRIP(thr_frac);
  if (thr_frac < *(float *)0x2533c0) {
    thr_frame = thr_frame - 1;
    thr_frac = thr_frac + *(float *)0x2533c8;
  }

  neg_thr_offset = *(unsigned short *)((int)blend_params + 0x16);
  if ((short)thr_frame >= (short)neg_thr_offset) {
    thr_frame = (int)(unsigned short)(neg_thr_offset - 1);
    thr_frac = 1.0f;
  }

  neg_thr_offset = *(unsigned short *)(blend_params + 5);
  if ((int)(short)thr_frame < -(int)(short)neg_thr_offset) {
    thr_frame = -(int)(unsigned int)neg_thr_offset;
    thr_frac = 0.0f;
  }
  thr_frame = thr_frame + (unsigned int)neg_thr_offset;

  thr_s = (short)thr_frame;
  dir_s = (short)dir_frame;

  if (thr_s < 0 || thr_s >= (short)throttle_count || dir_s < 0 ||
      dir_s >= dir_count_s) {
    return;
  }

  {
    int next_dir;
    int next_thr;
    next_dir = (int)(short)dir_s + 1;
    if (next_dir == (int)(short)dir_count_s) {
      next_dir = (int)(short)dir_s;
    }
    next_thr = (int)(short)thr_s + 1;
    if (next_thr == throttle_count_s) {
      next_thr = (int)(short)thr_s;
    }

    frame_00 = thr_frame * direction_count + dir_frame;
    frame_10 = thr_frame * direction_count + next_dir;
    frame_01 = dir_frame + next_thr * direction_count;
    frame_11 = next_thr * direction_count + next_dir;

    data_00 = (short *)FUN_00120500((void *)animation, (short)frame_00);
    data_10 = (short *)FUN_00120500((void *)animation, (short)frame_10);
    data_01 = (short *)FUN_00120500((void *)animation, (short)frame_01);
    data_11 = (short *)FUN_00120500((void *)animation, (short)frame_11);

    translation_counter = 0;
    node_idx = 0;
    rotation_counter = 0;

    if (0 >= ((animation_t *)animation)->field_2c) {
      return;
    }

    do {
      node_out_ptr = (short)node_idx * 0x20 + node_output;
      if ((node_idx & 0x1f) == 0) {
        int bit_idx = (int)(short)((short)node_idx >> 5);
        has_translation = *(unsigned int *)(animation + 0x5c + bit_idx * 4);
        has_rotation = *(unsigned int *)(animation + 0x6c + bit_idx * 4);
      }

      if ((has_rotation & 1) != 0) {
        if (is_compressed != '\0') {
          temp_int = (int)(short)frame_00;
          FUN_00121330((void *)animation, (float)temp_int,
                       (unsigned short)rotation_counter, (short)node_idx,
                       rot_00);
          temp_int = (int)(short)frame_10;
          FUN_00121330((void *)animation, (float)temp_int,
                       (unsigned short)rotation_counter, (short)node_idx,
                       rot_10);
          temp_int = (int)(short)frame_01;
          FUN_00121330((void *)animation, (float)temp_int,
                       (unsigned short)rotation_counter, (short)node_idx,
                       rot_01);
          temp_int = (int)(short)frame_11;
          FUN_00121330((void *)animation, (float)temp_int,
                       (unsigned short)rotation_counter, (short)node_idx,
                       rot_11);
          rotation_counter = rotation_counter + 1;
        } else {
          quaternion_decompress_8byte(data_00, rot_00);
          data_00 = data_00 + 4;
          quaternion_decompress_8byte(data_10, rot_10);
          data_10 = data_10 + 4;
          quaternion_decompress_8byte(data_01, rot_01);
          data_01 = data_01 + 4;
          quaternion_decompress_8byte(data_11, rot_11);
          data_11 = data_11 + 4;
        }
        quaternions_interpolate_and_normalize(rot_00, rot_10, dir_frac,
                                              blend_a);
        quaternions_interpolate_and_normalize(rot_01, rot_11, dir_frac,
                                              blend_b);
        quaternions_interpolate_and_normalize(blend_a, blend_b, thr_frac,
                                              final_rot);
        FUN_0010b9c0(final_rot, (float *)node_out_ptr, (float *)node_out_ptr);
      }
      has_rotation = has_rotation >> 1;

      if ((has_translation & 1) != 0) {
        dir_complement = *(float *)0x2533c8 - dir_frac;
        /* 0x1232d0: blend weight is reloaded after a float32 store. */
        thr_complement = *(float *)0x2533c8 - thr_frac;
        /* 0x1232dc: blend weight is reloaded after a float32 store. */

        if (is_compressed != '\0') {
          temp_int = (int)(short)frame_00;
          animation_get_node_orientations((void *)animation, (float)temp_int,
                                          (unsigned short)translation_counter,
                                          (short)node_idx, trans_00);
          temp_int = (int)(short)frame_10;
          animation_get_node_orientations((void *)animation, (float)temp_int,
                                          (unsigned short)translation_counter,
                                          (short)node_idx, trans_10);
          temp_int = (int)(short)frame_01;
          animation_get_node_orientations((void *)animation, (float)temp_int,
                                          (unsigned short)translation_counter,
                                          (short)node_idx, trans_01);
          temp_int = (int)(short)frame_11;
          animation_get_node_orientations((void *)animation, (float)temp_int,
                                          (unsigned short)translation_counter,
                                          (short)node_idx, trans_11);
          translation_counter = translation_counter + 1;
        } else {
          trans_00[0] = *(float *)data_00;
          trans_00[1] = *((float *)data_00 + 1);
          trans_00[2] = *((float *)data_00 + 2);
          data_00 = data_00 + 6;
          trans_10[0] = *(float *)data_10;
          trans_10[1] = *((float *)data_10 + 1);
          trans_10[2] = *((float *)data_10 + 2);
          data_10 = data_10 + 6;
          trans_01[0] = *(float *)data_01;
          trans_01[1] = *((float *)data_01 + 1);
          trans_01[2] = *((float *)data_01 + 2);
          data_01 = data_01 + 6;
          trans_11[0] = *(float *)data_11;
          trans_11[1] = *((float *)data_11 + 1);
          trans_11[2] = *((float *)data_11 + 2);
          data_11 = data_11 + 6;
        }

        *(float *)(node_out_ptr + 0x10) =
          (trans_10[0] * dir_frac + trans_00[0] * dir_complement) *
            thr_complement +
          (trans_11[0] * dir_frac + trans_01[0] * dir_complement) * thr_frac +
          *(float *)(node_out_ptr + 0x10);
        *(float *)(node_out_ptr + 0x14) =
          (trans_10[1] * dir_frac + trans_00[1] * dir_complement) *
            thr_complement +
          (trans_11[1] * dir_frac + trans_01[1] * dir_complement) * thr_frac +
          *(float *)(node_out_ptr + 0x14);
        *(float *)(node_out_ptr + 0x18) =
          (trans_10[2] * dir_frac + trans_00[2] * dir_complement) *
            thr_complement +
          (trans_11[2] * dir_frac + trans_01[2] * dir_complement) * thr_frac +
          *(float *)(node_out_ptr + 0x18);
      }
      has_translation = has_translation >> 1;
      node_idx = node_idx + 1;
    } while ((short)node_idx < ((animation_t *)animation)->field_2c);
  }
}

/* animation_get_root_velocity (0x1234b0) - animation_get_root_delta
 *
 * Computes the delta position between frame param_3 and frame (param_3-1)
 * of an animation. Uses _chkstk for 0x1000 bytes of stack.
 * Two 0x800-byte node data buffers are filled via FUN_00121d60.
 * The translation component (offset 0x10 from each buffer base) is
 * subtracted to produce the frame delta in param_4[0..2].
 *
 * Confirmed: 4 cdecl params, void return. _chkstk 0x1000 frame.
 */
void animation_get_root_velocity(void *mode_tag, void *animation, int frame_index,
                  float *out_delta)
{
  uint8_t frame_data[0x800];
  uint8_t prev_frame_data[0x800];
  int frame;

  if (((animation_t *)animation)->frame_count < 2) {
    display_assert("animation->frame_count>1",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0xdd, true);
    system_exit(-1);
  }

  frame = frame_index;
  if ((short)frame == 0) {
    frame = 1;
  }

  FUN_00121d60(mode_tag, animation, frame, frame_data);
  FUN_00121d60(mode_tag, animation, frame - 1, prev_frame_data);

  out_delta[0] =
    *(float *)(frame_data + 0x10) - *(float *)(prev_frame_data + 0x10);
  out_delta[1] =
    *(float *)(frame_data + 0x14) - *(float *)(prev_frame_data + 0x14);
  out_delta[2] =
    *(float *)(frame_data + 0x18) - *(float *)(prev_frame_data + 0x18);
}
