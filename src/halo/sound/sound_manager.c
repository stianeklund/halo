/* FUN_001ac030 (0x1ac030)
 *
 * Sets or clears bit 0x10000 of the flags dword at (unit-type object)+0x1b4.
 * Bit meaning is UNKNOWN -- no assert/string evidence names it; kept as
 * FUN_ per naming-confidence rules. No-op when param_1 == -1 (skips both
 * the object lookup and the flag update).
 *
 * Sole caller unit_set_enterable_by_player_evaluate (players.c, HaloScript builtin dispatcher)
 * passes (record[0], zero-extended byte at record+4) and discards the
 * (void) return. */
void FUN_001ac030(int param_1, int param_2)
{
  char *obj;
  uint32_t flags;

  if (param_1 != -1) {
    obj = (char *)object_get_and_verify_type(param_1, 3);
    flags = *(uint32_t *)(obj + 0x1b4);

    if (param_2 == 0) {
      flags |= 0x10000;
    } else {
      flags &= ~0x10000u;
    }

    *(uint32_t *)(obj + 0x1b4) = flags;
  }
}

/* FUN_001ac070 (0x1ac070)
 *
 * Stores the low byte of param_2 into the byte field at (unit-type
 * object)+0x258, then forces the object's region count to 6.
 * Field meaning at +0x258 is UNKNOWN -- no assert/string evidence; the
 * offset is only observed written here.  No caller evidence available
 * (xrefs_to empty in the Ghidra bundle), so the source of param_2 is an
 * explicit unknown.  No-op when param_1 == -1.
 *
 * The reference's single ADD ESP,0x10 is merged cdecl cleanup for the two
 * 2-argument calls, not a 4-argument call. */
void FUN_001ac070(int param_1, int param_2)
{
  char *obj;

  if (param_1 != -1) {
    obj = (char *)object_get_and_verify_type(param_1, 3);
    *(unsigned char *)(obj + 0x258) = (unsigned char)param_2;
    object_set_region_count(param_1, 6);
  }
}

/* FUN_001ac0a0 (0x1ac0a0)
 *
 * Sets or clears bit 0x4000 of the flags dword at (unit-type object)+0x1b4,
 * selected by the low byte of param_2 (non-zero sets, zero clears).  Bit
 * meaning is UNKNOWN -- no assert/string evidence names it; kept as FUN_
 * per naming-confidence rules.  No-op when param_1 == -1 (the reference
 * jumps straight to the epilogue, skipping the object lookup as well).
 *
 * Reference reads the selector as a byte (MOV CL,byte ptr [EBP+0xc];
 * TEST CL,CL) and loads the flags dword once before the branch, storing
 * from both arms.  The set path takes its OWN epilogue (MOV/POP EBP/RET at
 * 0x1ac0c9-0x1ac0d0); only the clear path falls into the shared bottom
 * epilogue that the param_1 == -1 guard's JZ also targets -- hence the
 * early return rather than a merged store.  The one call site in the XBE
 * (players.c, HaloScript builtin dispatcher; per check_arg_counts.py
 * --callee 0x1ac0a0: sites=1, push=2) passes a zero-extended byte, so the
 * kb decl stays int. */
void FUN_001ac0a0(int param_1, int param_2)
{
  char *obj;
  uint32_t flags;

  if (param_1 != -1) {
    obj = (char *)object_get_and_verify_type(param_1, 3);
    flags = *(uint32_t *)(obj + 0x1b4);

    if ((char)param_2 != 0) {
      *(uint32_t *)(obj + 0x1b4) = flags | 0x4000;
      return;
    }

    *(uint32_t *)(obj + 0x1b4) = flags & ~0x4000u;
  }
}

/* FUN_001AC0E0 (0x1ac0e0)
 *
 * Returns the number of animation frames remaining in a unit's currently
 * playing animation, clamped at zero.  Looks up the unit-type object, and
 * only proceeds when the byte at object+0x253 equals 0x1c (meaning of the
 * byte and of the 0x1c selector are UNKNOWN -- no assert/string evidence).
 * The object's 'antr' (model_animations) tag index lives at object+0x7c and
 * the animation index (signed 16-bit) at object+0x80; the animation block is
 * at antr_tag+0x74 with 0xb4-byte elements.  Frame count is the signed
 * 16-bit field at element+0x22, current frame the signed 16-bit field at
 * object+0x82.
 *
 * Binary evidence (0x1ac0e0-0x1ac14c, 41 instructions, cdecl, no FPU):
 *   CMP EAX,-0x1 / JZ epilogue -> handle == -1 returns 0 (XOR AX,AX; the
 *   result is 16-bit in AX, which is why the decl returns int16_t).
 *   MOV AL,byte ptr [ESI+0x253] / CMP AL,0x1c / JNZ epilogue -- signed byte
 *   compare, and the object pointer is dereferenced unguarded (the reference
 *   does NOT null-check object_get_and_verify_type).
 *   PUSH EAX([ESI+0x7c]) / PUSH 0x616e7472 -> tag_get('antr', tag_index).
 *   PUSH 0xb4 / PUSH ECX(MOVSX [ESI+0x80]) / PUSH EAX(tag+0x74) ->
 *   tag_block_get_element(block, index, 0xb4).
 *   MOVSX EAX,[EAX+0x22] / MOVSX EDX,[ESI+0x82] / SUB EAX,EDX / ADD EAX,-0x2
 *   then XOR ECX,ECX / TEST EAX,EAX / SETLE CL / DEC ECX / AND EAX,ECX --
 *   the branchless max(count, 0).
 *   One combined ADD ESP,0x14 (5 dwords) after the tag_block_get_element
 *   CALL folds that call's 3 args with tag_get's 2 (adjacent-call cleanup);
 *   the ARG_COUNT warning on 0x19b210 ("cleanup=5 vs decl=3") is that merge
 *   -- tag_block_get_element really takes 3 args, do NOT "fix" its decl.
 *
 * Sole caller unit_get_custom_animation_time_evaluate (players.c, HaloScript builtin dispatcher)
 * zero-extends the 16-bit result and forwards it to hs_return. */
int16_t FUN_001AC0E0(int handle)
{
  char *obj;
  char *antr;
  char *element;
  int count;

  if (handle != -1) {
    obj = (char *)object_get_and_verify_type(handle, 3);

    if (*(char *)(obj + 0x253) == 0x1c) {
      antr = (char *)tag_get(0x616e7472, *(int *)(obj + 0x7c));
      element = (char *)tag_block_get_element(
        antr + 0x74, (int)*(int16_t *)(obj + 0x80), 0xb4);

      count =
        (int)*(int16_t *)(element + 0x22) - (int)*(int16_t *)(obj + 0x82) - 2;
      return (int16_t)(count > 0 ? count : 0);
    }
  }

  return 0;
}

/* FUN_001ac150 (0x1ac150)
 *
 * Reports whether a unit-type object's byte at object+0x253 equals 0x1c.
 * The byte's meaning and the 0x1c selector are UNKNOWN -- no assert/string
 * evidence names them; the same test gates FUN_001AC0E0 just above.
 *
 * Binary evidence (0x1ac150-0x1ac17b, 14 instructions, cdecl, no FPU):
 *   MOV EAX,[EBP+0x8] / CMP EAX,-0x1 / JZ 0x1ac178 -> handle == -1 takes
 *   the XOR AL,AL exit (returns 0) without calling anything.
 *   PUSH 0x3 / PUSH EAX / CALL 0x13d680 (object_get_and_verify_type) with
 *   ADD ESP,0x8 (cdecl, 2 args).
 *   MOV DL,byte ptr [EAX+0x253] / CMP DL,0x1c / SETZ CL / MOV AL,CL --
 *   signed byte compare, the object pointer is dereferenced unguarded (the
 *   reference does NOT null-check object_get_and_verify_type); the result
 *   is materialized in AL, hence the unsigned char return. */
unsigned char FUN_001ac150(int handle)
{
  char *obj;

  if (handle != -1) {
    obj = (char *)object_get_and_verify_type(handle, 3);
    return (unsigned char)(*(char *)(obj + 0x253) == 0x1c);
  }

  return 0;
}

/* sound_object_apply_pitch_delta (0x1ac2f0)
 *
 * Computes a clamped pitch delta and accumulates it onto the object's
 * pitch field at offset 0x298.  The incoming pitch is differenced against
 * the current stored value; the resulting delta is clamped to [-0.3, 0.3]
 * before being added back to the stored pitch. */
void sound_object_apply_pitch_delta(int object_handle, float pitch)
{
  char *obj;
  float *stored_pitch;
  float delta;

  obj = (char *)object_get_and_verify_type(object_handle, 3);
  stored_pitch = (float *)(obj + 0x298);
  delta = pitch - *stored_pitch;

  if (delta < -0.3f) {
    delta = -0.3f;
  } else if (delta > 0.3f) {
    delta = 0.3f;
  }

  *stored_pitch += delta;
}

/* FUN_001ac3b0 (0x1ac3b0)
 *
 * Returns non-zero when the given weapon handle occupies one of the unit's
 * four weapon-handle slots at +0x2a8 (0x2a8 + index*4).
 *
 * Reference verifies the object as type 3 (unit) via
 * object_get_and_verify_type(handle, 3) at 0x1ac3ba, then walks EDX from
 * EAX+0x2a8 comparing dword [EDX] against the second argument (EBP+0xc).
 * Loop is a do/while: CMP at 0x1ac3d0, INC/ADD 4, CMP ECX,4 / JL back.
 * Found exit is MOV AL,1; the fallthrough exit moves BL (zeroed by
 * XOR BL,BL at 0x1ac3c2) into AL, i.e. returns 0. */
char FUN_001ac3b0(volatile int unit_handle, int weapon_handle)
{
  char *unit;
  int *slot;
  int index;
  char result;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  result = 0;
  slot = (int *)(unit + 0x2a8);
  index = 0;
  do {
    if (*slot == weapon_handle) {
      result = 1;
      break;
    }
    index++;
    slot++;
  } while (index < 4);

  return result;
}

/* sound_cache_sound_finished (0x1be090)
 *
 * Decrements the software_reference_count on a cache-sound entry when a
 * sound permutation finishes playing. Takes the permutation tag pointer,
 * reads the cache handle from offset +0x2c, and looks up the corresponding
 * cache-sound datum from the sound cache table at 0x4e9368.
 *
 * If the debug trace flag at 0x5054ec is set, logs the current reference
 * count and name before decrementing.
 *
 * Asserts that software_reference_count is nonzero before decrementing
 * (source: c:\halo\SOURCE\cache\xbox_sound_cache.c, line 263). */
void sound_cache_sound_finished(int permutation_ptr)
{
  char *cache_sound;
  int cache_handle;

  cache_handle = *(int *)(permutation_ptr + 0x2c);
  cache_sound = (char *)datum_get(*(data_t **)0x4e9368, cache_handle);

  if (*(uint8_t *)0x5054ec != 0) {
    error(2, "--- finish %d %s", (int)*(uint8_t *)(cache_sound + 4),
          *(char **)(cache_sound + 8));
  }

  if (*(uint8_t *)(cache_sound + 4) == 0) {
    display_assert("cache_sound->software_reference_count > 0",
                   "c:\\halo\\SOURCE\\cache\\xbox_sound_cache.c", 0x107, 1);
    system_exit(-1);
  }

  *(uint8_t *)(cache_sound + 4) -= 1;
}

/* FUN_001be100 (0x1be100)
 *
 * Hardware-reference counterpart of sound_cache_sound_finished: bumps the
 * hardware_reference_count on the cache-sound datum a permutation plays out
 * of. Takes the permutation tag pointer, reads the cache handle from +0x2c
 * and looks the datum up in the sound cache table at 0x4e9368 (same lookup
 * shape as 0x1be090).
 *
 * The count is a single byte (MOV CL,byte ptr [EAX + 0x5]) and the compare is
 * unsigned (CMP CL,0xff / JNC 0x1be128), so the increment saturates: while the
 * datum's own count is below 0xff it is incremented in place, otherwise the
 * 16-bit global at 0x5054ea is incremented instead (INC word ptr [0x5054ea];
 * xbox_sound_cache.c names that global the hardware sound reference count).
 * Whether 0xff is a saturation limit or a NONE marker is unproven -- 0xff is
 * the only byte value the unsigned compare admits either way. */
void FUN_001be100(int permutation_ptr)
{
  char *cache_sound;
  int cache_handle;

  cache_handle = *(int *)(permutation_ptr + 0x2c);
  cache_sound = (char *)datum_get(*(data_t **)0x4e9368, cache_handle);

  if (*(uint8_t *)(cache_sound + 5) < 0xff) {
    *(uint8_t *)(cache_sound + 5) += 1;
  } else {
    *(int16_t *)0x5054ea += 1;
  }
}

/* FUN_001be140 (0x1be140)
 *
 * Release counterpart of FUN_001be100: drops the hardware reference the
 * permutation holds on its cache-sound datum. Takes the permutation tag
 * pointer, reads the cache handle from +0x2c and looks the datum up in the
 * sound cache table at 0x4e9368 (same lookup shape as 0x1be090/0x1be100).
 *
 * The count is a single byte (MOV CL,byte ptr [EAX + 0x5]) and the test is
 * TEST CL,CL / JZ, so when the datum's own count is non-zero it is
 * decremented in place; when it is already zero the 16-bit global at
 * 0x5054ea is incremented instead (INC word ptr [0x5054ea]) -- the same
 * global FUN_001be100 bumps on saturation. The meaning of that global
 * counter is unproven beyond the name xbox_sound_cache.c gives it.
 *
 * No assert on underflow here (unlike the software path at 0x1be090). */
void FUN_001be140(int permutation_ptr)
{
  char *cache_sound;
  int cache_handle;

  cache_handle = *(int *)(permutation_ptr + 0x2c);
  cache_sound = (char *)datum_get(*(data_t **)0x4e9368, cache_handle);

  if (*(uint8_t *)(cache_sound + 5) != 0) {
    *(uint8_t *)(cache_sound + 5) -= 1;
  } else {
    *(int16_t *)0x5054ea += 1;
  }
}

/* FUN_001be170 (0x1be170)
 *
 * LRU-V block-query callback for the Xbox sound cache: sound_cache_new hands
 * this function to lruv_new as the second callback (see
 * cache/xbox_sound_cache.c:0x49), so the argument is a cache block index,
 * which in this cache is also the cache-sound datum index (FUN_001be2b0
 * asserts new_cache_sound_index==cache_block_index).
 *
 * Looks the datum up in the sound cache table at 0x4e9368 (same lookup shape
 * as 0x1be090/0x1be100/0x1be140) and returns 0 only when +0x02 is non-zero
 * and both reference counts are zero -- +0x04 is the software reference count
 * and +0x05 the hardware reference count (named in xbox_sound_cache.c), and
 * +0x02 is the byte FUN_001be2b0 hands to cache_file_read as the read
 * completion flag. Every other state returns 1.
 *
 * Which polarity the caller wants (0 = "may evict" vs 1 = "may evict") is not
 * proven from this function alone; lruv's use of the callback is unlifted.
 * The three byte loads are TEST CL,CL on byte ptr operands, and the returns
 * are a full-dword XOR EAX,EAX / MOV EAX,1, so the result is an int, not a
 * bool in AL. */
int FUN_001be170(int cache_block_index)
{
  char *cache_sound;

  cache_sound = (char *)datum_get(*(data_t **)0x4e9368, cache_block_index);

  if (*(uint8_t *)(cache_sound + 2) != 0 &&
      *(uint8_t *)(cache_sound + 4) == 0 &&
      *(uint8_t *)(cache_sound + 5) == 0) {
    return 0;
  }

  return 1;
}

/* FUN_001be1b0 (0x1be1b0)
 *
 * LRU-V block-delete callback for the Xbox sound cache: sound_cache_new hands
 * this function to lruv_new as the first callback (cast at
 * cache/xbox_sound_cache.c:122), so the stack argument is a cache block index
 * -- which in this cache is also the cache-sound datum index (FUN_001be2b0
 * asserts new_cache_sound_index==cache_block_index).  The kb decl was
 * void(void); the reference reads MOV EDI,dword ptr [EBP+0x8] at 0x1be1ba and
 * cleanup is caller-side (plain RET), so it is a single cdecl int parameter.
 *
 * Refuses to evict a block that is still referenced: +0x04 is the software
 * reference count and +0x05 the hardware reference count (same fields
 * FUN_001be170 tests), and a non-zero count reports
 * "tried to delete sound %s(%s) from the cache while it was playing."
 * (xbox_sound_cache.c line 0x141) and halts.
 *
 * Otherwise it asserts cache_sound->sound->cache_block_index==block_index
 * (line 0x144), clears the owning record's cache-block index (+0x2c = -1) and
 * cache page address (+0x30 = 0), then frees the datum.  +0x08 is the owning
 * sound-permutation record FUN_001be2b0 stored there, and +0x3c on that record
 * is the tag index handed to tag_get_name.
 *
 * The reference re-loads [ESI+0x8] before each of the three uses
 * (0x1be216 / 0x1be23e / 0x1be248), so the loads are left uncached here.
 *
 * The second %s consumes the record pointer itself (PUSH EAX at 0x1be1e9,
 * offset 0), so offset 0 of that record is most likely an embedded name
 * string -- UNPROVEN from this function, hence the raw pointer. */
void FUN_001be1b0(int block_index)
{
  char *cache_sound;

  cache_sound = (char *)datum_get(*(data_t **)0x4e9368, block_index);

  if (*(uint8_t *)(cache_sound + 4) != 0 ||
      *(uint8_t *)(cache_sound + 5) != 0) {
    display_assert(
      csprintf((char *)0x5ab100,
               "tried to delete sound %s(%s) from the cache while it was "
               "playing.",
               tag_get_name(*(int *)(*(char **)(cache_sound + 8) + 0x3c)),
               *(char **)(cache_sound + 8)),
      "c:\\halo\\SOURCE\\cache\\xbox_sound_cache.c", 0x141, 1);
    system_exit(-1);
  }

  if (*(int *)(*(char **)(cache_sound + 8) + 0x2c) != block_index) {
    display_assert("cache_sound->sound->cache_block_index==block_index",
                   "c:\\halo\\SOURCE\\cache\\xbox_sound_cache.c", 0x144, 1);
    system_exit(-1);
  }

  *(int *)(*(char **)(cache_sound + 8) + 0x2c) = -1;
  *(int *)(*(char **)(cache_sound + 8) + 0x30) = 0;
  datum_delete(*(data_t **)0x4e9368, block_index);
}

