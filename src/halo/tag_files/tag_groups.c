/*
 * tag_groups.c — tag system utility functions.
 * Source: c:\halo\SOURCE\tag_files\tag_groups.c
 *
 * tag_block_t layout (confirmed from binary accesses at 0x19b210):
 *   +0x00 int32  count
 *   +0x04 void * address   (base pointer to element data)
 *   +0x08 void * definition (pointer to block_definition, may be NULL)
 *
 * tag_block_definition_t layout (confirmed from binary accesses):
 *   +0x00 char * name          (pointer to name string)
 *   +0x0C int32  element_size
 *
 * tag_data_t layout (confirmed from binary accesses at 0x19b1a0):
 *   +0x00 int32  size
 *   +0x0C void * address (pointer to raw data buffer)
 *   Fields at +0x04 and +0x08 are not accessed by these functions.
 */

/* Minimal local struct describing the in-memory tag data header. */
typedef struct {
  int size; /* +0x00 */
  char pad_4[8]; /* +0x04..+0x0B unknown */
  void *address; /* +0x0C */
} tag_data_t;

/* Minimal local struct describing the in-memory tag block header. */
typedef struct {
  int count; /* +0x00 */
  void *address; /* +0x04 */
  void *definition; /* +0x08 */
} tag_block_t;

/* Minimal local struct describing the block definition. Only the fields
 * accessed by tag_block_get_element are modelled here. */
typedef struct {
  const char *name; /* +0x00 */
  char pad_4[8]; /* +0x04 unused in this function */
  int element_size; /* +0x0C */
} tag_block_definition_t;

/* Strips the directory path from a tag name, returning a pointer to the
 * basename (the portion after the last backslash). If no backslash is found,
 * returns the original pointer unchanged. The tag_name must not be NULL.
 * Source: c:\halo\SOURCE\tag_files\tag_files.c, line 0x55e. */
const char *tag_name_strip_path(const char *tag_name)
{
  char *last_sep;

  if (tag_name == NULL) {
    display_assert("name", "c:\\halo\\SOURCE\\tag_files\\tag_files.c", 0x55e,
                   true);
    system_exit(-1);
  }

  last_sep = strrchr((char *)tag_name, '\\');
  if (last_sep != NULL)
    return last_sep + 1;

  return tag_name;
}

/* Returns a pointer into the raw data buffer of a tag_data at the given
 * offset. Asserts that size >= 0 and that offset+size fits within the
 * tag_data's total size. Used to obtain typed pointers into tag data blobs.
 * Source: c:\halo\SOURCE\tag_files\tag_groups.c, lines ~0xC01-0xC02. */
