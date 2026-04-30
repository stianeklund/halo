/* HaloScript (hs) subsystem — scripting engine init/dispose/update/evaluate. */

/* Allocate and initialize the hs_syntax data table used to store script
 * nodes.  If a scenario is loaded and its script syntax data address field
 * (offset +0x474) already points to the magic value 0x5ccac, the existing
 * table is reused.  Otherwise a fresh data table is allocated (name
 * "script node", max 0x4a39 entries, datum size 0x14) and, if a scenario
 * is present, the old data pointer at scenario+0x480 is freed and the
 * scenario fields are updated to point at the new table. */
void hs_scripts_initialize(void)
{
  char *scenario_tag;

  if (*(int *)0x326a08 == -1)
    scenario_tag = 0;
  else {
    scenario_tag = (char *)global_scenario_get();
    if (scenario_tag != 0 && *(int *)(scenario_tag + 0x474) == 0x5ccac) {
      return;
    }
  }

  *(void **)0x5aa6c8 = (void *)data_new("script node", 0x4a39, 0x14);
  if (*(void **)0x5aa6c8 == 0) {
    error(0, "couldn't allocate script syntax data");
    return;
  }

  data_delete_all(*(data_t *volatile *)0x5aa6c8);

  if (scenario_tag == 0) {
    *(uint8_t *)0x46b6d9 = 1;
    return;
  }

  debug_free(*(void **)(scenario_tag + 0x480), "c:\\halo\\SOURCE\\hs\\hs.c",
             0x150);
  *(int *)(scenario_tag + 0x480) = *(int *)0x5aa6c8;
  *(int *)(scenario_tag + 0x474) = 0x5ccac;
  tag_data_resize((void *)(scenario_tag + 0x488), 0x400);
  tag_block_resize((void *)(scenario_tag + 0x49c), 0);
}

/* Dispose: clean up runtime state. */
void hs_dispose(void)
{
  hs_runtime_dispose_from_old_map();
}

/* Per-tick script update with optional profiling. */
void hs_update(void)
{
  if (*(uint8_t *)0x449ef1 != 0 && *(uint8_t *)0x2f1c18 != 0)
    profile_enter_private((void *)0x2f1c10);

  ((void (*)(void))0xcde00)();

  if (*(uint8_t *)0x449ef1 != 0 && *(uint8_t *)0x2f1c18 != 0)
    profile_exit_private((void *)0x2f1c10);
}

/* Dispose runtime script node data that isn't marked as persistent (bit 3
 * of the flags byte at datum offset +6).  Iterates over all live datums in
 * the hs_syntax data table at 0x5aa6c8 and deletes any whose flag byte
 * does NOT have bit 0x8 set. */
void hs_scripts_dispose(void)
{
  int index;

  for (index = data_next_index(*(data_t *volatile *)0x5aa6c8, NONE);
       index != NONE;
       index = data_next_index(*(data_t *volatile *)0x5aa6c8, index)) {
    void *datum = datum_get(*(data_t *volatile *)0x5aa6c8, index);
    if ((*(uint8_t *)((char *)datum + 6) & 0x8) == 0)
      datum_delete(*(data_t *volatile *)0x5aa6c8, index);
  }
}

/* 0xc3d00 — Look up an hs built-in function descriptor by index.
 * Returns a pointer into the function table at 0x2f1588 (0x1a2 entries). */
void *hs_function_table_get(int16_t function_index)
{
  if (function_index < 0 || function_index >= 0x1a2) {
    display_assert(
      "function_index>=0 && function_index<hs_function_table_count",
      "c:\\halo\\SOURCE\\hs\\hs.c", 0x20a, 1);
    system_exit(-1);
  }
  return ((void **)0x2f1588)[function_index];
}

/* 0xc3e10 — Bounds-checked accessor for the external-global descriptor table.
 * Table at 0x2f3708, count at 0x27d504. */
