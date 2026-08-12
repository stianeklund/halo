/* Cache file / scenario tag lifetime management.
 * Source: c:\halo\SOURCE\cache\cache_files.c */

/* Pointer to the loaded tags header. Loaded (MOV EAX,[0x4e5504]) rather than
 * addressed, so the global holds the pointer. The tag count lives at +0xc
 * (see cache/tags.c and tag_files/tag_groups.c). */
#define tags_header (*(void **)0x4e5504)

/* Byte flag gating all cache tag access; cleared here on unload.
 * MOV [0x4e4d00],AL -- byte width, not dword. */
#define cache_tags_available (*(uint8_t *)0x4e4d00)

/* Base of the 0x20-stride tag instance array. Cleared on unload. */
#define tag_instances (*(void **)0x5054f0)

/* Tear down everything the loaded scenario's tags own: the sound and texture
 * caches, the open cache file, and the tags header's vertex/index buffers.
 * Then mark the cache tags unavailable and drop the tag instance base. */
void scenario_tags_unload(void)
{
  sound_cache_close();
  texture_cache_close();
  cache_file_close();
  tags_header_deregister_vertex_and_index_buffers(tags_header);
  cache_tags_available = 0;
  tag_instances = 0;
}

/* Dword stamp compared against cache header field +0x128 when deciding whether
 * a saved game state still matches the loaded map (see the validity gate in
 * cache/cache_files_windows.c, which checks it alongside the header magic, the
 * build string, the scenario name and the difficulty). Whether the value is a
 * checksum, a map id or something else is NOT established by the binary, so the
 * name stays mechanical. No kb.json entry exists for this address. */
#define global_4e4d68 (*(int *)0x4e4d68)

/* Body is exactly `MOV EAX,[0x4e4d68]; RET` -- no frame, no callees. */
int FUN_001b9920(void)
{
  return global_4e4d68;
}

/* ASCII 'head' / 'foot'. The two magic words bracket the 0x800-byte cache
 * file header: 'head' at +0x00, 'foot' at +0x7fc. */
#define CACHE_FILE_HEAD_MAGIC 0x68656164
#define CACHE_FILE_FOOT_MAGIC 0x666f6f74

/* Header version this build understands. Anything else is reported as an old
 * version rather than as a malformed file. */
#define CACHE_FILE_VERSION 5

/* Largest accepted total file size, 0x11600000 = 278 MiB. The compare is
 * `CMP EAX,0x11600000 / JG fail`, so the bound is inclusive. */
#define CACHE_FILE_MAXIMUM_SIZE 0x11600000

/* Longest accepted scenario name in the header's +0x20 field, tested with an
 * UNSIGNED compare (`CMP EAX,0x1f / JA fail`). */
#define CACHE_FILE_MAXIMUM_NAME_LENGTH 0x1f

/* 0x1b9ce0 -- validate a cache file header that has already been read into
 * memory. `path` names the file for the diagnostics; the build-mismatch
 * message instead reports the header's own scenario name and build string.
 *
 * With report_errors clear a bad header simply returns false. With it set the
 * failure is fatal: the message is formatted into the shared error buffer,
 * handed to display_assert, and the process exits. report_errors is tested as
 * a BYTE in the original (MOV AL,[EBP+0x10] / TEST AL,AL), hence the cast. */
bool cache_file_header_verify(void *header, const char *path, int report_errors)
{
  if (*(int *)header == CACHE_FILE_HEAD_MAGIC &&
      *(int *)((char *)header + 0x7fc) == CACHE_FILE_FOOT_MAGIC &&
      *(int *)((char *)header + 8) >= 0 &&
      *(int *)((char *)header + 8) <= CACHE_FILE_MAXIMUM_SIZE &&
      (unsigned int)csstrlen((const char *)header + 0x20) <=
        CACHE_FILE_MAXIMUM_NAME_LENGTH) {
    if (*(int *)((char *)header + 4) != CACHE_FILE_VERSION) {
      if ((char)report_errors) {
        display_assert(csprintf(error_string_buffer,
                                "the cache file '%s' is an old version", path),
                       "c:\\halo\\SOURCE\\cache\\cache_files.c", 0x1f1, 1);
        system_exit(-1);
      }
      return false;
    }
    if (csstrcmp((const char *)header + 0x40, "01.10.12.2276") != 0) {
      if ((char)report_errors) {
        display_assert(
          csprintf(error_string_buffer,
                   "the cache file '%s' belongs to a different build (%s)",
                   (const char *)header + 0x20, (const char *)header + 0x40),
          "c:\\halo\\SOURCE\\cache\\cache_files.c", 0x1f6, 1);
        system_exit(-1);
      }
      return false;
    }
    return true;
  }
  if ((char)report_errors) {
    display_assert(csprintf(error_string_buffer,
                            "'%s' does not appear to be a cache file", path),
                   "c:\\halo\\SOURCE\\cache\\cache_files.c", 0x1ed, 1);
    system_exit(-1);
  }
  return false;
}

/* 0x1b9de0 -- advance the background map precache by one slice. Called from
 * the main loop and from the network client while it waits for a map.
 *
 * Returns true only on the one early exit where the map is already precached
 * (MOV AL,0x1 at 001b9df8). Every other tail returns the same zero flag
 * register -- BL, zeroed by XOR BL,BL in the prologue and read back as
 * MOV AL,BL at 001b9e42 and 001b9e65 -- hence the single `done` local rather
 * than separate false literals.
 *
 * The stale-copy check runs first: if a copy is in progress for some OTHER
 * map, end it before looking at any status. Then either poll the running copy
 * or start a new one. A status of 2 and a map_begin that returns false share
 * the single damaged-media tail at 001b9e5f. `progress` is an out-param for
 * cache_files_precache_map_status and is never read here. */
