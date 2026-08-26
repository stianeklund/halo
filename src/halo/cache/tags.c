/* 0x1b9930 — tag_loaded: linear search for a loaded tag by group/name.
 * Returns tag index from tag instance +0x0c on match; otherwise -1.
 * Requires cache tags to be available (byte flag at 0x4e4d00). If the
 * global tag-instance table pointer (0x5054f0) is NULL while tags are
 * enabled, asserts and exits. Comparison uses case-insensitive CRT
 * string compare (__stricmp). */
int tag_loaded(int group_tag, const char *name, ...)
{
  int tag_count;
  int *entry;
  short index;

  if (*(uint8_t *)0x4e4d00 == 0) {
    return -1;
  }

  if (*(int **)0x5054f0 == 0) {
    display_assert("global_tag_instances",
                   "c:\\halo\\SOURCE\\cache\\cache_files.c", 0x127, true);
    system_exit(-1);
  }

  tag_count = *(int *)(*(int *)0x4e5504 + 0xc);
  if (tag_count <= 0) {
    return -1;
  }

  index = 0;
  do {
    entry = (int *)((int)*(int **)0x5054f0 + ((int)index << 5));
    if (entry[0] == group_tag &&
        crt_stricmp(name, (const char *)entry[4]) == 0) {
      return entry[3];
    }

    index = (short)(index + 1);
  } while ((int)index < tag_count);

  return -1;
}

/* 0x1ba0c0 — FUN_001ba0c0: dispose the currently-loaded structure_bsp tag's
 * geometry. Deregisters (waits idle on) the D3D vertex/index buffers for the
 * block pointer held in the global at 0x4e5508, resolves the tag instance
 * for the tag index at element+0x1c via tag_instance_resolve (0x1b9bf0),
 * asserts the instance has a base address and that its group tag is
 * STRUCTURE_BSP_TAG ('sbsp', 0x73627370 — same literal used for this group
 * in scenario.c), then clears the instance's base address (+0x14) and the
 * global block pointer. */
void FUN_001ba0c0(void *element)
{
  int *entry;

  structure_bsp_header_deregister_vertex_buffers(*(void **)0x4e5508);

  entry = tag_instance_resolve(*(int *)((char *)element + 0x1c));

  if (entry[5] == 0) {
    display_assert("tag_instance->base_address",
                   "c:\\halo\\SOURCE\\cache\\cache_files.c", 0xcd, true);
    system_exit(-1);
  }
  if (entry[0] != 0x73627370) {
    display_assert("tag_instance->group_tag==STRUCTURE_BSP_TAG",
                   "c:\\halo\\SOURCE\\cache\\cache_files.c", 0xce, true);
    system_exit(-1);
  }

  entry[5] = 0;
  *(void **)0x4e5508 = 0;
}

/* 0x1ba140 — tag_get: resolve a tag handle and return its base/data
 * pointer. Calls 0x1b9bf0 (tag_instance_resolve) with the 16-bit tag
 * index in EDI (hidden register param); that helper returns a pointer
 * to the tag instance record. The record stores the tag's own group
 * at +0, parent group at +4, grandparent group at +8, and data pointer
 * at +0x14. The group check accepts a match at any of the three levels
 * (supports parent-group lookups). Asserts if the group doesn't match
 * or if the data pointer is NULL. */
void *tag_get(int group_tag, int tag_index)
{
  int *entry;

  entry = tag_instance_resolve(tag_index);

  if (entry[0] != group_tag && entry[1] != group_tag && entry[2] != group_tag) {
    error(2, "expected tag group %08x but got %08x for datum %08x", group_tag,
          entry[0], tag_index);
    display_assert("expected tag group mismatch",
                   "c:\\halo\\SOURCE\\cache\\cache_files.c", 0xf7, true);
    system_exit(-1);
  }
  if (entry[5] == 0) {
    display_assert("can't get() a tag with a base address!",
                   "c:\\halo\\SOURCE\\cache\\cache_files.c", 0xfb, true);
    system_exit(-1);
  }
  return (void *)entry[5];
}

/* 0x1ba1f0 — tag_get_name: return the name string for a tag by index.
 * Calls tag_instance_resolve (0x1b9bf0) with the tag index in EDI,
 * then reads the name pointer at offset +0x10 of the tag instance record. */
const char *tag_get_name(int tag_index)
{
  int *entry;

  entry = tag_instance_resolve(tag_index);
  return (const char *)entry[4];
}

/* 0x1ba210 — tag_get_group_tag: return the primary group tag (4CC class
 * identifier) for a tag by index. Calls tag_instance_resolve (0x1b9bf0)
 * with the tag index in EDI, then reads the group tag at offset +0x00
 * of the tag instance record. */
int tag_get_group_tag(int tag_index)
{
  int *entry;

  entry = tag_instance_resolve(tag_index);
  return entry[0];
}