void *hs_external_global_get(int16_t global_index)
{
  int16_t external_count = *(int16_t *)0x27d504;
  if (global_index < 0 || global_index >= external_count) {
    display_assert("global_index>=0 && global_index<hs_external_global_count",
                   "c:\\halo\\SOURCE\\hs\\hs.c", 0x240, 1);
    system_exit(-1);
  }
  return ((void **)0x2f3708)[global_index];
}

/* 0xc3e60 — Return the HS type of a global variable by script_ref.
 * Bit 15 set: external global (descriptor+0x4).
 * Bit 15 clear: scenario global (element+0x20, block at scenario+0x4a8). */
int16_t hs_global_get_type(uint16_t script_ref)
{
  if (script_ref & 0x8000) {
    void *desc = hs_external_global_get((int16_t)(script_ref & 0x7fff));
    return *(int16_t *)((char *)desc + 0x4);
  }
  char *scenario = (char *)global_scenario_get();
  void *element = tag_block_get_element((void *)(scenario + 0x4a8),
                                        (int)(script_ref & 0x7fff), 0x5c);
  return *(int16_t *)((char *)element + 0x20);
}

/* Find a HaloScript global variable by name.  Searches external globals
 * first (table at 0x2f3708, count at 0x27d504), returning the index with
 * bit 15 set (| 0x8000).  Then searches scenario globals (tag_block at
 * scenario+0x4a8, element size 0x5c), returning the index with bit 15
 * clear (& 0x7FFF).  Returns NONE (-1) if not found. */
int16_t hs_find_global_by_name(const char *name)
{
  int16_t external_count = *(int16_t *)0x27d504;
  int16_t i;
  char *scenario_tag;

  /* Search external globals */
  for (i = 0; i < external_count; i++) {
    if (i < 0 || i >= external_count) {
      display_assert("global_index>=0 && global_index<hs_external_global_count",
                     "c:\\halo\\SOURCE\\hs\\hs.c", 0x240, 1);
      system_exit(-1);
    }
    if (crt_stricmp(name, *(const char **)(((void **)0x2f3708)[i])) == 0) {
      return (int16_t)((uint16_t)i | 0x8000);
    }
  }

  /* Search scenario globals */
  if (*(int *)0x326a08 != NONE) {
    scenario_tag = (char *)global_scenario_get();
    for (i = 0; (int)i < *(int *)(scenario_tag + 0x4a8); i++) {
      const char *global_name = (const char *)tag_block_get_element(
        (void *)(scenario_tag + 0x4a8), (int)i, 0x5c);
      if (crt_stricmp(name, global_name) == 0) {
        return (int16_t)((uint16_t)i & 0x7FFF);
      }
    }
  }

  return NONE;
}

/* 0xc3fc0 — Search the HS function table (0x2f1588, 0x1a2 entries) for a
 * function whose name (at descriptor+4) matches case-insensitively.
 * Returns the zero-based function index, or -1 if not found. */
int16_t hs_find_function_by_name(const char *name)
{
  int16_t i;
  for (i = 0; i < 0x1a2; i++) {
    if (crt_stricmp(*(const char **)((char *)((void **)0x2f1588)[i] + 4),
                    name) == 0)
      return i;
  }
  return -1;
}

/* Load a single HaloScript source file into the scenario's source file list.
 * The file_ref is passed via EBX (register argument).
 *
 * Steps:
 *   1. Verify the file exists on disk via file_exists.
 *   2. Add a new element to the scenario's source files tag_block at +0x4c0.
 *   3. Read the file contents into a temporary memory buffer.
 *   4. Resize the element's tag_data (at element+0x20) to hold the file.
 *   5. Copy the file's short name (up to 0x1f chars) into element+0x00.
 *   6. Copy the file data from the temp buffer into the tag_data region.
 *
 * Returns true if the file was loaded successfully, false on any failure.
 * Error messages are emitted via error() at severity 2. */
