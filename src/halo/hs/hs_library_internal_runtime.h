/* c:\halo\source\hs\hs_library_internal_runtime.h
 *
 * Header path recovered from the XBE: the HS evaluator bodies below carry
 * display_assert calls whose __FILE__ is this header, at header lines 21, 69,
 * 80, 119, 207, 273, 305, 349, 363, 366, 369, 370, 393, 485, 700, 732, 772 and
 * 816. Those line numbers ascend in exactly the order the functions appear
 * here, which is what proves this is the file they lived in and the order they
 * lived in -- not merely that such a header existed.
 *
 * NOT a self-contained header, deliberately. This is an "internal" header in
 * the original sense: it holds the definitions of the per-script-function
 * evaluators and is #included at one point in hs_runtime.c, after the file-
 * scope statics they call (hs_return, hs_thread_stack_alloc, FUN_000cc1d0,
 * hs_function_table_get, ...). It will not compile alone, and must never be
 * included from a second translation unit -- these are external definitions,
 * so a second include is a duplicate-symbol error, not a redefinition warning.
 *
 * hs_evaluate_set is the one function here with no assert of its own. It is
 * included on positional evidence only: it sits between hs_evaluate_if (header
 * line 119) and hs_evaluate_logical (header line 207) in our source, and every
 * other function in that span is assert-proven to be in this header, so the
 * gap 119..207 is where it belongs. Marked as inference, not proof.
 *
 * See the header-recovery skill for the extraction sweep. */

#ifndef HALO_HS_HS_LIBRARY_INTERNAL_RUNTIME_H
#define HALO_HS_HS_LIBRARY_INTERNAL_RUNTIME_H


/* 0xcc590 — HS 'begin' evaluator. Evaluates a sequence of expressions in
 * order, returning the value of the last one. On init, sets up the expression
 * list pointer (skipping the function-name child). Each call evaluates one
 * expression and advances to the next sibling. */
void hs_evaluate_begin(int16_t function_index, int thread_datum, char init)
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
    char *expr;
    FUN_000cc1d0(thread_datum, *expr_ptr, result_ptr);
    expr = (char *)datum_get(*(data_t **)0x5aa6c8, *expr_ptr);
    *expr_ptr = *(int *)(expr + 0x8);
    return;
  }

  hs_return(thread_datum, *result_ptr);
}

/* 0xcc660 — HS 'begin_random' evaluator.
 * Implements (begin_random <arg0> <arg1> ... <argN-1>): on each call selects
 * one not-yet-evaluated argument at random and evaluates it.  When all
 * arguments have been evaluated it pops the frame with the last result.
 *
 * Multi-phase protocol:
 *   init==true  : count the argument list, memset the used-bit array.
 *   init==false : pick the next unused slot and evaluate it; when all slots
 *                 are used call hs_return to commit the result.
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
void hs_evaluate_begin_random(int16_t function_index, int thread_datum,
                              char init)
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
    hs_return(thread_datum, *result_value);
  }
}

/* 0xcc870 — HS 'if' evaluator. Three-phase: init evaluates the condition,
 * second call selects then/else branch, third call pops frame with result.
 * (if <condition> <then> [else]) */
void hs_evaluate_if(int16_t function_index, int thread_datum, char init)
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
    char *node;
    char *child;
    *(int *)cond_result = 0;
    *branch_ptr = -1;
    node = (char *)datum_get(*(data_t **)0x5aa6c8,
                             *(int *)(*(char **)(thread + 0x10) + 0x4));
    child = (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    FUN_000cc1d0(thread_datum, *(int *)(child + 0x8), cond_result);
    return;
  }

  if (*branch_ptr != -1) {
    hs_return(thread_datum, *value_ptr);
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
        hs_return(thread_datum, 0);
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
void hs_evaluate_set(int16_t function_index, int thread_datum, char init)
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
    hs_return(thread_datum, *(int *)(global_datum + 4));
  }
}

/* 0xccb40 — HS 'and'/'or' evaluator. Short-circuits: AND stops on first
 * false, OR stops on first true. function_index 5 = and, 6 = or. */
void hs_evaluate_logical(int16_t function_index, int thread_datum, char init)
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

  hs_return(thread_datum, (int)(uint8_t)*running);
}

/* 0xccc70 — HS arithmetic evaluator (+, -, *, /, min, max). Accumulates
 * results across multiple operand expressions. Function indices 7-12. */
void hs_evaluate_arithmetic(int16_t function_index, int thread_datum, char init)
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
    char *node;
    char *child;
    *counter = 0;
    node = (char *)datum_get(*(data_t **)0x5aa6c8,
                             *(int *)(*(char **)(thread + 0x10) + 0x4));
    child = (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
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

  hs_return(thread_datum, *(int *)accum);
}

