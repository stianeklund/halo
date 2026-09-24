#include "x87_math.h"

/* event_controller_index_compatible_with_widget (0xe3b80) — true if the
 * widget accepts input from any controller (local_player_index == -1, at
 * widget+8) or if the widget's local_player_index matches the event's
 * controller_index (at event+2). Same check is inlined below as
 * "allowed_player" in widget_instance_process_one_event_recursive. */
int event_controller_index_compatible_with_widget(void *event, void *widget)
{
  short player_index;

  player_index = *(short *)((char *)widget + 8);
  if (player_index == -1 || player_index == *(short *)((char *)event + 2)) {
    return 1;
  }
  return 0;
}

/* set_ui_plasma_effect_color (0xe3bb0) — stores four caller-supplied dword
 * values into four consecutive UI plasma-effect-color globals at
 * 0x5aa460-0x5aa46c. No callers found in the binary (xrefs empty) and no
 * callees; the values are copied verbatim via plain MOV (no FPU/other
 * interpretation), so whether they are int or float components is not
 * proven by this evidence — kept as raw dwords. */
void set_ui_plasma_effect_color(uint32_t component_0, uint32_t component_1,
                                uint32_t component_2, uint32_t component_3)
{
  *(uint32_t *)0x5aa460 = component_0;
  *(uint32_t *)0x5aa464 = component_1;
  *(uint32_t *)0x5aa468 = component_2;
  *(uint32_t *)0x5aa46c = component_3;
}

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

/* ui_widgets_set_fade_value — stores the caller's value into the fade
 * float at 0x46cc4c (the same global ui_widgets_initialize seeds to
 * -1.0f). A raw 4-byte MOV in the original; written as a float store
 * to preserve the bit pattern. */
void ui_widgets_set_fade_value(float value)
{
  *(float *)0x46cc4c = value;
}

/* ui_widget_debug_show_path — sets the debug overlay flag at 0x46cc84 that
 * controls whether render_ui_widgets() draws each on-screen widget's tag
 * name in the small debug font (see the render_ui_widgets comment below). */
void ui_widget_debug_show_path(unsigned char value)
{
  *(uint8_t *)0x46cc84 = value;
}

/* widget_instance_count_children (0xe3cb0) — counts widget's children by
 * walking the next_sibling chain (+0x2c) starting from first_child (+0x34),
 * the same fields widget_instance_get_nth_child below walks. Unlike that
 * function, this one does NOT assert on a NULL widget: a NULL widget or an
 * empty first_child both just return 0. */
int widget_instance_count_children(void *widget)
{
  int count;
  void *child;

  count = 0;
  if (widget != NULL) {
    child = *(void **)((char *)widget + 0x34);
    if (child != NULL) {
      do {
        child = *(void **)((char *)child + 0x2c);
        count = count + 1;
      } while (child != NULL);
    }
  }
  return count;
}

/* widget_instance_get_nth_child — walks the first_child linked list of
 * widget (offset +0x34) following next_sibling (+0x2c) n times, returning
 * the widget reached (or NULL if the chain runs out before n steps).
 * n <= 0 returns first_child unchanged. Asserts widget is non-NULL. */
