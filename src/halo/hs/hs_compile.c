/* 0xc5730 — Append a source file's contents to the HS source buffer.
 * Reallocates the buffer at 0x46b6e8 to hold source_size + file_size + 1,
 * copies the file data, updates source_size at 0x46b6e4, and null-terminates.
 * Returns pointer to the appended data, or NULL on allocation failure. */
char *hs_compile_initialize(int source_file_size, void *source_ptr)
{
  int old_size;
  char *new_buf;
  char *dest;

  dest = 0;
  old_size = *(int *)0x46b6e4;
  new_buf =
    (char *)debug_realloc(*(void **)0x46b6e8, old_size + source_file_size + 1,
                          "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0xfd);
  if (new_buf != 0) {
    dest = new_buf + *(int *)0x46b6e4;
    *(char **)0x46b6e8 = new_buf;
    csmemcpy(dest, source_ptr, source_file_size);
    *(int *)0x46b6e4 = *(int *)0x46b6e4 + source_file_size;
    *(char *)(*(int *)0x46b6e8 + *(int *)0x46b6e4) = 0;
  }
  return dest;
}

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

/* 0xc5b50 — Validate and parse a real (float) literal from an HS expression.
 * Checks each character is a digit or single decimal point, then calls atof
 * to store the parsed value. Sets compile error on invalid input. */
bool FUN_000c5b50(int datum_index)
{
  char c;
  char seen_dot;
  char *node;
  char *str;
  bool result;

  result = true;
  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  str = (char *)(*(int *)(node + 0xc) + *(int *)0x46b6e8);
  seen_dot = 0;

  if (*(int16_t *)(node + 0x4) != 6) {
    display_assert("expression->type==_hs_type_real",
                   "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x5d6, 1);
    system_exit(-1);
  }

  if (*(int16_t *)(node + 0x2) != *(int16_t *)(node + 0x4)) {
    display_assert("expression->constant_type==expression->type",
                   "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x5d7, 1);
    system_exit(-1);
  }

  if (*str == '-') {
    str = str + 1;
  }

  c = *str;
  if (c != '\0') {
    do {
      if (crt_isdigit((int)c) == 0) {
        if (seen_dot || *str != '.') {
          *(const char **)0x46b6fc = "this is not a valid real number.";
          *(int *)0x46b700 = *(int *)(node + 0xc);
          result = false;
          goto done;
        }
        seen_dot = 1;
      }
      c = str[1];
      str = str + 1;
    } while (c != '\0');
  }

done:
  *(float *)(node + 0x10) =
    (float)crt_atof((const char *)(*(int *)(node + 0xc) + *(int *)0x46b6e8));

  return result;
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

/* 0xc6a70 — Intern a string into the HS string constant pool.
 * Returns the byte offset of the string in the pool, deduplicating
 * via FUN_000c57d0. Asserts if the pool is full. */
int FUN_000c6a70(char *str)
{
  int offset;
  int len;

  offset = FUN_000c57d0(str);
  if (offset == -1) {
    len = (int)(short)(csstrlen(str) + 1);
    if (len < *(int *)0x46b6f4) {
      csmemcpy((char *)(*(int *)0x46b6ec + *(int *)0x46b6f0), str, len);
      offset = *(int *)0x46b6f0;
      *(int *)0x46b6f0 = *(int *)0x46b6f0 + len;
      *(int *)0x46b6f4 = *(int *)0x46b6f4 - len;
      return offset;
    }
    display_assert("ran out of script string constant memory",
                   "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x264, 1);
    system_exit(-1);
  }
  return offset;
}

/* 0xc6b00 — Compile a (global <type> <name> <initial-value>) declaration.
 * Validates exactly 3 args, matches type name, checks name length < 32,
 * ensures no duplicate global, type-checks the value expression, and
 * allocates a new entry in the scenario globals tag block. */
bool FUN_000c6b00(int datum_index)
{
  char *node;
  int type_arg;
  int name_arg;
  int value_arg;
  char *type_name;
  char *var_name;
  int16_t i;
  int16_t new_idx;
  char *element;
  bool result;

  result = false;

  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  node = (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
  type_arg = *(int *)(node + 0x8);
  if (type_arg != -1) {
    node = (char *)datum_get(*(data_t **)0x5aa6c8, type_arg);
    name_arg = *(int *)(node + 0x8);
    if (name_arg != -1) {
      node = (char *)datum_get(*(data_t **)0x5aa6c8, name_arg);
      value_arg = *(int *)(node + 0x8);
      if (value_arg != -1) {
        node = (char *)datum_get(*(data_t **)0x5aa6c8, value_arg);
        if (*(int *)(node + 0x8) == -1) {
          node = (char *)datum_get(*(data_t **)0x5aa6c8, type_arg);
          type_name = (char *)(*(int *)(node + 0xc) + *(int *)0x46b6e8);

          i = 0;
          do {
            if (csstrcmp(type_name, ((const char **)0x2f14a8)[(int)(short)i]) ==
                0) {
              if ((short)i < 4 || (short)i >= 0x31)
                break;

              node = (char *)datum_get(*(data_t **)0x5aa6c8, name_arg);
              var_name = (char *)(*(int *)(node + 0xc) + *(int *)0x46b6e8);

              if (csstrlen(var_name) == 0 || csstrlen(var_name) > 0x1f) {
                *(const char **)0x46b6fc =
                  "i expected a global variable name less than 32 characters.";
                node = (char *)datum_get(*(data_t **)0x5aa6c8, name_arg);
                *(int *)0x46b700 = *(int *)(node + 0xc);
                return false;
              }

              if (hs_find_global_by_name(var_name) != -1) {
                *(const char **)0x46b6fc =
                  "there is already a variable by this name.";
                node = (char *)datum_get(*(data_t **)0x5aa6c8, name_arg);
                *(int *)0x46b700 = *(int *)(node + 0xc);
                return false;
              }

              *(uint8_t *)0x46b806 = 1;
              *(uint8_t *)0x46b807 = 1;
              if (hs_type_check(value_arg, (int)i)) {
                new_idx =
                  tag_block_add_element((char *)global_scenario_get() + 0x4a8);
                if (new_idx == -1) {
                  *(const char **)0x46b6fc =
                    "i couldn't allocate space for this global.";
                  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
                  *(int *)0x46b700 = *(int *)(node + 0xc);
                } else {
                  element = (char *)tag_block_get_element(
                    (char *)global_scenario_get() + 0x4a8, (int)new_idx, 0x5c);
                  csstrcpy(element, var_name);
                  *(int16_t *)(element + 0x20) = i;
                  *(int *)(element + 0x28) = value_arg;
                  result = true;
                }
              }
              *(uint8_t *)0x46b806 = 0;
              *(uint8_t *)0x46b807 = 0;
              return result;
            }
            i++;
          } while ((short)i < 0x31);

          *(const char **)0x46b6fc = "this is not a valid type.";
          node = (char *)datum_get(*(data_t **)0x5aa6c8, type_arg);
          *(int *)0x46b700 = *(int *)(node + 0xc);
          return result;
        }
      }
    }
  }

  *(const char **)0x46b6fc = "i expected (global<type> <name> <initial value>)";
  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  *(int *)0x46b700 = *(int *)(node + 0xc);
  return false;
}

/* 0xc6d90 — Compile a (script <type> [<return-type>] <name> <body>) definition.
 *
 * Walks the argument list under datum_index to extract: script type, optional
 * return type (for static/stub scripts), script name, and the expression body.
 * If a script with the same name already exists the function validates that the
 * existing definition is a compatible stub (or static overriding a stub).
 * On success allocates two new syntax nodes for the "begin" wrapper and body,
 * writes the script element (name, type, return type, node handle), calls
 * hs_type_check on the body, and returns true.
 *
 * Script-type table at 0x2f156c (5 entries): startup(0), dormant(1),
 * continuous(2), static(3), stub(4).
 * HS type table at 0x2f14a8 (0x31 entries, indices 4..0x30 valid for return).
 *
 * Node layout used here (offsets confirmed from disassembly):
 *   +0x2  int16_t  (script index / function index)
 *   +0x4  int16_t  type
 *   +0x6  uint16_t flags
 *   +0x8  int      next sibling datum_index
 *   +0xc  int      string offset (relative to compiled source base at 0x46b6e8)
 *   +0x10 int      first child datum_index
 *
 * Script element layout (element_size=0x5c, confirmed from tag_block calls):
 *   +0x00 char[32] name (written via csstrcpy)
 *   +0x20 int16_t  script_type
 *   +0x22 int16_t  return_type
 *   +0x24 int      root_node datum_index
 *   +0x28 int      (init expression, used by globals; not touched here)
 */
bool FUN_000c6d90(int datum_index)
{
  char *node;
  int type_arg; /* first child of the "script" node */
  int name_arg; /* sibling after type [and optional return-type] arg */
  int name_next_arg; /* sibling after name (body expression) */
  int16_t script_type_idx; /* index into the script-type name table (0-4) */
  int16_t return_type_idx; /* index into the HS type table */
  int return_type_arg; /* datum index of the return-type token node (static/stub
                          only) */
  char *type_str; /* pointer into source buffer for the type token */
  char *name_str; /* pointer into source buffer for the name token */
  int head_handle; /* datum index of newly-allocated head syntax node */
  int body_handle; /* datum index of newly-allocated body syntax node */
  char *head_node; /* pointer to head syntax node data */
  char *body_node; /* pointer to body syntax node data */
  char *script_elem; /* pointer to the script tag-block element */
  int16_t existing; /* existing script index (-1 = not found) */

  /* Navigate: datum_index -> (script) -> first child -> next sibling.
   * That sibling is the script-type token node (type_arg). */
  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  node = (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
  type_arg = *(int *)(node + 0x8);

  if (type_arg == -1) {
    *(const char **)0x46b6fc =
      "i expected (script <type> <name> <expression(s)>)";
    node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
    *(int *)0x46b700 = *(int *)(node + 0xc);
    return false;
  }

  /* Resolve the script-type string and search the script-type table. */
  node = (char *)datum_get(*(data_t **)0x5aa6c8, type_arg);
  type_str = (char *)(*(int *)(node + 0xc) + *(int *)0x46b6e8);

  script_type_idx = 0;
  do {
    if (csstrcmp(type_str, ((const char **)0x2f156c)[(int)script_type_idx]) ==
        0)
      goto found_script_type;
    script_type_idx++;
  } while (script_type_idx < 5);

  /* No match — emit error, report position of the type token. */
  *(const char **)0x46b6fc = "script type must be \"startup\", \"dormant\", "
                             "\"continuous\", or \"static\".";
  node = (char *)datum_get(*(data_t **)0x5aa6c8, type_arg);
  *(int *)0x46b700 = *(int *)(node + 0xc);
  return false;

found_script_type:
  if (script_type_idx == 3 || script_type_idx == 4) {
    /* static or stub: next arg is return type, then name. */
    node = (char *)datum_get(*(data_t **)0x5aa6c8, type_arg);
    return_type_arg = *(int *)(node + 0x8);
    if (return_type_arg == -1) {
      *(const char **)0x46b6fc =
        "i expected (script local <type> <name> <expression(s)>).";
      node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
      *(int *)0x46b700 = *(int *)(node + 0xc);
      return false;
    }

    /* Search HS type table for return type name. */
    node = (char *)datum_get(*(data_t **)0x5aa6c8, return_type_arg);
    type_str = (char *)(*(int *)(node + 0xc) + *(int *)0x46b6e8);
    return_type_idx = 0;
    while (csstrcmp(type_str,
                    ((const char **)0x2f14a8)[(int)return_type_idx]) != 0) {
      return_type_idx++;
      if (return_type_idx >= 0x31) {
        return_type_idx = -1;
        break;
      }
    }

    /* Advance to the name node: next sibling of the return-type token.
     * Error position (if invalid type) uses the return-type token node. */
    node = (char *)datum_get(*(data_t **)0x5aa6c8, return_type_arg);
    name_arg = *(int *)(node + 0x8);

    if ((short)return_type_idx < 4 || 0x30 < (short)return_type_idx) {
      *(const char **)0x46b6fc = "this is not a valid return type.";
      node = (char *)datum_get(*(data_t **)0x5aa6c8, return_type_arg);
      *(int *)0x46b700 = *(int *)(node + 0xc);
      return false;
    }
  } else {
    /* startup / dormant / continuous: no explicit return type; use index 4. */
    return_type_idx = 4;
    node = (char *)datum_get(*(data_t **)0x5aa6c8, type_arg);
    name_arg = *(int *)(node + 0x8);
  }

  /* name_arg now points to the script-name token node. */
  if (name_arg == -1) {
    if (script_type_idx == 3) {
      *(const char **)0x46b6fc =
        "i expected (script static <type> <name> <expression(s)>)";
    } else if (script_type_idx == 4) {
      *(const char **)0x46b6fc =
        "i expected (script stub <type> <name> <expression(s)>)";
    } else {
      *(const char **)0x46b6fc =
        "i expected (script <type> <name> <expression(s)>)";
    }
    node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
    *(int *)0x46b700 = *(int *)(node + 0xc);
    return false;
  }

  node = (char *)datum_get(*(data_t **)0x5aa6c8, name_arg);
  name_next_arg = *(int *)(node + 0x8);

  if (name_next_arg == -1) {
    if (script_type_idx == 3) {
      *(const char **)0x46b6fc =
        "i expected (script static <type> <name> <expression(s)>)";
    } else if (script_type_idx == 4) {
      *(const char **)0x46b6fc =
        "i expected (script stub <type> <name> <expression(s)>)";
    } else {
      *(const char **)0x46b6fc =
        "i expected (script <type> <name> <expression(s)>)";
    }
    node = (char *)datum_get(*(data_t **)0x5aa6c8, name_arg);
    *(int *)0x46b700 = *(int *)(node + 0xc);
    return false;
  }

  /* Validate name length (1..31 characters). */
  node = (char *)datum_get(*(data_t **)0x5aa6c8, name_arg);
  name_str = (char *)(*(int *)(node + 0xc) + *(int *)0x46b6e8);
  if (csstrlen(name_str) == 0 || csstrlen(name_str) >= 0x20) {
    *(const char **)0x46b6fc =
      "i expected a script name less than 32 characters.";
    node = (char *)datum_get(*(data_t **)0x5aa6c8, name_arg);
    *(int *)0x46b700 = *(int *)(node + 0xc);
    return false;
  }

  /* Look up scenario scripts block at scenario+0x49c. */
  {
    scenario_t *scenario = global_scenario_get();
    void *scripts_block = (char *)scenario + 0x49c;

    existing = hs_find_script_by_name(name_str);
    if (existing == -1) {
      /* No existing script — allocate a new script element. */
      existing = (int16_t)tag_block_add_element(scripts_block);
      if (existing == -1) {
        *(const char **)0x46b6fc = "i couldn't allocate a script.";
        node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
        *(int *)0x46b700 = *(int *)(node + 0xc);
        return false;
      }
    } else {
      /* Script exists — validate override rules. */
      char *elem =
        (char *)tag_block_get_element(scripts_block, (int)existing, 0x5c);
      int16_t elem_type = *(int16_t *)(elem + 0x20);
      int16_t elem_rtype = *(int16_t *)(elem + 0x22);

      /* A static overriding a stub of the same return type: allowed. */
      if (elem_type == 4 && elem_rtype == return_type_idx &&
          script_type_idx == 3) {
        /* Return early: override accepted, existing stub will be replaced. */
      } else if (elem_type == 3 && elem_rtype == return_type_idx &&
                 script_type_idx == 4) {
        /* A stub redeclaring a static of the same return type: already done. */
        return true;
      } else {
        *(const char **)0x46b6fc =
          "only static scripts of the same type can override stub scripts.";
        node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
        *(int *)0x46b700 = *(int *)(node + 0xc);
        return false;
      }
    }

    /* Allocate two syntax nodes for the script body. */
    script_elem =
      (char *)tag_block_get_element(scripts_block, (int)existing, 0x5c);
    head_handle = data_new_at_index(*(data_t **)0x5aa6c8);
    body_handle = data_new_at_index(*(data_t **)0x5aa6c8);
    if (head_handle == -1 || body_handle == -1) {
      *(const char **)0x46b6fc = "i couldn't allocate a syntax node.";
      return false;
    }

    /* Initialise head node: type=0, flags=0, next=-1, string=param_1 string,
     * first_child=body_handle. */
    head_node = (char *)datum_get(*(data_t **)0x5aa6c8, head_handle);
    body_node = (char *)datum_get(*(data_t **)0x5aa6c8, body_handle);
    *(int *)(head_node + 0x10) = body_handle;
    *(int *)(head_node + 0x8) = -1;
    node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
    *(int *)(head_node + 0xc) = *(int *)(node + 0xc);
    *(int16_t *)(head_node + 0x6) = 0;

    /* Initialise body node: flags=1 (leaf?), type_b=2, type_a=0,
     * next=name_next_arg, function_index=0, string=-1 (none). */
    *(int *)(body_node + 0x8) = name_next_arg;
    *(int *)(body_node + 0xc) = -1;
    *(int16_t *)(body_node + 0x2) = 0;
    *(int16_t *)(body_node + 0x6) = 1;
    *(int16_t *)(body_node + 0x4) = 2;

    /* Type-check the body. */
    if (!hs_type_check(head_handle, (int16_t)return_type_idx)) {
      return false;
    }

    /* Write script element: name, return type, script type, root node. */
    csstrcpy(script_elem, name_str);
    *(int16_t *)(script_elem + 0x22) = return_type_idx;
    *(int16_t *)(script_elem + 0x20) = script_type_idx;
    *(int *)(script_elem + 0x24) = head_handle;
    return true;
  }
}

/* 0xc71c0 — Parse an atom (non-parenthesized token) from the HS source.
 * Quoted strings: scan to closing '"', null-terminate, report unterminated.
 * Bare tokens: scan until ')', ';', whitespace, or NUL.
 * Both paths lowercase the result via csstr_tolower. */
void FUN_000c71c0(int datum_index, char **cursor)
{
  char *node;
  char *p;
  char ch;
  int16_t i;

  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  p = *cursor;
  if (*p == '"') {
    *cursor = p + 1;
    *(int *)(node + 0xc) = (int)(*cursor - *(char **)0x46b6e8);
    while (**cursor != '\0' && **cursor != '"')
      (*cursor)++;
    if (**cursor == '\0') {
      *(const char **)0x46b6fc = "this quoted constant is unterminated.";
      *(int *)0x46b700 = *(int *)(node + 0xc) - 1;
    }
    **cursor = '\0';
    (*cursor)++;
    csstr_tolower(*(char **)0x46b6e8 + *(int *)(node + 0xc));
    return;
  }

  *(int *)(node + 0xc) = (int)(p - *(char **)0x46b6e8);
  if (**cursor != '\0') {
    for (;;) {
      ch = **cursor;
      if (ch == ')' || ch == ';')
        break;
      for (i = 0; i < 2; i++) {
        if (ch == *(char *)(0x27bb78 + i))
          goto done;
      }
      for (i = 0; i < 2; i++) {
        if (ch == *(char *)(0x27bb7c + i))
          goto done;
      }
      (*cursor)++;
      if (**cursor == '\0')
        break;
    }
  }
done:
  csstr_tolower(*(char **)0x46b6e8 + *(int *)(node + 0xc));
}

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
  char *node;
  char *node2;
  int first_child;
  char *child_node;
  int16_t type;
  char *child_node2;
  int16_t fn_idx;
  void *fn_desc;
  int16_t fn_ret;
  typedef bool (*hs_parse_fn_t)(int16_t function_index, int datum_index);
  hs_parse_fn_t parse_fn;

  /* Three datum_get calls at entry (matching binary) to load expression node
   * and first-child node. */
  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  node2 = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  first_child = *(int *)(node2 + 0x10);
  child_node = (char *)datum_get(*(data_t **)0x5aa6c8, first_child);

  /* Validate expression type. */
  type = *(int16_t *)(node + 0x4);
  if (!((type >= 4 && type <= 0x30) || type == 1 || type == 0)) {
    display_assert(
      "hs_type_valid(expression->type) || expression->type==_hs_special_form"
      " || expression->type==_hs_unparsed",
      "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x534, 1);
    system_exit(-1);
  }

  /* Reload child node (fourth datum_get, matches binary). */
  child_node2 = (char *)datum_get(*(data_t **)0x5aa6c8, first_child);
  if (!(*(uint8_t *)(child_node2 + 0x6) & 0x1)) {
    {
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

  fn_idx = *(int16_t *)(node + 0x2);
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
  fn_desc = hs_function_table_get((int16_t)fn_idx);
  fn_ret = *(int16_t *)fn_desc;

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
  parse_fn = *(hs_parse_fn_t *)(((char *)fn_desc) + 0x8);
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
  char *node;
  char *node2;
  int16_t type;

  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  *(uint8_t *)(node + 0x6) |= 0x8;

  /* Re-read the node flags to test bit 0 (function vs expression). */
  node2 = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  if (!(*(uint8_t *)(node2 + 0x6) & 0x1)) {
    /* Non-function node: recurse into children. */
    int child = *(int *)(node + 0x10);
    while (child != -1) {
      char *child_node;
      FUN_000c7b10(child);
      child_node = (char *)datum_get(*(data_t **)0x5aa6c8, child);
      child = *(int *)(child_node + 0x8);
    }
    return;
  }

  /* Function node: re-intern the string constant for recompilation. */
  type = *(int16_t *)(node + 0x4);
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

/* 0xc7be0 — Allocate and initialize a new HS syntax node, then dispatch to
 * the atom parser (FUN_000c71c0) or parenthesized expression parser
 * (FUN_000c7ca0) depending on whether the cursor points to '('. */
int FUN_000c7be0(char **cursor)
{
  int datum_index;
  char *node;

  datum_index = data_new_at_index(*(data_t **)0x5aa6c8);

  if (*(char **)0x46b6fc != 0) {
    display_assert("!hs_compile_globals.error",
                   "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x39e, 1);
    system_exit(-1);
  }

  if (datum_index == -1) {
    *(const char **)0x46b6fc = "i couldn't allocate a syntax node.";
    return -1;
  }

  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  *(int16_t *)(node + 0x6) = 0;
  *(int16_t *)(node + 0x4) = 0;
  *(int16_t *)(node + 0x2) = -1;
  *(int *)(node + 0x8) = -1;
  *(uint16_t *)(node + 0x6) = (uint16_t)(**cursor != '(');

  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  if (*(uint8_t *)(node + 0x6) & 1) {
    FUN_000c71c0(datum_index, cursor);
    return datum_index;
  }

  FUN_000c7ca0(cursor, datum_index);
  return datum_index;
}

/* 0xc7ca0 — Parse a parenthesized HS expression. Advances cursor past '(',
 * then loops: skip whitespace (null-terminating gaps), parse sub-expressions
 * via FUN_000c7be0, and chain them via each node's next_node field (+0x8).
 * Stops at ')' (null-terminates it) or NUL (unmatched paren error).
 * Sets "this expression is empty." if no children were parsed. */
void FUN_000c7ca0(char **cursor, int datum_index)
{
  char *node;
  char *pre_ws;
  int *link_ptr;
  int child_index;
  char *child_node;

  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
  *(int *)(node + 0xc) = (int)(*cursor - *(char **)0x46b6e8);
  (*cursor)++;
  link_ptr = (int *)(node + 0x10);

  if (*(char **)0x46b6fc == 0) {
    do {
      pre_ws = *cursor;
      FUN_000c72b0(cursor);
      if (*cursor != pre_ws)
        *pre_ws = '\0';

      if (**cursor == '\0') {
        *(const char **)0x46b6fc = "this left parenthesis is unmatched.";
        *(int *)0x46b700 = *(int *)(node + 0xc);
        break;
      }

      if (**cursor == ')') {
        **cursor = '\0';
        (*cursor)++;
        break;
      }

      child_index = FUN_000c7be0(cursor);
      *link_ptr = child_index;
      if (child_index != -1) {
        child_node = (char *)datum_get(*(data_t **)0x5aa6c8, child_index);
        link_ptr = (int *)(child_node + 0x8);
      }
    } while (*(char **)0x46b6fc == 0);
  }

  if (link_ptr == (int *)(node + 0x10) && *(char **)0x46b6fc == 0) {
    *(const char **)0x46b6fc = "this expression is empty.";
    *(int *)0x46b700 = *(int *)(node + 0xc);
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
