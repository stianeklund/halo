#define FILE_REF_MAGIC 0x66696C6F

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
    ((void (*)(file_ref_t *, const char *))0x199700)(info, directory);
  } else {
    ((void (*)(file_ref_t *, const char *))0x199780)(info, directory);
  }

  return info;
}
