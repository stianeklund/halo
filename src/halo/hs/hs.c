/* HaloScript (hs) subsystem — scripting engine init/dispose/update/evaluate. */

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