void *widget_instance_get_nth_child(void *widget, int n)
{
  void *child;
  int i;

  if (widget == NULL) {
    display_assert("widget", "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x41a,
                   true);
    system_exit(-1);
  }

  child = *(void **)((char *)widget + 0x34);
  i = 0;
  if (0 < n) {
    do {
      if (child == NULL)
        return child;
      child = *(void **)((char *)child + 0x2c);
      i = i + 1;
    } while (i < n);
  }
  return child;
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

/* widget_free — releases a widget node back to the global widget stack
 * memory pool at [0x31e04c]. Thin wrapper around
 * stack_memory_pool_deallocate, same pool used by ui_widget_realloc above
 * and the other stack_memory_pool_deallocate call sites in this file. */
void widget_free(void *widget)
{
  stack_memory_pool_deallocate(*(void **)0x31e04c, widget);
}

/* ui_widgets_active — reports whether the widget subsystem is initialized
 * (0x46cc82) and at least one of the 4 widget root stack slots
 * (0x46cc20..0x46cc2c, same slots as main_screen_shell_begin_fade and the
 * other 0x46cc20[] scans in this file) holds a non-NULL root widget.
 * Returns false immediately if the subsystem hasn't been initialized;
 * otherwise scans the root slots and returns true on the first non-zero
 * slot, false if all 4 are zero.
 * 0xe3d70: MOV CL,[0x46cc82]; TEST CL,CL; JZ ret-false; loop: CMP
 * dword[ECX],0; JNZ ret-true (AL=1); ADD ECX,4; CMP ECX,0x46cc30;
 * JL loop; else fall through to RET with AL still 0 from the entry
 * XOR AL,AL. */
bool ui_widgets_active(void)
{
  int *slot;
  bool active;

  active = false;
  if (*(uint8_t *)0x46cc82 != 0) {
    for (slot = (int *)0x46cc20; (int)slot < 0x46cc30; slot++) {
      if (*slot != 0) {
        return true;
      }
    }
  }

  return active;
}

/* ui_widgets_inhibit_processing — sets or clears the events-suppressed
 * flag at 0x46cc85 in the widget globals block. When suppressed, the
 * per-frame event dispatch in process_ui_widgets skips input processing.
 * Asserts that the widget subsystem has been initialized (0x46cc82). */
void ui_widgets_inhibit_processing(bool suppress)
{
  assert_halt(*(uint8_t *)0x46cc82);
  *(uint8_t *)0x46cc85 = (uint8_t)suppress;
}

/* widget_instance_get_topmost_parent (0xe4310) — walks the parent chain (field
 * +0x30, the same field walked by
 * widget_instance_give_focus_directly/widget_instance_give_focus_by_tag above
 * and by the inline root-walk at
 * ui_widget_delete(widget_instance_get_topmost_parent(w)) below) from widget up
 * to the top-most ancestor with no parent, and returns that root.
 * 0xe4313-0xe432a: MOV EAX,[EBP+8]; MOV ECX,[EAX+0x30]; TEST/JZ; loop MOV
 * EAX,ECX; MOV ECX,[EAX+0x30]; TEST/JNZ while ECX!=0; RET EAX. */
void *widget_instance_get_topmost_parent(void *widget)
{
  void *parent;

  parent = *(void **)((char *)widget + 0x30);
  while (parent != NULL) {
    widget = parent;
    parent = *(void **)((char *)parent + 0x30);
  }

  return widget;
}

/* widget_instance_get_child_index_from_parent (0xe4330) - returns widget's
 * zero-based position in its parent's child list, or -1 when widget has no
 * parent (+0x30 NULL), the parent has no children (+0x34 NULL), or widget is
 * not found in the chain. Walks first_child (+0x34) then next_sibling (+0x2c),
 * the same fields widget_instance_count_children/get_nth_child walk above.
 * 000e433a: MOV ECX,[ESI+0x30]; OR EAX,-1; TEST/JZ ret; MOV ECX,[ECX+0x34];
 * XOR EDX,EDX; TEST/JZ ret; loop: CMP ECX,ESI; JZ (MOV EAX,EDX); MOV
 * ECX,[ECX+0x2c]; INC EDX; TEST ECX,ECX; JNZ loop; RET with EAX still -1. */
int widget_instance_get_child_index_from_parent(void *widget)
{
  void *parent;
  void *child;
  int index;
  int result;

  result = -1;
  parent = *(void **)((char *)widget + 0x30);
  if (parent != NULL) {
    child = *(void **)((char *)parent + 0x34);
    index = 0;
    while (child != NULL) {
      if (child == widget) {
        result = index;
        break;
      }
      child = *(void **)((char *)child + 0x2c);
      index = index + 1;
    }
  }

  return result;
}

/* widget_instance_set_visibility_recursive (0xe4370) — sets widget's
 * visible flag (+0x10, the same flag checked by the text-box render path
 * above as "return early if 0") to `visible`, then recurses over every
 * child in the first_child (+0x34) / next_sibling (+0x2c) list, same
 * fields widget_instance_count_children/widget_instance_get_nth_child
 * walk above. Asserts widget is non-NULL (same "widget" tag / file /
 * halt=true display_assert call as widget_instance_get_nth_child). */
void widget_instance_set_visibility_recursive(void *widget, bool visible)
{
  void *child;

  if (widget == NULL) {
    display_assert("widget", "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x743,
                   true);
    system_exit(-1);
  }

  *(uint8_t *)((char *)widget + 0x10) = (uint8_t)visible;

  child = *(void **)((char *)widget + 0x34);
  while (child != NULL) {
    widget_instance_set_visibility_recursive(child, visible);
    child = *(void **)((char *)child + 0x2c);
  }
}

/* main_menu_active — sets or clears the "main menu active" byte at 0x46cc88,
 * checked by main_menu_screen_is_active() (below) and main_menu_is_active()
 * (0xe43e0). Called from main.c's main_menu_unload/main_menu_load
 * teardown/setup paths with a literal false/true argument. */
void main_menu_active(bool active)
{
  *(uint8_t *)0x46cc88 = (uint8_t)active;
}

/* main_menu_is_active — reads the "main menu active" byte at 0x46cc88 set by
 * main_menu_active() (above). 0xe43e0: `MOV AL,[0x46cc88]; RET` — a single
 * byte load truncated to bool, no other logic. */
bool main_menu_is_active(void)
{
  return (bool)(*(uint8_t *)0x46cc88);
}

bool main_menu_screen_is_active(void)
{
  int root_widget;

  if (*(uint8_t *)0x46cc88 == 1) {
    root_widget = *(int *)0x46cc20;
    if (root_widget != 0) {
      if (csstrcmp(*(const char **)(root_widget + 4), "the_main_menu") == 0) {
        return true;
      }
    }
  }

  return false;
}

/* ui_set_next_level (0xe4420) — selects what happens after the current solo
 * level is completed. The incoming slot is read as a 16-bit index
 * (0xe4423-0xe4426: MOV ECX,[EBP+8]; MOVSX EAX,CX), so only the low word is
 * significant:
 *   -1        → roll the credits (tail JMP 0x102070 at 0xe4462)
 *   [0, 9]    → look the solo level name up by index and arm it as the next
 *               map, then disallow persistent storage (tail JMP 0xfff90)
 *   otherwise → priority-2 "unknown level" error and fall back to the main
 *               menu (tail JMP 0x100620)
 * The single ADD ESP,0x8 at 0xe4443 is coalesced cleanup for the index push
 * at 0xe4437 and the name push at 0xe443d. */
void ui_set_next_level(int level_index)
{
  int16_t index;

  index = (int16_t)level_index;
  if (index != -1) {
    if (index >= 0 && index <= 9) {
      main_set_map_name(main_get_solo_level_name(index));
      main_disallow_persistent_storage();
      return;
    }
    error(2, "unknown level");
    main_goto_main_menu();
    return;
  }
  main_roll_credits();
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

bool filesystem_check_thread_is_active(void)
{
  return *(int *)0x46cc7c != 0;
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

/* Deferred per-local-player error slots at 0x46cc50, stride 6 bytes:
 * word error_handle (-1 == slot empty), word local_player_index, then the
 * two flag bytes forwarded to the error screen. */
typedef struct ui_widget_deferred_error {
  int16_t error_handle;
  int16_t local_player_index;
  uint8_t a3;
  uint8_t a4;
} ui_widget_deferred_error_t;

/* display_error_deferred — queues one error message per local player, to be
 * dispatched by the deferred-error sweep in process_ui_widgets(). A
 * player_index of -1 (no specific local player) uses slot 0 without the
 * range assert; any other value must be a valid local player index. If the
 * player's slot is already occupied the request is dropped with a
 * priority-2 warning, same shape as display_error_when_main_menu_loaded(). */
__declspec(noinline) void
display_error_deferred(int error_code, int player_index, bool a3, bool a4)
{
  ui_widget_deferred_error_t *deferred_errors;
  int index;

  deferred_errors = (ui_widget_deferred_error_t *)0x46cc50;
  if ((int16_t)player_index == -1) {
    index = 0;
  } else {
    index = (int16_t)player_index;
    if ((index < 0) || (index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)) {
      display_assert("(index>=0) && (index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x8f0, true);
      system_exit(-1);
    }
  }

  if (deferred_errors[index].error_handle == -1) {
    deferred_errors[index].error_handle = (int16_t)error_code;
    deferred_errors[index].local_player_index = (int16_t)player_index;
    deferred_errors[index].a3 = (uint8_t)a3;
    deferred_errors[index].a4 = (uint8_t)a4;
    return;
  }
  error(2,
        "there is already a deferred error message for local player %d; "
        "ignoring this one",
        index);
}

/* display_error_abort_to_dashboard_deferred (0xe4590) — queues a single
 * "abort to dashboard" error (the error_handle/allow_abort pair consumed
 * by ui_widget_load_error_screen(), whose "error_abort_to_dashboard"
 * widget name this function's name mirrors) for the deferred-dispatch
 * check on the next per-frame widget update (word at 0x46cc68 == -1
 * means "slot empty", byte at 0x46cc6a is the paired allow_abort flag).
 * If the slot is already occupied, the request is dropped with a
 * priority-2 warning, same "queue is full, ignore" shape as
 * display_error_when_main_menu_loaded() above. Only caller found in the
 * binary is FUN_001c5560 (0x1c55d8, unconditional call). */
void display_error_abort_to_dashboard_deferred(int16_t error_handle,
                                               uint8_t allow_abort)
{
  if (*(int16_t *)0x46cc68 == -1) {
    *(int16_t *)0x46cc68 = error_handle;
    *(uint8_t *)0x46cc6a = allow_abort;
    return;
  }
  error(2, "there is already a deferred dashbaord error queued; ignoring "
           "this one!");
}

/* ui_start_main_menu_music — starts the main menu looping music track.
 * Checks whether title music is already playing (0x46cc86) and whether a
 * game is in progress (0x1006c0). If neither, looks up the "lsnd" tag
 * "sound\music\title1\title1" via the tag system (0x1b9930) and starts
 * it as a looping sound (0x1c8510) with volume 1.0. Sets the
 * title_music_playing flag on success. */
void ui_start_main_menu_music(void)
{
  int tag_index;

  if (*(uint8_t *)0x46cc86 != 0)
    return;

  if (main_change_map_name_in_progress())
    return;

  tag_index = tag_loaded(0x6c736e64, "sound\\music\\title1\\title1");
  if (tag_index != -1) {
    error(2, "starting main menu music");
    sound_looping_start(tag_index, -1, 1.0f);
    *(uint8_t *)0x46cc86 = 1;
    return;
  }
  error(2, "title music tag not found");
}

void ui_stop_main_menu_music(void)
{
  int tag_index;

  if (*(uint8_t *)0x46cc86 != 1) {
    return;
  }

  tag_index = tag_loaded(0x6c736e64, "sound\\music\\title1\\title1");
  if (tag_index != -1) {
    error(2, "stopping main menu music");
    sound_looping_stop(tag_index);
    *(uint8_t *)0x46cc86 = 0;
    return;
  }

  error(2, "title music tag not found");
  *(uint8_t *)0x46cc86 = 0;
}

bool ui_main_menu_music_active(void)
{
  return *(bool *)0x46cc86;
}

void ui_widgets_disable_pause_game(int duration_ticks)
{
  assert_halt(duration_ticks >= 0);
  dword_46CC44 = duration_ticks;
}

void push_widget(int *head, void *record);

/* pop_widget (0xe4770) — pops the head node off the intrusive list at
 * *head, copies its first 3 dwords into *output, relinks *head to the
 * popped node's 4th dword (next pointer, +0xc), then returns the node to
 * the widget stack memory pool at [0x31e04c]. Asserts both head and output
 * are non-NULL (ui_widget.c:0x9fc). */
void pop_widget(int *head, void *output)
{
  int *top;

  assert_halt_msg_at("top && data", "c:\\halo\\SOURCE\\interface\\ui_widget.c",
                     0x9fc, head != NULL && output != NULL);

  top = (int *)*head;
  ((int *)output)[0] = top[0];
  ((int *)output)[1] = top[1];
  ((int *)output)[2] = top[2];
  *head = top[3];
  stack_memory_pool_deallocate(*(void **)0x31e04c, top);
}

/* ui_widget_add_child (0xe4800) — appends child to the end of parent's
 * sibling list. `child` arrives in EBX (see kb.json @<ebx>), `parent` on the
 * stack at [EBP+8]. Asserts the child is unlinked (prev_sibling +0x28 and
 * next_sibling +0x2c both NULL, ui_widget.c:0xa9f), then walks
 * parent->first_child (+0x34) along next_sibling (+0x2c) to find the tail.
 * With a tail, asserts tail->next == NULL (ui_widget.c:0xaa9) and links
 * tail->next = child / child->previous = tail; with an empty list, sets
 * parent->first_child = child. Note the original never sets child->parent
 * (+0x30) here. */
void ui_widget_add_child(void *child, void *parent)
{
  int *tail_child;
  int *cursor;

  assert_halt_msg_at("(child->previous == NULL) && (child->next == NULL)",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0xa9f,
                     *(int *)((char *)child + 0x28) == 0 &&
                       *(int *)((char *)child + 0x2c) == 0);

  tail_child = NULL;
  cursor = *(int **)((char *)parent + 0x34);
  while (cursor != NULL) {
    tail_child = cursor;
    cursor = *(int **)((char *)cursor + 0x2c);
  }

  if (tail_child != NULL) {
    assert_halt_msg_at("tail_child->next == NULL",
                       "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0xaa9,
                       *(int *)((char *)tail_child + 0x2c) == 0);
    *(int **)((char *)tail_child + 0x2c) = (int *)child;
    *(int **)((char *)child + 0x28) = tail_child;
  } else {
    *(int **)((char *)parent + 0x34) = (int *)child;
  }
}

/* ui_widget_delete_children_recursive — walks the first_child linked list of a
 * widget and closes each child via ui_widget_delete. Asserts that each child's
 * prev_sibling is NULL (since it should be the head of the sibling list)
 * and that the next sibling's prev_sibling points back correctly. After
 * closing each child, clears the next sibling's prev_sibling link before
 * advancing. */
void ui_widget_delete_children_recursive(void *widget)
{
  int *child;
  int *next;

  child = *(int **)((char *)widget + 0x34);
  if (child == NULL)
    return;

  do {
    next = *(int **)((char *)child + 0x2c);

    assert_halt_msg(*(int *)((char *)child + 0x28) == 0,
                    "child->previous == NULL");
    assert_halt_msg(next == NULL || *(int *)((char *)next + 0x28) == (int)child,
                    "next->previous == child");

    ui_widget_delete(child);

    if (next != NULL)
      *(int *)((char *)next + 0x28) = 0;

    child = next;
  } while (child != NULL);
}

int ui_widget_load_widget_children(void *definition, void *widget);
void ui_widget_link_child(void *parent, void *child);

/* widget_instance_find_by_tag_index_recursive — depth-first search over a
 * widget subtree for the first node whose tag handle (offset +0x0) equals
 * tag_handle.  Checks the current node first, then recurses into each child
 * from first_child (+0x34) following next_sibling (+0x2c). Returns NULL when no
 * match exists. */
int *widget_instance_find_by_tag_index_recursive(int *widget, int tag_handle)
{
  int *result;
  int *child;

  if (*widget == tag_handle)
    return widget;

  result = NULL;
  child = *(int **)((char *)widget + 0x34);
  while (child != NULL && result == NULL) {
    if (*child == tag_handle) {
      result = child;
    } else {
      result = widget_instance_find_by_tag_index_recursive(child, tag_handle);
    }
    child = *(int **)((char *)child + 0x2c);
  }

  return result;
}

/* widget_instance_can_receive_events (0xe4980) — widget ancestor-chain check,
 * called from widget_instance_set_focused_child_by_index (0xe5090). Returns
 * false immediately if the widget is disabled (+0x12 != 0). Otherwise walks up
 * the parent chain
 * (+0x30 — the same field widget_instance_give_focus_directly and
 * widget_instance_give_focus_by_tag use to climb to the root widget) starting
 * at the widget's immediate parent. For each ancestor, looks up its DeLa tag
 * definition (tag_get(0x44654c61, ancestor[0])) and requires either the
 * *previous* ancestor's tag definition to have bit 0 of field_0x2c set, or the
 * *current* ancestor's type field (+0xe) to be 2 or 3 (list types — same
 * values widget_instance_give_focus_directly tests). The first ancestor that
 * fails this check stops the walk and yields false; running out of ancestors
 * (or having none at all) yields true. */
bool widget_instance_can_receive_events(void *widget)
{
  int *w;
  int *parent;
  void *prev_tag;
  void *cur_tag;
  bool result;

  w = (int *)widget;

  if (*(uint8_t *)((char *)w + 0x12) != 0) {
    return false;
  }

  parent = *(int **)((char *)w + 0x30);
  if (parent == NULL) {
    return true;
  }

  prev_tag = tag_get(0x44654c61, *parent);
  result = true;

  while (parent != NULL && result) {
    cur_tag = tag_get(0x44654c61, *parent);
    if ((*(uint8_t *)((char *)prev_tag + 0x2c) & 1) == 0 &&
        *(int16_t *)((char *)parent + 0xe) != 2 &&
        *(int16_t *)((char *)parent + 0xe) != 3) {
      result = false;
    } else {
      result = true;
    }
    parent = *(int **)((char *)parent + 0x30);
    prev_tag = cur_tag;
  }

  return result;
}

/* widget_instance_text_box_is_focused (0xe4a40) — validates the parent
 * chain, then returns whether its top-most widget is type 2 or 3. The widget
 * argument and byte return use EAX/AL. */
char widget_instance_text_box_is_focused(void *widget)
{
  void *parent;
  void *next_parent;
  char focused;

  parent = *(void **)((char *)widget + 0x30);
  if (parent == NULL) {
    return 1;
  }

  focused = (char)(*(void **)((char *)parent + 0x38) == widget);
  if (focused != 0) {
    return focused;
  }

  do {
    next_parent = *(void **)((char *)parent + 0x30);
    if (next_parent == NULL) {
      return focused;
    }
    if (*(void **)((char *)next_parent + 0x38) != parent) {
      return focused;
    }
    focused = (char)((*(uint16_t *)((char *)next_parent + 0x0e) == 2) ||
                     (*(uint16_t *)((char *)next_parent + 0x0e) == 3));
    parent = next_parent;
  } while (parent != NULL);

  return focused;
}

/* get_icon_type (0xe4a80) — linear case-insensitive search of the 0x28-entry
 * (40) wide-string table at 0x31e098 (Ghidra label PTR_u_a_button_0031e098)
 * for an entry matching the implicit @<ebx> argument. Comparison is
 * __wcsnicmp(name, table[i], _wcslen(table[i])) — i.e. name only needs to
 * match table[i]'s full length as a prefix. Returns the matching table
 * index (0..0x27), or -1 if none of the 0x28 entries match. Table
 * contents/semantics not otherwise evidenced by this bundle; name kept
 * mechanical. Callers: string_has_icons_to_draw, FUN_000e5de0, FUN_000e4da0
 * (x4) — none ported yet, so caller-side intent is not available as
 * corroboration. */
int16_t get_icon_type(const wchar_t *name)
{
  const wchar_t *entry;
  size_t entry_length;
  int16_t index;

  index = 0;
  do {
    entry = *(const wchar_t **)(0x31e098 + (int)index * 4);
    entry_length = _wcslen(entry);
    if (__wcsnicmp(name, entry, entry_length) == 0) {
      break;
    }
    index = index + 1;
  } while (index < 0x28);

  if (index == 0x28) {
    return -1;
  }
  return index;
}

/* render_state_bitmap (0xe4ad0) — draws one inline icon bitmap for the
 * draw_string_and_hack_in_icons() text pass (sole caller FUN_000e5de0, the
 * unconditional call at 0xe60cb) and advances the running x stored in
 * dst_rect[1].
 *
 * The bitmap tag comes from the interface globals tag block, the same block
 * interface_get_tag_index() walks: game globals +0x140, element 0, element
 * size 0x130; +0xec is the tag_index field of interface tag slot 14
 * (0xec == 14 * 0x10 + 0xc). When the block is empty the element pointer is
 * NULL and the load happens anyway, exactly as in the reference (MOV EDI,
 * [EAX + 0xec] with EAX zeroed at 0xe4b01) and as interface.c already
 * reproduces. The leading global_scenario_get() at 0xe4ad6 has its result
 * discarded; it is preserved because the reference makes it unconditionally.
 *
 * ABI, from the call site at 0xe60cb (PUSH ECX ... PUSH EAX / PUSH EDX /
 * LEA EBX,[EBP + -0x14] / CALL / ADD ESP,0xc — three stack arguments, EBX
 * and ESI live in): @ebx = dst_rect, @esi = state, stack = (param_1, color,
 * param_3). Only the second stack argument is read here ([EBP + 0xc] at
 * 0xe4be1); param_1 and param_3 are never touched on any path, so their
 * meaning is unproven and they stay unnamed. The caller builds the color
 * argument as (rgb & 0xffffff) | alpha.
 *
 * state offsets accessed (meanings unproven except as noted):
 *   +0x00 uint16  bitmap sequence index (zero-extended at 0xe4b3d)
 *   +0x02 int16   x advance bias
 *   +0x04 int16   x offset, multiplied by the screen scale
 *   +0x06 int16   y offset, multiplied by the screen scale
 *   +0x08 int32   override color, used when (+0x0d & 2)
 *   +0x0c int8    animation divisor: frames advance at 30/s divided by it
 *   +0x0d uint8   flags; bit 2 selects +0x08 as the color, bit 4 advances
 *                 by +0x02 alone instead of by the bitmap width
 * dst_rect[1] (+0x2) is the running x, dst_rect[2] (+0x4) the baseline y.
 * bitmap_data + 4 is the bitmap_data width field (int16) — corroborated by
 * the sprite path, which scales it by the sprite's u extent
 * (sprite[1] - sprite[0]) to get a pixel width.
 *
 * The two FPU constants are 1.0f at 0x2533c8 (FADD, x) and 2.0f at 0x253f40
 * (FSUB, y). The y term is dst_rect[2] - state+0x6 * scale: FILD of the
 * dst_rect value is pushed first and FSUBP computes st(1) - st(0). */
void render_state_bitmap(short *dst_rect, void *state, int param_1, int color,
                         int param_3)
{
  char *globals;
  char *element;
  char *s;
  int bitmap_tag;
  unsigned int frame_index;
  int bitmap_data;
  int sprite;
  float scale;
  int draw_color;
  short screen_pos[2];

  global_scenario_get();
  globals = (char *)game_globals_get();
  if (*(int *)(globals + 0x140) != 0) {
    element = (char *)tag_block_get_element(globals + 0x140, 0, 0x130);
  } else {
    element = NULL;
  }
  bitmap_tag = *(int *)(element + 0xec);

  s = (char *)state;
  frame_index = 0;
  bitmap_data = 0;
  sprite = 0;
  if (*(signed char *)(s + 0xc) != 0) {
    frame_index = (system_milliseconds() * 30) / 1000 /
                  (unsigned int)(int)*(signed char *)(s + 0xc);
  }
  hud_retrieve_bitmap_and_bounding_rect(bitmap_tag, (short)*(unsigned short *)s,
                                        frame_index, &bitmap_data, &sprite);
  if (bitmap_data == 0) {
    return;
  }
  if (xbox_texture_cache_get_hardware_format((void *)bitmap_data, false,
                                             true) == NULL) {
    return;
  }

  scale = hud_globals_get_scale(local_player_count() > 1);
  screen_pos[0] = (short)(int)((float)(int)*(short *)(s + 4) * scale +
                               (float)(int)dst_rect[1] + 1.0f);
  screen_pos[1] = (short)(int)((float)(int)dst_rect[2] -
                               (float)(int)*(short *)(s + 6) * scale - 2.0f);

  if ((*(unsigned char *)(s + 0xd) & 2) != 0) {
    draw_color = *(int *)(s + 8);
  } else {
    draw_color = color;
  }
  hud_draw_bitmap_direct(bitmap_data, 2, screen_pos, sprite, scale, 0.0f,
                         draw_color, 0);

  if ((*(unsigned char *)(s + 0xd) & 4) != 0) {
    dst_rect[1] = (short)(*(short *)(s + 2) + screen_pos[0]);
    return;
  }
  if (sprite != 0) {
    dst_rect[1] =
      (short)(int)((*(float *)(sprite + 4) - *(float *)sprite) *
                     (float)(int)*(short *)(bitmap_data + 4) +
                   (float)(int)*(short *)(s + 2) + (float)(int)screen_pos[0]);
    return;
  }
  dst_rect[1] =
    (short)(*(short *)(bitmap_data + 4) + *(short *)(s + 2) + screen_pos[0]);
}

/* render_state_text (0xe4c70) — draws text into dst_rect using the indent
 * difference between src_rect and dst_rect (row 1, e.g. "top"), then copies
 * src_rect back into dst_rect. A negative computed indent is clamped to 0
 * and reported via error() with the message "initial_indent<0 in
 * render_state_text() and was about to explode" — the same wording used by
 * render_state_text(), so this is presumably an extracted final-draw step of
 * that routine. Sole caller is draw_string_and_hack_in_icons (FUN_000e5de0,
 * unconditional call), which is not yet ported, so caller-side register
 * setup is not available as corroboration; the @ebx/@edi roles and the
 * dst_rect/src_rect naming are taken directly from this function's own
 * disassembly (matches FUN_0019cdb0's out_rect/in_rect argument order) and
 * from the structurally identical draw_string_set_indents/FUN_0019cdb0/
 * rasterizer_draw_unicode_string sequence already lifted as render_state_text_0's
 * non-icon path in hud_messaging.c. The incoming ESI register (PUSH ESI at
 * entry, POP ESI at exit) is a callee-saved scratch register the original
 * compiler reused for the indent computation, not a real argument: its low 16
 * bits are unconditionally overwritten by the indent subtraction before any
 * read, and the surviving high 16 bits are only ever pushed as the padding half
 * of a stack dword for a `short` parameter (draw_string_set_indents), which the
 * callee never reads — so its incoming value has no observable effect.
 * ABI: @ebx=dst_rect, @edi=src_rect, stack: text */
void render_state_text(short *dst_rect, void *text, short *src_rect)
{
  short indent;
  short local_bounds[4];

  indent = (short)((int)(unsigned short)src_rect[1] -
                   (int)(unsigned short)dst_rect[1]);
  if (indent < 0) {
    error(2,
          "initial_indent<0 in render_state_text() and was about to explode");
    if (indent < 0) {
      indent = 0;
    }
  }
  draw_string_set_indents(indent, 0);
  FUN_0019cdb0(dst_rect, text, local_bounds, src_rect);
  src_rect[1] = src_rect[1] - 3;
  local_bounds[1] = dst_rect[1];
  rasterizer_draw_unicode_string(local_bounds, NULL, NULL, 0, (unsigned short *)text);
  *dst_rect = *src_rect;
}

/* should_flip_sticks_for_local_player (0xe4d40) — evaluates whether a local
 * player's input preferences select control scheme 1 or 3. If
 * local_player_index is -1 (unspecified), resolves it via
 * local_player_get_next(-1) first. Builds a 0x18-byte zeroed preferences block
 * on the stack, fills it via input_abstraction_get_local_player_preferences()
 * when a valid local player was found, then tests the int16 field at buffer
 * offset 0x14 against 1 and 3. field_14: offset is accessed, meaning unproven.
 * Callers: FUN_000e4da0 (x2, both unconditional calls per xrefs_to). */
bool should_flip_sticks_for_local_player(int16_t local_player_index)
{
  uint8_t preferences[0x18];
  int16_t control_scheme;

  if (local_player_index == -1) {
    local_player_index = local_player_get_next(-1);
  }

  csmemset(preferences, 0, 0x18);

  if (local_player_index != -1) {
    input_abstraction_get_local_player_preferences(local_player_index,
                                                   preferences);
  }

  control_scheme = *(int16_t *)(preferences + 0x14);

  return (control_scheme == 1) || (control_scheme == 3);
}

/* widget_instance_give_focus_directly — applies focus to target_widget within
 * the root's focus chain. Walks to the top-most parent (+0x30), snapshots the
 * current focused-descendant chain head (+0x38), optionally retargets when the
 * input widget is disabled (+0x12==1) by scanning sibling/parent lists for a
 * focusable widget (DeLa handlers>0 or type 2/3), then rewrites ancestor
 * focused-descendant links (+0x38). If the previous and new focus share the
 * same direct parent, updates only that parent and exits early. */
void widget_instance_give_focus_directly(void *root_widget, void *target_widget)
{
  int root;
  int focused;
  int parent;
  int *target;
  int *candidate;
  int tag_data;

  root = (int)root_widget;
  target = (int *)target_widget;

  while (*(int *)(root + 0x30) != 0) {
    root = *(int *)(root + 0x30);
  }

  focused = *(int *)(root + 0x38);

  if (*(uint8_t *)((char *)target + 0x12) == 1) {
    candidate = (int *)target[0xb];
    while (candidate != NULL) {
      tag_data = (int)tag_get(0x44654c61, candidate[0]);
      if (*(uint8_t *)((char *)candidate + 0x12) == 0 &&
          (*(int *)(tag_data + 0x54) > 0 ||
           *(int16_t *)((char *)candidate + 0xe) == 2 ||
           *(int16_t *)((char *)candidate + 0xe) == 3)) {
        goto set_candidate;
      }
      candidate = (int *)candidate[0xb];
    }

    parent = target[0xc];
    if (parent != 0) {
      candidate = *(int **)(parent + 0x34);
      while (candidate != NULL) {
        tag_data = (int)tag_get(0x44654c61, candidate[0]);
        if (*(uint8_t *)((char *)candidate + 0x12) == 0 &&
            (*(int *)(tag_data + 0x54) > 0 ||
             *(int16_t *)((char *)candidate + 0xe) == 2 ||
             *(int16_t *)((char *)candidate + 0xe) == 3)) {
          break;
        }
        candidate = (int *)candidate[0xb];
      }

      if (candidate == *(int **)(parent + 0x38)) {
        candidate = (int *)target[0xa];
        while (candidate != NULL) {
          tag_data = (int)tag_get(0x44654c61, candidate[0]);
          if (*(uint8_t *)((char *)candidate + 0x12) == 0 &&
              (*(int *)(tag_data + 0x54) > 0 ||
               *(int16_t *)((char *)candidate + 0xe) == 2 ||
               *(int16_t *)((char *)candidate + 0xe) == 3)) {
            break;
          }
          candidate = (int *)candidate[0xa];
        }
      }
    }

    if (candidate != NULL) {
    set_candidate:
      target = candidate;
    }
  }

  if (focused != 0) {
    if (target != NULL && *(int *)(focused + 0x30) == target[0xc] &&
        *(int *)(focused + 0x30) != 0) {
      *(int *)(target[0xc] + 0x38) = (int)target;
      return;
    }

    do {
      *(int *)(*(int *)(focused + 0x30) + 0x38) = 0;
      focused = *(int *)(focused + 0x38);
    } while (focused != 0);
  }

  parent = target[0xc];
  while (parent != 0) {
    *(int *)(parent + 0x38) = (int)target;
    target = (int *)target[0xc];
    parent = target[0xc];
  }
}

void widget_instance_set_focused_child_by_index(int pending_a6, int widget,
                                                int16_t a7);

void column_list_update(void *widget, void *definition);

void widget_instance_tab_to_next_valid_widget(void *widget);

void widget_instance_tab_to_previous_valid_widget(void *widget);

/* search_and_replace (0xe5180) — in-place/realloc search-and-replace over a
 * wide string held by *text. Returns the number of replacements, 0 when
 * text or *text is NULL or no match exists, and -1 when the pool realloc
 * needed to grow the buffer fails. When the replacement is no longer than
 * the search string the edit happens in place and *text is left alone;
 * otherwise the buffer is reallocated from the widget stack memory pool at
 * [0x31e04c] (assert file/line "ui_widget.c":0x1382) and *text is
 * rewritten. Lengths are in wide characters; total_length counts the
 * terminator. */
int search_and_replace(const wchar_t *search, const wchar_t *replace,
                       wchar_t **text)
{
  wchar_t *buffer;
  wchar_t *found;
  void *block;
  int search_length;
  int replace_length;
  int total_length;
  int delta;
  int count;

  count = 0;
  if (text == NULL || *text == NULL) {
    return count;
  }

  buffer = *text;
  search_length = ustrlen((const unsigned short *)search);
  replace_length = ustrlen((const unsigned short *)replace);
  total_length = ustrlen((const unsigned short *)buffer) + 1;

  if (replace_length <= search_length) {
    delta = search_length - replace_length;
    found = ustrstr(buffer, search);
    while (found != NULL) {
      count++;
      csmemcpy(found, (void *)replace, (size_t)(replace_length * 2));
      if (delta > 0) {
        csmemmove(found + replace_length, found + search_length,
                  (unsigned int)((total_length -
                                  ((int)(found - buffer) + replace_length)) *
                                 2));
        total_length -= delta;
      }
      found = ustrstr(buffer, search);
    }
    return count;
  }

  delta = replace_length - search_length;
  found = ustrstr(buffer, search);
  if (found == NULL) {
    return count;
  }
  do {
    count++;
    found = ustrstr(found + search_length, search);
  } while (found != NULL);

  if (count <= 0) {
    return count;
  }

  block = stack_memory_pool_realloc(
    *(void **)0x31e04c, (int)buffer,
    (unsigned short)((delta * count + total_length) * 2),
    "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x1382);
  if (block == NULL) {
    return -1;
  }
  buffer = (wchar_t *)block;

  found = ustrstr(buffer, search);
  if (found != NULL) {
    replace_length = replace_length * 2;
    do {
      csmemmove((wchar_t *)((char *)found + replace_length),
                found + search_length,
                (unsigned int)((total_length -
                                ((int)(found - buffer) + search_length)) *
                               2));
      csmemcpy(found, (void *)replace, (size_t)replace_length);
      total_length += delta;
      found = ustrstr(buffer, search);
    } while (found != NULL);
  }

  *text = buffer;
  return count;
}

/* Local shape-only float4, not a claimed Bungie struct: mirrors the
 * reference's whole-struct copy (see get_ui_argb_white below) so the
 * compiler spills/overwrites the same EBP slots the original does. */
typedef struct {
  float f0, f1, f2, f3;
} get_ui_argb_white_color_t;

/* get_ui_argb_white (0xe5530) — writes an ARGB-style float[4] color into
 * out_color: [0] is the alpha carried over from the shared default-color
 * pointer at 0x2ee6c4 (points to the all-ones {1,1,1,1} color at 0x267700 —
 * the same global player_effects.c and render_sprite.c read as a colour);
 * [1..3] are the RGB white constants at 0x31e148/0x31e14c/0x31e150.
 * Disassembly does a whole-struct copy from *default_color first (word0
 * stays resident in a register; word1/word2/word3 spill to EBP-relative
 * temps), then overwrites 3 of those temps with the RGB constants in
 * 1,3,2 order, then stores the local back through out_color once. */
float *get_ui_argb_white(float *out_color)
{
  get_ui_argb_white_color_t local_color;

  local_color = **(get_ui_argb_white_color_t **)0x2ee6c4;
  local_color.f1 = *(float *)0x31e148;
  local_color.f3 = *(float *)0x31e150;
  local_color.f2 = *(float *)0x31e14c;

  *(get_ui_argb_white_color_t *)out_color = local_color;

  return out_color;
}

/* filesystem_initialization_thread_proc(x) (0xe5590) — saved-game
 * filesystem-check thread procedure. Registered with thread_new as the
 * background thread entry point by perform_filesystem_initialization, and also
 * invoked directly (synchronously, with param_1 = 0) if thread creation fails.
 * `RET 0x4` marks it __stdcall with one unused thread-proc parameter (the Win32
 * thread lpParameter slot). Calls saved_game_perform_file_system_checks and
 * stores its bool/short result to the filesystem-check result word at 0x46cc80
 * (read back by widget_instance_go_back_to_previous's caller as 1 == "no saved
 * games", 2 == "disk error"). Only on success (result == 0) does it run the two
 * saved-game enumeration helpers at 0x1c26b0/0x1c0d50 (each takes -1 plus
 * out-params pointing at two EBP locals shared between both calls: local_4 is
 * pre-initialized to 1 before the first call so the callee can read a caller
 * default; local_8 is left uninitialized for the callee to fill) and the
 * profile-index getter, whose return value is unused here. */
void __stdcall filesystem_initialization_thread_proc(int param_1)
{
  int local_4;
  int local_8;
  int16_t result;

  (void)param_1;

  result = saved_game_perform_file_system_checks();
  *(int16_t *)0x46cc80 = result;
  if (result == 0) {
    local_4 = 1;
    FUN_001c26b0(-1, &local_4, &local_8);
    FUN_001c0d50(-1, &local_4, &local_8, 1);
    player_ui_get_player1_last_used_profile_index();
  }
}

/* modulate_pixel32_by_real_alpha (0xe55e0) — scales the alpha byte of a
 * packed 32-bit pixel by a float factor, leaving the low 24 bits (the
 * colour channels) untouched. The alpha byte is extracted with SHR 24
 * (unsigned) and converted with FILD as a signed dword; the TEST/JGE
 * "negative int" fixup that follows is provably unreachable (the shifted
 * value is 0..255), which is why Ghidra removes that block. The product is
 * rounded back to an integer with a bare FLD/FISTP — the FPU's current
 * rounding mode (round-to-nearest), NOT a truncating C cast — and OR'd back
 * into bits 24..31. The intermediate is stored to a 32-bit float slot and
 * reloaded before the FISTP, so the product is narrowed to single precision
 * first. No callers found in the binary (xrefs empty); the name describes
 * the operation, the caller-side meaning of the float is unproven. */
uint32_t modulate_pixel32_by_real_alpha(uint32_t pixel, float alpha)
{
  int alpha_byte;
  float scaled;

  alpha_byte = (int)(pixel >> 24);
  scaled = (float)alpha_byte * alpha;
  HALO_FLT_ROUNDTRIP(scaled);
  return (pixel & 0x00ffffff) | ((uint32_t)x87_round_to_int(scaled) << 24);
}

/* ui_widget_delete — tears down a single UI widget and frees its memory.
 * Handles the "widget deleted" event handlers (type 0x19) from the widget's
 * DeLa tag definition, firing each matching handler via
 * ui_widget_event_handler_function_invoke and optionally spawning replacement
 * widgets. Manages the pause counter (if the widget pauses the game), unlinks
 * the widget from its sibling/parent chains, performs type-specific cleanup
 * (text data for type 1, list data for types 2-3), frees the widget from
 * the stack memory pool, and clears any root widget slot that pointed to it.
 * The being_deleted flag at +0x14 prevents re-entrant closing. */
void ui_widget_delete(void *widget)
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
    player_control_inhibit_buttons(*(int16_t *)((char *)w + 0x8), 0xfff, 1);
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
        handler_result = ui_widget_event_handler_function_invoke(
          w, 0, *(uint16_t *)(handler + 0x6), &widget_deleted);

        assert_halt_msg(!widget_deleted,
                        "a 'widget deleted' event handler tried to delete the "
                        "widget being deleted!");

        if (handler_result && (handler[0] & 0x8) != 0 &&
            *(int *)(handler + 0x14) != -1) {
          if (ui_widget_launch_widget(w, *(int *)(handler + 0x14)) == 0) {
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
  ui_widget_delete_children_recursive(w);

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
      ui_widget_delete((void *)w[0x12]);
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
 * them down. For each stack, closes the root widget via ui_widget_delete
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
      ui_widget_delete((void *)list_heads[-4]);
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

/* ui_widgets_close_all_for_local_player (0xe5910) — like ui_widgets_close_all
 * above, but only tears down the one root-widget stack (of the 4 at
 * 0x46cc20..2c / 0x46cc30..3c) whose root widget's local_player_index field
 * (+8) matches local_player_index: closes that root via ui_widget_delete
 * (0xe5620), then drains its pending-close list at 0x46cc30[i] (linked
 * through +0xc) back to the stack memory pool at [0x31e04c]. Asserts
 * local_player_index is in [0,4) -- unlike ui_widgets_pop_stack below,
 * -1 is NOT special-cased to player 0 here. */
void ui_widgets_close_all_for_local_player(int16_t local_player_index)
{
  int *list_heads;
  int root;
  int widget;
  int next;
  void *pool;

  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert("expected a valid local_player_index",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x482, true);
    system_exit(-1);
  }

  list_heads = (int *)0x46cc30;
  do {
    root = list_heads[-4];
    if (root != 0 && *(int16_t *)(root + 8) == local_player_index) {
      ui_widget_delete((void *)root);

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
    }
    list_heads++;
  } while ((int)list_heads < 0x46cc40);
}

/* ui_widgets_pop_stack — drains one pending queued entry from the
 * pending-load list at 0x46cc30[local_player_index] (see
 * pop_widget / push_widget above).
 * local_player_index == -1 is treated as player 0; otherwise it must be in
 * [0, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS). The popped record is discarded --
 * this only drains one node, it does not apply it. */
void ui_widgets_pop_stack(int16_t local_player_index)
{
  unsigned char record[12]; /* sizeof(ui_widget_pending_load_t); output is
                                discarded by this caller, so no need for the
                                named struct which is defined later in this
                                TU */

  if (local_player_index == -1) {
    local_player_index = 0;
  } else if ((local_player_index < 0) ||
             (local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)) {
    display_assert("(local_player_index>=0) && "
                   "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x4b4, true);
    system_exit(-1);
  }

  if (*(int *)(0x46cc30 + (int)local_player_index * 4) != 0) {
    pop_widget((int *)(0x46cc30 + (int)local_player_index * 4),
               (void *)&record);
  }
}

/* main_screen_shell_begin_fade — starts the shell's screen-fade-out on each
 * of the 4 UI root widget stacks (0x46cc20..2c) whose root is not in
 * "in_game_mode" (+0x15, see render_ui_widgets above). Stops attract mode,
 * then for each eligible root stamps the fade duration (+0x20) with
 * duration_ms and the timeout (+0x1c) with (current_tick - start_tick) + 100
 * ticks, where start_tick is +0x18 and current_tick is the global at
 * 0x46cc40 (see ui_widget_event_handler_function_invoke's timeout check against
 * +0x18/+0x1c/+0x20 above). Finally frees every widget already queued on
 * that stack's pending-close list (0x46cc30[i], linked through +0xc) back
 * to the stack memory pool — the same list-drain as ui_widgets_close_all,
 * but without closing the root widget itself. */
void main_screen_shell_begin_fade(int duration_ms)
{
  int *root_slots;
  int *list_head;
  int root;
  int widget;
  int next;
  void *pool;

  ui_stop_main_menu_music();

  root_slots = (int *)0x46cc20;
  do {
    root = *root_slots;
    if (root != 0 && *(uint8_t *)(root + 0x15) == 0) {
      *(int *)(root + 0x20) = duration_ms;
      *(int *)(root + 0x1c) = (*(int *)0x46cc40 - *(int *)(root + 0x18)) + 100;

      list_head = root_slots + 4; /* matching slot in 0x46cc30[] */
      widget = *list_head;
      while (widget != 0) {
        pool = *(void **)0x31e04c;
        next = *(int *)(widget + 0xc);
        *list_head = next;
        stack_memory_pool_deallocate(pool, (void *)widget);
        widget = *list_head;
      }
    }
    root_slots++;
  } while ((int)root_slots < 0x46cc30);
}

/* ui_play_audio_feedback_sound (0xe5ab0) — plays one of four canned UI
 * feedback sounds selected by sound_selector: 1=cursor, 2=forward, 3=back,
 * 4=flag_failure. DEC EAX / CMP EAX,3 / JA default in the disassembly
 * matches switch(1..4); any other value falls straight through to the
 * default arm and does nothing. Resolves the sound tag via
 * tag_loaded('snd!', path) and, if found (result != -1), starts it at
 * full volume via sound_impulse_start(tag_index, 1.0f). Same
 * tag_loaded/sound_impulse_start pattern as the inline sound-selector
 * switch inside the event handler above (~line 1830), but exposed here
 * as its own callable — many UI event handlers below call it directly
 * (see xrefs). */
void ui_play_audio_feedback_sound(short sound_selector)
{
  int sound_tag_index;

  switch (sound_selector) {
  case 1:
    sound_tag_index =
      tag_loaded(0x736e6421 /* 'snd!' */, "sound\\sfx\\ui\\cursor");
    break;
  case 2:
    sound_tag_index =
      tag_loaded(0x736e6421 /* 'snd!' */, "sound\\sfx\\ui\\forward");
    break;
  case 3:
    sound_tag_index =
      tag_loaded(0x736e6421 /* 'snd!' */, "sound\\sfx\\ui\\back");
    break;
  case 4:
    sound_tag_index =
      tag_loaded(0x736e6421 /* 'snd!' */, "sound\\sfx\\ui\\flag_failure");
    break;
  default:
    return;
  }

  if (sound_tag_index != -1) {
    sound_impulse_start(sound_tag_index, 1.0f);
  }
}

/* widget_instance_render_text_box (0xe6140) — refreshes and draws a text-box
 * widget.
 *
 * Register ABI confirmed from the prologue: MOV ESI,EAX / MOV EBX,ECX, so
 * EAX carries the widget *definition* (tag data: offsets 0x24..0x132) and
 * ECX the runtime *widget* instance (offsets 0x10, 0x3c, 0x40 and the
 * parent chain at 0x30).  Three cdecl stack params follow.
 *
 * Two error strings anchor the name and the two validity checks:
 *   0x2839c8 "failed to render text box widget because the justification
 *             was invalid"
 *   0x283a10 "failed to render text box widget because the font tag was
 *             invalid"
 *
 * Sequence:
 *  1. If the definition names a string-list tag (+0xf8 != NONE), fetch the
 *     string (widget's own index at +0x40 overrides definition +0x12e),
 *     realloc the widget's cached text block (+0x3c) out of the UI stack
 *     memory pool and copy it in.  On allocation failure the cached
 *     pointer is pointed at the literal L"<out of memory>" (0x283a54).
 *  2. Run every text search-and-replace function block (count +0x60,
 *     base +0x64, stride 0x22: 0x20-byte ascii name + uint16 function
 *     index) over the cached text.
 *  3. Validate font tag and justification, then draw.
 *
 * The colour is the definition's real_argb quad at +0x10c.  It is replaced
 * by the global UI "white" colour when the caller asks for it or when the
 * definition's RGB is exactly (1,1,1); the alpha is always the definition's
 * alpha scaled by the widget's inherited opacity (the product of field_24 up
 * the parent chain, computed by widget_instance_get_cumulative_alpha_modifier
 * and returned in ST0 — the original keeps it live on the x87 stack across the
 * whole colour selection, which is not expressible in C).  Flag 0x4 at +0x11e
 * adds a cosine pulse driven by the UI millisecond clock at 0x46cc40 (FILD plus
 * a negative fixup of 4294967296.0f, i.e. the clock is unsigned).
 *
 * Note the asymmetry between the two rects, which is what the binary does:
 * the drawn position rect always starts from the definition's bounds
 * (+0x24/+0x28), while the clip/bounds rect passed as the second draw
 * argument honours position_override when it is non-NULL.  position_offset
 * is a packed {int16 x; int16 y} pair; x shifts left/right, y shifts
 * top/bottom, and the top-left corner additionally picks up the definition's
 * text offsets at +0x132 (y) and +0x130 (x). */
void widget_instance_render_text_box(void *definition, void *widget,
                                     const int32_t *position_override,
                                     int32_t position_offset,
                                     bool use_white_color)
{
  const int16_t *offset;
  volatile int32_t offset_pair;
  wchar_t name[32];
  float white_temp[4];
  float color[4];
  int16_t bounds[4];
  int16_t position[4];
  const float *definition_color;
  const float *white;
  wchar_t **text;
  const wchar_t *source_text;
  const char *function_name;
  void *block;
  float opacity;
  int string_tag;
  int16_t string_index;
  int length;
  int i;
  int function_offset;
  int font_tag;
  int16_t justification;

  string_tag = *(int *)((char *)definition + 0xf8);
  if (string_tag != -1) {
    string_index = *(int16_t *)((char *)widget + 0x40);
    if (string_index == -1) {
      string_index = *(int16_t *)((char *)definition + 0x12e);
    }
    source_text = (const wchar_t *)FUN_0019d420(string_tag, string_index);
    length = ustrlen((const unsigned short *)source_text) * 2;
    block = stack_memory_pool_realloc(
      *(void **)0x31e04c, (int)*(void **)((char *)widget + 0x3c),
      (unsigned short)(length + 2), "c:\\halo\\SOURCE\\interface\\ui_widget.c",
      0x1145);
    *(void **)((char *)widget + 0x3c) = block;
    if (block != NULL) {
      csmemcpy(block, (void *)source_text, (size_t)length);
      *(wchar_t *)((char *)*(void **)((char *)widget + 0x3c) + length) = 0;
    } else {
      *(const wchar_t **)((char *)widget + 0x3c) = L"<out of memory>";
    }
  }

  text = (wchar_t **)((char *)widget + 0x3c);
  if (*text == NULL || **text == 0) {
    return;
  }

  function_offset = 0;
  for (i = 0; i < *(int *)((char *)definition + 0x60); i++) {
    function_name =
      *(const char **)((char *)definition + 0x64) + function_offset;
    if (function_name != NULL && *function_name != '\0') {
      search_and_replace(ascii_to_wide(function_name, name, 0x40),
                         ui_widget_search_and_replace_invoke(
                           widget, *(const uint16_t *)(function_name + 0x20)),
                         text);
    }
    function_offset += 0x22;
  }

  font_tag = *(int *)((char *)definition + 0x108);
  if (font_tag == -1) {
    error(2,
          "failed to render text box widget because the font tag was invalid");
    return;
  }

  justification = *(int16_t *)((char *)definition + 0x11c);
  if (justification < 0 || justification >= 3) {
    error(
      2,
      "failed to render text box widget because the justification was invalid");
    return;
  }

  if (*(uint8_t *)((char *)widget + 0x10) == 0) {
    return;
  }

  opacity = widget_instance_get_cumulative_alpha_modifier(widget);

  *(int32_t *)&position[0] = *(int32_t *)((char *)definition + 0x24);
  *(int32_t *)&position[2] = *(int32_t *)((char *)definition + 0x28);
  if (position_override != NULL) {
    *(int32_t *)&bounds[0] = position_override[0];
    *(int32_t *)&bounds[2] = position_override[1];
  } else {
    *(int32_t *)&bounds[0] = *(int32_t *)((char *)definition + 0x24);
    *(int32_t *)&bounds[2] = *(int32_t *)((char *)definition + 0x28);
  }

  /* offset[0] = x (low half), offset[1] = y (high half) */
  offset_pair = position_offset;
  offset = (const int16_t *)&offset_pair;
  position[0] = (int16_t)(position[0] + offset[1] +
                          *(int16_t *)((char *)definition + 0x132));
  position[1] = (int16_t)(position[1] + offset[0] +
                          *(int16_t *)((char *)definition + 0x130));
  position[2] = (int16_t)(position[2] + offset[1]);
  position[3] = (int16_t)(position[3] + offset[0]);

  definition_color = (const float *)((char *)definition + 0x10c);
  if (use_white_color) {
    white = get_ui_argb_white(white_temp);
    color[0] = white[0];
    color[1] = white[1];
    color[2] = white[2];
    color[3] = white[3];
  } else {
    if (*(const float *)0x2533c8 == definition_color[1] &&
        *(const float *)0x2533c8 == definition_color[2] &&
        *(const float *)0x2533c8 == definition_color[3]) {
      white = get_ui_argb_white(white_temp);
      color[0] = white[0];
      color[1] = white[1];
      color[2] = white[2];
      color[3] = white[3];
    } else {
      color[0] = definition_color[0];
      color[1] = definition_color[1];
      color[2] = definition_color[2];
      color[3] = definition_color[3];
    }
  }
  color[0] = definition_color[0] * opacity;

  if ((*(uint8_t *)((char *)definition + 0x11e) & 4) != 0) {
    color[0] = (x87_fcos((float)*(uint32_t *)0x46cc40 * 0.001f * 3.0f) + 1.5f) *
               0.4f * color[0];
  }

  draw_string_set_font(font_tag, -1, justification, 0, color);

  if (string_has_icons_to_draw(*text)) {
    draw_string_and_hack_in_icons(position, (int)bounds, 0, 0, *text, 0);
  } else {
    rasterizer_draw_unicode_string(position, bounds, NULL, 0, (unsigned short *)*text);
  }
}

/* widget_instance_give_focus_by_tag — walks up the parent chain (field_0x30)
 * from the given widget to the root, then searches the widget tree for one
 * matching tag_handle.  If found, calls widget_instance_give_focus_directly;
 * otherwise logs an error.
 */
void widget_instance_give_focus_by_tag(void *widget, int tag_handle,
                                       int16_t player_index)
{
  void *root = widget;
  void *found;

  (void)player_index;

  while (*(void **)((char *)root + 0x30) != NULL)
    root = *(void **)((char *)root + 0x30);

  found = widget_instance_find_by_tag_index_recursive(root, tag_handle);
  if (found != NULL) {
    widget_instance_give_focus_directly(root, found);
    return;
  }

  error(2, "failed to find event focus target widget");
}

void widget_instance_go_back_to_previous(void *widget);

/* perform_filesystem_initialization — spawns a background thread to perform
 * filesystem and saved-game file enumeration. Asserts that no initialization
 * thread is already running (0x46cc7c == NULL) and that the widget subsystem
 * is initialized (0x46cc82). Suppresses UI events (0x46cc85 = 1) and resets
 * the filesystem check result word at 0x46cc80 to 0 before spawning the
 * thread via thread_new (0x81630). If thread creation fails, runs the check
 * procedure synchronously (0xe5590) and re-clears the suppress flag. */
void perform_filesystem_initialization(void)
{
  assert_halt(*(int *)0x46cc7c == 0);
  error(2, "begining filesystem checks & saved game file enumeration...");
  assert_halt(*(uint8_t *)0x46cc82);
  *(uint8_t *)0x46cc85 = 1;
  *(int16_t *)0x46cc80 = 0;
  if (!thread_new(0, (void *)0xe5590, 0, (void **)0x46cc7c)) {
    error(2, "failed to spawn thread for filesystem checks - running "
             "synchronously!");
    *(int *)0x46cc7c = 0;
    /* hazard-ok: fnptr-conv */ ((void(__stdcall *)(int))0xe5590)(0);
    assert_halt(*(uint8_t *)0x46cc82);
    *(uint8_t *)0x46cc85 = 0;
  }
}

/* ui_widgets_dispose — tears down the UI widget system. Closes all open
 * widgets, frees the widget memory pool allocated by ui_widgets_initialize
 * (0x4000 bytes at [ptr+4]), zeros the pool pointer and size fields, and
 * clears the 0x68-byte static widget state block at 0x46cc20. Called during
 * engine shutdown. */
void ui_widgets_dispose(void)
{
  ui_widgets_close_all();

  if ((*(int **)0x31e04c)[1] != 0) {
    debug_free((void *)(*(int **)0x31e04c)[1],
               "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x76);
  }
  (*(int **)0x31e04c)[1] = 0;
  (*(int **)0x31e04c)[2] = 0;
  csmemset((void *)0x46cc20, 0, 0x68);
}

int widget_event_function_list_widget_goto_next_item(void *widget,
                                                     void *event_data,
                                                     char *widget_deleted);

int widget_event_function_list_widget_goto_previous_item(void *widget,
                                                         void *event_data,
                                                         char *widget_deleted);

void event_handler_dispatch(void *widget, void *definition, void *event_data,
                            void *event_handler, char *widget_deleted);

/* widget_instance_render_column_list — called from the widget-tree recursive
 * render helper at 0xe73c0 (xref 0xe75e9; that function is itself still
 * unported — the calls below reach its original binary code through the kb.json
 * redirect thunk). Two independent steps:
 *   1. If widget+0x48 ("target") is non-NULL, accumulates a scale/alpha
 *      value starting from widget+0x24 and multiplying in +0x24 of every
 *      ancestor reached by following the +0x30 "parent" chain, stores the
 *      product into target+0x24, then re-renders target via 0xe73c0.
 *   2. If param_2's flag byte at +0x150 has bit 0 set, walks widget's child
 *      list (head at +0x34, next-link at +0x2c) and renders up to +0x44
 *      children via 0xe73c0, flagging the child at index +0x3c as the
 *      last one (bool arg 5).
 * Clears widget+0x3e (a pending-count field) on every exit path. */
void widget_instance_render_column_list(int widget, int param_2,
                                        viewport_bounds_t *bounds, int param_4,
                                        int param_5)
{
  int target;
  float scale;
  int parent;
  int child;
  int index;
  int is_last;

  target = *(int *)(widget + 0x48);
  if (target != 0) {
    scale = *(float *)(widget + 0x24);
    parent = *(int *)(widget + 0x30);
    while (parent != 0) {
      scale *= *(float *)(parent + 0x24);
      parent = *(int *)(parent + 0x30);
    }
    *(float *)(target + 0x24) = scale;
    widget_instance_render_recursive(target, bounds, param_4, 0, 1);
  }

  if ((*(unsigned char *)(param_2 + 0x150) & 1) != 0) {
    child = *(int *)(widget + 0x34);
    index = 0;
    while (child != 0) {
      if (index >= (int)*(uint16_t *)(widget + 0x44))
        break;
      is_last = (index == (int)*(int16_t *)(widget + 0x3c));
      widget_instance_render_recursive(child, bounds, param_4, param_5,
                                       is_last);
      child = *(int *)(child + 0x2c);
      index++;
    }
  }

  *(uint16_t *)(widget + 0x3e) = 0;
}

/* render_ui_widgets — renders all active UI widget stacks and an optional
 * screen fade overlay. For each of the 4 widget root slots (0x46cc20..2c),
 * determines whether the widget should render based on its local player index
 * vs the current player, the "always render" flag at +0x11, and the
 * "in_game_mode" flag at +0x15. Renders the widget tree via the recursive
 * helper at 0xe73c0. If the debug overlay flag at 0x46cc84 is set, also
 * renders the widget's tag name in a small debug font. After all stacks,
 * checks the global fade value at 0x46cc4c: if it is in [0.0, 1.0], draws
 * a fullscreen fade rectangle (alpha = fade * 255, shifted to the high byte
 * of an ARGB color). When fade >= 0.95 it is clamped to 1.0. The fade
 * value is initialised to -1.0 (inactive). */
void render_ui_widgets(int16_t player_index, viewport_bounds_t *window_bounds)
{
  int widget;
  int16_t clamped_player;
  int i;
  viewport_bounds_t local_bounds;
  float color[4];
  int font_tag;
  const char *tag_name;
  float fade;

  assert_halt(window_bounds != NULL);

  /* store clamped player index: -1 maps to 0, otherwise keep player_index */
  *(uint16_t *)0x5aa45c =
    (uint16_t)((player_index == -1 ? 0 : 1) & (uint16_t)player_index);

  /* if network loading screen is active, bail */
  if (((char (*)(void))0x1c5960)() != 0) {
    return;
  }

  /* if virtual keyboard is active, render it and return */
  if (((char (*)(void))0xf5640)() != 0) {
    ((void (*)(void))0xf5fa0)();
    return;
  }

  /* clamp player_index into [0, 3] range */
  if (player_index < 0) {
    clamped_player = 0;
  } else if (player_index > 3) {
    clamped_player = 3;
  } else {
    clamped_player = player_index;
  }

  for (i = 0; i < 4; i++) {
    widget = *(int *)(0x46cc20 + i * 4);
    if (widget == 0)
      continue;

    /* always-render flag at widget+0x11 */
    if (*(uint8_t *)(widget + 0x11) == 1) {
      goto do_render;
    }

    /* in-game mode flag at widget+0x15 */
    if (*(uint8_t *)(widget + 0x15) == 1) {
      uint16_t widget_player = *(uint16_t *)(widget + 0x8);
      if (widget_player == (uint16_t)clamped_player)
        goto do_render;
      if (widget_player == 0xffff)
        goto do_render;
      if ((uint16_t)clamped_player == 0xffff)
        goto do_render;
      if (*(uint8_t *)0x46cc88 != 0)
        goto do_render;
      continue;
    } else {
      /* not in-game: render if player matches or (player == -1 and stack 0) */
      uint16_t widget_player = *(uint16_t *)(widget + 0x8);
      if (widget_player == 0xffff && i == 0)
        goto do_render;
      if (widget_player == (uint16_t)clamped_player)
        goto do_render;
      continue;
    }

  do_render:
    local_bounds.x1 = window_bounds->x1 - window_bounds->x0;
    local_bounds.y1 = window_bounds->y1 - window_bounds->y0;
    local_bounds.x0 = 0;
    local_bounds.y0 = 0;

    ((void (*)(int, viewport_bounds_t *, int, int, int))0xe73c0)(
      widget, &local_bounds, 0, 1, 0);

    /* debug overlay: draw widget tag name */
    if (*(uint8_t *)0x46cc84 != 0) {
      local_bounds.x0 += 0x20;
      local_bounds.x1 += 0x20;
      local_bounds.y0 += 0x20;
      local_bounds.y1 += 0x20;
      color[0] = 1.0f;
      color[1] = 1.0f;
      color[2] = 1.0f;
      color[3] = 1.0f;
      font_tag = tag_loaded(0x666f6e74, "ui\\small_ui", -1, 0, 0, color);
      ((void (*)(int))0x19b8b0)(font_tag);
      tag_name = tag_get_name(*(int *)(0x46cc20 + i * 4));
      rasterizer_draw_string(&local_bounds, 0, 0, 0, tag_name);
    }
  }

  /* screen fade overlay */
  fade = *(float *)0x46cc4c;
  if (fade >= 0.0f && fade <= 1.0f) {
    local_bounds.y0 = 0;
    local_bounds.x1 = 0x280;
    local_bounds.x0 = 0;
    local_bounds.y1 = 0x1e0;
    if (fade >= *(float *)0x255ed4) { /* 0.95f */
      *(float *)0x46cc4c = 1.0f;
    }
    {
      int alpha = (int)(*(float *)0x46cc4c * *(float *)0x2602c8); /* * 255.0 */
      ((void (*)(viewport_bounds_t *, int))0x92ec0)(&local_bounds, alpha << 24);
    }
  }
}

void widget_instance_process_one_event_recursive(void *widget, void *widget_tag,
                                                 void *event_data,
                                                 void *handled)
{
  int *w;
  int *definition;
  uint8_t *event;
  uint8_t *handled_out;
  bool allowed_player;
  bool consumed;
  bool widget_deleted;
  int16_t sound_effect;
  int i;
  int offset;
  int elapsed;
  int timeout;
  int fade_ticks;
  int child;
  int child_tag;
  int16_t type;
  uint32_t flags;
  const char *sound_name;
  int sound_tag;

  w = (int *)widget;
  definition = (int *)widget_tag;
  event = (uint8_t *)event_data;
  handled_out = (uint8_t *)handled;

  consumed = false;
  widget_deleted = false;

  allowed_player = (*(int16_t *)((char *)w + 8) == -1) ||
                   (*(int16_t *)((char *)w + 8) == *(int16_t *)(event + 2));
  sound_effect = 0;

  if (w == NULL || definition == NULL || event == NULL || handled_out == NULL) {
    display_assert("widget && definition && event && return_widget_deleted",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0xbfb, true);
    system_exit(-1);
  }

  if (*(int16_t *)event == 3 && event[5] > 1 && *(int16_t *)(event + 2) >= 0 &&
      *(int16_t *)(event + 2) < 4 && event[4] >= 8 && event[4] <= 0xb &&
      (uint32_t)(*(uint32_t *)0x46cc40 -
                 *(uint32_t *)(0x46cc90 +
                               ((event[4] - 8) + *(int16_t *)(event + 2) * 4) *
                                 4)) >= 0xfa) {
    event[5] = 1;
  }

  if (*(uint8_t *)((char *)w + 0x16) == 1) {
    int16_t widget_player = *(int16_t *)((char *)w + 8);

    if (widget_player >= 0 && widget_player < 4) {
      if (input_has_gamepad(widget_player)) {
        ui_widget_delete(widget_instance_get_topmost_parent(w));
        widget_deleted = true;
      }
    } else {
      if (widget_player != -1) {
        display_assert("widget->local_player_index==NONE",
                       "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0xc23, true);
        system_exit(-1);
      }

      for (i = 0; i < 4; i++) {
        if (input_has_gamepad(i)) {
          child = (int)w;
          while (*(int *)(child + 0x30) != 0) {
            child = *(int *)(child + 0x30);
          }
          ui_widget_delete((void *)child);
          widget_deleted = true;
          break;
        }
      }
    }
  }

  if (allowed_player && !widget_deleted) {
    if (*(int16_t *)event == 3 && event[5] == 1) {
      if ((event[4] == 0xd || event[4] == 1) &&
          *(int *)(definition + 0x15) > 0) {
        int found = 0;

        for (i = 0; i < *(int *)(definition + 0x15); i++) {
          int16_t handler_type =
            *(int16_t *)(*(int *)(definition + 0x16) + i * 0x48 + 4);
          if ((event[4] == 0xd && handler_type == 0xd) ||
              (event[4] == 1 && handler_type == 1)) {
            found = 1;
            break;
          }
        }

        if (!found) {
          widget_instance_go_back_to_previous(w);
          sound_effect = 3;
          widget_deleted = true;
          consumed = true;
        }
      }
    }
  }

  if (!widget_deleted) {
    timeout = *(int *)((char *)w + 0x1c);
    if (timeout > 0) {
      elapsed = *(int *)0x46cc40 - *(int *)((char *)w + 0x18);
      fade_ticks = *(int *)((char *)w + 0x20);

      if ((uint32_t)elapsed >= (uint32_t)(timeout + fade_ticks)) {
        child = (int)w;
        while (*(int *)(child + 0x30) != 0) {
          child = *(int *)(child + 0x30);
        }
        ui_widget_delete((void *)child);
        widget_deleted = true;
        goto after_local_handling;
      }

      if (fade_ticks > 0 && (elapsed - timeout) > 0) {
        float fade_den = (float)fade_ticks;
        if (fade_ticks < 0) {
          fade_den += *(float *)0x25fb8c;
        }
        *(float *)((char *)w + 0x24) =
          1.0f - (float)(elapsed - timeout) / fade_den;
      }
    }

    if (*(int16_t *)((char *)w + 0x52) < 0) {
      *(int16_t *)((char *)w + 0x52) = 0;
    }
    if (*(int16_t *)((char *)w + 0x54) < 0) {
      *(int16_t *)((char *)w + 0x54) = 0;
    }

    if (*(int16_t *)((char *)w + 0xe) == 2) {
      for (child = *(int *)((char *)w + 0x34); child != 0;
           child = *(int *)(child + 0x2c)) {
        *(int16_t *)(child + 0x50) = 0;
        if (child == *(int *)((char *)w + 0x38) &&
            *(int16_t *)(child + 0x56) == 2) {
          *(int16_t *)(child + 0x50) = 1;
        }
      }
    } else if (*(int16_t *)((char *)w + 0xe) == 3) {
      column_list_update(w, definition);
    }

    if (allowed_player) {
      flags = *(uint32_t *)(definition + 0xb);

      if ((flags & 8) != 0 && *(int *)((char *)w + 0x38) != 0 &&
          !widget_deleted) {
        if (*(int16_t *)event == 3 && event[5] == 1) {
          if (event[4] == 8) {
            widget_instance_tab_to_previous_valid_widget(w);
            sound_effect = 1;
            consumed = true;
          } else if (event[4] == 9) {
            widget_instance_tab_to_next_valid_widget(w);
            sound_effect = 1;
            consumed = true;
          }
        } else if (*(int16_t *)event == 1) {
          if (*(int16_t *)(event + 6) == (int16_t)0x8000) {
            widget_instance_tab_to_next_valid_widget(w);
            sound_effect = 1;
            consumed = true;
          } else if (*(int16_t *)(event + 6) == 0x7fff) {
            widget_instance_tab_to_previous_valid_widget(w);
            sound_effect = 1;
            consumed = true;
          }
        }
      }

      if (!consumed && (flags & 0x10) != 0 && *(int *)((char *)w + 0x38) != 0 &&
          !widget_deleted) {
        if (*(int16_t *)event == 3 && event[5] == 1) {
          if (event[4] == 0xa) {
            widget_instance_tab_to_previous_valid_widget(w);
            if (sound_effect == 0) {
              sound_effect = 1;
            }
            consumed = true;
          } else if (event[4] == 0xb) {
            widget_instance_tab_to_next_valid_widget(w);
            if (sound_effect == 0) {
              sound_effect = 1;
            }
            consumed = true;
          }
        } else if (*(int16_t *)event == 1) {
          if (*(int16_t *)(event + 4) == (int16_t)0x8000) {
            widget_instance_tab_to_previous_valid_widget(w);
            if (sound_effect == 0) {
              sound_effect = 1;
            }
            consumed = true;
          } else if (*(int16_t *)(event + 4) == 0x7fff) {
            widget_instance_tab_to_next_valid_widget(w);
            if (sound_effect == 0) {
              sound_effect = 1;
            }
            consumed = true;
          }
        }
      }

      type = *(int16_t *)((char *)w + 0xe);
      if ((flags & 0x20) != 0 && (type == 2 || type == 3) && !consumed &&
          !widget_deleted) {
        if (*(int16_t *)event == 3 && event[5] == 1) {
          if (event[4] == 8) {
            widget_event_function_list_widget_goto_previous_item(
              w, event, (char *)&widget_deleted);
            if (sound_effect == 0) {
              sound_effect = 1;
            }
            consumed = true;
          } else if (event[4] == 9) {
            widget_event_function_list_widget_goto_next_item(
              w, event, (char *)&widget_deleted);
            if (sound_effect == 0) {
              sound_effect = 1;
            }
            consumed = true;
          }
        } else if (*(int16_t *)event == 1) {
          if (*(int16_t *)(event + 6) == (int16_t)0x8000) {
            widget_event_function_list_widget_goto_next_item(
              w, event, (char *)&widget_deleted);
            if (sound_effect == 0) {
              sound_effect = 1;
            }
            consumed = true;
          } else if (*(int16_t *)(event + 6) == 0x7fff) {
            widget_event_function_list_widget_goto_previous_item(
              w, event, (char *)&widget_deleted);
            if (sound_effect == 0) {
              sound_effect = 1;
            }
            consumed = true;
          }
        }
      }

      if ((flags & 0x40) != 0 && (type == 2 || type == 3) && !consumed &&
          !widget_deleted) {
        if (*(int16_t *)event == 3 && event[5] == 1) {
          if (event[4] == 0xa) {
            widget_event_function_list_widget_goto_previous_item(
              w, event, (char *)&widget_deleted);
            if (sound_effect == 0) {
              sound_effect = 1;
            }
            consumed = true;
          } else if (event[4] == 0xb) {
            widget_event_function_list_widget_goto_next_item(
              w, event, (char *)&widget_deleted);
            if (sound_effect == 0) {
              sound_effect = 1;
            }
            consumed = true;
          }
        } else if (*(int16_t *)event == 1) {
          if (*(int16_t *)(event + 4) == (int16_t)0x8000) {
            widget_event_function_list_widget_goto_previous_item(
              w, event, (char *)&widget_deleted);
            if (sound_effect == 0) {
              sound_effect = 1;
            }
            consumed = true;
          } else if (*(int16_t *)(event + 4) == 0x7fff) {
            widget_event_function_list_widget_goto_next_item(
              w, event, (char *)&widget_deleted);
            if (sound_effect == 0) {
              sound_effect = 1;
            }
            consumed = true;
          }
        }
      }
    }
  }

after_local_handling:
  if (allowed_player && *(int *)(definition + 0x15) > 0) {
    offset = 0;
    for (i = 0; i < *(int *)(definition + 0x15) && !widget_deleted; i++) {
      uint8_t *event_handler =
        (uint8_t *)(*(int *)(definition + 0x16) + offset);
      bool matches = false;

      type = *(int16_t *)event;
      if (type == 1) {
        switch (*(int16_t *)(event_handler + 4)) {
        case 0x10:
          matches = *(int16_t *)(event + 6) == 0x7fff;
          break;
        case 0x11:
          matches = *(int16_t *)(event + 6) == (int16_t)0x8000;
          break;
        case 0x12:
          matches = *(int16_t *)(event + 4) == (int16_t)0x8000;
          break;
        case 0x13:
          matches = *(int16_t *)(event + 4) == 0x7fff;
          break;
        default:
          break;
        }
      } else if (type == 2) {
        switch (*(int16_t *)(event_handler + 4)) {
        case 0x14:
          matches = *(int16_t *)(event + 6) == 0x7fff;
          break;
        case 0x15:
          matches = *(int16_t *)(event + 6) == (int16_t)0x8000;
          break;
        case 0x16:
          matches = *(int16_t *)(event + 4) == (int16_t)0x8000;
          break;
        case 0x17:
          matches = *(int16_t *)(event + 4) == 0x7fff;
          break;
        default:
          break;
        }
      } else if (type == 3 && *(int16_t *)(event_handler + 4) == event[4]) {
        matches = event[5] == 1;
      }

      if (matches) {
        consumed = true;
        event_handler_dispatch(w, definition, event, event_handler,
                               (char *)&widget_deleted);
      }

      offset += 0x48;
    }
  }

  flags = *(uint32_t *)(definition + 0xb);
  if ((flags & 0x400) != 0 && (flags & 1) == 0) {
    display_assert("if the _widget_pass_handled_events_to_all_children_bit "
                   "flag is checked, "
                   "_widget_pass_unhandled_events_to_children_bit must also "
                   "be checked for it to work",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0xd95, false);
  }

  if (!(((flags & 0x400) == 0 && consumed) ||
        (((flags & 1) == 0 && (flags & 0x100) == 0)) || widget_deleted)) {
    if ((flags & 0x100) == 0) {
      child = *(int *)((char *)w + 0x38);
      if (child != 0) {
        int16_t child_player = *(int16_t *)(child + 8);
        if (child_player == -1 || child_player == *(int16_t *)(event + 2)) {
          child_tag = *(int *)child;
          widget_instance_process_one_event_recursive(
            (void *)(uintptr_t)child, tag_get(0x44654c61, child_tag), event,
            (char *)&widget_deleted);
        }
      }
    } else {
      child = *(int *)((char *)w + 0x34);
      while (child != 0) {
        int16_t child_player = *(int16_t *)(child + 8);
        if (child_player == -1 || child_player == *(int16_t *)(event + 2)) {
          child_tag = *(int *)child;
          widget_instance_process_one_event_recursive(
            (void *)(uintptr_t)child, tag_get(0x44654c61, child_tag), event,
            (char *)&widget_deleted);
          if (widget_deleted) {
            break;
          }
        }
        child = *(int *)(child + 0x2c);
      }
    }
  }

  if (widget_deleted && (flags & 0x800) != 0) {
    for (i = 0; i < 4; i++) {
      if (*(int *)(0x46cc20 + i * 4) != 0) {
        break;
      }
    }
    if (i == 4) {
      main_goto_main_menu();
    }
  }

  if (*(int16_t *)event == 3 && event[5] == 1 && *(int16_t *)(event + 2) >= 0 &&
      *(int16_t *)(event + 2) < 4 && event[4] >= 8 && event[4] < 0xc) {
    *(int *)(0x46cc90 + ((event[4] - 8) + *(int16_t *)(event + 2) * 4) * 4) =
      *(int *)0x46cc40;
  }

  switch (sound_effect) {
  case 1:
    sound_name = "sound\\sfx\\ui\\cursor";
    break;
  case 2:
    sound_name = "sound\\sfx\\ui\\forward";
    break;
  case 3:
    sound_name = "sound\\sfx\\ui\\back";
    break;
  default:
    sound_name = NULL;
    break;
  }

  if (sound_name != NULL) {
    sound_tag = tag_loaded(0x736e6421, sound_name);
    if (sound_tag != -1) {
      sound_impulse_start(sound_tag, 1.0f);
    }
  }

  *handled_out = (uint8_t)widget_deleted;
}

__declspec(noinline) void *ui_widget_load_by_name_or_tag(const char *name,
                                                         int tag_index, int a3,
                                                         int widget_stack,
                                                         int parent_tag_index,
                                                         int a6, int a7)
{
  typedef struct ui_widget_pending_load_entry {
    int tag_index;
    int a6;
    int16_t a7;
    int16_t widget_stack;
  } ui_widget_pending_load_entry_t;

  int tag_data;
  int widget;
  int widget_stack_base;
  int16_t stack_index;
  int16_t previous_stack_player;
  int root_widget;
  ui_widget_pending_load_entry_t pending_load;

  widget_stack_base = ((int16_t)widget_stack == -1) ? 0 : widget_stack;

  if (*(uint8_t *)0x46cc82 == 0) {
    display_assert("widget_globals.initialized",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x179, true);
    system_exit(-1);
  }

  if (name == NULL && tag_index == -1) {
    display_assert("(name != NULL) || (tag_index != NONE)",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x17a, true);
    system_exit(-1);
  }

  stack_index = (int16_t)widget_stack_base;
  if (stack_index < 0 || stack_index >= 4) {
    display_assert("(widget_stack>=0) && (widget_stack<MAXIMUM_GAMEPADS)",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x17b, true);
    system_exit(-1);
  }

  if (tag_index == -1) {
    tag_index = tag_loaded(0x44654c61, name);
    if (tag_index == -1) {
      error(2, "ui_widget_definition tag '%s'/%d not loaded", name, -1);
      return NULL;
    }
  }

  tag_data = (int)tag_get(0x44654c61, tag_index);
  widget = (int)stack_memory_pool_allocate(
    *(void **)0x31e04c, 0x58, "c:\\halo\\SOURCE\\interface\\ui_widget.c",
    0x18b);
  if (widget == 0) {
    error(2, "failed to create new widget; out of memory!");
    return NULL;
  }

  if (a3 == 0) {
    root_widget = *(int *)(0x46cc20 + (int)stack_index * 4);
    if (root_widget != 0) {
      previous_stack_player = *(int16_t *)(root_widget + 8);
      ui_widget_delete((void *)root_widget);
    } else {
      previous_stack_player = -1;
    }

    *(int *)(0x46cc20 + (int)stack_index * 4) = widget;

    if (parent_tag_index != -1 &&
        (*(uint32_t *)((int)tag_get(0x44654c61, parent_tag_index) + 0x2c) &
         0x4000) == 0) {
      pending_load.tag_index = parent_tag_index;
      pending_load.a6 = a6;
      pending_load.a7 = (int16_t)a7;
      pending_load.widget_stack = previous_stack_player;
      push_widget((int *)(0x46cc30 + (int)stack_index * 4), &pending_load);
    }
  }

  if ((int16_t)widget_stack == -1) {
    switch (*(int16_t *)(tag_data + 2)) {
    case 0:
      widget_stack = 0;
      break;
    case 1:
      widget_stack = 1;
      break;
    case 2:
      widget_stack = 2;
      break;
    case 3:
      widget_stack = 3;
      break;
    case 4:
      widget_stack = -1;
      break;
    default:
      break;
    }
  }

  widget_instance_initialize((void *)tag_data, (void *)widget, (void *)a3,
                             tag_index, widget_stack, widget_stack_base);
  return (void *)widget;
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
    if (crt_stricmp(command_line, "xdemo") != 0) {
    play_intro:
      bink_playback_start("d:\\bink\\intro.bik", 0xe6);
      play_main_menu = false;
      if (bink_playback_active() == 0) {
        play_main_menu = true;
      }
    } else {
      error(2, "xbox command line= '%s'", command_line);
    }
    perform_filesystem_initialization();
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
    ui_start_main_menu_music();
  }

  reset_last_player1_profile_index();

done:
  if (!virtual_keyboard_initialize()) {
    error(2, "failed to initialize the virtual keyboard");
  }
  *(uint8_t *)0x31e050 = 0;
}

void ui_widget_display_error(int16_t error_handle, int local_player_index,
                             char is_modal, char pause_game)
{
  int16_t stack_index;
  int16_t local_player_count;
  int16_t local_player;
  int16_t matched_player;
  int16_t text_value;
  bool target_is_primary;
  const char *widget_name;
  int root_widget;
  int root_tag_index;
  int widget;
  int text_widget;
  int widget_stack_index;
  int16_t deferred_slot;

  if (cinematic_in_progress()) {
    stack_index = (int16_t)local_player_index;
    if (stack_index == -1) {
      stack_index = 0;
    } else if (stack_index < 0 || stack_index >= 4) {
      display_assert(
        "local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_"
        "PLAYERS",
        "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x81d, true);
      system_exit(-1);
    }

    deferred_slot = *(int16_t *)(0x46cc6c + (int)stack_index * 4);
    if (deferred_slot != -1) {
      error(2,
            "there is already a deferred-for-cinematic error queued for player"
            " #%d; ignoring this one",
            (int)stack_index);
      return;
    }

    *(int16_t *)(0x46cc6c + (int)stack_index * 4) = error_handle;
    *(uint8_t *)(0x46cc6e + (int)stack_index * 4) = (uint8_t)is_modal;
    *(uint8_t *)(0x46cc6f + (int)stack_index * 4) = (uint8_t)pause_game;
    return;
  }

  stack_index = (int16_t)local_player_index;
  local_player_count = 0;
  local_player = -1;
  matched_player = -1;
  target_is_primary = true;

  if (stack_index != -1) {
    local_player = local_player_get_next(-1);
    while (local_player != -1) {
      if (local_player == stack_index) {
        matched_player = stack_index;
        if (local_player_count > 0) {
          target_is_primary = false;
        }
      }
      local_player_count++;
      local_player = local_player_get_next(local_player);
    }

    if (*(uint8_t *)0x46cc88 == 0) {
      if (matched_player == -1) {
        stack_index = -1;
      }
    }
  }

  switch (local_player_count) {
  case 0:
  case 1:
    widget_name = is_modal ? "ui\\shell\\error\\error_modal_fullscreen" :
                             "ui\\shell\\error\\error_nonmodal_fullscreen";
    break;
  case 2:
    widget_name = is_modal ? "ui\\shell\\error\\error_modal_halfscreen" :
                             "ui\\shell\\error\\error_nonmodal_halfscreen";
    break;
  case 3:
    if (target_is_primary) {
      widget_name = is_modal ? "ui\\shell\\error\\error_modal_halfscreen" :
                               "ui\\shell\\error\\error_nonmodal_halfscreen";
    } else {
      widget_name = is_modal ? "ui\\shell\\error\\error_modal_qtrscreen" :
                               "ui\\shell\\error\\error_nonmodal_qtrscreen";
    }
    break;
  case 4:
    widget_name = is_modal ? "ui\\shell\\error\\error_modal_qtrscreen" :
                             "ui\\shell\\error\\error_nonmodal_qtrscreen";
    break;
  default:
    display_assert("invalid local player count",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x871, true);
    system_exit(-1);
    return;
  }

  if (stack_index == -1) {
    widget_stack_index = 0;
  } else {
    if (stack_index < 0 || stack_index >= 4) {
      display_assert("(widget_stack>=0) && (widget_stack<MAXIMUM_NUMBER_OF_"
                     "LOCAL_PLAYERS)",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x878, true);
      system_exit(-1);
    }
    widget_stack_index = stack_index;
  }

  if (*(uint8_t *)0x46cc88 != 0 && *(float *)0x46cc4c <= 1.0f &&
      *(float *)0x46cc4c >= 0.0f) {
    error(2, "aborting to the main menu root, for safety's sake");
    main_screen_shell_load();
    main_defer_map_map_change();
    *(float *)0x46cc4c = -1.0f;
  }

  root_widget = *(int *)(0x46cc20 + widget_stack_index * 4);
  if (root_widget == 0) {
    root_tag_index = -1;
  } else {
    root_tag_index = *(int *)root_widget;
    if (*(uint8_t *)(root_widget + 0x15) == 1) {
      error(2,
            "there is already an error message displayed for this local player"
            " index");
      goto error_failed;
    }
  }

  widget = (int)ui_widget_load_by_name_or_tag(widget_name, -1, 0, stack_index,
                                              root_tag_index, -1, -1);
  if (widget == 0) {
  error_failed:
    error(2, "failed to display error message");
    return;
  }

  if (*(int *)(widget + 0x34) == 0 ||
      *(int *)(*(int *)(widget + 0x34) + 0x34) == 0) {
    display_assert("error screen widget tag not layed out as expected",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x89e, true);
    system_exit(-1);
  }

  text_widget = *(int *)(*(int *)(widget + 0x34) + 0x34);
  if (*(int16_t *)(text_widget + 0xe) != 1) {
    display_assert("expected a text box widget in the error widget",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x8a0, true);
    system_exit(-1);
  }

  if (error_handle < 0) {
    text_value = 0;
  } else if (error_handle > 0x27) {
    text_value = 0x27;
  } else {
    text_value = error_handle;
  }
  *(int16_t *)(text_widget + 0x40) = text_value;

  *(uint8_t *)(widget + 0x15) = 1;
  if (*(uint8_t *)(widget + 0x13) == 0) {
    *(uint8_t *)(widget + 0x13) = (uint8_t)pause_game;
    if (pause_game == 1) {
      if (*(int16_t *)0x46cc4a < 0) {
        display_assert("widget pause counter is out of whack",
                       "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x8a9, true);
        system_exit(-1);
      }

      (*(int16_t *)0x46cc4a)++;
      if (!game_time_get_paused()) {
        game_time_set_paused(1);
      }

      if (*(uint8_t *)0x46cc87 == 0 && *(uint8_t *)0x46cc88 == 0) {
        sound_set_music_enabled(1);
        *(uint8_t *)0x46cc87 = 1;
      }
    }
  }

  switch (error_handle) {
  case 0xd:
    *(uint8_t *)(widget + 0x16) = 1;
    /* fallthrough */
  case 0xc:
    *(int *)(widget + 0x1c) = 0;
    *(int *)(widget + 0x20) = 0;
    break;
  default:
    *(uint8_t *)(widget + 0x16) = 0;
    break;
  }
}

/* ui_widget_load_error_screen — displays a fatal/abort error overlay that
 * forces the player back to the Xbox dashboard. If allow_abort is true
 * (== 1), the "error_abort_to_dashboard" widget is shown (user can confirm);
 * otherwise "error_abort_to_dashboard_you_have_no_choice" is shown and all
 * existing widgets are closed first. The loaded widget's text-box child
 * receives the error_handle string index at +0x40, its in_game_mode flag
 * (+0x15) is set, and the global "last displayed error" at 0x31e054 is
 * updated. Asserts that the widget's type (+0x0e) is 1 (text box).
 * Source line: 0x90f in ui_widget.c. */
void ui_widget_load_error_screen(int16_t error_handle, int allow_abort)
{
  const char *widget_name;
  void *widget;
  bool abort = *(bool *)&allow_abort;

  if (abort == 1) {
    widget_name = "ui\\shell\\error\\error_abort_to_dashboard";
  } else {
    widget_name =
      "ui\\shell\\error\\error_abort_to_dashboard_you_have_no_choice";
    if (abort == 0) {
      ui_widgets_close_all();
    }
  }

  widget = ui_widget_load_by_name_or_tag(widget_name, -1, 0, -1, -1, -1, -1);
  if (widget != NULL) {
    if (*(int16_t *)((char *)widget + 0xe) != 1) {
      display_assert("expected a text box widget",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x90f, true);
      system_exit(-1);
    }
    *(int16_t *)((char *)widget + 0x40) = error_handle;
    *(uint8_t *)((char *)widget + 0x15) = 1;
    *(int16_t *)0x31e054 = error_handle;
    return;
  }
  error(2, "failed to load '%s' widget", widget_name);
}

bool ui_check_for_pause_game(void)
{
  int stack_index;
  int i;
  int root_widget;
  int pause_ticks;
  int16_t local_player_count;
  int16_t local_player;
  int16_t target_local_player;
  bool network_game;
  bool handled;
  bool target_is_primary;
  const char *widget_name;
  void *gamepad_state;
  void *client;

  handled = false;
  network_game = network_game_in_progress();

  if (game_in_progress() && !cinematic_in_progress() &&
      game_connection() != 3 && *(uint8_t *)0x46cc88 == 0 &&
      dword_46CC44 == 0) {
    for (stack_index = 0; stack_index < 4; stack_index++) {
      if (!input_has_gamepad((int16_t)stack_index) ||
          !local_player_exists((int16_t)stack_index)) {
        continue;
      }

      gamepad_state = input_get_gamepad_state(stack_index);
      if (*(uint8_t *)((char *)gamepad_state + 0x1c) != 1) {
        continue;
      }

      handled = true;
      target_is_primary = true;
      local_player_count = 0;
      target_local_player = -1;

      local_player = local_player_get_next(-1);
      while (local_player != -1) {
        if (local_player == (int16_t)stack_index) {
          target_local_player = (int16_t)stack_index;
          if (local_player_count > 0) {
            target_is_primary = false;
          }
        }
        local_player_count++;
        local_player = local_player_get_next(local_player);
      }

      if (network_game) {
        if (game_engine_allow_pause() &&
            target_local_player == (int16_t)stack_index) {
          root_widget = *(int *)(0x46cc20 + stack_index * 4);
          if (root_widget == 0) {
            client = global_network_game_client_get();
            network_game_client_get_game(client);
            network_game_client_get_machine_index(client);

            switch (local_player_count) {
            case 1:
              widget_name =
                "ui\\shell\\multiplayer_game\\pause_game\\1p_pause_game";
              break;
            case 2:
              widget_name =
                "ui\\shell\\multiplayer_game\\pause_game\\2p_pause_game";
              break;
            case 3:
              if (target_is_primary) {
                widget_name =
                  "ui\\shell\\multiplayer_game\\pause_game\\2p_pause_game";
              } else {
                widget_name =
                  "ui\\shell\\multiplayer_game\\pause_game\\4p_pause_game";
              }
              break;
            case 4:
              widget_name =
                "ui\\shell\\multiplayer_game\\pause_game\\4p_pause_game";
              break;
            default:
              error(2, "invalid local player count for multiplayer game");
              goto done;
            }

            if (ui_widget_load_by_name_or_tag(widget_name, -1, 0, stack_index,
                                              -1, -1, -1) == 0) {
              error(2, "failed to load multiplayer pause game window");
            }
          } else {
            ui_widget_delete((void *)root_widget);
          }
        }
      } else {
        if (local_player_count < 0 || local_player_count > 2) {
          error(2, "the ui seems to be confused... assuming you are playing "
                   "full-screen single player?");

          if (*(uint8_t *)0x46cc82 != 0) {
            for (i = 0; i < 4; i++) {
              if (*(int *)(0x46cc20 + i * 4) != 0) {
                ui_widgets_close_all();
                break;
              }
            }
          }

          if (ui_widget_load_by_name_or_tag(
                "ui\\shell\\solo_game\\pause_game\\pause_game", -1, 0,
                stack_index, -1, -1, -1) == 0) {
            error(2, "failed to load full screen pause game window");
          }
          goto done;
        }

        root_widget = *(int *)(0x46cc20 + stack_index * 4);
        if (local_player_count == 2 && root_widget == 0) {
          if (!game_time_get_paused()) {
            if (ui_widget_load_by_name_or_tag(
                  "ui\\shell\\solo_game\\pause_game\\pause_game_split_"
                  "screen",
                  -1, 0, stack_index, -1, -1, -1) == 0) {
              error(2, "failed to load split screen pause game window");
            }
          }
          goto done;
        }

        if (root_widget == 0) {
          if (ui_widget_load_by_name_or_tag(
                "ui\\shell\\solo_game\\pause_game\\pause_game", -1, 0,
                stack_index, -1, -1, -1) == 0) {
            error(2, "failed to load full screen pause game window");
          }
          goto done;
        }

        if (game_time_get_paused()) {
          ui_widgets_close_all();
        }
      }

    done:
      break;
    }
  }

  pause_ticks = dword_46CC44 - 1;
  dword_46CC44 = (((pause_ticks < 0) ? 1 : 0) - 1) & pause_ticks;
  return handled;
}

/* ui_widget_launch_widget — creates a new widget from an event
 * handler's spawn tag_index (called from ui_widget_delete when a "widget
 * deleted" handler has the spawn bit set). Looks up the DeLa (UI widget
 * definition) tag for tag_index and resolves the target widget_stack slot
 * from the tag's own controller_index field (offset +2, same field/values
 * switched on in ui_widget_load_by_name_or_tag): if the definition's
 * "explicit controller" flag (+0x2c & 0x1000) is set, cases 0-3 select
 * that stack directly and case 4 means "no specific player" (-1); if the
 * flag is clear, cases 0-3 are identical but case 4 instead inherits the
 * spawning widget's own local_player_index (+8). Any other
 * controller_index value halts (two separate halt sites, one per flag
 * branch, hence the two different line numbers below). Then walks up the
 * spawning widget's parent chain (+0x30) to find the root ancestor, and
 * searches the immediate parent's child list (+0x34 first_child, +0x2c
 * next_sibling) for the spawning widget's own index among its siblings.
 * Finally loads the new widget via ui_widget_load_by_name_or_tag, passing
 * the root ancestor's tag_index, the immediate parent's tag_index (or -1
 * if there is no parent), and the sibling index (or -1 if not found). */
void *ui_widget_launch_widget(void *widget, int tag_index)
{
  int tag_data;
  int widget_stack;
  int *parent;
  int *walker;
  int *root;
  int immediate_parent_tag_index;
  int sibling_index;
  void *child;
  int index;
  void *new_widget;

  tag_data = (int)tag_get(0x44654c61, tag_index);

  if ((*(uint32_t *)(tag_data + 0x2c) & 0x1000) != 0) {
    switch (*(int16_t *)(tag_data + 2)) {
    case 0:
      widget_stack = 0;
      break;
    case 1:
      widget_stack = 1;
      break;
    case 2:
      widget_stack = 2;
      break;
    case 3:
      widget_stack = 3;
      break;
    case 4:
      widget_stack = -1;
      break;
    default:
      display_assert("invalid widget controller index specified",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x1504, true);
      system_exit(-1);
      break;
    }
  } else {
    switch (*(int16_t *)(tag_data + 2)) {
    case 0:
      widget_stack = 0;
      break;
    case 1:
      widget_stack = 1;
      break;
    case 2:
      widget_stack = 2;
      break;
    case 3:
      widget_stack = 3;
      break;
    case 4:
      widget_stack = *(int16_t *)((char *)widget + 8);
      break;
    default:
      display_assert("invalid widget controller index specified",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x1510, true);
      system_exit(-1);
      break;
    }
  }

  parent = *(int **)((char *)widget + 0x30);
  root = (int *)widget;
  walker = parent;
  while (walker != NULL) {
    root = walker;
    walker = (int *)walker[0xc];
  }

  immediate_parent_tag_index = (parent == NULL) ? -1 : *parent;

  sibling_index = -1;
  if (parent != NULL && (child = (void *)parent[0xd]) != NULL) {
    index = 0;
    do {
      sibling_index = index;
      if (child == widget) {
        break;
      }
      child = *(void **)((char *)child + 0x2c);
      index++;
      sibling_index = -1;
    } while (child != NULL);
  }

  new_widget =
    ui_widget_load_by_name_or_tag(NULL, tag_index, 0, widget_stack, *root,
                                  immediate_parent_tag_index, sibling_index);
  if (new_widget == NULL) {
    error(2, "event handler failed to spawn widget");
  }

  return new_widget;
}

typedef struct ui_widget_process_data {
  int16_t unk0;
  int16_t unk2;
  int16_t unk4;
  int16_t unk6;
} ui_widget_process_data_t;

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


/* process_ui_widgets — main per-frame UI widget tick. Handles async
 * filesystem operations, bink video updates, pre-title screen logic,
 * deferred error display, and the per-stack widget event dispatch loop.
 * Called once per frame from the main loop. */
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
    system_exit(-1);
  }

  /* Record frame timestamp for event throttling. */
  *(uint32_t *)0x46cc40 = system_milliseconds();

  /* If an async filesystem operation is pending, poll for completion. */
  if (*(int *)0x46cc7c != 0) {
    if (thread_is_done((void *)*(int *)0x46cc7c) != 0) {
      thread_close((void *)*(int *)0x46cc7c);
      *(int *)0x46cc7c = 0;
      ui_widgets_inhibit_processing(false);
      if (*(int16_t *)0x46cc80 == 1) {
        if (bink_playback_has_video()) {
          bink_playback_stop();
        }
        ui_widget_load_error_screen(0x21, 1);
        return;
      }
      if (*(int16_t *)0x46cc80 == 2) {
        if (bink_playback_has_video()) {
          bink_playback_stop();
        }
        ui_widget_load_error_screen(0x22, 1);
        return;
      }
    }
    return;
  }

  /* If UI automation is driving the menu, skip normal processing. */
  if (ui_automation_is_active()) {
    return;
  }

  /* If a bink video is playing, update it and flush events. */
  if (((bool (*)(void))0xf5640)() != 0) {
    ((void (*)(void))0xf6740)();
    event_manager_flush();
    return;
  }

  /* Pre-title screen (language select / content rating). */
  if (event_manager_tab_check()) {
    event_manager_tab_process();
    return;
  }

  /* If a pending error screen load is queued, dispatch it now. */
  if (*(int16_t *)0x46cc68 != -1) {
    ui_widget_load_error_screen(*(int16_t *)0x46cc68, *(uint8_t *)0x46cc6a);
    *(int16_t *)0x46cc68 = -1;
    return;
  }

  /* If any deferred error slots are populated, try to show them. */
  if ((*(int16_t *)0x46cc50 == -1) && (*(int16_t *)0x46cc56 == -1) &&
      (*(int16_t *)0x46cc5c == -1) && (*(int16_t *)0x46cc62 == -1)) {
    /* Normal widget event processing path. */
    blocked_by_pause = ui_check_for_pause_game() != 0;

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
          (event_manager_get_next_event(&process_data,
                                        *(uint16_t *)(widget + 8)) != 0)) {
        do {
          handled = 0;
          if (blocked_by_pause == 0) {
            widget_instance_process_one_event_recursive(
              (void *)widget, widget_tag, &process_data, &handled);
          }
          if ((handled == 1) || (widget != *widget_roots)) {
            break;
          }
        } while (event_manager_get_next_event(&process_data,
                                              *(uint16_t *)(widget + 8)) != 0);
      } else if (blocked_by_pause == 0) {
        process_data.unk2 = *(uint16_t *)(widget + 8);
        handled = 0;
        widget_instance_process_one_event_recursive((void *)widget, widget_tag,
                                                    &process_data, &handled);
      }

      did_work = 1;
      if ((*widget_roots == 0) && (widget_roots[4] != 0)) {
        pop_widget(&widget_roots[4], (void *)&pending_load);
        if (pending_load.tag_index != -1) {
          loaded_widget = ui_widget_load_by_name_or_tag(
            0, pending_load.tag_index, 0, pending_load.widget_stack, -1, -1,
            -1);
          if (loaded_widget != 0) {
            widget_instance_set_focused_child_by_index(
              pending_load.a6, (int)loaded_widget, pending_load.a7);
          }
        }
      }
    }

    if (did_work != 0) {
      event_manager_flush();
      return;
    }
    return;
  }

  /* Deferred error display: wait for game_in_progress and enough
   * ticks before showing queued error dialogs. */
  deferred_error = (ui_widget_deferred_error_t *)0x46cc50;
  while ((int)deferred_error < 0x46cc68) {
    if (deferred_error->error_handle != -1) {
      if ((*(uint8_t *)0x46cc88 == 0) && (!network_game_in_progress()) &&
          (game_time_get() < 0x1e)) {
        error(2, "waiting for %d ticks before displaying deferred errors",
              0x1e);
      } else {
        ui_widget_display_error(
          deferred_error->error_handle, deferred_error->local_player_index,
          (char)deferred_error->a3, (char)deferred_error->a4);
        deferred_error->error_handle = -1;
      }
    }
    deferred_error++;
  }
}

/**
 * Clears the last error index by resetting it to -1 (no error).
 * The global at 0x31e4c0 tracks which error was most recently displayed
 * by the UI widget error system.
 */
__declspec(noinline) void reset_last_player1_profile_index(void)
{
  *(int *)0x31e4c0 = -1;
}

/* initialize sp level list (0x0e98c0) — rebuilds the 0x50-byte single-player
 * level list scratch block at 0x46cce8 (10 entries x 8 bytes: a level name
 * pointer from the table at 0x31e498 plus four flag bytes at +4..+7).  An
 * entry is unlocked when either local player's profile flag byte (profile
 * offset 0x1c + i) is set, when i is one past either player's stored last
 * level played, or for i == 0 (the first level is always available).  Then
 * asserts the bound 'solo level list' widget is a spinner list ('DeLa' type
 * 2) with 3 list items, publishes the block pointer/count at widget +0x40 /
 * +0x44 and clamps the selected index at +0x3c to [0, 9]. */
bool ui_widget_initialize_single_player_level_list(void *widget,
                                                   void *event_data,
                                                   bool *widget_deleted)
{
  uint8_t profile0[0x30];
  uint8_t profile1[0x30];
  int16_t last_level0;
  int16_t last_level_unused0;
  int16_t last_level1;
  int16_t last_level_unused1;
  int i;
  uint8_t flags0;
  unsigned int flags;
  int16_t *list_tag;
  int16_t selected;

  (void)event_data;
  (void)widget_deleted;

  csmemset((void *)0x46cce8, 0, 0x50);
  player_ui_get_active_player_profile(0, profile0);
  player_profile_save_last_level_played(profile0, &last_level0,
                                        &last_level_unused0);
  player_ui_get_active_player_profile(1, profile1);
  player_profile_save_last_level_played(profile1, &last_level1,
                                        &last_level_unused1);

  /* Spelled do/while: the reference is a bottom-tested loop MSVC left rolled
   * (INC EAX / CMP EAX,0xa / JL). The equivalent `for (i = 0; i < 10; i++)`
   * body is fully unrolled 5x by clang (294 candidate insns vs the
   * reference's 138, 60.6% match). */
  i = 0;
  do {
    *(const char **)(0x46cce8 + i * 8) = ((const char **)0x31e498)[i];
    flags0 = profile0[0x1c + i];
    if ((flags0 != 0) || (i == (int)last_level0 + 1) ||
        (profile1[0x1c + i] != 0) || (i == (int)last_level1 + 1) || (i == 0)) {
      flags = (unsigned int)((int)(signed char)profile1[0x1c + i] |
                             (int)(signed char)flags0);
      *(uint8_t *)(0x46cce8 + i * 8 + 5) = (uint8_t)((flags >> 1) & 1);
      *(uint8_t *)(0x46cce8 + i * 8 + 4) = 1;
      *(uint8_t *)(0x46cce8 + i * 8 + 6) = (uint8_t)((flags >> 2) & 1);
      *(uint8_t *)(0x46cce8 + i * 8 + 7) = (uint8_t)((flags >> 3) & 1);
    }
    i++;
  } while (i < 10);

  list_tag = (int16_t *)tag_get(0x44654c61, *(int *)widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'solo level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x2b1,
      true);
    system_exit(-1);
  }
  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'solo level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x2b2,
      true);
    system_exit(-1);
  }

  *(int *)((char *)widget + 0x40) = 0x46cce8;
  *(int16_t *)((char *)widget + 0x44) = 10;

  if (player_ui_get_last_single_player_level_played(0) < 0) {
    *(int16_t *)((char *)widget + 0x3c) = 0;
    return true;
  }
  if (player_ui_get_last_single_player_level_played(0) > 9) {
    *(int16_t *)((char *)widget + 0x3c) = 9;
    return true;
  }
  selected = player_ui_get_last_single_player_level_played(0);
  *(int16_t *)((char *)widget + 0x3c) = selected;
  return true;
}

/* dispose sp level list (event handler table index 7, 0x0e9a60) — clears the
 * 0x50-byte single-player level list scratch block at 0x46cce8 and drops the
 * widget's cached list pointer/count at +0x40/+0x44. */
bool solo_level_dispose_list(void *widget, void *event_data,
                             bool *widget_deleted)
{
  csmemset((void *)0x46cce8, 0, 0x50);
  *(int *)((char *)widget + 0x40) = 0;
  *(int16_t *)((char *)widget + 0x44) = 0;
  return true;
}

/* difficulty_set (event handler, 0x0e9bd0) — reads the widget's selected
 * item index (signed 16-bit) at +0x3c. If it is a valid difficulty
 * (0 <= selected < 4), applies it via main_set_difficulty and plays audio
 * feedback sound 2, then returns true. Otherwise it asserts (message plus
 * this file/line 0x313) and, since display_assert's halt argument is true,
 * falls through to system_exit(-1) — a path the reference marks
 * non-returning. */
bool difficulty_set(void *widget, void *event_data, bool *widget_deleted)
{
  (void)event_data;
  (void)widget_deleted;

  if (*(short *)((char *)widget + 0x3c) < 0 ||
      *(short *)((char *)widget + 0x3c) >= 4) {
    display_assert(
      "I don't think this is the difficulty list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x313,
      true);
    system_exit(-1);
  }

  main_set_difficulty(*(short *)((char *)widget + 0x3c));
  ui_play_audio_feedback_sound(2);
  return true;
}

/* join controller to multiplayer game (event handler table index ??,
 * 0x0e9cb0) — the widget's local_player_index field (+0x8) must already be
 * resolved to a specific gamepad (not NONE/-1); asserts and exits otherwise.
 * Forwards the (zero/sign-extended) index to
 * player_ui_local_player_joined_multiplayer_game and always returns true. */
bool player_wants_to_join_multiplayer_game(void *widget, void *event_data,
                                           bool *widget_deleted)
{
  char *local_player_index_ptr;

  (void)event_data;
  (void)widget_deleted;

  local_player_index_ptr = (char *)widget + 8;
  if (*(int16_t *)local_player_index_ptr == -1) {
    display_assert(
      "need a specific local player index when joining a multiplayer game",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x369,
      true);
    system_exit(-1);
  }

  player_ui_local_player_joined_multiplayer_game(
    *(int16_t *)local_player_index_ptr);
  return true;
}

/* start network game server if not already advertised (event handler table
 * index 17, 0x0e9d40) — disposes any existing server, clears the cached
 * multiplayer variant UI text, and re-enables incoming connections. If no
 * server is currently advertised, initializes the game engine playlist and
 * attempts to start hosting (create_global_network_game_server); on success,
 * fetches the fresh server handle, pauses its countdown, begins the playlist,
 * and switches the local game connection state to 2 (host). Once past that gate
 * (or if a server was already up), checks for a local client and, if none,
 * re-derives the result via create_global_network_game_client. On any failure
 * the server and client are torn down, "accept connections" is cleared, the
 * multiplayer variant text is re-cleared, and error 2 "failed to initiate a
 * multiplayer game server" is reported. widget/event_data/widget_deleted
 * are unused — the original never establishes a stack frame and never
 * touches its incoming event-handler params. Called both through the
 * dispatch table above and directly (tail-propagated) by
 * start_network_game_if_no_advertised_servers (0x0f01d0). */
bool network_game_start_new_server(void *widget, void *event_data,
                                   bool *widget_deleted)
{
  bool result;
  void *server;
  void *client;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  result = true;
  dispose_global_network_game_client();
  player_ui_clear_multiplayer_variant();
  network_game_set_accept_remote_connections(1);
  server = global_network_game_server_get();
  if (server == NULL) {
    game_engine_playlist_initialize();
    result = create_global_network_game_server();
    if (result) {
      server = global_network_game_server_get();
      network_game_server_pause_countdown(server, 1);
      game_engine_playlist_begin();
      set_game_connection(2);
    }
    if (!result) {
      goto fail;
    }
  }

  client = global_network_game_client_get();
  if (client == NULL) {
    result = create_global_network_game_client();
  }
  if (result) {
    return result;
  }

fail:
  dispose_global_network_game_server();
  dispose_global_network_game_client();
  network_game_set_accept_remote_connections(0);
  player_ui_clear_multiplayer_variant();
  error(2, "failed to initiate a multiplayer game server");
  return result;
}

/* join selected network game server (event handler, single data xref at
 * 0x31e1a8, 0x0e9dd0) — reads the widget's cached server-list pointer at
 * +0x40 and selected index at +0x3c (signed 16-bit) against the cached
 * count at +0x44, then validates the selected server entry: byte +0xe0
 * must be 1 ("open") and word +0xde must be 0 (same platform). It then
 * builds a transport_address for the entry via FUN_00082bd0 (entry+0x18,
 * entry+0x08, entry+0x00, port 0x141e), requires a non-zero address dword
 * and a non-zero port, prepares the join parameters (word +0x02 = 0,
 * token at +0x12) and hands them to
 * network_game_client_initiate_join_game. On success it spawns the
 * connected-pregame screen, switches the connection state to 1 (client),
 * and marks the widget deleted; a spawn failure also marks the widget
 * deleted but returns false.
 *
 * The widget+0x38 gate and every failure path return the [EBP-1] flag,
 * which is only ever set on the fully successful path.
 *
 * Uncertain: the meaning of the entry fields at +0x00/+0x08/+0x18 handed
 * to FUN_00082bd0 is not evidenced here, so they stay raw offsets; the
 * 0x18-byte address record is zeroed as six dwords because the callee
 * writes +0x14, past the declared transport_address tail. */
bool FUN_000e9dd0(void *widget, void *event_data, bool *widget_deleted)
{
  unsigned char address[0x18];
  unsigned char join_params[0x22];
  void *entry;
  void *spawned;
  void *last_child;
  int *player_index_ptr;
  int player_index;
  int child_index;
  short selected;
  bool result;

  (void)event_data;

  result = false;
  if (*(int *)((char *)widget + 0x38) == 0) {
    goto done;
  }
  selected = *(short *)((char *)widget + 0x3c);
  if (selected < 0) {
    goto done;
  }
  if ((int)selected < (int)*(uint16_t *)((char *)widget + 0x44) &&
      *(int *)((char *)widget + 0x40) != 0) {
    if (*(uint16_t *)((char *)widget + 0x44) != 0) {
      entry = *(void **)(*(char **)((char *)widget + 0x40) + (int)selected * 4);
      if (*(unsigned char *)((char *)entry + 0xe0) == 1) {
        if (*(short *)((char *)entry + 0xde) == 0) {
          ((uint32_t *)address)[0] = 0;
          ((uint32_t *)address)[1] = 0;
          ((uint32_t *)address)[2] = 0;
          ((uint32_t *)address)[3] = 0;
          ((uint32_t *)address)[4] = 0;
          ((uint32_t *)address)[5] = 0;
          FUN_00082bd0((char *)entry + 0x18,
                       (const uint32_t *)((char *)entry + 8),
                       (const uint32_t *)entry, 0x141e, (uint32_t *)address);
          if (((uint32_t *)address)[0] != 0 &&
              *(uint16_t *)(address + 0x12) != 0) {
            *(uint16_t *)(join_params + 2) = 0;
            network_game_generate_join_game_token(join_params + 0x12);
            if (network_game_client_initiate_join_game(
                  global_network_game_client_get(), entry, join_params,
                  address)) {
              last_child = widget_instance_get_topmost_parent(widget);
              player_index_ptr = *(int **)((char *)widget + 0x30);
              if (player_index_ptr != NULL) {
                player_index = *player_index_ptr;
              } else {
                player_index = -1;
              }
              child_index = widget_instance_get_child_index_from_parent(widget);
              spawned = ui_widget_load_by_name_or_tag(
                "ui\\shell\\main_menu\\multiplayer_type_"
                "select\\connected\\pregame\\"
                "connected_pregame_screen",
                -1, 0, -1, *(int *)last_child, player_index, child_index);
              if (spawned == NULL) {
                error(2, "event handler failed to spawn widget");
                *widget_deleted = true;
                goto done;
              }

              set_game_connection(1);
              result = true;
              *widget_deleted = true;
            } else {
              network_game_abort();
              error(2, "failed to initiate join game procedures");
            }
          } else {
            error(2, "attempted to join a network game with a bogus address");
          }

        } else {
          error(2, "attempted to join a network game running on a different "
                   "platform than the local system");
        }
      } else {
        error(2, "attempted to join a closed game");
        ui_play_audio_feedback_sound(4);
      }
    } else {
      error(2, "unable to join server: there are no servers in the server list "
               "(maybe the server list was disposed?)");
    }
  } else {
    error(2, "unable to join server: this doesn't look like a valid server "
             "list to me... or the server list has been disposed?");
  }
done:
  return result;
}

/* dispose net game server list (event handler table index 18, 0x0e9fd0) —
 * drops the widget's cached list pointer/count at +0x40/+0x44. */
bool network_server_list_dispose(void *widget, void *event_data,
                                 bool *widget_deleted)
{
  *(int *)((char *)widget + 0x40) = 0;
  *(int16_t *)((char *)widget + 0x44) = 0;
  return true;
}

/* start split-screen game networking (0x0ea010, single data xref at
 * 0x31e1ac) — counterpart to network_game_start_new_server's "failed to
 * initiate a multiplayer game server" path, but for split screen: disallows
 * remote connections, then if no network game server exists yet, spins one up
 * via the game engine playlist and switches the connection to server
 * mode (2); bails out immediately on playlist-begin failure without
 * ever probing the client. If a server already existed (or was just
 * created), then checks for an existing client and creates one if
 * needed. On any failure, tears down both client and server, clears
 * the multiplayer variant, and reports the error. Returns true on
 * success. */
bool split_screen_game_initialize(void)
{
  void *server;
  void *client;
  bool result;

  network_game_set_accept_remote_connections(0);
  server = global_network_game_server_get();
  if (server == NULL) {
    game_engine_playlist_initialize();
    result = create_global_network_game_server();
    if (!result) {
      goto fail;
    }
    game_engine_playlist_begin();
    set_game_connection(2);
  }
  result = true;
  client = global_network_game_client_get();
  if (client == NULL) {
    result = create_global_network_game_client();
  }
  if (result) {
    return result;
  }

fail:
  dispose_global_network_game_server();
  dispose_global_network_game_client();
  player_ui_clear_multiplayer_variant();
  error(2, "failed to initiate split screen game networking");
  return result;
}

/* mp level list initialize (event handler table entry at data 0x31e1c0,
 * 0x0ea100) — validates that `widget` itself is a spinner-list tag with
 * exactly 3 items ("multiplayer level list", same assert-file/line
 * pattern as the profile-list siblings above), points its list
 * pointer/count at the built-in level_name_table (13 entries) at
 * +0x40/+0x44 — the inverse of multiplayer_level_list_dispose
 * (0x0ea1f0) below, which clears the same two fields — then, if there is
 * a remembered last-used multiplayer map, linearly scans the table for a
 * case-insensitive name match and leaves the matching index selected at
 * +0x3c (reset to 0 if no match is found; left untouched if no map was
 * remembered). Always returns true. */
bool multiplayer_level_list_initialize(void *widget, void *event_data,
                                       bool *widget_deleted)
{
  short *list_tag;
  char saved_map_name[256];
  int16_t index;

  (void)event_data;
  (void)widget_deleted;

  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'multiplayer level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x4cc,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'multiplayer level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x4cd,
      1);
    system_exit(-1);
  }

  *(int *)((char *)widget + 0x40) =
    0x31e4c8; /* level_name_table (DAT_0031e4c8), 13 entries */
  *(int16_t *)((char *)widget + 0x44) = 13;

  if (saved_game_file_retrieve_last_used_multiplayer_map(saved_map_name)) {
    *(int16_t *)((char *)widget + 0x3c) = 0;
    do {
      index = *(int16_t *)((char *)widget + 0x3c);
      if (crt_stricmp(saved_map_name, ((char **)0x31e4c8)[index]) == 0) {
        break;
      }
      *(int16_t *)((char *)widget + 0x3c) = (int16_t)(index + 1);
    } while (*(int16_t *)((char *)widget + 0x3c) < 13);

    if (*(int16_t *)((char *)widget + 0x3c) == 13) {
      *(int16_t *)((char *)widget + 0x3c) = 0;
    }
  }

  return true;
}

/* mp level list dispose (event handler table index 27, 0x0ea1f0) — drops the
 * widget's cached list pointer/count at +0x40/+0x44. */
bool multiplayer_level_list_dispose(void *widget, void *event_data,
                                    bool *widget_deleted)
{
  *(int *)((char *)widget + 0x40) = 0;
  *(int16_t *)((char *)widget + 0x44) = 0;
  return true;
}

/* multiplayer level select (event handler, 0x0ea210) — fired when the user
 * accepts a level on the multiplayer level select screen.  Asserts the
 * widget chain: `widget` is a wrapper tag (child count +0x3e0 == 1), its
 * child at +0x34 is a container tag (type 0) with 3 children, and that
 * container's child at +0x34 is a spinner list tag (type 2) with 3 list
 * items.  Reads the list widget's selected index at +0x3c, bounds-checks
 * it against the 13-entry level_name_table at 0x31e4c8, and takes that
 * entry's name.  A debug override file "d:\map_automation.txt", when it
 * opens, replaces the name with its first whitespace-delimited token.  The
 * name is then pushed to main, to the game engine map-name override, and
 * to the network server when one exists; finally the table is scanned
 * case-insensitively and the matching entry is remembered as the last used
 * multiplayer map.  Always returns true. */
bool ui_widget_multiplayer_level_select(void *widget, void *event_data,
                                        bool *widget_deleted)
{
  void *wrapper_tag;
  short *screen_tag;
  void *screen_widget;
  void *list_widget;
  short *list_tag;
  int16_t selected_index;
  char *map_name;
  void *file;
  void *server;
  int index;
  char line[64];

  (void)event_data;
  (void)widget_deleted;

  wrapper_tag = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*(int *)((char *)wrapper_tag + 0x3e0) != 1) {
    display_assert(
      "expected a wrapper widget around the multiplayer level select screen",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x500,
      1);
    system_exit(-1);
  }

  screen_widget = *(void **)((char *)widget + 0x34);
  screen_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)screen_widget);
  if (*screen_tag != 0 || *(int *)((char *)screen_tag + 0x3e0) != 3) {
    display_assert(
      "expected the multiplayer level select screen to be a container w/ 3 "
      "children",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x505,
      1);
    system_exit(-1);
  }

  list_widget = *(void **)((char *)screen_widget + 0x34);
  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)list_widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'multiplayer level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x508,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'multiplayer level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x509,
      1);
    system_exit(-1);
  }

  selected_index =
    *(int16_t *)((char *)*(void **)((char *)*(void **)((char *)widget + 0x34) +
                                    0x34) +
                 0x3c);
  if (selected_index < 0 || selected_index >= 13) {
    display_assert(
      "invalid multiplayer level specified from 'multiplayer level list' list "
      "widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x512,
      1);
    system_exit(-1);
  }

  map_name = ((char **)0x31e4c8)[selected_index];

  file = crt_fopen("d:\\map_automation.txt", "r");
  if (file != NULL) {
    crt_fgets(line, 0x40, file);
    line[63] = '\0';
    csstrtok(line, "\n\r \t");
    map_name = line;
    crt_fclose(file);
  }

  main_set_multiplayer_map_name(map_name);
  game_engine_override_map_name(map_name);

  server = global_network_game_server_get();
  if (server != NULL) {
    network_game_server_change_map_name((int)server, map_name);
  }

  index = 0;
  do {
    if (crt_stricmp(map_name, ((char **)0x31e4c8)[index]) == 0) {
      saved_game_file_remember_last_used_multiplayer_map(
        ((char **)0x31e4c8)[index]);
      return true;
    }
    index++;
  } while (index < 13);

  return true;
}

