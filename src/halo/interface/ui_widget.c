/* ui_widgets_initialize — sets up the UI widget subsystem. Allocates a
 * 0x4000-byte block via debug_malloc for the stack memory pool at
 * [0x31e04c], initializes the pool, zeroes the 0x68-byte static widget
 * state block at 0x46cc20, and sets sentinel values (-1) in various
 * 16-bit slots within the state block. The byte at 0x46cc82 records
 * whether the allocation succeeded. The float at 0x46cc4c is set to
 * -1.0f as an initial value. */
void ui_widgets_initialize(void)
{
  int alloc_result;
  int *pool;
  int16_t *ptr_a;
  int16_t *ptr_b;
  bool succeeded;

  succeeded = true;
  alloc_result = (int)debug_malloc(
    0x4000, 0, "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x75);
  if (alloc_result != 0) {
    pool = *(int **)0x31e04c;
    pool[1] = alloc_result;
    pool = *(int **)0x31e04c;
    pool[2] = 0x4000;
  } else {
    succeeded = false;
  }

  stack_memory_pool_initialize(*(void **)0x31e04c);
  csmemset((void *)0x46cc20, 0, 0x68);

  *(int16_t *)0x46cc48 = -1;
  *(int16_t *)0x46cc68 = -1;

  ptr_b = (int16_t *)0x46cc6c;
  ptr_a = (int16_t *)0x46cc50;
  do {
    *ptr_a = -1;
    *ptr_b = -1;
    ptr_a = (int16_t *)((char *)ptr_a + 6);
    ptr_b = (int16_t *)((char *)ptr_b + 4);
  } while ((int)ptr_a < 0x46cc68);

  *(uint8_t *)0x46cc82 = (uint8_t)succeeded;
  *(uint32_t *)0x46cc4c = 0xBF800000; /* -1.0f */
}

void ui_widgets_safe_to_load(bool a1)
{
}

/* ui_widget_realloc — thin wrapper around stack_memory_pool_realloc.
 * Passes the global widget stack memory pool at [0x31e04c] as the
 * first argument, forwarding the caller's block pointer, new size,
 * source file path, and line number for debug tracking. Returns the
 * reallocated block pointer (or NULL on failure). */
void *ui_widget_realloc(int a1, unsigned short a2, const char *a3,
                        unsigned int a4)
{
  return stack_memory_pool_realloc(*(void **)0x31e04c, a1, a2, a3, a4);
}

/* ui_widget_load_progress_widget — stub that fires a priority-2 error
 * stating the old loading progress screen was replaced. The original
 * progress widget system was superseded by the "glowy halo gravy"
 * loading screen; this function is never expected to succeed. */
void ui_widget_load_progress_widget(void)
{
  error(2, "the old loading progress screen has been replaced with glowy "
           "halo gravy");
}

/* display_error_when_main_menu_loaded — queues a single error message handle
 * to be shown the next time the main menu shell window is loaded. The
 * 16-bit slot at word_46CC48 holds the pending handle; -1 means "no
 * pending error". main_screen_shell_load consumes the slot by passing the
 * handle to the error-dialog routine and resetting it back to -1. Only
 * one queued message is supported at a time; subsequent calls while a
 * message is pending are dropped with a priority-2 warning. */
void display_error_when_main_menu_loaded(int16_t error_handle)
{
  if (word_46CC48 == -1) {
    word_46CC48 = error_handle;
    return;
  }
  error(2, "there is already an error message queued for display at the "
           "main menu; ignoring this one");
}

void ui_widgets_disable_pause_game(int duration_ticks)
{
  assert_halt(duration_ticks >= 0);
  dword_46CC44 = duration_ticks;
}

/* ui_widget_close — tears down a single UI widget and frees its memory.
 * Handles the "widget deleted" event handlers (type 0x19) from the widget's
 * DeLa tag definition, firing each matching handler via
 * ui_widget_event_handler_dispatch and optionally spawning replacement widgets.
 * Manages the pause counter (if the widget pauses the game), unlinks the
 * widget from its sibling/parent chains, performs type-specific cleanup
 * (text data for type 1, list data for types 2-3), frees the widget from
 * the stack memory pool, and clears any root widget slot that pointed to it.
 * The being_deleted flag at +0x14 prevents re-entrant closing. */
void ui_widget_close(void *widget)
{
  int *w;
  int tag_data;
  int handler_offset;
  int i;
  uint8_t *handler;
  bool widget_deleted;
  bool handler_result;
  int16_t widget_type;
  int prev;
  int next;
  int parent;
  char *tag_name;
  int idx;

  w = (int *)widget;

  assert_halt(widget && *(uint8_t *)0x46cc82);

  /* already being deleted — bail out */
  if (*(uint8_t *)((char *)w + 0x14) != 0)
    return;

  /* mark as being deleted */
  *(uint8_t *)((char *)w + 0x14) = 1;

  /* notify player control if this widget has a local player and no parent */
  if (*(int16_t *)((char *)w + 0x8) != -1 && w[0xc] == 0) {
    player_control_set_action_flags(*(int16_t *)((char *)w + 0x8), 0xfff, 1);
  }

  /* look up the DeLa (UI widget definition) tag */
  tag_data = (int)tag_get(0x44654c61, w[0]);

  /* iterate over event handlers, firing "widget deleted" (type 0x19) ones */
  i = 0;
  if (*(int *)(tag_data + 0x54) > 0) {
    handler_offset = 0;
    do {
      handler = (uint8_t *)(*(int *)(tag_data + 0x58) + handler_offset);

      if (*(int16_t *)(handler + 0x4) == 0x19 && (int8_t)handler[0] < 0) {
        widget_deleted = false;
        handler_result = ui_widget_event_handler_dispatch(
          w, 0, *(uint16_t *)(handler + 0x6), &widget_deleted);

        assert_halt_msg(!widget_deleted,
                        "a 'widget deleted' event handler tried to delete the "
                        "widget being deleted!");

        if (handler_result && (handler[0] & 0x8) != 0 &&
            *(int *)(handler + 0x14) != -1) {
          if (ui_widget_spawn_from_event_handler(w, *(int *)(handler + 0x14)) ==
              0) {
            error(2, "event handler failed to spawn widget");
          }
        }
      }

      i++;
      handler_offset += 0x48;
    } while (i < *(int *)(tag_data + 0x54));
  }

  /* manage the pause counter */
  if (*(uint8_t *)((char *)w + 0x13) == 1) {
    assert_halt_msg(*(int16_t *)0x46cc4a >= 1,
                    "widget pause counter out of whack");

    (*(int16_t *)0x46cc4a)--;

    if (*(int16_t *)0x46cc4a == 0) {
      if (game_time_get_paused()) {
        game_time_set_paused(0);
        if (*(uint8_t *)0x46cc88 != 0) {
          main_reset_player_actions();
          game_time_dispose_from_old_map();
          game_time_initialize_for_new_map();
          game_time_start();
        }
      }
      if (*(uint8_t *)0x46cc87 == 1) {
        sound_set_music_enabled(0);
        *(uint8_t *)0x46cc87 = 0;
      }
    }
  }

  /* close all children of this widget */
  ui_widget_close_children(w);

  /* unlink from prev sibling */
  prev = w[0xa];
  if (prev != 0) {
    *(int *)(prev + 0x2c) = w[0xb];
  }

  /* unlink from next sibling */
  next = w[0xb];
  if (next != 0) {
    *(int *)(next + 0x28) = w[0xa];
  }

  /* update parent's first_child if needed */
  parent = w[0xc];
  if (parent != 0 && *(int *)(parent + 0x34) == (int)w) {
    *(int *)(parent + 0x34) = w[0xb];
  }

  /* type-specific cleanup */
  widget_type = *(int16_t *)((char *)w + 0xe);
  if (widget_type == 1) {
    /* text widget: free text data */
    if (w[0xf] != 0) {
      stack_memory_pool_deallocate(*(void **)0x31e04c, (void *)w[0xf]);
    }
  } else if (widget_type > 1 && widget_type < 4) {
    /* list widget (type 2 or 3): warn about possible leak, free skin data,
     * recursively close header widget */
    if (w[0x10] != 0) {
      tag_name = (char *)(tag_data + 4);
      if (tag_name == NULL) {
        tag_name = "<unknown>";
      }
      error(2,
            "###WARNING: possible memory leak disposing of a list widget (%s)",
            tag_name);
    }
    if (w[0x13] != 0) {
      stack_memory_pool_deallocate(*(void **)0x31e04c, (void *)w[0x13]);
    }
    if (w[0x12] != 0) {
      ui_widget_close((void *)w[0x12]);
    }
  }

  /* free the widget itself */
  stack_memory_pool_deallocate(*(void **)0x31e04c, w);

  /* clear root widget slot if this widget was a root */
  for (idx = 0; idx < 4; idx++) {
    if (*(int *)(0x46cc20 + idx * 4) == (int)w) {
      *(int *)(0x46cc20 + idx * 4) = 0;
      return;
    }
  }
}

/* ui_widgets_close_all — iterates over the 4 UI widget stacks and tears
 * them down. For each stack, closes the root widget via ui_widget_close
 * (0xe5620), then walks the linked list at 0x46cc30[i] and deallocates
 * each widget node from the stack memory pool at [0x31e04c]. The list
 * is linked through offset +0xc in each widget node. */
void ui_widgets_close_all(void)
{
  int *list_heads;
  int widget;
  int next;
  void *pool;

  list_heads = (int *)0x46cc30;
  do {
    /* close the root widget for this stack if present */
    if (list_heads[-4] != 0) {
      ui_widget_close((void *)list_heads[-4]);
    }
    /* walk the linked list at list_heads[i], freeing each node */
    widget = *list_heads;
    if (widget != 0) {
      while (widget != 0) {
        pool = *(void **)0x31e04c;
        next = *(int *)(widget + 0xc);
        *list_heads = next;
        stack_memory_pool_deallocate(pool, (void *)widget);
        widget = *list_heads;
      }
    }
    list_heads++;
  } while ((int)list_heads < 0x46cc40);
}

/* ui_widgets_dispose — tears down the UI widget system. Closes all open
 * widgets, frees the widget memory pool allocated by ui_widgets_initialize
 * (0x4000 bytes at [ptr+4]), zeros the pool pointer and size fields, and
 * clears the 0x68-byte static widget state block at 0x46cc20. Called during
 * engine shutdown. */
void ui_widgets_dispose(void)
{
  int *ptr;

  ui_widgets_close_all();

  ptr = *(int **)0x31e04c;
  if (ptr[1] != 0) {
    debug_free((void *)ptr[1], "c:\\halo\\SOURCE\\interface\\ui_widget.c",
               0x76);
  }
  ptr[1] = 0;
  ptr[2] = 0;
  csmemset((void *)0x46cc20, 0, 0x68);
}

/* main_screen_shell_load — loads the main menu shell UI. On the first boot
 * (when the first-run flag at 0x31e050 is set), plays the intro bink movie
 * and kicks off filesystem checks / saved game enumeration. If the command
 * line is "xdemo", the intro movie is skipped. After the bink + fs-check
 * phase, loads the main menu widget ("ui\shell\main_menu\main_menu"),
 * displays any queued error message (word_46CC48), starts title music if
 * not already playing, and initializes the virtual keyboard. The first-run
 * flag is cleared at the end so subsequent calls skip the intro path. */
void main_screen_shell_load(void)
{
  bool play_main_menu;
  char *command_line;
  int widget;

  play_main_menu = true;
  assert_halt(*(uint8_t *)0x46cc82);

  *(uint8_t *)0x46cc85 = 0;

  if (*(uint8_t *)0x31e050 == 1) {
    command_line = shell_get_command_line();
    if (command_line == NULL) {
      goto play_intro;
    }
    if (((int (*)(const char *, const char *))0x1dd801)(command_line,
                                                        "xdemo") != 0) {
    play_intro:
      bink_playback_start("d:\\bink\\intro.bik", 0xe6);
      play_main_menu = false;
      if (bink_playback_active() == 0) {
        play_main_menu = true;
      }
    } else {
      error(2, "xbox command line= '%s'", command_line);
    }
    ui_widget_begin_filesystem_checks();
    input_abstraction_mark_time();
    if (!play_main_menu)
      goto done;
  }

  event_manager_mark_time();
  ui_widgets_close_all();

  widget = (int)ui_widget_load_by_name_or_tag("ui\\shell\\main_menu\\main_menu",
                                              -1, 0, -1, -1, -1, -1);
  if (widget == 0) {
    error(2, "failed to load main screen shell window");
  }

  if (word_46CC48 != -1) {
    ui_widget_display_error(word_46CC48, -1, 1, 0);
    word_46CC48 = -1;
  }

  if (*(uint8_t *)0x46cc86 == 0) {
    ui_widget_start_title_music();
  }

  ui_widget_clear_last_error_index();

done:
  if (!((bool (*)(void))0xf53a0)()) {
    error(2, "failed to initialize the virtual keyboard");
  }
  *(uint8_t *)0x31e050 = 0;
}

typedef struct ui_widget_process_data {
  int16_t unk0;
  int16_t unk2;
  int16_t unk4;
  int16_t unk6;
} ui_widget_process_data_t;

typedef struct ui_widget_deferred_error {
  int16_t error_handle;
  int16_t local_player_index;
  uint8_t a3;
  uint8_t a4;
} ui_widget_deferred_error_t;

/* Pending-load nodes on 0x46cc30..0x46cc3c are pushed by
 *
 * ui_widget_load_by_name_or_tag (0xe84e0) and popped by this helper.
 * The
 * callee expects the queue head in EDI and the output record in ESI. */
typedef struct ui_widget_pending_load {
  int tag_index;
  int a6;
  int16_t a7;
  int16_t widget_stack;
} ui_widget_pending_load_t;

static void ui_widget_pending_load_pop(int *head,
                                       ui_widget_pending_load_t *record)
{
  int *_edi = head;
  ui_widget_pending_load_t *_esi = record;

  __asm__ __volatile__("call *%[fn]"
                       : "+D"(_edi), "+S"(_esi)
                       : [fn] "r"((void *)0xe4770)
                       : "eax", "ecx", "edx", "memory", "cc");
}

static void ui_widget_pending_load_apply(int a6, int widget, int16_t a7)
{
  int _eax = a6;

  __asm__ __volatile__(
    "pushl %[a7]\n\t"
    "pushl %[widget]\n\t"
    "call *%[fn]\n\t"
    "addl $8, %%esp"
    : "+a"(_eax)
    : [a7] "r"((int)a7), [widget] "r"(widget), [fn] "r"((void *)0xe5090)
    : "ecx", "edx", "memory", "cc");
}

void process_ui_widgets(void)
{
  uint8_t active_widget_stacks[4];
  ui_widget_pending_load_t pending_load;
  ui_widget_process_data_t process_data;
  ui_widget_deferred_error_t *deferred_error;
  int *widget_roots;
  void *widget_tag;
  int widget;
  void *loaded_widget;
  int stack_index;
  uint8_t any_active_widget_stack;
  uint8_t did_work;
  uint8_t blocked_by_pause;
  uint8_t handled;

  did_work = 0;
  if (*(uint8_t *)0x46cc82 == 0) {
    display_assert("widget_globals.initialized",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x284, true);
    ((void (*)(int))0x8e2f0)(-1);
  }

  *(uint32_t *)0x46cc40 = ((uint32_t(*)(void))0x8e370)();
  if (*(int *)0x46cc7c != 0) {
    if (((char (*)(int))0x81720)(*(int *)0x46cc7c) != 0) {
      ((void (*)(int))0x81770)(*(int *)0x46cc7c);
      *(int *)0x46cc7c = 0;
      ((void (*)(int))0xe3e10)(0);
      if (*(int16_t *)0x46cc80 == 1) {
        if (((char (*)(void))0x1c5980)() != 0) {
          ((void (*)(void))0x1c62a0)();
        }
        ((void (*)(int16_t, int))0xe8c90)(0x21, 1);
        return;
      }
      if (*(int16_t *)0x46cc80 == 2) {
        if (((char (*)(void))0x1c5980)() != 0) {
          ((void (*)(void))0x1c62a0)();
        }
        ((void (*)(int16_t, int))0xe8c90)(0x22, 1);
        return;
      }
    }
    return;
  }

  if (((char (*)(void))0xe1ce0)() != 0) {
    return;
  }
  if (((char (*)(void))0xf5640)() != 0) {
    ((void (*)(void))0xf6740)();
    ((void (*)(void))0xdc220)();
    return;
  }
  if (((char (*)(void))0xdc070)() != 0) {
    ((void (*)(void))0xdc140)();
    return;
  }

  if (*(int16_t *)0x46cc68 != -1) {
    ((void (*)(int16_t, int))0xe8c90)(*(int16_t *)0x46cc68,
                                      *(uint8_t *)0x46cc6a);
    *(int16_t *)0x46cc68 = -1;
    return;
  }

  if ((*(int16_t *)0x46cc50 == -1) && (*(int16_t *)0x46cc56 == -1) &&
      (*(int16_t *)0x46cc5c == -1) && (*(int16_t *)0x46cc62 == -1)) {
    blocked_by_pause = ((char (*)(void))0xe9080)() != 0;

    active_widget_stacks[0] =
      (*(int *)0x46cc20 != 0) && (*(uint8_t *)(*(int *)0x46cc20 + 0x15) == 1);
    active_widget_stacks[1] =
      (*(int *)0x46cc24 != 0) && (*(uint8_t *)(*(int *)0x46cc24 + 0x15) == 1);
    active_widget_stacks[2] =
      (*(int *)0x46cc28 != 0) && (*(uint8_t *)(*(int *)0x46cc28 + 0x15) == 1);
    active_widget_stacks[3] =
      (*(int *)0x46cc2c != 0) && (*(uint8_t *)(*(int *)0x46cc2c + 0x15) == 1);
    any_active_widget_stack = active_widget_stacks[0] |
                              active_widget_stacks[1] |
                              active_widget_stacks[2] | active_widget_stacks[3];

    widget_roots = (int *)0x46cc20;
    for (stack_index = 0; stack_index < 4; stack_index++, widget_roots++) {
      widget = *widget_roots;
      if (active_widget_stacks[stack_index] == 1) {
        if ((widget == 0) || (*(uint8_t *)(widget + 0x15) != 1)) {
          continue;
        }
      } else if (*(uint8_t *)0x46cc88 == 0) {
        if (widget == 0) {
          continue;
        }
      } else {
        if ((widget == 0) || (any_active_widget_stack != 0)) {
          continue;
        }
      }

      widget_tag = tag_get(0x44654c61, *(int *)widget);
      process_data.unk0 = 0;
      process_data.unk2 = 0;
      process_data.unk4 = 0;
      process_data.unk6 = 0;

      if ((*(uint8_t *)0x46cc85 == 0) &&
          (((char (*)(void *, uint16_t))0xdc250)(
             &process_data, *(uint16_t *)(widget + 8)) != 0)) {
        do {
          handled = 0;
          if (blocked_by_pause == 0) {
            ((void (*)(int, void *, void *, uint8_t *))0xe7d00)(
              widget, widget_tag, &process_data, &handled);
          }
          if ((handled == 1) || (widget != *widget_roots)) {
            break;
          }
        } while (((char (*)(void *, uint16_t))0xdc250)(
                   &process_data, *(uint16_t *)(widget + 8)) != 0);
      } else if (blocked_by_pause == 0) {
        process_data.unk2 = *(uint16_t *)(widget + 8);
        handled = 0;
        ((void (*)(int, void *, void *, uint8_t *))0xe7d00)(
          widget, widget_tag, &process_data, &handled);
      }

      did_work = 1;
      if ((*widget_roots == 0) && (widget_roots[4] != 0)) {
        ui_widget_pending_load_pop(&widget_roots[4], &pending_load);
        if (pending_load.tag_index != -1) {
          loaded_widget = ui_widget_load_by_name_or_tag(
            0, pending_load.tag_index, 0, pending_load.widget_stack, -1, -1,
            -1);
          if (loaded_widget != 0) {
            ui_widget_pending_load_apply(pending_load.a6, (int)loaded_widget,
                                         pending_load.a7);
          }
        }
      }
    }

    if (did_work != 0) {
      ((void (*)(void))0xdc220)();
      return;
    }
    return;
  }

  deferred_error = (ui_widget_deferred_error_t *)0x46cc50;
  while ((int)deferred_error < 0x46cc68) {
    if (deferred_error->error_handle != -1) {
      if ((*(uint8_t *)0x46cc88 == 0) && (((char (*)(void))0x12a000)() == 0) &&
          (((int (*)(void))0xb5aa0)() < 0x1e)) {
        error(2, "waiting for %d ticks before displaying deferred errors",
              0x1e);
      } else {
        ((void (*)(int16_t, int16_t, char, char))0xe8910)(
          deferred_error->error_handle, deferred_error->local_player_index,
          (char)deferred_error->a3, (char)deferred_error->a4);
        deferred_error->error_handle = -1;
      }
    }
    deferred_error++;
  }
}
