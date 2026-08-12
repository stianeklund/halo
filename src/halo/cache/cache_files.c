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
