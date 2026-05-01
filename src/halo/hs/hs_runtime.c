/* Validate the syntax tree after loading a scenario. Iterates all syntax
 * nodes and checks for consistency: valid types, valid source offsets,
 * valid script indices, and correct function references. If any node fails
 * validation, sets the compile error message and returns false.
 *
 * This is the last step before scripts can run — it catches stale compiled
 * data that no longer matches the current function table or scenario layout.
 *
 * Callees (all via hardcoded addresses, not in kb.json):
 *   0xc3d00 = hs_function_table_get (short function_index) -> void*
 *   0xc3e60 = hs_script_get_type (uint16 script_ref) -> short
 *   0xc3fc0 = hs_function_find_by_name (char *name) -> short
 *   0xc57a0 = hs_source_offset_valid (int offset) -> bool
 *   0xc73a0 = hs_type_check_expression (@EDI=datum_index) -> bool
 *   0xcb070 = hs_types_compatible (short actual, short desired) -> bool
 * [ported]
 *
 * Globals:
 *   0x5aa6c8 = hs_syntax_data (data_t*)
 *   0x46b6e4 = hs_compile_globals.source_size
 *   0x46b6e8 = hs_compile_globals.compiled_source
 *   0x46b6fc = hs_compile_globals.error_message
 *   0x46b700 = hs_compile_globals.error_offset
 *   0x46b808 = hs_compile_globals.validating (uint8_t)
 */
bool hs_validate_syntax(char **error_info, char **error_text)
{
  bool ok;
  int datum_index;
  char *node;
  short node_type;
  short result_type;
  char *scenario;
  char *script_element;

  ok = true;

  /* Set up compile globals for validation pass. */
  scenario = (char *)global_scenario_get();
  *(int *)0x46b6e8 = *(int *)(scenario + 0x494);
  scenario = (char *)global_scenario_get();
  *(int *)0x46b6e4 = *(int *)(scenario + 0x488) - 0x400;
  *(int *)0x46b6fc = 0;
  *(uint8_t *)0x46b808 = 1;
  *(int *)error_info = 0;
  *(int *)error_text = 0;

  /* Iterate all syntax nodes. */
  datum_index = data_next_index(*(data_t **)0x5aa6c8, -1);
  while (datum_index != -1) {
    if (!ok)
      break;

    node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
    node_type = *(int16_t *)(node + 0x4);

    /* Check if the node type is valid (4..0x30 inclusive) or type 2. */
    if (node_type < 4 || node_type > 0x30) {
      if (node_type != 2) {
        *(int *)0x46b6fc = (int)"missing type (you need to recompile scripts.)";
        goto error;
      }
      /* Type 2 (function call) — skip to next node. */
      goto next;
    }

    /* Check the constant flag (bit 0 of byte at +6). */
    {
      char *node2 = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
      if (*(uint8_t *)(node2 + 0x6) & 1) {
        /* Constant node — check source offset range and type-check. */
        if (*(int16_t *)(node + 0x4) >= 9 || (*(uint8_t *)(node + 0x6) & 4)) {
          /* Source offset validation. */
          bool offset_ok = true;
          if (*(int *)(node + 0xc) < 0 ||
              *(int *)(node + 0xc) >= *(int *)0x46b6e4) {
            *(int *)0x46b6fc =
              (int)"bad source offset (you need to recompile.)";
            offset_ok = false;
          }
          ok = offset_ok;
          if (ok) {
            ok = FUN_000c73a0(datum_index);
          }
        }

        if (!ok)
          goto use_node_type;

        /* If the reparse flag (bit 2) is set, get the script type. */
        if (*(uint8_t *)(node + 0x6) & 4) {
          result_type =
            hs_global_get_type((uint16_t) * (int16_t *)(node + 0x10));
          goto check_type;
        }

        goto use_node_type;
      }
    }

    /* Non-constant node: check the script-reference flag (bit 1). */
    if (*(uint8_t *)(node + 0x6) & 2) {
      /* Script reference node. */
      if (*(int16_t *)(node + 0x2) < 0) {
        goto check_script_index;
      }

      {
        int16_t script_idx = *(int16_t *)(node + 0x2);
        scenario = (char *)global_scenario_get();
        if ((int)script_idx >= *(int *)(scenario + 0x49c)) {
          goto check_script_index;
        }

        /* tag_block_get_element (0x19b210): 3 stack args. */
        {
          char *scenario2 = (char *)global_scenario_get();
          script_element = (char *)tag_block_get_element(scenario2 + 0x49c,
                                                         (int)script_idx, 0x5c);
        }

        if (*(int16_t *)(script_element + 0x20) == 3) {
          goto script_ok;
        }
      }

    check_script_index:
      if (*(int16_t *)(script_element + 0x20) != 4) {
        *(int *)0x46b6fc = (int)"bad script index (you need to recompile.)";
        goto error;
      }

    script_ok:
      result_type = *(int16_t *)(script_element + 0x22);
      goto check_type;
    }

    /* Non-constant, non-script-reference: function call node. */
    {
      char *fn_node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
      int next_expr = *(int *)(fn_node + 0x10);

      if (next_expr == -1) {
        *(int *)0x46b6fc =
          (int)"corrupt syntax tree (you need to recompile scripts.)";
        goto error;
      }

      {
        char *inner_node = (char *)datum_get(*(data_t **)0x5aa6c8, next_expr);
        if (*(int16_t *)(inner_node + 0x4) != 2) {
          *(int *)0x46b6fc =
            (int)"corrupt syntax tree (you need to recompile scripts.)";
          goto error;
        }

        /* Validate the source offset of the inner node. */
        {
          bool src_ok;
          int src_offset = *(int *)(inner_node + 0xc);
          src_ok = hs_source_offset_valid(src_offset);
          if (!src_ok)
            goto error;

          /* Look up the function by name in the compiled source. */
          {
            int name_addr = *(int *)(inner_node + 0xc) + *(int *)0x46b6e8;
            int16_t func_idx =
              hs_find_function_by_name((const char *)name_addr);
            if (func_idx == -1) {
              *(int *)0x46b6fc =
                (int)"missing function (you need to recompile scripts.)";
              goto error;
            }

            /* Update the node's function index and look up the return
             * type. */
            *(int16_t *)(node + 0x2) = func_idx;

            hs_function_table_get(func_idx);
            result_type =
              *(int16_t *)hs_function_table_get(*(int16_t *)(node + 0x2));
            goto check_type;
          }
        }
      }
    }

  use_node_type:
    result_type = *(int16_t *)(node + 0x2);

  check_type:
    if (ok) {
      /* Validate that result_type is valid (4..0x30) or passthrough
       * (3). */
      if ((result_type < 4 || result_type > 0x30) && result_type != 3) {
        *(int *)0x46b6fc = (int)"type is inconsistent with usage "
                                "(you need to recompile scripts.)";
        goto error;
      }

      if (!hs_types_compatible(result_type, *(int16_t *)(node + 0x4))) {
        *(int *)0x46b6fc = (int)"type is inconsistent with usage "
                                "(you need to recompile scripts.)";
        goto error;
      }
      ok = true;
    }
    goto next;

  error:
    ok = false;

  next:
    datum_index = data_next_index(*(data_t **)0x5aa6c8, datum_index);
  }

  /* If validation failed, report the error. */
  if (!ok) {
    *(int *)error_info = *(int *)0x46b6fc;
    if (*(int *)0x46b700 != -1) {
      *(int *)error_text = *(int *)0x46b700 + *(int *)0x46b6e8;
    }
  }

  /* Clean up compile globals. */
  *(int *)0x46b6e8 = 0;
  *(int *)0x46b6fc = 0;
  *(uint8_t *)0x46b808 = 0;

  return ok;
}

/* Compile a HaloScript expression from source text. Allocates syntax nodes,
 * copies source into the compiled source buffer, parses one expression,
 * and wraps it in a begin/void node pair for execution. Returns the root
 * syntax datum index on success, or -1 on failure.
 *
 * If no scenario is loaded, allocates a temporary buffer for the source
 * (freed later by hs_compile_cleanup). Otherwise uses the scenario's
 * string constants area offset by 0x400 bytes.
 *
 * Globals:
 *   0x326a08 = global_scenario_index
 *   0x5aa6c8 = hs_syntax_data (data_t*)
 *   0x46b6e4 = hs_compile_globals.source_size
 *   0x46b6e8 = hs_compile_globals.compiled_source
 *   0x46b6fc = hs_compile_globals.error_message
 *   0x46b700 = hs_compile_globals.error_offset
 *   0x46b804 = hs_compile_globals.source_allocated
 */
