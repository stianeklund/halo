#define FILE_REF_MAGIC 0x66696C6F

#define FIND_FILES_RECURSIVE_BIT 1
#define FIND_FILES_DIRECTORIES_BIT 2

/* File-reference flag families. See docs/halocea/README.md for the corpus and
 * .claude/skills/naming-confidence for the name_source tiers.
 *
 * All four BOUNDS were read out of this build before the corpus was consulted,
 * and all four agree with it exactly:
 *
 *   reference_info flags  files.c:0x1fe          rejects & 0xfffe  => 1
 *   name flags            files.c:0xba           rejects & 0xfff0  => 4
 *   find_files flags      files_windows.c:0x224  rejects & ~3      => 2
 *   permission flags      files_windows.c:0x134  rejects & ~7      => 3
 *
 * Unusually for this lane, most MEMBER names are T1 here too: 2276 stamps the
 * identifiers themselves into assert strings, and those asserts also pin the
 * bit values.
 *
 *   _has_filename_bit           files.c:0x8a and files_windows.c:0x225 assert
 *                               !TEST_FLAG(info->flags, _has_filename_bit)
 *                               against `& 1`                       => bit 0
 *   _name_directory_bit         0xbc asserts flags !=
 * (FLAG(_name_directory_bit) _name_extension_bit         |
 * FLAG(_name_extension_bit)) against `== 9`, so the pair is bits 0 and 3; 0xbd
 * asserts _name_directory_bit vs _name_parent_directory_bit against `(flags&1)
 * && (flags&2)`, fixing directory=0, so extension=3. _name_parent_directory_bit
 * from the same 0xbd pair                => bit 1 _name_filename_bit T1 BY
 * EXHAUSTION: bits 0, 1 and 3 are each named verbatim above and
 * NUMBER_OF_NAME_FLAGS is 4 (proven by the 0xfff0 reject), so bit 2 is forced.
 * Both halves are needed for this to be T1 rather than a guess.
 *   _permission_read_bit        0x135 asserts flags &
 * (FLAG(_permission_read_bit) _permission_write_bit       |
 * FLAG(_permission_write_bit)) against `& 3`; 0x136 asserts
 * _permission_write_bit against
 *                               `& 2`, so write=1 and read=0.
 *   _permission_append_bit      named at 0x136 against `& 4`           => bit 2
 *                               Corroborated below: bit 0 maps to GENERIC_READ,
 *                               bit 1 to GENERIC_WRITE, bit 2 to a seek-to-end.
 *
 * find_files members carry no 2276 string and are name_source: halocea, T2 —
 * DB-verified there (types_enum_values _3BB901B9596B139CD611B9F64EADD532), and
 * consistent with the existing FIND_FILES_*_BIT masks above, which this commit
 * deliberately leaves alone (they are masks under a *_BIT name; correcting that
 * is a rename, not a constants change, and belongs in its own commit).
 */
enum reference_info_flags {
  _has_filename_bit = 0,
  NUMBER_OF_REFERENCE_INFO_FLAGS = 1
};

enum name_flags {
  _name_directory_bit = 0,
  _name_parent_directory_bit = 1,
  _name_filename_bit = 2,
  _name_extension_bit = 3,
  NUMBER_OF_NAME_FLAGS = 4
};

enum find_files_flags {
  _find_files_recursive_bit = 0,
  _find_files_enumerate_directories_bit = 1,
  NUMBER_OF_FIND_FILES_FLAGS = 2
};

enum permission_flags {
  _permission_read_bit = 0,
  _permission_write_bit = 1,
  _permission_append_bit = 2,
  NUMBER_OF_PERMISSION_FLAGS = 3
};

/* Bits at or above each bound; what the VALID_FLAGS guards reject. Spelled to
 * keep the original immediate: the reference-info and name sites load a 16-bit
 * word, the find-files and permission sites a 32-bit one. */
#define REFERENCE_INFO_FLAGS_INVALID_MASK 0xfffe
#define NAME_FLAGS_INVALID_MASK 0xfff0
#define FIND_FILES_FLAGS_INVALID_MASK (~3)
#define PERMISSION_FLAGS_INVALID_MASK (~7)

typedef int(__stdcall *find_first_file_fn)(const char *path, void *find_data);
typedef bool(__stdcall *find_next_file_fn)(int handle, void *find_data);
typedef bool(__stdcall *close_handle_fn)(int handle);
typedef int(__stdcall *create_file_fn)(
  const char *path, uint32_t desired_access, uint32_t share_mode,
  void *security_attributes, uint32_t creation_disposition,
  uint32_t flags_and_attributes, int template_file);
typedef int(__stdcall *set_file_pointer_fn)(int handle, int distance_to_move,
                                            int *distance_high,
                                            uint32_t move_method);
typedef int(__stdcall *get_file_size_fn)(int handle, int *size_high);
typedef bool(__stdcall *read_file_fn)(int handle, void *buffer,
                                      uint32_t number_of_bytes_to_read,
                                      int *number_of_bytes_read,
                                      void *overlapped);
typedef uint16_t (*intl_string_prev_char_fn)(const char *str, int16_t *index);
typedef int (*is_alpha_fn)(int c);
typedef void (*debug_log_fn)(int level, const char *format, ...);
typedef uint32_t(__stdcall *xget_last_error_fn)(void);
typedef void(__stdcall *xset_last_error_fn)(uint32_t error);
typedef int(__stdcall *nt_create_file_fn)(const char *path, int access);
typedef bool(__stdcall *remove_directory_fn)(const char *path);
typedef bool(__stdcall *set_file_attributes_fn)(const char *path,
                                                uint32_t attributes);
typedef bool(__stdcall *delete_file_fn)(const char *path);
typedef bool(__stdcall *move_file_fn)(const char *existing_path,
                                      const char *new_path);
/* 0x1d0ee1: stdcall (no ADD ESP after the 3 pushes at 0x19ad94); the caller
 * tests the full EAX (TEST EAX,EAX), so the return is int, not bool. */
typedef int(__stdcall *get_file_attributes_ex_fn)(const char *path,
                                                  int info_level, void *data);

/* WIN32_FILE_ATTRIBUTE_DATA, 0x24 bytes (frame slot EBP-0x24..EBP at
 * 0x19ad30); last_write_time is at +0x14 (EBP-0x10 at the memcpy site). */
typedef struct file_attribute_data_s {
  uint32_t attributes; /* 0x00 */
  uint32_t creation_time[2]; /* 0x04 */
  uint32_t last_access_time[2]; /* 0x0c */
  uint32_t last_write_time[2]; /* 0x14 */
  uint32_t file_size_high; /* 0x1c */
  uint32_t file_size_low; /* 0x20 */
} file_attribute_data_t;

#define XFindFirstFile \
  ((find_first_file_fn)0x1d3576) /* hazard-ok: fnptr-conv */
#define XFindNextFile                                    \
  ((find_next_file_fn)0x1d3683) /* hazard-ok: fnptr-conv \
                                 */