/* FUN_001be270 (0x1be270)
 *
 * Name-formatting callback for the Xbox sound cache LRU dump: FUN_001be2b0
 * hands this function's address to FUN_0011db90 as the last argument
 * (PUSH 0x1be270 in the "SOUND CACHE BLOWN" cold path), so the single cdecl
 * stack argument (MOV EAX,dword ptr [EBP+0x8] at 0x1be273) is a cache block
 * index -- which in this cache is also the cache-sound datum index.
 *
 * The kb decl was void(void); the reference both reads [EBP+0x8] and ends
 * with MOV EAX,0x4e9268 / POP EBP / RET, so it takes one int and returns the
 * static formatting buffer at 0x4e9268.
 *
 * Same record fields the sibling callbacks use: +0x08 on the cache-sound
 * datum is the owning sound-permutation record, and +0x3c on that record is
 * the tag index handed to tag_get_name.  The second %s consumes the record
 * pointer itself (the PUSH EAX at 0x1be28c survives the ADD ESP,0x4 after
 * tag_get_name and is sprintf's last vararg), so offset 0 of that record is
 * most likely an embedded name string -- UNPROVEN, hence the raw pointer,
 * exactly as in FUN_001be1b0.
 *
 * [EAX+0x8] is loaded once and reused for both the +0x3c load and the vararg
 * push (0x1be283 / 0x1be286 / 0x1be28c), so the load is cached in a local. */
char *FUN_001be270(int cache_block_index)
{
  char *cache_sound;
  char *sound;

  cache_sound = (char *)datum_get(*(data_t **)0x4e9368, cache_block_index);
  sound = *(char **)(cache_sound + 8);
  crt_sprintf((char *)0x4e9268, "%s (%s)", tag_get_name(*(int *)(sound + 0x3c)),
              sound);
  return (char *)0x4e9268;
}

/* FUN_001be2b0 (0x1be2b0)
 *
 * Sound-cache counterpart of xbox_texture_cache_request: reserves an LRU
 * cache block for a pending sound-permutation request (record passed in ESI)
 * and starts the asynchronous read into that block.
 *
 * Request record offsets (confirmed from the disassembly at 0x1be2b6 ff.):
 *   +0x2c  cache block index   (written on success)
 *   +0x30  cache page address  (written on success)
 *   +0x34  cache file index    -> cache_file_read param_1
 *   +0x40  requested size      -> lruv allocation size and read size
 *   +0x48  file offset         -> cache_file_read offset
 *
 * On allocation failure it reports "SOUND CACHE BLOWN" and dumps the LRU
 * state to d:\stabbed.txt, at most once per 10 seconds (last-report
 * timestamp at 0x4e9374); the cache-sound datum is left untouched.
 *
 * Asserts new_cache_sound_index==cache_block_index
 * (c:\halo\SOURCE\cache\xbox_sound_cache.c line 0x170).
 *
 * The cache_file_read return value is genuinely discarded here (unlike the
 * texture path, which stores it at entry+2); entry+2 is instead handed to
 * cache_file_read as the completion flag. */
void FUN_001be2b0(char *request /* @<esi> */)
{
  int cache_block_index;
  int cache_page_index;
  int new_cache_sound_index;
  char *cache_sound;

  cache_block_index =
    FUN_0011de10(*(void **)0x4e9370, *(unsigned int *)(request + 0x40));
  if (cache_block_index != -1) {
    cache_page_index =
      lruv_block_get_address(*(void **)0x4e9370, cache_block_index) +
      *(int *)0x4e936c;
    new_cache_sound_index =
      data_new_datum(*(data_t **)0x4e9368, cache_block_index);
    cache_sound = (char *)datum_get(*(data_t **)0x4e9368, cache_block_index);

    if (new_cache_sound_index != cache_block_index) {
      display_assert("new_cache_sound_index==cache_block_index",
                     "c:\\halo\\SOURCE\\cache\\xbox_sound_cache.c", 0x170, 1);
      system_exit(-1);
    }

    *(int *)(request + 0x2c) = cache_block_index;
    *(int *)(request + 0x30) = cache_page_index;
    *(char **)(cache_sound + 8) = request;
    cache_file_read(*(int *)(request + 0x34), *(int *)(request + 0x48),
                    *(unsigned int *)(request + 0x40), cache_page_index,
                    cache_sound + 2, 0);
    return;
  }

  /* Cold path: MSVC lays the cache-blown reporting out after the RET. */
  if (system_milliseconds() - *(unsigned int *)0x4e9374 > 10000u) {
    terminal_printf(
      *(void **)0x2ee6f4,
      "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!",
      NULL);
    error(2, "SOUND CACHE BLOWN!!!! double-click \"GETSTABBED.BAT\" on your "
             "PC now!!!");
    terminal_printf(
      *(void **)0x2ee6f4,
      "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!",
      NULL);
    FUN_0011db90("d:\\stabbed.txt", request, *(int *)(request + 0x40),
                 *(void **)0x4e9370, (void *)0x18ef30, (void *)0x1be270);
    *(unsigned int *)0x4e9374 = system_milliseconds();
  }
}

/* sound_pitch_push_sample (0x1c7b00)
 *
 * Conditionally applies a pitch sample to an object's pitch-track field.
 * Only runs if the object looping sounds table (0x5054e4) is initialized
 * (byte +0x24 != 0).  Validates the object handle as type 3 (biped|vehicle)
 * via object_try_and_get_and_verify_type before forwarding to
 * sound_object_apply_pitch_delta (0x1ac2f0).
 *
 * noinline: the original is a real out-of-line call from sound_update_music
 * (direct CALL 0x1c7b00 at 0x1cf0d3).  Without this the optimizer inlines the
 * body into that caller and the call shape diverges. */
__declspec(noinline) void sound_pitch_push_sample(int object_handle,
                                                  float pitch)
{
  if (*(uint8_t *)(*(int *)0x5054e4 + 0x24) != 0) {
    if (object_try_and_get_and_verify_type(object_handle, 3) != 0) {
      sound_object_apply_pitch_delta(object_handle, pitch);
    }
  }
}

/* FUN_001c7b40 (0x1c7b40)
 *
 * Rebuilds the global per-cluster audibility bitfield at 0x5054a0 for the
 * current frame.  The table is first cleared to
 * ((clusters.count + 0x1f) >> 5) * 4 bytes (one bit per cluster, rounded up
 * to whole dwords).  Then, for each of the four local players that has a
 * player index (local_player_get_player_index != NONE) and whose observer
 * camera has a valid cluster (camera +0x10 != NONE), every cluster of the
 * structure BSP is tested: structure_bsp_get_cluster_encoded_sound_distance returns a
 * packed byte whose low 7 bits scale by 0x256148 into a distance which is
 * compared against the cutoff at 0x2642a0.  Clusters under the cutoff get
 * their bit set, so the bit is the union over all local cameras.
 *
 * Consumed by sound_cluster_is_audible (game_sound.c) which reads the same
 * table at 0x5054a0.
 *
 * Call-site evidence (0x1c7bb9): PUSH ECX (camera cluster, +0x10), PUSH EDI
 * (loop cluster index), PUSH EBX (bsp) -> (bsp, from_cluster = loop index,
 * to_cluster = camera cluster).  scenario_get() is called once outside both
 * loops and reused (EBX). */
void FUN_001c7b40(void)
{
  void *bsp;
  void *camera;
  int16_t local_player_index;
  int16_t cluster_index;
  int cluster;
  uint8_t encoding;

  bsp = scenario_get();
  csmemset((void *)0x5054a0, 0,
           (size_t)(((*(int *)((char *)bsp + 0x134) + 0x1f) >> 5) << 2));

  local_player_index = 0;
  do {
    if (local_player_get_player_index(local_player_index) != -1) {
      camera = observer_get_camera((unsigned short)local_player_index);
      if (*(int16_t *)((char *)camera + 0x10) != -1) {
        cluster_index = 0;
        cluster = 0;
        while (cluster < *(int *)((char *)bsp + 0x134)) {
          encoding = structure_bsp_get_cluster_encoded_sound_distance(
            bsp, cluster_index, *(int16_t *)((char *)camera + 0x10));
          if ((float)(int)(encoding & 0x7f) * *(float *)0x256148 <
              *(float *)0x2642a0) {
            ((uint32_t *)0x5054a0)[cluster >> 5] |=
              1u << ((uint8_t)cluster & 0x1f);
          }
          cluster_index = (int16_t)(cluster_index + 1);
          cluster = (int)cluster_index;
        }
      }
    }
    local_player_index = (int16_t)(local_player_index + 1);
  } while (local_player_index < 4);
}

/* sound_is_active (0x1c88a0)
 *
 * Byte-swaps one "bungie ima adpcm header" record in place.
 *
 * Disassembly (0x1c88a0-0x1c88b7) is a single forwarding call:
 *   MOV EAX,[EBP+8]; PUSH 1; PUSH EAX; PUSH 0x32ecf4;
 *   CALL 0x00118be0; ADD ESP,0xc
 * so the arguments are (definition = 0x32ecf4, data = param, count = 1).
 * 0x32ecf4 is the byte-swap definition whose name string is
 * "bungie ima adpcm header" (Ghidra: PTR_s_bungie_ima_adpcm_header_0032ecf4).
 *
 * The one stack argument at [EBP+8] is proven by the binary; the kb.json
 * placeholder decl carried (void).  RET (no imm) => cdecl, caller cleans.
 * The record layout behind `data` is unknown here, so it stays void *. */
void sound_is_active(void *data)
{
  FUN_00118be0((void *)0x32ecf4, data, 1);
}

/* Return a pointer to the sound class definition at class_index.
 * The definitions live in a static table at 0x32ed08 with a stride of 0x2c.
 * Originally inlined from sound_classes.h; compiled into sound_manager.obj.
 * Asserts that class_index is in range [0,0x33), the class name is non-empty,
 * and the per-definition and per-object instance limits are <= 0x10. */
void *sound_class_get_definition(short class_index)
{
  int idx = (int)class_index;
  void *definition = (void *)(0x32ed08 + idx * 0x2c);

  assert_halt(class_index >= 0 && class_index < 0x33);
  assert_halt(((const char **)0x32f5d0)[idx][0]);
  assert_halt(*(short *)definition <= 0x10);
  assert_halt(*(short *)((char *)definition + 2) <= 0x10);

  return definition;
}

/* Return the default priority for a sound tag (0x1c8d10).
 * Reads the priority field at tag offset 0xc. If zero, falls back to
 * the sound class definition's default priority at offset 0x1c. */
/* noinline (VC71 verification only): the original build emits this out of
 * line -- callers such as FUN_001cc4f0 (0x1cc4f0) CALL 0x1c8d10/0x1c8d50
 * rather than carrying an inlined copy of the fallback path. */
__declspec(noinline) float sound_get_default_priority(int sound_tag_index)
{
  void *sound_tag = tag_get(0x736e6421, sound_tag_index);
  float priority = *(float *)((char *)sound_tag + 0xc);

  if (priority == *(float *)0x2533c0) {
    void *class_def =
      sound_class_get_definition(*(short *)((char *)sound_tag + 0x4));
    return *(float *)((char *)class_def + 0x1c);
  }

  return priority;
}

/* Return the minimum-distance attenuation for a sound tag.
 * Reads the per-tag min_distance override at tag offset 0x8.
 * If it is zero, falls back to the min_distance field (offset 0x18)
 * of the sound class definition looked up via the tag's class_index
 * at offset 0x4. */
/* noinline (VC71 verification only): the original build emits this out of
 * line -- callers such as FUN_001cc4f0 (0x1cc4f0) CALL 0x1c8d10/0x1c8d50
 * rather than carrying an inlined copy of the fallback path. */
__declspec(noinline) float sound_class_get_min_distance(int sound_tag_index)
{
  void *sound_tag = tag_get(0x736e6421, sound_tag_index);
  float min_distance = *(float *)((char *)sound_tag + 0x8);

  if (min_distance == *(float *)0x2533c0) {
    void *class_def =
      sound_class_get_definition(*(short *)((char *)sound_tag + 0x4));
    return *(float *)((char *)class_def + 0x18);
  }

  return min_distance;
}

/* Select the best pitch range for a given random scale value (0x1c8de0).
 * If hint_index is valid and its bend bounds contain random_scale with at
 * least one permutation, returns hint_index directly. Otherwise scans all
 * pitch ranges: an exact match returns immediately, otherwise tracks the
 * closest range by ratio of bend bounds to scale. */
short sound_select_pitch_range(void *sound_tag, float random_scale,
                               short hint_index)
{
  short best_index = -1;
  char *tag = (char *)sound_tag;

  if (hint_index != -1) {
    if ((int)hint_index < *(int *)(tag + 0x98)) {
      char *pitch_range =
        (char *)tag_block_get_element(tag + 0x98, (int)hint_index, 0x48);
      float min_bend = *(float *)(pitch_range + 0x24);
      float max_bend = *(float *)(pitch_range + 0x28);
      if (min_bend <= random_scale && random_scale <= max_bend &&
          *(int *)(pitch_range + 0x3c) != 0) {
        return hint_index;
      }
    }
  }

  {
    float best_distance = 3.4028235e+38f;
    short i;
    for (i = 0; (int)i < *(int *)(tag + 0x98); i++) {
      char *pitch_range;
      float min_bend;
      float max_bend;
      float distance;

      pitch_range = (char *)tag_block_get_element(tag + 0x98, (int)i, 0x48);
      if (*(int *)(pitch_range + 0x3c) == 0)
        continue;

      min_bend = *(float *)(pitch_range + 0x24);
      max_bend = *(float *)(pitch_range + 0x28);

      if (min_bend <= random_scale && random_scale <= max_bend)
        return i;

      if (random_scale <= max_bend) {
        distance = min_bend / random_scale;
      } else {
        distance = random_scale / max_bend;
      }

      if (distance < best_distance) {
        best_distance = distance;
        best_index = i;
      }
    }
  }

  return best_index;
}

/* Reset the played-permutation mask after every permutation in this pitch
 * range has been selected (0x1c8ee0). */
void FUN_001c8ee0(void *pitch_range)
{
  char *record;
  short count;
  unsigned int all_played;

  record = (char *)pitch_range;
  count = *(short *)(record + 0x2c);
  all_played = (1u << ((unsigned char)count & 0x1f)) - 1u;

  if ((~*(unsigned int *)(record + 0x34) & all_played) == 0) {
    *(int *)(record + 0x34) = 0;
    if (count > 1) {
      *(int *)(record + 0x34) = 1u
                                << (*(unsigned char *)(record + 0x38) & 0x1f);
    }
  }
}

/* Sample a pitch value from a permutation's mouth data (0x1c8f20).
 *
 * Clamps permutation_index into [0, mouth_data.size-1], reads the
 * corresponding byte from the mouth data block via FUN_001c8d90,
 * and normalizes it to [0.0, 1.0] by dividing by 255.
 * If the permutation has no mouth data (size == 0), logs an error
 * and returns 0.0f. */
float sound_get_permutation_pitch(int permutation_block_ptr,
                                  int permutation_index)
{
  int mouth_data_size = *(int *)((char *)permutation_block_ptr + 0x54);
  int clamped_index;

  if (mouth_data_size != 0) {
    if ((short)permutation_index < 0) {
      clamped_index = 0;
    } else {
      clamped_index = mouth_data_size - 1;
      if ((short)permutation_index <= clamped_index) {
        clamped_index = (short)permutation_index;
      }
    }

    {
      uint8_t *byte_ptr =
        (uint8_t *)FUN_001c8d90(permutation_block_ptr, clamped_index);
      int byte_value = (int)*byte_ptr;
      return (float)byte_value * (1.0f / 255.0f);
    }
  }

  error(2, "but how can you speak if you have no mouth data? (permutation %s)",
        permutation_block_ptr);
  return 0.0f;
}

/* Select a permutation from a pitch range (0x1c8f80).
 * If a next-permutation is queued (offset 0x3a), consumes and returns it.
 * If the sound tag has the sequential flag (bit 1) and a hint is given,
 * returns that permutation's chained next index (offset 0x2a).
 * Otherwise performs weighted random selection with a played-bit mask
 * (offset 0x34) to avoid repeats, accepting unconditionally after 16 tries. */
short sound_select_permutation(void *sound_tag, short pitch_range_index,
                               short hint_permutation_index)
{
  char *tag = (char *)sound_tag;
  char *pitch_range =
    (char *)tag_block_get_element(tag + 0x98, (int)pitch_range_index, 0x48);
  short attempts = 0;

  if (*(int *)(pitch_range + 0x3c) == 0) {
    display_assert("range->permutations.count",
                   "c:\\halo\\SOURCE\\sound\\sound_definitions.c", 0x37c, 1);
    system_exit(-1);
  }

  {
    short next = *(short *)(pitch_range + 0x3a);
    if (next != -1) {
      *(short *)(pitch_range + 0x3a) = -1;
      *(short *)(pitch_range + 0x38) = next;
      return next;
    }
  }

  if ((*(uint8_t *)tag & 2) != 0 && hint_permutation_index != -1) {
    char *perm = (char *)tag_block_get_element(
      pitch_range + 0x3c, (int)hint_permutation_index, 0x7c);
    return *(short *)(perm + 0x2a);
  }

  {
    short count = *(short *)(pitch_range + 0x2c);
    unsigned int *seed = random_math_get_local_seed_address();
    short selected = seed_random_range(seed, 0, count);

    for (;;) {
      uint32_t all_bits = (1u << ((uint8_t)count & 0x1f)) - 1;

      if ((~*(uint32_t *)(pitch_range + 0x34) & all_bits) == 0) {
        *(uint32_t *)(pitch_range + 0x34) = 0;
        if (count > 1) {
          *(uint32_t *)(pitch_range + 0x34) =
            1u << ((uint8_t) * (short *)(pitch_range + 0x38) & 0x1f);
        }
      }

      {
        uint32_t bit = 1u << ((uint8_t)selected & 0x1f);
        if ((*(uint32_t *)(pitch_range + 0x34) & bit) == 0) {
          *(uint32_t *)(pitch_range + 0x34) |= bit;

          if (attempts == 0x10) {
            *(short *)(pitch_range + 0x38) = selected;
            return selected;
          }

          attempts++;
          seed = random_math_get_local_seed_address();

          {
            float random_val = random_math_real(seed);
            char *perm = (char *)tag_block_get_element(pitch_range + 0x3c,
                                                       (int)selected, 0x7c);

            if (random_val >= *(float *)(perm + 0x20)) {
              *(short *)(pitch_range + 0x38) = selected;
              return selected;
            }
          }
        }
      }

      selected++;
      if (selected == *(short *)(pitch_range + 0x2c)) {
        selected = 0;
      }
    }
  }
}