/* multiplayer profiles list initialize (event handler, 0x0ea3e0) — builds
 * the "multiplayer settings list" (game-variant profile) list owned by
 * `widget`.  Clears the pending profile handle (DAT_0031e494) and the
 * 0x144-byte profile scratch block at DAT_005aa260 to -1, asserts that
 * `widget` is a spinner-list tag ('DeLa' type 2) with exactly 3 list
 * items, then (re)allocates a 400-byte / 100-entry handle buffer at
 * widget+0x40 through ui_widget_realloc.  FUN_001c26b0 fills the buffer
 * and writes back the number of entries found (capacity passed in as
 * 100); any shortfall below 3 entries is padded with -1 so the list always
 * has at least 3 rows.  The final count lands at widget+0x44.  Finally, if
 * a last-used multiplayer variant directory is remembered and resolves to
 * a profile index, the buffer is scanned linearly and the matching row is
 * left selected at widget+0x3c (left untouched when there is no match).
 * A failed allocation skips everything after the store.  event_data and
 * widget_deleted are unused; always returns true. */
bool ui_widget_multiplayer_profiles_list_initialize(void *widget,
                                                    void *event_data,
                                                    bool *widget_deleted)
{
  short *list_tag;
  int *items;
  int count;
  int profile_index;
  unsigned short i;
  char variant_directory[256];

  (void)event_data;
  (void)widget_deleted;

  *(int *)0x31e494 = -1; /* DAT_0031e494 — pending profile handle */
  csmemset((void *)0x5aa260, -1,
           0x144); /* DAT_005aa260 — profile scratch block */

  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'multiplayer settings list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x568,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'multiplayer settings list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x569,
      1);
    system_exit(-1);
  }

  items = (int *)ui_widget_realloc(
    *(int *)((char *)widget + 0x40), 400,
    "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x56e);
  *(int **)((char *)widget + 0x40) = items;
  if (items == NULL) {
    return true;
  }

  count = 100;
  FUN_001c26b0(0, &count, items);

  while ((unsigned short)count < 3) {
    items[count] = -1;
    count++;
  }
  *(int16_t *)((char *)widget + 0x44) = (int16_t)count;

  if (!saved_game_file_retrieve_last_used_multiplayer_variant_directory(
        variant_directory)) {
    return true;
  }

  profile_index =
    saved_game_file_find_profile_index_for_directory_path(variant_directory, 1);
  if (profile_index == -1) {
    return true;
  }

  for (i = 0; i < (unsigned short)count; i++) {
    if (items[i] == profile_index) {
      *(int16_t *)((char *)widget + 0x3c) = (int16_t)i;
      break;
    }
  }

  return true;
}