/* 0x1ba8b0 — FUN_001ba8b0: wait for the pending synchronous cache-copy
 * read to complete, then clear the "raw read in progress" bit.
 *
 * self arrives implicitly in ESI (first instruction reads [ESI+0x950]
 * with no preceding write to ESI) -- the same per-file decompression
 * read-state block documented at cache_files_windows.c's
 * cache_copy_initialize_read_data/FUN_001bb430/FUN_001bb8a0 (TU confirmed
 * via the __FILE__ assert strings below, matching
 * cache_files_decompress_windows.c). Sole caller is
 * cache_copy_initialize_read_data (xref 0x1bb84b, unconditional call),
 * where self is still live in ESI from that function's own @<eax> entry
 * parameter.
 *
 *   +0x950  manual-reset event handle (assert-proven role via
 *           FUN_001bc280's comment in cache_files_windows.c; passed here
 *           to WaitForSingleObjectEx)
 *   +0x994  overlapped_in_use_flags (assert-proven name, reused from
 *           FUN_001bb8a0's comment); bit 0x100 here is a distinct flag
 *           from the per-buffer bits 0-7 tested there -- the assert text
 *           names it "_raw_read_offset", so it is reproduced as a raw
 *           mask on the same field rather than folded into the per-buffer
 *           bit-vector helper.
 *
 * WaitForSingleObjectEx(handle, 5000, TRUE) is called first (disassembly:
 * 3 pushes -- 1, 0x1388, EAX -- immediately before CALL 0x1d00b9, cdecl
 * push order with no ADD ESP after, i.e. the callee-cleans WINAPI
 * convention already used by this TU's other Win32 wrapper thunks). Its
 * return value is compared against WAIT_IO_COMPLETION (0xc0) only after
 * the flag assert below, matching the disassembly order (Ghidra's own
 * decompile named the callee "WaitForSingleObjectEx()" but lost track of
 * the saved-EDI register holding the return value across the intervening
 * branch, misreporting the second compare as an "extraout_EAX" read --
 * disassembly's CMP EDI,0xc0 is authoritative).
 *
 * Source: c:\halo\SOURCE\cache\cache_files_decompress_windows.c, asserts
 * at lines 0x5ca, 0x5cb.
 */
void FUN_001ba8b0(char *self)
{
  unsigned int wait_result;

  wait_result = WaitForSingleObjectEx(*(void **)(self + 0x950), 0x1388, 1);

  if ((*(unsigned int *)(self + 0x994) & 0x100) != 0) {
    display_assert("!BIT_VECTOR_TEST_FLAG(self->overlapped_in_use_flags, "
                   "_raw_read_offset)",
                   "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c",
                   0x5ca, 1);
    system_exit(-1);
  }

  if (wait_result != 0xc0) {
    display_assert("wait_result==WAIT_IO_COMPLETION",
                   "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c",
                   0x5cb, 1);
    system_exit(-1);
  }

  *(unsigned int *)(self + 0x994) &= 0xfffffeff;
}

/* 0x1ba930 — FUN_001ba930: wait for the pending synchronous cache-copy
 * write to complete, then clear the "raw write in progress" bit.
 *
 * self arrives implicitly in ESI (first instruction reads [ESI+0x994]
 * with no preceding write to ESI) -- the same per-file decompression
 * read-state block used by FUN_001ba8b0 above. Sole caller is
 * cache_copy_initialize_read_data (xref 0x1bb806, unconditional call),
 * where self is passed explicitly from that function's own parameter
 * (see the updated call site and comment in cache_files_windows.c).
 *
 *   +0x950  manual-reset event handle (same field FUN_001ba8b0 waits on)
 *   +0x994  overlapped_in_use_flags; bit 0x400 here is "_raw_write_offset"
 *           per the assert text, distinct from FUN_001ba8b0's bit 0x100
 *           "_raw_read_offset"
 *
 * Unlike FUN_001ba8b0, this function asserts the write-in-progress bit
 * is SET before waiting (disassembly: TEST AH,0x4 / JNZ over the assert
 * -- fires when the bit is clear), then calls WaitForSingleObjectEx(handle,
 * 5000, TRUE), then asserts the bit is CLEAR (fires when still set), then
 * asserts wait_result==WAIT_IO_COMPLETION (0xc0), then clears the bit.
 * Disassembly saves the wait's return value across the intervening asserts
 * in EDI (PUSH EDI / ... / MOV EDI,EAX / ... / CMP EDI,0xc0 / POP EDI),
 * matching FUN_001ba8b0's own note about Ghidra losing track of this and
 * misreporting the compare as "extraout_EAX".
 *
 * Source: c:\halo\SOURCE\cache\cache_files_decompress_windows.c, asserts
 * at lines 0x5d6, 0x5da, 0x5db.
 */
void FUN_001ba930(char *self)
{
  unsigned int wait_result;

  if ((*(unsigned int *)(self + 0x994) & 0x400) == 0) {
    display_assert("BIT_VECTOR_TEST_FLAG(self->overlapped_in_use_flags, "
                   "_raw_write_offset)",
                   "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c",
                   0x5d6, 1);
    system_exit(-1);
  }

  wait_result = WaitForSingleObjectEx(*(void **)(self + 0x950), 0x1388, 1);

  if ((*(unsigned int *)(self + 0x994) & 0x400) != 0) {
    display_assert("!BIT_VECTOR_TEST_FLAG(self->overlapped_in_use_flags, "
                   "_raw_write_offset)",
                   "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c",
                   0x5da, 1);
    system_exit(-1);
  }

  if (wait_result != 0xc0) {
    display_assert("wait_result==WAIT_IO_COMPLETION",
                   "c:\\halo\\SOURCE\\cache\\cache_files_decompress_windows.c",
                   0x5db, 1);
    system_exit(-1);
  }

  *(unsigned int *)(self + 0x994) &= 0xfffffbff;
}