/* sound_valid_for_channel (0x1cb790)
 *
 * Check whether a sound definition's compression, encoding, sample_rate,
 * and spatialization_mode are compatible with a channel's type_flags word.
 *
 * Each bit of type_flags encodes a channel capability:
 *   bit 0: spatialization (mono/stereo) — skipped if bit 1 is set
 *   bit 1: skip-spatialization flag
 *   bit 2: sample rate (0 = 22050, 1 = 44100)
 *   bit 3: compression (0 = uncompressed, 1 = compressed)
 *
 * For bits 0, 1, 3 the test is inverted: the NOT of the bit must equal
 * whether the corresponding parameter is zero. */
bool sound_valid_for_channel(short compression, short encoding,
                             unsigned short sample_rate,
                             short spatialization_mode,
                             unsigned short type_flags)
{
  int flags;
  bool result;

  flags = (int)(short)type_flags;

  /* Check compression: (~(flags >> 3) & 1) must equal (compression == 0) */
  result = true;
  if ((~(flags >> 3) & 1) != (unsigned int)(compression == 0))
    result = false;

  /* Check encoding: (~(flags >> 1) & 1) must equal (encoding == 0) */
  if ((~(flags >> 1) & 1) != (unsigned int)(encoding == 0))
    result = false;

  /* Check sample_rate: ((flags >> 2) & 1) must equal sample_rate */
  if ((unsigned short)((flags >> 2) & 1) != sample_rate)
    result = false;

  /* Check spatialization: only if bit 1 of type_flags is clear */
  if (!(type_flags & 2)) {
    if ((~flags & 1) != (unsigned int)(spatialization_mode == 0))
      result = false;
  }

  return result;
}

/* Empty on Xbox — no per-map sound initialization needed. */
void sound_initialize_for_new_map(void)
{
}

/* FUN_001cb820 (0x1cb820)
 *
 * Tear down the sound manager (called from shell_dispose, 0x191140).
 *
 * If the initialized flag (0x4eaf40) is set: stop the hardware backend via
 * its vtable+0x8 entry (vtable pointer at 0x4eaf48), invalidate the sounds
 * table (0x4fdba4) and then the looping-sounds table (0x4fdba0), and clear
 * the flag.  Afterwards dispose each table that is still allocated, then
 * tail-call FUN_001bde90.
 *
 * Note: the reference emits a single `ADD ESP,0x8` after the two
 * data_make_invalid calls (coalesced cdecl cleanup for two 1-arg calls),
 * not a 2-argument call. */
void FUN_001cb820(void)
{
  if (*(unsigned char *)0x4eaf40 != 0) {
    (*(void (**)(void))(*(int *)0x4eaf48 + 8))();
    data_make_invalid(*(data_t **)0x4fdba4);
    data_make_invalid(*(data_t **)0x4fdba0);
    *(unsigned char *)0x4eaf40 = 0;
  }
  if (*(data_t **)0x4fdba4 != (data_t *)0) {
    data_dispose(*(data_t **)0x4fdba4);
  }
  if (*(data_t **)0x4fdba0 != (data_t *)0) {
    data_dispose(*(data_t **)0x4fdba0);
  }
  FUN_001bde90();
}

/* Fade out all active sounds and then stop the sound manager for map unload.
 *
 * If the sound manager is initialized and hardware is present:
 *   1. Trigger a linear fade-out on every active sound in the sounds table
 *      (0.3s fade, mode=linear, fade_out only — fade_in_index=NONE).
 *   2. Spin-loop calling sound_update() until 300 ms have elapsed since the
 *      fade was started (blocking wait for the fade to complete).
 *   3. After the wait, if the fade flag (0x4eaf42) is set, clear it, call
 *      vtable+0x28(0) to stop hardware output, and record the timestamp.
 * Finally, unconditionally stop all channels (sound_stop_all) and re-validate
 * the looping-sounds table so it is ready for the next map. */
/* sound_set_music_enabled (0x1cb8a0)
 *
 * Sets the sound system's music/fade flag at 0x4eaf42. If the new state
 * differs from the current one, updates the flag, calls the sound driver's
 * vtable+0x28 method with the enabled parameter, and -- when disabling
 * (enabled=0) -- records the current timestamp so the fade-out timing in
 * sound_dispose_from_old_map has a reference point.
 */
void sound_set_music_enabled(int enabled)
{
  char cur = *(char *)0x4eaf42;
  char val = (char)enabled;

  if (val != cur) {
    *(char *)0x4eaf42 = val;
    (*(void (**)(int))(*(int *)0x4eaf48 + 0x28))(enabled);
    if (val == '\0')
      *(unsigned int *)0x4eaf4c = system_milliseconds();
  }
}

/* sound_get_previous_ms (0x1cb8e0)
 *
 * Returns the sound system's last-recorded millisecond timestamp.
 */
unsigned int sound_render_time(void)
{
  return *(unsigned int *)0x4eaf4c;
}

/* sound_reconnect_to_structure_bsp (0x1cb8f0)
 *
 * After a structure BSP switch, re-resolve the cached scenario location of
 * every active sound.  Runs only when the sound system is initialized
 * (0x4eaf40) and hardware is present (0x4eaf41).
 *
 * Walks the sounds table (0x4fdba4) with data_next_index and, for each entry
 * whose 16-bit field at +0x14 equals 1, recomputes the location record at
 * entry+0x44 from the world point at entry+0x20 via
 * scenario_location_from_point.  Arg order confirmed from the reference:
 * PUSH EDX(entry+0x20) then PUSH EAX(entry+0x44), so entry+0x44 is the
 * out-location and entry+0x20 the source point. */
void sound_reconnect_to_structure_bsp(void)
{
  int sound_index;
  char *sound_entry;

  if (*(uint8_t *)0x4eaf40 != 0 && *(uint8_t *)0x4eaf41 != 0) {
    for (sound_index = data_next_index(*(data_t **)0x4fdba4, -1);
         sound_index != -1;
         sound_index = data_next_index(*(data_t **)0x4fdba4, sound_index)) {
      sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, sound_index);
      if (*(short *)(sound_entry + 0x14) == 1) {
        scenario_location_from_point(sound_entry + 0x44, sound_entry + 0x20);
      }
    }
  }
}

/* sound_try_and_get (0x1cb960)
 *
 * Resolves an absolute index against the sounds table (0x4fdba4) and reports
 * whether it names a live entry.  The reference loads the table pointer into
 * ECX, pushes the caller's [EBP+8] argument first and the table second
 * (cdecl, ADD ESP,8), then normalizes the returned index to 0/1 with
 * NEG/SBB/NEG -- i.e. a plain "!= 0" test on an int return.
 *
 * The exact meaning of a nonzero result beyond "index resolved" is unknown;
 * the name is the kb.json symbol. */
int sound_try_and_get(int absolute_index)
{
  return datum_absolute_index_to_index(*(data_t **)0x4fdba4, absolute_index) !=
         0;
}

/* sound_enable (0x1cb980)
 *
 * Stores the caller's byte argument into the global at 0x4eaf41.  The
 * reference is a bare frame: PUSH EBP / MOV EBP,ESP / MOV AL,[EBP+8] /
 * MOV [0x004eaf41],AL / POP EBP / RET.  No other side effects, no callees.
 *
 * kb.json declares the parameter as bool.  Other functions in this TU gate
 * work on (0x4eaf40 != 0 && 0x4eaf41 != 0); the precise meaning of 0x4eaf41
 * beyond "a byte flag this setter writes" is unknown. */
void sound_enable(bool enable)
{
  *(uint8_t *)0x4eaf41 = (uint8_t)enable;
}

/* sound_scripted_dialog_is_playing (0x1cb990)
 *
 * Reports whether the scripted-dialog hold is still in effect: frameless
 * reference is CALL game_time_get (0xb5aa0) / MOV EDX,[0x004eaf44] /
 * XOR ECX,ECX / CMP EAX,EDX / SETL CL / MOV AL,CL / RET -- a signed
 * "current game time < deadline" test returning bool in AL.  Ghidra's
 * decompile drops the comparison and shows a void tail call; the
 * disassembly is authoritative here.
 *
 * 0x4eaf44 is the same deadline this TU clears in sound_manager_stop_all
 * and raises with a max() when scripted dialog is queued. */
bool sound_scripted_dialog_is_playing(void)
{
  return game_time_get() < *(int *)0x4eaf44;
}

/* sound_manager_set_sound_environment (0x1cb9b0)
 *
 * Copies a 0x48-byte (18-dword) sound-environment block into the sound
 * manager globals at 0x4eb068.  Reference is a bare cdecl frame plus
 * MOV ESI,[EBP+8] / MOV ECX,0x12 / MOV EDI,0x4eb068 / REP MOVSD -- an
 * MSVC 18-dword struct assignment, no calls, no return value.
 *
 * The parameter is the pointer VALUE produced by
 * scenario_get_sound_environment's out-param (see the caller at 0x1c815b
 * in game_sound_update), not the address of that local.  Element type is
 * unproven, so the copy is expressed as a raw 0x48-byte block move; the
 * inline memcpy is what reproduces the reference REP MOVSD. */
void sound_manager_set_sound_environment(const void *sound_environment)
{
  memcpy((void *)0x4eb068, sound_environment, 0x12 * sizeof(uint32_t));
}

/* Check whether a sound tag can currently play.
 *
 * sound_tag_index is passed in EAX (register argument).
 *
 * Loads the sound tag, checks that pitch range 0 has at least one permutation,
 * then looks up the sound class definition for the tag's class index. If the
 * class definition's "suppress" byte (offset 0x28) is zero, the sound is
 * allowed to play. Returns true (1) if playable, false (0) otherwise. */
bool sound_can_play(int sound_tag_index /* @<eax> */)
{
  void *sound_tag;
  void *pitch_range_element;
  void *class_def;

  sound_tag = tag_get(0x736e6421, sound_tag_index);
  if (*(int *)((char *)sound_tag + 0x98) != 0) {
    pitch_range_element =
      tag_block_get_element((char *)sound_tag + 0x98, 0, 0x48);
    if (*(int *)((char *)pitch_range_element + 0x3c) != 0) {
      class_def = sound_class_get_definition(*(short *)((char *)sound_tag + 4));
      if (*(char *)((char *)class_def + 0x28) == '\0') {
        return 1;
      }
    }
  }
  return 0;
}

/* sound_channel_get (0x1cba80)
 *
 * Return a pointer to the sound channel entry at the given index.
 * Channels live in a static array at 0x4fc3a0 with a stride of 0x18.
 * The channel count is stored at 0x4eb0b4
 * (sound_manager_globals.channel_count). Asserts that channel_index is in range
 * [0, channel_count). */
void *sound_channel_get(short channel_index /* @<si> */)
{
  assert_halt(channel_index >= 0 && channel_index < *(short *)0x4eb0b4);

  return (void *)(0x4fc3a0 + (int)channel_index * 0x18);
}

/* Return a pointer to the sound listener entry for a local player.
 * The listeners table lives at 0x4eaf58 with a stride of 0x44.
 * Asserts that listener_index is in [0, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS). */