bool cache_files_give_time_to_precache(const char *name)
{
  bool done = false;
  float progress;

  if (cache_files_precache_map_loaded((char *)name))
    return true;

  if (cache_files_precache_in_progress() &&
      !cache_files_precache_is_copying_map((char *)name))
    cache_files_precache_map_end();

  if (cache_files_precache_in_progress()) {
    /* CMP AX,0x2 then CMP AX,0x1 -- a 16-bit compare on the declared
     * __int16 return, so the status stays narrow. */
    int16_t status = cache_files_precache_map_status(&progress);
    if (status != 2) {
      if (status != 1)
        return done;
      cache_files_precache_map_end();
      return done;
    }
  } else {
    cache_files_precache_set_priority(false);
    if (cache_files_precache_map_begin((char *)name, false))
      return done;
  }
  display_error_damaged_media();
  return done;
}

/* The 0x800-byte cache file header, read from disk into this fixed buffer by
 * cache_file_open and validated by cache_file_header_verify (which reads the
 * 'foot' magic at +0x7fc, so the buffer is at least 0x800 bytes). Addressed,
 * not loaded: PUSH 0x4e4d04 at 001b9e8f and 001b9eb8. */
#define cache_file_header ((void *)0x4e4d04)

/* Header fields +0x10 and +0x14, loaded absolutely (MOV EDX,[0x4e4d14] /
 * MOV ECX,[0x4e4d18]) and handed straight to cache_file_read as its offset
 * and size arguments. */
#define cache_file_tag_data_offset (*(int *)0x4e4d14)
#define cache_file_tag_data_size (*(unsigned int *)0x4e4d18)

/* Big-endian FourCC 'tags' stamped at tags header +0x20. The mismatch message
 * prints the four bytes high offset first and says they "should be 'tags'", so
 * the dword 0x74616773 sits in memory as 's','g','a','t'. */
#define TAGS_HEADER_SIGNATURE 0x74616773

/* Bytes of tag memory scrubbed with the 0xcd uninitialized-fill pattern before
 * the tag block is read over them. 0x1600000 = 22 MiB. */
#define CACHE_FILE_TAGS_BUFFER_SIZE 0x1600000

/* 0x1b9e70 -- open the named map's cache file and load its tag block.
 *
 * Returns -1 on either failure (the file would not open, or its header did not
 * validate); EBX is seeded with OR EBX,0xffffffff in the prologue and is the
 * value returned by both early exits. On success the result is the tags
 * header's +0x4 field, re-read through the just-stored global rather than from
 * the local buffer pointer (MOV EDX,[0x4e5504] / MOV EAX,[EDX+4]).
 *
 * cache_file_open gets the path-stripped name; cache_file_header_verify gets
 * the ORIGINAL `map_name` for its diagnostics. Both spellings live in ESI/EDI
 * in the original and ESI is later reused for the tag buffer, which is exactly
 * the register-aliasing case the decompiler collapses.
 *
 * The read is issued asynchronously (async_flag 1) and its return is discarded;
 * completion is awaited by spinning on the byte out-param at EBP-1. */
int FUN_001b9e70(const char *map_name)
{
  int result = -1;
  char completion_flag;
  const char *stripped_name;
  void *tags_base;
  unsigned int signature;

  stripped_name = tag_name_strip_path(map_name);
  texture_cache_open();
  FUN_001bdec0();
  if (cache_file_open(stripped_name, cache_file_header)) {
    tags_base = (void *)FUN_001bdd50();
    if (cache_file_header_verify(cache_file_header, map_name, 1)) {
      csmemset(tags_base, 0xcd, CACHE_FILE_TAGS_BUFFER_SIZE);
      cache_file_read(-1, cache_file_tag_data_offset, cache_file_tag_data_size,
                      (int)tags_base, &completion_flag, 1);
      while (completion_flag == 0)
        SwitchToThread();

      tags_header = tags_base;
      /* The signature dword stays live across the compare: the fourth %c is
       * its low byte (MOVSX EAX,CL), not a fresh load of +0x20. That is why
       * Ghidra renders that one operand as (char)puVar2[8] while the other
       * three stay byte loads. */
      signature = *(unsigned int *)((char *)tags_base + 0x20);
      if (signature != TAGS_HEADER_SIGNATURE) {
        display_assert(csprintf(error_string_buffer,
                                "signature is '%c%c%c%c', should be '%c%c%c%c'",
                                ((char *)tags_base)[0x23],
                                ((char *)tags_base)[0x22],
                                ((char *)tags_base)[0x21], (char)signature, 't',
                                'a', 'g', 's'),
                       "c:\\halo\\SOURCE\\cache\\cache_files.c", 0x61, 1);
        system_exit(-1);
      }

      tag_instances = *(void **)tags_base;
      /* Both of these re-read the global instead of reusing the buffer
       * pointer: the tail holds two separate loads of 0x4e5504, one into EAX
       * for the push and one into EDX for the returned +0x4 field. */
      tags_header_register_vertex_and_index_buffers(tags_header);
      cache_tags_available = 1;
      result = *(int *)((char *)tags_header + 4);
    }
  }
  return result;
}