/* dispose owned list (event handler, 0x0ea540; single data xref at
 * 0x31e1d0) — if the widget's cached list pointer at +0x40 is non-NULL,
 * frees it via widget_free and clears the pointer; always clears the
 * 16-bit count at +0x44. Unlike the static-table list-dispose siblings
 * above (mp level list dispose, sp level list dispose, dispose net game
 * server list, ...), this variant owns and frees its buffer. event_data
 * and widget_deleted are unused. Always returns true. */
bool multiplayer_profiles_list_dispose(void *widget, void *event_data,
                                       bool *widget_deleted)
{
  void *list_ptr;

  (void)event_data;
  (void)widget_deleted;

  list_ptr = *(void **)((char *)widget + 0x40);
  if (list_ptr != NULL) {
    widget_free(list_ptr);
    *(void **)((char *)widget + 0x40) = NULL;
  }
  *(int16_t *)((char *)widget + 0x44) = 0;
  return true;
}

/* swap teams (event handler, 0x0ea810) — asserts event_data is non-NULL,
 * then, if a network game exists with teams enabled (+0xc0 == 1, same flag
 * multiplayer_game_set_text_box_for_teams_noteams reads) and this client has
 * a valid local machine index (network_game_client_get_local_machine_index),
 * walks the game's 16-slot player table (index_base+0x226, stride 0x20 —
 * same table netgame_join_player and multiplayer_profiles_list_dispose's
 * neighbor walk) for a valid record (network_player_is_valid) whose
 * machine-index byte (record+0x1c) matches the local machine index and
 * whose controller-index byte (record+0x1d) matches the event's
 * controller_index (event_data+2, same field netgame_join_player reads).
 * On the first match it copies the 0x20-byte record to a local buffer,
 * flips the byte at record offset 0x1e to its logical complement (0/1
 * toggle — the team field), and pushes the updated record via
 * network_game_client_update_local_player_data(global_network_game_client_get(),
 * &local_record), logging "failed to update player's team for multiplayer
 * game" via error(2, ...) on failure, then stops walking the table. widget
 * and widget_deleted are unused; always returns true. */