/* 0xccdf0 — HS equal/not-equal evaluator. Evaluates two arguments of the
 * same type via FUN_000cc3a0, then compares with csmemcmp using the type's
 * size from the table at 0x26f350. function_index 0xd = equal, 0xe = not_equal.
 */
void hs_evaluate_equality(int16_t function_index, int thread_datum, char init)
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
    hs_return(thread_datum, (int)(uint8_t)result);
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
 * Result is committed via hs_return(thread_datum, (int)(uint8_t)result).
 */
void hs_evaluate_inequality(int16_t function_index, int thread_datum, char init)
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
    float a;
    float b;
    if (type != 7 && (type < 0x20 || type > 0x24)) {
      display_assert("parameter_types[0]==_hs_type_short_integer || "
                     "HS_TYPE_IS_ENUM(parameter_types[0])",
                     "c:\\halo\\source\\hs\\hs_library_internal_runtime.h",
                     0x171, 1);
      system_exit(-1);
    }
    a = (float)(int)*(int16_t *)&values[0];
    b = (float)(int)*(int16_t *)&values[1];
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

  hs_return(thread_datum, (int)(uint8_t)result);
}

/* 0xcd0e0 — HS 'sleep' evaluator. Puts a thread (or another thread by script
 * index) to sleep for a given number of ticks. Three-phase protocol:
 *   init: evaluate the sleep-ticks expression, set phase=0.
 *   phase 0: resolve optional target-thread expression; increment phase.
 *   phase 1: apply the sleep. If target != -1, look up thread by script index
 *            via FUN_000cada0(@EDI). Negative ticks → sleep_until = -2
 * (forever). Positive ticks → sleep_until = game_time + ticks. Backs up the
 *            target's old sleep_until if sleeping a different thread.
 *
 * Stack allocations:
 *   4 bytes — sleep_ticks (int16_t value from evaluation)
 *   4 bytes — target_ref (int16_t script index of target thread, or -1)
 *   2 bytes — phase counter
 *
 * Assert: function_index == 0x13 (_hs_function_sleep).
 *
 * Globals:
 *   0x5aa6c4 = hs_thread_data (data_t*)
 *   0x5aa6c8 = hs_syntax_data (data_t*)
 */
void hs_evaluate_sleep(int16_t function_index, int thread_datum, char init)
{
  char *thread;
  int16_t *sleep_ticks;
  int16_t *target_ref;
  int16_t *phase;
  int local_thread;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  sleep_ticks = (int16_t *)hs_thread_stack_alloc(thread_datum, 4);
  target_ref = (int16_t *)hs_thread_stack_alloc(thread_datum, 4);
  phase = (int16_t *)hs_thread_stack_alloc(thread_datum, 2);
  local_thread = thread_datum;

  if (function_index != 0x13) {
    display_assert("function_index==_hs_function_sleep",
                   "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 0x189,
                   1);
    system_exit(-1);
  }

  if (init) {
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(int *)(thread + 0x10) + 4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    FUN_000cc1d0(thread_datum, *(int *)(child + 0x8), sleep_ticks);
    *phase = 0;
    return;
  }

  if (*phase == 0) {
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(int *)(thread + 0x10) + 4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    char *ticks_node =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(child + 0x8));
    int next_expr = *(int *)(ticks_node + 0x8);
    *phase = *phase + 1;
    if (next_expr != -1) {
      FUN_000cc1d0(thread_datum, next_expr, target_ref);
      return;
    }
    *(int *)target_ref = -1;
    if (*phase == 0)
      return;
  }

  {
    int16_t ticks = *sleep_ticks;
    if (ticks != 0) {
      if (*target_ref != -1) {
        local_thread = FUN_000cada0(*target_ref);
      }
      if (local_thread != -1) {
        char *target = (char *)datum_get(*(data_t **)0x5aa6c4, local_thread);
        int new_sleep;
        if (ticks < 0) {
          new_sleep = -2;
        } else {
          new_sleep = game_time_get() + (int)ticks;
        }
        if (*(int *)(target + 0x8) != -1) {
          if (local_thread != thread_datum &&
              (*(uint8_t *)(target + 0x3) & 2) == 0) {
            *(uint8_t *)(target + 0x3) |= 2;
            *(int *)(target + 0xc) = *(int *)(target + 0x8);
          }
          target = (char *)datum_get(*(data_t **)0x5aa6c4, local_thread);
          *(int *)(target + 0x8) = new_sleep;
        }
      }
    }
    hs_return(thread_datum, 0);
  }
}