bool hs_load_source_file(void *file_ref)
{
  char *scenario_tag;
  void *scripts_block;
  int16_t element_index;
  char *element;
  void *buffer;
  int file_size;
  void *tag_data_ptr;
  void *dest;
  char name_buf[256];

  scenario_tag = (char *)global_scenario_get();

  if (!file_exists((file_ref_t *)file_ref))
    return 0;

  scripts_block = (void *)(scenario_tag + 0x4c0);
  element_index = tag_block_add_element(scripts_block);
  if (element_index == -1) {
    error(2, "maximum source files per scenario exceeded.");
    return 0;
  }

  element =
    (char *)tag_block_get_element(scripts_block, (int)element_index, 0x34);

  buffer = file_read_into_buffer((file_ref_t *)file_ref, &file_size);
  if (buffer == NULL) {
    error(2, "couldn't read source file into memory.");
    return 0;
  }

  tag_data_ptr = (void *)(element + 0x20);
  if (!tag_data_resize(tag_data_ptr, file_size)) {
    error(2, "maximum source file size exceeded.");
    return 0;
  }

  file_reference_get_name((file_ref_t *)file_ref, 4, name_buf);
  csstrncpy(element, name_buf, 0x1f);
  *(uint8_t *)(element + 0x1f) = 0;

  dest = tag_data_get_pointer(tag_data_ptr, 0, file_size);
  csmemcpy(dest, buffer, file_size);

  return 1;
}

/* Check whether the scenario's HaloScript source files have changed on
 * disk since they were last compiled.  Searches for "data\global_scripts.hsc"
 * and all .hsc files in the scenario's "data\<mapname>\scripts\" directory.
 * For each file found, calls FUN_0xc4660 (hs_load_source_file, @EBX=file_ref)
 * to attempt loading.  Returns true (1) if all source files loaded
 * successfully, false (0) if any failed (indicating recompilation needed).
 *
 * Uses find_files to enumerate .hsc files, sorts them with qsort using
 * stricmp-based comparison (FUN_0xc4770), then iterates.  The file extension
 * "hsc" at 0x27ba34 is compared with csstrcmp to filter results. */
bool hs_needs_recompile(void)
{
  uint8_t result;
  char *scenario_tag;
  char path[260]; /* EBP+0xfffffef8 => EBP-0x108, size 0x104 */
  char global_ref[268]; /* EBP+0xfffffdf0 => EBP-0x210, file_ref_t */
  char dir_ref[268]; /* EBP+0xfffffbe4 => EBP-0x41c, file_ref_t */
  char results[2144]; /* EBP+0xfffff384 => EBP-0xc7c, 8*0x10c */
  char name_buf[256]; /* EBP+0xfffffcf0 => EBP-0x310 */
  int16_t count;
  int16_t i;
  const char *name_result;
  char *ebx_ptr;

  result = 1;
  scenario_tag = (char *)global_scenario_get();

  /* Reset scenario source file list at +0x4c0 */
  tag_block_resize((void *)(scenario_tag + 0x4c0), 0);

  /* Get the tag name for the current scenario and build the scripts path */
  name_result = tag_get_name(*(int *)0x326a08);
  crt_sprintf(path, "data\\%s", name_result);
  {
    char *last_sep = strrchr(path, 0x5c);
    crt_sprintf(last_sep + 1, "scripts");
  }

  /* Check for global_scripts.hsc */
  file_reference_create_from_path((file_ref_t *)global_ref,
                                  "data\\global_scripts.hsc", 0);
  if (file_exists((file_ref_t *)global_ref)) {
    result = (uint8_t)hs_load_source_file((void *)global_ref);
  }

  /* Find all .hsc files in the scripts directory */
  file_reference_create_from_path((file_ref_t *)dir_ref, path, 1);
  count = find_files(0, (file_ref_t *)dir_ref, 8, (void *)results);

  /* Sort the results by name using the comparison callback at 0xc4770 */
  qsort((void *)results, (size_t)(int16_t)count, 0x10c,
        (int (*)(const void *, const void *))0xc4770);

  if ((int16_t)count > 0) {
    ebx_ptr = results;
    for (i = (uint16_t)count; i != 0; i--) {
      file_reference_get_name((void *)ebx_ptr, 8, name_buf);
      if (csstrcmp(name_buf, (const char *)0x27ba34) == 0) {
        if (!hs_load_source_file((void *)ebx_ptr))
          result = 0;
      }
      ebx_ptr += 0x10c;
    }
  }

  return (bool)result;
}