bool multiplayer_game_swap_teams(void *widget, void *event_data,
                                 bool *widget_deleted)
{
  int index_base;
  short local_machine_index;
  int i;
  char *player_slot;
  uint32_t local_record[8];
  bool updated;

  (void)widget;
  (void)widget_deleted;

  if (event_data == NULL) {
    display_assert(
      "event",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x624,
      true);
    system_exit(-1);
  }

  index_base = network_game_get_game();
  if (index_base != 0 && *(char *)(index_base + 0xc0) == 1) {
    local_machine_index = network_game_client_get_local_machine_index();
    if (local_machine_index != -1) {
      player_slot = (char *)index_base + 0x226;
      for (i = 0; i < 0x10; i++, player_slot += 0x20) {
        if (network_player_is_valid(player_slot) &&
            *(player_slot + 0x1c) == local_machine_index &&
            *(player_slot + 0x1d) == *(int16_t *)((char *)event_data + 2)) {
          memcpy(local_record, player_slot, sizeof(local_record));
          ((char *)local_record)[0x1e] = (((char *)local_record)[0x1e] == 0);
          updated = network_game_client_update_local_player_data(
            global_network_game_client_get(), local_record);
          if (!updated) {
            error(2, "failed to update player's team for multiplayer game");
          }
          break;
        }
      }
    }
  }

  return true;
}