void *tag_data_get_pointer(void *tag_data, int offset, int size)
{
  tag_data_t *data = (tag_data_t *)tag_data;

  /* assert: size>=0 */
  if (size < 0) {
    display_assert("size>=0", "c:\\halo\\SOURCE\\tag_files\\tag_groups.c",
                   0xc01, true);
    system_exit(-1);
  }

  /* assert: offset>=0 && offset+size<=data->size */
  if (offset < 0 || offset + size > data->size) {
    display_assert("offset>=0 && offset+size<=data->size",
                   "c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 0xc02, true);
    system_exit(-1);
  }

  return (char *)data->address + offset;
}

/* Returns a pointer to element [index] within the tag block, or asserts
 * and exits on any invalid input. element_size must match the definition's
 * recorded element_size when a definition is present. Used widely across
 * many subsystems to index into tag data arrays. */
void *tag_block_get_element(void *block, int index, int element_size)
{
  tag_block_t *b = (tag_block_t *)block;
  tag_block_definition_t *def = NULL;

  /* assert: block != NULL */
  if (b == NULL) {
    display_assert("block", "c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 0xc0c,
                   true);
    system_exit(-1);
  }

  /* assert: block->count >= 0 */
  if (b->count < 0) {
    display_assert("block->count>=0",
                   "c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 0xc0d, true);
    system_exit(-1);
  }

  def = (tag_block_definition_t *)b->definition;

  /* assert: !block->definition || block->definition->element_size==element_size
   */
  if (def != NULL && def->element_size != element_size) {
    display_assert(
      "!block->definition || block->definition->element_size==element_size",
      "c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 0xc0e, true);
    system_exit(-1);
  }

  /* assert: index is in [0, block->count)
   * 0x25b724 is the literal "<unknown>" string in the XBE, used as fallback
   * when no block definition (and thus no name) is available. */
  if (index < 0 || index >= b->count) {
    const char *name =
      (def != NULL) ? def->name : (const char *)0x25b724; /* "<unknown>" */
    csprintf((char *)0x5ab100, "#%d is not a valid %s index in [#0,#%d)", index,
             name, b->count);
    display_assert((const char *)0x5ab100,
                   "c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 0xc11, true);
    system_exit(-1);
  }

  /* assert: block->address != NULL */
  if (b->address == NULL) {
    display_assert("block->address",
                   "c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 0xc12, true);
    system_exit(-1);
  }

  return (char *)b->address + index * element_size;
}

/* 0x19b320 — no-op placeholder (cache build: read-only tag groups). */
void FUN_0019b320(void)
{
  return;
}

/* 0x19b3a0 — reset the cached localization tag index to -1.
 * Clears the module-level tag_index stored at 0x4d9b08. */
void FUN_0019b3a0(void)
{
  *(int *)0x4d9b08 = -1;
  return;
}

/* 0x19b3b0 — no-op placeholder (cache build: read-only tag groups). */
void FUN_0019b3b0(void)
{
  return;
}

/* Stub: tag_block_resize is not supported when running from a cache file
 * (the map is memory-mapped read-only). Logs an error and returns false.
 * In the tools build this would perform actual reallocation. */
bool tag_block_resize(void *block, int count)
{
  error(2, "tag_block_resize() is not supported with a cache file active");
  return false;
}

/* Stub: tag_data_resize is not supported when running from a cache file
 * (the map is memory-mapped read-only). Logs an error and returns false.
 * In the tools build this would perform actual reallocation. */
bool tag_data_resize(void *tag_data, int size)
{
  error(2, "tag_data_resize() is not supported with a cache file active");
  return false;
}

/* Stub: tag_block_add_element is not supported when running from a cache file
 * (the map is memory-mapped read-only). Logs an error and returns -1.
 * In the tools build this would append a new element and return its index. */
int16_t tag_block_add_element(void *tag_block)
{
  error(2, "tag_block_add_element() is not supported with a cache file active");
  return -1;
}

/* Stub: tag_load is not supported when running from a cache file.
 * Logs an error and returns an invalid tag handle (-1).
 * All three parameters are intentionally unused in this stub. */
int FUN_001b9b00(int tag_class, const char *name, int flags)
{
  (void)tag_class;
  (void)name;
  (void)flags;
  error(2, "tag_load() is not supported with a cache file active");
  return -1;
}

/* Stub: tag_file_get_path is not supported when running from a cache file.
 * Logs an error and writes a NUL byte to the output path buffer. */
void FUN_001b9b30(int tag_class, int param_2, char *out_path)
{
  (void)tag_class;
  (void)param_2;
  error(2, "tag_file_get_path() is not supported with a cache file active");
  *out_path = 0;
}

/* Stub: tag_reference_set is not supported when running from a cache file.
 * Logs an error and returns immediately. */
void FUN_001b9b50(void)
{
  error(2, "tag_reference_set() is not supported with a cache file active");
  return;
}

/* Tag group iterator init: initialize iterator state at 'state' for tag class
 * 'tag_class'. Sets the index counter to 0 and stores the class filter. */
void FUN_001b9b60(int state, int tag_class)
{
  *(short *)(state + 4) = 0;
  *(int *)(state + 0x10) = tag_class;
  return;
}

/* Tag group iterator step: advance iterator 'state' to the next tag group
 * entry matching the stored class filter. Returns the datum handle
 * (entry[3]) on match, or -1 when no more entries remain.
 * 0x4e5504: ptr to globals (count at +0xc); 0x5054f0: tag group table base */
int FUN_001b9b80(int state)
{
  int iVar1;
  int iVar2;
  int *piVar3;

  iVar2 = -1;
  if ((int)*(short *)(state + 4) < *(int *)(*(int *)0x4e5504 + 0xc)) {
    while (1) {
      piVar3 = (int *)(*(short *)(state + 4) * 0x20 + *(int *)0x5054f0);
      *(short *)(state + 4) = *(short *)(state + 4) + 1;
      if ((piVar3 != (int *)0) &&
          (iVar1 = *(int *)(state + 0x10),
           iVar1 == -1 || iVar1 == piVar3[0] || iVar1 == piVar3[1] || iVar1 == piVar3[2])) {
        break;
      }
      if (*(int *)(*(int *)0x4e5504 + 0xc) <= (int)*(short *)(state + 4)) {
        return iVar2;
      }
    }
    iVar2 = piVar3[3];
  }
  return iVar2;
}
