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