void *sound_listener_get(short listener_index /* @<si> */)
{
  short index = listener_index;

  assert_halt(index >= 0 && index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  return (void *)(0x4eaf58 + (int)index * 0x44);
}

/* Check whether a sound should be promoted to its promotion sound.
 *
 * sound_tag_index is passed in EAX (register argument).
 *
 * Loads the sound tag and checks if it has a promotion count (tag+0x80).
 * If so, updates the promotion accumulator (tag+0x88) based on the time
 * elapsed since the last check (tag+0x8c vs global timestamp 0x4eaf4c).
 * Clamps the accumulator to zero if negative, then adds the promotion
 * interval (tag+0x84). If the accumulator exceeds
 * promotion_count * promotion_interval, either:
 *   - Returns 1 (promote) if a promotion sound tag is set (tag+0x7c != -1),
 *     resetting the accumulator to zero.
 *   - Returns 2 (reject) if no promotion sound, subtracting the interval.
 * Returns 0 if no promotion is needed. */
int16_t sound_check_promotion(int sound_tag_index /* @<eax> */)
{
  int16_t result = 0;
  char *sound_tag;
  int16_t count;
  int delta;
  int accumulator;
  int interval;

  sound_tag = (char *)tag_get(0x736e6421, sound_tag_index);
  count = *(int16_t *)(sound_tag + 0x80);
  if (count != result) {
    delta = *(int *)(sound_tag + 0x8c) - *(int *)0x4eaf4c;
    accumulator = *(int *)(sound_tag + 0x88) + delta;
    *(int *)(sound_tag + 0x88) = accumulator;
    accumulator = (accumulator < 0) ? 0 : accumulator;
    interval = *(int *)(sound_tag + 0x84);
    *(int *)(sound_tag + 0x88) = accumulator;
    *(int *)(sound_tag + 0x8c) = *(int *)0x4eaf4c;
    accumulator += interval;
    *(int *)(sound_tag + 0x88) = accumulator;
    if (accumulator > count * interval) {
      if (*(int *)(sound_tag + 0x7c) != -1) {
        *(int *)(sound_tag + 0x88) = result;
        return 1;
      }
      *(int *)(sound_tag + 0x88) = accumulator - interval;
      return 2;
    }
  }

  return result;
}

/* FUN_001cbc40 (0x1cbc40)
 *
 * Per-sound update tick.  sound_index arrives in EBX (register argument) and
 * the result comes back in AL.
 *
 * Resolves the sound entry from the sounds table (0x4fdba4), validates the
 * cached playing channel (0x8c) against the channel array at 0x4fc3a0
 * (stride 0x18, sound_index at +0x00), then, unless the entry is flagged
 * (+0x04 bit 0), invokes the entry's update callback at +0x10 with
 * (entry+0x0c, entry+0x54, entry+0x14) if the entry timestamp (+0x84) is
 * older than the render time (0x4eaf4c).
 *
 * Returns true (AL=1) when the sound is done being serviced this tick, and
 * false when the callback reported completion but the sound must keep
 * playing (entry+0x02 non-zero, or the sound class definition byte at +0x08
 * is set).  In the remaining case the callback pointer is cleared and true is
 * returned. */
bool FUN_001cbc40(int sound_index /* @<ebx> */)
{
  char *sound;
  short channel_index;
  char (*update_proc)(int, void *, void *);
  void *sound_tag;
  void *class_definition;

  sound = (char *)datum_get(*(data_t **)0x4fdba4, sound_index);
  channel_index = *(short *)(sound + 0x8c);
  if (channel_index != -1) {
    assert_halt_msg_at("index>=0 && index<sound_manager_globals.channel_count",
                       "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428,
                       channel_index >= 0 &&
                         channel_index < *(short *)0x4eb0b4);
    assert_halt_msg_at(
      "sound->playing_channel_index==NONE || "
      "channel_get(sound->playing_channel_index)->sound_index==sound_index",
      "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x59f,
      *(int *)(0x4fc3a0 + (int)channel_index * 0x18) == sound_index);
  }

  if ((*(unsigned char *)(sound + 4) & 1) != 0) {
    return 1;
  }

  update_proc = *(char (**)(int, void *, void *))(sound + 0x10);
  if (update_proc == 0) {
    return 1;
  }

  if (*(int *)(sound + 0x84) >= *(int *)0x4eaf4c) {
    return 1;
  }

  if (update_proc(*(int *)(sound + 0xc), sound + 0x54, sound + 0x14) != 0) {
    return 1;
  }

  if (*(short *)(sound + 2) != 0) {
    return 0;
  }

  sound_tag = tag_get(0x736e6421, *(int *)(sound + 8));
  class_definition =
    sound_class_get_definition(*(short *)((char *)sound_tag + 4));
  if (*(char *)((char *)class_definition + 8) != 0) {
    return 0;
  }

  *(int *)(sound + 0x10) = 0;
  return 1;
}

/* sound_collect_like_sounds (0x1cbd30)
 *
 * Build a summary of channels currently playing sounds that are "like" the
 * given sound_handle, for instance-limiting purposes.
 *
 * The summary buffer (0x48 bytes, passed via ESI) is laid out as:
 *   0x00  like_definition_count      (short) — channels with same tag_index
 *   0x02  like_definition_channels[16] (short[16]) — their channel indices
 *   0x22  maximum_instance_count     (short) — from the sound class definition
 *   0x24  like_source_count          (short) — channels with same tag AND
 * source 0x26  like_source_channels[16]   (short[16]) — their channel indices
 *   0x46  maximum_source_instance_count (short) — from the sound class
 * definition
 *
 * Iterates all active channels.  For each channel that holds a different
 * sound_handle, checks sound_valid_for_channel and whether the tag_index
 * matches.  Matching channels are added to the definition list; if the
 * source field (+0x0c) also matches, they are added to the source list. */
void sound_collect_like_sounds(int sound_handle, void *summary /* @<esi> */)
{
  char *sound_entry;
  char *sound_tag;
  char *class_def;
  short channel_index;
  int *channel_base;
  int other_handle;
  char *other_entry;
  short like_def_count;
  short like_src_count;

  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);
  sound_tag = (char *)tag_get(0x736e6421, *(int *)(sound_entry + 0x8));

  /* Initialize counts to zero. */
  *(short *)((char *)summary + 0x00) = 0;
  *(short *)((char *)summary + 0x24) = 0;

  /* Read max instance counts from the sound class definition. */
  class_def =
    (char *)sound_class_get_definition(*(unsigned short *)(sound_tag + 0x4));
  *(short *)((char *)summary + 0x22) = *(short *)(class_def + 0x0);

  class_def =
    (char *)sound_class_get_definition(*(unsigned short *)(sound_tag + 0x4));
  *(short *)((char *)summary + 0x46) = *(short *)(class_def + 0x2);

  if (*(short *)((char *)summary + 0x46) > 0x10) {
    display_assert("summary->maximum_source_instance_count<=MAXIMUM_SOUND_"
                   "INSTANCES_PER_DEFINITION",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x6a4, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)summary + 0x22) > 0x10) {
    display_assert("summary->maximum_instance_count<=MAXIMUM_SOUND_INSTANCES_"
                   "PER_OBJECT_PER_DEFINITION",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x6a5, 1);
    system_exit(-1);
  }

  if (*(short *)0x4eb0b4 <= 0)
    return;
  for (channel_index = 0; channel_index < *(short *)0x4eb0b4; channel_index++) {
    if (channel_index < 0 || channel_index >= *(short *)0x4eb0b4) {
      display_assert("index>=0 && index<sound_manager_globals.channel_count",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
      system_exit(-1);
    }

    channel_base = (int *)(0x4fc3a0 + (int)channel_index * 0x18);
    other_handle = channel_base[0];

    if (other_handle == -1 || other_handle == sound_handle)
      continue;

    other_entry = (char *)datum_get(*(data_t **)0x4fdba4, other_handle);

    if (!sound_valid_for_channel(
          *(short *)(sound_tag + 0x6e), *(unsigned short *)(sound_tag + 0x6c),
          *(unsigned short *)(sound_tag + 0x6),
          *(unsigned short *)(sound_entry + 0x14),
          *(unsigned short *)((char *)channel_base + 0x4)))
      continue;

    if (*(int *)(sound_entry + 0x8) != *(int *)(other_entry + 0x8))
      continue;

    /* Same definition — add to like-definition list. */
    like_def_count = *(short *)((char *)summary + 0x00);
    if (like_def_count >= *(short *)((char *)summary + 0x22)) {
      display_assert(
        "summary->like_definition_count<summary->maximum_instance_count",
        "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x6b5, 1);
      system_exit(-1);
    }
    *(short *)((char *)summary + 0x02 + like_def_count * 2) = channel_index;
    *(short *)((char *)summary + 0x00) += 1;

    /* Same source — also add to like-source list. */
    if (*(int *)(sound_entry + 0xc) == -1)
      continue;
    if (*(int *)(sound_entry + 0xc) != *(int *)(other_entry + 0xc))
      continue;

    like_src_count = *(short *)((char *)summary + 0x24);
    if (like_src_count >= *(short *)((char *)summary + 0x46)) {
      display_assert(
        "summary->like_source_count<summary->maximum_source_instance_count",
        "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x6bb, 1);
      system_exit(-1);
    }
    *(short *)((char *)summary + 0x26 + like_src_count * 2) = channel_index;
    *(short *)((char *)summary + 0x24) += 1;
  }
}

/* sound_channel_start_new (0x1cbf30)
 *
 * Start a new permutation on a sound channel.  If the channel already has
 * a current permutation at offset +0x14, release it via
 * sound_cache_sound_finished.  Then call the sound driver's start entry
 * (vtable offset 0x18) to begin playback.
 *
 * If the channel already has a queued permutation at +0x10, the new
 * permutation is stored at +0x14 (next slot).  Otherwise it becomes the
 * current permutation at +0x10, and the time accumulator at +0x08 is
 * reset to zero.
 *
 * Register args: DI = channel_index, EBX = permutation.
 */
void sound_channel_start_new(short channel_index, int permutation)
{
  int ch;
  int *channel_base;

  if (channel_index < 0 || channel_index >= *(short *)0x4eb0b4) {
    display_assert("index>=0 && index<sound_manager_globals.channel_count",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
    system_exit(-1);
  }

  ch = (int)channel_index;
  channel_base = (int *)(0x4fc3a0 + ch * 0x18);

  /* Release any existing permutation at +0x14. */
  if (channel_base[5]) {
    sound_cache_sound_finished(channel_base[5]);
  }

  /* Call sound driver start (vtable +0x18). */
  (*(void (**)(int, int))(*(int *)0x4eaf48 + 0x18))(channel_index, permutation);

  if (channel_base[4]) {
    /* Already have a current permutation — queue as next. */
    channel_base[5] = permutation;
  } else {
    /* No current permutation — set as current and reset time. */
    channel_base[4] = permutation;
    channel_base[2] = 0;
  }
}

/* sound_channel_set_properties (0x1cbfb0)
 *
 * Push volume/pitch/3D properties to the sound driver for a channel.
 * When update_only is zero, the channel's pitch rate (offset +0x0c in the
 * channel table at 0x4fc3a0) is also written from properties+0x08 before
 * calling the driver.  When update_only is non-zero the pitch store is
 * skipped and only the driver vtable call is made.
 *
 * Register args: SI = channel_index, EBX = update_only.
 * Stack arg:     properties pointer.
 */
void sound_channel_set_properties(short channel_index, int update_only,
                                  void *properties)
{
  char *channel_base;

  if (channel_index < 0 || channel_index >= *(short *)0x4eb0b4) {
    display_assert("index>=0 && index<sound_manager_globals.channel_count",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
    system_exit(-1);
  }

  channel_base = (char *)(0x4fc3a0 + (int)channel_index * 0x18);

  if (!update_only) {
    if (!(*(float *)((char *)properties + 8) > 0.0f)) {
      display_assert("properties->pitch>0.f",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x848, 1);
      system_exit(-1);
    }
    *(int *)(channel_base + 0x0c) = *(int *)((char *)properties + 8);
  }

  /* Call the sound driver's set-properties entry (vtable offset 0x34). */
  (*(void (**)(int, void *, int))(*(int *)0x4eaf48 + 0x34))(
    channel_index, properties, update_only);
}

/* sound_channel_update_status (0x1cc050)
 *
 * Query the sound driver for playback status of the given channel and
 * advance the channel's permutation queue accordingly.
 *
 * The channel struct lives in the channel table at 0x4fc3a0 (stride 0x18):
 *   +0x08  float  time accumulator
 *   +0x0c  float  time rate
 *   +0x10  int    current permutation pointer
 *   +0x14  int    queued permutation pointer
 *
 * Driver vtable+0x24 returns the playback status for a channel:
 *   0 = stopped, 1 = playing, 2 = finished.
 *
 * If a queued permutation exists and status < 2 (still playing or stopped):
 *   - Release the old current permutation via sound_cache_sound_finished.
 *   - Promote the queued permutation to current and clear the queue.
 *   - Reset the time accumulator.
 *   - Request the new permutation via sound_cache_request_sound; if that
 *     fails, override the status to 0.
 *
 * If no queue but current permutation exists and status < 1 (stopped):
 *   - Assert no queued permutation remains.
 *   - Release current permutation via sound_cache_sound_finished and clear it.
 *
 * Finally, accumulate time: accumulator += delta_time * rate.
 *
 * channel_index is passed in AX (register arg, thunked to stack).
 * Returns the driver playback status as a short. */
short sound_channel_update_status(short channel_index)
{
  int ch;
  int *channel_base;
  short status;
  int queued_perm;

  /* Validate channel_index. */
  if (channel_index < 0 || channel_index >= *(short *)0x4eb0b4) {
    display_assert("index>=0 && index<sound_manager_globals.channel_count",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
    system_exit(-1);
  }

  ch = (int)channel_index;
  channel_base = (int *)(0x4fc3a0 + ch * 0x18);

  /* Query the sound driver for playback status (vtable+0x24). */
  status = (short)(*(int (**)(int))(*(int *)0x4eaf48 + 0x24))(channel_index);

  /* If a queued permutation is pending and channel hasn't finished, promote it.
   */
  if (channel_base[5] != 0 && status < 2) {
    sound_cache_sound_finished(channel_base[4]);
    queued_perm = channel_base[5];
    channel_base[4] = queued_perm;
    channel_base[5] = 0;
    channel_base[2] = 0;
    if (!sound_cache_request_sound((void *)queued_perm, 0, 0, 0)) {
      status = 0;
    }
  }

  /* If current permutation exists but channel is stopped, release it. */
  if (channel_base[4] != 0 && status < 1) {
    if (channel_base[5] != 0) {
      display_assert("!channel->queued_permutation",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x868, 1);
      system_exit(-1);
    }
    sound_cache_sound_finished(channel_base[4]);
    channel_base[4] = 0;
  }

  /* Accumulate time: accumulator += delta_time * rate. */
  *(float *)&channel_base[2] += *(float *)0x4eaf50 * *(float *)&channel_base[3];

  return status;
}

/* sound_channel_stop (0x1cc140)
 *
 * Release cache-sound references for a channel and stop the hardware
 * via the sound driver vtable+0x20.
 *
 * The channel struct lives in the channel table at 0x4fc3a0 (stride 0x18).
 * Two permutation pointers at offsets +0x10 and +0x14 in the channel
 * entry are checked: if non-zero, sound_cache_sound_finished is called
 * to decrement the software reference count and the pointer is cleared.
 * Finally, the sound driver's stop function (vtable+0x20) is called
 * with the channel index.
 *
 * channel_index is passed in DI (register arg, thunked to stack). */
void sound_channel_stop(short channel_index)
{
  int ch;
  int *channel_base;

  /* Validate channel_index. */
  if (channel_index < 0 || channel_index >= *(short *)0x4eb0b4) {
    display_assert("index>=0 && index<sound_manager_globals.channel_count",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
    system_exit(-1);
  }

  ch = (int)channel_index;
  channel_base = (int *)(0x4fc3a0 + ch * 0x18);

  /* Release cache-sound ref at +0x14 if present. */
  if (channel_base[5] != 0) {
    sound_cache_sound_finished(channel_base[5]);
    channel_base[5] = 0;
  }

  /* Release cache-sound ref at +0x10 if present. */
  if (channel_base[4] != 0) {
    sound_cache_sound_finished(channel_base[4]);
    channel_base[4] = 0;
  }

  /* Stop the hardware channel via driver vtable+0x20. */
  (*(void (**)(int))(*(int *)0x4eaf48 + 0x20))(channel_index);
}

/* FUN_001cc1c0 (0x1cc1c0)
 *
 * Looping-sound source refresh callback.  Stored as a function pointer in
 * the sound entry at +0x10 (see the store of 0x1cc1c0 in FUN_001cda50);
 * the only xref to this address is that DATA reference, so the parameter
 * meanings below come from the binary alone.
 *
 * Looks the looping-sound handle up in the looping-sounds table
 * (*(data_t **)0x4fdba0) via datum_absolute_index_to_index.  If the datum
 * is gone, returns 0.  Otherwise copies the 0x40-byte block at
 * looping_source + 0xc into the caller's buffer (REP MOVSD, ECX = 0x10)
 * and returns 1.  That is the same source block FUN_001cda50 seeds into
 * sound_entry + 0x14 with qmemcpy(..., looping_source + 0xc, 0x40).
 *
 * param_2 ([EBP + 0xc]) is never read by this function; its meaning is
 * unknown. */
char FUN_001cc1c0(int looping_handle, int param_2, void *out_source_data)
{
  char *looping_source;

  looping_source = (char *)(int)datum_absolute_index_to_index(
    *(data_t **)0x4fdba0, looping_handle);
  if (looping_source != (char *)0) {
    qmemcpy(out_source_data, looping_source + 0xc, 0x40);
    return 1;
  }
  return 0;
}

/* FUN_001cc2f0 (0x1cc2f0)
 *
 * Both arguments arrive in registers: the sound datum handle in EAX and the
 * value in ESI (the two callers at 0x1ce7ea / 0x1ce930 in FUN_001ce550 set
 * them up).  Resolves the handle in the sounds table (0x4fdba4) and, when the
 * entry's field_08 differs from the value, stores the value into field_98.
 * The meanings of both fields are unknown from this function alone. */
void FUN_001cc2f0(int sound_handle /* @<eax> */, int value /* @<esi> */)
{
  char *sound_entry;

  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);
  if (*(int *)(sound_entry + 8) != value) {
    *(int *)(sound_entry + 0x98) = value;
  }
}

/* sound_update_channel_attenuation (0x1cc310)
 *
 * Advance the attenuation envelope for a sound datum. Computes the
 * interpolation parameter t from the transition start/end tick fields
 * (+0xa4, +0xa8) relative to the global sound timestamp at 0x4eaf4c.
 *
 * Three envelope shapes are selected by the short at +0x92:
 *   0 (linear): t is used directly.
 *   1 (power):  t is warped through pow(t, 1/2.5) or 1-pow(1-t, 1/2.5)
 *               depending on whether target > current attenuation.
 *   default:    assert -- invalid envelope type.
 *
 * When t reaches 1.0 the transition start/end fields are cleared.
 * Returns lerp(current_atten, target_atten, shaped_t).
 * If no transition is active (start == end), returns 1.0 (fully audible). */
float sound_update_channel_attenuation(int sound_handle)
{
  char *sound_entry;
  int start_tick;
  int end_tick;
  float t;

  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);
  start_tick = *(int *)(sound_entry + 0xa4);
  end_tick = *(int *)(sound_entry + 0xa8);

  if (start_tick == end_tick)
    return 1.0f;

  /* Compute t = (current_tick - start) / (end - start), clamped to [0, 1]. */
  t = ((float)*(int *)0x4eaf4c - start_tick) / (end_tick - start_tick);
  if (t < 0.0f) {
    t = 0.0f;
  } else if (t > 1.0f) {
    t = 1.0f;
  }

  /* Apply envelope shape based on type at +0x92. */
  switch (*(short *)(sound_entry + 0x92)) {
  case 0:
    /* Linear: use t directly. */
    break;
  case 1:
    /* Power curve: ease-in or ease-out depending on direction. */
    if (*(float *)(sound_entry + 0xa0) > *(float *)(sound_entry + 0x9c)) {
      t = (float)pow(t, 1.0f / 2.5f);
    } else {
      t = (float)(1.0 - pow(1.0f - t, 1.0f / 2.5f));
    }
    break;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0xa76, 1);
    system_exit(-1);
    break;
  }

  /* When t reaches 1.0, clear the transition. */
  if (t == 1.0f) {
    *(int *)(sound_entry + 0xa8) = 0;
    *(int *)(sound_entry + 0xa4) = 0;
  }

  /* Lerp between current and target attenuation. */
  return (*(float *)(sound_entry + 0xa0) - *(float *)(sound_entry + 0x9c)) * t +
         *(float *)(sound_entry + 0x9c);
}

/* FUN_001cc440 (0x1cc440)
 *
 * The single argument arrives in EDI (the caller at 0x1ce5c5 in
 * FUN_001ce550 sets it up); the disassembly compares EDI at 0x1cc46b
 * without ever initialising it in this function.
 *
 * Walks the looping-sounds table (0x4fdba0) with data_next_index and
 * returns the index of the first entry whose field_08 equals the
 * argument, or -1 when no entry matches.  The meaning of field_08 is
 * unknown from this function alone. */
int FUN_001cc440(int value /* @<edi> */)
{
  int index;
  char *entry;

  index = data_next_index(*(data_t **)0x4fdba0, -1);
  while (index != -1) {
    entry = (char *)datum_get(*(data_t **)0x4fdba0, index);
    if (*(int *)(entry + 8) == value) {
      return index;
    }
    index = data_next_index(*(data_t **)0x4fdba0, index);
  }
  return -1;
}

/* sound_volume_crossfade (0x1cc490)
 *
 * Step a volume level toward a target value using a multiplicative rate.
 * If rate is zero or current already equals target, returns current unchanged.
 *
 * When fading down (current > target): computes target * rate and returns
 * that if it is still below current (making progress), otherwise clamps to
 * current to avoid overshoot.
 *
 * When fading up (current <= target): computes target / rate and returns
 * that if it is still above current (making progress), otherwise clamps to
 * current to avoid overshoot. */
float sound_volume_crossfade(float current, float target, float rate)
{
  if (rate == 0.0f || current == target)
    return current;

  if (current > target) {
    float result = target * rate;
    if (current > result)
      return result;
    return current;
  } else {
    float result = target / rate;
    if (current > result)
      return current;
    return result;
  }
}

/* FUN_001cc4f0 (0x1cc4f0)
 *
 * Debug overlay for one active sound.  Gated on the debug byte at 0x4fc382
 * (MOV AL,[0x4fc382]; TEST AL,AL; JZ epilogue), it resolves the handle in the
 * sounds table (0x4fdba4), touches the sound's 'snd!' tag (the tag_get result
 * is discarded — the call is kept for its side effects/asserts), then draws:
 *   - a sphere at the sound position (entry + 0x20) with the tag's default
 *     priority as radius, colour [0x2ee6e0];
 *   - a sphere at the same position with the sound class minimum distance as
 *     radius, colour [0x2ee6d0];
 *   - a text label "<tag name>|n<f> <f>" from the floats at entry + 0x4c and
 *     entry + 0x50, colour [0x2ee6c4].
 * Both radii arrive via FSTP [ESP] over the pushed slot (0x1cc53d / 0x1cc55b),
 * and the two label floats are widened to double for the sprintf varargs
 * (FSTP double ptr [ESP] / [ESP+8] at 0x1cc56c / 0x1cc573, so entry + 0x4c is
 * the first %f and entry + 0x50 the second).  Local buffer is 512 bytes
 * (SUB ESP,0x200; LEA EDX,[EBP-0x200]). */
void FUN_001cc4f0(int sound_handle)
{
  char *sound_entry;
  void *position;
  char text[512];

  if (*(char *)0x4fc382 != '\0') {
    sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);
    tag_get(0x736e6421, *(int *)(sound_entry + 8));
    position = (void *)(sound_entry + 0x20);
    render_debug_sphere('\0', position,
                 sound_get_default_priority(*(int *)(sound_entry + 8)),
                 *(void **)0x2ee6e0);
    render_debug_sphere('\0', position,
                 sound_class_get_min_distance(*(int *)(sound_entry + 8)),
                 *(void **)0x2ee6d0);
    crt_sprintf(text, "%s|n%f %f", tag_get_name(*(int *)(sound_entry + 8)),
                (double)*(float *)(sound_entry + 0x4c),
                (double)*(float *)(sound_entry + 0x50));
    render_debug_string_at_point('\0', position, text, (int)*(void **)0x2ee6c4);
  }
}

/* sound_initialize (0x1cc710)
 *
 * Bring the sound manager up.  Sequence follows the reference exactly:
 *   1. sound_manager_globals.initialized (0x4eaf40) = 0, and the hardware
 *      byte at 0x4eaf41 = 1.
 *   2. FUN_001cf820(&config) hands back the platform sound-configuration
 *      block (it stores the fixed address 0x32fce4 through the out-pointer).
 *   3. sound_cache_new() (0x1be3e0).
 *   4. REP MOVSD of 0x12 dwords from 0x2c1220 into the environment block at
 *      0x4eb068 (same shape as the copy in sound_environment_set), then the
 *      float at 0x4eb0b0 = 1.0f (MOV dword ptr,0x3f800000).
 *   5. config + 0x0 is a device index; it must be in [0, 2) and select a
 *      non-NULL backend descriptor from the pointer table at 0x32f6dc whose
 *      own first int16 equals the index.  The descriptor pointer is cached at
 *      0x4eaf48.  Every failure path just returns with initialized == 0.
 *   6. Allocate the two data arrays ("sounds" 0x200 x 0xac at 0x4fdba4,
 *      "looping sounds" 0x80 x 0xe4 at 0x4fdba0); a NULL from either returns.
 *   7. Call the backend's function pointer at descriptor + 4 with the config
 *      block; it returns a bool in AL (TEST AL,AL at 0x1cc7e4).
 *   8. data_delete_all on both arrays, then walk four channel classes: the
 *      int16 at config + 0xa + 2*class is that class's channel count, which is
 *      accumulated into sound_manager_globals.channel_count (0x4eb0b4, asserted
 *      <= MAXIMUM_SOUND_CHANNELS = 0x100 at line 0x168).  Each channel gets
 *      sound_index = NONE, the int16 from the parallel table at 0x32fcee +
 *      0xa + 2*class stored at +4, and +0x10 / +0x14 zeroed.
 *   9. initialized = 1.
 *
 * The meaning of the config block fields and of the 0x32fcee table entry is
 * UNKNOWN beyond the widths and offsets proven by the disassembly. */
void sound_initialize(void)
{
  void *config;
  short *backend;
  char (*backend_initialize)(void *);
  int *channel;
  short device_index;
  short channel_index;
  short class_offset;
  short count_in_class;
  short channel_total;
  int class_remaining;

  *(uint8_t *)0x4eaf40 = 0;
  *(uint8_t *)0x4eaf41 = 1;

  config = NULL;
  FUN_001cf820(&config);
  sound_cache_new();

  memcpy((void *)0x4eb068, (const void *)0x2c1220, 0x12 * sizeof(uint32_t));
  *(float *)0x4eb0b0 = 1.0f;

  device_index = *(short *)config;
  if (device_index < 0 || device_index >= 2)
    return;

  backend = *(short **)(0x32f6dc + (int)device_index * 4);
  if (backend == NULL || *backend != device_index)
    return;

  *(short **)0x4eaf48 = backend;

  *(data_t **)0x4fdba4 = data_new("sounds", 0x200, 0xac);
  if (*(data_t **)0x4fdba4 == NULL)
    return;

  *(data_t **)0x4fdba0 = data_new("looping sounds", 0x80, 0xe4);
  if (*(data_t **)0x4fdba0 == NULL)
    return;

  backend_initialize = *(char (**)(void *))((char *)backend + 4);
  if (backend_initialize(config) == '\0')
    return;

  channel_index = 0;
  data_delete_all(*(data_t **)0x4fdba4);
  data_delete_all(*(data_t **)0x4fdba0);

  class_offset = 10;
  class_remaining = 4;
  do {
    channel_total =
      (short)(*(short *)0x4eb0b4 + *(short *)((char *)config + class_offset));
    *(short *)0x4eb0b4 = channel_total;
    assert_halt_msg_at(
      "sound_manager_globals.channel_count<=MAXIMUM_SOUND_CHANNELS",
      "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x168,
      channel_total <= 0x100);

    count_in_class = 0;
    while (count_in_class < *(short *)((char *)config + class_offset)) {
      channel = (int *)sound_channel_get(channel_index);
      channel[0] = -1;
      channel_index++;
      *(short *)((char *)channel + 4) =
        *(short *)(0x32fcee + (int)class_offset);
      channel[4] = 0;
      channel[5] = 0;
      count_in_class++;
    }

    class_offset = (short)(class_offset + 2);
    class_remaining--;
  } while (class_remaining != 0);

  *(uint8_t *)0x4eaf40 = 1;
}

/* sound_compute_random_scale (0x1cc8c0)
 *
 * Compute a randomised scale factor for a sound parameter.
 * Gets a deterministic random float in [random_min, random_max] from the
 * local random seed, then evaluates:
 *   result = random_value * (bend_min + (bend_max - bend_min) * source_scale)
 *
 * Confirmed: 5 cdecl args, all floats.
 * Confirmed: CALL 0x10b120 (random_math_get_local_seed_address).
 * Confirmed: CALL 0x10b270 (random_real_range) with (seed, random_min,
 * random_max). Confirmed: FPU arithmetic: (bend_max - bend_min) * source_scale
 * + bend_min, then multiplied by the random_real_range result. Confirmed:
 * returns float on FPU stack.
 */
float sound_compute_random_scale(float random_min, float random_max,
                                 float bend_min, float bend_max,
                                 float source_scale)
{
  unsigned int *seed;
  float random_value;

  seed = random_math_get_local_seed_address();
  random_value = random_real_range((int *)seed, random_min, random_max);
  return random_value * ((bend_max - bend_min) * source_scale + bend_min);
}

/* sound_start_fade (0x1cc8f0)
 *
 * Begin a timed linear or crossfade between two sound entries. Mode must be
 * either _sound_fade_mode_linear (0) or _sound_fade_mode_crossfade (1).
 *
 * Computes a fade window [start_tick, end_tick] based on the global sound
 * timestamp at 0x4eaf4c. start_tick is timestamp-1, and end_tick is derived
 * from seconds*1000 + start_tick, clamped to at least the current timestamp.
 *
 * For fade_in_sound_index: sets target volume to 1.0 and computes current
 * attenuation; if already at the endpoint, sets current volume to 0.
 * For fade_out_sound_index: sets target volume to 0.0 and computes current
 * attenuation.
 *
 * Sound entry fields written:
 *   +0x92 (short) = fade mode
 *   +0x9c (float) = current fade volume
 *   +0xa0 (float) = target fade volume
 *   +0xa4 (int)   = fade start tick
 *   +0xa8 (int)   = fade end tick */
void sound_start_fade(short mode, float seconds, int fade_in_sound_index,
                      int fade_out_sound_index)
{
  int sound_timestamp;
  int start_tick;
  int end_tick;
  char *sound_entry;

  if (mode != 0 && mode != 1) {
    display_assert(
      "mode==_sound_fade_mode_linear || mode==_sound_fade_mode_crossfade",
      "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x43f, 1);
    system_exit(-1);
  }
  if (!(seconds >= 0.0f)) {
    display_assert("seconds>=0.f", "c:\\halo\\SOURCE\\sound\\sound_manager.c",
                   0x440, 1);
    system_exit(-1);
  }
  if (fade_in_sound_index == -1 && fade_out_sound_index == -1) {
    display_assert("fade_in_sound_index!=NONE || fade_out_sound_index!=NONE",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x441, 1);
    system_exit(-1);
  }

  sound_timestamp = *(int *)0x4eaf4c;
  start_tick = sound_timestamp - 1;
  end_tick = (int)(seconds * *(float *)0x254cb8 + (float)start_tick);
  if (end_tick <= sound_timestamp) {
    end_tick = sound_timestamp;
  }

  if (fade_in_sound_index != -1) {
    sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, fade_in_sound_index);
    if (*(int *)(sound_entry + 0xa4) == *(int *)(sound_entry + 0xa8)) {
      *(float *)(sound_entry + 0x9c) = 0.0f;
    } else {
      *(float *)(sound_entry + 0x9c) =
        sound_update_channel_attenuation(fade_in_sound_index);
    }
    *(float *)(sound_entry + 0xa0) = 1.0f;
    *(short *)(sound_entry + 0x92) = mode;
    *(int *)(sound_entry + 0xa4) = start_tick;
    *(int *)(sound_entry + 0xa8) = end_tick;
  }

  if (fade_out_sound_index != -1) {
    sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, fade_out_sound_index);
    *(float *)(sound_entry + 0x9c) =
      sound_update_channel_attenuation(fade_out_sound_index);
    *(float *)(sound_entry + 0xa0) = 0.0f;
    *(short *)(sound_entry + 0x92) = mode;
    *(int *)(sound_entry + 0xa4) = start_tick;
    *(int *)(sound_entry + 0xa8) = end_tick;
  }
}

/* sound_stop_channel (0x1cca60)
 *
 * Stop and release the channel currently holding a sound, then delete the
 * sound datum. sound_handle is passed in EBX (register argument).
 *
 * If the sound has a playing channel (playing_channel_index != NONE at +0x8c):
 *   1. Clear the channel's sound_handle to NONE (-1) in the channel table
 *      at 0x4fc3a0 (stride 0x18).
 *   2. Call sound_channel_stop (0x1cc140, @<di>) to release cache-sound
 *      references and stop the hardware channel.
 *   3. Clear the sound's playing_channel_index to NONE.
 *
 * If the sound has no playing channel but has flags bit 1 set (+0x4 & 2):
 *   Navigate the sound tag's pitch_range -> permutation hierarchy and call
 *   sound_cache_sound_finished (0x1be090) to decrement the cache refcount.
 *
 * If the sound's type (+0x2) is non-zero (looping sound):
 *   Look up the looping-sound datum via datum_absolute_index_to_index
 *   on the looping-sounds table (0x4fdba0). If found, decrement
 *   playing_count (+0x50) and clear the track entry at
 *   +0xd4 + track_index*4 if it matches sound_handle.
 *
 * Clear the sound tag's last-played field (+0x94) if it matches sound_handle.
 * Assert that the sound's playing_channel_index is NONE after processing.
 * Finally, delete the sound datum from the sounds table (0x4fdba4). */
void sound_stop_channel(int sound_handle /* @<ebx> */)
{
  char *sound_entry;
  void *tag_ptr;
  short playing_channel_index;

  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);
  tag_ptr = tag_get(0x736e6421, *(int *)(sound_entry + 0x8));

  playing_channel_index = *(short *)(sound_entry + 0x8c);

  if (playing_channel_index != -1) {
    /* Active channel -- stop it. */
    if (playing_channel_index < 0 ||
        playing_channel_index >= *(short *)0x4eb0b4) {
      display_assert("index>=0 && index<sound_manager_globals.channel_count",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
      system_exit(-1);
    }

    /* Clear the channel's sound_handle to NONE. */
    *(int *)(0x4fc3a0 + (int)playing_channel_index * 0x18) = -1;

    /* Release cache sounds and stop hardware for this channel.
     * Re-read playing_channel_index into DI for the register-arg callee. */
    sound_channel_stop(*(short *)(sound_entry + 0x8c));

    /* Clear the sound's playing_channel_index. */
    *(short *)(sound_entry + 0x8c) = -1;
  } else if ((*(uint8_t *)(sound_entry + 0x4) & 2) != 0) {
    /* No active channel -- release cache sound if flags bit 1 set. */
    sound_cache_sound_finished((int)tag_block_get_element(
      (char *)tag_block_get_element(
        (char *)tag_get(0x736e6421, *(int *)(sound_entry + 0x8)) + 0x98,
        (int)*(short *)(sound_entry + 0x8e), 0x48) +
        0x3c,
      (int)*(short *)(sound_entry + 0x90), 0x7c));
  }

  /* If this is a looping sound (type != 0), update the looping-sound entry. */
  if (*(short *)(sound_entry + 0x2) != 0) {
    int looping_sound = datum_absolute_index_to_index(
      *(data_t **)0x4fdba0, *(int *)(sound_entry + 0xc));

    if (looping_sound != 0) {
      char *ls = (char *)looping_sound;
      short track_index = *(short *)(sound_entry + 0x94);

      /* Decrement the looping sound's playing count. */
      (*(short *)(ls + 0x50))--;

      /* Clear the track entry if it matches our sound_handle. */
      if (*(int *)(ls + 0xd4 + track_index * 4) == sound_handle) {
        *(int *)(ls + 0xd4 + track_index * 4) = -1;
      }
    }
  }

  /* Clear the sound tag's last-played field if it matches our handle. */
  if (*(int *)((char *)tag_ptr + 0x94) == sound_handle) {
    *(int *)((char *)tag_ptr + 0x94) = -1;
  }

  /* Assert the sound's playing_channel_index is now NONE. */
  {
    char *verify = (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);
    if (*(short *)(verify + 0x8c) != -1) {
      display_assert("sound->playing_channel_index==NONE",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x4cf, 1);
      system_exit(-1);
    }
  }

  /* Delete the sound datum. */
  datum_delete(*(data_t **)0x4fdba4, sound_handle);
}

/* Sound manager — low-level sound system lifecycle and rendering. */

/* Compute listener distance squared for a channel/source pair. */
extern float FUN_001ccbe0(int channel_index, void *source);

/* Compute distance for random scale checks. */
extern float FUN_001ccca0(int channel_index, void *source);

/* sound_find_oldest_channel (0x1ccd70)
 *
 * Scan a list of candidate channel indices and return the first one whose
 * sound has been playing long enough (elapsed >= class threshold) and whose
 * listener distance is within 1.0 of our sound's distance.
 *
 * Resolves the caller's sound_handle to get a reference distance via
 * FUN_001ccbe0.  Then for each candidate channel, looks up the channel's
 * sound datum, checks that enough time has elapsed since it started
 * (sound_entry+0x84 vs global timer 0x4eaf4c), and compares distances.
 *
 * Returns the channel index of the best eviction candidate, or -1 if
 * none qualifies. */
short sound_find_oldest_channel(int sound_handle, short *channels, short count)
{
  char *sound_entry;
  char *sound_tag;
  float best_distance;
  short i;
  short index;
  char *other_entry;
  void *class_def;
  int elapsed;

  /* Resolve our sound datum and its tag. */
  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);
  sound_tag = (char *)tag_get(0x736e6421, *(int *)(sound_entry + 0x8));

  /* Compute reference distance for our sound. */
  best_distance =
    FUN_001ccbe0(*(short *)(sound_entry + 0x6), (void *)(sound_entry + 0x14));

  for (i = 0; i < count; i++) {
    index = channels[i];

    if (index < 0 || index >= *(short *)0x4eb0b4) {
      display_assert("index>=0 && index<sound_manager_globals.channel_count",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
      system_exit(-1);
    }

    /* Get the sound datum currently playing on this channel. */
    other_entry = (char *)datum_get(*(data_t **)0x4fdba4,
                                    *(int *)(0x4fc3a0 + (int)index * 0x18));

    /* Look up the sound class time threshold. */
    class_def =
      sound_class_get_definition(*(unsigned short *)(sound_tag + 0x4));

    /* Check elapsed time against class threshold. */
    elapsed = *(int *)0x4eaf4c - *(int *)(other_entry + 0x84);
    if (elapsed >= *(int *)((char *)class_def + 0x4)) {
      /* Check if the candidate's distance is within 1.0 of ours. */
      if (best_distance - FUN_001ccbe0(*(short *)(other_entry + 0x6),
                                       (void *)(other_entry + 0x14)) <
          1.0f) {
        return index;
      }
    }
  }

  return -1;
}

/* sound_update_time (0x1cce80)
 *
 * Priority comparison between two active sounds ("challenger" vs
 * "champion" per the assert string at 0x1cced8/0x1ccee2).  Returns
 * true when the challenger should win the contest.
 *
 * Register ABI (disassembly 0x1cce89/0x1cce8b): ECX and EAX carry the two
 * sound handles, the float arrives on the stack at [EBP+8] (FCOMP float ptr
 * [EBP+8] at 0x1ccf54), and the result is returned in EAX (XOR EAX,EAX /
 * MOV EAX,1 at 0x1ccf60 / 0x1ccf69 -- a full-EAX boolean, so the return
 * type is int, not the 1-byte bool typedef).  Which handle is the challenger
 * and which the champion is INFERRED from the return sense (return true when
 * the ECX-derived class ranks higher), not proven by the binary.
 *
 * Sequence:
 *   1. Resolve both sound datums and their snd! tags.
 *   2. assert(challenger != champion) at line 0x763.
 *   3. Compare the two sound classes' field_0a (rank/priority, meaning
 * unproven; signed 16-bit, CMP/JG at 0x1ccf18): challenger higher -> true.
 *   4. If the priorities differ at all (JNZ at 0x1ccf46) -> false.
 *   5. Tie-break: recompute the champion's listener distance via
 *      FUN_001ccbe0 (@<eax> = sound_entry+0x6, @<edi> = sound_entry+0x14)
 *      and return true when it exceeds the caller-supplied threshold. */
int sound_update_time(int challenger_sound_index, int champion_sound_index,
                      float threshold)
{
  char *challenger_entry;
  char *champion_entry;
  char *volatile challenger_tag;
  char *volatile champion_tag;
  void *volatile challenger_class;
  void *volatile champion_class;

  challenger_entry =
    (char *)datum_get(*(data_t **)0x4fdba4, challenger_sound_index);
  challenger_tag =
    (char *)tag_get(0x736e6421, *(int *)(challenger_entry + 0x8));
  champion_entry =
    (char *)datum_get(*(data_t **)0x4fdba4, champion_sound_index);
  champion_tag = (char *)tag_get(0x736e6421, *(int *)(champion_entry + 0x8));

  if (challenger_sound_index == champion_sound_index) {
    display_assert("challenger_sound_index!=champion_sound_index",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x763, 1);
    system_exit(-1);
  }

  /* Higher class priority wins outright. */
  challenger_class =
    sound_class_get_definition(*(unsigned short *)(challenger_tag + 0x4));
  champion_class =
    sound_class_get_definition(*(unsigned short *)(champion_tag + 0x4));
  if (*(short *)((char *)challenger_class + 0xa) >
      *(short *)((char *)champion_class + 0xa))
    return 1;

  /* Different (lower) priority loses outright.  The original reloads both
   * class definitions here rather than reusing the values above. */
  challenger_class =
    sound_class_get_definition(*(unsigned short *)(challenger_tag + 0x4));
  champion_class =
    sound_class_get_definition(*(unsigned short *)(champion_tag + 0x4));
  if (*(short *)((char *)challenger_class + 0xa) !=
      *(short *)((char *)champion_class + 0xa))
    return 0;

  /* Equal priority: the champion's listener distance decides. */
  if (!(FUN_001ccbe0(*(short *)(champion_entry + 0x6),
                     (void *)(champion_entry + 0x14)) > threshold))
    return 0;
  return 1;
}

/* sound_update_channel (0x1ccf80)
 *
 * Apply volume/pan/pitch updates for a non-music sound channel.
 *
 * Resolves the sound datum and tag, then computes a composite volume
 * from the tag's gain range (tag+0x40..0x58), the sound_entry's
 * interpolation fraction (+0x18), per-entry gain (+0x1c), class gain
 * (sound_class_get_gain), and the incoming attenuation scalar.
 * Non-ambient classes (not 0x2c/0x2e/0x2f) additionally scale by
 * the global volume at 0x4eb0b0.
 *
 * If the sound's playing channel index (+0x8c) is NONE (-1), this is a
 * new channel: build a full properties struct (pitch, max_distance,
 * pan, volume, direction, class flags), assert the permutation is
 * cache-loaded, then push properties and start the new permutation via
 * sound_channel_set_properties and sound_channel_start_new.
 *
 * If the channel is already playing, compute volume from the existing
 * permutation's gain and push it directly to the sound driver via
 * vtable+0x34 (volume-only update, flag=1).
 *
 * Finally, call vtable+0x1c to commit the channel update. */
void sound_update_channel(int channel_index, float attenuation)
{
  short si = (short)channel_index;
  int *channel_ptr;
  char *sound_entry;
  char *tag_ptr;
  short class_index;
  float class_gain;
  float fraction;
  float volume;
  int pitch_range;
  int permutation;
  void *class_def;

  /* Properties struct: 0x20 bytes (8 floats).
   *   +0x00 float min_distance (pitch)
   *   +0x04 float max_distance
   *   +0x08 float pan
   *   +0x0C float gain/volume
   *   +0x10 float direction[3] (from tag+0x1c..0x24)
   *   +0x1C int   class_def_flags (class_def+0x10) */
  float properties[8];

  /* Validate channel_index. */
  if (si < 0 || si >= *(short *)0x4eb0b4) {
    display_assert("index>=0 && index<sound_manager_globals.channel_count",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
    system_exit(-1);
  }

  /* Resolve channel, sound entry, and tag. */
  channel_ptr = (int *)(0x4fc3a0 + (int)si * 0x18);
  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, *channel_ptr);
  tag_ptr = (char *)tag_get(0x736e6421, *(int *)(sound_entry + 0x8));

  /* Cache interpolation fraction from sound_entry+0x18. */
  fraction = *(float *)(sound_entry + 0x18);

  /* Get class gain. */
  class_index = *(short *)(tag_ptr + 0x4);
  class_gain = sound_class_get_gain((int)class_index);

  /* Non-ambient classes scale by global volume. */
  if (class_index != 0x2c && class_index != 0x2e && class_index != 0x2f) {
    class_gain = class_gain * *(float *)0x4eb0b0;
  }

  /* Compute composite volume:
   * lerp(tag+0x40, tag+0x58, fraction) * sound_entry+0x1c * class_gain *
   * attenuation */
  volume =
    (*(float *)(tag_ptr + 0x58) - *(float *)(tag_ptr + 0x40)) * fraction +
    *(float *)(tag_ptr + 0x40);
  volume = volume * *(float *)(sound_entry + 0x1c) * class_gain * attenuation;

  if (*(short *)(sound_entry + 0x8c) == -1) {
    /* New channel: build full properties and start permutation. */
    pitch_range = (int)tag_block_get_element(
      tag_ptr + 0x98, (int)*(short *)(sound_entry + 0x8e), 0x48);
    permutation = (int)tag_block_get_element(
      (char *)pitch_range + 0x3c, (int)*(short *)(sound_entry + 0x90), 0x7c);

    /* Volume: permutation+0x24 * tag+0x28 * composite volume. */
    properties[3] =
      *(float *)(permutation + 0x24) * *(float *)(tag_ptr + 0x28) * volume;

    /* Pan: sound_entry+0x88 * pitch_range+0x30. */
    properties[2] =
      *(float *)(sound_entry + 0x88) * *(float *)(pitch_range + 0x30);

    /* Pitch/min-distance from tag or class definition. */
    {
      float min_dist = *(float *)(tag_ptr + 8);
      if (min_dist == *(float *)0x2533c0) {
        void *cls = sound_class_get_definition(*(short *)(tag_ptr + 4));
        min_dist = *(float *)((char *)cls + 0x18);
      }
      properties[0] = min_dist;
    }

    /* Direction from tag+0x1c..0x24. */
    *(int *)&properties[4] = *(int *)(tag_ptr + 0x1c);
    *(int *)&properties[5] = *(int *)(tag_ptr + 0x20);
    *(int *)&properties[6] = *(int *)(tag_ptr + 0x24);

    /* Max distance: FLT_MAX. */
    *(int *)&properties[1] = 0x7f7fffff;

    /* Class definition flags. */
    class_def = sound_class_get_definition(class_index);
    *(int *)&properties[7] = *(int *)((char *)class_def + 0x10);

    /* Assert permutation is cache-loaded. */
    if (!sound_cache_request_sound((void *)permutation, 0, 0, 0)) {
      display_assert("sound_cache_sound_loaded(permutation)",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x79a, 1);
      system_exit(-1);
    }

    /* Push properties and start the new permutation.
     * sound_channel_set_properties: SI=channel_index, EBX=0 (full update).
     * sound_channel_start_new: DI=channel_index, EBX=permutation. */
    sound_channel_set_properties(si, 0, properties);
    sound_channel_start_new(si, permutation);

    /* Record this channel as playing in the sound entry. */
    *(short *)(sound_entry + 0x8c) = si;
  } else {
    /* Existing channel: update volume from current permutation. */
    properties[3] = *(float *)(*(int *)((char *)channel_ptr + 0x10) + 0x24) *
                    *(float *)(tag_ptr + 0x28) * volume;

    /* Validate channel_index again (matches original). */
    if (si < 0 || si >= *(short *)0x4eb0b4) {
      display_assert("index>=0 && index<sound_manager_globals.channel_count",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
      system_exit(-1);
    }

    /* Push volume-only update (flag=1) directly through the sound driver. */
    (*(void (**)(int, void *, int))(*(int *)0x4eaf48 + 0x34))(channel_index,
                                                              properties, 1);
  }

  /* Commit the channel update. */
  (*(void (**)(int))(*(int *)0x4eaf48 + 0x1c))(channel_index);
}

/* sound_start_next_looping_permutation (0x1cd2c0)
 *
 * Advance a looping sound to its next permutation. Called when the current
 * permutation finishes and the next_sound chain is exhausted (+0x98 == -1).
 *
 * Phase 1 — set up the new permutation:
 *   Resolves the sound datum and its tag (snd!).  Sets the "next ready"
 *   flag (flags |= 8).  Saves the current looping_sound_tag_index to the
 *   sound's tag_index field (+0x8) and clears it (+0x98 = -1).  Selects a
 *   new pitch range via sound_select_pitch_range using the random_scale
 *   (+0x88) and hint pitch range index (+0x8e), then selects a permutation
 *   within that range via sound_select_permutation (hint = -1).  Stores the
 *   new pitch_range_index (+0x8e) and permutation_index (+0x90).
 *
 * Phase 2 — instance limiting (only if playing_channel_index != -1):
 *   Collects a summary of similar sounds via sound_collect_like_sounds
 *   (0x1cbd30, @<esi>).  If the like_source_count has reached
 *   max_source_instance_count, searches the source channel list for the
 *   oldest to steal.  Otherwise, if the like_definition_count has reached
 *   max_instance_count, searches the definition channel list.  If a
 *   stealable channel is found, stops its sound; if not, stops our own.
 *
 * sound_handle passed in EAX (register argument). */
void sound_start_next_looping_permutation(int sound_handle /* @<eax> */)
{
  char *sound_entry;
  void *sound_tag;
  short pitch_range_index;
  char summary[0x48];
  short oldest_channel;
  short *channels_ptr;
  int count_val;

  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, sound_handle);
  sound_tag = tag_get(0x736e6421, *(int *)(sound_entry + 0x98));

  *(uint8_t *)(sound_entry + 0x4) |= 8;
  *(int *)(sound_entry + 0x8) = *(int *)(sound_entry + 0x98);
  *(int *)(sound_entry + 0x98) = -1;

  pitch_range_index =
    sound_select_pitch_range(sound_tag, *(float *)(sound_entry + 0x88),
                             (uint16_t) * (short *)(sound_entry + 0x8e));
  *(short *)(sound_entry + 0x8e) = pitch_range_index;

  *(short *)(sound_entry + 0x90) =
    sound_select_permutation(sound_tag, pitch_range_index, -1);

  if (*(short *)(sound_entry + 0x8c) != -1) {
    sound_collect_like_sounds(sound_handle, summary);

    if (*(short *)(summary + 0x24) >= *(short *)(summary + 0x46)) {
      channels_ptr = (short *)(summary + 0x26);
      count_val = *(int *)(summary + 0x24);
    } else if (*(short *)(summary + 0x00) >= *(short *)(summary + 0x22)) {
      channels_ptr = (short *)(summary + 0x02);
      count_val = *(int *)(summary + 0x00);
    } else {
      return;
    }

    oldest_channel =
      sound_find_oldest_channel(sound_handle, channels_ptr, (short)count_val);

    if (oldest_channel != -1) {
      sound_handle = *(int *)sound_channel_get(oldest_channel);
    }

    sound_stop_channel(sound_handle);
  }
}

void sound_stop_impulse(int sound_index)
{
  void *sound;

  if (!datum_absolute_index_to_index(*(data_t **)0x4fdba4, sound_index))
    return;

  sound = datum_get(*(data_t **)0x4fdba4, sound_index);
  if (*(short *)((char *)sound + 2) != 0) {
    display_assert("sound_get(sound_index)->type==_sound_impulse",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x2c1, 1);
    system_exit(-1);
  }

  sound = datum_get(*(data_t **)0x4fdba4, sound_index);
  if (*(short *)((char *)sound + 2) == 0)
    sound_start_fade(0, 0.3f, -1, sound_index);
}

/* sound_stop_impulse_by_source_and_definition (0x1cd4d0)
 *
 * Walk the sounds table (0x4fdba4) with data_next_index and stop the first
 * impulse sound whose source and definition both match the arguments.
 *
 * The reference declares no parameters in kb.json but reads two stack dwords:
 * [EBP+8] is compared against entry+0xc and [EBP+0xc] against entry+0x8.
 * entry+0x8 is the 'snd!' definition tag index (it is the tag_get argument in
 * sound_update at 0x1cc0e0); entry+0xc is the source handle (assigned from an
 * object handle at 0x1ccf50), so the parameter order matches the symbol name:
 * source first, definition second.
 *
 * entry+0x2 is the sound type; 0 means impulse (from the
 * "sound_get(sound_index)->type==_sound_impulse" assert in
 * sound_stop_impulse). Only the FIRST match is stopped -- the reference
 * returns immediately after the sound_stop_impulse call. */
void sound_stop_impulse_by_source_and_definition(int source_handle,
                                                 int definition_index)
{
  int sound_index;
  char *sound_entry;

  for (sound_index = data_next_index(*(data_t **)0x4fdba4, -1);
       sound_index != -1;
       sound_index = data_next_index(*(data_t **)0x4fdba4, sound_index)) {
    sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, sound_index);
    if (*(short *)(sound_entry + 2) == 0 &&
        *(int *)(sound_entry + 0xc) == source_handle &&
        *(int *)(sound_entry + 8) == definition_index) {
      sound_stop_impulse(sound_index);
      return;
    }
  }
}