/* join network game (event handler, 0x0ea900) — asserts event_data is
 * non-NULL, then, if a network game client exists and its state
 * (network_game_client_get_state) is 2, walks the client's 16-slot player
 * table (index_base from network_game_get_game, records at index_base+0x226,
 * stride 0x20 — same table network_game_client_local_player_quit walks) looking
 * for a valid record whose machine-index byte (+0x242, relative to
 * index_base+i*0x20) matches this client's local machine index
 * (network_game_client_get_local_machine_index) and whose controller-index
 * byte (+0x243) matches the event's controller_index (event_data+2, same
 * field event_controller_index_compatible_with_widget reads above) — if
 * found, the player is already present and the function returns
 * immediately. Otherwise it asks the client to add the player via
 * network_game_client_add_player(client, controller_index), logging
 * "failed to send join request" via network_event on failure. Always
 * returns true; widget and widget_deleted are unused. */
bool netgame_join_player(void *widget, void *event_data, bool *widget_deleted)
{
  void *client;
  int16_t state;
  int state_out;
  int index_base;
  short local_machine_index;
  short i;
  char *player_slot;
  char *record;
  int16_t controller_index;
  bool added;

  (void)widget;
  (void)widget_deleted;

  if (event_data == NULL) {
    display_assert(
      "event",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x652,
      true);
    system_exit(-1);
  }

  client = global_network_game_client_get();
  if (client != NULL) {
    state = network_game_client_get_state(client, &state_out);
    if (state == 2) {
      index_base = network_game_get_game();
      local_machine_index = network_game_client_get_local_machine_index();

      if (index_base == 0) {
        display_assert(
          "game",
          "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
          0x65b, true);
        system_exit(-1);
      }

      controller_index = *(int16_t *)((char *)event_data + 2);

      if (local_machine_index != -1) {
        for (i = 0; i < 0x10; i++) {
          player_slot = (char *)index_base + 0x226 + i * 0x20;
          if (network_player_is_valid(player_slot)) {
            record = (char *)index_base + i * 0x20;
            if (*(record + 0x242) == local_machine_index &&
                *(record + 0x243) == controller_index) {
              return true;
            }
          }
        }
      }

      added =
        network_game_client_add_player(client, (uint16_t)controller_index);
      if (!added) {
        network_event("failed to send join request");
      }
    }
  }

  return true;
}

