/* HaloScript runtime — thread management and script execution. */

/* Dispose runtime state from old map: invalidate thread data and
 * clean up any allocated script globals. */
void hs_runtime_dispose_from_old_map(void)
{
  int16_t idx;
  char *data;

  ((void (*)(void *))0x119550)(*(void **)0x5aa6c4);

  idx = *(int16_t *)0x27d504;
  data = *(char **)0x5aa6c0;
  while (idx < *(int16_t *)(data + 0x2e)) {
    if (((int (*)(void *, int))0x119270)(data, (int)idx) != 0)
      ((void (*)(void *, int))0x1196d0)(data, (int)idx);
    idx++;
    data = *(char **)0x5aa6c0;
  }

  *(uint8_t *)0x46b810 = 0;
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
  ((void (*)(void *))0x119b20)(*(void **)0x5aa6c4); /* data_delete_all */
  *(uint8_t *)0x46b810 = 1;
  *(int16_t *)0x46b812 = -1;

  /* Phase 2: allocate the internal initialization thread. */
  thread_index =
    ((int (*)(void *))0x119610)(*(void **)0x5aa6c4); /* data_new_at_index */
  if (thread_index != -1) {
    internal_thread = (char *)((int (*)(void *, int))0x119320)(
      *(void **)0x5aa6c4, thread_index); /* datum_get */
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
    scenario = (char *)((int (*)(void))0x18e380)(); /* global_scenario_get */
    internal_thread = (char *)((int (*)(void *, int))0x119320)(
      *(void **)0x5aa6c4, thread_index); /* datum_get */

    loop_var = 0;
    if (*(int *)(scenario + 0x4a8) > 0) {
      loop_idx = 0;
      do {
        /* Get the current script element from the scripts block. */
        {
          char *block_base =
            (char *)((int (*)(void))0x18e380)(); /* global_scenario_get */
          block_base += 0x4a8;
          script_element = (char *)((int (*)(void *, int, int))0x19b210)(
            block_base, loop_idx, 0x5c); /* tag_block_get_element */
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

          /* Allocate the syntax datum with magic handle. */
          ((int (*)(void *, int))0x119570)(
            *(void **)0x5aa6c0,
            (int)(datum_idx | 0xaced0000)); /* data_new_datum */

          /* Re-derive datum_idx (same logic, needed after the call). */
          if (loop_var & (int16_t)0x8000)
            datum_idx = raw_idx;
          else
            datum_idx = (int)*(int16_t *)0x27d504 + raw_idx;

          datum_ptr = (char *)((int (*)(void *, int))0x119320)(
            *(void **)0x5aa6c0, datum_idx); /* datum_get */
        }

        /* Reset internal thread state and call hs_default_value.
         * hs_default_value (0xcc1d0) takes EAX=thread_index,
         * stack args: (hs_type, dest_ptr). */
        *(int *)(internal_thread + 0x4) = -1;
        {
          char *sf = *(char **)(internal_thread + 0x10);
          *(int16_t *)(sf + 0xc) = 0;
        }
        {
          int _eax = thread_index;
          int type_arg = *(int *)(script_element + 0x28);
          int dest_arg = (int)(datum_ptr + 4);
          asm volatile("pushl %[dest]\n\t"
                       "pushl %[type]\n\t"
                       "call *%[fn]\n\t"
                       "addl $8, %%esp"
                       : "+a"(_eax)
                       : [fn] "r"((void *)0xcc1d0), [type] "r"(type_arg),
                         [dest] "r"(dest_arg)
                       : "ecx", "edx", "memory", "cc");
        }

        /* If the script was successfully parsed (bit 0 of byte +3),
         * execute it. */
        if (*(uint8_t *)(internal_thread + 0x3) & 1) {
          /* hs_execute_thread (0xcd840) takes EAX=thread_index. */
          {
            int _eax = thread_index;
            asm volatile("call *%[fn]"
                         : "+a"(_eax)
                         : [fn] "r"((void *)0xcd840)
                         : "ecx", "edx", "ebx", "esi", "edi", "memory", "cc");
          }

          /* If this is a global initialization script (type == 0x17),
           * store the result back into the globals. */
          if (*(int16_t *)(script_element + 0x20) == 0x17) {
            /* hs_global_value_store (0xcb230) takes EDI=loop_var. */
            {
              int _edi = (int)loop_var;
              asm volatile("call *%[fn]"
                           : "+D"(_edi)
                           : [fn] "r"((void *)0xcb230)
                           : "eax", "ecx", "edx", "ebx", "esi", "memory", "cc");
            }

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

              datum_ptr = (char *)((int (*)(void *, int))0x119320)(
                *(void **)0x5aa6c0, datum_idx); /* datum_get */
              /* hs_evaluate (0xce350) takes one stack arg. */
              ((void (*)(int))0xce350)(*(int *)(datum_ptr + 0x4));
            }
            /* Restore internal_thread (original saved in [EBP-0x10],
             * we re-derive via datum_get). */
            internal_thread = (char *)((int (*)(void *, int))0x119320)(
              *(void **)0x5aa6c4, thread_index);
          }

          /* Assert: global init scripts must not sleep.
           * hs_get_thread_script_name (0xcaa80) takes ESI=thread_index
           * as register arg and returns the script name string. */
          if (*(int *)(internal_thread + 0x8) != 0) {
            char *script_name;
            {
              int _esi = thread_index;
              asm volatile("call *%[fn]"
                           : "+S"(_esi), "=a"(script_name)
                           : [fn] "r"((void *)0xcaa80)
                           : "ecx", "edx", "edi", "memory", "cc");
            }
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

        /* hs_global_value_finish (0xcb7b0) takes EBX=loop_var. */
        {
          int _ebx = (int)loop_var;
          asm volatile("call *%[fn]"
                       : "+b"(_ebx)
                       : [fn] "r"((void *)0xcb7b0)
                       : "eax", "ecx", "edx", "esi", "edi", "memory", "cc");
        }

        loop_var++;
        loop_idx = (int)(int16_t)loop_var;
        scenario = (char *)((int (*)(void))0x18e380)();
      } while (loop_idx < *(int *)(scenario + 0x4a8));
    }

    /* Verify internal thread type and delete it. */
    internal_thread = (char *)((int (*)(void *, int))0x119320)(
      *(void **)0x5aa6c4, thread_index); /* datum_get */
    if (*(uint8_t *)(internal_thread + 0x2) == 0) {
      display_assert(
        "hs_thread_get(thread_index)->type!=_hs_thread_type_script",
        "c:\\halo\\SOURCE\\hs\\hs_runtime.c", 0x290, true);
      system_exit(-1);
    }
    ((void (*)(void *, int))0x1196d0)(*(void **)0x5aa6c4,
                                      thread_index); /* datum_delete */

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
          char *script = (char *)((int (*)(void *, int, int))0x19b210)(
            scripts_block, script_idx, 0x5c); /* tag_block_get_element */
          int16_t script_type = *(int16_t *)(script + 0x20);
          if (script_type != 3 && script_type != 4) {
            /* hs_thread_new (0xca940): EBX=script_index, stack arg=type.
             * Returns thread index in EAX, -1 on failure. */
            int result;
            {
              int _ebx = script_idx;
              int _type = 0;
              asm volatile("pushl %[type]\n\t"
                           "call *%[fn]\n\t"
                           "addl $4, %%esp"
                           : "+b"(_ebx), "=a"(result)
                           : [fn] "r"((void *)0xca940), [type] "r"(_type)
                           : "ecx", "edx", "esi", "edi", "memory", "cc");
            }
            if (result == -1) {
              ((void (*)(uint16_t, const char *, ...))0x8f390)(
                0, "ran out of script threads.");
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