/* Report a HaloScript compile error with optional line-number context.
 *
 * Register arguments: error_text@ESI, script_element@EBX, source_start@EDI.
 * Stack argument: error_info (the error description string).
 *
 * If error_text is non-NULL, truncates it at the first newline.  If both
 * script_element and a newline were found, counts newlines backward from the
 * newline position to source_start to determine the line number, then prints:
 *   "[<script_element> line <N>] <error_info>: <error_text>"
 * Otherwise prints the simpler:
 *   "<error_info>: <error_text>"
 *
 * The line counter is treated as int16_t (MOVSX ECX,CX in the original). */
void hs_report_compile_error(void *error_info, char *error_text,
                             char *script_element, void *source_start)
{
  char *nl;
  char *ptr;
  int16_t line;

  nl = NULL;
  if (error_text != NULL) {
    nl = crt_strchr(error_text, '\n');
    if (nl != NULL)
      *nl = '\0';
  }

  if (script_element != NULL && nl != NULL) {
    line = 1;
    ptr = nl;
    if (ptr > (char *)source_start) {
      do {
        if (*ptr == '\n')
          line++;
        ptr--;
      } while (ptr > (char *)source_start);
    }
    error(2, "[%s line %d] %s: %s", script_element, (int)line, error_info,
          error_text);
    return;
  }

  error(2, (const char *)0x259f2c, error_info, error_text);
}

/* Recompile all scenario scripts.  Iterates over each script source entry
 * in the scenario's script sources tag_block at offset +0x4c0 (element size
 * 0x34).  For each, calls tag_data_get_pointer to get the source text, then
 * hs_compile_source to compile it.  If compilation fails, calls the error
 * reporter at 0xc4900 (which takes 3 register args: @ESI, @EBX, @EDI plus
 * 1 stack arg).  Returns true if all scripts compiled successfully.
 * Prints "scripts successfully compiled." on success via console_printf. */
bool hs_mark_recompile(void)
{
  char *scenario_tag;
  void *scripts_block; /* &scenario+0x4c0 */
  char *element;
  void *tag_data_ptr; /* &element[0x20] */
  int source_size;
  void *source_ptr;
  char *error_info;
  char *error_text;
  uint8_t ok;
  int loop_index;
  int i;

  scenario_tag = (char *)global_scenario_get();
  ok = 1;
  hs_syntax_reset(1);

  scripts_block = (void *)(scenario_tag + 0x4c0);
  i = 0;
  loop_index = 0;

  if (*(int *)scripts_block > 0) {
    do {
      element = (char *)tag_block_get_element(scripts_block, i, 0x34);

      tag_data_ptr = (void *)(element + 0x20);
      source_size = *(int *)(element + 0x20);

      source_ptr = tag_data_get_pointer(tag_data_ptr, 0, source_size);
      hs_compile_source(*(int *)tag_data_ptr, source_ptr, &error_info,
                        &error_text);

      if (error_info != 0) {
        /* Get source start pointer again for error reporting */
        void *src_start =
          tag_data_get_pointer(tag_data_ptr, 0, *(int *)tag_data_ptr);
        hs_report_compile_error(error_info, error_text, element, src_start);
        ok = 0;
      }

      loop_index = loop_index + 1;
      i = (int)(int16_t)loop_index;
    } while (i < *(int *)scripts_block);

    if (ok == 0)
      goto cleanup;
  }

  console_printf(0, "scripts successfully compiled.");

cleanup:
  hs_compile_cleanup();
  return (bool)ok;
}