/* sound_stop_all (0x1cd540)
 *
 * Stop every active sound channel and reset the fade deadline.
 *
 * If the sound system is initialized (0x4eaf40), walk the sounds table
 * (0x4fdba4) with data_next_index and stop each entry via
 * sound_stop_channel (@<ebx>), then re-validate the looping-sounds table
 * (0x4fdba0) and call the hardware backend's +0x2c vtable entry
 * (0x4eaf48). The fade deadline (0x4eaf44) is cleared unconditionally,
 * even when the sound system is not initialized. */
void sound_stop_all(void)
{
  int sound_index;
  if (*(uint8_t *)0x4eaf40 != 0) {
    for (sound_index = data_next_index(*(data_t **)0x4fdba4, -1);
         sound_index != -1;
         sound_index = data_next_index(*(data_t **)0x4fdba4, sound_index)) {
      sound_stop_channel(sound_index);
    }
    data_make_valid(*(data_t **)0x4fdba0);
    (*(void (**)(void))((*(uint8_t **)0x4eaf48) + 0x2c))();
  }
  *(int *)0x4eaf44 = 0;
}

/* Allocate a sound channel for a source based on its spatialization mode.
 *
 * source is passed in EAX (register argument); priority is on the stack.
 *
 * Behavior depends on source->spatialization_mode (short at offset 0):
 *   - Mode 0 (none): returns channel 0 immediately.
 *   - Mode 2 (single listener): computes distance via 0x1ccbe0 with
 *     channel=-1 and the source pointer. If distance >= priority, returns
 *     the channel index; otherwise returns 0.
 *   - Other modes (1 = listener-relative): iterates over up to 4 local
 *     player listener slots (0x4eaf58 + i*0x44), computing distance for
 *     each active listener. Tracks the closest listener. If found, calls
 *     0x1c8310 to evaluate channel suitability. Returns the best channel
 *     index, or -1 if priority^2 < min distance or source[0x3c] == 1.0f.
 */