/* 0xcd2a0 — HS 'sleep_until' evaluator. Repeatedly evaluates a condition
 * expression until it becomes true or a timeout expires. Sleeps between
 * evaluations for a configurable number of ticks (default 30).
 *
 * Stack allocations:
 *   4 bytes — evaluated (char flag: 0=pending, nonzero=condition true)
 *   4 bytes — ticks_per_eval (int16_t, default 30)
 *   4 bytes — timeout_ticks (int, -1 = no timeout)
 *   4 bytes — start_time (int, game_time at init)
 *   2 bytes — phase counter
 *
 * Multi-phase:
 *   init: set defaults, evaluate optional ticks_per_eval expression.
 *   phase 0: evaluate optional timeout expression.
 *   phase 1+: re-evaluate condition; if true or timed out, wake thread.
 *             Otherwise set sleep_until = game_time + ticks, clamped to
 * deadline.
 *
 * Assert: function_index == 0x14 (_hs_function_sleep_until).
 */
void hs_evaluate_sleep_until(int16_t function_index, int thread_datum,
                             char init)
{
  char *thread;
  char *evaluated;
  int16_t *ticks_per_eval;
  int *timeout_ticks;
  int *start_time;
  int16_t *phase;
  int ticks_expr;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  evaluated = (char *)hs_thread_stack_alloc(thread_datum, 4);
  ticks_per_eval = (int16_t *)hs_thread_stack_alloc(thread_datum, 4);
  timeout_ticks = (int *)hs_thread_stack_alloc(thread_datum, 4);
  start_time = (int *)hs_thread_stack_alloc(thread_datum, 4);
  phase = (int16_t *)hs_thread_stack_alloc(thread_datum, 2);

  {
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(int *)(thread + 0x10) + 4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    char *cond = (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(child + 0x8));
    ticks_expr = *(int *)(cond + 0x8);
  }

  if (function_index != 0x14) {
    display_assert("function_index==_hs_function_sleep_until",
                   "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 0x1e5,
                   1);
    system_exit(-1);
  }

  if (init) {
    *evaluated = 0;
    *start_time = game_time_get();
    *phase = 0;
    *ticks_per_eval = 0x1e;
    *timeout_ticks = -1;
    if (ticks_expr != -1) {
      FUN_000cc1d0(thread_datum, ticks_expr, ticks_per_eval);
      return;
    }
  }

  if (*phase == 0) {
    *phase = 1;
    if (ticks_expr != -1) {
      char *ticks_node = (char *)datum_get(*(data_t **)0x5aa6c8, ticks_expr);
      if (*(int *)(ticks_node + 0x8) != -1) {
        FUN_000cc1d0(thread_datum, *(int *)(ticks_node + 0x8), timeout_ticks);
        return;
      }
    }
  }

  if (*phase != 1)
    return;

  if (*evaluated == 0 && (*timeout_ticks == -1 ||
                          game_time_get() < *timeout_ticks + *start_time)) {
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(int *)(thread + 0x10) + 4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    FUN_000cc1d0(thread_datum, *(int *)(child + 0x8), evaluated);
    {
      int ticks;
      int new_sleep;
      ticks = 1;
      if (*ticks_per_eval >= 1)
        ticks = (int)*ticks_per_eval;
      new_sleep = game_time_get() + ticks;
      *(int *)(thread + 0x8) = new_sleep;
      if (*timeout_ticks != -1) {
        int deadline = *timeout_ticks + *start_time;
        if (deadline <= new_sleep)
          new_sleep = deadline;
        *(int *)(thread + 0x8) = new_sleep;
      }
    }
  } else {
    hs_return(thread_datum, 0);
  }
}

/* 0xcd4a0 — HS 'inspect' evaluator. Evaluates one argument and prints its
 * value using the type-specific inspect function from the table at 0x2f3df8.
 * function_index must be 0x16 (_hs_function_inspect).
 *
 * Stack allocation: 4 bytes — result value.
 *
 * Globals:
 *   0x5aa6c4 = hs_thread_data (data_t*)
 *   0x5aa6c8 = hs_syntax_data (data_t*)
 *   0x2f3df8 = hs_type_inspect_table (code*[])
 */
