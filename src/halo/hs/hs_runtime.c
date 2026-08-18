
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

/* Type-check the argument list of a `sleep_until' call.
 *
 * sleep_until takes a boolean condition and, optionally, a short tick
 * period and a long timeout. The syntax node at expression_index is the
 * function-call node; +0x10 is the index of its function-name node, whose
 * +0x8 (next) is the first argument.
 *
 * Syntax node offsets (raw, matching the rest of this TU):
 *   +0x08 = next node index (NONE == -1)
 *   +0x0c = source offset
 *   +0x10 = long value / first child node index
 *
 * Globals:
 *   0x5aa6c8 = hs_syntax_data (data_t*)
 *   0x46b6fc = hs_compile_globals.error_message
 *   0x46b700 = hs_compile_globals.error_offset
 */
bool hs_sleep_until_parse(int16_t function_index, int expression_index)
{
  char *node;
  int condition_index;
  int period_index;
  int timeout_index;
  bool success;

  success = false;

  if (function_index != 0x14) { /* _hs_function_sleep_until */
    display_assert("function_index==_hs_function_sleep_until",
                   "c:\\halo\\source\\hs\\hs_library_internal_compile.h", 0x235,
                   true);
    system_exit(-1);
  }

  node = (char *)datum_get(*(data_t **)0x5aa6c8, expression_index);
  node = (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
  condition_index = *(int *)(node + 0x8);

  if (condition_index != -1) {
    node = (char *)datum_get(*(data_t **)0x5aa6c8, condition_index);
    period_index = *(int *)(node + 0x8);

    if (hs_type_check(condition_index, 5)) { /* _hs_type_boolean */
      if (period_index == -1) {
        success = true;
      } else {
        node = (char *)datum_get(*(data_t **)0x5aa6c8, period_index);
        timeout_index = *(int *)(node + 0x8);

        if (hs_type_check(period_index, 7)) { /* _hs_type_short */
          if (timeout_index == -1) {
            success = true;
          } else {
            success = hs_type_check(timeout_index, 8); /* _hs_type_long */
          }
        }
      }
    }
  } else {
    *(const char **)0x46b6fc =
      "the sleep_until call requires a condition and, optionally, a period.";
    node = (char *)datum_get(*(data_t **)0x5aa6c8, expression_index);
    *(int *)0x46b700 = *(int *)(node + 0xc);
  }

  return success;
}

/* 0xc8d30 — Compile-time checker for the HaloScript "wake" call.
 *
 * Binary evidence (0xc8d30..0xc8df8):
 *   - CMP SI,0x15 guards an assert built from the literals at 0x27d1bc
 *     ("function_index==_hs_function_wake") and 0x27cdc0
 *     ("c:\halo\source\hs\hs_library_internal_compile.h", line 0x25d), so the
 *     only legal function index is 0x15.
 *   - LEA EAX,[EBP+8]; PUSH EAX at 0xc8d64 passes the address of the *first
 *     stack parameter* as the argument_nodes array of FUN_000c55d0, with
 *     EDI = [EBP+0xc] (the expression node) and EBX = 1 (one expected
 *     argument).  FUN_000c55d0 writes the argument node handle over that
 *     slot, which is why 0xc8d8a reloads [EBP+8] as the argument handle
 *     rather than as the function index.  The full 32-bit datum handle
 *     (salt<<16 | index) is what is stored and reloaded.
 *   - The argument must pass hs_type_check(handle, 10); the node's int16 at
 *     +0x10 indexes the scenario scripts tag_block (scenario+0x49c, element
 *     size 0x5c), and the script's int16 at +0x20 is its script type.  Types
 *     3 and 4 are rejected with the literal at 0x27d194.
 *   - AL is loaded from the [EBP-1] flag byte (initialised to 0) on every
 *     exit except the accept path at 0xc8dd5, which returns 1.
 *
 * Globals: 0x5aa6c8 = hs_syntax_data (data_t *), 0x46b6fc =
 * hs_compile_globals.error_message, 0x46b700 = hs_compile_globals.error_offset.
 *
 * The name stays FUN_000c8d30: the assert string proves the function index
 * that reaches this callback, not the callback's own symbol name. */
bool FUN_000c8d30(int function_index, int script_node)
{
  const char *function_name;
  char *node;
  char *script;
  bool success;

  success = false;

  if ((int16_t)function_index != 0x15) { /* _hs_function_wake */
    display_assert("function_index==_hs_function_wake",
                   "c:\\halo\\source\\hs\\hs_library_internal_compile.h", 0x25d,
                   true);
    system_exit(-1);
  }

  function_name = *(
    const char **)((char *)hs_function_table_get((int16_t)function_index) + 4);

  /* &function_index is the one-element argument_nodes array: the callee
   * overwrites the incoming first parameter slot with the argument handle. */
  if (FUN_000c55d0(function_name, &function_index, script_node, 1)) {
    node = (char *)datum_get(*(data_t **)0x5aa6c8, function_index);

    if (hs_type_check(function_index, 10)) { /* _hs_type_script */
      script = (char *)tag_block_get_element(
        (char *)global_scenario_get() + 0x49c, *(int16_t *)(node + 0x10), 0x5c);

      if (*(int16_t *)(script + 0x20) != 3 &&
          *(int16_t *)(script + 0x20) != 4) {
        return true;
      }

      *(const char **)0x46b6fc = "this static script cannot be awakened.";
      *(int *)0x46b700 = *(int *)(node + 0xc);
    }
  }

  return success;
}

/* 0xc8f40 — Type-check the arguments of a debug-string function call.
 *
 * The syntax node at expression_index is the function-call node; +0x10 is
 * the index of its function-name node, whose +0x08 (next) is the first
 * argument. Each untyped argument (+0x04 == 0) is assigned the
 * debug-string type (9) and dispatched by its constant flag (+0x06 bit 0)
 * to FUN_000c73a0 (@EDI) or FUN_000c74c0 (@EBX). The walk stops at the
 * first failed check and returns false; the return value is BL, which the
 * epilogue moves to AL (MOV AL,BL at 0xc903e).
 *
 * Both asserts are in binary order: the function_index range check
 * (hs_library_internal_compile.h line 0x2ae) runs after the two
 * datum_get calls, and the !hs_compile_globals.error check
 * (hs_compile.c line 0x48e) runs inside the loop after the per-argument
 * datum_get, matching the inlined hs_type_check body at 0xc7d80.
 *
 * Globals:
 *   0x5aa6c8 = hs_syntax_data (data_t*)
 *   0x46b6fc = hs_compile_globals.error_message
 */
bool FUN_000c8f40(int16_t function_index, int expression_index)
{
  bool valid;
  int argument_index;
  char *node;
  char *node2;

  node = (char *)datum_get(*(data_t **)0x5aa6c8, expression_index);
  node = (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
  argument_index = *(int *)(node + 0x8);

  if (function_index < 0x18 || function_index > 0x1a) {
    display_assert("(function_index>=_hs_function_debug_string__first) && "
                   "(function_index<=_hs_function_debug_string__last)",
                   "c:\\halo\\source\\hs\\hs_library_internal_compile.h", 0x2ae,
                   true);
    system_exit(-1);
  }

  while (argument_index != -1) {
    valid = true;
    node = (char *)datum_get(*(data_t **)0x5aa6c8, argument_index);

    if (*(int *)0x46b6fc != 0) {
      display_assert("!hs_compile_globals.error",
                     "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x48e, true);
      system_exit(-1);
    }

    if (*(int16_t *)(node + 0x4) == 0) {
      *(int16_t *)(node + 0x4) = 9; /* _hs_type_string */
      node2 = (char *)datum_get(*(data_t **)0x5aa6c8, argument_index);
      if (*(uint8_t *)(node2 + 0x6) & 1) {
        *(int16_t *)(node + 0x2) = 9;
        valid = FUN_000c73a0(argument_index);
      } else {
        valid = FUN_000c74c0(argument_index);
      }
    }

    node = (char *)datum_get(*(data_t **)0x5aa6c8, argument_index);
    argument_index = *(int *)(node + 0x8);
    if (!valid)
      return false;
  }

  return true;
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
  int expr_datum;
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

  expr_datum = FUN_000c7be0(&src_cursor);

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

  cursor = hs_compile_initialize(source_file_size, source_ptr);

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

/* 0xc95c0 — Boolean-inverting accessor. Reads the caller's byte argument and
 * returns 1 when it is zero, 0 otherwise.
 *
 * Binary evidence (0xc95c0..0xc95ce): the whole body is
 *   MOV CL,byte ptr [EBP+8] / XOR EAX,EAX / TEST CL,CL / SETZ AL / POP EBP /
 * RET so EAX is fully zeroed and only AL is set from the zero flag. Leaf: no
 * callees, no globals, no memory writes. Ghidra's decompile for this address
 * is stale (it models the function as void(void) with an empty body); the
 * disassembly above is authoritative.
 *
 * The single caller is FUN_000bdef0 (CALL at 0xbdf19). Semantic role is
 * unknown beyond the byte-inversion, so the name is left as FUN_000c95c0. */
unsigned char FUN_000c95c0(unsigned char value)
{
  return (unsigned char)(value == 0);
}

/* 0xc95d0 — forward one dword to the terminal overlay as a formatted line,
 * using the color global at 0x2ee6d4.
 *
 * Binary evidence (0xc95d0..0xc95e7), whole body:
 *   PUSH EBP / MOV EBP,ESP / MOV EAX,dword ptr [EBP+8] /
 *   MOV ECX,dword ptr [0x002ee6d4] / PUSH EAX / PUSH ECX /
 *   CALL 0x000e3a10 (terminal_output) / ADD ESP,0x8 / POP EBP / RET
 *
 * Exactly two stack args are pushed and cleaned (ADD ESP,0x8), so
 * terminal_output is entered with (color, format) only; the third parameter
 * in its kb.json declaration is the first formatting slot and is not
 * supplied at this site. Reached through a 2-arg function-pointer cast, the
 * same idiom already used for this callee in cheats.c
 * (cheat_teleport_to_camera).
 *
 * cdecl: the first PUSH is the last argument, so ECX (= *(void **)0x2ee6d4)
 * is the color and EAX (= param_1) is the format string.
 *
 * param_1 arrives from FUN_000bdf40 as the first dword of an HS macro
 * function result record. Its pointee type is unproven beyond being the
 * string terminal_output formats, so the kb.json declaration keeps int and
 * the cast is local. Semantic role is otherwise unknown, so the name is left
 * as FUN_000c95d0. */
void FUN_000c95d0(int param_1)
{
  typedef void (*terminal_output_2_t)(void *, const char *);

  ((terminal_output_2_t)terminal_output)(*(void **)0x2ee6d4,
                                         (const char *)param_1);
}

/* 0xc95f0
 *
 * Confirmed from the disassembly at 0xc95f0: cdecl, no parameters. The
 * FUN_000ce200 result is held in EDI across the loop and moved back into EAX
 * at 0xc9649, so the function returns it (kb.json decl already says int).
 *
 * Iterates the player table (global at 0x5aa6d4) with
 * data_next_index/datum_get. The original reloads [0x5aa6d4] before every
 * call (0xc95f9, 0xc9610, 0xc9632), which the raw-address form preserves.
 * For each player, the unit handle at +0x34 is loaded once into EAX
 * (0xc961d), tested against NONE, and passed as the second argument to
 * FUN_000ce2b0 together with the FUN_000ce200 result.
 *
 * player+0x34 as the controlled unit handle is corroborated by the same
 * offset use in scenario.c/units.c. The roles of FUN_000ce200 and
 * FUN_000ce2b0 are unproven, so both keep their FUN_ names and the local
 * holding the FUN_000ce200 result stays mechanically named. */
int FUN_000c95f0(void)
{
  int result;
  int player_index;
  int unit_handle;

  result = FUN_000ce200();
  for (player_index = data_next_index(*(data_t **)0x5aa6d4, -1);
       player_index != -1;
       player_index = data_next_index(*(data_t **)0x5aa6d4, player_index)) {
    unit_handle =
      *(int *)((char *)datum_get(*(data_t **)0x5aa6d4, player_index) + 0x34);
    if (unit_handle != -1)
      FUN_000ce2b0(result, unit_handle);
  }
  return result;
}

/* 0xc9700 — Locate the object's head position when it is a unit; otherwise
 * copy the position at +0x50, then test param_1 against that point and the
 * supplied angle scaled by the global at 0x253d4c. The parameter roles beyond
 * these mechanically observed uses are unproven. */
void FUN_000c9700(int param_1, int param_2, float param_3)
{
  vector3_t position;

  if (param_2 != -1) {
    if (object_try_and_get_and_verify_type(param_2, 3) != NULL)
      unit_get_head_position(param_2, (float *)&position);
    else
      position =
        *(vector3_t *)((char *)object_get_and_verify_type(param_2, -1) + 0x50);
    FUN_001aa430(param_1, (float *)&position, param_3 * *(float *)0x253d4c);
  }
}

/* 0xc98e0 — Report whether this object, any object in its child chain, or any
 * object in its parent chain is a player-controlled unit; failing that, whether
 * the object is one of the types in mask 0x1c and has flag bit 1 set.
 *
 * Binary evidence (0xc98e0..0xc998a, cdecl, one stack arg at [EBP+8] read into
 * ESI at 0xc98e6):
 *
 *   PUSH -0x1 / PUSH ESI / CALL 0x13d680 => object_get_and_verify_type(handle,
 *   -1); the result is parked in EBX (0xc98f3) and is the base for every later
 *   field read.  PUSH ESI / CALL 0xba500 => player_index_from_unit_index(the
 *   same handle, NOT the object pointer).  The single ADD ESP,0xc at 0xc98fa
 *   folds both calls' pushes (2 + 1); the ARG_COUNT warning on 0xba500 is that
 *   merge — its decl really takes one arg.
 *
 *   CMP EAX,-0x1 / SETNZ AL / MOV [EBP-1],AL stores the "is a player" result in
 *   a stack byte, and JNZ 0xc9984 skips straight to the epilogue with AL still
 *   set, so a player hit returns true without touching either chain or the
 *   type/flag test.  The two loops and the type/flag test are therefore reached
 *   only when that byte is 0, which is why falling out of them returns false
 *   (MOV AL,[EBP-1] at 0xc9937/0xc9967 reloads the stored zero).
 *
 *   Child loop (0xc9915..0xc9935): starts at [EBX+0xc8] (unk_200), resolves the
 *   handle, RECURSES on 0xc98e0 with the child HANDLE (PUSH ESI at 0xc991d, not
 *   the resolved pointer in EDI), returns true on a nonzero AL, and advances
 *   through [EDI+0xc4] (next_object_index) — the resolved child, not the base
 *   object.
 *
 *   Parent loop (0xc9945..0xc9965): starts at [EBX+0xcc], calls
 *   player_index_from_unit_index on the parent HANDLE, returns true when it is
 *   not NONE, and advances through [EDI+0xcc].  CMP ESI,EAX at 0xc9963 compares
 *   against EAX, which is -1 on that path, so it is the same NONE test.
 *
 *   Tail (0xc996a..0xc9982): MOV CL,byte ptr [EBX+0x64] is a BYTE read of the
 *   object type, then MOV EDX,1 / SHL EDX,CL / TEST DL,0x1c — x86 masks the
 *   shift count to 5 bits and only the low byte is tested, so the C form keeps
 *   both the & 31 and the 0x1c mask.  TEST byte ptr [EBX+0x1a4],0x2 is one byte
 *   past the end of object_data_t (size 0x1a4), so it stays an explicit offset
 *   rather than a guessed field; the same raw form is already used for +0x1a4
 * in items.c, weapons.c, and game_engine.c.
 *
 * Callees (both cdecl, both in kb.json, no @<reg> args):
 *   0x13d680 = object_get_and_verify_type(int datum_handle, int type_mask)
 *   0xba500  = player_index_from_unit_index(int unit_index)
 *
 * Callers (0xc99ed in FUN_000c99e0, 0xc9adc in FUN_000c9a50, plus the self
 * recursion) each push a single dword handle, which is what fixes the kb.json
 * decl from void(void) to bool(int); the recursive site's TEST AL,AL proves the
 * bool return.  Whether the mask-0x1c/flag-bit-1 tail means "item is held or at
 * rest" is unproven, so the name is left as FUN_000c98e0. */
bool FUN_000c98e0(int object_handle)
{
  object_data_t *object;
  object_data_t *linked_object;
  int linked_handle;
  bool is_player;

  object = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  is_player = (bool)(player_index_from_unit_index(object_handle) != -1);
  if (is_player)
    return is_player;

  linked_handle = object->unk_200.value;
  while (linked_handle != -1) {
    linked_object =
      (object_data_t *)object_get_and_verify_type(linked_handle, -1);
    if (FUN_000c98e0(linked_handle))
      return true;
    linked_handle = linked_object->next_object_index.value;
  }

  linked_handle = object->parent_object_index.value;
  while (linked_handle != -1) {
    linked_object =
      (object_data_t *)object_get_and_verify_type(linked_handle, -1);
    if (player_index_from_unit_index(linked_handle) != -1)
      return true;
    linked_handle = linked_object->parent_object_index.value;
  }

  if ((1U << (*(uint8_t *)((char *)object + 0x64) & 31) & 0x1c) != 0 &&
      (*(uint8_t *)((char *)object + 0x1a4) & 2) != 0)
    return true;

  return is_player;
}

/* Reject deletion of a player or its linked object; otherwise delete datum. */
void FUN_000c99e0(int datum)
{
  if (datum != -1) {
    if (!FUN_000c98e0(datum)) {
      object_delete(datum);
      return;
    }
    error(2, "### ERROR a script tried to delete the player (or the horse he "
             "rode in on, or his six-shooter)");
  }
}

/* 0xc9b10 — Invoke `iterator` once for every scenario object-name whose text
 * contains `substring`.
 *
 * Binary evidence (0xc9b10..0xc9b82, cdecl).  The one stack argument at
 * [EBP+8] is the callback; the substring arrives in EBX, which this function
 * never writes — it is only PUSHed as crt_strstr's second argument at 0xc9b60.
 * All three callers prove the register argument by doing the same thing:
 *   0xc9b90  MOV EBX,[EBP+8] / PUSH offset 0xc9990 / CALL 0xc9b10
 *   0xc9bb0  MOV EBX,[EBP+8] / PUSH offset 0xc9a20 / CALL 0xc9b10
 *   0xca140  MOV EBX,[EBP+8] / PUSH offset 0xca110 / CALL 0xc9b10
 *
 *   CALL 0x18e380 (global_scenario_get) at 0xc9b15 runs BEFORE the callback
 *   null test at 0xc9b1c, so the scenario pointer is an initialised
 *   declaration rather than part of the guarded body.
 *
 *   The failing test falls into display_assert("iterator",
 *   "c:\halo\SOURCE\hs\hs_library_external.c", 0x197, true) — the __FILE__
 *   string at 0x280408 puts this routine's source in hs_library_external.c
 *   even though the .obj partition keeps it with hs_runtime.  The terminal is
 *   PUSH -1 / CALL 0x8e2f0; 0x8e2f0 is a JMP thunk onto 0x1029a0, so the
 *   source form is the arg-passing system_exit(-1) and NOT the arg-less
 *   halt_and_catch_fire() that the delinked reloc name suggests.  The single
 *   ADD ESP,0x14 at 0xc9b40 folds display_assert's 4 args and system_exit's 1.
 *
 *   MOV EAX,[ESI+0x204] reads the block count before ADD ESI,0x204 rebases
 *   ESI, and the loop tail re-reads MOV ECX,[ESI]; the block at scenario+0x204
 *   with 0x24-byte elements is the object-names block already established by
 *   FUN_0018ea50 in scenario.c.
 *
 *   The loop counter is 16-bit: INC EDI / MOVSWL EAX,DI / CMP EAX,ECX.  PUSH
 *   EDI hands it to the callback, and both known callback targets (0xc9990,
 *   0xca110) are declared int16_t-taking.
 *
 *   ADD ESP,0x14 at 0xc9b67 folds tag_block_get_element's 3 pushes and
 *   crt_strstr's 2.  PUSH EBX before PUSH EAX makes the block element the
 *   haystack and the register argument the needle; the CALL at 0xc9b62
 *   resolves to 0x1d9690 (crt_strstr). */
void hs_object_iterate_names_containing(hs_object_name_iterator_t iterator,
                                        const char *substring)
{
  char   *scenario;
  char   *object_names;
  int16_t index;

  scenario = (char *)global_scenario_get();
  if (iterator == NULL) {
    display_assert("iterator", "c:\\halo\\SOURCE\\hs\\hs_library_external.c",
                   0x197, true);
    system_exit(-1);
  }

  object_names = scenario + 0x204;
  for (index = 0; index < *(int *)object_names; index++) {
    if (crt_strstr((const char *)tag_block_get_element(object_names, index,
                                                       0x24),
                   substring) != NULL)
      iterator(index);
  }
}

/* 0xc9b90 — Run FUN_000c9990 over every scenario object-name containing the
 * caller's string.  Confirmed at 0xc9b90..0xc9b96: MOV EBX,[EBP+8] loads the
 * one stack argument into the register slot that 0xc9b10 reads, PUSH offset
 * FUN_000c9990 supplies the callback, and ADD ESP,4 cleans the single push.
 * EAX is never consumed after the CALL, so the function is void. */
void FUN_000c9b90(int substring)
{
  hs_object_iterate_names_containing(FUN_000c9990, (const char *)substring);
}

/* 0xc9bb0 — Same shape as 0xc9b90 (MOV EBX,[EBP+8]; PUSH offset FUN_000c9a20;
 * CALL 0xc9b10; ADD ESP,4) with FUN_000c9a20 as the callback.  FUN_000c9a20's
 * kb.json decl was void(void); its own body reads the caller's argument as a
 * 16-bit value at [ESP+4] (Ghidra: in_stack_00000004, a short compared against
 * -1), which the shared callback type fixes to int16_t. */
void FUN_000c9bb0(int substring)
{
  hs_object_iterate_names_containing(FUN_000c9a20, (const char *)substring);
}

/* 0xc9bd0 — Return the object handle `index` steps into an HS object list,
 * or NONE when the list runs out first.
 *
 * Binary evidence (0xc9bd0..0xc9c0d, cdecl, two stack args, one dword local
 * at [EBP-4]):
 *
 *   The list handle is held in EDI (MOV EDI,[EBP+8] at 0xc9bd6) and is passed
 *   to both iterator calls; [EBP-4] is the iterator cursor, and FUN_000ce450
 *   writes exactly one dword through it (see FUN_000ce450 below), so a single
 *   int local is the correct buffer — not an array.
 *
 *   MOV ESI,[EBP+0xc] loads the whole dword but every subsequent use is 16-bit
 *   (TEST SI,SI at 0xc9bd9 and 0xc9c03), which is the ordinary MSVC shape for
 *   a short parameter: the argument slot is 4 bytes wide, so only the low half
 *   is defined.  Both call sites zero-extend a 16-bit record field into the
 *   slot (0xbe38c: XOR EDX,EDX / MOV DX,[EAX+4] / PUSH EDX), so the value is
 *   always small and the signedness cannot be observed.
 *
 *   The advance is guarded BEFORE it runs (CMP EAX,-1 / JE at the top of the
 *   loop body), so a list whose first element is NONE returns NONE without
 *   calling FUN_000ce320, and the value left in EAX at the epilogue is the
 *   return — the caller at 0xbe39b consumes it with PUSH EAX into hs_return. */
int FUN_000c9bd0(int object_list, short index)
{
  int   iterator;
  int   object_handle;
  short remaining;

  object_handle = FUN_000ce450(object_list, &iterator);
  for (remaining = index; remaining > 0; remaining--) {
    if (object_handle == -1)
      break;
    object_handle = FUN_000ce320(object_list, &iterator);
  }
  return object_handle;
}

/* 0xc9c10 — Scale the object's field at +0x94 by a caller-supplied fraction
 * clamped to [0,1] times the object's field at +0x8c.
 *
 * Binary evidence (0xc9c10..0xc9c70, cdecl, no frame locals):
 *
 *   The second argument is a float, not a pointer: the caller at 0xbe30c does
 *   FLD [EAX+4] / PUSH ECX / FSTP [ESP] — the classic MSVC push-then-fstp, so
 *   the pushed ECX is a dummy and the real argument is the FPU value.
 *
 *   FCOM against the .rdata constants at 0x2533c0 (0.0f) and 0x2533c8 (1.0f).
 *   The first branch is FNSTSW/TEST AH,5/JP: (AH & 5) is 0 for greater-or-
 *   equal and 5 for unordered (both even parity, jump taken) and 1 for less
 *   (odd parity, fall through), so the fall-through path is exactly x < 0.0f
 *   and replaces the value with 0.0f.  The second is TEST AH,0x41/JNE, which
 *   jumps when C0 or C3 is set (x <= 1.0f) and otherwise replaces the value
 *   with 1.0f.  MSVC duplicates the multiply/store tail into the first branch
 *   because the x87 value has to be reloaded there.
 *
 *   +0x8c and +0x94 are object_data_t.unk_140 / unk_148; types.h already cites
 *   this exact function (.text:000C9C40 / 000C9C46) for both fields.  Their
 *   meaning (a vitality or charge pair) is unproven, so the mechanical field
 *   names and the FUN_ name are kept.
 *
 * VC71 87.1%, with an [IMM-WARN] on 0x3f800000.  The warning is a
 * materialisation difference, not a wrong literal: 1.0f IS in the reference,
 * loaded as FLD [0x2533c8] from the constant pool, whereas cl.exe spills
 * `fraction = 1.0f` into the parameter slot (MOV [EBP+0xc],3f800000h) and
 * reloads it.  Rewriting the clamp as a three-way if/else-if/else to avoid the
 * spill was measured and made it WORSE (83.1%, 34 insns vs 31), so the
 * assignment form is kept.  The residual gap is FCOMP+reload versus the
 * original's FCOM-without-pop, which keeps the value live in ST(0). */
void FUN_000c9c10(int object_handle, float fraction)
{
  object_data_t *object;

  if (object_handle != -1) {
    object = (object_data_t *)object_get_and_verify_type(object_handle, -1);
    if (fraction < 0.0f)
      object->unk_148 = 0.0f * object->unk_140;
    else {
      if (fraction > 1.0f)
        fraction = 1.0f;
      object->unk_148 = fraction * object->unk_140;
    }
  }
}

/* 0xc9c80 — Resolve a region name against the object's model tag and apply a
 * permutation to it; an empty region name applies to every region (NONE).
 *
 * Binary evidence (0xc9c80..0xc9d32, cdecl, three stack args, one dword local
 * at [EBP-4] holding the region index):
 *
 *   MOV DWORD PTR [EBP-4],-1 is stored before the csstrcmp result is tested,
 *   so the NONE seed is unconditional.
 *
 *   The csstrcmp second argument is the address 0x25386f, which is the NUL
 *   terminator of the preceding literal — i.e. the empty string.  A zero
 *   result (name is empty) skips the whole lookup and leaves the index NONE.
 *
 *   ADD ESP,0x18 at 0xc9cae folds three calls' pushes: object_get_and_verify_
 *   type (2), tag_get (2) and csstrcmp (2).
 *
 *   [obje_tag+0x34] is the model tag index; it is loaded once at 0xc9cb5 and
 *   reused as tag_get's argument, and the regions tag_block is at
 *   model_tag+0xc4 with 0x4c-byte elements.  The comparison call at 0xc9cfb
 *   resolves to 0x1dd801 (crt_stricmp), NOT csstricmp at 0x8e190; the argument
 *   order is (element, region_name) because EBX is pushed first.
 *
 *   The loop counter is 16-bit (INC EDI / MOVSWL EAX,DI / CMP EAX,ECX) and a
 *   match stores EDI into [EBP-4] and leaves the loop.
 *
 *   The tail always runs object_permute_region(handle, arg3, region_index, 1)
 *   — PUSH 1 / PUSH [EBP-4] / PUSH [EBP+0x10] / PUSH [EBP+8], ADD ESP,0x10. */
void FUN_000c9c80(int object_handle, int region_name, int permutation_name)
{
  char *object_tag;
  char *model_tag;
  char *regions;
  int   model_index;
  short region_index;
  short i;

  if (object_handle == -1)
    return;

  object_tag = (char *)tag_get(
    0x6f626a65 /* 'obje' */,
    (int)((object_data_t *)object_get_and_verify_type(object_handle, -1))
      ->tag_index);
  region_index = -1;
  if (csstrcmp((const char *)region_name, "") != 0) {
    model_index = *(int *)(object_tag + 0x34);
    if (model_index != -1) {
      model_tag = (char *)tag_get(0x6d6f6465 /* 'mode' */, model_index);
      regions = model_tag + 0xc4;
      for (i = 0; i < *(int *)regions; i++) {
        if (crt_stricmp((const char *)tag_block_get_element(regions, i, 0x4c),
                        (const char *)region_name) == 0) {
          region_index = i;
          break;
        }
      }
    }
  }
  object_permute_region(object_handle, (const char *)permutation_name,
                        region_index, 1);
}

/* 0xc9d40 — Walk an HS object list and hand every member handle to
 * FUN_0013ddd0.
 *
 * Binary evidence (0xc9d40..0xc9d6c, cdecl, one stack arg, one dword local at
 * [EBP-4]): the list handle stays in ESI across both iterator calls, the
 * cursor is the single dword local, and the loop is the ordinary
 * first/next/NONE walk.  ADD ESP,0xc at 0xc9d60 folds FUN_0013ddd0's one push
 * and FUN_000ce320's two.
 *
 * kb.json previously modelled this as void(void); the caller at 0xbeb8c does
 * MOV EDX,[EAX] / PUSH EDX / CALL, which is what fixes the single argument. */
void FUN_000c9d40(int object_list)
{
  int iterator;
  int object_handle;

  object_handle = FUN_000ce450(object_list, &iterator);
  while (object_handle != -1) {
    FUN_0013ddd0(object_handle);
    object_handle = FUN_000ce320(object_list, &iterator);
  }
}

/* 0xc9d80 — Delete every live object whose definition tag index matches the
 * caller's, then run FUN_00145490.
 *
 * Binary evidence (0xc9d80..0xc9dc0, cdecl, one stack arg, 0x10 bytes of
 * locals):
 *
 *   SUB ESP,0x10 with LEA EAX,[EBP-0x10] passed to object_iterator_new is one
 *   contiguous object_iter_t, so the later MOV EDX,[EBP-8] is NOT a separate
 *   local — [EBP-8] is iter+0x8, object_iter_t.last_handle.  Reading it as an
 *   independent variable is the buffer-alias trap.
 *
 *   PUSH 0 / PUSH -1 / PUSH &iter gives object_iterator_new(&iter, -1, 0):
 *   type mask NONE, i.e. every object type.
 *
 *   CMP [EAX],ESI compares object_data_t.tag_index against the argument held
 *   in ESI, and ADD ESP,0x10 at 0xc9d8c folds object_iterator_new's 3 pushes
 *   and the first object_iterator_next's 1.
 *
 *   The trailing CALL 0x145490 is outside the loop and outside the
 *   `first != NULL` guard — it runs even when nothing was deleted.
 *
 * The hazard scan invoked by tools/build/build.py reports a 16-byte frame gap
 * here (a bare `check_lift_hazards.py` run does not — it selects a different
 * file set, so do not conclude the finding is stale).  It is a detector blind
 * spot: _sum_locals measures arrays, scalars and struct ARRAYS, so the bare
 * `object_iter_t iterator` counts as 0 even though types.h asserts
 * cs(object_iter_t, 0x10) — exactly the original SUB ESP,0x10. */
void FUN_000c9d80(int tag_index)
{
  object_iter_t  iterator;
  object_data_t *object;

  object_iterator_new(&iterator, -1, 0);
  object = (object_data_t *)object_iterator_next(&iterator);
  while (object != NULL) {
    if ((int)object->tag_index == tag_index)
      object_delete(iterator.last_handle);
    object = (object_data_t *)object_iterator_next(&iterator);
  }
  FUN_00145490();
}

/* 0xc9de0 — Spawn an unattached effect at a scenario cutscene-flag: the flag's
 * point at +0x24 is the marker position and its angles at +0x30 are converted
 * into the marker forward vector.
 *
 * Binary evidence (0xc9de0..0xc9e32, cdecl, two stack args, 0xc bytes of
 * locals = one vector3_t):
 *
 *   MOVSWL EAX,[EBP+0xc] is a SIGNED 16-bit load, so the second parameter is a
 *   short — kb.json previously said uint16_t.  Both call sites zero-extend a
 *   record field into the slot (0xbe41c: XOR EDX,EDX / MOV DX,[EAX+4]), so the
 *   two readings cannot diverge for real flag indices.
 *
 *   PUSH 0x5c / PUSH EAX happen BEFORE CALL global_scenario_get, which takes
 *   no arguments — Ghidra's cdecl arg mis-grouping.  They belong to
 *   tag_block_get_element(scenario+0x4e4, flag_index, 0x5c), whose block
 *   pointer is only formed afterwards by ADD EAX,0x4e4.
 *
 *   MOV ECX,[0x31fc38] loads the pointer value of global_zero_vector_ptr, so
 *   the effect gets a zero translational velocity.
 *
 *   ADD ESP,0x44 at 0xc9e2b folds all three calls: tag_block_get_element (3),
 *   angles_to_vector (2) and effect_new_unattached_from_markers (12).  The two
 *   0x3f800000 immediates are the float scale arguments and the two PUSH 0
 *   immediately after them are the two trailing float arguments. */
void FUN_000c9de0(int effect_tag_index, short flag_index)
{
  vector3_t forward;
  char     *flag;

  flag = (char *)tag_block_get_element((char *)global_scenario_get() + 0x4e4,
                                       flag_index, 0x5c);
  angles_to_vector((float *)&forward, (float *)(flag + 0x30));
  effect_new_unattached_from_markers(effect_tag_index, -1,
                                     global_zero_vector_ptr, 1, NULL,
                                     (float *)(flag + 0x24), (float *)&forward,
                                     1.0f, 1.0f, 0.0f, 0.0f, 1);
}

/* 0xc9e50 — Spawn an effect attached to a named marker on an object.
 *
 * Binary evidence (0xc9e50..0xc9ea1, cdecl, three stack args, 0x6c bytes of
 * locals):
 *
 *   SUB ESP,0x6c is exactly one object marker record: FUN_00140f10
 *   (object_get_markers_by_string_id) writes +0x00, +0x04.. and a 13-dword
 *   block at +0x38, i.e. through +0x6b.  The later LEA [EBP-0xc] and
 *   LEA [EBP-0x30] are therefore marker+0x60 and marker+0x3c, NOT independent
 *   locals — the buffer-alias trap.  MOV EDX,[EBP-0x6c] is likewise marker+0.
 *
 *   The two NONE guards are nested (CMP EDI,-1 then CMP ESI,-1) and the marker
 *   lookup result is tested 16-bit (TEST AX,AX), matching its int16_t decl.
 *
 *   LEA ECX,[EBP+0x10] takes the ADDRESS of the third parameter's stack slot
 *   and passes it as the definition/name-array argument, with marker count 1 —
 *   the same slot whose value was passed by value to the marker lookup.
 *
 *   ADD ESP,0x30 at 0xc9e99 covers effect_new_attached_from_markers' 12 pushes
 *   alone; the marker lookup's 4 were already cleaned by ADD ESP,0x10. */
void FUN_000c9e50(int effect_tag_index, int object_handle, int marker_name)
{
  char marker[0x6c];

  if (effect_tag_index != -1) {
    if (object_handle != -1) {
      if (object_get_markers_by_string_id(object_handle, (void *)marker_name,
                                          marker, 1) != 0) {
        effect_new_attached_from_markers(
          effect_tag_index, -1, object_handle, *(int *)marker, 1, &marker_name,
          (float *)(marker + 0x60), (float *)(marker + 0x3c), 1.0f, 1.0f, 0.0f,
          0.0f);
      }
    }
  }
}

/* 0xc9ec0 — Apply a damage effect at a scenario cutscene-flag position.
 *
 * Binary evidence (0xc9ec0..0xc9f18, cdecl, two stack args, 0x54 bytes of
 * locals):
 *
 *   SUB ESP,0x54 matches damage_data_new exactly — FUN_00136750 memsets 0x54
 *   bytes through its first argument — so the frame is one damage-parameter
 *   block and every [EBP-N] below is a field of it, not a separate local.
 *
 *   MOVSWL EAX,[EBP+0xc] is a signed 16-bit load (kb.json said int); as in
 *   0xc9de0 the PUSH 0x5c / PUSH EAX pair precedes the argument-less
 *   global_scenario_get call and belongs to tag_block_get_element.
 *
 *   Store offsets, taken from the raw MOV [EBP-N] operands rather than the
 *   decompiler (params base = EBP-0x54):
 *     params+0x14  <- scenario_location_from_point output (LEA [EBP-0x40])
 *     params+0x28/0x2c/0x30 <- flag+0x24 .x/.y/.z  (MOV [EBP-0x2c/-0x28/-0x24])
 *     params+0x1c/0x20/0x24 <- flag+0x24 .x/.y/.z  (MOV [EBP-0x38/-0x34/-0x30])
 *   The +0x28 group is emitted first and the +0x1c group re-uses the SAME three
 *   registers rather than re-loading flag+0x24, so the second copy's source is
 *   params+0x28, not the flag.  Sourcing both copies from the flag compiled to
 *   three extra loads (88.4%, 45 insns vs 41); chaining the second copy off the
 *   first is what reaches 100%.
 *
 *   ADD ESP,0x24 at 0xc9f11 folds tag_block_get_element (3), damage_data_new
 *   (2), scenario_location_from_point (2) and FUN_00138e30 (2). */
void FUN_000c9ec0(int damage_effect_tag_index, short flag_index)
{
  char  damage_params[0x54];
  char *flag;

  flag = (char *)tag_block_get_element((char *)global_scenario_get() + 0x4e4,
                                       flag_index, 0x5c);
  damage_data_new(damage_params, damage_effect_tag_index);
  *(vector3_t *)(damage_params + 0x28) = *(vector3_t *)(flag + 0x24);
  *(vector3_t *)(damage_params + 0x1c) = *(vector3_t *)(damage_params + 0x28);
  scenario_location_from_point(damage_params + 0x14, flag + 0x24);
  FUN_00138e30(damage_params, -1);
}

/* 0xc9f30 — Apply a damage effect to an object at its own world position.
 *
 * Binary evidence (0xc9f30..0xc9f7e, cdecl, two stack args, 0x54 bytes of
 * locals — again exactly damage_data_new's memset size):
 *
 *   Only the second parameter is NONE-guarded (CMP ESI,-1 at 0xc9f3a); the
 *   effect tag index is used unchecked.
 *
 *   object_get_world_position writes into params+0x1c (LEA [EBP-0x38]), and
 *   the three dwords are then re-loaded FROM params+0x1c and stored to
 *   params+0x28/0x2c/0x30 (MOV [EBP-0x2c/-0x28/-0x24]) — the copy's source is
 *   the buffer field, not a separate local.
 *
 *   scenario_location_from_point(params+0x14, params+0x1c) follows, then
 *   object_cause_damage(params, object_handle, -1, -1, -1, NULL).  ADD ESP,
 *   0x30 folds damage_data_new (2), object_get_world_position (2),
 *   scenario_location_from_point (2) and object_cause_damage (6). */
void FUN_000c9f30(int damage_effect_tag_index, int object_handle)
{
  char damage_params[0x54];

  if (object_handle != -1) {
    damage_data_new(damage_params, damage_effect_tag_index);
    object_get_world_position(object_handle,
                              (vector3_t *)(damage_params + 0x1c));
    *(vector3_t *)(damage_params + 0x28) =
      *(vector3_t *)(damage_params + 0x1c);
    scenario_location_from_point(damage_params + 0x14, damage_params + 0x1c);
    object_cause_damage(damage_params, object_handle, -1, -1, -1, NULL);
  }
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

/* 0xcada0 — Find an HS thread whose script index (at +4) matches the given
 * index. Iterates hs_thread_data; returns the matching datum handle or -1. */
int FUN_000cada0(int16_t script_index)
{
  int datum_index;

  datum_index = data_next_index(*(data_t **)0x5aa6c4, -1);
  while (datum_index != -1) {
    char *thread = (char *)datum_get(*(data_t **)0x5aa6c4, datum_index);
    if (*(int *)(thread + 0x4) == (int)script_index)
      return datum_index;
    datum_index = data_next_index(*(data_t **)0x5aa6c4, datum_index);
  }
  return -1;
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

/* 0xcb980 — Return the script name of the currently executing HS thread,
 * or "[unknown]" if no thread is currently running.
 *
 * Reads hs_runtime_globals.current_thread (int16_t at 0x46b812).  If -1, no
 * thread is executing and the fallback string is returned.  Otherwise passes
 * the index (sign-extended) to hs_get_thread_script_name and returns that
 * result, or "[unknown]" if it returns NULL.
 *
 * Key globals:
 *   0x46b812 = hs_runtime_globals.current_thread (int16_t, -1 = none)
 */
const char *hs_runtime_get_executing_thread_name(void)
{
  int16_t current_thread;
  const char *name;

  current_thread = *(int16_t *)0x46b812;
  if (current_thread == -1) {
    return "[unknown]";
  }
  name = (const char *)hs_get_thread_script_name((int)current_thread);
  if (name == NULL) {
    return "[unknown]";
  }
  return name;
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
static void hs_return(int thread_handle, int value)
{
  char *thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_handle);
  char *stack_ptr = *(char **)(thread + 0x10);
  int node_handle = *(int *)(stack_ptr + 0x4);
  char *node = (char *)datum_get(*(data_t **)0x5aa6c8, node_handle);
  int16_t actual_type;
  int16_t desired_type;
  int result;
  char *top_frame;
  char *cur_sp;

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
  desired_type = (int16_t) * (uint16_t *)(node + 0x4);
  result = hs_can_cast(thread_handle, actual_type, desired_type, value);
  top_frame = *(char **)(*(char **)(thread + 0x10));
  *(int32_t *)(*(int32_t **)(top_frame + 0x8)) = result;

  /* Pop the top stack frame: advance thread->stack_ptr to previous frame. */
  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_handle);
  cur_sp = *(char **)(thread + 0x10);
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
    hs_return(thread_handle, *(int *)result);
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
    char *node;
    char *child;
    *arg_index = 0;
    node = (char *)datum_get(*(data_t **)0x5aa6c8,
                             *(int *)(*(char **)(thread + 0x10) + 0x4));
    child = (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
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
    char *name;
    if (*(int16_t *)(expr + 0x4) !=
        *(int16_t *)(formal_params + (int)*arg_index * 2)) {
      datum_get(*(data_t **)0x5aa6c4, thread_datum);
      name = hs_get_thread_script_name(thread_datum);
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
 * from the function descriptor table.
 * Returns FUN_000cc3a0's result — callers (e.g. ai_allegiance at 0xc06b0)
 * read EAX after this call to get the evaluated script value. */
int hs_macro_function_evaluate(int16_t function_index, int thread_datum,
                               char init)
{
  char *desc = (char *)hs_function_table_get(function_index);
  return FUN_000cc3a0(thread_datum, *(int16_t *)(desc + 0x18),
                      (int)(desc + 0x1a), init);
}

/* The per-script-function evaluators live in the header the binary says
 * they lived in; it is included here, at the point they occupied, so this
 * translation unit is unchanged. See that header for the evidence. */
#include "hs_library_internal_runtime.h"

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
          hs_return(thread_handle, *(int *)result);
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

/* 0x000ce200 — allocate a new object-list header.
 *
 * Confirmed (0xce200-0xce236): data_new_at_index(*(data_t **)0x5aa698); on
 * success the header is re-fetched with datum_get (the original reloads
 * [0x5aa698] at 0xce216) and two fields are initialised:
 *   header+0x6 (word) = 0    ; entry count, read back by FUN_000ce420
 *   header+0x8 (dword) = -1  ; head reference link, matches FUN_000ce450
 * header+0x4 (reference count) is left as the allocator zeroed it.
 * Returns the header datum index in EAX (ESI holds it across the call).
 * The role beyond these mechanical observations is unproven, so the name
 * stays FUN_000ce200 (its five call sites already use that name).
 *
 * noinline: the original build compiled the object-list family in a separate
 * TU (the asserts name c:\halo\SOURCE\hs\object_lists.c), so FUN_000c95f0 in
 * this file calls it instead of expanding it inline. */
__declspec(noinline) int FUN_000ce200(void)
{
  int list_index;
  char *list;

  list_index = data_new_at_index(*(data_t **)0x5aa698);
  if (list_index != -1) {
    list = (char *)datum_get(*(data_t **)0x5aa698, list_index);
    *(int16_t *)(list + 0x6) = 0;
    *(int *)(list + 0x8) = -1;
  }
  return list_index;
}

/* 0x000ce240 — object_list_delete
 *
 * Confirmed (0xce240-0xce2a4): NONE handle is a no-op; otherwise the header
 * is fetched from the header pool (0x5aa698) and its reference count
 * (header+0x4, word) must be zero — the assert string at 0x280ef0 is
 * "list->reference_count==0" with file 0x280f0c
 * "c:\halo\SOURCE\hs\object_lists.c" line 0x64 and halt=1, followed by
 * system_exit(-1). The reference chain starting at header+0x8 is released
 * through FUN_000ce110(reference_pool, first_reference) — argument order
 * confirmed by the pushes at 0xce28b/0xce28c (PUSH ECX = header+0x8,
 * PUSH EDX = [0x5aa694]) and by FUN_000ce110's own frame reads
 * ([EBP+0x8] = pool, [EBP+0xc] = index). The header datum is then deleted.
 * The single ADD ESP,0x10 at 0xce29e is MSVC's deferred cleanup for both
 * two-argument calls. */
void object_list_delete(int list_handle)
{
  char *list;

  if (list_handle != -1) {
    list = (char *)datum_get(*(data_t **)0x5aa698, list_handle);
    if (*(int16_t *)(list + 0x4) != 0) {
      display_assert("list->reference_count==0",
                     "c:\\halo\\SOURCE\\hs\\object_lists.c", 0x64, 1);
      system_exit(-1);
    }
    FUN_000ce110(*(data_t **)0x5aa694, *(int *)(list + 0x8));
    datum_delete(*(data_t **)0x5aa698, list_handle);
  }
}

/* 0x000ce2b0 — push one object handle onto an object list.
 *
 * Confirmed (0xce2b0-0xce31d): the header is fetched first (ESI), then a
 * reference datum is allocated from the reference pool 0x5aa694 (EBX holds
 * the pool pointer across both later uses at 0xce2df and 0xce305).
 * On success the new reference is linked at the head of the chain:
 *   reference+0x4 = object handle ([EBP+0xc])
 *   reference+0x8 = old header+0x8
 *   header+0x8    = new reference index
 * On allocation failure the original reports
 *   error(2, "WARNING: maximum %ss per map (%d) exceeded.",
 *         pool->name, pool->maximum_count)
 * — PUSH EBX is the pool pointer, which is &pool->name (data_t starts with
 * char name[32]), and MOVSX EDX,word [EBX+0x20] is pool->maximum_count.
 * Both paths increment the entry count header+0x6 (0xce2f7 and 0xce315);
 * the original tail-duplicated the increment, the single increment here is
 * the equivalent source form. Header/reference offsets are unproven beyond
 * these accesses, so the function keeps its FUN_ name. */
void FUN_000ce2b0(int param_1, int param_2)
{
  char *list;
  data_t *references;
  int reference_index;
  char *reference;

  list = (char *)datum_get(*(data_t **)0x5aa698, param_1);
  references = *(data_t **)0x5aa694;
  reference_index = data_new_at_index(references);
  if (reference_index != -1) {
    reference = (char *)datum_get(references, reference_index);
    *(int *)(reference + 0x4) = param_2;
    *(int *)(reference + 0x8) = *(int *)(list + 0x8);
    *(int *)(list + 0x8) = reference_index;
  } else {
    error(2, "WARNING: maximum %ss per map (%d) exceeded.", references->name,
          references->maximum_count);
  }
  *(int16_t *)(list + 0x6) += 1;
}

/* 0x000ce320 — object_list_iterator_next
 * Advances an object-list iterator to the next entry.
 * Returns the object datum handle, or -1 if the list is exhausted.
 * Updates *iter_state to point to the next node's link.
 *
 * Confirmed: datum_get(0x5aa694, *iter_state) at 0xce335.
 * Confirmed: node+0x8 = next link, node+0x4 = object handle.
 *
 * noinline (VC71 verification only): the original build emits this out of line
 * and its callers CALL it — 0xc9bd0, 0xc9d40 and the 0xce450 pair all carry
 * real relocs to 0xce320.  Because our TU has the body in scope, cl.exe inlines
 * the whole thing into those callers (the 0x5aa694 datum_get and the node+0x4 /
 * node+0x8 loads show up in their codegen, none of which the reference
 * contains), which alone held FUN_000c9bd0 at 53.2% (49 insns vs 30) and
 * FUN_000c9d40 at 56.7% (41 vs 26).
 *
 * The guard is `_MSC_VER && !__clang__` because our clang build targets
 * i386-pc-win32 and therefore also defines _MSC_VER; this must apply to cl.exe
 * ONLY and must never change the shipped binary's codegen.
 */
#if defined(_MSC_VER) && !defined(__clang__)
__declspec(noinline)
#endif
int FUN_000ce320(int param_1, int *param_2)
{
  char *node;

  if (*param_2 != -1) {
    node = (char *)datum_get(*(data_t **)0x5aa694, *param_2);
    *param_2 = *(int *)(node + 8);
    return *(int *)(node + 4);
  }
  return -1;
}

/* 0xce350 */
void FUN_000ce350(int expression_datum)
{
  if (expression_datum != -1) {
    char *node = (char *)datum_get(*(data_t **)0x5aa698, expression_datum);
    *(int16_t *)(node + 0x4) += 1;
  }
}

/* 0xce370 — Decrement the reference count of an object list entry.
 * Asserts that the count is > 0 before decrementing.
 * Source: object_lists.c line 0xa5.
 *
 * Globals:
 *   0x5aa698 = hs_object_list_data (data_t*)
 */
void FUN_000ce370(int expression_datum)
{
  if (expression_datum != -1) {
    char *node = (char *)datum_get(*(data_t **)0x5aa698, expression_datum);
    if (*(int16_t *)(node + 0x4) < 1) {
      display_assert("list->reference_count>0",
                     "c:\\halo\\SOURCE\\hs\\object_lists.c", 0xa5, 1);
      system_exit(-1);
    }
    *(int16_t *)(node + 0x4) -= 1;
  }
}

/* 0x000ce3c0 — delete every object list whose reference count is zero.
 *
 * Confirmed (0xce3c0-0xce410): walks the header pool 0x5aa698 with
 * data_next_index starting from -1; for each header, datum_get is used to
 * test header+0x4 (word reference count) against zero and object_list_delete
 * is called on the index when it is zero. The original advances with
 * data_next_index(pool, index) using the same index it just deleted — the
 * iteration order is preserved here rather than "fixed".
 * Its only caller is the still-unported runtime update routine at 0xcde00. */
void FUN_000ce3c0(void)
{
  int list_index;
  char *list;

  for (list_index = data_next_index(*(data_t **)0x5aa698, -1); list_index != -1;
       list_index = data_next_index(*(data_t **)0x5aa698, list_index)) {
    list = (char *)datum_get(*(data_t **)0x5aa698, list_index);
    if (*(int16_t *)(list + 0x4) == 0) {
      object_list_delete(list_index);
    }
  }
}

/* 0x000ce420 — read an object list's entry count.
 *
 * Confirmed (0xce420-0xce441): XOR EAX,EAX seeds the result with 0, and for
 * a non-NONE handle only AX is loaded from header+0x6 (MOV AX,word [EAX+6]),
 * so the result is a 16-bit value — kb.json declares int16_t and the single
 * caller (FUN_000be3b0 at 0xbe3b0) narrows it with (uint16_t). The upper
 * half of EAX is left holding datum_get's pointer in the original; only AX
 * is meaningful. */
int16_t FUN_000ce420(int param_1)
{
  int16_t count;
  char *list;

  count = 0;
  if (param_1 != -1) {
    list = (char *)datum_get(*(data_t **)0x5aa698, param_1);
    count = *(int16_t *)(list + 0x6);
  }
  return count;
}

/* 0x000ce450 — object_list_iterator_first
 * Initializes an object-list iterator and returns the first object handle.
 * Returns -1 if the list is empty or param_1 is -1.
 *
 * Confirmed: datum_get(0x5aa698, param_1) at 0xce466.
 * Confirmed: datum_get(0x5aa694, first_link) at 0xce483.
 * Confirmed: node+0x8 = head link (list entry), then node+0x8 = next, node+0x4
 * = handle.
 *
 * noinline (VC71 verification only) — same reason as FUN_000ce320 above: the
 * original emits a real CALL from 0xc9bd0 and 0xc9d40, while cl.exe inlines the
 * in-TU body.  Guarded `_MSC_VER && !__clang__` so it never affects the shipped
 * clang codegen.
 */
#if defined(_MSC_VER) && !defined(__clang__)
__declspec(noinline)
#endif
int FUN_000ce450(int param_1, int *param_2)
{
  char *node;
  int first_link;

  if (param_1 != -1) {
    node = (char *)datum_get(*(data_t **)0x5aa698, param_1);
    first_link = *(int *)(node + 8);
    *param_2 = first_link;
    if (first_link != -1) {
      node = (char *)datum_get(*(data_t **)0x5aa694, first_link);
      *param_2 = *(int *)(node + 8);
      return *(int *)(node + 4);
    }
  }
  return -1;
}