int16_t sound_allocate_channel(void *source /* @<eax> */, float priority)
{
  short spatialization_mode;
  int best_channel;
  float best_dist_sq;
  int i;
  char *listener_ptr;
  float sqrt_dist;

  spatialization_mode = *(short *)source;
  best_channel = -1;

  if (spatialization_mode == 0)
    return 0;

  if (spatialization_mode == 2) {
    /* Single listener: compute distance with channel=-1. */
    {
      float dist_result = FUN_001ccbe0(-1, source);
      if (dist_result >= priority)
        return (short)best_channel;
      return 0;
    }
  }

  /* Mode 1 / other: iterate over local player listeners. */
  best_dist_sq = 3.4028235e+38f; /* FLT_MAX (0x7f7fffff) */
  listener_ptr = (char *)0x4eaf58;

  for (i = 0; (short)i < 4; i++, listener_ptr += 0x44) {
    if ((short)i < 0 || (short)i >= 4) {
      display_assert("index>=0 && index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x430, 1);
      system_exit(-1);
    }
    if (*listener_ptr != '\0') {
      /* Compute distance squared for this listener. */
      float this_dist = FUN_001ccbe0(i, source);
      if (this_dist < best_dist_sq) {
        best_dist_sq = this_dist;
        best_channel = i;
      }
    }
  }

  if ((short)best_channel != -1) {
    /* Evaluate channel suitability with sqrt of best distance. */
    sqrt_dist = xbox_sqrtf(best_dist_sq);
    sound_compute_source_obstruction(best_channel, source, sqrt_dist);
  }

  if (priority * priority < best_dist_sq)
    return -1;
  if (*(int *)((char *)source + 0x3c) == 0x3f800000)
    return -1;

  return (short)best_channel;
}

/* sound_create_looping_entry (0x1cda50)
 *
 * Create a new sound entry for a looping sound track. This is the main
 * constructor for looping sound playback: it checks playability, allocates
 * a channel and sound datum, initializes the datum with tag data, selects
 * an initial pitch range and permutation, and requests the sound to be
 * cached.
 *
 * Parameters:
 *   sound_tag_handle  (@<eax>) — tag index of the sound definition (snd!)
 *   looping_handle    — datum handle of the parent looping sound source
 *   track_index       — which track within the looping sound (16-bit)
 *   type              — sound entry type/class (16-bit)
 *
 * Returns:  datum handle of the new sound entry, or -1 on failure.
 *
 * Flow:
 *   1. Resolve the looping sound source and extract its gain field (+0x10).
 *   2. Check if the sound can play (sound_can_play). If not, return -1.
 *   3. Resolve the snd! tag and compute a default priority.
 *   4. Allocate a channel via sound_allocate_channel. If -1, return -1.
 *   5. Allocate a new sound datum via data_new_at_index. If -1, return -1.
 *   6. Initialize the sound datum fields: channel, tag handle, looping
 *      handle, type, track index, timestamp, update callback, and copy
 *      64 bytes of source data from the looping sound.
 *   7. Compute random gain via random_real_range on the tag's gain bounds.
 *   8. Select pitch range and permutation from the tag.
 *   9. Resolve the permutation's tag block element and request sound cache.
 *  10. Increment the looping sound source's reference count (+0x50). */
int sound_create_looping_entry(int sound_tag_handle /* @<eax> */,
                               int looping_handle, int track_index, int type)
{
  char *looping_source;
  float source_gain;
  void *sound_tag;
  float priority;
  int16_t channel_index;
  int new_handle;
  char *sound_entry;
  float random_gain;
  short pitch_range_index;
  short permutation_index;

  looping_source = (char *)datum_get(*(data_t **)0x4fdba0, looping_handle);
  source_gain = *(float *)(looping_source + 0x10);

  if (!sound_can_play(sound_tag_handle)) {
    return -1;
  }

  sound_tag = tag_get(0x736e6421, sound_tag_handle);
  priority = sound_get_default_priority(sound_tag_handle);

  channel_index = sound_allocate_channel(looping_source + 0xc, priority);
  if (channel_index == -1) {
    return -1;
  }

  new_handle = data_new_at_index(*(data_t **)0x4fdba4);
  if (new_handle == -1) {
    return new_handle;
  }

  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, new_handle);

  *(short *)(sound_entry + 0x6) = channel_index;
  *(int *)(sound_entry + 0x8) = sound_tag_handle;
  *(short *)(sound_entry + 0x8c) = -1;
  *(short *)(sound_entry + 0x4) = 0;

  random_gain = random_real_range((int *)random_math_get_local_seed_address(),
                                  *(float *)((char *)sound_tag + 0x14),
                                  *(float *)((char *)sound_tag + 0x18));

  *(float *)(sound_entry + 0x88) = random_gain;
  *(int *)(sound_entry + 0xc) = looping_handle;

  qmemcpy(sound_entry + 0x14, looping_source + 0xc, 0x40);

  *(short *)(sound_entry + 0x2) = (short)type;
  *(unsigned int *)(sound_entry + 0x84) = *(unsigned int *)0x4eaf4c;
  *(short *)(sound_entry + 0x94) = (short)track_index;
  *(unsigned int *)(sound_entry + 0x10) = 0x1cc1c0;
  *(int *)(sound_entry + 0xa8) = 0;
  *(int *)(sound_entry + 0xa4) = 0;
  *(int *)(sound_entry + 0x98) = -1;

  pitch_range_index =
    sound_select_pitch_range(sound_tag,
                             ((*(float *)((char *)sound_tag + 0x5c) -
                               *(float *)((char *)sound_tag + 0x44)) *
                                source_gain +
                              *(float *)((char *)sound_tag + 0x44)) *
                               random_gain,
                             -1);

  *(short *)(sound_entry + 0x8e) = pitch_range_index;

  permutation_index =
    sound_select_permutation(sound_tag, pitch_range_index, -1);

  *(short *)(sound_entry + 0x90) = permutation_index;

  sound_cache_request_sound(
    tag_block_get_element(
      (char *)tag_block_get_element(
        (char *)tag_get(0x736e6421, *(int *)(sound_entry + 0x8)) + 0x98,
        (int)*(short *)(sound_entry + 0x8e), 0x48) +
        0x3c,
      (int)permutation_index, 0x7c),
    0, 1, 0);

  (*(short *)(looping_source + 0x50))++;

  return new_handle;
}