/* dispose owned list, duplicate table entry (event handler, 0x0eab70; data
 * xref at 0x31e1e4, 0x14 bytes after multiplayer_profiles_list_dispose's
 * 0x31e1d0 entry) — byte-identical body to multiplayer_profiles_list_dispose
 * above: if the widget's cached list pointer at +0x40 is non-NULL, frees it via
 * widget_free and clears the pointer; always clears the 16-bit count at +0x44.
 * event_data and widget_deleted are unused. Always returns true. */
bool player_profiles_list_dispose(void *widget, void *event_data,
                                  bool *widget_deleted)
{
  void *list_ptr;

  (void)event_data;
  (void)widget_deleted;

  list_ptr = *(void **)((char *)widget + 0x40);
  if (list_ptr != NULL) {
    widget_free(list_ptr);
    *(void **)((char *)widget + 0x40) = NULL;
  }
  *(int16_t *)((char *)widget + 0x44) = 0;
  return true;
}

/* player_profile_set_for_game_3wide (0xeaba0) — event-handler table entry
 * at 0x31e1e8.  Validates the profile-selection container and its spinner-list
 * child, then applies the selected profile result. */
bool player_profile_set_for_game_3wide(void *widget, void *event_data,
                                       bool *widget_deleted)
{
  wchar_t profile[24];
  int profile_index;
  int local_player_index;
  short *widget_definition;
  void *list_widget;
  short selected_index;

  if (event_data == NULL || *(int16_t *)((char *)event_data + 2) == -1) {
    display_assert(
      "setting a player profile requires a valid controller index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x6e3,
      true);
    system_exit(-1);
  }

  widget_definition = (short *)tag_get(0x44654c61, *(int *)widget);
  if (*widget_definition != 0 ||
      *(int *)((char *)widget_definition + 0x3e0) < 3) {
    display_assert(
      "expected the player profile select screen to be a container w/ 3 or "
      "more children",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x6ec,
      true);
    system_exit(-1);
  }

  list_widget = *(void **)((char *)widget + 0x34);
  widget_definition = (short *)tag_get(0x44654c61, *(int *)list_widget);
  if (*widget_definition != 2) {
    display_assert(
      "expected a spinner list widget for 'player profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x6ef,
      true);
    system_exit(-1);
  }
  if (*(int *)((char *)widget_definition + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'player profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x6f0,
      true);
    system_exit(-1);
  }

  selected_index = *(int16_t *)((char *)list_widget + 0x3c);
  if (selected_index < 0 ||
      (int)selected_index >= (int)*(uint16_t *)((char *)list_widget + 0x44)) {
    display_assert(
      "invalid multiplayer profile specified from 'player profile list' list "
      "widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x6f8,
      true);
    system_exit(-1);
  }

  profile_index =
    *(int *)(*(int *)((char *)list_widget + 0x40) + selected_index * 4);
  if (profile_index != -1) {
    if (profile_index >= 0) {
      display_error_deferred(0x1f, -1, true, false);
      ui_play_audio_feedback_sound(4);
      *widget_deleted = true;
      return false;
    }

    if (player_profile_new(profile_index, profile)) {
      local_player_index =
        player_ui_get_single_player_local_player_from_controller(
          *(int16_t *)((char *)event_data + 2));
      player_ui_set_active_player_profile(
        (short)local_player_index,
        *(int *)(*(int *)((char *)list_widget + 0x40) + selected_index * 4),
        profile);
      return true;
    }

    error(2, "failed to retrieve user selected player profile");
    return false;
  }

  error(2, "this is not a selectable player profile");
  ui_play_audio_feedback_sound(4);
  return false;
}