int hs_compile(int source_length, const char *source, int *error_info,
               char **error_text)
{
  int base_offset;
  char *src_cursor;

  if (source_length >= 0x400)
    return -1;

  if (*(int *)0x326a08 == -1) {
    /* No scenario loaded — allocate temporary buffer. */
    base_offset = 0;
    *(int *)0x46b6e8 = (int)debug_malloc(
      source_length + 1, 0, "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0xaf);
    *(uint8_t *)0x46b804 = 1;
    if (*(int *)0x46b6e8 == 0) {
      display_assert("hs_compile_globals.compiled_source",
                     "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0xb2, true);
      system_exit(-1);
    }
  } else {
    /* Scenario loaded — use string constants area. */
    char *scenario = (char *)global_scenario_get();
    if (*(int *)(scenario + 0x488) < 0x400) {
      display_assert("global_scenario_get()->hs_string_constants.size>="
                     "HS_MAXIMUM_DYNAMIC_SOURCE_DATA_BYTES",
                     "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0xa6, true);
      system_exit(-1);
    }
    scenario = (char *)global_scenario_get();
    base_offset = *(int *)(scenario + 0x488) - 0x400;
    scenario = (char *)global_scenario_get();
    *(int *)0x46b6e8 = *(int *)(scenario + 0x494);
  }

  /* Copy source into compiled source buffer at the base offset. */
  csmemcpy((void *)(*(int *)0x46b6e8 + base_offset), (void *)source,
           source_length);
  *(int *)0x46b6e4 = base_offset + source_length;
  *(uint8_t *)(*(int *)0x46b6e4 + *(int *)0x46b6e8) = 0;

  /* Initialize parse state. */
  src_cursor = (char *)(*(int *)0x46b6e8 + base_offset);
  *(int *)0x46b6fc = 0;
  *(int *)error_info = 0;
  *(int *)error_text = 0;
  *(int *)0x46b700 = -1;

  FUN_000c72b0(&src_cursor);

  if (*src_cursor == '\0')
    return -1;

  int expr_datum = FUN_000c7be0(&src_cursor);

  if (*(int *)0x46b6fc != 0)
    goto compile_error;

  /* Allocate two new syntax nodes to wrap the expression. */
  {
    int node1 = data_new_at_index(*(data_t **)0x5aa6c8);
    int node2 = data_new_at_index(*(data_t **)0x5aa6c8);

    if (node1 != -1 && node2 != -1) {
      char *n1 = (char *)datum_get(*(data_t **)0x5aa6c8, node1);
      char *n2 = (char *)datum_get(*(data_t **)0x5aa6c8, node2);

      *(int *)(n1 + 0x10) = node2;
      *(int *)(n1 + 0x8) = -1;

      /* Copy source offset from the parsed expression node. */
      {
        char *expr_node = (char *)datum_get(*(data_t **)0x5aa6c8, expr_datum);
        *(int *)(n1 + 0xc) = *(int *)(expr_node + 0xc);
      }

      *(int16_t *)(n1 + 0x6) = 0;
      *(int *)(n2 + 0x8) = expr_datum;
      *(int *)(n2 + 0xc) = -1;
      *(int16_t *)(n2 + 0x2) = 0x16; /* hs_type_void */
      *(int16_t *)(n2 + 0x6) = 1;
      *(int16_t *)(n2 + 0x4) = 2; /* hs_node_type_function_call */

      /* hs_type_check: 2 stack args (datum_index, check_type). */
      {
        bool ok = hs_type_check(node1, 4);
        if (ok)
          return node1;
      }
    }
  }

compile_error:
  *(int *)error_info = *(int *)0x46b6fc;
  if (*(int *)0x46b700 != -1) {
    *(int *)0x46b700 = *(int *)0x46b700 - base_offset;
    if (*(int *)0x46b700 < 0 || *(int *)0x46b700 >= source_length) {
      display_assert("hs_compile_globals.error_offset>=0 && "
                     "hs_compile_globals.error_offset<source_size",
                     "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0xeb, true);
      system_exit(-1);
    }
    *error_text = (char *)(*(int *)0x46b700 + (int)source);
  }

  return -1;
}

/* Compile a source file into the syntax tree. Parses multiple top-level
 * expressions from the source, checking each with hs_type_check. On
 * failure, reports error info and adjusts error offset relative to the
 * source file.
 *
 * 0xc5730 = hs_compile_source_setup (@EDI=source_file_size, stack: source_ptr)
 * 0xc72b0 = skip_whitespace (@ESI=&cursor)
 * 0xc7be0 = hs_parse_expression (@EAX=&cursor, returns datum index)
 */
bool hs_compile_source(int source_file_size, void *source_ptr,
                       char **error_info, char **error_text)
{
  char *cursor;
  bool ok;
  int expr_datum;

  cursor = FUN_000c5730(source_file_size, source_ptr);

  if (cursor == NULL) {
    *(int *)error_info = (int)"couldn't allocate memory for compiled source.";
    return false;
  }

  *(int *)0x46b6fc = 0;
  *(int *)error_info = 0;
  *(int *)error_text = 0;
  ok = true;
  *(int *)0x46b700 = -1;

  FUN_000c72b0(&cursor);

  while (*cursor != '\0') {
    expr_datum = FUN_000c7be0(&cursor);
    FUN_000c72b0(&cursor);

    if (*(int *)0x46b6fc != 0)
      goto parse_error;

    ok = hs_type_check(expr_datum, 1);
    if (!ok)
      goto parse_error;
  }

  if (ok)
    return true;

parse_error:
  if (*(int *)0x46b6fc == 0) {
    display_assert("tell matt that somebody failed to correctly report a "
                   "parsing error.",
                   "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x131, true);
    system_exit(-1);
  }

  *error_info = (char *)*(int *)0x46b6fc;
  *(uint8_t *)0x46b6f8 = 1;

  if (*(int *)0x46b700 != -1) {
    *(int *)0x46b700 = *(int *)0x46b700 + (source_file_size - *(int *)0x46b6e4);
    if (*(int *)0x46b700 < 0 || *(int *)0x46b700 >= source_file_size) {
      display_assert("hs_compile_globals.error_offset>=0 && "
                     "hs_compile_globals.error_offset<source_file_size",
                     "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x13b, true);
      system_exit(-1);
    }
    *error_text = (char *)(*(int *)0x46b700 + (int)source_ptr);
  }

  return false;
}

/* Clean up compile state after hs_compile or hs_compile_source.
 * If scripts were successfully compiled (hs_compile_globals.dirty),
 * either recompiles scripts from scratch or resizes the scenario's
 * tag blocks. Frees any temporarily allocated source buffer.
 *
 * Globals:
 *   0x46b6e0 = hs_compile_globals.initialized
 *   0x46b6e8 = hs_compile_globals.compiled_source (allocation ptr)
 *   0x46b6f8 = hs_compile_globals.error_occurred
 *   0x46b804 = hs_compile_globals.source_allocated
 *   0x46b805 = hs_compile_globals.dirty
 *   0x5aa6c8 = hs_syntax_data (data_t*)
 */
void hs_compile_cleanup(void)
{
  if (*(uint8_t *)0x46b6e0 == 0) {
    display_assert("hs_compile_globals.initialized",
                   "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x75, true);
    system_exit(-1);
  }

  if (*(uint8_t *)0x46b805 != 0) {
    if (*(uint8_t *)0x46b6f8 == 0) {
      /* No error — recompile scripts from scratch. */
      hs_compile_recompile_scripts();
    } else {
      /* Error occurred — resize tag blocks to zero and re-validate
       * syntax data. */
      char *scenario = (char *)global_scenario_get();
      tag_block_resize(scenario + 0x49c, 0);
      tag_block_resize(scenario + 0x4a8, 0);
      tag_data_resize(scenario + 0x488, 0);
      data_make_valid(*(data_t **)0x5aa6c8);
    }

    /* Free the compiled source allocation if it exists. */
    if (*(int *)0x46b6e8 != 0) {
      debug_free(*(void **)0x46b6e8, "c:\\halo\\SOURCE\\hs\\hs_compile.c",
                 0x87);
    }
  }

  if (*(uint8_t *)0x46b804 != 0) {
    debug_free(*(void **)0x46b6e8, "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x8c);
    *(int *)0x46b6e8 = 0;
    *(uint8_t *)0x46b804 = 0;
  }

  *(uint8_t *)0x46b6e0 = 0;
}

/* HaloScript runtime — thread management and script execution. */

/* Dispose runtime state from old map: invalidate thread data and
 * clean up any allocated script globals. */
void hs_runtime_dispose_from_old_map(void)
{
  int16_t idx;
  char *data;

  data_make_invalid(*(data_t **)0x5aa6c4);

  idx = *(int16_t *)0x27d504;
  data = *(char **)0x5aa6c0;
  while (idx < *(int16_t *)(data + 0x2e)) {
    if (datum_absolute_index_to_index((data_t *)data, (int)idx) != 0)
      datum_delete((data_t *)data, (int)idx);
    idx++;
    data = *(char **)0x5aa6c0;
  }

  *(uint8_t *)0x46b810 = 0;
}

/* 0xca940 */
static int hs_thread_new(int script_index, int type)
{
  int thread_index;
  char *thread;
  char *stack;
  char *script;

  if (type < 0 || type >= 3) {
    display_assert("type>=0 && type<NUMBER_OF_HS_THREAD_TYPES",
                   "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x26f, true);
    system_exit(-1);
  }

  if (type == 0 && script_index == -1) {
    display_assert("type!=_hs_thread_type_script || script_index!=NONE",
                   "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x270, true);
    system_exit(-1);
  }

  thread_index = data_new_at_index(*(data_t **)0x5aa6c4);
  if (thread_index != -1) {
    thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_index);
    stack = thread + 0x18;
    *(char **)(thread + 0x10) = stack;
    *(int32_t *)stack = 0;
    *(int16_t *)(*(char **)(thread + 0x10) + 0xc) = 0;
    *(int32_t *)(*(char **)(thread + 0x10) + 0x4) = -1;
    *(uint8_t *)(thread + 0x2) = (uint8_t)type;
    *(int32_t *)(thread + 0x4) = script_index;
    *(uint8_t *)(thread + 0x3) = 0;

    if (script_index != -1) {
      script = (char *)tag_block_get_element(
        (char *)global_scenario_get() + 0x49c, script_index, 0x5c);
      if (*(int16_t *)(script + 0x20) == 1) {
        *(int32_t *)(thread + 0x8) = -2;
        return thread_index;
      }
    }
    *(int32_t *)(thread + 0x8) = 0;
  }
  return thread_index;
}

/* 0xcaa30 — Delete an HS thread by handle. Asserts that the thread's type is
 * not _hs_thread_type_script (type==0) before deleting. Called when a
 * console-command thread (type==2) finishes execution in FUN_000cd840.
 */
static void FUN_000caa30(int thread_handle)
{
  char *thread;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_handle);
  if (*(uint8_t *)(thread + 0x2) == 0) {
    display_assert("hs_thread_get(thread_index)->type!=_hs_thread_type_script",
                   "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x290, true);
    system_exit(-1);
  }
  datum_delete(*(data_t **)0x5aa6c4, thread_handle);
}

/* 0xcaa80 */
static char *hs_get_thread_script_name(int thread_index)
{
  char *thread;
  uint8_t type;
  int script_index;
  char *scenario;
  char *script_entry;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_index);
  type = *(uint8_t *)(thread + 0x2);

  if (type == 0) {
    thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_index);
    script_index = *(int32_t *)(thread + 0x4);
    scenario = (char *)global_scenario_get();
    script_entry = (char *)tag_block_get_element((char *)scenario + 0x49c,
                                                 script_index, 0x5c);
    return script_entry;
  }

  if (type == 1) {
    return "[global initialize]";
  }

  if (type == 2) {
    return "[console command]";
  }

  display_assert(NULL, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x2a9, true);
  system_exit(-1);
  return NULL;
}

/* 0xcab00 — Push a new frame onto the HaloScript thread's stack.
 * Allocates the next frame by advancing thread->stack_ptr past the current
 * frame, sets the new frame's back-link to the previous frame pointer, and
 * zeroes the new frame's size field.
 *
 * Frame layout (each frame is at thread+0x18..thread+0x218):
 *   +0x00 (void*) : back-link to previous frame
 *   +0x04 (int)   : expression index (set by caller after push)
 *   +0x08 (void*) : destination value pointer (set by caller)
 *   +0x0c (int16_t): frame size in bytes (this function zeroes it)
 *
 * Stack overflow is fatal: formats a message and halts via display_assert.
 */
static void hs_thread_push_frame(int thread_handle)
{
  char *thread;
  char *cur_frame;
  char *new_frame;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_handle);
  cur_frame = *(char **)(thread + 0x10);

  /* new_frame = cur_frame + cur_frame->size + 0x10 */
  new_frame = cur_frame + (int)*(int16_t *)(cur_frame + 0xc) + 0x10;

  /* Overflow check: (new_frame + 0x10) must be below thread+0x218 */
  if ((unsigned int)(new_frame + 0x10) >= (unsigned int)(thread + 0x218)) {
    const char *script_name = hs_get_thread_script_name(thread_handle);
    const char *msg = csprintf(
      (char *)0x5ab100,
      "a problem occurred while executing the script %s: %s (%s)", script_name,
      "stack overflow.",
      "(byte *) (new_frame+1)<thread->stack_data+HS_THREAD_STACK_SIZE");
    display_assert(msg, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x35e, true);
    system_exit(-1);
  }

  /* Link new frame and advance stack pointer */
  *(char **)(new_frame + 0x0) = cur_frame;
  *(char **)(thread + 0x10) = new_frame;
  *(int16_t *)(new_frame + 0xc) = 0;
}

/* 0xcaba0 — Allocate `size` bytes from the current HaloScript thread stack
 * frame's data area. Returns a pointer to the newly allocated region.
 *
 * The HS thread stack is a fixed-size region [thread+0x18 .. thread+0x218).
 * Each frame begins with a 0xe-byte header:
 *   +0x00 (void*)   : back-link to previous frame
 *   +0x04 (int)     : expression index
 *   +0x08 (void*)   : destination value pointer
 *   +0x0c (int16_t) : current data size in bytes
 * Data starts at frame+0xe; this function returns (frame + old_size + 0xe)
 * and increments frame->size by `size`.
 *
 * Three fatal assertions (line 0x37d–0x37f):
 *   1. valid_thread: thread pointer is within the data array, and the frame
 *      pointer lies in [thread+0x18, thread+0x218).
 *   2. size != 0
 *   3. frame->data + frame->size + size <= thread + HS_THREAD_STACK_SIZE
 *
 * ABI: thread_handle@<eax>, size on stack; returns void* in EAX.
 */
static void *hs_thread_stack_alloc(int thread_handle, int size)
{
  data_t *hs_threads;
  char *thread;
  char *frame;
  int16_t old_size;
  const char *script_name;
  const char *msg;

  hs_threads = *(data_t **)0x5aa6c4;
  thread = (char *)datum_get(hs_threads, thread_handle);
  frame = *(char **)(thread + 0x10);

  /* valid_thread(thread): thread in array bounds, frame in stack area,
   * and current data end within stack.
   * data_t offsets: +0x34=data (base), +0x2e=current_count, +0x22=size (elem).
   */
  if ((unsigned int)thread < (unsigned int)(hs_threads->data) ||
      (unsigned int)thread >= (unsigned int)((char *)hs_threads->data +
                                             (int)hs_threads->current_count *
                                               (int)hs_threads->size) ||
      (unsigned int)frame < (unsigned int)(thread + 0x18) ||
      (unsigned int)frame >= (unsigned int)(thread + 0x218) ||
      (unsigned int)(frame + (int)*(int16_t *)(frame + 0xc) + 0xe) >
        (unsigned int)(thread + 0x218)) {
    script_name = hs_get_thread_script_name(thread_handle);
    msg = csprintf((char *)0x5ab100,
                   "a problem occurred while executing the script %s: %s (%s)",
                   script_name, "valid_thread(thread)", "corrupted stack.");
    display_assert(msg, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x37d, true);
    system_exit(-1);
  }

  if (size == 0) {
    script_name = hs_get_thread_script_name(thread_handle);
    msg = csprintf((char *)0x5ab100,
                   "a problem occurred while executing the script %s: %s (%s)",
                   script_name,
                   "attempt to allocate zero space from the stack.", "size");
    display_assert(msg, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x37e, true);
    system_exit(-1);
  }

  /* frame->data + frame->size + size <= thread + HS_THREAD_STACK_SIZE */
  if ((unsigned int)(frame + (int)*(int16_t *)(frame + 0xc) + 0xe + size) >
      (unsigned int)(thread + 0x218)) {
    script_name = hs_get_thread_script_name(thread_handle);
    msg = csprintf(
      (char *)0x5ab100,
      "a problem occurred while executing the script %s: %s (%s)", script_name,
      "stack overflow.",
      "frame->data+frame->size+size<=thread->stack_data+HS_THREAD_STACK_SIZE");
    display_assert(msg, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x37f, true);
    system_exit(-1);
  }

  old_size = *(int16_t *)(frame + 0xc);
  *(int16_t *)(frame + 0xc) = old_size + (int16_t)size;
  return (void *)(frame + (int)old_size + 0xe);
}

/* 0xcaff0 */
static bool hs_object_types_compatible(int16_t actual_offset,
                                       int16_t desired_offset)
{
  uint16_t *masks = (uint16_t *)0x26f320;
  uint16_t actual_mask;
  uint16_t desired_mask;

  if (actual_offset < 0 || actual_offset >= 6) {
    display_assert("actual_type>=0 && actual_type<NUMBER_OF_HS_OBJECT_TYPES",
                   "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x599, true);
    system_exit(-1);
  }

  if (desired_offset < 0 || desired_offset >= 6) {
    display_assert("desired_type>=0 && desired_type<NUMBER_OF_HS_OBJECT_TYPES",
                   "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x59a, true);
    system_exit(-1);
  }

  actual_mask = masks[actual_offset];
  desired_mask = masks[desired_offset];
  return (desired_mask & actual_mask) == actual_mask;
}

/* 0xcb070 */
bool hs_types_compatible(int16_t actual_type, int16_t desired_type)
{
  if (actual_type != 3 && (actual_type < 4 || actual_type >= 0x31)) {
    display_assert("actual_type==_hs_passthrough || hs_type_valid(actual_type)",
                   "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x5a4, true);
    system_exit(-1);
  }

  if (desired_type < 4 || desired_type >= 0x31) {
    display_assert("hs_type_valid(desired_type)",
                   "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x5a5, true);
    system_exit(-1);
  }

  if (actual_type == 3 || actual_type == desired_type)
    return true;

  if (desired_type >= 0x25 && desired_type <= 0x2a) {
    int16_t d_off = desired_type - 0x25;
    if (actual_type >= 0x25 && actual_type <= 0x2a)
      return hs_object_types_compatible((int16_t)(actual_type - 0x25), d_off);
    if (actual_type >= 0x2b && actual_type <= 0x30)
      return hs_object_types_compatible((int16_t)(actual_type - 0x2b), d_off);
    return false;
  }

  if (desired_type >= 0x2b && desired_type <= 0x30) {
    if (actual_type < 0x2b || actual_type > 0x30)
      return false;
    return hs_object_types_compatible((int16_t)(actual_type - 0x2b),
                                      (int16_t)(desired_type - 0x2b));
  }

  return *(int *)((char *)0x2f3ec0 +
                  ((int)desired_type * 0x31 + (int)actual_type) * 4) != 0;
}

/* 0xcb170 — Cast an HS value from actual_type to desired_type, returning the
 * converted value. Uses a function dispatch table at 0x2f3ec0 indexed as
 * [desired_type * 0x31 + actual_type] for most type pairs. Object handle
 * types (0x2b..0x30) to object reference types (0x25..0x2a) are handled by
 * object_name_list_get_handle which converts a handle index to a datum-based
 * reference. Passthrough (actual==3) and identity casts return value unchanged.
 *
 * Assert string confirms name: "hs_can_cast(actual_type, desired_type)"
 * at source line 0x5d8 (c:\halo\SOURCE\hs\hs_runtime.c).
 */
static int hs_can_cast(int thread_handle, int16_t actual_type,
                       int16_t desired_type, int value)
{
  char *script_name;
  char *msg;
  int (*cast_fn)(int);

  if (!hs_types_compatible(actual_type, desired_type)) {
    script_name = hs_get_thread_script_name(thread_handle);
    msg = csprintf((char *)0x5ab100,
                   "a problem occurred while executing the script %s: %s (%s)",
                   script_name, "bad typecast.",
                   "hs_can_cast(actual_type, desired_type)");
    display_assert(msg, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x5d8, true);
    system_exit(-1);
  }

  if (actual_type == desired_type || actual_type == 3)
    return value;

  if (desired_type >= 0x2b && desired_type <= 0x30)
    return value;

  if (desired_type >= 0x25 && desired_type <= 0x2a) {
    if (actual_type >= 0x2b && actual_type <= 0x30)
      return object_name_list_get_handle((int16_t)value);
    return value;
  }

  cast_fn = *(int (**)(int))((char *)0x2f3ec0 +
                             ((int)desired_type * 0x31 + (int)actual_type) * 4);
  return cast_fn(value);
}

/* 0xcb230 — Copy an external global's live C value into the HS globals datum
 * pool, type-dispatched. Only processes external globals (bit 15 set in
 * handle). Callees: datum_get, hs_external_global_get (0xc3e10),
 * hs_global_get_type (0xc3e60). ext_ptr+0x8 is the backing pointer to the live
 * C variable; NULL means use static default from the data segment.
 */
static void FUN_000cb230(int loop_var)
{
  char *datum_ptr;
  char *ext_ptr;
  int16_t type;

  if ((loop_var & 0x8000) == 0)
    return;

  datum_ptr = (char *)datum_get(*(data_t **)0x5aa6c0, loop_var & 0x7fff);
  ext_ptr = (char *)hs_external_global_get((int16_t)(loop_var & 0x7fff));
  type = hs_global_get_type((uint16_t)loop_var);

  switch (type) {
  case 5:
    if (*(uint8_t **)(ext_ptr + 8) == NULL) {
      *(uint8_t *)(datum_ptr + 4) = *(uint8_t *)0x26f3b2;
    } else {
      *(uint8_t *)(datum_ptr + 4) = **(uint8_t **)(ext_ptr + 8);
    }
    return;
  case 6:
    if (*(float **)(ext_ptr + 8) == NULL) {
      *(float *)(datum_ptr + 4) = *(float *)0x26f3b4;
    } else {
      *(float *)(datum_ptr + 4) = **(float **)(ext_ptr + 8);
    }
    return;
  case 7:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3b8;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 8:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f3bc;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 9:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x2f1580;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 10:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3c0;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0xb:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3c4;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0xc:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3c8;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0xd:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3cc;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0xe:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3d0;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0xf:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3d4;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0x10:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3d8;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0x11:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f3dc;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x12:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3e0;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0x13:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3e4;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0x14:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3e8;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0x15:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3ec;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0x16:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f3f0;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0x17:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f3f4;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x18:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f3f8;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x19:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f400;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x1a:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f404;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x1b:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f3fc;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x1c:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f408;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x1d:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f40c;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x1e:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f410;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x1f:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f414;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x20:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f418;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0x21:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f41c;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0x22:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f420;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0x23:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f424;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0x24:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f428;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  case 0x25:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f430;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x26:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f434;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x27:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f438;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x28:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f43c;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x29:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f440;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x2a:
    if (*(int32_t **)(ext_ptr + 8) == NULL) {
      *(int32_t *)(datum_ptr + 4) = *(int32_t *)0x26f444;
    } else {
      *(int32_t *)(datum_ptr + 4) = **(int32_t **)(ext_ptr + 8);
    }
    return;
  case 0x2b:
    if (*(int16_t **)(ext_ptr + 8) == NULL) {
      *(int16_t *)(datum_ptr + 4) = *(int16_t *)0x26f42c;
    } else {
      *(int16_t *)(datum_ptr + 4) = **(int16_t **)(ext_ptr + 8);
    }
    return;
  default:
    display_assert(NULL, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x638, true);
    system_exit(-1);
    return;
  }
}

/* 0xcb7b0 — Write HS datum values back to external C globals, type-dispatched.
 * Reverse of FUN_000cb230: datum_ptr+4 → *ext_ptr+8. Only writes if the
 * backing pointer (ext_ptr+8) is non-NULL.
 */
static void FUN_000cb7b0(int loop_var)
{
  char *datum_ptr;
  char *ext_ptr;
  int16_t type;

  if ((loop_var & 0x8000) == 0)
    return;

  datum_ptr = (char *)datum_get(*(data_t **)0x5aa6c0, loop_var & 0x7fff);
  ext_ptr = (char *)hs_external_global_get((int16_t)(loop_var & 0x7fff));
  type = hs_global_get_type((uint16_t)loop_var);

  switch (type) {
  case 5:
    if (*(uint8_t **)(ext_ptr + 8) != NULL) {
      **(uint8_t **)(ext_ptr + 8) = *(uint8_t *)(datum_ptr + 4);
    }
    return;
  case 6:
    if (*(float **)(ext_ptr + 8) != NULL) {
      **(float **)(ext_ptr + 8) = *(float *)(datum_ptr + 4);
    }
    return;
  case 7:
  case 10:
  case 0xd:
  case 0x10:
  case 0x13:
  case 0x16:
  case 0x22:
  case 0x2b:
    if (*(int16_t **)(ext_ptr + 8) != NULL) {
      **(int16_t **)(ext_ptr + 8) = *(int16_t *)(datum_ptr + 4);
    }
    return;
  case 8:
  case 0x11:
  case 0x17:
  case 0x1a:
  case 0x1d:
  case 0x26:
  case 0x29:
    if (*(int32_t **)(ext_ptr + 8) != NULL) {
      **(int32_t **)(ext_ptr + 8) = *(int32_t *)(datum_ptr + 4);
    }
    return;
  case 9:
  case 0x18:
  case 0x1b:
  case 0x1e:
  case 0x27:
  case 0x2a:
    if (*(int32_t **)(ext_ptr + 8) != NULL) {
      **(int32_t **)(ext_ptr + 8) = *(int32_t *)(datum_ptr + 4);
    }
    return;
  case 0xb:
  case 0xe:
  case 0x14:
  case 0x20:
  case 0x23:
    if (*(int16_t **)(ext_ptr + 8) != NULL) {
      **(int16_t **)(ext_ptr + 8) = *(int16_t *)(datum_ptr + 4);
    }
    return;
  case 0xc:
  case 0xf:
  case 0x12:
  case 0x15:
  case 0x21:
  case 0x24:
    if (*(int16_t **)(ext_ptr + 8) != NULL) {
      **(int16_t **)(ext_ptr + 8) = *(int16_t *)(datum_ptr + 4);
    }
    return;
  case 0x19:
  case 0x1c:
  case 0x1f:
  case 0x25:
  case 0x28:
    if (*(int32_t **)(ext_ptr + 8) != NULL) {
      **(int32_t **)(ext_ptr + 8) = *(int32_t *)(datum_ptr + 4);
    }
    return;
  default:
    display_assert(NULL, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x671, true);
    system_exit(-1);
    return;
  }
}

/* 0xcbf80 — Execute a pending script-call expression on an HS thread.
 * Resolves the return type of the callee (either a built-in function or a
 * scenario script), casts the supplied value to that type via hs_can_cast,
 * writes the result into the current stack frame's dest slot, then pops the
 * top stack frame (advances thread->stack_ptr to the previous frame).
 *
 * Asserts valid_thread(thread) — checks that the thread pointer lies within
 * the thread-data array bounds and that its stack pointer is within the
 * per-thread stack window [thread+0x18, thread+0x218).
 *
 * Node layout (hs_syntax datum, EBX):
 *   +0x2 (int16_t) : function/script index (or global index when reparse set)
 *   +0x4 (int16_t) : desired return type (cast target)
 *   +0x6 (uint8_t) : flags; bit 1 (0x2) = script-reference (vs. built-in)
 *
 * Stack frame layout (top frame ptr, *(*(thread+0x10))):
 *   +0x8 (int32_t*): pointer to the destination value slot
 *
 * Scenario script element (offset 0x49c into scenario, stride 0x5c):
 *   +0x22 (int16_t): script return type
 *
 * Key globals:
 *   0x5aa6c4 = hs_thread_data  (data_t*)
 *   0x5aa6c8 = hs_syntax_data  (data_t*)
 *   0x5ab100 = scratch string buffer (for assert message)
 */
static void FUN_000cbf80(int thread_handle, int value)
{
  /* Resolve thread and current syntax node. */
  char *thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_handle);
  char *stack_ptr = *(char **)(thread + 0x10);
  int node_handle = *(int *)(stack_ptr + 0x4);
  char *node = (char *)datum_get(*(data_t **)0x5aa6c8, node_handle);

  /* valid_thread(thread) — assert the thread and its stack are sane. */
  {
    data_t *td = *(data_t **)0x5aa6c4;
    char *data_base = *(char **)(((char *)td) + 0x34);
    int16_t stride = *(int16_t *)(((char *)td) + 0x2e);
    int16_t count = *(int16_t *)(((char *)td) + 0x22);
    char *data_end = data_base + (int)stride * (int)count;
    char *sp = *(char **)(thread + 0x10);
    char *frame_end = sp + 0xe + (int)*(int16_t *)(sp + 0xc);
    if (thread < data_base || thread >= data_end || sp < thread + 0x18 ||
        sp >= thread + 0x218 || frame_end > thread + 0x218) {
      const char *script_name = hs_get_thread_script_name(thread_handle);
      const char *msg =
        csprintf((char *)0x5ab100,
                 "a problem occurred while executing the script %s: %s (%s)",
                 script_name, "valid_thread(thread)", "corrupted stack.");
      display_assert(msg, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x325, true);
      system_exit(-1);
    }
  }

  /* Resolve the actual return type of the callee. */
  int16_t actual_type;
  if (*(uint8_t *)(node + 0x6) & 0x2) {
    /* Script reference: look up the scenario script element. */
    int script_index = (int)*(int16_t *)(node + 0x2);
    char *scenario = (char *)global_scenario_get();
    char *script_elem =
      (char *)tag_block_get_element(scenario + 0x49c, script_index, 0x5c);
    actual_type = *(int16_t *)(script_elem + 0x22);
  } else {
    /* Built-in function: look up its return type from the function table. */
    int16_t func_index = (int16_t) * (uint16_t *)(node + 0x2);
    char *func_entry = (char *)hs_function_table_get(func_index);
    actual_type = *(int16_t *)func_entry;
  }

  /* Cast value to the desired type and store into the current frame's dest. */
  int16_t desired_type = (int16_t) * (uint16_t *)(node + 0x4);
  int result = hs_can_cast(thread_handle, actual_type, desired_type, value);
  char *top_frame = *(char **)(*(char **)(thread + 0x10));
  *(int32_t *)(*(int32_t **)(top_frame + 0x8)) = result;

  /* Pop the top stack frame: advance thread->stack_ptr to previous frame. */
  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_handle);
  char *cur_sp = *(char **)(thread + 0x10);
  *(char **)(thread + 0x10) = *(char **)cur_sp;
}

/* 0xcc0a0 — Resolve an HS global reference to its current value. Syncs
 * external globals via FUN_000cb230, then indexes into hs_globals_data.
 * External globals (bit 0x8000 set) index directly; scenario globals
 * add hs_globals_start_index (0x27d504) as a base offset.
 */
int FUN_000cc0a0(int16_t global_ref)
{
  int index;
  char *datum_ptr;

  FUN_000cb230((int)global_ref);
  if (global_ref & 0x8000) {
    index = global_ref & 0x7fff;
  } else {
    index = (global_ref & 0x7fff) + (int)*(int16_t *)0x27d504;
  }
  datum_ptr = (char *)datum_get(*(data_t **)0x5aa6c0, index);
  return *(int *)(datum_ptr + 4);
}

/* 0xcc1d0 — Evaluate an HS expression and store the result at dest_ptr.
 * If the expression is a constant, evaluates immediately via hs_can_cast.
 * If the expression is a global reference (reparse bit), resolves the global
 * first via FUN_000cc0a0 and hs_global_get_type before evaluating.
 * If the expression is non-constant, sets up the thread stack frame for
 * deferred evaluation: stores dest_ptr and expression_index in the stack
 * frame, pushes a new frame via hs_thread_push_frame, and sets the evaluation
 * flag.
 *
 * Validates thread integrity (stack bounds) and asserts dest_ptr != NULL.
 */
static void FUN_000cc1d0(int thread_handle, int expression_index,
                         void *dest_ptr)
{
  char *thread;
  char *expr;
  char *expr2;
  char *stack_ptr;
  data_t *thread_data;

  thread_data = *(data_t **)0x5aa6c4;
  thread = (char *)datum_get(thread_data, thread_handle);
  expr = (char *)datum_get(*(data_t **)0x5aa6c8, expression_index);

  /* valid_thread(thread) check — verify stack pointer is within bounds */
  {
    uint32_t pool_base = *(uint32_t *)((char *)thread_data + 0x34);
    int16_t datum_count = *(int16_t *)((char *)thread_data + 0x2e);
    int16_t datum_size = *(int16_t *)((char *)thread_data + 0x22);
    uint32_t pool_end = pool_base + (int)datum_count * (int)datum_size;
    uint32_t thr = (uint32_t)thread;
    uint32_t sp = *(uint32_t *)(thread + 0x10);
    uint32_t stack_base = thr + 0x18;
    uint32_t stack_end = thr + 0x218;

    if (thr < pool_base || thr >= pool_end || sp < stack_base ||
        sp >= stack_end || sp + (int)*(int16_t *)(sp + 0xc) + 0xe > stack_end) {
      char *script_name = hs_get_thread_script_name(thread_handle);
      char *msg =
        csprintf((char *)0x5ab100,
                 "a problem occurred while executing the script %s: %s (%s)",
                 script_name, "corrupted stack.", "valid_thread(thread)");
      display_assert(msg, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x2ff, true);
      system_exit(-1);
    }
  }

  if (dest_ptr == NULL) {
    display_assert("destination", "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x300,
                   true);
    system_exit(-1);
  }

  expr2 = (char *)datum_get(*(data_t **)0x5aa6c8, expression_index);

  /* Constant expression — evaluate immediately */
  if (*(uint8_t *)(expr2 + 0x6) & 1) {
    if (*(uint8_t *)(expr + 0x6) & 4) {
      /* Global reference (reparse bit): resolve via external global */
      int resolved = FUN_000cc0a0(*(int16_t *)(expr + 0x10));
      int16_t type = hs_global_get_type((uint16_t) * (int16_t *)(expr + 0x10));
      *(int *)dest_ptr =
        hs_can_cast(thread_handle, (int)type,
                    (int)(uint16_t) * (int16_t *)(expr + 0x4), resolved);
    } else {
      *(int *)dest_ptr = hs_can_cast(
        thread_handle, (int)(uint16_t) * (int16_t *)(expr + 0x2),
        (int)(uint16_t) * (int16_t *)(expr + 0x4), *(int *)(expr + 0x10));
    }
    return;
  }

  /* Non-constant expression — set up stack frame for deferred evaluation */
  stack_ptr = *(char **)(thread + 0x10);
  *(void **)(stack_ptr + 0x8) = dest_ptr;
  hs_thread_push_frame(thread_handle);
  *(uint8_t *)(thread + 0x3) |= 1;
  *(int *)(*(char **)(thread + 0x10) + 0x4) = expression_index;
}

/* 0xcc340 — Evaluate a script-reference call. Gets the script element from
 * the scenario scripts block (scenario+0x49c), allocates 4 bytes on the
 * thread stack, then either evaluates the script's expression tree (init)
 * or pops the frame with the stored result. */
void FUN_000cc340(int16_t script_index, int thread_handle, char init)
{
  char *script;
  void *result;

  script = (char *)tag_block_get_element((char *)global_scenario_get() + 0x49c,
                                         (int)script_index, 0x5c);
  datum_get(*(data_t **)0x5aa6c4, thread_handle);
  result = hs_thread_stack_alloc(thread_handle, 4);

  if (init) {
    FUN_000cc1d0(thread_handle, *(int *)(script + 0x24), result);
  } else {
    FUN_000cbf80(thread_handle, *(int *)result);
  }
}

/* 0xcc3a0 — Evaluate function arguments. Allocates a values array on the
 * thread stack, then evaluates each argument expression one-per-call into
 * the array, type-checking against the formal parameter list. Returns the
 * values array pointer when all arguments are evaluated, or 0 if still
 * processing. */
int FUN_000cc3a0(int thread_datum, int16_t param_count, int formal_params,
                 char init)
{
  char *thread;
  int *values;
  int16_t *arg_index;
  int *expr_ptr;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  values = (int *)hs_thread_stack_alloc(thread_datum, (int)param_count * 4);
  arg_index = (int16_t *)hs_thread_stack_alloc(thread_datum, 2);
  expr_ptr = (int *)hs_thread_stack_alloc(thread_datum, 4);

  if (init) {
    *arg_index = 0;
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(char **)(thread + 0x10) + 0x4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    *expr_ptr = *(int *)(child + 0x8);
  }

  if (*arg_index >= param_count) {
    if (*expr_ptr != -1) {
      char *name = hs_get_thread_script_name(thread_datum);
      char *msg =
        csprintf((char *)0x5ab100,
                 "a problem occurred while executing the script %s: %s (%s)",
                 name, "corrupted syntax tree.", "*expression_index==NONE");
      display_assert(msg, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x3d1, 1);
      system_exit(-1);
    }
    return (int)values;
  }

  if (*expr_ptr == -1) {
    char *name = hs_get_thread_script_name(thread_datum);
    char *msg =
      csprintf((char *)0x5ab100,
               "a problem occurred while executing the script %s: %s (%s)",
               name, "corrupted syntax tree.", "*expression_index!=NONE");
    display_assert(msg, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x3c4, 1);
    system_exit(-1);
  }

  {
    char *expr = (char *)datum_get(*(data_t **)0x5aa6c8, *expr_ptr);
    if (*(int16_t *)(expr + 0x4) !=
        *(int16_t *)(formal_params + (int)*arg_index * 2)) {
      datum_get(*(data_t **)0x5aa6c4, thread_datum);
      char *name = hs_get_thread_script_name(thread_datum);
      error(2, "script %s needs to be recompiled. (%s: %s)", name,
            "unexpected actual parameters.",
            "hs_syntax_get(*expression_index)->type=="
            "formal_parameters[*argument_index]");
      return (int)values;
    }
  }

  FUN_000cc1d0(thread_datum, *expr_ptr, &values[(int)*arg_index]);
  {
    char *expr = (char *)datum_get(*(data_t **)0x5aa6c8, *expr_ptr);
    *expr_ptr = *(int *)(expr + 0x8);
  }
  (*arg_index)++;
  return 0;
}

/* 0xcc560 — Evaluate an HS built-in function call by dispatching to
 * FUN_000cc3a0 with the function's formal parameter count and types
 * from the function descriptor table. */
void FUN_000cc560(int16_t function_index, int thread_datum, char init)
{
  char *desc = (char *)hs_function_table_get(function_index);
  FUN_000cc3a0(thread_datum, *(int16_t *)(desc + 0x18), (int)(desc + 0x1a),
               init);
}

/* 0xcc590 — HS 'begin' evaluator. Evaluates a sequence of expressions in
 * order, returning the value of the last one. On init, sets up the expression
 * list pointer (skipping the function-name child). Each call evaluates one
 * expression and advances to the next sibling. */
void FUN_000cc590(int16_t function_index, int thread_datum, char init)
{
  char *thread;
  int *expr_ptr;
  int *result_ptr;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  expr_ptr = (int *)hs_thread_stack_alloc(thread_datum, 4);
  result_ptr = (int *)hs_thread_stack_alloc(thread_datum, 4);

  if (function_index != 0) {
    display_assert("function_index==_hs_function_begin",
                   "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 0x15,
                   1);
    system_exit(-1);
  }

  if (init) {
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(char **)(thread + 0x10) + 0x4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    *expr_ptr = *(int *)(child + 0x8);
    *result_ptr = 0;
  }

  if (*expr_ptr != -1) {
    FUN_000cc1d0(thread_datum, *expr_ptr, result_ptr);
    char *expr = (char *)datum_get(*(data_t **)0x5aa6c8, *expr_ptr);
    *expr_ptr = *(int *)(expr + 0x8);
    return;
  }

  FUN_000cbf80(thread_datum, *result_ptr);
}

/* 0xcc660 — HS 'begin_random' evaluator.
 * Implements (begin_random <arg0> <arg1> ... <argN-1>): on each call selects
 * one not-yet-evaluated argument at random and evaluates it.  When all
 * arguments have been evaluated it pops the frame with the last result.
 *
 * Multi-phase protocol:
 *   init==true  : count the argument list, memset the used-bit array.
 *   init==false : pick the next unused slot and evaluate it; when all slots
 *                 are used call FUN_000cbf80 to commit the result.
 *
 * Stack allocations (via hs_thread_stack_alloc):
 *   2 bytes  — int16_t argument_count
 *   4 bytes  — uint32_t used_bits[]  (one bit per argument, up to 32)
 *   4 bytes  — int       result_value
 *
 * Random selection: random_range(get_global_random_seed_address(), 0,
 *   argument_count) gives a starting offset sVar2; then we try
 *   (i + sVar2) % argument_count for i = 0, 1, ... until we find an
 *   unset bit.
 *
 * Assert: function_index must equal 1 (_hs_function_begin_random).
 * Assert: argument_count must be < 32 (LONG_BITS).
 *
 * Globals:
 *   0x5aa6c4 = hs_thread_data  (data_t*)
 *   0x5aa6c8 = hs_syntax_data  (data_t*)
 */
void FUN_000cc660(int16_t function_index, int thread_datum, char init)
{
  char *thread;
  int16_t *argument_count;
  int *used_bits;
  int *result_value;
  int16_t sVar2;
  int16_t sVar10;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  argument_count = (int16_t *)hs_thread_stack_alloc(thread_datum, 2);
  used_bits = (int *)hs_thread_stack_alloc(thread_datum, 4);
  result_value = (int *)hs_thread_stack_alloc(thread_datum, 4);

  if (function_index != 1) {
    display_assert("function_index==_hs_function_begin_random",
                   "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 0x45,
                   1);
    system_exit(-1);
  }

  if (init) {
    /* Walk the argument list to count arguments. */
    char *frame = *(char **)(thread + 0x10);
    char *fn_node =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(frame + 0x4));
    char *first_child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(fn_node + 0x10));
    int arg_datum = *(int *)(first_child + 0x8);

    *argument_count = 0;
    if (arg_datum != -1) {
      do {
        char *arg = (char *)datum_get(*(data_t **)0x5aa6c8, arg_datum);
        arg_datum = *(int *)(arg + 0x8);
        *argument_count = *argument_count + 1;
      } while (arg_datum != -1);

      if (*argument_count >= 0x20) {
        display_assert("*argument_count<LONG_BITS",
                       "c:\\halo\\source\\hs\\hs_library_internal_runtime.h",
                       0x50, 1);
        system_exit(-1);
      }
    }

    csmemset(used_bits, 0, (int)((*argument_count + 0x1f) >> 5) << 2);
  }

  /* Pick a random starting offset in [0, argument_count). */
  sVar2 = random_range((unsigned int *)get_global_random_seed_address(), 0,
                       *argument_count);

  sVar10 = 0;
  if (sVar10 < *argument_count) {
    do {
      /* Compute candidate slot: (sVar10 + sVar2) % argument_count. */
      int16_t sVar11 =
        (int16_t)(((int)sVar10 + (int)sVar2) % (int)*argument_count);

      if ((used_bits[(int)sVar11 >> 5] & (1 << ((int)sVar11 & 0x1f))) == 0) {
        /* Slot not yet used: walk to the sVar11-th argument. */
        char *frame2 = *(char **)(thread + 0x10);
        char *fn_node2 =
          (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(frame2 + 0x4));
        char *first_child2 =
          (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(fn_node2 + 0x10));
        int cur_datum = *(int *)(first_child2 + 0x8);

        if (sVar11 > 0) {
          int walk = (int)(uint16_t)sVar11;
          do {
            char *node = (char *)datum_get(*(data_t **)0x5aa6c8, cur_datum);
            cur_datum = *(int *)(node + 0x8);
            walk--;
          } while (walk != 0);
        }

        /* Evaluate the chosen argument. */
        FUN_000cc1d0(thread_datum, cur_datum, result_value);

        /* Mark the slot as used. */
        used_bits[(int)sVar11 >> 5] |= 1 << ((int)sVar11 & 0x1f);
        break;
      }

      sVar10++;
    } while (sVar10 < *argument_count);
  }

  /* If all slots have been tried (counter wrapped to argument_count), pop
   * the frame and commit the result. */
  if (sVar10 == *argument_count) {
    FUN_000cbf80(thread_datum, *result_value);
  }
}

/* 0xcc870 — HS 'if' evaluator. Three-phase: init evaluates the condition,
 * second call selects then/else branch, third call pops frame with result.
 * (if <condition> <then> [else]) */
void FUN_000cc870(int16_t function_index, int thread_datum, char init)
{
  char *thread;
  char *cond_result;
  int *branch_ptr;
  int *value_ptr;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  cond_result = (char *)hs_thread_stack_alloc(thread_datum, 4);
  branch_ptr = (int *)hs_thread_stack_alloc(thread_datum, 4);
  value_ptr = (int *)hs_thread_stack_alloc(thread_datum, 4);

  if (function_index != 2) {
    display_assert("function_index==_hs_function_if",
                   "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 0x77,
                   1);
    system_exit(-1);
  }

  if (init) {
    *(int *)cond_result = 0;
    *branch_ptr = -1;
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(char **)(thread + 0x10) + 0x4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    FUN_000cc1d0(thread_datum, *(int *)(child + 0x8), cond_result);
    return;
  }

  if (*branch_ptr != -1) {
    FUN_000cbf80(thread_datum, *value_ptr);
    return;
  }

  {
    int frame_expr = *(int *)(*(char **)(thread + 0x10) + 0x4);
    char *fn_name = (char *)datum_get(
      *(data_t **)0x5aa6c8,
      *(int *)((char *)datum_get(*(data_t **)0x5aa6c8, frame_expr) + 0x10));

    if (*cond_result) {
      char *cond =
        (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(fn_name + 0x8));
      *branch_ptr = *(int *)(cond + 0x8);
    } else {
      char *cond =
        (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(fn_name + 0x8));
      char *then_node =
        (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(cond + 0x8));
      *branch_ptr = *(int *)(then_node + 0x8);
      if (*branch_ptr == -1) {
        FUN_000cbf80(thread_datum, 0);
        return;
      }
    }

    FUN_000cc1d0(thread_datum, *branch_ptr, value_ptr);
  }
}

/* 0xcca00 — HS 'set' evaluator. Assigns a value to a global variable.
 * Init: evaluates the value expression, storing result at the global's address.
 * Not init: syncs globals, optionally handles object-list type (0x17), pops
 * frame with the global's current value. */
void FUN_000cca00(int16_t function_index, int thread_datum, char init)
{
  char *thread;
  char *var_node;
  int var_node_idx;
  int16_t global_type;
  int global_index;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  {
    char *frame_expr = (char *)datum_get(
      *(data_t **)0x5aa6c8, *(int *)(*(char **)(thread + 0x10) + 0x4));
    char *fn_child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(frame_expr + 0x10));
    var_node_idx = *(int *)(fn_child + 0x8);
  }
  var_node = (char *)datum_get(*(data_t **)0x5aa6c8, var_node_idx);
  hs_thread_stack_alloc(thread_datum, 4);
  global_type = hs_global_get_type((uint16_t) * (int16_t *)(var_node + 0x10));

  if (init) {
    if (global_type == 0x17)
      FUN_000ce370(FUN_000cc0a0(*(int16_t *)(var_node + 0x10)));

    global_index = (int)*(int16_t *)(var_node + 0x10) & 0x7fff;
    if (!((uint8_t)(*((uint8_t *)(var_node + 0x10) + 1)) & 0x80))
      global_index += (int)*(int16_t *)0x27d504;

    {
      char *global_datum =
        (char *)datum_get(*(data_t **)0x5aa6c0, global_index);
      char *value_expr = (char *)datum_get(*(data_t **)0x5aa6c8, var_node_idx);
      FUN_000cc1d0(thread_datum, *(int *)(value_expr + 0x8), global_datum + 4);
    }
    return;
  }

  FUN_000cb7b0(*(int16_t *)(var_node + 0x10));
  if (global_type == 0x17)
    FUN_000ce350(FUN_000cc0a0(*(int16_t *)(var_node + 0x10)));

  FUN_000cb230(*(int16_t *)(var_node + 0x10));
  {
    int ref = (int)*(int16_t *)(var_node + 0x10);
    if (ref & 0x8000)
      global_index = ref & 0x7fff;
    else
      global_index = (ref & 0x7fff) + (int)*(int16_t *)0x27d504;
  }

  {
    char *global_datum = (char *)datum_get(*(data_t **)0x5aa6c0, global_index);
    FUN_000cbf80(thread_datum, *(int *)(global_datum + 4));
  }
}

/* 0xccb40 — HS 'and'/'or' evaluator. Short-circuits: AND stops on first
 * false, OR stops on first true. function_index 5 = and, 6 = or. */
void FUN_000ccb40(int16_t function_index, int thread_datum, char init)
{
  char *thread;
  int *expr_ptr;
  char *result_ptr;
  char *running;
  char is_and;
  char new_val;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  expr_ptr = (int *)hs_thread_stack_alloc(thread_datum, 4);
  result_ptr = (char *)hs_thread_stack_alloc(thread_datum, 4);
  running = (char *)hs_thread_stack_alloc(thread_datum, 1);

  is_and = (char)(function_index == 5);

  if (function_index != 5 && function_index != 6) {
    display_assert(
      "function_index==_hs_function_and || function_index==_hs_function_or",
      "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 0xcf, 1);
    system_exit(-1);
  }

  if (init) {
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(char **)(thread + 0x10) + 0x4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    *expr_ptr = *(int *)(child + 0x8);
    *running = is_and;
  } else {
    if (is_and)
      new_val = (*running && *result_ptr) ? 1 : 0;
    else
      new_val = (*running || *result_ptr) ? 1 : 0;
    *running = new_val;
  }

  if (*expr_ptr != -1 && *running == is_and) {
    FUN_000cc1d0(thread_datum, *expr_ptr, result_ptr);
    {
      char *expr = (char *)datum_get(*(data_t **)0x5aa6c8, *expr_ptr);
      *expr_ptr = *(int *)(expr + 0x8);
    }
    return;
  }

  FUN_000cbf80(thread_datum, (int)(uint8_t)*running);
}

/* 0xccc70 — HS arithmetic evaluator (+, -, *, /, min, max). Accumulates
 * results across multiple operand expressions. Function indices 7-12. */
void FUN_000ccc70(int16_t function_index, int thread_datum, char init)
{
  char *thread;
  int16_t *counter;
  int *expr_ptr;
  float *operand;
  float *accum;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  counter = (int16_t *)hs_thread_stack_alloc(thread_datum, 2);
  expr_ptr = (int *)hs_thread_stack_alloc(thread_datum, 4);
  operand = (float *)hs_thread_stack_alloc(thread_datum, 4);
  accum = (float *)hs_thread_stack_alloc(thread_datum, 4);

  if (init) {
    *counter = 0;
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(char **)(thread + 0x10) + 0x4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    *expr_ptr = *(int *)(child + 0x8);
  } else {
    if (*counter == 0) {
      *accum = *operand;
    } else {
      switch (function_index) {
      case 7:
        *accum = *operand + *accum;
        break;
      case 8:
        *accum = *accum - *operand;
        break;
      case 9:
        *accum = *operand * *accum;
        break;
      case 10:
        *accum = *accum / *operand;
        break;
      case 0xb:
        if (*operand < *accum)
          *accum = *operand;
        break;
      case 0xc:
        if (*operand > *accum)
          *accum = *operand;
        break;
      default:
        display_assert(0, "c:\\halo\\source\\hs\\hs_library_internal_runtime.h",
                       0x111, 1);
        system_exit(-1);
        break;
      }
    }
    (*counter)++;
  }

  if (*expr_ptr != -1) {
    FUN_000cc1d0(thread_datum, *expr_ptr, operand);
    {
      char *expr = (char *)datum_get(*(data_t **)0x5aa6c8, *expr_ptr);
      *expr_ptr = *(int *)(expr + 0x8);
    }
    return;
  }

  FUN_000cbf80(thread_datum, *(int *)accum);
}

/* 0xccdf0 — HS equal/not-equal evaluator. Evaluates two arguments of the
 * same type via FUN_000cc3a0, then compares with csmemcmp using the type's
 * size from the table at 0x26f350. function_index 0xd = equal, 0xe = not_equal.
 */
void FUN_000ccdf0(int16_t function_index, int thread_datum, char init)
{
  int16_t type;
  int16_t param_types[2];
  int *values;

  if (function_index != 0xd && function_index != 0xe) {
    display_assert("function_index==_hs_function_equal || "
                   "function_index==_hs_function_not_equal",
                   "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 0x131,
                   1);
    system_exit(-1);
  }

  {
    char *thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(char **)(thread + 0x10) + 0x4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    char *arg1 = (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(child + 0x8));
    type = *(int16_t *)(arg1 + 0x4);
  }

  param_types[0] = type;
  param_types[1] = type;
  values = (int *)FUN_000cc3a0(thread_datum, 2, (int)param_types, init);
  if (values != 0) {
    int size = (int)*(int16_t *)(0x26f350 + (int)type * 2);
    char result = (csmemcmp(values, values + 1, size) == 0) ? 1 : 0;
    if (function_index == 0xe)
      result = (result == 0) ? 1 : 0;
    FUN_000cbf80(thread_datum, (int)(uint8_t)result);
  }
}

/* 0xcced0 — HS comparison evaluator (gt/lt/ge/lte). Evaluates two arguments
 * of matching numeric type via FUN_000cc3a0 using the static param_types pair
 * at 0x46b80c/0x46b80e, then performs FPU comparison. Handles three type
 * classes: real (type==6, FLD float), long_integer (type==8, FILD dword),
 * and short_integer/enum (type==7 or 0x20..0x24, MOVSX word then FILD).
 * function_index 0xf=gt, 0x10=lt, 0x11=ge, 0x12=lte.
 *
 * The formal_params passed to FUN_000cc3a0 is a static int16_t[2] at
 * 0x0046b80c; both slots are filled with the argument's inferred type.
 * Result is committed via FUN_000cbf80(thread_datum, (int)(uint8_t)result).
 */
void FUN_000cced0(int16_t function_index, int thread_datum, char init)
{
  int16_t type;
  /* static param_types pair: [0x0046b80c] = type, [0x0046b80e] = type */
  int16_t *param_types = (int16_t *)0x0046b80c;
  int *values;
  char result;

  if (function_index < 0xf || function_index > 0x12) {
    display_assert(
      "function_index>=_hs_function_gt && function_index<=_hs_function_lte",
      "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 0x15d, 1);
    system_exit(-1);
  }

  {
    char *thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(char **)(thread + 0x10) + 0x4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    char *arg1 = (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(child + 0x8));
    type = *(int16_t *)(arg1 + 0x4);
  }

  param_types[0] = type;
  param_types[1] = type;
  values = (int *)FUN_000cc3a0(thread_datum, 2, (int)param_types, init);
  if (values == NULL)
    return;

  if (type == 6) {
    /* real: load as float directly */
    float a = *(float *)values;
    float b = ((float *)values)[1];
    switch (function_index) {
    case 0xf:
      result = (a > b) ? 1 : 0;
      break; /* gt */
    case 0x10:
      result = (a < b) ? 1 : 0;
      break; /* lt */
    case 0x11:
      result = (a >= b) ? 1 : 0;
      break; /* ge */
    case 0x12:
      result = (a <= b) ? 1 : 0;
      break; /* lte */
    default:
      display_assert(0, "c:\\halo\\source\\hs\\hs_library_internal_runtime.h",
                     0x16b, 1);
      system_exit(-1);
      result = 0;
      break;
    }
  } else if (type == 8) {
    /* long_integer: load as int32 → float for comparison */
    float a = (float)*(int32_t *)values;
    float b = (float)*((int32_t *)values + 1);
    switch (function_index) {
    case 0xf:
      result = (a > b) ? 1 : 0;
      break; /* gt */
    case 0x10:
      result = (a < b) ? 1 : 0;
      break; /* lt */
    case 0x11:
      result = (a >= b) ? 1 : 0;
      break; /* ge */
    case 0x12:
      result = (a <= b) ? 1 : 0;
      break; /* lte */
    default:
      display_assert(0, "c:\\halo\\source\\hs\\hs_library_internal_runtime.h",
                     0x16e, 1);
      system_exit(-1);
      result = 0;
      break;
    }
  } else {
    /* short_integer or enum (type==7 or 0x20..0x24): load as int16 → float */
    if (type != 7 && (type < 0x20 || type > 0x24)) {
      display_assert("parameter_types[0]==_hs_type_short_integer || "
                     "HS_TYPE_IS_ENUM(parameter_types[0])",
                     "c:\\halo\\source\\hs\\hs_library_internal_runtime.h",
                     0x171, 1);
      system_exit(-1);
    }
    float a = (float)(int)*(int16_t *)values;
    float b = (float)(int)*((int16_t *)values + 1);
    switch (function_index) {
    case 0xf:
      result = (a > b) ? 1 : 0;
      break; /* gt */
    case 0x10:
      result = (a < b) ? 1 : 0;
      break; /* lt */
    case 0x11:
      result = (a >= b) ? 1 : 0;
      break; /* ge */
    case 0x12:
      result = (a <= b) ? 1 : 0;
      break; /* lte */
    default:
      display_assert(0, "c:\\halo\\source\\hs\\hs_library_internal_runtime.h",
                     0x172, 1);
      system_exit(-1);
      result = 0;
      break;
    }
  }

  FUN_000cbf80(thread_datum, (int)(uint8_t)result);
}

/* 0xcd0e0 — HS 'sleep' evaluator. Implements the (sleep <ticks>) built-in.
 *
 * Three stack allocations from the current thread frame:
 *   sched_time_ptr  (4 bytes, int16_t*): stores the sleep duration in ticks
 *   wake_datum_ptr  (4 bytes, int32_t*): stores the datum handle to wake at
 *   step_ptr        (2 bytes, int16_t*): step counter (0=first, 1=subsequent)
 *
 * Execution phases (controlled by step_ptr):
 *   init=1: evaluate the ticks argument expression via FUN_000cc1d0, storing
 *            the result at sched_time_ptr; set step=0 and return.
 *   init=0, step=0 (first tick):
 *     - Read the sleep duration from the syntax node directly.
 *     - Increment step to 1.
 *     - If ticks != -1: kick off deferred evaluation of ticks arg
 * (FUN_000cc1d0) and return (deferred). Otherwise: set wake_datum_ptr = -1 and
 * fall through to the step!=0 block. init=0, step!=0 (subsequent ticks):
 *     - Read sched_time (evaluated ticks) from sched_time_ptr.
 *     - If sched_time == 0: complete immediately via FUN_000cbf80(thread_datum,
 * 0).
 *     - If wake_datum_ptr != -1: find the target thread via FUN_000cada0
 *       (@EDI = (int16_t)*wake_datum_ptr), look it up, then:
 *         * If sched_time < 0: set EBX = -2 (relative-forever sentinel).
 *         * If sched_time >= 0: compute wake_at = game_time_get() + sched_time.
 *       - If target thread has an active wakeup (thread+0x8 != -1):
 *         * If target != current thread and bit 0x2 not yet set:
 *           save old wakeup tick to thread+0xc and set flag bit 0x2.
 *         * Re-derive thread ptr and write wake_at to thread+0x8.
 *     - Call FUN_000cbf80(thread_datum, 0) to complete the expression.
 *
 * Note: [EBP+0xc] (param_2 slot) is overwritten with wake_datum_ptr after
 * the three hs_thread_stack_alloc calls. The param_3 (init) slot at [EBP+0x10]
 * is overwritten with the computed wake_at value on the game_time_get path.
 *
 * function_index 0x13 = _hs_function_sleep.
 *
 * Globals:
 *   0x5aa6c4 = hs_thread_data  (data_t*)
 *   0x5aa6c8 = hs_syntax_data  (data_t*)
 */
void FUN_000cd0e0(int16_t function_index, int thread_datum, char init)
{
  /* Three stack allocations from the thread frame. */
  int16_t *sched_time_ptr; /* 4-byte alloc: sleep duration in ticks */
  int32_t *wake_datum_ptr; /* 4-byte alloc: wake datum handle (int32 write,
                              int16 read) */
  int16_t *step_ptr; /* 2-byte alloc: step counter */

  /* Allocate all three slots before any branching. */
  sched_time_ptr = (int16_t *)hs_thread_stack_alloc(thread_datum, 4);
  wake_datum_ptr = (int32_t *)hs_thread_stack_alloc(thread_datum, 4);
  step_ptr = (int16_t *)hs_thread_stack_alloc(thread_datum, 2);

  /* Derive thread pointer for expression traversal. */
  char *thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);

  if (function_index != 0x13) {
    display_assert("function_index==_hs_function_sleep",
                   "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 0x189,
                   1);
    system_exit(-1);
  }

  if (init) {
    /* Phase: init. Evaluate the ticks argument and store at sched_time_ptr. */
    char *call_node = (char *)datum_get(
      *(data_t **)0x5aa6c8, *(int *)(*(char **)(thread + 0x10) + 0x4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(call_node + 0x10));
    FUN_000cc1d0(thread_datum, *(int *)(child + 0x8), (void *)sched_time_ptr);
    *step_ptr = 0;
    return;
  }

  if (*step_ptr == 0) {
    /* Phase: step 0, first tick — read sleep_ticks from syntax node. */
    char *call_node = (char *)datum_get(
      *(data_t **)0x5aa6c8, *(int *)(*(char **)(thread + 0x10) + 0x4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(call_node + 0x10));
    char *first_arg =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(child + 0x8));
    int sleep_ticks = *(int *)(first_arg + 0x8);

    (*step_ptr)++; /* advance to step 1 */

    if (sleep_ticks != -1) {
      /* Kick off deferred evaluation of ticks argument. */
      FUN_000cc1d0(thread_datum, sleep_ticks, (void *)wake_datum_ptr);
      return;
    }
    /* sleep_ticks == -1: set wake_datum to -1 (32-bit write). */
    *wake_datum_ptr = -1;

    /* step is now 1; fall through to step != 0 block. */
    if (*step_ptr == 0)
      return;
  }

  /* Phase: step != 0, subsequent ticks. */
  {
    int16_t sched_time = *sched_time_ptr;
    if (sched_time != 0) {
      /* Find target thread to schedule the wakeup. */
      int local_thread = thread_datum;
      int16_t wake_val = (int16_t)*wake_datum_ptr;
      if (wake_val != (int16_t)-1) {
        local_thread = FUN_000cada0((int)(uint16_t)wake_val);
      }
      if (local_thread != -1) {
        char *tgt_thread =
          (char *)datum_get(*(data_t **)0x5aa6c4, local_thread);
        int wake_at;
        if (sched_time < 0) {
          wake_at = -2;
        } else {
          wake_at = game_time_get() + (int)sched_time;
        }
        if (*(int *)(tgt_thread + 0x8) != -1) {
          if (local_thread != thread_datum &&
              (*(uint8_t *)(tgt_thread + 0x3) & 0x2) == 0) {
            *(uint8_t *)(tgt_thread + 0x3) |= 0x2;
            *(int *)(tgt_thread + 0xc) = *(int *)(tgt_thread + 0x8);
          }
          /* Re-derive thread pointer and write wake time. */
          tgt_thread = (char *)datum_get(*(data_t **)0x5aa6c4, local_thread);
          *(int *)(tgt_thread + 0x8) = wake_at;
        }
      }
    }
  }

  FUN_000cbf80(thread_datum, 0);
}

/* 0xcd2a0 — HS sleep_until evaluator. Suspends the current thread until a
 * condition expression becomes true, with an optional timeout.
 *
 * Stack frame allocations (in order, from hs_thread_stack_alloc):
 *   done_flag_ptr  (4-byte slot, byte access): set to 0 on init; set to 1
 *                  by the callee (FUN_000cc1d0) when the condition is true.
 *   ticks_ptr      (4-byte slot, int16 access): default sleep interval in
 *                  ticks (0x1e = 30); may be overwritten by condition eval.
 *   timeout_ptr    (4-byte slot, int32 access): deadline in ticks (-1 = none).
 *                  Populated on step==0 if a timeout expression exists.
 *   start_time_ptr (4-byte slot, int32 access): game_time_get() captured on
 *                  init; used with timeout_ptr to bound the wait.
 *   step_ptr       (2-byte slot, int16 access): phase counter.
 *                  0 = first tick (set up timeout), 1 = subsequent ticks.
 *
 * Syntax tree traversal at the top:
 *   thread_ptr = datum_get(hs_thread_data, thread_datum)
 *   call_node  = datum_get(hs_syntax_data, *(frame + 0x4))
 *   child_node = datum_get(hs_syntax_data, *(call_node + 0x10))
 *   grandchild = datum_get(hs_syntax_data, *(child_node + 0x8))
 *   sleep_until_target = *(int *)(grandchild + 0x8)
 *   (The sleep_until_target is -1 if the condition sub-expression is absent.)
 *
 * Execution phases:
 *   init != 0: set done_flag=0, start_time=game_time_get(), step=0,
 *              ticks=0x1e, timeout=-1. If sleep_until_target != -1, kick off
 *              FUN_000cc1d0(thread_datum, sleep_until_target, ticks_ptr) and
 *              return (deferred). Otherwise return immediately.
 *   init==0, step==0 (first tick):
 *     Set step=1. If sleep_until_target != -1:
 *       look up child = datum_get(hs_syntax_data, sleep_until_target).
 *       If *(child + 0x8) != -1, kick FUN_000cc1d0(thread_datum, that,
 *       timeout_ptr) and return.
 *   init==0, step==1 (subsequent ticks):
 *     If done_flag==0 and (timeout_ptr==-1 or game_time_get() <
 * timeout+start_time): Re-evaluate condition: traverse syntax tree again and
 * call FUN_000cc1d0(thread_datum, new_target, done_flag_ptr). Then compute next
 * wake tick: max(1, *ticks_ptr). Write wake = game_time_get() + tick to
 * thread+0x8. If timeout != -1, clamp wake to min(wake, timeout+start). Else:
 * FUN_000cbf80(thread_datum, 0) to complete.
 *
 * function_index 0x14 = _hs_function_sleep_until.
 *
 * Globals:
 *   0x5aa6c4 = hs_thread_data  (data_t*)
 *   0x5aa6c8 = hs_syntax_data  (data_t*)
 */
void FUN_000cd2a0(int16_t function_index, int thread_datum, char init)
{
  /* Stack frame allocations — all before any branching. */
  char *done_flag_ptr; /* 4-byte slot; byte access: 0=pending, 1=done */
  int16_t *ticks_ptr; /* 4-byte slot; int16 access: re-check interval */
  int32_t *timeout_ptr; /* 4-byte slot; int32 access: absolute deadline  */
  int32_t *start_time_ptr; /* 4-byte slot; int32 access: game_time at init  */
  int16_t *step_ptr; /* 2-byte slot; int16 access: phase counter      */

  done_flag_ptr = (char *)hs_thread_stack_alloc(thread_datum, 4);
  ticks_ptr = (int16_t *)hs_thread_stack_alloc(thread_datum, 4);
  timeout_ptr = (int32_t *)hs_thread_stack_alloc(thread_datum, 4);
  start_time_ptr = (int32_t *)hs_thread_stack_alloc(thread_datum, 4);
  step_ptr = (int16_t *)hs_thread_stack_alloc(thread_datum, 2);

  /* Traverse the syntax tree to locate the sleep_until condition target. */
  char *thread_ptr = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  char *call_node = (char *)datum_get(
    *(data_t **)0x5aa6c8, *(int *)(*(char **)(thread_ptr + 0x10) + 0x4));
  char *child_node =
    (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(call_node + 0x10));
  char *grandchild =
    (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(child_node + 0x8));
  int sleep_until_target = *(int *)(grandchild + 0x8);

  if (function_index != 0x14) {
    display_assert("function_index==_hs_function_sleep_until",
                   "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 0x1e5,
                   1);
    system_exit(-1);
  }

  if (init) {
    /* Phase: init. Capture start time, set defaults, optionally kick
     * evaluation of the condition expression. Stores are in the order
     * the original MSVC code emits them (field rotation preserved). */
    *done_flag_ptr = 0;
    *start_time_ptr = game_time_get();
    *step_ptr = 0;
    *ticks_ptr = 0x1e; /* 30 ticks default re-check interval */
    *timeout_ptr = -1;
    if (sleep_until_target != -1) {
      FUN_000cc1d0(thread_datum, sleep_until_target, (void *)ticks_ptr);
      return;
    }
    return;
  }

  if (*step_ptr == 0) {
    /* Phase: step 0 (first tick). Advance step, then optionally kick
     * evaluation of the timeout expression into timeout_ptr. */
    *step_ptr = 1;
    if (sleep_until_target != -1) {
      char *cond_child =
        (char *)datum_get(*(data_t **)0x5aa6c8, sleep_until_target);
      int timeout_expr = *(int *)(cond_child + 0x8);
      if (timeout_expr != -1) {
        FUN_000cc1d0(thread_datum, timeout_expr, (void *)timeout_ptr);
        return;
      }
    }
    /* fall through to step 1 logic */
  } else if (*step_ptr != 1) {
    return;
  }

  /* Phase: step 1 (subsequent ticks) — also reachable by fall-through from
   * step 0 when no timeout expression exists. Check whether we should keep
   * waiting or complete. */
  if (*done_flag_ptr == 0 &&
      (*timeout_ptr == -1 ||
       game_time_get() < *timeout_ptr + *start_time_ptr)) {
    /* Condition not yet met and not timed out: re-evaluate condition.
     * The original re-derives thread_ptr via datum_get here (EDI was
     * clobbered by the datum_get calls in the step-0 path above). */
    char *thread2 = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
    char *frame2 = *(char **)(thread2 + 0x10);
    char *call2 =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(frame2 + 0x4));
    char *child2 =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(call2 + 0x10));
    FUN_000cc1d0(thread_datum, *(int *)(child2 + 0x8), (void *)done_flag_ptr);

    /* Compute next re-check tick: max(1, *ticks_ptr). */
    int tick = (int)*ticks_ptr;
    if (tick < 1)
      tick = 1;
    int wake = game_time_get() + tick;
    *(int *)(thread2 + 0x8) = wake;

    /* Clamp to timeout deadline if set. */
    if (*timeout_ptr != -1) {
      int deadline = *timeout_ptr + *start_time_ptr;
      if (deadline <= wake) {
        *(int *)(thread2 + 0x8) = deadline;
      }
      return;
    }
  } else {
    /* Condition met or timed out: complete the expression. */
    FUN_000cbf80(thread_datum, 0);
  }
}

/* 0xcd5a0 — HS object-to-unit type converter. Evaluates one argument,
 * checks if the object's type matches the target conversion mask from
 * the table at 0x26f320. Returns the object if compatible, NONE if not.
 * function_index 0x17 = object_to_unit. */
void FUN_000cd5a0(int16_t function_index, int thread_datum, char init)
{
  char *thread;
  int *result_ptr;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  result_ptr = (int *)hs_thread_stack_alloc(thread_datum, 4);

  if (function_index < 0x17 || function_index > 0x17) {
    display_assert("function_index>=_hs_function_object_to_unit && "
                   "function_index<=_hs_function_object_to_unit",
                   "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 0x2dc,
                   1);
    system_exit(-1);
  }

  if (init) {
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(char **)(thread + 0x10) + 0x4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    FUN_000cc1d0(thread_datum, *(int *)(child + 0x8), result_ptr);
    return;
  }

  if (*result_ptr == -1) {
    FUN_000cbf80(thread_datum, -1);
    return;
  }

  {
    char *obj = (char *)object_get_and_verify_type(*result_ptr, -1);
    int type_idx = (int)(int16_t)(function_index - 0x16);
    int type_bit = 1 << (*(uint8_t *)(obj + 0x64) & 0x1f);
    int type_mask = (int)*(int16_t *)(0x26f320 + type_idx * 2);

    if (type_mask & type_bit) {
      FUN_000cbf80(thread_datum, *result_ptr);
      return;
    }

    const char *tag_name = tag_get_name(*(int *)obj);
    error(2, "attempt to convert object %s to type %s", tag_name,
          *(const char **)(0x2f153c + type_idx * 4));
    FUN_000cbf80(thread_datum, -1);
  }
}

/* 0xcd840 — Main HS thread execution tick. Runs the thread's expression
 * evaluation loop: resolves the current stack frame's expression, dispatches
 * to either a built-in function evaluate callback or a script-reference
 * evaluation. Respects sleep_until timing and the runtime-enabled flag.
 * On completion, marks continuous/dormant scripts as finished (sleep=-1)
 * and deletes console-command threads.
 */
static void FUN_000cd840(int thread_handle)
{
  char *thread;
  char *script;
  char *stack_base;
  typedef void (*hs_evaluate_t)(int, int, int);

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_handle);
  *(int16_t *)0x46b812 = (int16_t)thread_handle;
  script = NULL;

  /* If script-type thread, look up and validate the script entry */
  if (*(uint8_t *)(thread + 0x2) == 0) {
    char *scenario = (char *)global_scenario_get();
    script = (char *)tag_block_get_element(scenario + 0x49c,
                                           *(int32_t *)(thread + 0x4), 0x5c);
    if (*(int16_t *)(script + 0x20) == 3 || *(int16_t *)(script + 0x20) == 4) {
      char *name = hs_get_thread_script_name(thread_handle);
      char *msg =
        csprintf((char *)0x5ab100,
                 "a problem occurred while executing the script %s: %s (%s)",
                 name, "found a static script at toplevel.",
                 "script->script_type!=_hs_script_static && "
                 "script->script_type!=_hs_script_stub");
      display_assert(msg, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x2ba, true);
      system_exit(-1);
    }
  }

  /* valid_thread(thread) — verify stack pointer is within bounds */
  {
    data_t *td = *(data_t **)0x5aa6c4;
    uint32_t pool_base = *(uint32_t *)((char *)td + 0x34);
    int16_t datum_count = *(int16_t *)((char *)td + 0x2e);
    int16_t datum_size = *(int16_t *)((char *)td + 0x22);
    uint32_t pool_end = pool_base + (int)datum_count * (int)datum_size;
    uint32_t thr = (uint32_t)thread;
    uint32_t sp = *(uint32_t *)(thread + 0x10);
    uint32_t sb = thr + 0x18;
    uint32_t se = thr + 0x218;

    if (thr < pool_base || thr >= pool_end || sp < sb || sp >= se ||
        sp + (int)*(int16_t *)(sp + 0xc) + 0xe > se) {
      char *name = hs_get_thread_script_name(thread_handle);
      char *msg =
        csprintf((char *)0x5ab100,
                 "a problem occurred while executing the script %s: %s (%s)",
                 name, "corrupted stack.", "valid_thread(thread)");
      display_assert(msg, "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x2bd, true);
      system_exit(-1);
    }
  }

  *(int32_t *)(thread + 0x8) = 0;
  stack_base = thread + 0x18;

  /* First tick: initialize the root expression evaluation */
  if (*(char **)(thread + 0x10) == stack_base) {
    if (script == NULL) {
      display_assert("script", "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x2c3,
                     true);
      system_exit(-1);
    }
    *(int16_t *)(*(char **)(thread + 0x10) + 0xc) = 0;
    {
      void *result = hs_thread_stack_alloc(thread_handle, 4);
      FUN_000cc1d0(thread_handle, *(int *)(script + 0x24), result);
    }
    if (*(char **)(thread + 0x10) == stack_base)
      goto done;
  }

  /* Main execution loop */
  do {
    char *expr;
    uint8_t eval_flag;

    if (*(int32_t *)(thread + 0x8) < 0)
      break;
    if (game_in_progress() && game_time_get() < *(int32_t *)(thread + 0x8))
      break;
    if (*(uint8_t *)0x46b810 == 0)
      break;

    expr = (char *)datum_get(*(data_t **)0x5aa6c8,
                             *(int *)(*(char **)(thread + 0x10) + 0x4));
    eval_flag = *(uint8_t *)(thread + 0x3) & 1;
    *(int16_t *)(*(char **)(thread + 0x10) + 0xc) = 0;
    *(uint8_t *)(thread + 0x3) &= 0xfe;

    if (!(*(uint8_t *)(expr + 0x6) & 2)) {
      /* Built-in function call */
      int func_idx = (int)(uint16_t) * (int16_t *)(expr + 0x2);
      char *func_entry = (char *)hs_function_table_get((int16_t)func_idx);
      hs_evaluate_t evaluate = *(hs_evaluate_t *)(func_entry + 0xc);
      if (evaluate == NULL) {
        display_assert("function->evaluate",
                       "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x2d8, true);
        system_exit(-1);
      }
      func_idx = (int)(uint16_t) * (int16_t *)(expr + 0x2);
      evaluate(func_idx, thread_handle, (int)eval_flag);
    } else {
      /* Script reference */
      int script_idx = (int)*(int16_t *)(expr + 0x2);
      char *scenario = (char *)global_scenario_get();
      char *ref_script =
        (char *)tag_block_get_element(scenario + 0x49c, script_idx, 0x5c);
      datum_get(*(data_t **)0x5aa6c4, thread_handle);
      {
        void *result = hs_thread_stack_alloc(thread_handle, 4);
        if (eval_flag) {
          FUN_000cc1d0(thread_handle, *(int *)(ref_script + 0x24), result);
        } else {
          FUN_000cbf80(thread_handle, *(int *)result);
        }
      }
    }
  } while (*(char **)(thread + 0x10) != stack_base);

done:
  if (*(char **)(thread + 0x10) == stack_base) {
    if (*(uint8_t *)(thread + 0x2) == 0) {
      if (*(int16_t *)(script + 0x20) == 0 ||
          *(int16_t *)(script + 0x20) == 1) {
        *(int32_t *)(thread + 0x8) = -1;
        *(int16_t *)0x46b812 = -1;
        return;
      }
    } else if (*(uint8_t *)(thread + 0x2) == 2) {
      FUN_000caa30(thread_handle);
    }
  }
  *(int16_t *)0x46b812 = -1;
}

/* Initialize HaloScript runtime for a new map. Deletes all existing thread
 * data, creates an internal initialization thread, runs all global
 * initialization scripts (type 0x17), then starts continuous/dormant script
 * threads. Asserts if a global init script attempts to sleep.
 *
 * Scenario globals block is at scenario+0x49c (element size 0x5c).
 * Scenario scripts block is at scenario+0x4a8 (element size 0x5c).
 *
 * Key globals:
 *   0x5aa6c4 = hs_thread_data (data_t*)
 *   0x5aa6c0 = hs_globals_data (data_t*)
 *   0x5aa6c8 = hs_syntax_data (data_t*)
 *   0x46b810 = hs_runtime_globals.executing (uint8_t)
 *   0x46b812 = hs_runtime_globals.current_thread (int16_t)
 *   0x27d504 = hs_globals_start_index (int16_t)
 *   0x326a08 = global_scenario_index (int)
 *   0x5aa6a0 = hs_runtime return values buffer (0x20 bytes)
 */
void hs_runtime_initialize_for_new_map(void)
{
  int thread_index;
  char *internal_thread;
  char *scenario;
  char *script_element;
  char *datum_ptr;
  char *stack_frame;
  short loop_var;
  int loop_idx;

  /* Phase 1: wipe all thread data, mark runtime as executing. */
  data_delete_all(*(data_t **)0x5aa6c4);
  *(uint8_t *)0x46b810 = 1;
  *(int16_t *)0x46b812 = -1;

  /* Phase 2: allocate the internal initialization thread. */
  thread_index = data_new_at_index(*(data_t **)0x5aa6c4);
  if (thread_index != -1) {
    internal_thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_index);
    *(int *)(internal_thread + 0x10) = (int)(internal_thread + 0x18);
    *(int *)(internal_thread + 0x18) = 0;
    stack_frame = *(char **)(internal_thread + 0x10);
    *(int16_t *)(stack_frame + 0xc) = 0;
    *(int *)(stack_frame + 0x4) = -1;
    *(uint8_t *)(internal_thread + 0x2) = 1;
    *(int *)(internal_thread + 0x4) = -1;
    *(uint8_t *)(internal_thread + 0x3) = 0;
    *(int *)(internal_thread + 0x8) = 0;
  }

  /* Phase 3: run global initialization scripts if a scenario is loaded. */
  if (*(int *)0x326a08 != -1) {
    scenario = (char *)global_scenario_get();
    internal_thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_index);

    loop_var = 0;
    if (*(int *)(scenario + 0x4a8) > 0) {
      loop_idx = 0;
      do {
        /* Get the current script element from the scripts block. */
        {
          char *block_base = (char *)global_scenario_get();
          block_base += 0x4a8;
          script_element =
            (char *)tag_block_get_element(block_base, loop_idx, 0x5c);
        }

        /* Compute the global datum index: if bit 15 set on loop_var, use
         * raw index; otherwise add hs_globals_start_index. */
        {
          int raw_idx = loop_idx & 0x7fff;
          int datum_idx;
          if (loop_var & (int16_t)0x8000)
            datum_idx = raw_idx;
          else
            datum_idx = (int)*(int16_t *)0x27d504 + raw_idx;

          data_new_datum(*(data_t **)0x5aa6c0, (int)(datum_idx | 0xaced0000));

          /* Re-derive datum_idx (same logic, needed after the call). */
          if (loop_var & (int16_t)0x8000)
            datum_idx = raw_idx;
          else
            datum_idx = (int)*(int16_t *)0x27d504 + raw_idx;

          datum_ptr = (char *)datum_get(*(data_t **)0x5aa6c0, datum_idx);
        }

        /* Reset internal thread state and call hs_default_value.
         * hs_default_value (0xcc1d0) takes EAX=thread_index,
         * stack args: (hs_type, dest_ptr). */
        *(int *)(internal_thread + 0x4) = -1;
        {
          char *sf = *(char **)(internal_thread + 0x10);
          *(int16_t *)(sf + 0xc) = 0;
        }
        FUN_000cc1d0(thread_index, *(int *)(script_element + 0x28),
                     (void *)(datum_ptr + 4));

        /* If the script was successfully parsed (bit 0 of byte +3),
         * execute it. */
        if (*(uint8_t *)(internal_thread + 0x3) & 1) {
          FUN_000cd840(thread_index);

          /* If this is a global initialization script (type == 0x17),
           * store the result back into the globals. */
          if (*(int16_t *)(script_element + 0x20) == 0x17) {
            FUN_000cb230((int)loop_var);

            /* Re-derive datum pointer and evaluate the expression.
             * The original code re-calls datum_get here because EDI
             * (internal_thread) was clobbered by cb230. */
            {
              int raw_idx = loop_idx & 0x7fff;
              int datum_idx;
              if (loop_var & (int16_t)0x8000)
                datum_idx = raw_idx;
              else
                datum_idx = (int)*(int16_t *)0x27d504 + raw_idx;

              datum_ptr = (char *)datum_get(*(data_t **)0x5aa6c0, datum_idx);
              FUN_000ce350(*(int *)(datum_ptr + 0x4));
            }
            /* Restore internal_thread (original saved in [EBP-0x10],
             * we re-derive via datum_get). */
            internal_thread =
              (char *)datum_get(*(data_t **)0x5aa6c4, thread_index);
          }

          /* Assert: global init scripts must not sleep.
           * hs_get_thread_script_name (0xcaa80) takes ESI=thread_index
           * as register arg and returns the script name string. */
          if (*(int *)(internal_thread + 0x8) != 0) {
            char *script_name = hs_get_thread_script_name(thread_index);
            display_assert(
              csprintf(error_string_buffer,
                       "a problem occurred while executing the script "
                       "%s: %s (%s)",
                       script_name,
                       "a global initialization attempted to sleep.",
                       "internal_thread->sleep_until==0"),
              "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0xe7, true);
            system_exit(-1);
          }
        }

        FUN_000cb7b0((int)loop_var);

        loop_var++;
        loop_idx = (int)(int16_t)loop_var;
        scenario = (char *)global_scenario_get();
      } while (loop_idx < *(int *)(scenario + 0x4a8));
    }

    /* Verify internal thread type and delete it. */
    internal_thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_index);
    if (*(uint8_t *)(internal_thread + 0x2) == 0) {
      display_assert(
        "hs_thread_get(thread_index)->type!=_hs_thread_type_script",
        "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x290, true);
      system_exit(-1);
    }
    datum_delete(*(data_t **)0x5aa6c4, thread_index);

    /* Phase 4: start script threads for non-static/startup scripts.
     * Iterates the scenario globals block (offset 0x49c). Scripts with
     * type 3 (static) or 4 (startup) are skipped; others get a new
     * hs thread via ca940 which takes EBX=script_index as register arg
     * and one stack arg (type=0). */
    {
      short script_loop = 0;
      int script_idx = 0;
      char *scripts_block = scenario + 0x49c;
      if (*(int *)scripts_block > 0) {
        do {
          char *script =
            (char *)tag_block_get_element(scripts_block, script_idx, 0x5c);
          int16_t script_type = *(int16_t *)(script + 0x20);
          if (script_type != 3 && script_type != 4) {
            int result = hs_thread_new(script_idx, 0);
            if (result == -1) {
              error(0, "ran out of script threads.");
            }
          }
          script_loop++;
          script_idx = (int)(int16_t)script_loop;
        } while (script_idx < *(int *)scripts_block);
      }
    }
  }

  /* Phase 5: clear the return values buffer. */
  csmemset((void *)0x5aa6a0, 0, 0x20);
}

/* Execute a HaloScript expression at runtime. Allocates a new thread,
 * initializes it as a runtime thread (type 2), sets up the default value
 * for the expression type, and either executes it immediately or returns
 * the result value. Returns -1 if the runtime is not active, the
 * thread_index is invalid, or no threads are available.
 *
 * 0xcc1d0 = hs_default_value (@EAX=thread_handle, stack: expression_index,
 *           dest_ptr)
 * 0xcd840 = hs_execute_thread (@EAX=thread_handle)
 *
 * Globals:
 *   0x46b810 = hs_runtime_globals.executing (uint8_t)
 *   0x5aa6c4 = hs_thread_data (data_t*)
 */
int hs_runtime_execute(int thread_index)
{
  int thread_handle;
  char *thread_ptr;

  if (*(uint8_t *)0x46b810 == 0 || thread_index == -1)
    return -1;

  thread_handle = data_new_at_index(*(data_t *volatile *)0x5aa6c4);

  if (thread_handle == -1) {
    error(2, "there are not enough threads to execute that command.");
    return -1;
  }

  thread_ptr = (char *)datum_get(*(data_t *volatile *)0x5aa6c4, thread_handle);

  /* Initialize thread structure. */
  *(int *)(thread_ptr + 0x10) = (int)(thread_ptr + 0x18);
  *(int *)(thread_ptr + 0x18) = 0;
  {
    char *sf = *(char **)(thread_ptr + 0x10);
    *(int16_t *)(sf + 0xc) = 0;
    *(int *)(sf + 0x4) = -1;
  }
  *(uint8_t *)(thread_ptr + 0x2) = 2; /* runtime thread */
  *(int *)(thread_ptr + 0x4) = -1;
  *(uint8_t *)(thread_ptr + 0x3) = 0;
  *(int *)(thread_ptr + 0x8) = 0;

  /* Re-derive thread pointer (original does a second datum_get). */
  thread_ptr = (char *)datum_get(*(data_t *volatile *)0x5aa6c4, thread_handle);

  FUN_000cc1d0(thread_handle, thread_index, (void *)(thread_ptr + 0x14));

  if (*(uint8_t *)(thread_ptr + 0x3) & 1) {
    /* Thread needs execution — run it. */
    FUN_000cd840(thread_handle);
    return -1;
  }

  /* Return the result value stored at thread+0x14. */
  return *(int *)(thread_ptr + 0x14);
}

/* Initialize HaloScript runtime data structures. Calls data_delete_all
 * on both hs object list data pools.
 *
 * 0x5aa698 = hs_object_list_header_data (data_t*)
 * 0x5aa694 = hs_object_list_reference_data (data_t*)
 */
void hs_runtime_initialize(void)
{
  data_delete_all(*(data_t **)0x5aa698);
  data_delete_all(*(data_t **)0x5aa694);
}

/* Dispose HaloScript runtime data structures. Calls data_make_invalid
 * on both hs object list data pools.
 *
 * 0x5aa698 = hs_object_list_header_data (data_t*)
 * 0x5aa694 = hs_object_list_reference_data (data_t*)
 */
void hs_runtime_dispose(void)
{
  data_make_invalid(*(data_t **)0x5aa698);
  data_make_invalid(*(data_t **)0x5aa694);
}

/* 0xce350 */
void FUN_000ce350(int expression_datum)
{
  if (expression_datum != -1) {
    char *node = (char *)datum_get(*(data_t **)0x5aa698, expression_datum);
    *(int16_t *)(node + 0x4) += 1;
  }
}
