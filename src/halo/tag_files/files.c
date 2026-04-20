#define FILE_REF_MAGIC 0x66696C6F

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
      /* file_error takes file_ref in EAX (register arg) and function name
         on the stack. Since it's not ported and uses @eax, we must use
         inline asm to call it. */
      {
        int _eax = (int)info;
        __asm__ __volatile__(
          "pushl %[str]\n\t"
          "call *%[fn]\n\t"
          "addl $4, %%esp"
          : "+a"(_eax)
          : [str] "r"("file_exists"), [fn] "r"((void *)0x19a450)
          : "ecx", "edx", "memory", "cc");
      }
    }
  }

  return false;
}