void hs_evaluate_inspect(int16_t function_index, int thread_datum, char init)
{
  char *thread;
  int *result_ptr;
  char local_404[1024];

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  result_ptr = (int *)hs_thread_stack_alloc(thread_datum, 4);

  if (function_index != 0x16) {
    display_assert("function_index==_hs_function_inspect",
                   "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 700,
                   1);
    system_exit(-1);
  }

  {
    int first_arg = *(int *)(*(int *)(thread + 0x10) + 4);
    if (init) {
      char *node = (char *)datum_get(*(data_t **)0x5aa6c8, first_arg);
      char *child =
        (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
      FUN_000cc1d0(thread_datum, *(int *)(child + 0x8), result_ptr);
      return;
    }

    {
      char *node = (char *)datum_get(*(data_t **)0x5aa6c8, first_arg);
      char *child =
        (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
      char *value_node =
        (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(child + 0x8));
      int16_t type = *(int16_t *)(value_node + 0x4);
      typedef void (*inspect_fn)(int16_t, int, char *);
      inspect_fn fn = ((inspect_fn *)0x2f3df8)[(int)type];
      if (fn != NULL) {
        fn(type, *result_ptr, local_404);
        console_printf(0, local_404);
      }
    }
    hs_return(thread_datum, 0);
  }
}

/* 0xcd5a0 — HS object-to-unit type converter. Evaluates one argument,
 * checks if the object's type matches the target conversion mask from
 * the table at 0x26f320. Returns the object if compatible, NONE if not.
 * function_index 0x17 = object_to_unit. */
void hs_evaluate_object_cast_up(int16_t function_index, int thread_datum,
                                char init)
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
    hs_return(thread_datum, -1);
    return;
  }

  {
    char *obj = (char *)object_get_and_verify_type(*result_ptr, -1);
    int type_idx = (int)(int16_t)(function_index - 0x16);
    int type_bit = 1 << (*(uint8_t *)(obj + 0x64) & 0x1f);
    int type_mask = (int)*(int16_t *)(0x26f320 + type_idx * 2);

    const char *tag_name;
    if (type_mask & type_bit) {
      hs_return(thread_datum, *result_ptr);
      return;
    }

    tag_name = tag_get_name(*(int *)obj);
    error(2, "attempt to convert object %s to type %s", tag_name,
          *(const char **)(0x2f153c + type_idx * 4));
    hs_return(thread_datum, -1);
  }
}

/* 0xcd6c0 — HS debug_string evaluator. Collects up to 32 evaluated arguments
 * into a buffer, then dispatches to one of three output functions based on
 * function_index: 0x18 → ai_debug_communication_suppress, 0x19 →
 * ai_debug_communication_ignore, 0x1a → ai_debug_communication_focus.
 *
 * Stack allocations:
 *   4 bytes — current expression datum (int*)
 *   4 bytes — argument count (int*)
 *   128 bytes — argument values array (int[32])
 *
 * Assert: function_index in [0x18..0x1a] (_hs_function_debug_string range).
 */
void hs_evaluate_debug_string(int16_t function_index, int thread_datum,
                              char init)
{
  char *thread;
  int *cur_expr;
  int *arg_count;
  int arg_buf;

  thread = (char *)datum_get(*(data_t **)0x5aa6c4, thread_datum);
  cur_expr = (int *)hs_thread_stack_alloc(thread_datum, 4);
  arg_count = (int *)hs_thread_stack_alloc(thread_datum, 4);
  arg_buf = (int)hs_thread_stack_alloc(thread_datum, 0x80);

  if (function_index < 0x18 || function_index > 0x1a) {
    display_assert("(function_index>=_hs_function_debug_string__first) && "
                   "(function_index<=_hs_function_debug_string__last)",
                   "c:\\halo\\source\\hs\\hs_library_internal_runtime.h", 0x304,
                   1);
    system_exit(-1);
  }

  if (init) {
    char *node = (char *)datum_get(*(data_t **)0x5aa6c8,
                                   *(int *)(*(int *)(thread + 0x10) + 4));
    char *child =
      (char *)datum_get(*(data_t **)0x5aa6c8, *(int *)(node + 0x10));
    *cur_expr = *(int *)(child + 0x8);
    *arg_count = 0;
    csmemset((void *)arg_buf, 0, 0x80);
  }

  if (*cur_expr != -1 && *arg_count < 0x20) {
    int result;
    FUN_000cc1d0(thread_datum, *cur_expr, &result);
    {
      char *expr_node = (char *)datum_get(*(data_t **)0x5aa6c8, *cur_expr);
      *cur_expr = *(int *)(expr_node + 0x8);
    }
    *(int *)(arg_buf + *arg_count * 4) = result;
    *arg_count = *arg_count + 1;
    return;
  }

  {
    typedef void (*debug_string_fn)(int, int);
    debug_string_fn fn;
    if (function_index == 0x18) {
      fn = (debug_string_fn)0x4a650;
    } else if (function_index == 0x19) {
      fn = (debug_string_fn)0x4a680;
    } else if (function_index == 0x1a) {
      fn = (debug_string_fn)0x4a6b0;
    } else {
      display_assert(0, "c:\\halo\\source\\hs\\hs_library_internal_runtime.h",
                     0x330, 1);
      system_exit(-1);
      hs_return(thread_datum, -1);
      return;
    }
    if (fn != NULL)
      fn(*arg_count, arg_buf);
  }
  hs_return(thread_datum, -1);
}

#endif /* HALO_HS_HS_LIBRARY_INTERNAL_RUNTIME_H */