/* Load scenario scripts from the scenario tag.  Allocates a fresh syntax
 * data table via hs_scripts_initialize, then either validates existing
 * compiled scripts or recompiles from source.  If the scenario has no
 * pre-existing globals (offset +0x49c count == 0) but has scripts (offset
 * +0x4c0 count > 0), a full recompile is triggered.  On failure, resets
 * the script tables to empty.
 *
 * If preserve_syntax is non-zero, restores the original hs_syntax_data
 * pointer on exit (used during console evaluate recompile). */
bool hs_load_scenario_scripts(int preserve_syntax)
{
  char *scenario_tag;
  void *old_syntax_data;
  bool needs_recompile;
  bool ok;
  char *error_info;
  char *error_text;

  ok = 1;
  scenario_tag = (char *)global_scenario_get();
  old_syntax_data = *(void **)0x5aa6c8;

  hs_scripts_initialize();

  /* Determine if recompilation is needed: no existing globals but
   * scripts exist */
  if (*(int *)(scenario_tag + 0x49c) == 0 && *(int *)(scenario_tag + 0x4c0) > 0)
    needs_recompile = 1;
  else
    needs_recompile = 0;

  /* Set up syntax data pointer from scenario tag data */
  *(void **)0x5aa6c8 = *(void **)(scenario_tag + 0x480);
  {
    char *syn = *(char **)0x5aa6c8;
    *(int *)(syn + 0x34) = (int)(syn + 0x38);
  }

  if (needs_recompile) {
    /* Scenarios were merged — must recompile */
    error(0, "recompiling scripts after scenarios were merged.");
  } else {
    /* Try to validate existing compiled syntax tree */
    if (hs_validate_syntax(&error_info, &error_text)) {
      /* Validation succeeded — grow string data if needed */
      if (*(int *)(scenario_tag + 0x488) < 0x400) {
        ok = tag_data_resize((void *)(scenario_tag + 0x488),
                             *(int *)(scenario_tag + 0x488) + 0x400);
      }
      goto done;
    }

    /* Validation failed — report the error.
     * Note: when both strings present, error_text is printed first
     * in the "%s: %s" format, matching the original push order. */
    if (error_info == 0) {
      error(0, "an unspecified error occurred loading scripts");
    } else if (error_text == 0) {
      error(0, (const char *)0x257984, error_info);
    } else {
      error(0, (const char *)0x259f2c, error_text, error_info);
    }
  }

  /* Recompile path */
  if (hs_mark_recompile()) {
    if (hs_validate_syntax(&error_info, &error_text)) {
      ok = 1;
      goto done;
    }
  }

  /* Recompile failed or validation still fails — reset everything */
  data_make_valid(*(data_t *volatile *)0x5aa6c8);
  if (!tag_block_resize((void *)(scenario_tag + 0x4a8), 0))
    goto reset_error;
  if (!tag_block_resize((void *)(scenario_tag + 0x49c), 0))
    goto reset_error;
  {
    char *s2 = (char *)global_scenario_get();
    if (!tag_data_resize((void *)(s2 + 0x488), 0x400))
      goto reset_error;
  }
  goto reset_ok;

reset_error:
  error(0, "couldn't reset scripts.");

reset_ok:
  ok = 0;

done:
  if (preserve_syntax != 0)
    *(void **)0x5aa6c8 = old_syntax_data;

  return ok;
}

/* Initialize hs for a new map: set up the script environment from the
 * scenario's script data if present. */
void hs_initialize_for_new_map(void)
{
  char *scenario_tag;

  if (*(int *)0x326a08 == -1)
    scenario_tag = 0;
  else
    scenario_tag = (char *)global_scenario_get();

  hs_scripts_initialize();

  if (scenario_tag != 0 && *(int *)(scenario_tag + 0x474) != 0)
    hs_load_scenario_scripts(0);

  hs_runtime_initialize();
  hs_runtime_initialize_for_new_map();
}