/* sound_update_music_channel (0x1cdc30)
 *
 * Apply volume/pan/pitch updates for a music-class (looping) channel.
 *
 * Resolves the sound datum, its tag (snd!), and the parent looping-sound
 * datum (lsnd).  Computes a composite volume from the tag's gain range
 * (tag+0x40..0x58), the looping-sound track gain (track+0x4), per-entry
 * gain (sound_entry+0x1c), class gain (sound_class_get_gain), the tag's
 * overall gain scalar (tag+0x28), and the incoming attenuation.
 * Non-ambient classes (not 0x2c/0x2e/0x2f) additionally scale by the
 * global volume at 0x4eb0b0.
 *
 * If the sound's playing channel index (+0x8c) is NONE (-1), this is a
 * new channel: build properties, assert the permutation is cache-loaded,
 * then push via sound_channel_set_properties and start_new.
 *
 * If already playing, advance the channel queue (sound_channel_update_status),
 * handle pitch-range crossfading, and manage permutation sequencing for
 * looped playback (select next permutation, handle linked-permutation
 * chains, and looping-sound iteration transitions).
 *
 * Finally, apply the permutation's gain scalar, push properties, and
 * commit the channel update via vtable+0x1c. */
void sound_update_music_channel(int channel_index, float attenuation)
{
  short si = (short)channel_index;
  int *channel_ptr;
  char *sound_entry;
  char *tag_ptr;
  char *looping_sound;
  int *track_channel_ptr;
  char *lsnd_tag;
  char *track_tag;
  short class_index;
  float class_gain;
  float fraction;
  float local_c; /* gain from tag gain range * sound_entry+0x88 */
  int pitch_range;
  int permutation;

  /* Properties struct: 0x20 bytes (8 floats).
   *   +0x00 float min_distance (pitch)
   *   +0x04 float max_distance
   *   +0x08 float pan (gain)
   *   +0x0C float volume
   *   +0x10 float direction[3] (from tag+0x1c..0x24)
   *   +0x1C int   class_def_flags (class_def+0x10) */
  float properties[8];

  /* Validate channel_index. */
  if (si < 0 || si >= *(short *)0x4eb0b4) {
    display_assert("index>=0 && index<sound_manager_globals.channel_count",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
    system_exit(-1);
  }

  /* Resolve channel, sound entry, tag, and looping-sound. */
  channel_ptr = (int *)(0x4fc3a0 + (int)si * 0x18);
  sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, *channel_ptr);
  tag_ptr = (char *)tag_get(0x736e6421, *(int *)(sound_entry + 0x8));
  looping_sound =
    (char *)datum_get(*(data_t **)0x4fdba0, *(int *)(sound_entry + 0xc));
  track_channel_ptr =
    (int *)(looping_sound + 0xd4 + (int)*(short *)(sound_entry + 0x94) * 4);
  lsnd_tag = (char *)tag_get(0x6c736e64, *(int *)(looping_sound + 0x4));
  track_tag = (char *)tag_block_get_element(
    lsnd_tag + 0x3c, (int)*(short *)(sound_entry + 0x94), 0xa0);

  /* Cache interpolation fraction from sound_entry+0x18. */
  fraction = *(float *)(sound_entry + 0x18);

  /* Compute local_c: lerp(tag+0x44, tag+0x5c, fraction) * sound_entry+0x88. */
  local_c =
    ((*(float *)(tag_ptr + 0x5c) - *(float *)(tag_ptr + 0x44)) * fraction +
     *(float *)(tag_ptr + 0x44)) *
    *(float *)(sound_entry + 0x88);

  /* Assert this is not an impulse sound. */
  if (*(short *)(sound_entry + 2) == 0) {
    display_assert("sound->type!=_sound_impulse",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x9c4, 1);
    system_exit(-1);
  }

  /* Pitch: min-distance from sound class. */
  {
    float min_dist = *(float *)(tag_ptr + 8);
    if (min_dist == *(float *)0x2533c0) {
      void *cls = sound_class_get_definition(*(short *)(tag_ptr + 4));
      min_dist = *(float *)((char *)cls + 0x18);
    }
    properties[0] = min_dist;
  }

  /* Max distance: FLT_MAX. */
  *(int *)&properties[1] = 0x7f7fffff;

  /* Direction from tag+0x1c..0x24. */
  *(int *)&properties[4] = *(int *)(tag_ptr + 0x1c);
  *(int *)&properties[5] = *(int *)(tag_ptr + 0x20);
  *(int *)&properties[6] = *(int *)(tag_ptr + 0x24);

  /* Class definition flags. */
  {
    void *class_def =
      sound_class_get_definition(*(unsigned short *)(tag_ptr + 0x4));
    *(int *)&properties[7] = *(int *)((char *)class_def + 0x10);
  }

  /* Get class gain. */
  class_index = *(short *)(tag_ptr + 0x4);
  class_gain = sound_class_get_gain((int)(unsigned short)class_index);

  /* Non-ambient classes scale by global volume. */
  if (class_index != 0x2c && class_index != 0x2e && class_index != 0x2f) {
    class_gain = class_gain * *(float *)0x4eb0b0;
  }

  /* Compute composite volume:
   * lerp(tag+0x40, tag+0x58, fraction) * track+0x4 * tag+0x28 *
   * sound_entry+0x1c * class_gain * attenuation */
  properties[3] =
    (*(float *)(tag_ptr + 0x58) - *(float *)(tag_ptr + 0x40)) * fraction +
    *(float *)(tag_ptr + 0x40);
  properties[3] = properties[3] * *(float *)(track_tag + 0x4) *
                  *(float *)(tag_ptr + 0x28) * *(float *)(sound_entry + 0x1c) *
                  class_gain * attenuation;

  if (*(short *)(sound_entry + 0x8c) == -1) {
    /* New channel: build full properties and start permutation. */
    pitch_range = (int)tag_block_get_element(
      tag_ptr + 0x98, (int)*(short *)(sound_entry + 0x8e), 0x48);
    permutation = (int)tag_block_get_element(
      (char *)pitch_range + 0x3c, (int)*(short *)(sound_entry + 0x90), 0x7c);

    /* Volume: permutation+0x24 * existing volume. */
    properties[3] = properties[3] * *(float *)(permutation + 0x24);

    /* Pan: local_c * pitch_range+0x30. */
    properties[2] = local_c * *(float *)(pitch_range + 0x30);

    /* Assert permutation is cache-loaded. */
    if (!sound_cache_request_sound((void *)permutation, 0, 0, 0)) {
      display_assert("sound_cache_sound_loaded(permutation)",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x9da, 1);
      system_exit(-1);
    }

    /* Push properties and start the new permutation. */
    sound_channel_set_properties(si, 0, properties);
    sound_channel_start_new(si, permutation);

    /* Record this channel as playing in the sound entry. */
    *(short *)(sound_entry + 0x8c) = si;

    /* Commit the channel update. */
    (*(void (**)(int))(*(int *)0x4eaf48 + 0x1c))(channel_index);
    return;
  }

  /* Existing channel: resolve pitch range block. */
  pitch_range = (int)tag_block_get_element(
    tag_ptr + 0x98, (int)*(short *)(sound_entry + 0x8e), 0x48);

  /* Validate the existing playing channel index. */
  {
    short playing_channel = *(short *)(sound_entry + 0x8c);
    if (playing_channel < 0 || playing_channel >= *(short *)0x4eb0b4) {
      display_assert("index>=0 && index<sound_manager_globals.channel_count",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
      system_exit(-1);
    }
  }

  /* Volume crossfade toward target. */
  {
    short playing_channel = *(short *)(sound_entry + 0x8c);
    int playing_idx = (int)playing_channel;
    float channel_rate = *(float *)(0x4fc3ac + playing_idx * 0x18);
    local_c = sound_volume_crossfade(
      local_c, channel_rate * *(float *)(pitch_range + 0x20),
      *(float *)(tag_ptr + 0x2c));
  }

  /* Pan: local_c * pitch_range+0x30. */
  properties[2] = local_c * *(float *)(pitch_range + 0x30);

  /* Pitch-range crossfade: if the sound is in looping state and the
   * fade endpoints differ (or target attenuation not reached), try
   * selecting a new pitch range. If it changed, allocate a new looping
   * entry and start a crossfade. */
  if (*(short *)(sound_entry + 0x2) == 2 &&
      (*(int *)(sound_entry + 0xa4) == *(int *)(sound_entry + 0xa8) ||
       *(float *)(sound_entry + 0xa0) != *(float *)0x2533c0)) {
    short new_pitch_range = sound_select_pitch_range(
      tag_ptr, local_c, (int)(unsigned short)*(short *)(sound_entry + 0x8e));
    if (new_pitch_range != *(short *)(sound_entry + 0x8e)) {
      if (*channel_ptr == *track_channel_ptr && *(char *)0x4eaf43 == '\0') {
        int new_entry = sound_create_looping_entry(
          *(int *)(sound_entry + 0x8), *(int *)(sound_entry + 0xc),
          (int)(unsigned short)*(short *)(sound_entry + 0x94), 2);
        if (new_entry != -1) {
          sound_start_fade(1, *(float *)0x2c1278, new_entry, *channel_ptr);
          *track_channel_ptr = new_entry;
        }
      }
    }
  }

  /* Check if the sound should stop or continue. */
  if (*(short *)(sound_entry + 0x2) == 4)
    goto final_update;

  if (*(short *)(sound_entry + 0x2) == 1 && (*(uint8_t *)track_tag & 1) != 0)
    goto final_update;

  {
    short status = sound_channel_update_status(*(short *)(sound_entry + 0x8c));
    if (status == 2 && (*(uint8_t *)(sound_entry + 0x4) & 8) == 0) {
      /* Channel finished playing; check if permutation/next-sound chain
       * means we should skip to final update rather than queue more. */
      if (*(short *)(channel_ptr[4] + 0x2a) != -1)
        goto final_update;
      if (*(int *)(sound_entry + 0x98) == -1)
        goto final_update;
    }

    /* Permutation sequencing. */
    if (*(int *)(sound_entry + 0x98) == -1 ||
        (channel_ptr[4] != 0 && *(short *)(channel_ptr[4] + 0x2a) != -1)) {
      /* Next permutation ready or queued. */
      if ((*(uint8_t *)(sound_entry + 0x4) & 8) == 0) {
        short next_perm = sound_select_permutation(
          tag_ptr, *(unsigned short *)(sound_entry + 0x8e),
          *(unsigned short *)(sound_entry + 0x90));
        if (next_perm == -1) {
          if ((*(uint8_t *)tag_ptr & 2) == 0) {
            display_assert("TEST_FLAG(definition->flags, "
                           "_sound_definition_linked_permutations_bit)",
                           "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0xa1c,
                           1);
            system_exit(-1);
          }
          if ((*(uint8_t *)lsnd_tag & 2) == 0) {
            next_perm = sound_select_permutation(
              tag_ptr, *(unsigned short *)(sound_entry + 0x8e), -1);
            if (next_perm != -1)
              goto set_next_perm;
          } else {
            *(short *)(sound_entry + 0x2) = 4;
            *(uint8_t *)(looping_sound + 0x4e) = 1;
          }
        } else {
        set_next_perm:
          *(uint8_t *)(sound_entry + 0x4) |= 8;
          *(short *)(sound_entry + 0x90) = next_perm;
        }
      }
    } else {
      /* No next permutation and no queued: start next looping iteration. */
      sound_start_next_looping_permutation(*channel_ptr);
      tag_ptr = (char *)tag_get(0x736e6421, *(int *)(sound_entry + 0x8));
      pitch_range = (int)tag_block_get_element(
        tag_ptr + 0x98, (int)*(short *)(sound_entry + 0x8e), 0x48);
    }

    /* Get the current permutation and try to cache-load it. */
    permutation = (int)tag_block_get_element(
      (char *)pitch_range + 0x3c, (int)*(short *)(sound_entry + 0x90), 0x7c);
    if (*(short *)(sound_entry + 0x2) != 4 &&
        sound_cache_request_sound((void *)permutation, 0, 1, 1)) {
      *(uint8_t *)(sound_entry + 0x4) &= ~8;
      sound_channel_start_new((short)channel_index, permutation);

      /* If both the next-sound field (+0x98) and the permutation's
       * next-permutation link (+0x2a) are NONE, the chain has ended:
       * advance type 1->2 (start looping) or 3->4 (stop). */
      if (*(int *)(sound_entry + 0x98) == -1 &&
          *(short *)(permutation + 0x2a) == -1) {
        if (*(short *)(sound_entry + 0x2) == 1) {
          *(short *)(sound_entry + 0x2) = 2;
        } else if (*(short *)(sound_entry + 0x2) == 3) {
          *(short *)(sound_entry + 0x2) = 4;
        }
      }
    }
  }

final_update:
  /* Final permutation gain and property push. */
  permutation = (int)tag_block_get_element(
    (char *)pitch_range + 0x3c, (int)*(short *)(sound_entry + 0x90), 0x7c);
  properties[3] = properties[3] * *(float *)(permutation + 0x24);

  sound_channel_set_properties(si, 0, properties);

  /* Commit the channel update. */
  (*(void (**)(int))(*(int *)0x4eaf48 + 0x1c))(channel_index);
}

/* sound_start (0x1ce180)
 *
 * Attempts to start playing a sound from the given sound tag.
 *
 * 1. Loads the sound tag data via tag_get('snd!', sound_tag_index).
 * 2. Validates track_data_size <= 0x30 and source spatialization.
 * 3. For certain sound classes (0x2c, 0x2e, 0x2f), extends the minimum
 *    fade-out deadline and optionally forces spatialization to none.
 * 4. If sound system is initialized and hardware present, checks encoding
 *    compatibility (mono 22k or stereo 22k/44k compressed), volume/distance
 *    culling via random distance check, priority, and channel availability.
 * 5. Allocates a sound datum, fills in tag index, channel, source data,
 *    track data, pitch range, permutation, and timing info.
 * 6. Returns the new sound datum handle, or -1 on failure.
 */
int sound_start(int sound_tag_index, void *source, int object_handle,
                int track_data, void *track_data_ptr, int track_data_size)
{
  int result = -1;
  void *sound_tag;
  float source_scale;
  short sound_class;
  int game_time;
  int fade_deadline;
  short channel_index;
  short promotion_result;
  char *sound_entry;
  int ftol_result;
  short pitch_range_index;
  short permutation_index;

  sound_tag = tag_get(0x736e6421, sound_tag_index);
  source_scale = *(float *)((char *)source + 4);

  /* Assert: track_data_size <= MAXIMUM_SOUND_CALLBACK_DATA (0x30 = 48) */
  if ((short)track_data_size > 0x30) {
    display_assert("track_data_size<=MAXIMUM_SOUND_CALLBACK_DATA",
                   "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x240, 1);
    system_exit(-1);
  }

  if (*(short *)source != 0 &&
      !valid_real_normal3d((float *)((char *)source + 0x18))) {
    display_assert(
      "source->spatialization_mode==_sound_spatialization_mode_none || "
      "valid_real_normal3d(&source->location.forward)",
      "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x242, 1);
    system_exit(-1);
  }

  /* Check sound class for fade-related classes (0x2c, 0x2e, 0x2f). */
  sound_class = *(short *)((char *)sound_tag + 4);
  if (sound_class == 0x2c || sound_class == 0x2e || sound_class == 0x2f) {
    game_time = game_time_get();
    fade_deadline =
      game_time + *(int *)((char *)sound_tag + 0x84) * 30 / 1000 + 10;
    if (fade_deadline > *(int *)0x4eaf44)
      *(int *)0x4eaf44 = fade_deadline;
    /* If a global flag is set, force spatialization to none. */
    if (*(char *)0x4fc383 != '\0')
      *(short *)source = 0;
  }

  /* Class 0x2f always forces spatialization to none. */
  if (*(short *)((char *)sound_tag + 4) == 0x2f)
    *(short *)source = 0;

  if (*(uint8_t *)0x4eaf40 != 0 && *(uint8_t *)0x4eaf41 != 0) {
    /* Check encoding compatibility: must be 1 pitch range, and either
     * (0 channels + 0 compression) or (1 channel). */
    if (*(short *)((char *)sound_tag + 0x6e) == 1 &&
        ((*(short *)((char *)sound_tag + 0x6c) == 0 &&
          *(short *)((char *)sound_tag + 6) == 0) ||
         *(short *)((char *)sound_tag + 0x6c) == 1)) {
      /* Volume/distance culling: skip if both source scale and sound
       * skip_fraction are zero (always audible). */
      if (*(float *)((char *)source + 4) != 0.0f ||
          *(float *)((char *)sound_tag + 0x40) != 0.0f) {
        unsigned int *seed = random_math_get_local_seed_address();
        float random_val = random_math_real(seed);
        float skip_min = *(float *)((char *)sound_tag + 0x3c);
        float skip_max = *(float *)((char *)sound_tag + 0x54);
        float max_dist = *(float *)((char *)sound_tag + 0x10);
        if (((skip_max - skip_min) * source_scale + skip_min) * max_dist <
            random_val) {
          float priority = sound_get_default_priority(sound_tag_index);
          if (*(int *)((char *)sound_tag + 0x98) > 0) {
            void *pr0 =
              tag_block_get_element((char *)sound_tag + 0x98, 0, 0x48);
            if (*(int *)((char *)pr0 + 0x3c) > 0) {
              void *cls =
                sound_class_get_definition(*(short *)((char *)sound_tag + 4));
              if (*(char *)((char *)cls + 0x28) == '\0') {
                channel_index = sound_allocate_channel(source, priority);
                if (channel_index != -1) {
                  promotion_result = sound_check_promotion(sound_tag_index);
                  if (promotion_result != 0) {
                    if (promotion_result == 1) {
                      /* Promote: recurse with the promotion sound tag. */
                      return sound_start(*(int *)((char *)sound_tag + 0x7c),
                                         source, object_handle, track_data,
                                         track_data_ptr, track_data_size);
                    }
                    /* Reject (promotion_result >= 2). */
                    return -1;
                  }

                  /* Allocate a new sound datum. */
                  result = data_new_at_index(*(data_t **)0x4fdba4);
                  if (result != -1) {
                    sound_entry =
                      (char *)datum_get(*(data_t **)0x4fdba4, result);

                    /* 0x1ccca0: compute distance (EAX = channel_index,
                     * EDI = source). Returns float distance in ST(0). Then
                     * multiply by constant 8.9647 and convert to int. */
                    {
                      float dist = FUN_001ccca0(channel_index, source);
                      ftol_result = (int)(*(float *)0x2c1288 * dist);
                    }

                    /* Fill in sound entry fields. */
                    *(int *)(sound_entry + 0x8) = sound_tag_index;
                    *(short *)(sound_entry + 0x8c) = (short)-1;
                    *(short *)(sound_entry + 0x6) = channel_index;
                    *(short *)(sound_entry + 0x2) = 0;

                    /* Compute random scale for this sound instance. */
                    {
                      float rscale = sound_compute_random_scale(
                        *(float *)((char *)sound_tag + 0x14),
                        *(float *)((char *)sound_tag + 0x18),
                        *(float *)((char *)sound_tag + 0x44),
                        *(float *)((char *)sound_tag + 0x5c),
                        *(float *)((char *)source + 4));
                      *(float *)(sound_entry + 0x88) = rscale;
                    }

                    /* Copy source struct (0x40 bytes = 16 dwords) into sound
                     * entry at offset 0x14. */
                    *(short *)(sound_entry + 0x4) = 0;
                    *(int *)(sound_entry + 0xc) = object_handle;
                    qmemcpy(sound_entry + 0x14, source, 0x40);

                    /* Store track_data flag and copy track data if present. */
                    *(int *)(sound_entry + 0x10) = track_data;
                    if (track_data != 0) {
                      if (sound_entry + 0x54 == 0) {
                        display_assert(
                          "sound->track_data",
                          "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x28e, 1);
                        system_exit(-1);
                      }
                      csmemcpy(sound_entry + 0x54, track_data_ptr,
                               (int)(short)track_data_size);
                    }

                    /* Select pitch range and permutation. */
                    pitch_range_index = sound_select_pitch_range(
                      sound_tag, *(float *)(sound_entry + 0x88), -1);
                    *(short *)(sound_entry + 0x8e) = pitch_range_index;

                    permutation_index = sound_select_permutation(
                      sound_tag, pitch_range_index, -1);
                    *(short *)(sound_entry + 0x90) = permutation_index;

                    *(int *)(sound_entry + 0xa8) = 0;
                    *(int *)(sound_entry + 0xa4) = 0;
                    *(short *)(sound_entry + 0x94) = (short)-1;

                    /* Look up the permutation element in the sound tag and
                     * request it from the sound cache. */
                    sound_cache_request_sound(
                      tag_block_get_element(
                        (char *)tag_block_get_element(
                          (char *)tag_get(0x736e6421,
                                          *(int *)(sound_entry + 0x8)) +
                            0x98,
                          (int)pitch_range_index, 0x48) +
                          0x3c,
                        (int)permutation_index, 0x7c),
                      0, 1, 0);

                    /* Set timing: if ftol_result > 250, delay start by that
                     * amount relative to current timestamp, and set bit 0 of
                     * flags. */
                    if (ftol_result > 250) {
                      *(uint8_t *)(sound_entry + 4) =
                        *(uint8_t *)(sound_entry + 4) | 1;
                      *(int *)(sound_entry + 0x84) =
                        *(int *)0x4eaf4c + ftol_result;
                      return result;
                    }
                    *(int *)(sound_entry + 0x84) = *(int *)0x4eaf4c;
                    return result;
                  }
                }
              }
            }
          }
        }
      }
    } else {
      error(2, "attempt to play a sound that was not a mono 22k compressed "
               "sound or a stereo 22k or 44k compressed sound.");
    }
  }

  return result;
}

/* sound_update_music (0x1ceda0)
 *
 * Per-channel tick for spatialized sound playback. Iterates the global
 * channel table (0x4fc3a0, stride 0x18 bytes, count at 0x4eb0b4) and, for
 * each channel that holds a live sound:
 *
 *   1. Resolves the sound datum and its tag, then updates the channel's
 *      attenuation curve via sound_update_channel_attenuation (0x1cc310,
 *      @<eax>). If the computed attenuation and the sound entry's target
 *      attenuation (sound_entry+0xa0) both reach 0.0f, the sound has
 *      faded out — stop it via sound_stop_channel (0x1cca60, @<ebx>)
 *      and mark the channel free (-1).
 *   2. Otherwise, dispatches on the sound's spatialization mode
 *      (sound_entry+0x14). Two top-level branches select on the channel
 *      flags bit 0 (+0x4 & 1):
 *        - BIT SET: drive the sound driver directly. Mode 0 asserts,
 *          mode 1 transforms position/forward/up into the listener's
 *          frame (matrix3x3 transforms + velocity compensation via
 *          listener+0x38..0x40 scaled by 30.0) and issues vtable+0x30
 *          with the transformed triple plus sound_entry+0x4c/0x50 and
 *          listener[+1]. Mode 2 issues vtable+0x30 with the raw
 *          world-space position.
 *        - BIT CLEAR: compute an audible-volume scalar. Copy the source
 *          position, then for mode 1 fetch the listener via
 *          sound_listener_get (0x1cbac0, @<si>) and transform the
 *          position through real_matrix3x3_transform_point (0x1096e0).
 *          For modes 1 and 2, scale the current attenuation by
 *          1 - (sqrt(|pos|^2) - min_dist) / (max_dist - min_dist) using
 *          sound_get_default_priority variants (0x1c8d50 min-dist,
 *          0x1c8d10 max-dist), clamped to [0, 1]. Mode 0 leaves the
 *          attenuation unchanged.
 *   3. Update the per-channel volume/pan/pitch state. If the sound's
 *      channel kind (sound_entry+0x2) is 0, invoke sound_update_channel
 *      (0x1ccf80); otherwise invoke sound_update_music_channel
 *      (0x1cdc30). Both take (channel_index, attenuation).
 *   4. If the sound class is marked "pitch-track" (class_def+0x8) and
 *      the sound's update hook (sound_entry+0x10) matches the pitch
 *      callback at 0x1c7a10, evaluate the next pitch sample via
 *      sound_get_random_permutation_pitch (0x1c8f20) driven by the
 *      channel's current ftol-truncated phase (channel+0x8 -> int) and
 *      channel+0x10, then forward it to the pitch sink at 0x1c7b00 with
 *      sound_entry+0xc. */
void sound_update_music(void)
{
  short i;
  int channel_count;
  int *channel;
  char *sound_entry;
  void *tag_ptr;
  float attenuation;
  float pos[3];
  struct {
    float position[3];
    float forward[3];
    float up[3];
  } location;
  char *listener;

  channel_count = *(int *)0x4eb0b4;
  if ((short)channel_count <= 0)
    return;

  for (i = 0; i < (short)channel_count; i++, channel_count = *(int *)0x4eb0b4) {
    if (i < 0 || i >= (short)channel_count) {
      display_assert("index>=0 && index<sound_manager_globals.channel_count",
                     "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x428, 1);
      system_exit(-1);
    }

    channel = (int *)(0x4fc3a0 + (int)i * 0x18);
    if (channel[0] == -1)
      continue;

    sound_entry = (char *)datum_get(*(data_t **)0x4fdba4, channel[0]);
    tag_ptr = tag_get(0x736e6421, *(int *)(sound_entry + 0x8));

    attenuation = sound_update_channel_attenuation(channel[0]);

    if (attenuation == *(float *)0x2533c0 &&
        *(float *)(sound_entry + 0xa0) == *(float *)0x2533c0) {
      sound_stop_channel(channel[0]);
      channel[0] = -1;
      continue;
    }

    if ((*(uint8_t *)((char *)channel + 4) & 1) != 0) {
      short mode = *(short *)(sound_entry + 0x14);
      void *matrix;

      switch (mode) {
      case 0:
        display_assert(0, "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x7d4, 1);
        system_exit(-1);
        break;
      case 1:
        listener = (char *)sound_listener_get(*(short *)(sound_entry + 0x6));
        if (*listener == '\0') {
          display_assert("listener->valid",
                         "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x7db, 1);
          system_exit(-1);
        }

        matrix = listener + 4;
        real_matrix3x3_transform_point(matrix, (float *)(sound_entry + 0x20),
                                       location.position);
        real_matrix4x3_transform_point(matrix, sound_entry + 0x2c,
                                       location.forward);
        real_matrix3x3_transform_vector(
          matrix, (vector3_t *)(sound_entry + 0x38), (vector3_t *)location.up);

        location.up[0] = location.up[0] * 30.0f - *(float *)(listener + 0x38);
        location.up[1] = location.up[1] * 30.0f - *(float *)(listener + 0x3c);
        location.up[2] = location.up[2] * 30.0f - *(float *)(listener + 0x40);

        (*(void (**)(int, int, void *, int, int, int))(*(int *)0x4eaf48 +
                                                       0x30))(
          (int)i, 1, location.position, *(int *)(sound_entry + 0x4c),
          *(int *)(sound_entry + 0x50), (int)*(uint8_t *)(listener + 1));
        break;
      case 2:
        (*(void (**)(int, int, void *, int, int, int))(
          *(int *)0x4eaf48 + 0x30))((int)i, 1, (void *)(sound_entry + 0x20), 0,
                                    0, 0);
        break;
      default:
        display_assert(0, "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x7ec, 1);
        system_exit(-1);
        break;
      }
    } else {
      short mode;
      pos[0] = *(float *)(sound_entry + 0x20);
      pos[1] = *(float *)(sound_entry + 0x24);
      pos[2] = *(float *)(sound_entry + 0x28);
      mode = *(short *)(sound_entry + 0x14);
      switch (mode) {
      case 0:
        break;
      case 1:
        listener = (char *)sound_listener_get(*(short *)(sound_entry + 0x6));
        if (*listener == '\0') {
          display_assert("listener->valid",
                         "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x7fa, 1);
          system_exit(-1);
        }
        real_matrix3x3_transform_point(listener + 4,
                                       (float *)(sound_entry + 0x20), pos);
        break;
      case 2:
        break;
      default:
        display_assert(0, "c:\\halo\\SOURCE\\sound\\sound_manager.c", 0x80a, 1);
        system_exit(-1);
        break;
      }

      if (mode != 0) {
        float min_dist =
          sound_class_get_min_distance(*(int *)(sound_entry + 0x8));
        float max_dist =
          sound_get_default_priority(*(int *)(sound_entry + 0x8));
        float dist =
          xbox_sqrtf(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
        float falloff = 1.0f - (dist - min_dist) / (max_dist - min_dist);
        if (falloff < 0.0f)
          falloff = 0.0f;
        else if (falloff > 1.0f)
          falloff = 1.0f;
        attenuation = falloff * attenuation;
      }
    }

    if (*(short *)(sound_entry + 2) == 0) {
      sound_update_channel(i, attenuation);
    } else {
      sound_update_music_channel(i, attenuation);
    }

    {
      void *class_def =
        sound_class_get_definition(*(short *)((char *)tag_ptr + 4));
      /* Compare against the SYMBOL, not the literal 0x1c7a10.  The only
       * store site (object_impulse_sound_new, 0x1c7f48) is ported, so the
       * stored callback is our impl's address, never the original VA; a
       * literal compare never matches and lip-sync silently dies. */
      if (*(char *)((char *)class_def + 8) != '\0' &&
          *(void **)(sound_entry + 0x10) == (void *)&FUN_001c7a10) {
        float sample = sound_get_permutation_pitch(
          *(int *)((char *)channel + 0x10),
          (short)(int)*(float *)((char *)channel + 8));
        sound_pitch_push_sample(*(int *)(sound_entry + 0xc), sample);
      }
    }
  }
}

/* sound_pump (0x1cf2f0)
 *
 * Stripped-down sound tick used during fade-out spin loops.  Locks the
 * sound driver, updates timing + music, unlocks, then runs game_sound_update.
 */
void sound_idle(void)
{
  int current_ms;

  *(uint8_t *)0x4eaf43 = 1;
  if (*(uint8_t *)0x4eaf40 != 0 && *(uint8_t *)0x4eaf41 != 0) {
    (*(void (**)(void))(*(int *)0x4eaf48 + 0x10))();
    if (*(uint8_t *)0x4eaf42 == 0) {
      current_ms = system_milliseconds();
      *(float *)0x4eaf50 = ((float)current_ms - *(int *)0x4eaf4c) * 0.03f;
      *(int *)0x4eaf4c = current_ms;
      sound_update_music();
    }
    (*(void (**)(void))(*(int *)0x4eaf48 + 0x14))();
  }
  xbox_sound_cache_idle();
  *(uint8_t *)0x4eaf43 = 0;
}

void sound_dispose_from_old_map(void)
{
  int start_ms;
  int sound_index;
  float fade_end_ms;

  if (*(uint8_t *)0x4eaf42 == 0) {
    /* Only attempt fade if both initialized and hardware_present. */
    if (*(uint8_t *)0x4eaf40 == 0 || *(uint8_t *)0x4eaf41 == 0)
      goto skip_fade;

    /* Record start time and iterate all active sounds, triggering fade-out. */
    start_ms = system_milliseconds();
    sound_index = data_next_index(*(data_t **)0x4fdba4, -1);
    if (sound_index != -1) {
      do {
        /* sound_manager_fade: mode=0 (linear), seconds=0.3f,
         * fade_in_sound_index=NONE (-1), fade_out_sound_index=sound_index */
        sound_start_fade(0, 0.3f, -1, sound_index);
        sound_index = data_next_index(*(data_t **)0x4fdba4, sound_index);
      } while (sound_index != -1);

      /* Compute deadline: start_ms + 300.0f ms (constant at 0x2c1a60). */
      fade_end_ms = (float)start_ms + 300.0f;

      /* Spin until current time >= fade_end_ms, pumping sound each iteration.
       */
      while ((float)system_milliseconds() < fade_end_ms) {
        sound_idle();
      }
    }

    if (*(uint8_t *)0x4eaf42 == 0)
      goto skip_fade;
  }

  /* Clear the fading flag, stop hardware output, record current timestamp. */
  *(uint8_t *)0x4eaf42 = 0;
  (*(void (**)(int))((*(uint8_t **)0x4eaf48) + 0x28))(0);
  *(unsigned int *)0x4eaf4c = system_milliseconds();

skip_fade:
  /* Stop all active sound channels and reset channel count. */
  ((void (*)(void))0x1cd540)();

  /* Re-validate the looping-sounds table for the next map if present. */
  if (*(data_t **)0x4fdba0 != 0)
    data_make_valid(*(data_t **)0x4fdba0);
}

/* Per-frame sound rendering tick.
 *
 * Guarded by profiling markers (profile_enter/exit_private on "sound_render").
 * If the sound system is initialized (0x4eaf40) and hardware is present
 * (0x4eaf41):
 *   1. Call vtable+0x10 on the sound driver (lock / begin-frame).
 *   2. If not currently fading (0x4eaf42 == 0):
 *      a. Compute delta_ms = (current_ms - previous_ms) * 0.03f and store
 *         to the global sound delta (0x4eaf50). Update previous_ms (0x4eaf4c).
 *      b. Convert delta to integer ticks and pass to sound_cache_update
 *         (0x1c8c00) via the sound_listener_update result (0x1d9068).
 *      c. Run the sound subsystem pipeline: sound_update_channels (0x1ce9c0),
 *         sound_update_sources (0x1cf100), sound_update_output (0x1cd690),
 *         sound_update_effects (0x1cf360), sound_update_music (0x1ceda0).
 *      d. Toggle the per-frame flip flag at 0x4eaf54.
 *   3. Call vtable+0x14 on the sound driver (unlock / end-frame).
 * If not fading, also call game_sound_update (0x1bded0). */
void sound_render(void)
{
  int current_ms;
  float delta;

  /* Profiling: enter "sound_render" section. */
  if (*(uint8_t *)0x449ef1 != 0 && *(uint8_t *)0x32f6f0 != 0)
    profile_enter_private((void *)0x32f6e8);

  if (*(uint8_t *)0x4eaf40 != 0 && *(uint8_t *)0x4eaf41 != 0) {
    /* Lock / begin-frame on the sound driver. */
    (*(void (**)(void))(*(int *)0x4eaf48 + 0x10))();

    if (*(uint8_t *)0x4eaf42 == 0) {
      /* Compute time delta in sound-system units (ms * 0.03). */
      current_ms = system_milliseconds();
      delta = ((float)current_ms - *(int *)0x4eaf4c) * 0.03f;
      *(int *)0x4eaf4c = current_ms;
      *(float *)0x4eaf50 = delta;

      /* Update sound subsystems. The truncated delta is passed to the
       * cache/listener update chain. Original calls __ftol2 (0x1d9068) to
       * truncate the float delta; we use a plain C cast. */
      sound_classes_update((int)*(float *)0x4eaf50);
      FUN_001ce9c0();
      FUN_001cf100();
      FUN_001cd690();
      FUN_001cf360();
      sound_update_music();

      /* Toggle per-frame flip flag. */
      *(uint8_t *)0x4eaf54 = *(uint8_t *)0x4eaf54 == 0;
    }

    /* Unlock / end-frame on the sound driver. */
    (*(void (**)(void))(*(int *)0x4eaf48 + 0x14))();
  }

  /* Update game sound (ambient/scripted) when not fading. */
  if (*(uint8_t *)0x4eaf42 == 0)
    xbox_sound_cache_idle();

  /* Profiling: exit "sound_render" section. */
  if (*(uint8_t *)0x449ef1 != 0 && *(uint8_t *)0x32f6f0 != 0)
    profile_exit_private((void *)0x32f6e8);
}

/* FUN_001cf820 @ 0x1cf820 -- store a fixed data-block address through an
 * out-pointer.
 *
 * Reference (0x1cf820..0x1cf82e, 6 instructions):
 *   PUSH EBP / MOV EBP,ESP
 *   MOV EAX,[EBP+0x8]          ; first (and only) cdecl argument
 *   MOV dword ptr [EAX],0x32fce4
 *   POP EBP / RET              ; plain RET => cdecl, caller cleans up
 *
 * The function has no callees and no call sites recorded in the fingerprinted
 * evidence bundle, so the meaning of both the out-pointer and the target
 * address 0x32fce4 is UNKNOWN. It is written as an opaque data address rather
 * than named: 0x32fce4 sits just below the int16 table at 0x32fcf8 used by
 * sound_dsound_xbox.c, but nothing in the bundle proves a relationship.
 * Kept as a raw absolute address, matching the existing sound/ idiom. */
void FUN_001cf820(void **out_data)
{
  *out_data = (void *)0x32fce4;
}