/* player_profile_set_for_game_1wide (0xead60) — event-handler table entry.
 * Same profile-selection idea as player_profile_set_for_game_3wide (0xeaba0)
 * above but for a single spinner list, found by walking the widget's child
 * chain (+0x34, sibling link +0x2c) for the first child of type 2 (spinner
 * list) instead of using a fixed container-of-3 layout, and there is no
 * "container w/ 3 or more children" assert at all (disasm has no such check
 * here). The code-generated-list assert also differs: it requires the tag's
 * +0x3e0 field to be exactly 0, not >= 3.
 *
 * Unlike the 3-wide sibling, this handler never calls
 * player_ui_get_single_player_local_player_from_controller: the raw
 * controller index read once from event_data+2 is reused directly as the
 * local_player_index argument to both display_error_deferred and
 * player_ui_set_active_player_profile (disasm: MOV BX,[ESI+2] once into EBX,
 * then PUSH EBX unmodified at both call sites further down — safe because
 * both callees only read a 16-bit slice of that pushed dword: an int16_t
 * param and int, respectively, at MSVC).
 *
 * Control flow is also flatter than the 3-wide sibling: there is a single
 * `if (profile_index >= 0) {...} else {...}` (JS on the sign bit), not a
 * three-way -1 vs. <-1 vs. >=0 split — so there is no separate "this is not
 * a selectable player profile" branch here, and widget_deleted is never
 * read or written anywhere in this function (unused param). */
bool player_profile_set_for_game_1wide(void *widget, void *event_data,
                                       bool *widget_deleted)
{
  wchar_t profile[24];
  int profile_index;
  int16_t controller_index;
  short selected_index;
  short *widget_definition;
  void *list_widget;

  (void)widget_deleted;

  if (event_data == NULL || *(int16_t *)((char *)event_data + 2) == -1) {
    display_assert(
      "setting a player profile requires a valid controller index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x72a,
      true);
    system_exit(-1);
  }

  controller_index = *(int16_t *)((char *)event_data + 2);

  list_widget = *(void **)((char *)widget + 0x34);
  while (list_widget != NULL && *(int16_t *)((char *)list_widget + 0xe) != 2) {
    list_widget = *(void **)((char *)list_widget + 0x2c);
  }
  if (list_widget == NULL) {
    display_assert(
      "failed to find the 1-wide spinner list for player profiles (expected "
      "it to be a child of this widget)",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x72e,
      true);
    system_exit(-1);
  }

  widget_definition = (short *)tag_get(0x44654c61, *(int *)list_widget);
  if (*(int *)((char *)widget_definition + 0x3e0) != 0) {
    display_assert(
      "expected a code-generated 1-wide spinner list for 'mp player profile "
      "list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x735,
      true);
    system_exit(-1);
  }

  selected_index = *(int16_t *)((char *)list_widget + 0x3c);
  if (selected_index < 0 ||
      (int)selected_index >= (int)*(uint16_t *)((char *)list_widget + 0x44)) {
    display_assert(
      "invalid multiplayer profile specified from 'mp player profile list' "
      "list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x73b,
      true);
    system_exit(-1);
  }

  profile_index =
    *(int *)(*(int *)((char *)list_widget + 0x40) + selected_index * 4);

  if (profile_index >= 0) {
    display_error_deferred(0x1f, controller_index, true, false);
    ui_play_audio_feedback_sound(4);
    return false;
  }

  if (player_profile_new(profile_index, profile)) {
    player_ui_set_active_player_profile(
      (short)controller_index,
      *(int *)(*(int *)((char *)list_widget + 0x40) + selected_index * 4),
      profile);
    return true;
  }

  error(2, "failed to retrieve user selected player profile");
  return false;
}

/* playlist_profile_begin_editing (0xeaec0) — event-handler table entry
 * (data xref 0x31e1f0). Validates 'widget' itself is a container with 3+
 * children (tag_get on *(int *)widget, same container check shape as
 * delete_player_profile_request), then the child list widget at widget+0x34
 * is a 3-item spinner list, resolves the selected item's profile handle
 * from that list, stores it to DAT_0031e494, and dispatches on it: -1 plays
 * the deny sound and returns false; a negative-but-not-(-1) handle begins
 * editing that profile (player_ui_begin_editing_profile) and returns true;
 * otherwise (>= 0) reports a deferred error and plays the deny sound,
 * returning false. Same three-way -1/<0/>=0 split and callee set as
 * player_profile_begin_editing (0xeed10), but with the container-of-3
 * tag_get check up front (like delete_player_profile_request) instead of
 * that sibling's simple container-flag check, and DAT_0031e494 is cleared
 * to -1 before the container check runs, not after (disasm: the MOV to
 * 0x31e494 is scheduled ahead of the first CALL tag_get). event_data and
 * widget_deleted are unused, same 3-arg handler-table shape as siblings. */
bool playlist_profile_begin_editing(void *widget, void *event_data,
                                    bool *widget_deleted)
{
  short *container_tag;
  int *list_widget;
  short *list_tag;
  short list_index;
  int profile_handle;
  int widget_tag_id;
  bool result;

  (void)event_data;
  (void)widget_deleted;

  result = false;

  widget_tag_id = *(int *)widget;
  *(int *)0x31e494 = -1; /* DAT_0031e494 — unknown purpose */

  container_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, widget_tag_id);
  if (*container_tag != 0 || *(int *)((char *)container_tag + 0x3e0) < 3) {
    display_assert(
      "expected the multiplayer profile select screen to be a container w/ "
      "3+ children",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x768,
      1);
    system_exit(-1);
  }

  list_widget = *(int **)((char *)widget + 0x34);
  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)list_widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'multiplayer profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x76b,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'multiplayer profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x76c,
      1);
    system_exit(-1);
  }

  list_widget = *(int **)((char *)widget + 0x34);
  list_index = *(short *)((char *)list_widget + 0x3c);
  if (list_index < 0 ||
      (int)list_index >= (int)*(unsigned short *)((char *)list_widget + 0x44)) {
    display_assert(
      "invalid multiplayer profile specified from 'multiplayer profile "
      "list' list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x775,
      1);
    system_exit(-1);
  }

  profile_handle = (*(int **)((char *)list_widget +
                              0x40))[*(short *)((char *)list_widget + 0x3c)];

  if (profile_handle == -1) {
    ui_play_audio_feedback_sound(4);
    goto exit;
  }

  if (profile_handle < 0) {
    player_ui_begin_editing_profile(profile_handle);
    result = true;
    goto exit;
  }

  display_error_deferred(0x1f, -1, true, false);
  ui_play_audio_feedback_sound(4);

exit:
  return result;
}

/* apply selected game engine item (event handler, data xref 0x31e1f8,
 * 0x14 bytes after player_profiles_list_dispose's 0x31e1e4 entry, same
 * ui_widget_event_handler_fn pointer array as the select_game_engine_item
 * table entry at 0x31e220) — 0xeb020. Runs the inverse of
 * playlist_profile_initialize_game_engine's (0xecd50) profile-to-widget
 * table: fetches the in-progress playlist-profile edit copy
 * (player_ui_get_edit_playlist_profile, called unconditionally first,
 * before the parent-widget check — order preserved), then asserts the
 * widget's PARENT (+0x30, not widget itself) is a column-list widget
 * (+0xe == 3), same "expected column list" display_assert/system_exit(-1)
 * shape as the sibling handlers.
 *
 * If no playlist profile is being edited, logs error(2, "failed to
 * retrieve editable game variant") and returns false.
 *
 * Otherwise remaps the parent's selected-index field (+0x3c, sign-extended
 * per the original's MOVSX) through the table 0 -> 1, 1 -> 4, 2 -> 2,
 * 3 -> 3, 4 -> 5 (exactly the inverse of select_game_engine_item's
 * default -> 0, 2 -> 2, 3 -> 3, 4 -> 1, 5 -> 4 pairing) into a local. Any
 * other value logs error(2, "unknown game engine option selected") and
 * falls back to the profile's current value at +0x18 (a self-comparison
 * no-op, preserved verbatim from the disassembly's MOV ESI,[EDI+0x18]
 * reload on the default arm). If the remapped value differs from the
 * profile's current dword field at +0x18 (unproven — pointed-to type of
 * the profile is void* upstream, same offset select_game_engine_item
 * reads), clears 0x18 bytes at profile+0x4c (csmemset) before storing the
 * new value into profile+0x18. Always returns true on the profile-found
 * path; event_data and widget_deleted are unused, same "3-arg handler
 * typedef pushed by the dispatcher regardless" shape noted at
 * select_game_engine_item. */
bool playlist_profile_set_game_engine(void *widget, void *event_data,
                                      bool *widget_deleted)
{
  void *profile;
  void *parent;
  int16_t selected;
  int new_value;

  (void)event_data;
  (void)widget_deleted;

  profile = player_ui_get_edit_playlist_profile();
  parent = *(void **)((char *)widget + 0x30);

  if (parent == NULL || *(int16_t *)((char *)parent + 0xe) != 3) {
    display_assert(
      "expected column list for game engine type list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x7a6,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    selected = *(int16_t *)((char *)parent + 0x3c);
    switch (selected) {
    case 0:
      new_value = 1;
      break;
    case 1:
      new_value = 4;
      break;
    case 2:
      new_value = 2;
      break;
    case 3:
      new_value = 3;
      break;
    case 4:
      new_value = 5;
      break;
    default:
      error(2, "unknown game engine option selected");
      new_value = *(int *)((char *)profile + 0x18);
      break;
    }

    if (new_value != *(int *)((char *)profile + 0x18)) {
      csmemset((char *)profile + 0x4c, 0, 0x18);
    }
    *(int *)((char *)profile + 0x18) = new_value;

    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* apply multiplayer radar/friends display options (event handler, data
 * xref 0x31e21c in the same ui_widget_event_handler_fn pointer array as
 * ui_widget_game_data_select_game_engine_item at 0x31e220) — 0xecb60.
 * Runs the widget-to-profile direction: fetches the in-progress
 * playlist-profile edit copy (player_ui_get_edit_playlist_profile,
 * called unconditionally first, before the widget is even loaded —
 * order preserved, and here the NULL test precedes the asserts, per the
 * TEST EBX,EBX / JZ at 0xecb6b before [EBP+8] is read at 0xecb73), then
 * walks three consecutive list items under the widget's first child
 * (+0x34), each followed via the sibling link (+0x2c).
 *
 * For each list item the handler scans that item's own child chain
 * (+0x34, following +0x2c) for the first widget whose type field
 * (+0xe) is 2 — the option-spinner list — and reads its selected-index
 * field (+0x3c) sign-extended (MOVSX, matching the sibling handlers'
 * selected-item slot). Every missing item or missing spinner trips the
 * same display_assert/system_exit(-1) shape as the sibling handlers,
 * at source lines 0xa80/0xa82, 0xa8c/0xa8e and 0xa97/0xa99.
 *
 * Item 1 ('radar display') writes the selected index straight through
 * to the profile's dword field at +0x24 (0, 1 or 2). Item 2 ('other
 * players on radar') sets (index 0) or clears (index 1) bit 0 of the
 * profile's flag dword at +0x20. Item 3 ('friends on screen') sets
 * (index 0) or clears (index 1) bit 1 of that same flag dword. Both
 * profile offsets are unproven field meanings — the profile is void*
 * at player_ui_get_edit_playlist_profile's kb decl, the same upstream
 * untyped producer the sibling handlers read +0x18 through.
 *
 * An out-of-range index on any of the three logs error(2, ...) with
 * that item's own message and leaves the corresponding profile field
 * untouched (the second block's common store at 0xecc98 is skipped
 * entirely on the default arm). Returns true whenever a profile was
 * retrieved, false after error(2, "failed to retrieve editable game
 * variant") when none is being edited. event_data/widget_deleted are
 * not read here; only [EBP+8] is touched, so only the widget parameter
 * is declared, same as the single-param sibling
 * ui_widget_game_data_select_game_engine_item. */
bool FUN_000ecb60(void *widget)
{
  void *profile;
  void *item;
  void *spinner;
  int16_t selected;

  profile = player_ui_get_edit_playlist_profile();

  if (profile != NULL) {
    item = *(void **)((char *)widget + 0x34);
    if (item == NULL) {
      display_assert(
        "expected 'radar display' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa80, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'radar display' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa82, true);
      system_exit(-1);
    }

    selected = *(int16_t *)((char *)spinner + 0x3c);
    switch (selected) {
    case 0:
      *(int *)((char *)profile + 0x24) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x24) = 1;
      break;
    case 2:
      *(int *)((char *)profile + 0x24) = 2;
      break;
    default:
      error(2,
            "unknown option selected in 'radar display' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'other players on radar' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa8c, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'other players on radar' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa8e, true);
      system_exit(-1);
    }

    selected = *(int16_t *)((char *)spinner + 0x3c);
    switch (selected) {
    case 0:
      *(uint32_t *)((char *)profile + 0x20) =
        *(uint32_t *)((char *)profile + 0x20) | 1;
      break;
    case 1:
      *(uint32_t *)((char *)profile + 0x20) =
        *(uint32_t *)((char *)profile + 0x20) & 0xfffffffe;
      break;
    default:
      error(2, "unknown option selected in 'other players on radar' option "
               "spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'friends on screen' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa97, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'friends on screen' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa99, true);
      system_exit(-1);
    }

    selected = *(int16_t *)((char *)spinner + 0x3c);
    switch (selected) {
    case 0:
      *(uint32_t *)((char *)profile + 0x20) =
        *(uint32_t *)((char *)profile + 0x20) | 2;
      break;
    case 1:
      *(uint32_t *)((char *)profile + 0x20) =
        *(uint32_t *)((char *)profile + 0x20) & 0xfffffffd;
      break;
    default:
      error(
        2,
        "unknown option selected in 'friends on screen' option spinner list");
      break;
    }

    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* select game engine item (event handler table index 50, data xref
 * 0x31e220 in the same ui_widget_event_handler_fn pointer array
 * ui_widget_event_handler_function_invoke indexes at 0x31e158; single-param
 * shape matches difficulty_menu_initialize (0xf0640,
 * table index 99) — the 3-arg handler typedef is pushed by the
 * dispatcher regardless, event_data/widget_deleted are simply not read
 * here) — 0xecd50. Fetches the in-progress playlist-profile edit copy
 * (player_ui_get_edit_playlist_profile, called unconditionally first,
 * before widget is even loaded — order preserved) then asserts widget
 * is a column-list widget (+0xe == 3, same "expected a column list"
 * display_assert/system_exit(-1) shape as the difficulty-item sibling).
 *
 * If no playlist profile is being edited, logs error(2, "failed to
 * retrieve editable game variant") and returns false.
 *
 * Otherwise remaps the profile's dword field at +0x18 (unproven —
 * pointed-to type of the profile is void* upstream, offset falls in
 * game_variant_t's un-split unk_2[] padding) through a fixed table
 * into the widget's selected-index field (+0x3c, the same "selected
 * list item" slot difficulty_menu_initialize uses):
 * profile field 1 (and anything outside [1,5], unsigned) -> 0, 2 -> 2,
 * 3 -> 3, 4 -> 1, 5 -> 4. This exact case/value pairing is Ghidra's
 * resolved jump-table decode (0xecd95 JMP [EAX*4+0xecdf0]) and is not
 * re-derivable from the visible disassembly text alone, so the mapping
 * is taken verbatim from the decompiler's switch rather than assumed
 * sequential.
 *
 * Reloads the just-stored selected index from +0x3c (sign-extended,
 * matching the original's MOVSX reload instead of reusing a cached
 * value) to call widget_instance_get_nth_child(widget, index), and
 * stores the resulting child pointer at the selected-child field
 * (+0x38, paired with +0x3c the same way across this widget family).
 * Always returns true on the profile-found path. */
bool playlist_profile_initialize_game_engine(void *widget)
{
  void *profile;
  void *child;

  profile = player_ui_get_edit_playlist_profile();

  if (*(int16_t *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected a column list for the list of available game engines",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xab2,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    switch (*(int *)((char *)profile + 0x18)) {
    default:
      *(int16_t *)((char *)widget + 0x3c) = 0;
      break;
    case 2:
      *(int16_t *)((char *)widget + 0x3c) = 2;
      break;
    case 3:
      *(int16_t *)((char *)widget + 0x3c) = 3;
      break;
    case 4:
      *(int16_t *)((char *)widget + 0x3c) = 1;
      break;
    case 5:
      *(int16_t *)((char *)widget + 0x3c) = 4;
      break;
    }

    child = widget_instance_get_nth_child(widget,
                                          *(int16_t *)((char *)widget + 0x3c));
    *(void **)((char *)widget + 0x38) = child;

    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* multiplayer profile init name (event handler, data xref 0x31e224,
 * same ui_widget_event_handler_fn pointer array as the game-engine-item
 * handlers above) — 0xece10. Fetches the in-progress playlist-profile
 * edit copy (player_ui_get_edit_playlist_profile, called unconditionally
 * first, before the widget-type check — order preserved per
 * disassembly), then asserts widget itself (not a parent) is a text box
 * widget (+0xe == 1), same "expected text box widget for profile name"
 * display_assert/system_exit(-1) shape as the sibling handlers, and the
 * same +0xe==1 text-box check widget_instance_render_text_box's siblings use.
 *
 * If a profile is being edited, (re)allocates a 0x100-byte name buffer
 * through ui_widget_realloc (same stack_memory_pool_realloc wrapper and
 * +0x3c buffer-pointer slot widget_instance_render_text_box above uses),
 * passing the widget's existing +0x3c buffer pointer as the realloc input. The
 * result is stored back to +0x3c unconditionally right after the call
 * (MOV before the NULL-test JZ in the disassembly, order preserved).
 * On successful allocation, copies up to 0x7f wide characters from the
 * profile pointer itself (not an offset field — the profile struct's
 * name is its first member, per the disassembly passing EDI, the raw
 * profile pointer, as ustrncpy's source with no added offset) into the
 * new buffer via ustrncpy, then null-terminates at wchar_t index 0x7f
 * (byte offset 0xfe) by reloading the buffer pointer from +0x3c rather
 * than reusing the local (matches the disassembly's MOV ECX,[ESI+0x3c]
 * reload). Always returns true on the profile-found path.
 *
 * If no playlist profile is being edited, logs error(2, "failed to
 * retrieve editable game variant") (same message/severity as the
 * sibling handlers) and returns false; event_data and widget_deleted are
 * unused, same 3-arg handler typedef shape as the sibling handlers
 * above. */
bool playlist_profile_initialize_name(void *widget, void *event_data,
                                      bool *widget_deleted)
{
  void *profile;
  void *name_buffer;

  (void)event_data;
  (void)widget_deleted;

  profile = player_ui_get_edit_playlist_profile();

  if (*(int16_t *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for profile name",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xad2,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    name_buffer = ui_widget_realloc(
      *(int *)((char *)widget + 0x3c), 0x100,
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0xad6);
    *(void **)((char *)widget + 0x3c) = name_buffer;

    if (name_buffer != NULL) {
      ustrncpy((wchar_t *)name_buffer, (wchar_t *)profile, 0x7f);
      *(int16_t *)((char *)*(void **)((char *)widget + 0x3c) + 0xfe) = 0;
    }

    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}