/* Dispose from old map: clean up script threads and runtime data. */
void hs_dispose_from_old_map(void)
{
  if (*(void **)0x5aa6c8 != 0) {
    hs_scripts_dispose();
    if (*(uint8_t *)0x46b6d9 != 0) {
      data_make_invalid(*(void **)0x5aa6c8);
      data_dispose(*(void **)0x5aa6c8);
      *(uint8_t *)0x46b6d9 = 0;
    }
    *(void **)0x5aa6c8 = 0;
  }
  hs_runtime_dispose_from_old_map();
  hs_runtime_dispose();
}

/* Initialize the scripting engine: validate type names, set up
 * runtime tables, and initialize for the current map. */
void hs_initialize(void)
{
  char *scenario_tag;

  if (*(void **)0x2f1568 == 0) {
    display_assert("you can't add an hs type without defining its name.",
                   "c:\\halo\\SOURCE\\hs\\hs.c", 0xf5, 1);
    system_exit(-1);
  }

  ((void (*)(void))0xce150)();
  ((void (*)(void))0xca700)();

  if (*(int *)0x326a08 == -1)
    scenario_tag = 0;
  else
    scenario_tag = (char *)global_scenario_get();

  hs_scripts_initialize();

  if (scenario_tag != 0 && *(int *)(scenario_tag + 0x474) != 0)
    hs_load_scenario_scripts(0);

  hs_runtime_initialize();
  hs_runtime_initialize_for_new_map();
}

/*
 * hs_console_evaluate — parse and evaluate a console command as HaloScript.
 *
 * Strips comments at ';', skips leading whitespace, then wraps the command
 * in S-expression syntax if needed:
 *   - If the command starts with '(', pass it through as-is.
 *   - If the first word is a known global variable:
 *     - With arguments: wrap as "(set <command>)" to assign.
 *     - Without arguments: pass through to read the global.
 *   - Otherwise: wrap as "(<command>)" to call as a function.
 *
 * After evaluation, if the recompile flag (0x46b6d8) is set, tears down
 * and reinitializes the script environment.
 *
 * Returns true if the command compiled and executed successfully.
 *
 * Confirmed: csstrncpy(copy, command, 0x400), null-terminate at [0x3ff].
 * Confirmed: strchr for ';' zeroes copy[0] (treats ';' lines as comments).
 * Confirmed: isspace loop to skip leading whitespace.
 * Confirmed: strchr for ' ' splits first word, hs_find_global_by_name.
 * Confirmed: sprintf with "(%s)" at 0x27bb64 or "(set %s)" at 0x27bb6c.
 * Confirmed: interleaved pushes — csstrlen(1 arg) then hs_compile(4 args).
 * Confirmed: error(2, "%s: %s", ...) at 0x259f2c on parse failure.
 * Confirmed: assert at line 0x507 for unreachable switch case.
 * Confirmed: post-eval recompile mirrors hs_initialize_for_new_map logic.
 */
