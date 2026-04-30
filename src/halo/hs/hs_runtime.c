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
 *   0xcb070 = hs_types_compatible (short actual, short desired) -> bool [ported]
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
          result_type = hs_global_get_type((uint16_t) * (int16_t *)(node + 0x10));
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
            int16_t func_idx = hs_find_function_by_name((const char *)name_addr);
            if (func_idx == -1) {
              *(int *)0x46b6fc =
                (int)"missing function (you need to recompile scripts.)";
              goto error;
            }

            /* Update the node's function index and look up the return
             * type. */
            *(int16_t *)(node + 0x2) = func_idx;

            hs_function_table_get(func_idx);
            result_type = *(int16_t *)hs_function_table_get(*(int16_t *)(node + 0x2));
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
      return FUN_000caff0((int16_t)(actual_type - 0x25), d_off);
    if (actual_type >= 0x2b && actual_type <= 0x30)
      return FUN_000caff0((int16_t)(actual_type - 0x2b), d_off);
    return false;
  }

  if (desired_type >= 0x2b && desired_type <= 0x30) {
    if (actual_type < 0x2b || actual_type > 0x30)
      return false;
    return FUN_000caff0((int16_t)(actual_type - 0x2b),
                        (int16_t)(desired_type - 0x2b));
  }

  return *(int *)((char *)0x2f3ec0 +
                  ((int)desired_type * 0x31 + (int)actual_type) * 4) != 0;
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
            char *script_name = FUN_000caa80(thread_index);
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
            /* hs_thread_new (0xca940): EBX=script_index, stack arg=type.
             * Returns thread index in EAX, -1 on failure. */
            int result = FUN_000ca940(script_idx, 0);
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