#define XCloseHandle CloseHandle
#define XCreateFile CreateFileA
#define XSetFilePointer SetFilePointer
#define XGetFileSize GetFileSize
#define XReadFile ReadFile
#define XWriteFile WriteFile
#define XSetEndOfFile SetEndOfFile
#define IntlStringPrevChar ((intl_string_prev_char_fn)0x19d240)
#define XIsAlpha ((is_alpha_fn)0x1daaaa)
#define DEBUG_LOG error
#define XGetLastError xapi_GetLastError
#define XSetLastError SetLastError
#define XNtCreateFile CreateDirectoryA
#define XRemoveDirectory FUN_001d347c
#define XSetFileAttributes FUN_001d0df0
#define XDeleteFile DeleteFileA
#define XMoveFile MoveFileA
#define XGetFileAttributesEx \
  ((get_file_attributes_ex_fn)0x1d0ee1) /* hazard-ok: fnptr-conv */

#if defined(_MSC_VER) && !defined(__clang__)
extern void *__cdecl memset(void *, int, unsigned int);
#pragma intrinsic(memset)
#else
#define memset(p, c, n) csmemset((p), (c), (n))
#endif

static uint32_t g_find_files_flags;
static int16_t g_find_files_index = -1;
static int16_t g_find_files_location;
static char g_find_files_path[260];
static int g_find_file_handles[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static unsigned char g_find_file_data[0x148];

/**
 * file_reference_create - initialize a file reference in place.
 *
 * Asserts the destination is non-NULL and that location is in
 * [NONE, NUMBER_OF_FILE_REFERENCE_LOCATIONS) == [-1, 2), then zeroes
 * the whole 0x10C-byte structure, stores the location and finally the
 * FILE_REFERENCE_SIGNATURE magic. Store order (location before magic)
 * follows the binary (MOV [ESI+6],DI at 0x199486 then MOV [ESI],imm32
 * at 0x19948b).
 *
 * Returns the initialized reference (MOV EAX,ESI at 0x199491).
 */
file_ref_t *file_reference_create(file_ref_t *info, int16_t location)
{
  if (info == NULL) {
    display_assert("info", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x5b, true);
    system_exit(-1);
  }
  if (location < -1 || location >= 2) {
    display_assert("location>=NONE && "
                   "location<NUMBER_OF_FILE_REFERENCE_LOCATIONS",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x5c, true);
    system_exit(-1);
  }
  csmemset(info, 0, 0x10c);
  info->unk_6 = location;
  info->magic = FILE_REF_MAGIC;
  return info;
}

/**
 * find_files - enumerate files matching a directory reference.
 *
 * Begins a file search using the given flags and directory reference,
 * then iterates up to max_count entries, storing each result in the
 * results array (stride = sizeof(file_ref_t) = 0x10C).
 * Returns the number of files found.
 */
int16_t find_files(int flags, file_ref_t *dir, int16_t max_count,
                   file_ref_t *results)
{
  int16_t count = 0;

  if (max_count < 1) {
    display_assert("maximum_count>0", "c:\\halo\\SOURCE\\tag_files\\files.c",
                   0x101, true);
    system_exit(-1);
  }
  if (results == NULL) {
    display_assert("references", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x102,
                   true);
    system_exit(-1);
  }

  find_files_begin(flags, dir);

  if (max_count > 0) {
    do {
      if (!find_files_next(&results[count], 0))
        return count;
      count++;
    } while (count < max_count);
  }

  return count;
}

/**
 * file_read_into_buffer - read an entire file into a newly allocated buffer.
 *
 * Opens the file for reading, queries its size via file_get_eof, allocates
 * a buffer with debug_malloc, reads the full contents into it, then closes
 * the file. On success, writes the file size to *size_out and returns
 * the allocated buffer. On any failure (open, alloc, or read), returns NULL.
 * If the read fails after allocation, the buffer is freed before returning.
 */
void *file_read_into_buffer(file_ref_t *file_ref, int *size_out)
{
  void *buffer;

  buffer = NULL;
  if (file_open(file_ref, 1)) {
    *size_out = file_get_eof(file_ref);
    buffer =
      debug_malloc(*size_out, 0, "c:\\halo\\SOURCE\\tag_files\\files.c", 0x118);
    if (buffer != NULL) {
      if (!file_read(file_ref, *size_out, buffer)) {
        debug_free(buffer, "c:\\halo\\SOURCE\\tag_files\\files.c", 0x11e);
        buffer = NULL;
      }
    }
    file_close(file_ref);
  }
  return buffer;
}

/**
 * file_printf (0x1995c0) - format a string and append it to an open file.
 *
 * Formats into a 1024-byte stack buffer with vsprintf, writes csstrlen(buffer)
 * bytes to the file, then truncates the file at the new position (the
 * file_get_position result is the second argument to file_set_eof). Does
 * nothing when the format string is NULL ([EBP+0xc] TEST/JZ at 0x1995cc).
 *
 * [EBP+8] = info (kept in ESI), [EBP+0xc] = format, [EBP+0x10] = first vararg,
 * so the arglist is (char *)&format + 4 (LEA ECX,[EBP+0x10] at 0x1995d1).
 * Return values of file_write and file_set_eof are discarded.
 */
void file_printf(file_ref_t *info, const char *format, ...)
{
  char buffer[1024];
  char *arglist;

  if (format != NULL) {
    arglist = (char *)&format + 4;
    vsprintf(buffer, format, arglist);
    file_write(info, csstrlen(buffer), buffer);
    file_set_eof(info, file_get_position(info));
  }
}

/**
 * file_reference_verify - validate a file_ref_t pointer.
 *
 * Checks that the pointer is non-NULL, the magic signature matches
 * FILE_REFERENCE_SIGNATURE (0x66696C6F), flags are valid (only bit 0
 * allowed in the reference info flags), and the location field is in
 * the range [-1, 1] (NONE through NUMBER_OF_FILE_REFERENCE_LOCATIONS).
 *
 * Returns the validated pointer.
 */
file_ref_t *file_reference_verify(file_ref_t *info)
{
  if (info == NULL) {
    display_assert("info", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1fc, true);
    system_exit(-1);
  }
  if (info->magic != FILE_REF_MAGIC) {
    display_assert("info->signature==FILE_REFERENCE_SIGNATURE",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1fd, true);
    system_exit(-1);
  }
  if ((*(uint16_t *)&info->unk_4[0] & REFERENCE_INFO_FLAGS_INVALID_MASK) != 0) {
    display_assert("VALID_FLAGS(info->flags, NUMBER_OF_REFERENCE_INFO_FLAGS)",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1fe, true);
    system_exit(-1);
  }
  if (info->unk_6 < -1 || info->unk_6 >= 2) {
    display_assert("info->location>=NONE && "
                   "info->location<NUMBER_OF_FILE_REFERENCE_LOCATIONS",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1ff, true);
    system_exit(-1);
  }
  return info;
}

/**
 * file_reference_copy - copy a validated file reference.
 *
 * Verifies the source reference (return value discarded; the assert side
 * effects are the point), then copies 0x108 bytes from source to
 * destination. Note the copy size is 0x108, not sizeof(file_ref_t)
 * (0x10C) — the binary uses the literal, so the trailing 4 bytes of the
 * destination are left untouched.
 *
 * Returns the destination pointer (MOV EAX,ESI at 0x1996ef, where ESI
 * was reloaded from [EBP+8]).
 */
file_ref_t *file_reference_copy(file_ref_t *destination, file_ref_t *source)
{
  file_reference_verify(source);
  csmemcpy(destination, source, 0x108);
  return destination;
}

/**
 * file_reference_add_directory - append a directory component to a file
 * reference's path.
 *
 * Verifies the file reference, asserts that the directory string is
 * non-NULL and that the _has_filename_bit flag is not set (cannot add a
 * directory component after a filename has been set), then appends the
 * directory to the internal path buffer using path_add_directory.
 *
 * Returns the file reference pointer.
 */
file_ref_t *file_reference_add_directory(file_ref_t *info,
                                         const char *directory)
{
  file_ref_t *ref;

  ref = file_reference_verify(info);
  if (directory == NULL) {
    display_assert("directory", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x89,
                   true);
    system_exit(-1);
  }
  if (ref->unk_4[0] & (1 << _has_filename_bit)) {
    display_assert("!TEST_FLAG(info->flags, _has_filename_bit)",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x8a, true);
    system_exit(-1);
  }
  path_add_directory(ref->unk_8, directory);
  return info;
}

/**
 * file_reference_set_name - set the filename component of a file reference.
 *
 * Verifies the file reference and asserts that the name string is
 * non-NULL. If the _has_filename_bit flag is already set, strips the
 * existing filename from the path (via path_remove_filename) before
 * appending the new name with path_add_directory. Sets the
 * _has_filename_bit flag afterward.
 *
 * Returns the file reference pointer.
 */
file_ref_t *file_reference_set_name(file_ref_t *info, const char *name)
{
  file_ref_t *ref;

  ref = file_reference_verify(info);
  if (name == NULL) {
    display_assert("name", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x97, true);
    system_exit(-1);
  }
  if (ref->unk_4[0] & 1) {
    path_remove_filename(ref->unk_8);
  }
  path_add_directory(ref->unk_8, name);
  ref->unk_4[0] |= 1;
  return info;
}

/* 0x1997f0 — return the location field (unk_6) of a validated file reference.
 * The location is a small integer: -1=relative, 0=E:, 1=D:, etc. */
int16_t file_reference_get_location(file_ref_t *info)
{
  file_ref_t *ref;
  ref = file_reference_verify(info);
  return ref->unk_6;
}

/**
 * file_reference_get_name - extract a formatted name string from a file
 * reference.
 *
 * Validates the file reference, builds the full path, splits it into
 * components (directory, parent directory, filename, extension), and
 * reassembles the requested parts based on the flags bitmask:
 *   bit 0: directory
 *   bit 1: parent directory
 *   bit 2: filename (stem)
 *   bit 3: extension
 *
 * Returns name_out.
 */
char *file_reference_get_name(file_ref_t *info, int flags, char *name_out)
{
  file_ref_t *ref;
  char path[256];
  char *dir_part;
  char *parent_part;
  char *file_part;
  char *ext_part;
  int has_dir_flag;

  ref = file_reference_verify(info);

  csmemset(path, 0, sizeof(path));

  if (name_out == NULL) {
    display_assert("name", "c:\\halo\\SOURCE\\tag_files\\files.c", 0xb9, true);
    system_exit(-1);
  }
  if ((*(uint16_t *)&ref->unk_4[0] & NAME_FLAGS_INVALID_MASK) != 0) {
    display_assert("VALID_FLAGS(info->flags, NUMBER_OF_NAME_FLAGS)",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0xba, true);
    system_exit(-1);
  }
  if (flags == 0) {
    display_assert("flags", "c:\\halo\\SOURCE\\tag_files\\files.c", 0xbb, true);
    system_exit(-1);
  } else if (flags ==
             ((1 << _name_directory_bit) | (1 << _name_extension_bit))) {
    display_assert(
      "flags!=(FLAG(_name_directory_bit)|FLAG(_name_extension_bit))",
      "c:\\halo\\SOURCE\\tag_files\\files.c", 0xbc, true);
    system_exit(-1);
  }
  if ((flags & (1 << _name_directory_bit)) &&
      (flags & (1 << _name_parent_directory_bit))) {
    display_assert(
      "!TEST_FLAG(flags, _name_directory_bit) || !TEST_FLAG(flags, "
      "_name_parent_directory_bit)",
      "c:\\halo\\SOURCE\\tag_files\\files.c", 0xbd, true);
    system_exit(-1);
  }

  has_dir_flag = flags & (1 << _name_directory_bit);

  path_from_file_reference(ref->unk_6, ref->unk_8, path);
  path_split(path, &dir_part, &parent_part, &file_part, &ext_part,
             (uint8_t)ref->unk_4[0] & 1);

  *name_out = '\0';

  if (has_dir_flag) {
    path_add_directory(name_out, dir_part);
  }
  if (flags & (1 << _name_parent_directory_bit)) {
    path_add_directory(name_out, parent_part);
  }
  if (flags & (1 << _name_filename_bit)) {
    path_add_directory(name_out, file_part);
  }
  if (flags & (1 << _name_extension_bit)) {
    path_add_extension(name_out, ext_part);
  }

  return name_out;
}

/* 0x1999a0 — return true if two file references point to the same path.
 * Compares the location field (unk_6) and path string (unk_8) of both
 * validated references. */
bool file_references_equal(file_ref_t *info1, file_ref_t *info2)
{
  file_ref_t *ref1;
  file_ref_t *ref2;
  ref1 = file_reference_verify(info1);
  ref2 = file_reference_verify(info2);
  if (ref1->unk_6 == ref2->unk_6) {
    if (csstrcmp(ref1->unk_8, ref2->unk_8) == 0) {
      return true;
    }
  }
  return false;
}

/**
 * file_reference_create_from_path - initialize a file reference from a path.
 *
 * Zeroes the file_ref_t, sets the magic and location fields, then either
 * adds the directory as a path component (a3=true) or sets it as the
 * base name (a3=false).
 */
file_ref_t *file_reference_create_from_path(file_ref_t *info,
                                            const char *directory, bool a3)
{
  if (info == NULL) {
    display_assert("info", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x5B, true);
    system_exit(-1);
  }

  csmemset(info, 0, sizeof(*info));
  info->magic = FILE_REF_MAGIC;
  info->unk_6 = -1;

  if (a3) {
    file_reference_add_directory(info, directory);
  } else {
    file_reference_set_name(info, directory);
  }

  return info;
}

/**
 * directory_create_or_delete_contents - ensure a directory exists and is empty.
 *
 * Builds a file reference for the directory (inlined create-from-path with
 * a3=true). If the directory already exists, enumerates its contents and
 * deletes every entry; otherwise creates it.
 */
void directory_create_or_delete_contents(const char *directory_name)
{
  file_ref_t entry;
  file_ref_t info;

  csmemset(&info, 0, sizeof(info));
  info.magic = FILE_REF_MAGIC;
  info.unk_6 = -1;
  file_reference_add_directory(&info, directory_name);

  if (file_exists(&info)) {
    find_files_begin(0, &info);
    if (find_files_next(&entry, 0)) {
      do {
        file_delete(&entry);
      } while (find_files_next(&entry, 0));
    }
  } else {
    file_create(&info);
  }
}

/* Data-store record layout, from the assert strings at 0x199bd4/0x199c04:
 *   DATASTORE_MAX_DATA_SIZE and DATASTORE_MAX_FIELD_NAME_SIZE are both 255
 *   (the compares are against 0xff). The record stride 0x1fe and the total
 *   file size 0x18e70 are read from the binary; the field count 200 (0xc8) is
 *   the loop bound at 0x199ced and satisfies 200 * 0x1fe == 0x18e70.
 * Each record is [field_name: 255 bytes][data: 255 bytes]; the data starts at
 * +0xff, which is the offset used by the csmemcpy at 0x199d0d. */
#define DATASTORE_MAX_FIELD_NAME_SIZE 255
#define DATASTORE_MAX_DATA_SIZE 255
#define DATASTORE_FIELD_COUNT 200
#define DATASTORE_RECORD_SIZE \
  (DATASTORE_MAX_FIELD_NAME_SIZE + DATASTORE_MAX_DATA_SIZE)
#define DATASTORE_FILE_SIZE (DATASTORE_FIELD_COUNT * DATASTORE_RECORD_SIZE)

/**
 * datastore_read_field (0x199b20) - read one named field out of a data-store
 * file into the caller's buffer.
 *
 * Reads the whole file, rejects it unless it is exactly DATASTORE_FILE_SIZE
 * bytes, then scans up to DATASTORE_FIELD_COUNT fixed-size records for one
 * whose name matches field_name, stopping early at the first record with an
 * empty name. On a hit it copies `length` bytes of that record's data area to
 * `buffer` and reports true.
 *
 * Parameter names are from the assert strings (file_name, field_name,
 * length); the function name itself has no string evidence and is derived
 * from the DATASTORE_* assert macros.
 *
 * Divergence note: the original keeps its result flag in the (dead) high byte
 * of the field_name parameter slot at [EBP+0xf] and only ever stores to it on
 * the hit path at 0x199d15 — the not-found and bad-size exits return whatever
 * that byte held, i.e. the high byte of the field_name pointer, which is 0 for
 * every reachable string address in this build. We initialize the flag to
 * false, which reproduces the observed behavior without relying on that
 * aliasing. There are no callers in this build (no xrefs to 0x199b20).
 */
bool datastore_read_field(const char *file_name, const char *field_name,
                          int length, void *buffer)
{
  file_ref_t info;
  char *data;
  char *cursor;
  int index;
  int size;
  bool found;

  size = 0;
  found = false;

  if (file_name == NULL) {
    display_assert("NULL != file_name", "c:\\halo\\SOURCE\\tag_files\\files.c",
                   0x171, true);
    system_exit(-1);
  }
  if (field_name == NULL) {
    display_assert("NULL != field_name", "c:\\halo\\SOURCE\\tag_files\\files.c",
                   0x172, true);
    system_exit(-1);
  }
  if (file_name[0] == '\0') {
    display_assert("'\\0' != file_name[0]",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x173, true);
    system_exit(-1);
  }
  if (field_name[0] == '\0') {
    display_assert("'\\0' != field_name[0]",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x174, true);
    system_exit(-1);
  }
  if (length >= DATASTORE_MAX_DATA_SIZE) {
    display_assert("length < DATASTORE_MAX_DATA_SIZE",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x175, true);
    system_exit(-1);
  }
  if ((unsigned int)csstrlen(field_name) >= DATASTORE_MAX_FIELD_NAME_SIZE) {
    display_assert("strlen(field_name) < DATASTORE_MAX_FIELD_NAME_SIZE",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x176, true);
    system_exit(-1);
  }

  csmemset(&info, 0, sizeof(info));
  info.magic = FILE_REF_MAGIC;
  info.unk_6 = -1;
  file_reference_set_name(&info, file_name);

  if (file_exists(&info)) {
    data = (char *)file_read_into_buffer(&info, &size);
    if (data == NULL) {
      file_delete(&info);
    }
    if (size != DATASTORE_FILE_SIZE) {
      debug_free(data, "c:\\halo\\SOURCE\\tag_files\\files.c", 0x185);
      file_delete(&info);
      return found;
    }
    if (data != NULL) {
      for (index = 0, cursor = data; index < DATASTORE_FIELD_COUNT;
           index++, cursor += DATASTORE_RECORD_SIZE) {
        if (csstrcmp(cursor, field_name) == 0) {
          csmemcpy(buffer,
                   data + index * DATASTORE_RECORD_SIZE +
                     DATASTORE_MAX_FIELD_NAME_SIZE,
                   length);
          found = true;
          break;
        }
        if (*cursor == '\0') {
          break;
        }
      }
      debug_free(data, "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1a0);
    }
  }

  return found;
}

/**
 * datastore_read (0x199d40) - store one named field into a data-store file.
 *
 * Despite the kb.json name (CEA PDB line-containment, marked probable), the
 * binary body writes: it loads or creates the DATASTORE_FILE_SIZE record file,
 * finds the first record whose name matches field_name or whose name is empty,
 * copies field_name and `length` bytes of `buffer` into that record, then
 * re-creates/opens the file, writes the whole buffer back and closes it. The
 * name is therefore UNVERIFIED for this address; the behavior below is taken
 * from the disassembly at 0x199d40-0x19a00f only.
 *
 * Parameter names come from the assert strings (file_name, field_name,
 * length); `buffer` is the [EBP+0x14] slot passed as the csmemcpy source at
 * 0x199f68. The final `success` assert at 0x1ef halts when no free or matching
 * record was found. There are no callers in this build (no xrefs to 0x199d40).
 */
bool datastore_read(const char *file_name, const char *field_name, int length,
                    void *buffer)
{
  file_ref_t info;
  char *data;
  char *cursor;
  int index;
  int size;
  bool success;

  success = false;
  size = 0;

  if (file_name == NULL) {
    display_assert("NULL != file_name", "c:\\halo\\SOURCE\\tag_files\\files.c",
                   0x1AE, true);
    system_exit(-1);
  }
  if (field_name == NULL) {
    display_assert("NULL != field_name", "c:\\halo\\SOURCE\\tag_files\\files.c",
                   0x1AF, true);
    system_exit(-1);
  }
  if (file_name[0] == '\0') {
    display_assert("'\\0' != file_name[0]",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1B0, true);
    system_exit(-1);
  }
  if (field_name[0] == '\0') {
    display_assert("'\\0' != field_name[0]",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1B1, true);
    system_exit(-1);
  }
  if (length >= DATASTORE_MAX_DATA_SIZE) {
    display_assert("length < DATASTORE_MAX_DATA_SIZE",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1B2, true);
    system_exit(-1);
  }
  if ((unsigned int)csstrlen(field_name) >= DATASTORE_MAX_FIELD_NAME_SIZE) {
    display_assert("strlen(field_name) < DATASTORE_MAX_FIELD_NAME_SIZE",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1B3, true);
    system_exit(-1);
  }

  csmemset(&info, 0, sizeof(info));
  info.magic = FILE_REF_MAGIC;
  info.unk_6 = -1;
  file_reference_set_name(&info, file_name);

  data = NULL;
  if (file_exists(&info)) {
    data = (char *)file_read_into_buffer(&info, &size);
    if (data == NULL) {
      file_delete(&info);
    }
    if (size != DATASTORE_FILE_SIZE) {
      debug_free(data, "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1C2);
      file_delete(&info);
      data = NULL;
    }
  }
  if (data == NULL) {
    data = (char *)debug_malloc(DATASTORE_FILE_SIZE, false,
                                "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1CD);
    if (data == NULL) {
      goto failure; /* 0x199f00: shares the final "success" assert arm */
    }
    csmemset(data, 0, DATASTORE_FILE_SIZE);
  }

  index = 0;
  cursor = data;
  do {
    if (*cursor == '\0' || csstrcmp(cursor, field_name) == 0) {
      csstrcpy(data + index * DATASTORE_RECORD_SIZE, field_name);
      csmemcpy(data + index * DATASTORE_RECORD_SIZE +
                 DATASTORE_MAX_FIELD_NAME_SIZE,
               buffer, length);
      success = true;
      break;
    }
    index++;
    cursor += DATASTORE_RECORD_SIZE;
  } while (index < DATASTORE_FIELD_COUNT);

  if (!file_exists(&info)) {
    file_create(&info);
  }
  if (file_open(&info, 2)) {
    file_write(&info, DATASTORE_FILE_SIZE, data);
    file_close(&info);
  }
  debug_free(data, "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1EC);

  if (success) {
    return success;
  }

failure:
  display_assert("success", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1EF,
                 true);
  system_exit(-1);
  return false;
}

/* 0x19a020 — compare the 8-byte last-modification timestamps of two files.
 *
 * Confirmed from disassembly: MOV EAX,[EBP+0xc] (date2); MOV ECX,[EBP+8]
 * (date1); PUSH 0x8; PUSH EAX; PUSH ECX; CALL 0x8da40 (csmemcmp, cdecl
 * `int csmemcmp(const void *a, const void *b, int size)`); ADD ESP,0xc;
 * POP EBP; RET. No EAX fixup after the call, so csmemcmp's result is this
 * function's return value.
 *
 * Unknown: the exact timestamp type behind the 8 bytes (FILETIME-shaped);
 * no callers are present in this build, so the parameters stay void *. */
int file_compare_last_modification_dates(const void *date1, const void *date2)
{
  return csmemcmp(date1, date2, 8);
}

void find_files_begin(int flags, file_ref_t *dir)
{
  file_ref_t *ref;
  int16_t count;
  int *handle_ptr;

  ref = file_reference_verify(dir);

  if ((flags & FIND_FILES_FLAGS_INVALID_MASK) != 0) {
    display_assert("VALID_FLAGS(flags, NUMBER_OF_FIND_FILES_FLAGS)",
                   "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x224, true);
    system_exit(-1);
  }
  if ((ref->unk_4[0] & (1 << _has_filename_bit)) != 0) {
    display_assert("!TEST_FLAG(info->flags, has_filename_bit)",
                   "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x225, true);
    system_exit(-1);
  }

  if (g_find_files_index >= 0) {
    handle_ptr = &g_find_file_handles[g_find_files_index];
    for (count = g_find_files_index + 1; count > 0; count--) {
      if (*handle_ptr != -1) {
        XCloseHandle(*handle_ptr);
        *handle_ptr = -1;
      }
      handle_ptr--;
    }
  }

  g_find_files_flags = (uint32_t)flags;
  g_find_files_index = 0;
  g_find_files_location = ref->unk_6;
  csstrcpy(g_find_files_path, ref->unk_8);
}

void path_add_directory(char *path, const char *directory)
{
  int path_length;
  char *tail;

  if (*directory == '\0') {
    return;
  }

  if ((unsigned int)(csstrlen(path) + 1 + csstrlen(directory)) > 0xFF) {
    display_assert("strlen(path)+1+strlen(name)<=MAXIMUM_FILENAME_LENGTH",
                   "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x2A0, true);
    system_exit(-1);
  }

  path_length = csstrlen(path);
  tail = path + path_length;
  if (tail != path) {
    *tail = '\\';
    tail++;
    *tail = '\0';
  }

  path_length = csstrlen(path);
  csstrncpy(tail, directory, 0xFF - path_length);
  path[0xFF] = '\0';
}

void path_add_extension(char *path, const char *extension)
{
  int path_length;
  char *tail;

  if (*extension == '\0') {
    return;
  }

  if ((unsigned int)(csstrlen(path) + 1 + csstrlen(extension)) > 0xFF) {
    display_assert("strlen(path)+1+strlen(extension)<=MAXIMUM_FILENAME_LENGTH",
                   "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x2b8, true);
    system_exit(-1);
  }

  path_length = csstrlen(path);
  tail = path + path_length;
  if (tail != path) {
    *tail = '.';
    tail++;
    *tail = '\0';
  }

  path_length = csstrlen(path);
  csstrncpy(tail, extension, 0xFF - path_length);
  path[0xFF] = '\0';
}

void path_remove_filename(char *path)
{
  char *base;

  base = path;
  path = (char *)csstrlen(path);
  do {
    if ((int16_t)(uintptr_t)path == 0) {
      break;
    }
  } while (unicode_cursor_backward(base, (int16_t *)&path) != '\\');

  if (unicode_cursor_forward(base, (int16_t *)&path) == '\\') {
    base[(int16_t)((uintptr_t)path - 1)] = '\0';
    return;
  }
  base[(int16_t)(uintptr_t)path] = '\0';
}

void path_split(const char *path, char **directory, char **parent_directory,
                char **filename, char **extension, int flags)
{
  char *mutable_path = (char *)path;
  int16_t path_length = (int16_t)csstrlen(path);
  char *end = mutable_path + path_length;
  uint16_t ch;

  *directory = end;
  *parent_directory = end;
  *filename = end;
  *extension = end;

  while (path_length != 0) {
    ch = IntlStringPrevChar(mutable_path, &path_length);

    if (ch == '.') {
      if (flags != 0 && **filename == '\0' && **extension == '\0') {
        mutable_path[path_length] = '\0';
        *extension = mutable_path + path_length + 1;
      }
    } else if (ch == '\\') {
      if (flags == 0 || **filename != '\0') {
        if (**parent_directory == '\0') {
          *parent_directory = mutable_path + path_length + 1;
        }
      } else {
        mutable_path[path_length] = '\0';
        *filename = mutable_path + path_length + 1;
      }
    }
  }

  if (flags != 0 && **filename == '\0') {
    *filename = mutable_path;
    return;
  }

  if (*filename != mutable_path) {
    *directory = mutable_path;
  }
}

void path_from_file_reference(int16_t location, const char *path, char *out)
{
  (void)location;

  if (path == NULL || out == NULL) {
    display_assert("path && full_path",
                   "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x314, true);
    system_exit(-1);
  }

  *out = '\0';

  if (!(path[0] != '\0' && path[1] != '\0' && path[2] != '\0' &&
        XIsAlpha((unsigned char)path[0]) != 0 && path[1] == ':' &&
        path[2] == '\\')) {
    csstrcpy(out, "d:\\");
  }

  csstrcpy(out + csstrlen(out), path);
}

/**
 * file_read_only - report whether the referenced file has the read-only
 * attribute set.
 *
 * 0x19a400: builds the full path from the verified file reference (no memset
 * of the 256-byte buffer here, unlike file_exists), then calls
 * file_get_full_attributes. CMP EAX,-1 / JZ returns false on failure;
 * TEST AL,0x1 returns true only when FILE_ATTRIBUTE_READONLY is set.
 */
bool file_read_only(file_ref_t *info)
{
  file_ref_t *ref;
  char path[256];
  int attributes;
  bool result;

  ref = file_reference_verify(info);

  /* Score lever (79.3% -> 100.0%): the zero-init + goto-shared-tail spelling is
   * what makes cl.exe emit the reference's XOR BL,BL early / MOV AL,1 / JNE /
   * MOV AL,BL / POP EBX tail. A short-circuit `return a != -1 && (a & 1);`
   * instead emits two JE sites with separate MOV EAX,1 and XOR EAX,EAX
   * epilogues. The placement of `result = 0` *after* file_reference_verify is
   * also load-bearing: it is what schedules the XOR BL,BL into the reference's
   * slot. Do not "simplify". */
  result = 0;

  path_from_file_reference(ref->unk_6, ref->unk_8, path);

  attributes = file_get_full_attributes(path);

  if (attributes == -1) {
    goto done;
  }

  if ((attributes & 1) != 0) {
    result = 1;
  }

done:
  return result;
}

/* 0x19a450: PUSH EAX (info) at entry — EAX is passed directly to
 * file_reference_verify (0x199620); [EBP+8] = function_name (only stack arg).
 * info is a register arg @<eax>; kb.json decl updated accordingly. */
void file_error(file_ref_t *info, const char *function_name)
{
  file_ref_t *ref;
  uint32_t err_code;

  ref = file_reference_verify(info);
  err_code = XGetLastError();
  DEBUG_LOG(2, "%s('%s') error 0x%08x", function_name, ref->unk_8, err_code);
  XSetLastError(0);
}

/**
 * file_create - create a file referenced by info.
 *
 * Builds the full path from the file reference.
 * If the write-mode bit (bit 0 of unk_4[0]) is clear, uses the NT
 * NtCreateFile wrapper (CreateDirectoryA) with default access flags.
 * If the write-mode bit is set, uses CreateFileA (XCreateFile) with
 * GENERIC_WRITE | FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_SEQUENTIAL_SCAN.
 * On success, closes the returned handle. On failure, logs the error and
 * clears it. Returns true on success, false on failure.
 */
bool file_create(file_ref_t *info)
{
  file_ref_t *ref;
  char path[256];
  int handle;

  ref = file_reference_verify(info);

  memset(path, 0, sizeof(path));

  path_from_file_reference(ref->unk_6, ref->unk_8, path);

  if (ref->unk_4[0] & 1) {
    handle = XCreateFile(path, 0x40000000, 0, 0, 2, 0x80, 0);
    if (handle == -1) {
      goto error;
    }
    XCloseHandle(handle);
  } else {
    handle = XNtCreateFile(ref->unk_8, 0);
    if (handle == 0) {
      goto error;
    }
  }
  return true;

error:
  ref = file_reference_verify(info);
  DEBUG_LOG(2, "%s('%s') error 0x%08x", "file_create", ref->unk_8,
            XGetLastError());
  XSetLastError(0);
  return false;
}

/* 0x19a560 — delete the file (or directory) referenced by info.
 * For directories (unk_4[0] bit 0 clear), uses XRemoveDirectory.
 * For regular files (unk_4[0] bit 0 set), clears FILE_ATTRIBUTE_NORMAL
 * then calls XDeleteFile. Returns true on success, logs error and
 * returns false on failure. */
bool file_delete(file_ref_t *info)
{
  file_ref_t *ref;
  char path[256];

  ref = file_reference_verify(info);
  memset(path, 0, sizeof(path));
  path_from_file_reference(ref->unk_6, ref->unk_8, path);

  if (ref->unk_4[0] & 1) {
    if (XSetFileAttributes(path, 0x80)) {
      if (XDeleteFile(path)) {
        return true;
      }
    }
  } else {
    if (XRemoveDirectory(path)) {
      return true;
    }
  }

  ref = file_reference_verify(info);
  DEBUG_LOG(2, "%s('%s') error 0x%08x", "file_delete", ref->unk_8,
            XGetLastError());
  XSetLastError(0);
  return false;
}

/**
 * file_exists - check whether a file referenced by info exists on disk.
 *
 * Builds the full path from the file reference, then calls
 * file_get_full_attributes (NtQueryFullAttributesFile wrapper).
 * Returns true if the file was found, false otherwise. Logs an error
 * via file_error if the failure was not ERROR_FILE_NOT_FOUND (2) or
 * ERROR_PATH_NOT_FOUND (3).
 */
bool file_exists(file_ref_t *info)
{
  file_ref_t *ref;
  char path[256];
  int result;

  ref = file_reference_verify(info);

  memset(path, 0, sizeof(path));

  path_from_file_reference(ref->unk_6, ref->unk_8, path);

  result = file_get_full_attributes(path);
  if (result != -1) {
    return true;
  }

  if (xapi_GetLastError() != 2) {
    if (xapi_GetLastError() != 3) {
      file_error(info, "file_exists");
    }
  }

  return false;
}

/* 0x19a6d0 — rename (or move) the file referenced by info to new_name.
 * Builds the full source path from info; builds the destination path by
 * copying the source path, stripping the filename, and appending new_name.
 * Calls XMoveFile to perform the rename. On success also updates the
 * file_ref's internal path. Returns true on success, false on failure. */
bool file_rename(file_ref_t *info, const char *new_name)
{
  file_ref_t *ref;
  char src_path[256];
  char dst_path[256];

  ref = file_reference_verify(info);
  src_path[0] = 0;
  memset(src_path + 1, 0, sizeof(src_path) - 1);
  dst_path[0] = 0;
  memset(dst_path + 1, 0, sizeof(dst_path) - 1);
  path_from_file_reference(ref->unk_6, ref->unk_8, src_path);
  csstrcpy(dst_path, src_path);
  path_remove_filename(dst_path);
  path_add_directory(dst_path, new_name);

  if (XMoveFile(src_path, dst_path)) {
    path_remove_filename(ref->unk_8);
    path_add_directory(ref->unk_8, new_name);
    return true;
  }
  return false;
}

bool file_open(file_ref_t *info, int flags)
{
  file_ref_t *ref;
  char path[256];
  uint32_t access;
  int handle;

  ref = file_reference_verify(info);

  memset(path, 0, sizeof(path));

  if ((flags & PERMISSION_FLAGS_INVALID_MASK) != 0) {
    display_assert("VALID_FLAGS(flags, NUMBER_OF_PERMISSION_FLAGS)",
                   "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x134, true);
    system_exit(-1);
  }
  if ((flags & ((1 << _permission_read_bit) | (1 << _permission_write_bit))) ==
      0) {
    display_assert(
      "flags & (FLAG(_permission_read_bit)|FLAG(_permission_write_bit))",
      "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x135, true);
    system_exit(-1);
  }
  if (((flags & (1 << _permission_write_bit)) == 0) &&
      ((flags & (1 << _permission_append_bit)) != 0)) {
    display_assert(
      "TEST_FLAG(flags, _permission_write_bit) || !TEST_FLAG(flags, "
      "_permission_append_bit)",
      "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x136, true);
    system_exit(-1);
  }

  path_from_file_reference(ref->unk_6, ref->unk_8, path);

  access = 0;
  if ((flags & (1 << _permission_read_bit)) != 0) {
    access = 0x80000000;
  }
  if ((flags & (1 << _permission_write_bit)) != 0) {
    access |= 0x40000000;
  }

  handle = XCreateFile(path, access, 0, 0, 3, 0x80, 0);
  if (handle != -1) {
    *(int *)&ref->unk_8[256] = handle;
    if ((flags & (1 << _permission_append_bit)) == 0) {
      return true;
    }

    if (XSetFilePointer(handle, 0, NULL, 2) != -1) {
      return true;
    }

    XCloseHandle(*(int *)&ref->unk_8[256]);
    *(int *)&ref->unk_8[256] = 0;
  }

  file_error(info, "file_open");
  return false;
}

bool file_close(file_ref_t *info)
{
  file_ref_t *ref;

  ref = file_reference_verify(info);
  if (XCloseHandle(*(int *)&ref->unk_8[256])) {
    *(int *)&ref->unk_8[256] = 0;
    return true;
  }

  file_error(info, "file_close");
  return false;
}

/* 0x19a9a0 — return the current byte offset within the open file.
 * Calls SetFilePointer with move=0 from FILE_CURRENT (1) to query the
 * position without moving. On failure, logs an error and returns -1. */
int file_get_position(file_ref_t *info)
{
  file_ref_t *ref;
  int pos;
  unsigned int err;

  ref = file_reference_verify(info);
  pos = XSetFilePointer(*(int *)&ref->unk_8[256], 0, NULL, 1);
  if (pos == -1) {
    ref = file_reference_verify(info);
    err = XGetLastError();
    error(2, "%s('%s') error 0x%08x", "file_get_position", ref->unk_8, err);
    XSetLastError(0);
  }
  return pos;
}

/* 0x19aa00 — seek to an absolute byte offset within the open file.
 * Calls SetFilePointer with method=FILE_BEGIN (0). Returns true on
 * success, false on failure; logs an error on failure. */
bool file_set_position(file_ref_t *info, int offset)
{
  file_ref_t *ref;
  int result;
  unsigned int err;

  ref = file_reference_verify(info);
  result = XSetFilePointer(*(int *)&ref->unk_8[256], offset, NULL, 0);
  if (result == -1) {
    ref = file_reference_verify(info);
    err = XGetLastError();
    error(2, "%s('%s') error 0x%08x", "file_set_position", ref->unk_8, err);
    XSetLastError(0);
  }
  return result != -1;
}

int file_get_eof(file_ref_t *info)
{
  file_ref_t *ref;
  int eof;

  ref = file_reference_verify(info);
  eof = XGetFileSize(*(int *)&ref->unk_8[256], NULL);
  if (eof == -1) {
    file_error(info, "file_get_eof");
  }

  return eof;
}

/* 0x19aad0 — truncate/extend the open file to 'offset'. Seeks there with
 * file_set_position, then calls SetEndOfFile on the handle at unk_8[256]
 * (+0x108). Returns true on success; on either failure re-verifies the
 * reference and logs the error inline (MSVC inlined file_error here), then
 * clears the last-error code and returns false. */
bool file_set_eof(file_ref_t *info, int offset)
{
  file_ref_t *ref;
  unsigned int err;

  ref = file_reference_verify(info);
  if (file_set_position(info, offset)) {
    if (XSetEndOfFile(*(int *)&ref->unk_8[256])) {
      return true;
    }
  }

  ref = file_reference_verify(info);
  err = XGetLastError();
  error(2, "%s('%s') error 0x%08x", "file_set_eof", ref->unk_8, err);
  XSetLastError(0);
  return false;
}

bool file_read(file_ref_t *info, int size, void *buffer)
{
  file_ref_t *ref;
  uint32_t bytes_read;

  ref = file_reference_verify(info);
  if (buffer == NULL) {
    display_assert("buffer", "c:\\halo\\SOURCE\\tag_files\\files_windows.c",
                   0x1a7, true);
    system_exit(-1);
  }

  if (XReadFile(*(int *)&ref->unk_8[256], buffer, (uint32_t)size, &bytes_read,
                NULL)) {
    if (bytes_read == size) {
      return true;
    }
    XSetLastError(0x26);
  }

  file_error(info, "file_read");
  return false;
}

/* 0x19ac00 — write 'size' bytes from 'buffer' to the open file handle at
 * ref->unk_8[256] (+0x108). Asserts buffer is non-NULL, then WriteFile;
 * success requires both a non-zero return and a full byte count. On failure
 * re-verifies the reference and logs the error inline (MSVC inlined
 * file_error here), clears the last-error code and returns false. */
bool file_write(file_ref_t *info, int size, const void *buffer)
{
  file_ref_t *ref;
  unsigned int err;
  uint32_t bytes_written;

  ref = file_reference_verify(info);
  if (buffer == NULL) {
    display_assert("buffer", "c:\\halo\\SOURCE\\tag_files\\files_windows.c",
                   0x1c3, true);
    system_exit(-1);
  }

  if (XWriteFile(*(int *)&ref->unk_8[256], (void *)buffer, (uint32_t)size,
                 &bytes_written, NULL)) {
    if (bytes_written == (uint32_t)size) {
      return true;
    }
  }

  ref = file_reference_verify(info);
  err = XGetLastError();
  error(2, "%s('%s') error 0x%08x", "file_write", ref->unk_8, err);
  XSetLastError(0);
  return false;
}

/* 0x19acb0 — seek to 'offset' then read 'size' bytes into 'buffer'.
 * Combines file_set_position and file_read; returns true only if both
 * succeed, false otherwise. */
bool file_read_from_position(file_ref_t *info, int offset, int size,
                             void *buffer)
{
  char ok_pos;
  char ok_read;

  ok_pos = file_set_position(info, offset);
  if (ok_pos != '\0') {
    ok_read = file_read(info, size, buffer);
    if (ok_read != '\0') {
      return 1;
    }
  }
  return 0;
}

/* 0x19acf0 — seek to 'offset' then write 'size' bytes from 'buffer'.
 * Combines file_set_position and file_write; returns true only if both
 * succeed, false otherwise. */
bool file_write_to_position(file_ref_t *info, int offset, int size,
                            const void *buffer)
{
  char ok_pos;
  char ok_write;

  ok_pos = file_set_position(info, offset);
  if (ok_pos != '\0') {
    ok_write = file_write(info, size, buffer);
    if (ok_write != '\0') {
      return 1;
    }
  }
  return 0;
}

/**
 * file_get_last_modification_date - 0x19ad30.
 *
 * Zero-fills the 8-byte out parameter, builds the full path from the verified
 * file reference and queries GetFileAttributesExA (info level 0 =
 * GetFileExInfoStandard). On success copies the 8-byte last-write time out of
 * the WIN32_FILE_ATTRIBUTE_DATA (+0x14). On failure logs the error and clears
 * it, leaving 'date' zeroed.
 *
 * Note: both epilogues are MOV AL,0x1 — the function returns true even on the
 * failure path. Preserved as-is from the binary.
 */
bool file_get_last_modification_date(file_ref_t *info, void *date)
{
  file_ref_t *ref;
  char path[256];
  file_attribute_data_t attribute_data;

  ref = file_reference_verify(info);

  /* Reference zeroes path[0] with a byte store then REP STOSD over the
   * remaining 255 bytes — the MSVC 7.1 `char path[256] = ""` shape. Spelled
   * explicitly because clang lowers the initializer to a _memset libcall that
   * does not exist in this freestanding build; the guarded macro above routes
   * that lane to csmemset. */
  path[0] = '\0';
  memset(path + 1, 0, sizeof(path) - 1);

  csmemset(date, 0, 8);

  path_from_file_reference(ref->unk_6, ref->unk_8, path);

  if (XGetFileAttributesEx(path, 0, &attribute_data) != 0) {
    csmemcpy(date, attribute_data.last_write_time, 8);
    return 1;
  }

  ref = file_reference_verify(info);
  DEBUG_LOG(2, "%s('%s') error 0x%08x", "file_get_last_modification_date",
            ref->unk_8, XGetLastError());
  XSetLastError(0);
  return 1;
}

/**
 * file_get_size - 0x19adf0.
 *
 * Verifies the file reference, zero-fills the 256-byte path buffer, asserts
 * the out parameter is non-NULL, builds the full path and queries
 * GetFileAttributesExA (info level 0 = GetFileExInfoStandard). On success
 * stores nFileSizeLow (+0x20 of the WIN32_FILE_ATTRIBUTE_DATA, read at
 * EBP-0x4 against the EBP-0x24 frame slot) through 'size' and returns true
 * (MOV AL,0x1). On failure re-verifies the reference, logs the error, clears
 * the last-error code and returns false (XOR AL,AL).
 */
bool file_get_size(file_ref_t *info, uint32_t *size)
{
  file_ref_t *ref;
  char path[256];
  file_attribute_data_t attribute_data;

  ref = file_reference_verify(info);

  /* Byte store of path[0] then REP STOSD/STOSW/STOSB over the remaining 255
   * bytes — the MSVC 7.1 `char path[256] = ""` shape (0x19ae09..0x19ae1f). */
  path[0] = '\0';
  memset(path + 1, 0, sizeof(path) - 1);

  if (size == NULL) {
    display_assert("size", "c:\\halo\\SOURCE\\tag_files\\files_windows.c",
                   0x20c, true);
    system_exit(-1);
  }

  path_from_file_reference(ref->unk_6, ref->unk_8, path);

  if (XGetFileAttributesEx(path, 0, &attribute_data) != 0) {
    *size = attribute_data.file_size_low;
    return 1;
  }

  ref = file_reference_verify(info);
  DEBUG_LOG(2, "%s('%s') error 0x%08x", "file_get_size", ref->unk_8,
            XGetLastError());
  XSetLastError(0);
  return 0;
}

bool find_files_next(file_ref_t *result, int param2)
{
  char full_path[256];

  csmemset(full_path, 0, sizeof(full_path));

  while (g_find_files_index >= 0) {
    int level = (uint16_t)g_find_files_index;
    int handle = g_find_file_handles[level];

    if (handle == -1) {
      path_from_file_reference(g_find_files_location, g_find_files_path,
                               full_path);
      path_add_directory(full_path, "*.*");

      handle = XFindFirstFile(full_path, g_find_file_data);
      g_find_file_handles[level] = handle;
      if (handle == -1) {
        path_remove_filename(g_find_files_path);
        g_find_files_index--;
        continue;
      }
    } else {
      if (!XFindNextFile(handle, g_find_file_data)) {
        XCloseHandle(g_find_file_handles[level]);
        g_find_file_handles[level] = -1;
        path_remove_filename(g_find_files_path);
        g_find_files_index--;
        continue;
      }
    }

    if ((*(uint32_t *)&g_find_file_data[0] & 0x10) != 0) {
      char *entry_name = (char *)&g_find_file_data[0x2C];

      if (csstrcmp(entry_name, ".") != 0 && csstrcmp(entry_name, "..") != 0) {
        if ((g_find_files_flags & FIND_FILES_DIRECTORIES_BIT) != 0) {
          csmemset(result, 0, sizeof(*result));
          result->unk_6 = g_find_files_location;
          result->magic = FILE_REF_MAGIC;
          file_reference_add_directory(result, g_find_files_path);
          file_reference_add_directory(result, entry_name);
        }

        if ((g_find_files_flags & FIND_FILES_RECURSIVE_BIT) != 0) {
          if ((g_find_files_flags & FIND_FILES_DIRECTORIES_BIT) == 0) {
            path_add_directory(g_find_files_path, entry_name);
          }
          g_find_files_index++;
        }

        if ((g_find_files_flags & FIND_FILES_DIRECTORIES_BIT) != 0) {
          if (param2 != 0) {
            csmemcpy((void *)param2, &g_find_file_data[0x14], 8);
          }
          return true;
        }
      }
    } else if ((g_find_files_flags & FIND_FILES_DIRECTORIES_BIT) == 0) {
      csmemset(result, 0, sizeof(*result));
      result->unk_6 = g_find_files_location;
      result->magic = FILE_REF_MAGIC;
      file_reference_add_directory(result, g_find_files_path);
      file_reference_set_name(result, (char *)&g_find_file_data[0x2C]);

      if (param2 != 0) {
        csmemcpy((void *)param2, &g_find_file_data[0x14], 8);
      }
      return true;
    }
  }

  return false;
}

/* 0x19a010 — file_location_is_valid */
boolean file_location_is_valid(void)
{
  return 1;
}
