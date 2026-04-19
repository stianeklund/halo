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
  int _edi = tag_index;
  int *entry;

  asm volatile("movl $0x1b9bf0, %%ecx\n\t"
               "call *%%ecx"
               : "+D"(_edi), "=a"(entry)
               :
               : "ecx", "edx", "memory", "cc");

  if (entry[0] != group_tag && entry[1] != group_tag && entry[2] != group_tag) {
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
  int _edi = tag_index;
  int *entry;

  asm volatile("movl $0x1b9bf0, %%ecx\n\t"
               "call *%%ecx"
               : "+D"(_edi), "=a"(entry)
               :
               : "ecx", "edx", "memory", "cc");

  return (const char *)entry[4];
}
