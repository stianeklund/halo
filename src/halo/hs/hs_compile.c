/* 0xc57d0 — Search the HS source string table for a name. The source buffer
 * at 0x46b6ec is a packed sequence of null-terminated strings; returns the
 * byte offset of the matching string, or -1 if not found. */
int FUN_000c57d0(char *str)
{
  int offset;

  offset = 0;
  while (offset < *(int *)0x46b6f0) {
    if (csstrcmp(str, (const char *)(*(int *)0x46b6ec + offset)) == 0)
      return offset;
    offset += csstrlen((const char *)(*(int *)0x46b6ec + offset)) + 1;
  }

  return -1;
}

/* 0xc5840 — Resolve expression as a global variable reference. Looks up the
 * node's name via hs_find_global_by_name. If found, validates type
 * compatibility and sets the variable_ref flag (bit 2). If the node's type
 * is unparsed (0), propagates the global's type.
 */
bool FUN_000c5840(int datum_index)
{
  char *node;
  int16_t type;
  int global_ref;
  int16_t global_type;

  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  type = *(int16_t *)(node + 0x4);

  if ((type < 4 || type > 0x30) && type != 0) {
    display_assert(
      "hs_type_valid(expression->type) || expression->type==_hs_unparsed",
      "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x4e4, 1);
    system_exit(-1);
  }

  global_ref = (int)hs_find_global_by_name(
    (const char *)(*(int *)(node + 0xc) + *(int *)0x46b6e8));
  *(int *)(node + 0x10) = global_ref;

  if (global_ref == -1) {
    if (*(uint8_t *)0x46b808 == 0)
      return false;
    *(const char **)0x46b6fc = "this is not a valid variable name.";
    *(int *)0x46b700 = *(int *)(node + 0xc);
    return false;
  }

  global_type = hs_global_get_type((uint16_t) * (int16_t *)(node + 0x10));

  if (*(int16_t *)(node + 0x4) != 0 &&
      !hs_types_compatible(global_type, *(int16_t *)(node + 0x4))) {
    const char *global_name =
      hs_global_get_name((uint16_t) * (int16_t *)(node + 0x10));
    crt_sprintf(
      (char *)0x46b704,
      "i expected a value of type %s, but the variable %s has type %s",
      ((const char **)(void *)0x2f14a8)[(int)*(int16_t *)(node + 0x4)],
      global_name,
      ((const char **)(void *)0x2f14a8)[(int)(int16_t)global_type]);
    *(const char **)0x46b6fc = (const char *)0x46b704;
    *(int *)0x46b700 = *(int *)(node + 0xc);
    return false;
  }

  if (*(int16_t *)(node + 0x4) == 0) {
    *(int16_t *)(node + 0x4) = global_type;
  }
  *(uint8_t *)(node + 0x6) |= 4;
  return true;
}

/* 0xc5960 — Resolve the function/script index for an expression node.
 * Looks up the first child (predicate) at datum_index->field_0x10. If the
 * predicate is already type 2 (function call), copies its function_index.
 * Otherwise tries hs_find_function_by_name, then hs_find_script_by_name
 * on the predicate's name string. Sets the is_script flag (bit 1) if
 * resolved as a script.
 */
void FUN_000c5960(int datum_index)
{
  char *node;
  char *node2;
  char *predicate;

  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  node2 = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  predicate = (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node2 + 0x10));

  if (*(int16_t *)(predicate + 0x4) != 2) {
    int16_t fn_idx;
    char *name = (char *)(*(int *)(predicate + 0xc) + *(int *)0x46b6e8);

    fn_idx = hs_find_function_by_name(name);
    *(int16_t *)(node + 0x2) = fn_idx;
    *(int16_t *)(predicate + 0x4) = 2;

    if (*(int16_t *)(node + 0x2) == -1) {
      int16_t script_idx = hs_find_script_by_name(name);
      *(int16_t *)(node + 0x2) = script_idx;
      if (script_idx != -1) {
        *(uint8_t *)(node + 0x6) |= 2;
      }
    }

    *(int16_t *)(predicate + 0x2) = *(int16_t *)(node + 0x2);
    return;
  }

  if (*(int16_t *)(predicate + 0x2) == -1) {
    display_assert("predicate->function_index!=NONE",
                   "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x520, 1);
    system_exit(-1);
  }

  *(int16_t *)(node + 0x2) = *(int16_t *)(predicate + 0x2);
}

/* Compile an HS function-call expression node (0xc73a0).
 *
 * Called from hs_type_check when a syntax node has flag bit 0 set (function
 * expression node). Receives the node's datum_index in EDI (register arg).
 *
 * The node's type field (node+0x4, int16_t) drives the dispatch:
 *   0           — unparsed/unknown; fall through to dispatch table
 *   1           — special form placeholder; emit "expected script/var" error
 *   4           — void expression; emit "void in non-void slot" error, fail
 *  [4..0x30]   — typed expression; dispatch to table at 0x27bb80[type*4]
 *
 * If hs_compile_globals.validating (0x46b808) is 0 or the node has flag bit 2
 * set, first attempts to resolve the expression as a global variable reference
 * via FUN_000c5840. If that succeeds, returns the result directly.
 *
 * Otherwise dispatches through the function-pointer table at 0x27bb80[type*4]
 * (indexed by the expression's type). Each table entry is a cdecl function
 * taking datum_index and returning bool. If the table entry is NULL, emits an
 * "unsupported expression type" error using the type-name table at 0x2f14a8
 * and returns false.
 *
 * Returns true if compilation succeeded, false on error (sets
 * hs_compile_globals.error at 0x46b6fc and error offset at 0x46b700).
 */

/* 0xc72b0 — Skip whitespace and comments in the HS source buffer. Advances
 * the cursor past spaces/tabs (0x27bb78 table), newlines (0x27bb7c table),
 * single-line comments (;...newline), and block comments (;*...*;).
 * Sets error at 0x46b6fc for unterminated block comments. */
void FUN_000c72b0(char **cursor)
{
  char *p;
  char ch;
  int16_t state;
  int16_t i;

  state = 0;
  do {
    switch (state) {
    case 0:
      p = *cursor;
      ch = *p;
      if (ch == ';') {
        *cursor = p + 1;
        state = 1;
        if (p[1] == '*') {
          state = 2;
          *cursor = p + 2;
        }
        break;
      }
      for (i = 0; i < 2; i++) {
        if (ch == *(char *)(0x27bb78 + i))
          goto skip_char;
      }
      for (i = 0; i < 2; i++) {
        if (ch == *(char *)(0x27bb7c + i))
          goto skip_char;
      }
      return;

    case 1:
      p = *cursor;
      if (*p == '\0')
        return;
      for (i = 0; i < 2; i++) {
        if (*p == *(char *)(0x27bb7c + i)) {
          state = 0;
          break;
        }
      }
      goto skip_char;

    case 2:
      p = *cursor;
      if (*p == '\0') {
        *(const char **)0x46b6fc = "unterminated comment.";
        return;
      }
      if (*p == '*' && p[1] == ';') {
        state = 0;
        *cursor = p + 1;
      }
      (*cursor)++;
      break;

    default:
      display_assert("!\"unreachable\"", "c:\\halo\\SOURCE\\hs\\hs_compile.c",
                     0x46c, 1);
      system_exit(-1);
      break;
    }
    continue;
  skip_char:
    *cursor = p + 1;
  } while (state != 3);
}

bool FUN_000c73a0(int datum_index)
{
  char *node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  int16_t type = *(int16_t *)(node + 0x4);

  if (!((type >= 4 && type <= 0x30) || type == 1 || type == 0)) {
    display_assert(
      "hs_type_valid(expression->type) || expression->type==_hs_special_form"
      " || expression->type==_hs_unparsed",
      "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x4af, 1);
    system_exit(-1);
  }

  if (type == 1) {
    *(const char **)0x46b6fc = "i expected a script or variable definition.";
    *(int *)0x46b700 = *(int *)(node + 0xc);
    return false;
  }

  if (type == 4) {
    *(const char **)0x46b6fc =
      "the value of this expression (in a <void> slot) can never be used.";
    *(int *)0x46b700 = *(int *)(node + 0xc);
    return false;
  }

  /* Attempt variable-reference resolution when not strictly validating or
   * when the node already carries the variable-resolved flag (bit 2). */
  if (*(uint8_t *)0x46b808 == 0 || (*(uint8_t *)(node + 0x6) & 0x4)) {
    bool result = FUN_000c5840(datum_index);
    if (result)
      return result;
  }

  type = *(int16_t *)(node + 0x4);
  if (type != 0 && *(const char **)0x46b6fc == NULL &&
      (*(uint8_t *)0x46b808 == 0 || !(*(uint8_t *)(node + 0x6) & 0x4))) {
    typedef bool (*hs_type_compile_fn_t)(int datum_index);
    hs_type_compile_fn_t fn =
      ((hs_type_compile_fn_t *)(void *)0x27bb80)[(int)type];
    if (fn == NULL) {
      crt_sprintf((char *)0x46b704,
                  "expressions of type %s are currently unsupported.",
                  ((const char **)(void *)0x2f14a8)[(int)type]);
      *(const char **)0x46b6fc = (const char *)0x46b704;
      *(int *)0x46b700 = *(int *)(node + 0xc);
      return false;
    }
    return fn(datum_index);
  }

  return false;
}

/* Type-check an HS expression (non-function) node (0xc74c0).
 *
 * Called from hs_type_check when a syntax node has flag bit 0 clear
 * (expression node, not a function-call). Receives datum_index in EBX
 * (register arg).
 *
 * Node layout (at hs_syntax_data[datum_index]):
 *   +0x2  int16_t  function_index / script_index
 *   +0x4  int16_t  type
 *   +0x6  uint8_t  flags (bit 0 = compiled, bit 1 = is_script, bit 2 =
 * variable_ref) +0x8  int      next sibling datum_index +0xc  int      string
 * offset (relative to compiled_source base) +0x10 int      first child
 * datum_index
 *
 * The function checks the expression's first child node (datum_index at
 * node+0x10) for the compiled flag (bit 0 of flags at child+0x6). If the
 * child is not compiled, emits an "i expected X, but i got an expression"
 * error and returns false.
 *
 * If compiled and type == 1 (hs_special_form):
 *   - Compares the child's string (child->string_offset + compiled_source)
 *     against "global" (strcmp). If matched, calls FUN_000c6b00 (global
 *     variable compile pass) with datum_index.
 *   - Otherwise compares against "script". If matched, calls FUN_000c6d90
 *     (script compile pass) with datum_index.
 *   - Otherwise sets error "i expected \"script\" or \"global\"." and returns
 *     false.
 *
 * For all other types, calls FUN_000c5960 (expression name resolution via
 * EAX = datum_index) to resolve the expression's function_index. Then:
 *   - If function_index == -1: error "this is not a valid function or
 *     script name." using child's string_offset.
 *   - If bit 1 of flags is set (is_script reference): looks up the script
 *     element via global_scenario_get()+0x49c, checking script type (must be
 *     3 or 4 = static/dormant). Validates return-type compatibility via
 *     hs_types_compatible if type != 0. Propagates return type if type == 0.
 *   - Otherwise (function reference): gets the function descriptor via
 *     hs_function_table_get. Validates return-type compatibility via
 * hs_types_compatible if type != 0. Checks hs_compile_globals.no_sleep
 * (0x46b806) and hs_compile_globals.no_set (0x46b807) context flags. Propagates
 * return type if type == 0 and fn_desc->return_type != 3 (void). Calls the
 *     function descriptor's parse function pointer at fn_desc+0x8 with
 *     (function_index, datum_index @EBX).
 *
 * Returns true on success, false on error.
 */
bool FUN_000c74c0(int datum_index)
{
  /* Three datum_get calls at entry (matching binary) to load expression node
   * and first-child node. */
  char *node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  char *node2 = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  int first_child = *(int *)(node2 + 0x10);
  char *child_node = (char *)datum_get(*(data_t **)0x5aa6c8, first_child);

  /* Validate expression type. */
  int16_t type = *(int16_t *)(node + 0x4);
  if (!((type >= 4 && type <= 0x30) || type == 1 || type == 0)) {
    display_assert(
      "hs_type_valid(expression->type) || expression->type==_hs_special_form"
      " || expression->type==_hs_unparsed",
      "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x534, 1);
    system_exit(-1);
  }

  /* Reload child node (fourth datum_get, matches binary). */
  char *child_node2 = (char *)datum_get(*(data_t **)0x5aa6c8, first_child);
  if (!(*(uint8_t *)(child_node2 + 0x6) & 0x1)) {
    /* Child is not compiled — emit error. */
    const char *what = (*(int16_t *)(node + 0x4) == 1) ?
                         "\"script\" or \"global\"" :
                         "a function name";
    crt_sprintf((char *)0x46b704, "i expected %s, but i got an expression.",
                what);
    *(const char **)0x46b6fc = (const char *)0x46b704;
    *(int *)0x46b700 = *(int *)(child_node + 0xc);
    return false;
  }

  if (*(int16_t *)(node + 0x4) == 1) {
    /* hs_special_form: child string must be "global" or "script". */
    char *str = (char *)(*(int *)(child_node + 0xc) + *(int *)0x46b6e8);
    if (csstrcmp(str, "global") == 0) {
      return FUN_000c6b00(datum_index);
    }
    if (csstrcmp(str, "script") == 0) {
      return FUN_000c6d90(datum_index);
    }
    *(const char **)0x46b6fc = "i expected \"script\" or \"global\".";
    *(int *)0x46b700 = *(int *)(child_node + 0xc);
    return false;
  }

  /* Resolve expression name: function_index written into node+0x2. */
  FUN_000c5960(datum_index);

  int16_t fn_idx = *(int16_t *)(node + 0x2);
  if (fn_idx == -1) {
    *(const char **)0x46b6fc = "this is not a valid function or script name.";
    *(int *)0x46b700 = *(int *)(child_node + 0xc);
    return false;
  }

  if (*(uint8_t *)(node + 0x6) & 0x2) {
    /* Script reference: look up script element. */
    char *script_elem = (char *)tag_block_get_element(
      (char *)global_scenario_get() + 0x49c, (int)(int16_t)fn_idx, 0x5c);
    int16_t script_type = *(int16_t *)(script_elem + 0x20);
    if (script_type != 3 && script_type != 4) {
      *(const char **)0x46b6fc = "this is not a static script.";
      *(int *)0x46b700 = *(int *)(node + 0xc);
      return false;
    }

    if (*(int16_t *)(node + 0x4) != 0) {
      /* Validate return type compatibility. */
      int16_t script_ret = *(int16_t *)(script_elem + 0x22);
      int16_t expected = *(int16_t *)(node + 0x4);
      if (!hs_types_compatible(script_ret, expected)) {
        crt_sprintf(
          (char *)0x46b704, "i expected a %s, but this script returns a %s.",
          ((const char *
              *)(void *)0x2f14a8)[(int)(int16_t)(*(int16_t *)(node + 0x4))],
          ((const char **)(void *)0x2f14a8)[(int)(int16_t)script_ret]);
        *(const char **)0x46b6fc = (const char *)0x46b704;
        *(int *)0x46b700 = *(int *)(node + 0xc);
        return false;
      }
    }

    if (*(int16_t *)(node + 0x4) == 0) {
      *(int16_t *)(node + 0x4) = *(int16_t *)(script_elem + 0x22);
    }
    return true;
  }

  /* Function reference: get descriptor. */
  void *fn_desc = hs_function_table_get((int16_t)fn_idx);
  int16_t fn_ret = *(int16_t *)fn_desc;

  if (*(int16_t *)(node + 0x4) != 0) {
    /* Validate return type compatibility. */
    int16_t expected = *(int16_t *)(node + 0x4);
    if (!hs_types_compatible(fn_ret, expected)) {
      crt_sprintf((char *)0x46b704,
                  "i expected a %s, but this function returns a %s.",
                  ((const char **)(void *)0x2f14a8)[(int)(int16_t)expected],
                  ((const char **)(void *)0x2f14a8)[(int)(int16_t)fn_ret]);
      *(const char **)0x46b6fc = (const char *)0x46b704;
      *(int *)0x46b700 = *(int *)(node + 0xc);
      return false;
    }
  }

  /* Context restrictions. */
  if (*(uint8_t *)0x46b806 != 0) {
    /* no_sleep context: sleep and sleep_forever are illegal. */
    int16_t fi = *(int16_t *)(node + 0x2);
    if (fi == 0x13 || fi == 0x14) {
      *(const char **)0x46b6fc = "it is illegal to block in this context.";
      *(int *)0x46b700 = *(int *)(node + 0xc);
      return false;
    }
  }

  if (*(uint8_t *)0x46b807 != 0 && *(int16_t *)(node + 0x2) == 0x4) {
    *(const char **)0x46b6fc =
      "it is illegal to set the value of variables in this context.";
    *(int *)0x46b700 = *(int *)(node + 0xc);
    return false;
  }

  /* Propagate return type if node is untyped and fn returns non-void. */
  if (*(int16_t *)(node + 0x4) == 0 && fn_ret != 3) {
    *(int16_t *)(node + 0x4) = fn_ret;
  }

  /* Invoke the function descriptor's parse callback. */
  if (*(void **)(((char *)fn_desc) + 0x8) == NULL) {
    display_assert("function->parse", "c:\\halo\\SOURCE\\hs\\hs_compile.c",
                   0x58c, 1);
    system_exit(-1);
  }
  typedef bool (*hs_parse_fn_t)(int16_t function_index, int datum_index);
  hs_parse_fn_t parse_fn = *(hs_parse_fn_t *)(((char *)fn_desc) + 0x8);
  return parse_fn(*(int16_t *)(node + 0x2), datum_index);
}

/* Mark an HS syntax node (and its children) as needing recompilation
 * (0xc7b10). Sets dirty flag (bit 3) on the node.
 *
 * If the node is a non-function expression (bit 0 of flags == 0), recursively
 * marks all children in the sibling linked list (node+0x10 = first child,
 * node+0x8 = next sibling).
 *
 * If the node is a function expression (bit 0 of flags == 1):
 *   - If type == 2 (call expression): re-interns the function name string
 *     into the compile-time string buffer and stores the new offset in
 *     node+0xc. If node+0xc was -1, looks up the function descriptor via
 *     hs_function_table_get and interns the name at descriptor+4. Otherwise
 *     re-interns the existing string at (node+0xc + compiled_source_base).
 *   - If type != 2 but flag bit 2 is set or type >= 9: same re-intern of
 *     node+0xc + compiled_source_base into node+0xc.
 */
void FUN_000c7b10(int datum_index)
{
  char *node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  *(uint8_t *)(node + 0x6) |= 0x8;

  /* Re-read the node flags to test bit 0 (function vs expression). */
  char *node2 = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  if (!(*(uint8_t *)(node2 + 0x6) & 0x1)) {
    /* Non-function node: recurse into children. */
    int child = *(int *)(node + 0x10);
    while (child != -1) {
      FUN_000c7b10(child);
      char *child_node = (char *)datum_get(*(data_t **)0x5aa6c8, child);
      child = *(int *)(child_node + 0x8);
    }
    return;
  }

  /* Function node: re-intern the string constant for recompilation. */
  int16_t type = *(int16_t *)(node + 0x4);
  if (type == 2) {
    int str_offset = *(int *)(node + 0xc);
    char *str_ptr;
    if (str_offset == -1) {
      /* No string interned yet: look up function name from descriptor. */
      void *fn_desc =
        hs_function_table_get((int16_t) * (uint16_t *)(node + 0x2));
      str_ptr = *(char **)((char *)fn_desc + 0x4);
    } else {
      str_ptr = (char *)(str_offset + *(int *)0x46b6e8);
    }
    *(int *)(node + 0xc) = FUN_000c6a70(str_ptr);
    return;
  }

  if ((*(uint8_t *)(node + 0x6) & 0x4) || type >= 9) {
    char *str_ptr = (char *)(*(int *)(node + 0xc) + *(int *)0x46b6e8);
    *(int *)(node + 0xc) = FUN_000c6a70(str_ptr);
  }
}

/* Type-check an HS syntax node (0xc7d80).
 * If the node is untyped (type==0), sets its type to check_type and
 * dispatches to FUN_000c73a0 (function-call nodes) or FUN_000c74c0
 * (expression nodes) based on the node's flag bit 0. Returns true
 * if the node was already typed. */
bool hs_type_check(int datum_index, int16_t check_type)
{
  char *node;

  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);

  if (*(int *)0x46b6fc != 0) {
    display_assert("!hs_compile_globals.error",
                   "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x48e, 1);
    system_exit(-1);
  }

  if (!((check_type >= 4 && check_type <= 0x30) || check_type == 1 ||
        check_type == 0)) {
    display_assert(
      "hs_type_valid(expected_type) || expected_type==_hs_special_form"
      " || expected_type==_hs_unparsed",
      "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x491, 1);
    system_exit(-1);
  }

  if (*(int16_t *)(node + 4) != 0)
    return true;

  *(int16_t *)(node + 4) = check_type;

  {
    char *node2 = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
    if (*(uint8_t *)(node2 + 6) & 1) {
      *(int16_t *)(node + 2) = check_type;
      return FUN_000c73a0(datum_index);
    }
  }

  return FUN_000c74c0(datum_index);
}

/* Recompile all HS scripts and globals in the current scenario (0xc93f0).
 * First resizes the scenario's HS string data block (scenario+0x488) to the
 * current source_size, then initialises the compile globals for a new pass.
 * Iterates every syntax node in the scripts block (scenario+0x4a8) and the
 * globals block (scenario+0x49c), marking each for recompilation via
 * FUN_000c7b10.  Calls hs_scripts_dispose() to release the compiled state,
 * then resizes the string block down to (accumulated_string_size + 0x400).
 * Both resize failures share the same assert/exit path (single error block in
 * binary at 0xc94c2). */
void hs_compile_recompile_scripts(void)
{
  scenario_t *scenario = global_scenario_get();

  if (tag_data_resize((void *)((char *)scenario + 0x488), *(int *)0x46b6e4)) {
    /* Initialise compile globals for the recompilation pass. */
    *(int *)0x46b6ec = *(int *)((char *)scenario + 0x494);
    *(int *)0x46b6f0 = 0;
    *(int *)0x46b6f4 = *(int *)0x46b6e4;

    /* Mark every node in the scripts block (scenario+0x4a8) dirty.
     * global_scenario_get() is called each iteration to get a fresh
     * scenario pointer for the block argument (matches binary). */
    {
      int16_t i = 0;
      while ((int)i < *(int *)((char *)scenario + 0x4a8)) {
        void *elem = tag_block_get_element(
          (void *)((char *)global_scenario_get() + 0x4a8), (int)i, 0x5c);
        FUN_000c7b10(*(int *)((char *)elem + 0x28));
        i++;
      }
    }

    /* Mark every node in the globals block (scenario+0x49c) dirty.
     * Count is reloaded from the block's own first word each iteration. */
    {
      void *block = (void *)((char *)scenario + 0x49c);
      int16_t i = 0;
      while ((int)i < *(int *)block) {
        void *elem = tag_block_get_element(block, (int)i, 0x5c);
        FUN_000c7b10(*(int *)((char *)elem + 0x24));
        i++;
      }
    }

    hs_scripts_dispose();

    if (tag_data_resize((void *)((char *)scenario + 0x488),
                        *(int *)0x46b6f0 + 0x400)) {
      return;
    }
  }

  display_assert("increase MAXIMUM_HS_STRING_DATA_PER_SCENARIO",
                 "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x16d, 1);
  system_exit(-1);
}