bool hs_console_evaluate(const char *command)
{
  char wrapped[1024];
  char copy[1024];
  int error_info;
  char *error_text;
  bool result;
  const char *source;
  char *space;
  int16_t mode;
  int compiled;
  int executed;
  char *scenario_tag;

  result = 0;
  csstrncpy(copy, command, 0x400);
  copy[0x3ff] = 0;

  /* strip comments: if ';' is present anywhere, blank the command */
  if (crt_strchr(copy, ';') != NULL) {
    copy[0] = 0;
  }

  /* skip leading whitespace */
  source = copy;
  while (*source != 0) {
    if (crt_isspace((int)(unsigned char)*source) == 0)
      break;
    source++;
  }
  if (*source == 0)
    goto post_eval;

  mode = 0;
  hs_syntax_reset(0);

  if (copy[0] != '(') {
    space = crt_strchr(copy, ' ');
    if (space != NULL)
      *space = 0;

    if (hs_find_global_by_name(copy) == -1) {
      mode = 1;
    } else {
      if (space == NULL)
        goto skip_wrap;
      mode = 2;
    }

    if (space != NULL)
      *space = ' ';
  }

skip_wrap:
  if (mode != 0) {
    if (mode == 1) {
      crt_sprintf(wrapped, (const char *)0x27bb64, copy);
    } else if (mode == 2) {
      crt_sprintf(wrapped, (const char *)0x27bb6c, copy);
    } else {
      display_assert(NULL, "c:\\halo\\SOURCE\\hs\\hs.c", 0x507, 1);
      system_exit(-1);
    }
    source = wrapped;
  }

  compiled = csstrlen(source);
  executed = hs_compile(compiled, source, &error_info, &error_text);

  if (executed != -1) {
    result = 1;
    hs_runtime_execute(executed);
  } else {
    if (error_info != 0) {
      if (error_text != NULL) {
        char *nl = crt_strchr(error_text, '\n');
        if (nl != NULL)
          *nl = 0;
      }
      error(2, (const char *)0x259f2c, error_info, error_text);
    }
  }

  hs_compile_cleanup();

post_eval:
  if (*(uint8_t *)0x46b6d8 != 0) {
    if (hs_needs_recompile()) {
      hs_mark_recompile();
      if (*(void **)0x5aa6c8 != 0) {
        hs_scripts_dispose();
        if (*(uint8_t *)0x46b6d9 != 0) {
          data_make_invalid(*(void **)0x5aa6c8);
          data_dispose(*(void **)0x5aa6c8);
          *(uint8_t *)0x46b6d9 = 0;
        }
        *(void **)0x5aa6c8 = 0;
      }
      hs_runtime_dispose_from_old_map();
      hs_runtime_dispose();

      if (*(int *)0x326a08 == -1) {
        scenario_tag = 0;
      } else {
        scenario_tag = (char *)global_scenario_get();
      }

      hs_scripts_initialize();

      if (scenario_tag != 0 && *(int *)(scenario_tag + 0x474) != 0)
        hs_load_scenario_scripts(0);

      hs_runtime_initialize();
      hs_runtime_initialize_for_new_map();
    }
    *(uint8_t *)0x46b6d8 = 0;
  }

  return result;
}

/* Reset the HaloScript compile state.  Asserts that the compiler is not
 * already initialized (global at 0x46b6e0).  Zeroes compile globals and
 * stores the recompile flag (param_1) at 0x46b805.  If param_1 is non-zero,
 * also resets several scenario tag_block and tag_data structures (globals,
 * scripts, source files) and resets the syntax data table index. */
void hs_syntax_reset(int param_1)
{
  char *scenario_tag;

  if (*(uint8_t *)0x46b6e0 != 0) {
    display_assert("!hs_compile_globals.initialized",
                   "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x5b, 1);
    system_exit(-1);
  }

  *(uint8_t *)0x46b6e0 = 1;
  *(int *)0x46b6e8 = 0;
  *(int *)0x46b6e4 = 0;
  *(uint8_t *)0x46b805 = (uint8_t)param_1;
  *(uint8_t *)0x46b6f8 = 0;
  *(int *)0x46b6fc = 0;

  if ((uint8_t)param_1 != 0) {
    scenario_tag = (char *)global_scenario_get();
    tag_block_resize((void *)(scenario_tag + 0x49c), 0);
    tag_block_resize((void *)(scenario_tag + 0x4a8), 0);
    tag_block_resize((void *)(scenario_tag + 0x4b4), 0);
    tag_data_resize((void *)(scenario_tag + 0x488), 0);
    data_make_valid(*(data_t *volatile *)0x5aa6c8);
  }
}

/* 0xc57a0 — Check whether a source offset is within the valid range
 * [0, hs_compile_globals.source_size). Sets error message on failure. */
bool hs_source_offset_valid(int offset)
{
  if (offset < 0 || offset >= *(int *)0x46b6e4) {
    *(const char **)0x46b6fc = "bad source offset (you need to recompile.)";
    return false;
  }
  return true;
}
