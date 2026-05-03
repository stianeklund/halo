#define FILE_REF_MAGIC 0x66696C6F

#define FIND_FILES_RECURSIVE_BIT 1
#define FIND_FILES_DIRECTORIES_BIT 2

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

#define XFindFirstFile ((find_first_file_fn)0x1d3576)
#define XFindNextFile ((find_next_file_fn)0x1d3683)
#define XCloseHandle ((close_handle_fn)0x1cf900)
#define XCreateFile ((create_file_fn)0x1d1d85)
#define XSetFilePointer ((set_file_pointer_fn)0x1d1610)
#define XGetFileSize ((get_file_size_fn)0x1d1d4a)
#define XReadFile ((read_file_fn)0x1d13c9)
#define IntlStringPrevChar ((intl_string_prev_char_fn)0x19d240)
#define XIsAlpha ((is_alpha_fn)0x1daaaa)
#define DEBUG_LOG ((debug_log_fn)0x8f390)
#define XGetLastError ((xget_last_error_fn)0x1d2240)
#define XSetLastError ((xset_last_error_fn)0x1d2268)

static uint32_t g_find_files_flags;
static int16_t g_find_files_index = -1;
static int16_t g_find_files_location;
static char g_find_files_path[260];
static int g_find_file_handles[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static unsigned char g_find_file_data[0x148];

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
  if ((*(uint16_t *)&info->unk_4[0] & 0xfffe) != 0) {
    display_assert("VALID_FLAGS(info->flags, NUMBER_OF_REFERENCE_INFO_FLAGS)",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1fe, true);
    system_exit(-1);
  }
  if (info->unk_6 < -1 || info->unk_6 > 1) {
    display_assert("info->location>=NONE && "
                   "info->location<NUMBER_OF_FILE_REFERENCE_LOCATIONS",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1ff, true);
    system_exit(-1);
  }
  return info;
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
  if (ref->unk_4[0] & 1) {
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
  if ((*(uint16_t *)&ref->unk_4[0] & 0xfff0) != 0) {
    display_assert("VALID_FLAGS(info->flags, NUMBER_OF_NAME_FLAGS)",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0xba, true);
    system_exit(-1);
  }
  if (flags == 0) {
    display_assert("flags", "c:\\halo\\SOURCE\\tag_files\\files.c", 0xbb, true);
    system_exit(-1);
  } else if (flags == 9) {
    display_assert(
      "flags!=(FLAG(_name_directory_bit)|FLAG(_name_extension_bit))",
      "c:\\halo\\SOURCE\\tag_files\\files.c", 0xbc, true);
    system_exit(-1);
  }
  if ((flags & 1) && (flags & 2)) {
    display_assert(
      "!TEST_FLAG(flags, _name_directory_bit) || !TEST_FLAG(flags, "
      "_name_parent_directory_bit)",
      "c:\\halo\\SOURCE\\tag_files\\files.c", 0xbd, true);
    system_exit(-1);
  }

  has_dir_flag = flags & 1;

  path_from_file_reference(ref->unk_6, ref->unk_8, path);
  path_split(path, &dir_part, &parent_part, &file_part, &ext_part,
             (uint8_t)ref->unk_4[0] & 1);

  *name_out = '\0';

  if (has_dir_flag) {
    path_add_directory(name_out, dir_part);
  }
  if (flags & 2) {
    path_add_directory(name_out, parent_part);
  }
  if (flags & 4) {
    path_add_directory(name_out, file_part);
  }
  if (flags & 8) {
    path_add_extension(name_out, ext_part);
  }

  return name_out;
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

void find_files_begin(int flags, file_ref_t *dir)
{
  file_ref_t *ref;

  ref = file_reference_verify(dir);

  if ((flags & ~3) != 0) {
    display_assert("VALID_FLAGS(flags, NUMBER_OF_FIND_FILES_FLAGS)",
                   "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x224, true);
    system_exit(-1);
  }
  if ((ref->unk_4[0] & 1) != 0) {
    display_assert("!TEST_FLAG(info->flags, has_filename_bit)",
                   "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x225, true);
    system_exit(-1);
  }

  while (g_find_files_index >= 0) {
    int handle = g_find_file_handles[(uint16_t)g_find_files_index];
    if (handle != -1) {
      XCloseHandle(handle);
      g_find_file_handles[(uint16_t)g_find_files_index] = -1;
    }
    g_find_files_index--;
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

  if (csstrlen(path) + 1 + csstrlen(directory) > 0xFF) {
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

  if (csstrlen(path) + 1 + csstrlen(extension) > 0xFF) {
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
  int i;
  int length;

  length = csstrlen(path);
  for (i = length - 1; i >= 0; i--) {
    if (path[i] == '\\') {
      path[i] = '\0';
      return;
    }
  }

  *path = '\0';
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

void file_error(file_ref_t *info, const char *function_name)
{
  file_ref_t *ref;
  uint32_t error;

  ref = file_reference_verify(info);
  error = XGetLastError();
  DEBUG_LOG(2, "%s('%s') error 0x%08x", function_name, ref->unk_8, error);
  XSetLastError(0);
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

  csmemset(path, 0, sizeof(path));

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

bool file_open(file_ref_t *info, int flags)
{
  file_ref_t *ref;
  char path[256];
  uint32_t access;
  int handle;

  ref = file_reference_verify(info);

  csmemset(path, 0, sizeof(path));

  if ((flags & ~7) != 0) {
    display_assert("VALID_FLAGS(flags, NUMBER_OF_PERMISSION_FLAGS)",
                   "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x134, true);
    system_exit(-1);
  }
  if ((flags & 3) == 0) {
    display_assert(
      "flags & (FLAG(_permission_read_bit)|FLAG(_permission_write_bit))",
      "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x135, true);
    system_exit(-1);
  }
  if (((flags & 2) == 0) && ((flags & 4) != 0)) {
    display_assert(
      "TEST_FLAG(flags, _permission_write_bit) || !TEST_FLAG(flags, "
      "_permission_append_bit)",
      "c:\\halo\\SOURCE\\tag_files\\files_windows.c", 0x136, true);
    system_exit(-1);
  }

  path_from_file_reference(ref->unk_6, ref->unk_8, path);

  access = 0;
  if ((flags & 1) != 0) {
    access = 0x80000000;
  }
  if ((flags & 2) != 0) {
    access |= 0x40000000;
  }

  handle = XCreateFile(path, access, 0, NULL, 3, 0x80, 0);
  if (handle != -1) {
    *(int *)&ref->unk_8[256] = handle;
    if ((flags & 4) == 0) {
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

bool file_read(file_ref_t *info, int size, void *buffer)
{
  file_ref_t *ref;
  int bytes_read;

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
