#include "x87_math.h"
#include "ui_widget_globals.h"

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
  ((uint32_t *)&ui_plasma_effect_color)[0] = component_0;
  ((uint32_t *)&ui_plasma_effect_color)[1] = component_1;
  ((uint32_t *)&ui_plasma_effect_color)[2] = component_2;
  ((uint32_t *)&ui_plasma_effect_color)[3] = component_3;
}

/* ui_widgets_initialize — sets up the UI widget subsystem. Allocates a
 * 0x4000-byte block via debug_malloc for widget_memory_pool ([0x31e04c]),
 * initializes the pool, zeroes widget_globals (0x68 bytes at 0x46cc20),
 * and sets the -1 sentinels: the queued main-menu error (field_28), the
 * deferred dashboard error (field_48), and the error_handle of each
 * per-player deferred error (field_30[]) and cinematic-deferred error
 * (field_4c[]). widget_globals_initialized (0x46cc82) records whether the
 * allocation succeeded. The fade float (field_2c, 0x46cc4c) starts at
 * -1.0f. */
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
    pool = (int *)widget_memory_pool;
    pool[1] = alloc_result;
    pool = (int *)widget_memory_pool;
    pool[2] = 0x4000;
  } else {
    succeeded = false;
  }

  stack_memory_pool_initialize(widget_memory_pool);
  csmemset((void *)widget_globals_base, 0, WIDGET_GLOBALS_SIZE);

  widget_globals_field_28 = -1;
  widget_globals_field_48 = -1;

  ptr_b = &widget_globals_field_4c[0].error_handle;
  ptr_a = &widget_globals_field_30[0].error_handle;
  do {
    *ptr_a = -1;
    *ptr_b = -1;
    ptr_a = (int16_t *)((char *)ptr_a + 6);
    ptr_b = (int16_t *)((char *)ptr_b + 4);
  } while ((int)ptr_a < (int)&widget_globals_field_48);

  widget_globals_initialized = succeeded;
  widget_globals_field_2c = -1.0f;
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
  widget_globals_field_2c = value;
}

/* ui_widget_debug_show_path — sets the debug overlay flag at 0x46cc84 that
 * controls whether render_ui_widgets() draws each on-screen widget's tag
 * name in the small debug font (see the render_ui_widgets comment below). */
void ui_widget_debug_show_path(unsigned char value)
{
  widget_globals_field_64 = value;
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
  return stack_memory_pool_realloc(widget_memory_pool, a1, a2, a3, a4);
}

/* widget_free — releases a widget node back to the global widget stack
 * memory pool at [0x31e04c]. Thin wrapper around
 * stack_memory_pool_deallocate, same pool used by ui_widget_realloc above
 * and the other stack_memory_pool_deallocate call sites in this file. */
void widget_free(void *widget)
{
  stack_memory_pool_deallocate(widget_memory_pool, widget);
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
  if (widget_globals_initialized != 0) {
    for (slot = widget_globals_field_00;
         (int)slot < (int)&widget_globals_field_00[4]; slot++) {
      if (*slot != 0) {
        return true;
      }
    }
  }

  return active;
}

/* ui_widgets_active_for_local_player (0xe3da0) — like ui_widgets_active,
 * but only returns true for a non-NULL root slot (0x46cc20..0x46cc2c)
 * whose widget's int16 at +0x08 equals local_player_index. Asserts
 * 0 <= local_player_index < 4 (line 0x456).
 * 0xe3da5: MOV SI,[EBP+8]; XOR BL,BL; loop: MOV ECX,[EAX]; TEST/JZ next;
 * CMP word[ECX+8],SI; JZ ret-true (AL=1); else RET AL=BL. */
bool ui_widgets_active_for_local_player(int16_t local_player_index)
{
  int *slot;
  bool active;

  active = false;
  assert_halt_msg_at("expected a valid local_player_index",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x456,
                     local_player_index >= 0 && local_player_index < 4);
  if (widget_globals_initialized != 0) {
    for (slot = widget_globals_field_00;
         (int)slot < (int)&widget_globals_field_00[4]; slot++) {
      if (*slot != 0 &&
          *(int16_t *)((char *)*slot + 0x08) == local_player_index) {
        active = true;
        break;
      }
    }
  }

  return active;
}

/* ui_widgets_inhibit_processing — sets or clears the events-suppressed
 * flag widget_globals_field_65 (0x46cc85). When suppressed, the
 * per-frame event dispatch in process_ui_widgets skips input processing.
 * Asserts that the widget subsystem has been initialized
 * (widget_globals_initialized, 0x46cc82). */
void ui_widgets_inhibit_processing(
  bool inhibit) /* name: PAL 2342 ui_widget.c:1624 */
{
  assert_halt_msg_at("widget_globals.initialized",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x496,
                     widget_globals_initialized);
  widget_globals_field_65 = (uint8_t)inhibit;
}

/* compute_offset_coordinate (0xe3e60-0xe3e7b) — returns the fractional part
 * fmod(param_1 * (param_2 * 0.001f), 1.0).
 * 0xe3e63: FLD [EBP+0xc]; FMUL [0x255ef8] (0.001f); FIMUL [EBP+8];
 * FLD double [0x2573d8] (1.0); JMP 0x1daf7e (_CIfmod, tail call, result in
 * ST0). No known direct callers (xrefs_to empty). Parameter meanings unknown.
 */
double compute_offset_coordinate(int param_1, float param_2)
{
#if defined(_MSC_VER) && !defined(__clang__)
  double __cdecl fmod(double, double);
  return fmod(param_1 * (param_2 * *(float *)0x255ef8), 1.0);
#else
  return x87_fmod(param_1 * (param_2 * *(float *)0x255ef8), 1.0);
#endif
}

/* draw_bitmap_in_rect (0xe3e80-0xe4303) — draw `bitmap` as one screen quad
 * over `rect`, sampling the `bitmap_rect` sub-rectangle (whole bitmap when
 * NULL), optionally clipped to `clip_rect`, modulated by `argb`.
 * Rectangles are {y0, x0, y1, x1} int16 (viewport_bounds_t layout): the
 * top-left vertex takes x from [+2] (EBP-0x44) and y from [+0] (EBP-0x40).
 * `bitmap` is a bitmap_data pointer; width at +4 and height at +6
 * (default_bitmap_rect x1/y1 at 0xe3ee5/0xe3ee9, MOVSX at 0xe3fee/0xe402e).
 * param_6 (EBP+0x1c) is never read in 2276 (PAL 2342 names it
 * multitexture_params and does not read it either). no_plasma is a byte
 * (MOV AL,[EBP+0x20]; TEST AL,AL at 0xe40e8).
 * With no_plasma the bitmap is map[0]. Otherwise map[0]/map[1] are element 0
 * of the bitmap block (+0x60, element size 0x30) of interface bitmap 0xf, the
 * plasma layers, scrolled by fmod(time * k, 1.0) (compute_offset_coordinate
 * inlined; the k = delta * 0.001f products are pre-folded constants
 * in the binary), and the bitmap is map[2].
 * plasma_fade is a copy of ui_plasma_effect_color (0x5aa460), taken on entry.
 */
void draw_bitmap_in_rect(int bitmap, int16_t *rect, int16_t *bitmap_rect,
                         int16_t *clip_rect, uint32_t argb, int param_6,
                         bool no_plasma)
{
#if defined(_MSC_VER) && !defined(__clang__)
  double __cdecl fmod(double, double);
#endif
  real_argb_color plasma_fade;
  float map_tint[3];
  float map_fade;
  int16_t default_bitmap_rect[4];
  real_point2d positions[4];
  dynamic_screen_vertex_t vertices[4];
  rasterizer_dynamic_screen_geometry_parameters_t parameters;
  float bitmap_width;
  float bitmap_height;
  float texture_width;
  float texture_height;
  real_point2d map0_offset;
  real_point2d map1_offset;
  int16_t rectangle_x0;
  int16_t rectangle_y0;
  int16_t rectangle_width;
  int16_t rectangle_height;
  int16_t source_width;
  int16_t source_height;
  int16_t vertex_index;
  void *plasma_bitmap;
  int time_ms;
  float time_real;

  if (bitmap != 0 && rect != NULL) {
    plasma_fade = ui_plasma_effect_color;
    map_tint[0] = 0.9f;
    map_tint[1] = 0.9f;
    map_tint[2] = 0.9f;
    map_fade = 0.9f;

    if (bitmap_rect == NULL) {
      default_bitmap_rect[1] = 0;
      default_bitmap_rect[0] = 0;
      default_bitmap_rect[3] = *(int16_t *)(bitmap + 4);
      default_bitmap_rect[2] = *(int16_t *)(bitmap + 6);
      bitmap_rect = default_bitmap_rect;
    }

    rectangle_width = rect[3] - rect[1];
    rectangle_height = rect[2] - rect[0];
    rectangle_x0 = rect[1];
    rectangle_y0 = rect[0];
    source_width = bitmap_rect[3] - bitmap_rect[1];
    source_height = bitmap_rect[2] - bitmap_rect[0];
    positions[0].x = (float)rectangle_x0;
    positions[0].y = (float)rectangle_y0;
    positions[1].x = (float)(rectangle_x0 + rectangle_width);
    positions[1].y = (float)rectangle_y0;
    positions[2].x = (float)(rectangle_x0 + rectangle_width);
    positions[2].y = (float)(rectangle_y0 + rectangle_height);
    positions[3].x = (float)rectangle_x0;
    positions[3].y = (float)(rectangle_y0 + rectangle_height);

    if (clip_rect != NULL) {
      if (clip_rect[1] > rect[1]) {
        positions[0].x = positions[3].x = (float)clip_rect[1];
      }
      if (clip_rect[3] < rect[3]) {
        positions[1].x = positions[2].x = (float)clip_rect[3];
      }
      if (clip_rect[0] > rect[0]) {
        positions[0].y = positions[1].y = (float)clip_rect[0];
      }
      if (clip_rect[2] < rect[2]) {
        positions[2].y = positions[3].y = (float)clip_rect[2];
      }
    }

    bitmap_width = (float)*(int16_t *)(bitmap + 4);
    bitmap_width = 1.0f > bitmap_width ? 1.0f : bitmap_width;
    texture_width = (float)source_width / bitmap_width;
    texture_width = texture_width > 1.0f ? 1.0f : texture_width;
    bitmap_height = (float)*(int16_t *)(bitmap + 6);
    bitmap_height = 1.0f > bitmap_height ? 1.0f : bitmap_height;
    texture_height = (float)source_height / bitmap_height;
    texture_height = texture_height > 1.0f ? 1.0f : texture_height;

    for (vertex_index = 0; vertex_index < 4; vertex_index++) {
      vertices[vertex_index].color = argb;
      vertices[vertex_index].texture_coordinates.x =
        vertex_index % 3 ? texture_width : 0.0f;
      vertices[vertex_index].texture_coordinates.y =
        vertex_index > 1 ? texture_height : 0.0f;
      vertices[vertex_index].position = positions[vertex_index];
    }

    csmemset(&parameters, 0, sizeof(parameters));
    if (no_plasma) {
      parameters.map_texture_scale[0].j = 1.0f;
      parameters.map_texture_scale[0].i = 1.0f;
      parameters.map_scale[0].j = 1.0f;
      parameters.map_scale[0].i = 1.0f;
      parameters.map[0] = (void *)bitmap;
    } else {
      plasma_bitmap = tag_block_get_element(
        (char *)tag_get(0x6269746d /* 'bitm' */, interface_get_tag_index(0xf)) +
          0x60,
        0, 0x30);
      time_ms = system_milliseconds();
      time_real = (float)time_ms;
#if defined(_MSC_VER) && !defined(__clang__)
      map0_offset.x = (float)fmod(time_real * 3.215434e-05f, 1.0) * 311.0f;
      map0_offset.y = (float)fmod(time_real * 2.6795286e-05f, 1.0) * 311.0f;
      map1_offset.x = -(float)fmod(time_real * 3.5536603e-05f, 1.0);
      map1_offset.x *= 201.0f;
      map1_offset.y = -(float)fmod(time_real * 3.1094525e-05f, 1.0);
      map1_offset.y *= 201.0f;
#else
      /* x87_fmod: FPREM like _CIfmod (0x1daf7e); clang's fmod is FPREM1. */
      map0_offset.x = x87_fmod(time_real * 3.215434e-05f, 1.0) * 311.0f;
      map0_offset.y = x87_fmod(time_real * 2.6795286e-05f, 1.0) * 311.0f;
      map1_offset.x = -x87_fmod(time_real * 3.5536603e-05f, 1.0) * 201.0f;
      map1_offset.y = -x87_fmod(time_real * 3.1094525e-05f, 1.0) * 201.0f;
#endif

      parameters.map[0] = plasma_bitmap;
      parameters.map0_to_1_blend_function = 5;
      parameters.map_scale[0].i = 1.0f;
      parameters.map_scale[0].j = 1.0f;
      parameters.map_anchor_screen[0] = 1;
      parameters.map_wrapped[0] = 1;
      parameters.map_texture_scale[0].i = 1.0f / 311.0f;
      parameters.map_texture_scale[0].j = 1.0f / 311.0f;
      parameters.map_tint[0] = map_tint;
      parameters.map_fade[0] = &map_fade;
      parameters.map_offset[0] = &map0_offset;
      parameters.map[1] = plasma_bitmap;
      parameters.map1_to_2_blend_function = 0;
      parameters.map_scale[1].i = 1.0f;
      parameters.map_scale[1].j = 1.0f;
      parameters.map_anchor_screen[1] = 1;
      parameters.map_wrapped[1] = 1;
      parameters.map_texture_scale[1].i = 1.0f / 201.0f;
      parameters.map_texture_scale[1].j = 1.0f / 201.0f;
      parameters.map_tint[1] = map_tint;
      parameters.map_fade[1] = &map_fade;
      parameters.map_offset[1] = &map1_offset;
      parameters.plasma_fade = plasma_fade;
      parameters.map_fade[2] = NULL;
      parameters.map_texture_scale[2].j = 1.0f;
      parameters.map_texture_scale[2].i = 1.0f;
      parameters.map_scale[2].j = 1.0f;
      parameters.map_scale[2].i = 1.0f;
      parameters.map[2] = (void *)bitmap;
      parameters.doing_plasma_effect = 1;
    }

    parameters.meter_parameters = NULL;
    parameters.point_sampled = 0;
    parameters.framebuffer_blend_function = 0;
    rasterizer_sprites_render(&parameters, vertices);
  }
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
  byte_46CC88 = (uint8_t)active;
}

/* main_menu_is_active — reads the "main menu active" byte at 0x46cc88 set by
 * main_menu_active() (above). 0xe43e0: `MOV AL,[0x46cc88]; RET` — a single
 * byte load truncated to bool, no other logic. */
bool main_menu_is_active(void)
{
  return (bool)byte_46CC88;
}

bool main_menu_screen_is_active(void)
{
  int root_widget;

  if (byte_46CC88 == 1) {
    root_widget = widget_globals_field_00[0];
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
  return widget_globals_initialization_thread != NULL;
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

/* Deferred per-local-player error slots: widget_globals_field_30[4]
 * (ui_widget_deferred_error_t, ui_widget_globals.h), stride 6 bytes:
 * word error_handle (-1 == slot empty), word local_player_index, then the
 * two flag bytes forwarded to the error screen. */

/* display_error_deferred — queues one error message per local player, to be
 * dispatched by the deferred-error sweep in process_ui_widgets(). A
 * player_index of -1 (no specific local player) uses slot 0 without the
 * range assert; any other value must be a valid local player index. If the
 * player's slot is already occupied the request is dropped with a
 * priority-2 warning, same shape as display_error_when_main_menu_loaded(). */
__declspec(noinline) void
display_error_deferred(short error_code, short player_index, bool a3, bool a4)
{
  ui_widget_deferred_error_t *deferred_errors;
  int index;

  deferred_errors = widget_globals_field_30;
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
  if (widget_globals_field_48 == -1) {
    widget_globals_field_48 = error_handle;
    widget_globals_field_4a = allow_abort;
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

  if (widget_globals_field_66 != 0)
    return;

  if (main_change_map_name_in_progress())
    return;

  tag_index = tag_loaded(0x6c736e64, "sound\\music\\title1\\title1");
  if (tag_index != -1) {
    error(2, "starting main menu music");
    sound_looping_start(tag_index, -1, 1.0f);
    widget_globals_field_66 = 1;
    return;
  }
  error(2, "title music tag not found");
}

void ui_stop_main_menu_music(void)
{
  int tag_index;

  if (widget_globals_field_66 != 1) {
    return;
  }

  tag_index = tag_loaded(0x6c736e64, "sound\\music\\title1\\title1");
  if (tag_index != -1) {
    error(2, "stopping main menu music");
    sound_looping_stop(tag_index);
    widget_globals_field_66 = 0;
    return;
  }

  error(2, "title music tag not found");
  widget_globals_field_66 = 0;
}

/* noinline: main_menu_initialize (0xea090) CALLs this at 0xea0bc; in PAL it
 * lives in a different TU (ui_widget_event_handler_functions.c), so the
 * original never inlined it. */
__declspec(noinline) bool ui_main_menu_music_active(void)
{
  return widget_globals_field_66;
}

void ui_widgets_disable_pause_game(int duration_ticks)
{
  assert_halt_msg_at("duration_ticks>=0",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x9d7,
                     duration_ticks >= 0);
  dword_46CC44 = duration_ticks;
}

/* push_widget (0xe46f0) — pushes a copy of the 0xc-byte widget_stack_data at
 * record onto the widget history list at *head. head arrives in EBX and
 * record in EDI (kb.json @<ebx>/@<edi>). The 0x10-byte node is allocated from
 * the widget stack memory pool at [0x31e04c] BEFORE the "top && data" assert
 * (ui_widget.c:0x9e6), as in the binary. On allocation failure the original
 * only warns (display_assert halt=0, no system_exit, ui_widget.c:0x9f0).
 * Mirror of pop_widget below. */
void push_widget(int *head, void *record)
{
  widget_stack_node_t *node;

  node = (widget_stack_node_t *)stack_memory_pool_allocate(
    widget_memory_pool, sizeof(widget_stack_node_t),
    "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x9e4);
  if (head == NULL || record == NULL) {
    display_assert("top && data", "c:\\halo\\SOURCE\\interface\\ui_widget.c",
                   0x9e6, true);
    system_exit(-1);
  }
  if (node != NULL) {
    node->data = *(widget_stack_data_t *)record;
    node->next = (widget_stack_node_t *)*head;
    *head = (int)node;
  } else {
    display_assert("out of memory! the UI screen history will be hosed.",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x9f0, false);
  }
}

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
  stack_memory_pool_deallocate(widget_memory_pool, top);
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

    assert_halt_msg_at("child->previous == NULL",
                       "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0xabe,
                       *(int *)((char *)child + 0x28) == 0);
    assert_halt_msg_at(
      "next->previous == child", "c:\\halo\\SOURCE\\interface\\ui_widget.c",
      0xac2, next == NULL || *(int *)((char *)next + 0x28) == (int)child);

    ui_widget_delete(child);

    if (next != NULL)
      *(int *)((char *)next + 0x28) = 0;

    child = next;
  } while (child != NULL);
}

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

/* widget_instance_get_cumulative_alpha_modifier (0xe4960-0xe497a) — the
 * widget's own alpha modifier (+0x24) multiplied by that of every ancestor
 * along the +0x30 parent chain. ABI: widget in EAX (callers 0xe6289
 * MOV EAX,EBX), float return in ST0 (FLD [EAX+0x24]; FMUL [EAX+0x24] per
 * parent; RET with no store). widget_instance_render_recursive (0xe73c0)
 * inlines the same walk. PAL 2342: static __inline. */
float widget_instance_get_cumulative_alpha_modifier(void *widget)
{
  float alpha_modifier;
  char *parent;

  alpha_modifier = *(float *)((char *)widget + 0x24);
  for (parent = *(char **)((char *)widget + 0x30); parent != NULL;
       parent = *(char **)(parent + 0x30)) {
    alpha_modifier *= *(float *)(parent + 0x24);
  }
  return alpha_modifier;
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

  if (((widget_instance_t *)w)->field_12 != 0) {
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
 * rasterizer_draw_string sequence already lifted as render_state_text_0's
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
  rasterizer_draw_string(local_bounds, NULL, NULL, 0, (unsigned short *)text);
  *dst_rect = *src_rect;
}

/* string_has_icons_to_draw (0xe4ce0-0xe4d3c) — true if some '%' in `text` is
 * followed by a name get_icon_type recognises (get_icon_type != -1).
 * ABI: text in EAX (TEST EAX,EAX at entry; caller 0xe63f2 MOV EAX,[ESI]);
 * byte return (XOR AL,AL / MOV AL,1; caller 0xe63fc TEST AL,AL).
 * Asserts "string" (0x27b838) at ui_widget.c line 0x1055 when text is NULL
 * (PAL 2342 names the parameter `string`). Each '%' is found with wcschr
 * (0x1db134); the scan resumes at the character after it. */
bool string_has_icons_to_draw(const wchar_t *text)
{
  const wchar_t *icon_spec;

  assert_halt_msg_at("string", "c:\\halo\\SOURCE\\interface\\ui_widget.c",
                     0x1055, text);
  while (text != NULL) {
    icon_spec = _wcschr(text, L'%');
    if (icon_spec == NULL)
      break;
    icon_spec++;
    if (get_icon_type(icon_spec) != -1)
      return true;
    text = icon_spec;
  }
  return false;
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

  if (local_player_index == -1) {
    local_player_index = local_player_get_next(-1);
  }

  csmemset(preferences, 0, 0x18);

  if (local_player_index != -1) {
    input_abstraction_get_local_player_preferences(local_player_index,
                                                   preferences);
  }

  switch (*(int16_t *)(preferences + 0x14)) {
  case 1:
  case 3:
    return true;
  }
  return false;
}

/* remap_sticks_for_local_player (0xe4da0) — jump table on icon_type - 0x10
 * (0..0xf). Icon types 0x10/0x1e (left-stick/move) return 0x10 plus 1 when
 * should_flip_sticks_for_local_player(local_player_index) is true; icon types
 * 0x11/0x1f (right-stick/look) return 0x11 minus 1 in that case. Each path
 * first asserts the get_icon_type() table indices (ui_widget.c lines
 * 0x1093/0x1094 and 0x109a/0x109b). All other icon types are returned
 * unchanged. */
short remap_sticks_for_local_player(short icon_type, int local_player_index)
{
  switch (icon_type) {
  case 0x10:
  case 0x1e:
    assert_halt_msg_at("16 == get_icon_type(L\"left-stick\")",
                       "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x1093,
                       get_icon_type(L"left-stick") == 0x10);
    assert_halt_msg_at("30 == get_icon_type(L\"move\")",
                       "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x1094,
                       get_icon_type(L"move") == 0x1e);
    return (short)(should_flip_sticks_for_local_player(
                     (int16_t)local_player_index) != 0) +
           0x10;
  case 0x11:
  case 0x1f:
    assert_halt_msg_at("17 == get_icon_type(L\"right-stick\")",
                       "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x109a,
                       get_icon_type(L"right-stick") == 0x11);
    assert_halt_msg_at("31 == get_icon_type(L\"look\")",
                       "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x109b,
                       get_icon_type(L"look") == 0x1f);
    return (short)(0x11 - (should_flip_sticks_for_local_player(
                             (int16_t)local_player_index) != 0));
  }
  return icon_type;
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

/* widget_instance_set_focused_child_by_index (0xe5090) — restores focus after
 * a widget is reloaded from the history stack. tag_index arrives in EAX
 * (kb.json @<eax>); widget (the freshly loaded root) and child_index are on
 * the stack. Finds the widget whose definition tag is tag_index under widget;
 * with child_index >= 0 it walks that widget's children and focuses the
 * child_index'th one (bailing out if it passes a multi-item spinner list) and
 * syncs the parent list's selected index; with child_index < 0 it focuses the
 * found widget itself when it can receive events and is not a multi-item
 * spinner list. Names/shape follow PAL 2342 (T2); control flow re-checked
 * against the 2276 disassembly. */
void widget_instance_set_focused_child_by_index(int tag_index, int widget,
                                                int16_t child_index)
{
  widget_instance_t *parent;
  widget_instance_t *child;
  ui_widget_definition_t *definition;
  int index;

  if (tag_index == -1) {
    return;
  }
  parent = (widget_instance_t *)widget_instance_find_by_tag_index_recursive(
    (int *)widget, tag_index);
  if (parent == NULL) {
    return;
  }
  if (child_index >= 0) {
    for (child = parent->child, index = 0; child != NULL;
         child = child->next, index++) {
      if (child->type == UI_WIDGET_TYPE_SPINNER_LIST) {
        definition = (ui_widget_definition_t *)tag_get(
          0x44654c61 /* 'DeLa' */, child->definition_tag_index);
        if (definition->child_widgets.count > 1) {
          return;
        }
      }
      if (index == child_index) {
        widget_instance_give_focus_directly((void *)widget, child);
        if (child->parent != NULL &&
            (child->parent->type == UI_WIDGET_TYPE_SPINNER_LIST ||
             child->parent->type == UI_WIDGET_TYPE_COLUMN_LIST)) {
          child->parent->list_selected_index = (int16_t)index;
        }
        return;
      }
    }
  } else if (widget_instance_can_receive_events(parent)) {
    if (parent->type == UI_WIDGET_TYPE_SPINNER_LIST) {
      definition = (ui_widget_definition_t *)tag_get(
        0x44654c61 /* 'DeLa' */, parent->definition_tag_index);
      if (definition->child_widgets.count > 1) {
        return;
      }
    }
    widget_instance_give_focus_directly((void *)widget, parent);
  }
}

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
    widget_memory_pool, (int)buffer,
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

/* column_list_update (0xe5380) — walks the child list starting at
 * [widget+0x34] via next-sibling [child+0x2c]. For each child whose
 * word at +0x56 equals 2, stores word +0x50 = 1 when the child is the
 * one at [widget+0x38], else 0. definition is never read (0xe5380-0xe53b6:
 * no EDX/stack access besides MOV EDX,2). Fields are unproven; raw
 * offsets kept. */
void column_list_update(void *widget, void *definition)
{
  char *child;

  (void)definition;
  for (child = *(char **)((char *)widget + 0x34); child != NULL;
       child = *(char **)(child + 0x2c)) {
    if (child == *(char **)((char *)widget + 0x38)) {
      if (((widget_instance_t *)child)->field_56 == 2) {
        *(short *)(child + 0x50) = 1;
      }
    } else if (((widget_instance_t *)child)->field_56 == 2) {
      *(short *)(child + 0x50) = 0;
    }
  }
}

void column_list_update(void *widget, void *definition);

/* widget_instance_tab_to_next_valid_widget (0xe53e0) — widget arrives in EDI
 * (kb.json @<edi>). Starting after the focused child (or at the first child),
 * walks the sibling ring and focuses the first child whose definition has
 * event handlers or passes unhandled events to its children; inside a
 * spinner/column list any child qualifies. Stops when it wraps back to the
 * focused child. Names follow PAL 2342 (T2). */
void widget_instance_tab_to_next_valid_widget(void *widget_ptr)
{
  widget_instance_t *widget;
  widget_instance_t *child;
  ui_widget_definition_t *definition;

  widget = (widget_instance_t *)widget_ptr;
  if (widget->focused_child != NULL && widget->focused_child->next != NULL) {
    child = widget->focused_child->next;
  } else {
    child = widget->child;
  }
  while (child != NULL && child != widget->focused_child) {
    definition = (ui_widget_definition_t *)tag_get(0x44654c61 /* 'DeLa' */,
                                                   child->definition_tag_index);
    if (definition->event_handlers.count > 0 ||
        (definition->flags &
         UI_WIDGET_PASS_UNHANDLED_EVENTS_TO_CHILDREN_FLAG) != 0 ||
        widget->type == UI_WIDGET_TYPE_SPINNER_LIST ||
        widget->type == UI_WIDGET_TYPE_COLUMN_LIST) {
      widget->focused_child = child;
      return;
    }
    child = child->next;
    if (child == NULL) {
      child = widget->child;
    }
  }
}

/* widget_instance_tab_to_previous_valid_widget (0xe5440) — widget@<edi>.
 * Starting from the focused child [widget+0x38], steps backwards via the
 * previous-sibling link [child+0x28], wrapping to the last child of the
 * list at [widget+0x34] (walked via [child+0x2c]). With no focused child
 * the candidate is [first+0x28], else first itself (first is dereferenced
 * without a NULL test, 0xe546c-0xe546f). Stops when it returns to the
 * focused child; otherwise the first candidate whose DeLa definition has
 * [def+0x54] > 0 or bit 0 of byte [def+0x2c] set, or whose widget word
 * +0xe is 2 or 3, becomes the new focused child. Fields unproven; raw
 * offsets kept. */
void widget_instance_tab_to_previous_valid_widget(void *widget)
{
  int *focused;
  int *candidate;
  int *next;
  char *definition;

  focused = *(int **)((char *)widget + 0x38);
  if (focused != NULL) {
    candidate = (int *)focused[10];
    if (candidate == NULL) {
      candidate = *(int **)((char *)widget + 0x34);
      if (candidate == NULL) {
        return;
      }
      for (next = (int *)candidate[11]; next != NULL; next = (int *)next[11]) {
        candidate = next;
      }
    }
  } else {
    next = *(int **)((char *)widget + 0x34);
    candidate = (int *)next[10];
    if (candidate == NULL) {
      candidate = next;
    }
  }
  if (candidate == NULL) {
    return;
  }
  while (candidate != *(int **)((char *)widget + 0x38)) {
    definition = (char *)tag_get(0x44654c61, *candidate);
    if (*(int *)(definition + 0x54) > 0 ||
        (*(unsigned char *)(definition + 0x2c) & 1) != 0 ||
        *(short *)((char *)widget + 0xe) == 2 ||
        *(short *)((char *)widget + 0xe) == 3) {
      *(int **)((char *)widget + 0x38) = candidate;
      return;
    }
    candidate = (int *)candidate[10];
    if (candidate == NULL) {
      candidate = *(int **)((char *)widget + 0x34);
      if (candidate != NULL) {
        for (next = (int *)candidate[11]; next != NULL;
             next = (int *)next[11]) {
          candidate = next;
        }
      }
      if (candidate == NULL) {
        return;
      }
    }
  }
}

/* Local shape-only float3, not a claimed Bungie struct: mirrors the
 * reference's whole-struct copy so the same EBP slots are written. */
typedef struct {
  float f0, f1, f2;
} get_ui_rgb_white_color_t;

/* get_ui_rgb_white (0xe54e0) — writes a float[3] RGB color into out_color
 * and returns out_color (EAX = [EBP+8] at RET; cdecl, no stack pop).
 * Disassembly copies 12 bytes from the struct pointed to by the global at
 * 0x2ee708 into a local, then overwrites all three local words with the
 * RGB white constants at 0x31e148/0x31e14c/0x31e150 (stores in 0,2,1
 * order), then copies the local out through out_color. */
float *get_ui_rgb_white(float *out_color)
{
  get_ui_rgb_white_color_t local_color;

  local_color = **(get_ui_rgb_white_color_t **)0x2ee708;
  local_color.f0 = *(float *)0x31e148;
  local_color.f2 = *(float *)0x31e150;
  local_color.f1 = *(float *)0x31e14c;

  *(get_ui_rgb_white_color_t *)out_color = local_color;

  return out_color;
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
  widget_globals_field_60 = result;
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

  assert_halt(widget && widget_globals_initialized);

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
  if (((widget_instance_t *)w)->field_13 == 1) {
    assert_halt_msg_at("widget pause counter out of whack",
                       "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x220,
                       widget_globals_field_2a >= 1);

    widget_globals_field_2a--;

    if (widget_globals_field_2a == 0) {
      if (game_time_get_paused()) {
        game_time_set_paused(0);
        if (byte_46CC88 != 0) {
          main_reset_player_actions();
          game_time_dispose_from_old_map();
          game_time_initialize_for_new_map();
          game_time_start();
        }
      }
      if (widget_globals_field_67 == 1) {
        sound_set_music_enabled(0);
        widget_globals_field_67 = 0;
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
      stack_memory_pool_deallocate(widget_memory_pool, (void *)w[0xf]);
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
      stack_memory_pool_deallocate(widget_memory_pool, (void *)w[0x13]);
    }
    if (w[0x12] != 0) {
      ui_widget_delete((void *)w[0x12]);
    }
  }

  /* free the widget itself */
  stack_memory_pool_deallocate(widget_memory_pool, w);

  /* clear root widget slot if this widget was a root */
  for (idx = 0; idx < 4; idx++) {
    if (widget_globals_field_00[idx] == (int)w) {
      widget_globals_field_00[idx] = 0;
      return;
    }
  }
}

/* ui_widgets_close_all — for each of the 4 UI widget stacks, closes the
 * root via ui_widget_delete (0xe5620), then frees every node on the
 * pending list widget_globals_field_10[i] (linked through +0xc). */
void ui_widgets_close_all(void)
{
  int *list_heads;
  int widget;
  int next;
  void *pool;

  list_heads = widget_globals_field_10;
  do {
    /* close the root widget for this stack if present */
    if (list_heads[-4] != 0) {
      ui_widget_delete((void *)list_heads[-4]);
    }
    /* walk the linked list at list_heads[i], freeing each node */
    widget = *list_heads;
    if (widget != 0) {
      while (widget != 0) {
        pool = widget_memory_pool;
        next = *(int *)(widget + 0xc);
        *list_heads = next;
        stack_memory_pool_deallocate(pool, (void *)widget);
        widget = *list_heads;
      }
    }
    list_heads++;
  } while ((int)list_heads < (int)&widget_globals_field_10[4]);
}

/* ui_widgets_close_all_for_local_player (0xe5910) — like ui_widgets_close_all
 * above, but only for the one stack whose root widget's local_player_index
 * (+8) matches local_player_index: closes that root via ui_widget_delete
 * (0xe5620), then drains its pending list (linked through +0xc) back to
 * widget_memory_pool. Asserts local_player_index is in [0,4) -- unlike
 * ui_widgets_pop_stack below, -1 is NOT special-cased to player 0 here. */
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

  list_heads = widget_globals_field_10;
  do {
    root = list_heads[-4];
    if (root != 0 && *(int16_t *)(root + 8) == local_player_index) {
      ui_widget_delete((void *)root);

      if (*list_heads != 0) {
        while (*list_heads != 0) {
          widget = *list_heads;
          pool = widget_memory_pool;
          next = *(int *)(widget + 0xc);
          *list_heads = next;
          stack_memory_pool_deallocate(pool, (void *)widget);
        }
      }
    }
    list_heads++;
  } while ((int)list_heads < (int)&widget_globals_field_10[4]);
}

/* ui_widgets_pop_stack — drains one pending queued entry from the
 * pending-load list at 0x46cc30[local_player_index] (see
 * pop_widget / push_widget above).
 * local_player_index == -1 is treated as player 0; otherwise it must be in
 * [0, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS). The popped record is discarded --
 * this only drains one node, it does not apply it. */
__declspec(noinline) void ui_widgets_pop_stack(int16_t local_player_index)
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

  if (widget_globals_field_10[(int)local_player_index] != 0) {
    pop_widget(&widget_globals_field_10[(int)local_player_index],
               (void *)&record);
  }
}

/* main_screen_shell_begin_fade — starts the shell's screen-fade-out on each
 * of the 4 UI root widget stacks whose root is not in "in_game_mode" (+0x15,
 * see render_ui_widgets above). Stops attract mode, then for each eligible
 * root stamps the fade duration (+0x20) with duration_ms and the timeout
 * (+0x1c) with (widget_globals_field_20 - start_tick (+0x18)) + 100. Finally
 * frees every widget queued on that stack's pending-close list
 * (widget_globals_field_10[i], linked through +0xc) — the same list-drain as
 * ui_widgets_close_all, but without closing the root widget itself. */
void main_screen_shell_begin_fade(int duration_ms)
{
  int *root_slots;
  int *list_head;
  int root;
  int widget;
  int next;
  void *pool;

  ui_stop_main_menu_music();

  root_slots = widget_globals_field_00;
  do {
    root = *root_slots;
    if (root != 0 && *(uint8_t *)(root + 0x15) == 0) {
      *(int *)(root + 0x20) = duration_ms;
      *(int *)(root + 0x1c) =
        ((int)widget_globals_field_20 - *(int *)(root + 0x18)) + 100;

      list_head = root_slots + 4; /* matching slot in field_10[] */
      widget = *list_head;
      while (widget != 0) {
        pool = widget_memory_pool;
        next = *(int *)(widget + 0xc);
        *list_head = next;
        stack_memory_pool_deallocate(pool, (void *)widget);
        widget = *list_head;
      }
    }
    root_slots++;
  } while ((int)root_slots < (int)&widget_globals_field_00[4]);
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

int ui_widget_load_widget_children(void *definition, void *widget);

/* ui_widget_load_children_recursive (0xe5b10) — Build a widget instance's
 * child list from its DeLa definition, including generated string-list items,
 * referenced child widgets, the column-list description widget, and initial
 * focus selection.
 *
 * Confirmed ABI: widget@ESI, definition on the stack, boolean result in AL.
 * PAL 2342 supplies the T2 semantic names; offsets and branches are verified
 * against the 2276 disassembly.
 */
char ui_widget_load_children_recursive(void *widget_ptr, void *definition_ptr)
{
  widget_instance_t *widget;
  ui_widget_definition_t *definition;
  char result;
  int child_index;

  widget = (widget_instance_t *)widget_ptr;
  definition = (ui_widget_definition_t *)definition_ptr;
  result = 1;

  if ((definition->list_flags &
       UI_LIST_ITEMS_GENERATED_FROM_STRING_LIST_TAG_FLAG) != 0) {
    int *string_list;
    int string_index;

    if (widget->type != UI_WIDGET_TYPE_SPINNER_LIST) {
      display_assert("_list_items_generated_from_string_list_tag flag should "
                     "only be set for 1-wide spinner list widgets",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0xa20, 1);
      system_exit(-1);
    }
    if (definition->child_widgets.count != 0) {
      display_assert("no child widget references are needed to define list "
                     "items when generating a list from a string list tag",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0xa22, 1);
      system_exit(-1);
    }
    if (definition->text_label_string_list.tag_index == -1) {
      display_assert("_list_items_generated_from_string_list_tag flag was set "
                     "but no string list tag was specified",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0xa24, 1);
      system_exit(-1);
    }
    string_list = (int *)tag_get(0x75737472 /* 'ustr' */,
                                 definition->text_label_string_list.tag_index);
    widget_globals_field_63 = 1;
    for (string_index = 0; string_index < *string_list; string_index++) {
      void *child;

      child = ui_widget_load_by_name_or_tag(
        NULL, widget->definition_tag_index, (int)widget,
        (unsigned short)widget->local_player_index, -1, -1, -1);
      if (child == NULL) {
        result = 0;
        break;
      }
      ui_widget_add_child(child, widget);
      widget->list_number_of_items++;
    }
    widget_globals_field_63 = 0;
  }

  for (child_index = 0; child_index < definition->child_widgets.count;
       child_index++) {
    char *reference =
      (char *)definition->child_widgets.address + child_index * 0x50;
    short controller_index = widget->local_player_index;
    if ((reference[0x30] & 1) != 0) {
      short custom_controller_index = *(short *)(reference + 0x34);
      if (custom_controller_index >= 0 && custom_controller_index < 4) {
        controller_index = custom_controller_index;
      } else {
        error(2, "invalid controller index specified for child widget (#%d)",
              (int)custom_controller_index);
      }
    }

    if (*(int *)(reference + 0xc) != -1) {
      widget_instance_t *child;

      child = (widget_instance_t *)ui_widget_load_by_name_or_tag(
        NULL, *(int *)(reference + 0xc), (int)widget,
        (unsigned short)controller_index, -1, -1, -1);
      if (child == NULL) {
        result = 0;
        break;
      }
      child->horizontal_offset =
        *(short *)(reference + 0x38) + widget->horizontal_offset;
      child->vertical_offset =
        *(short *)(reference + 0x36) + widget->vertical_offset;
      ui_widget_add_child(child, widget);
    }
  }

  if (widget->type == UI_WIDGET_TYPE_COLUMN_LIST &&
      *(int *)((char *)definition + 0x1b0) != -1) {
    widget_instance_t *description;
    *(widget_instance_t **)((char *)widget + 0x48) =
      (widget_instance_t *)ui_widget_load_by_name_or_tag(
        NULL, *(int *)((char *)definition + 0x1b0), (int)widget,
        (unsigned short)widget->local_player_index, -1, -1, -1);
    if (*(widget_instance_t **)((char *)widget + 0x48) != NULL) {
      description = *(widget_instance_t **)((char *)widget + 0x48);
      if (description->previous != NULL) {
        description->previous->next = NULL;
      }
      (*(widget_instance_t **)((char *)widget + 0x48))->previous = NULL;
      (*(widget_instance_t **)((char *)widget + 0x48))->parent = NULL;
    }
  }

  if (definition->flags >= 0) {
    if (widget->type == UI_WIDGET_TYPE_SPINNER_LIST ||
        widget->type == UI_WIDGET_TYPE_COLUMN_LIST) {
      widget->list_selected_index = 0;
      widget->list_last_tab_direction = 0;
    } else if ((definition->flags &
                UI_WIDGET_PASS_UNHANDLED_EVENTS_TO_CHILDREN_FLAG) == 0) {
      return result;
    }

    if (widget->child != NULL) {
      widget_instance_t *child = widget->child;
      while (child != NULL) {
        ui_widget_definition_t *child_definition;

        if (widget->type == UI_WIDGET_TYPE_SPINNER_LIST ||
            widget->type == UI_WIDGET_TYPE_COLUMN_LIST) {
          break;
        }
        child_definition = (ui_widget_definition_t *)tag_get(
          0x44654c61 /* 'DeLa' */, child->definition_tag_index);
        if (((widget_instance_t *)child)->field_12 == 0 &&
            (child_definition->event_handlers.count > 0 ||
             child->type == UI_WIDGET_TYPE_SPINNER_LIST ||
             child->type == UI_WIDGET_TYPE_COLUMN_LIST)) {
          break;
        }
        child = child->next;
      }
      if (child != NULL) {
        widget->focused_child = child;
      }
    }
  }

  return result;
}

/* draw_string_and_hack_in_icons (0xe5de0-0xe612e) — draws `text` into bounds,
 * replacing every "%<icon-name>" token with the matching HUD button icon.
 * The text is copied into the shared wide-string buffer at 0x46c420 and split
 * in place at each '%' (wcschr 0x1db134). Text runs before each token go
 * through render_state_text (the loop copies are inlined in the original, the
 * trailing run is a real CALL 0xe4c70); an unknown token draws a literal "%".
 *
 * Known icons are remapped for the local player at 0x5aa45c (uint16), then
 * resolved to a button-icon index: types <= 0x11 are used directly, 0x12..0x1c
 * index the player's input preferences (byte table at +8) through the signed
 * byte table at 0x31e126, 0x1d..0x1f use the same table then the jump table at
 * 0xe6130 (0x1c/0x1d/0x1e/0x1f -> 0xc/0xd/0x10/0x11); anything else asserts
 * FALSE at line 0x10f5. The icon element is hud globals (0x46bd0c) +0xc4,
 * element size 0x10: +0x02 int16 width offset, +0x08 pixel32 color, +0x0d
 * flags byte. Flags and width offset are saved, patched (bit 2 cleared; for
 * icons flagged in the byte table at 0x31e080, bit 4 cleared and width offset
 * forced to -5), and restored after render_state_bitmap.
 *
 * ABI (cdecl, RET 0): only [EBP+8] bounds, [EBP+0x18] text and the byte at
 * [EBP+0x1c] are read; param_2..param_4 are never touched. The
 * render_state_bitmap site (0xe60cb) pushes (bounds, color) with EBX = &cursor
 * and ESI = icon; its kb decl carries one more, never-read stack slot, passed
 * here as 0. */
void draw_string_and_hack_in_icons(short *bounds, int param_2, int param_3,
                                   int param_4, const wchar_t *text,
                                   bool ignore_icon_color)
{
  wchar_t *current;
  short cursor[4];

  current = (wchar_t *)0x46c420;
  memcpy(cursor, bounds, sizeof(cursor));
  _wcscpy(current, text);
  while (current != NULL) {
    wchar_t *icon_spec;
    short icon_type;

    icon_spec = _wcschr(current, L'%');
    if (icon_spec == NULL) {
      break;
    }
    *icon_spec = 0;
    icon_spec++;
    {
      short text_bounds[4];
      short indent;

      indent = (short)(cursor[1] - bounds[1]);
      if (indent < 0) {
        error(
          2,
          "initial_indent<0 in render_state_text() and was about to explode");
      }
      indent = (0 > indent)
                 ? 0
                 : indent;
      draw_string_set_indents(indent, 0);
      FUN_0019cdb0(bounds, current, text_bounds, cursor);
      cursor[1] -= 3;
      text_bounds[1] = bounds[1];
      rasterizer_draw_string(text_bounds, NULL, NULL, 0,
                             (unsigned short *)current);
      bounds[0] = cursor[0];
    }
    current = icon_spec;
    icon_type = get_icon_type(icon_spec);
    if (icon_type == -1) {
      short text_bounds[4];
      short indent;

      indent = (short)(cursor[1] - bounds[1]);
      if (indent < 0) {
        error(
          2,
          "initial_indent<0 in render_state_text() and was about to explode");
      }
      indent = (0 > indent)
                 ? 0
                 : indent;
      draw_string_set_indents(indent, 0);
      FUN_0019cdb0(bounds, L"%", text_bounds, cursor);
      cursor[1] -= 3;
      text_bounds[1] = bounds[1];
      rasterizer_draw_string(text_bounds, NULL, NULL, 0,
                             (unsigned short *)L"%");
      bounds[0] = cursor[0];
    } else {
      short remapped_icon_type;
      short icon_index;

      current =
        icon_spec + _wcslen(*(const wchar_t **)(0x31e098 + (int)icon_type * 4));
      remapped_icon_type = remap_sticks_for_local_player(
        icon_type, (int)*(unsigned short *)0x5aa45c);
      icon_index = -1;
      if (remapped_icon_type > 0x11) {
        if (remapped_icon_type <= 0x1f) {
          if (remapped_icon_type <= 0x1c) {
            unsigned char preferences[0x18];

            input_abstraction_get_local_player_preferences(
              (short)*(unsigned short *)0x5aa45c, preferences);
            icon_index =
              preferences[8 + *(signed char *)(0x31e126 + remapped_icon_type)];
          } else {
            icon_index = *(signed char *)(0x31e126 + remapped_icon_type);
            switch (remapped_icon_type) {
            case 0x1c:
              icon_index = 0xc;
              break;
            case 0x1d:
              icon_index = 0xd;
              break;
            case 0x1e:
              icon_index = 0x10;
              break;
            case 0x1f:
              icon_index = 0x11;
              break;
            }
          }
        } else {
          assert_halt_msg_at(
            "FALSE", "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x10f5, 0);
        }
      } else {
        icon_index = remapped_icon_type;
      }

      {
        char *icon;
        unsigned char saved_flags;
        short saved_width_offset;
        real_argb_color icon_color;
        real_argb_color text_color;
        int alpha;

        icon = (char *)tag_block_get_element(*(char **)0x46bd0c + 0xc4,
                                             icon_index, 0x10);
        saved_flags = *(unsigned char *)(icon + 0xd);
        saved_width_offset = *(short *)(icon + 2);
        pixel32_to_real_argb_color(*(unsigned int *)(icon + 8),
                                   (float *)&icon_color);
        *(unsigned char *)(icon + 0xd) &= ~2;
        assert_halt_msg_at("icon_index>=0 && icon_index<NUM_ICONS",
                           "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x110a,
                           icon_index >= 0 && icon_index < 0x12);
        if (*(unsigned char *)(0x31e080 + icon_index) != 0) {
          *(unsigned char *)(icon + 0xd) &= ~4;
          *(short *)(icon + 2) = -5;
        }
        draw_string_get_color(&text_color);
        alpha = (int)(text_color.alpha * 255.0f) << 24;
        if (*(unsigned int *)(icon + 8) == 0 || ignore_icon_color) {
          icon_color = text_color;
        }
        icon_color.red *= text_color.alpha;
        icon_color.green *= text_color.alpha;
        icon_color.blue *= text_color.alpha;
        render_state_bitmap(
          cursor, icon, (int)bounds,
          (int)((real_argb_color_to_pixel32((float *)&icon_color) & 0xffffff) |
                alpha),
          0);
        bounds[1]++;
        *(unsigned char *)(icon + 0xd) = saved_flags;
        *(short *)(icon + 2) = saved_width_offset;
      }
    }
  }
  if (current != NULL) {
    render_state_text(bounds, current, cursor);
  }
  draw_string_set_indents(0, 0);
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
      widget_memory_pool, (int)*(void **)((char *)widget + 0x3c),
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
    color[0] =
      (x87_fcos((float)widget_globals_field_20 * 0.001f * 3.0f) + 1.5f) *
      0.4f * color[0];
  }

  draw_string_set_font(font_tag, -1, justification, 0, color);

  if (string_has_icons_to_draw(*text)) {
    draw_string_and_hack_in_icons(position, (int)bounds, 0, 0, *text, 0);
  } else {
    rasterizer_draw_string(position, bounds, NULL, 0, (unsigned short *)*text);
  }
}

/* widget_instance_render_spinner_list (0x0e6450) — draws a spinner list's
 * header/footer arrow bitmaps and, for a list without child widgets, its
 * current item text.  PAL 2342 ui_widget.c:4994 (names T2); layout evidence is
 * the 2276 disassembly.  definition arrives in ESI (kb @<esi>).
 *
 *  - The cumulative alpha (widget alpha_modifier times that of every ancestor
 *    along the parent chain) is computed inline before the visible check.
 *  - last_list_tab_direction counts back toward zero one step per frame; a
 *    negative value selects header frame 1, a positive one footer frame 1.
 *  - Header/footer: bitmap from the definition's list header/footer bitmap tag
 *    (sequence 0), drawn at the definition's header/footer bounds shifted by
 *    offset (packed {int16 x low; int16 y high}); alpha * 255 is rounded with a
 *    bare FISTP (fast_ftol).
 *  - With no child widgets the item text is either the selected entry of the
 *    text label string list (copied into a widget_memory_pool block,
 *    ui_widget.c line 0x1202, run through the search-and-replace functions
 *    and freed at the end, also when the allocation failed) or the widget's
 *    own list_item_text.
 *  - Text colour: focused items use the UI white RGB with the definition
 *    alpha; the non-focused (1,1,1) comparison selects the same alpha in both
 *    arms, as in PAL.  Flashing-text flag 0x4 pulses the alpha with
 *    (sin(ms * 0.001 * 3) + 1) * 0.5 of widget_globals_field_20. */
void widget_instance_render_spinner_list(void *widget_ptr, void *definition_ptr,
                                         viewport_bounds_t *clip_rect,
                                         int32_t offset, char focus)
{
  widget_instance_t *widget;
  ui_widget_definition_t *definition;
  const widget_instance_t *parent;
  const ui_widget_search_and_replace_reference_t *reference;
  char parameters[0x8c];
  wchar_t search_string[32];
  float white[3];
  real_argb_color color;
  viewport_bounds_t bounds;
  viewport_bounds_t clip;
  wchar_t *item_text;
  wchar_t *replace;
  const wchar_t *string;
  void *bitmap;
  float alpha_modifier;
  float text_alpha_modifier;
  float color_alpha;
  const float *rgb;
  int header_frame_index;
  int footer_frame_index;
  int alpha;
  int length;
  int search_index;
  int16_t last_list_tab_direction;
  /* offset_xy[0] = x (low half), offset_xy[1] = y (high half) */
  const int16_t *offset_xy;

  widget = (widget_instance_t *)widget_ptr;
  definition = (ui_widget_definition_t *)definition_ptr;
  offset_xy = (const int16_t *)&offset;

  header_frame_index = 0;
  footer_frame_index = 0;

  alpha_modifier = widget->alpha_modifier;
  for (parent = widget->parent; parent != NULL; parent = parent->parent) {
    alpha_modifier *= parent->alpha_modifier;
  }

  if (!widget->visible) {
    return;
  }

  last_list_tab_direction = widget->list_last_tab_direction;
  if (last_list_tab_direction != 0) {
    switch (last_list_tab_direction >= 0 ? 1 : -1) {
    case -1:
      widget->list_last_tab_direction = (int16_t)(last_list_tab_direction + 1);
      header_frame_index = 1;
      break;
    case 1:
      widget->list_last_tab_direction = (int16_t)(last_list_tab_direction - 1);
      footer_frame_index = 1;
      break;
    }
  }

  bitmap = FUN_00077040(definition->list_header_bitmap.tag_index, 0,
                        (short)header_frame_index);
  if (bitmap != NULL) {
    alpha = x87_round_to_int(alpha_modifier * 255.0f);
    csmemset(parameters, 0, sizeof(parameters));
    bounds = definition->list_header_bounds;
    bounds.x0 += offset_xy[0];
    bounds.y0 += offset_xy[1];
    bounds.x1 += offset_xy[0];
    bounds.y1 += offset_xy[1];
    draw_bitmap_in_rect((int)bitmap, (int16_t *)&bounds, (int16_t *)&bounds,
                        (int16_t *)clip_rect, (alpha << 24) | 0x00ffffff,
                        (int)parameters, 0); /* dup-args-ok */
  }

  bitmap = FUN_00077040(definition->list_footer_bitmap.tag_index, 0,
                        (short)footer_frame_index);
  if (bitmap != NULL) {
    alpha = x87_round_to_int(alpha_modifier * 255.0f);
    csmemset(parameters, 0, sizeof(parameters));
    bounds = definition->list_footer_bounds;
    bounds.x0 += offset_xy[0];
    bounds.y0 += offset_xy[1];
    bounds.x1 += offset_xy[0];
    bounds.y1 += offset_xy[1];
    draw_bitmap_in_rect((int)bitmap, (int16_t *)&bounds, (int16_t *)&bounds,
                        (int16_t *)clip_rect, (alpha << 24) | 0x00ffffff,
                        (int)parameters, 0); /* dup-args-ok */
  }

  if (definition->child_widgets.count != 0) {
    return;
  }

  if (definition->text_label_string_list.tag_index != -1) {
    string = (const wchar_t *)FUN_0019d420(
      definition->text_label_string_list.tag_index,
      (int)widget->list_selected_index);
    length = ustrlen((const unsigned short *)string) * 2;
    item_text = (wchar_t *)stack_memory_pool_allocate(
      widget_memory_pool, length + 2,
      "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x1202);
    if (item_text != NULL) {
      csmemcpy(item_text, (void *)string, (size_t)length);
      *(wchar_t *)((char *)item_text + length) = 0;
      for (search_index = 0;
           search_index < definition->search_and_replace_functions.count;
           search_index++) {
        reference = (const ui_widget_search_and_replace_reference_t *)
                      definition->search_and_replace_functions.address +
                    search_index;
        if (reference != NULL && reference->search_string[0] != '\0') {
          replace = ui_widget_search_and_replace_invoke(
            widget, reference->replace_function);
          search_and_replace(ascii_to_wide(reference->search_string,
                                           search_string,
                                           sizeof(search_string)),
                             replace, &item_text);
        }
      }
    }
  } else {
    item_text = widget->list_item_text;
  }

  if (item_text != NULL) {
    if (definition->text_font.tag_index == -1) {
      error(
        2,
        "failed to render spinner list item because the font tag was invalid");
    } else if (definition->justification < 0 ||
               definition->justification >= 3) {
      error(2, "failed to render spinner list item because the justification "
               "was invalid");
    } else {
      text_alpha_modifier =
        widget_instance_get_cumulative_alpha_modifier(widget);
      clip = clip_rect != NULL ? *clip_rect : definition->bounds;



      bounds = definition->bounds;
      bounds.x1 += offset_xy[0];
      bounds.y1 += offset_xy[1];
      bounds.x0 += offset_xy[0];
      bounds.y0 += offset_xy[1];
      if (focus) {
        color.alpha = definition->text_color.alpha;
        rgb = get_ui_rgb_white(white);
        color.red = rgb[0];
        color.green = rgb[1];
        color.blue = rgb[2];
        color_alpha = color.alpha;
      } else {
        color = definition->text_color;
        if (1.0f == color.red && 1.0f == color.green && 1.0f == color.blue) {
          color_alpha = definition->text_color.alpha;
        } else {
          color_alpha = color.alpha;
        }
      }
      color.alpha = text_alpha_modifier * color_alpha;
      if ((definition->text_box_flags & UI_TEXT_BOX_FLASHING_TEXT_FLAG) != 0) {
        color.alpha =
          color.alpha *
          ((x87_fsin(3.0f * ((float)widget_globals_field_20 * 0.001f)) + 1.0f) * 0.5f);
      }
      draw_string_set_font(definition->text_font.tag_index, -1,
                           (uint16_t)definition->justification, 0, &color);
      rasterizer_draw_string(&bounds, (short *)&clip, NULL, 0,
                             (unsigned short *)item_text);
    }
  }

  if (definition->text_label_string_list.tag_index != -1) {
    stack_memory_pool_deallocate(widget_memory_pool, item_text);
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

/* widget_instance_go_back_to_previous (0xe68e0) — widget arrives in EAX
 * (kb.json @<eax>). Pops the history entry of the widget's player stack
 * (slot 0 when the index is NONE), deletes the widget's topmost parent (walk
 * inlined in the binary, no CALL to 0xe4310), then reloads the previous
 * screen and restores its focused child. */
void widget_instance_go_back_to_previous(void *widget_ptr)
{
  widget_instance_t *widget;
  widget_instance_t *topmost;
  void *new_widget;
  widget_stack_data_t data;
  int16_t widget_stack;
  int16_t previous_local_player_index;
  int previous_widget_tag;

  widget = (widget_instance_t *)widget_ptr;
  widget_stack =
    (widget->local_player_index == -1) ? 0 : widget->local_player_index;
  previous_local_player_index = -1;
  if (widget_globals_field_10[widget_stack] != 0) {
    pop_widget(&widget_globals_field_10[widget_stack], &data);
    previous_local_player_index = data.local_player_index;
    previous_widget_tag = data.previous_widget_tag;
  } else {
    previous_widget_tag = -1;
  }
  topmost = widget;
  while (topmost->parent != NULL) {
    topmost = topmost->parent;
  }
  ui_widget_delete(topmost);
  if (previous_widget_tag != -1) {
    new_widget = ui_widget_load_by_name_or_tag(
      NULL, previous_widget_tag, 0, previous_local_player_index, -1, -1, -1);
    if (new_widget != NULL) {
      widget_instance_set_focused_child_by_index(
        data.focused_child_parent_widget_tag, (int)new_widget,
        data.focused_child_index);
    }
  }
}

/* perform_filesystem_initialization — spawns a background thread to perform
 * filesystem and saved-game file enumeration. Asserts no initialization
 * thread is running and the widget subsystem is initialized. Suppresses UI
 * events (field_65 = 1) and resets the filesystem check result (field_60)
 * before thread_new (0x81630). If thread creation fails, runs the check
 * procedure synchronously (0xe5590) and re-clears the suppress flag. */
void perform_filesystem_initialization(void)
{
  assert_halt_msg_at("widget_globals.initialization_thread==NULL",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x153f,
                     widget_globals_initialization_thread == NULL);
  error(2, "begining filesystem checks & saved game file enumeration...");
  assert_halt_msg_at("widget_globals.initialized",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x496,
                     widget_globals_initialized);
  widget_globals_field_65 = 1;
  widget_globals_field_60 = 0;
  if (!thread_new(0, (void *)0xe5590, 0,
                  &widget_globals_initialization_thread)) {
    error(2, "failed to spawn thread for filesystem checks - running "
             "synchronously!");
    widget_globals_initialization_thread = NULL;
    filesystem_initialization_thread_proc(0);
    assert_halt_msg_at("widget_globals.initialized",
                       "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x496,
                       widget_globals_initialized);
    widget_globals_field_65 = 0;
  }
}

/* ui_widgets_dispose — tears down the UI widget system at engine shutdown.
 * Closes all open widgets, frees widget_memory_pool's 0x4000-byte block
 * (at [ptr+4]), zeros the pool pointer and size fields, and clears
 * widget_globals. */
void ui_widgets_dispose(void)
{
  ui_widgets_close_all();

  if (((int *)widget_memory_pool)[1] != 0) {
    debug_free((void *)((int *)widget_memory_pool)[1],
               "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x76);
  }
  ((int *)widget_memory_pool)[1] = 0;
  ((int *)widget_memory_pool)[2] = 0;
  csmemset((void *)widget_globals_base, 0, WIDGET_GLOBALS_SIZE);
}

/* widget_event_function_list_widget_goto_next_item (0xe6ab0) — dpad-down /
 * event-function-table handler (table 0x31e158) for spinner (type 2) and
 * column (type 3) list widgets. Only widget ([EBP+8]) is read; event_data and
 * widget_deleted are the table signature. Returns AL=1, or AL=0 when a column
 * list has no child to focus. With list items the selected index wraps to 0
 * past number_of_items; without them focus walks the child chain. Sets the
 * tab direction (+0x3e) to +15 on success. Names follow PAL 2342 (T2). */
bool widget_event_function_list_widget_goto_next_item(void *widget_ptr,
                                                      void *event_data,
                                                      char *widget_deleted)
{
  widget_instance_t *widget;
  ui_widget_definition_t *definition;
  widget_instance_t *child;
  int item_index;
  bool result;

  (void)event_data;
  (void)widget_deleted;
  widget = (widget_instance_t *)widget_ptr;
  result = true;
  if (widget == NULL) {
    display_assert("widget", "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x4ce,
                   true);
    system_exit(-1);
  }
  definition = (ui_widget_definition_t *)tag_get(0x44654c61 /* 'DeLa' */,
                                                 widget->definition_tag_index);
  if (widget->type != UI_WIDGET_TYPE_SPINNER_LIST &&
      widget->type != UI_WIDGET_TYPE_COLUMN_LIST) {
    display_assert("calling a list widget function on a non-list widget",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x4d1, true);
    system_exit(-1);
  }
  if (widget->list_items != NULL && widget->list_number_of_items > 0) {
    item_index = widget->list_selected_index + 1;
    if (item_index >= widget->list_number_of_items) {
      item_index = 0;
    }
    if (widget->type == UI_WIDGET_TYPE_COLUMN_LIST) {
      child =
        (widget_instance_t *)widget_instance_get_nth_child(widget, item_index);
      if (child != NULL) {
        widget_instance_give_focus_by_tag(widget, child->definition_tag_index,
                                          widget->local_player_index);
        widget->list_selected_index = (int16_t)item_index;
      } else {
        error(
          2, "failed to set focus to the #%d list item of a column list widget",
          item_index);
        result = false;
      }
    } else if (widget->type == UI_WIDGET_TYPE_SPINNER_LIST) {
      if (definition->child_widgets.count > 1) {
        if (definition->child_widgets.count != 3) {
          display_assert("spinner lists must be either 1- or 3-wide... sorry",
                         "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x4f4,
                         true);
          system_exit(-1);
        }
        if (widget->focused_child == widget->child ||
            widget->focused_child == widget->child->next) {
          if (widget->focused_child->next != NULL) {
            widget_instance_give_focus_directly(widget,
                                                widget->focused_child->next);
          }
        }
      }
      widget->list_selected_index = (int16_t)item_index;
    }
  } else {
    if (widget->type == UI_WIDGET_TYPE_SPINNER_LIST &&
        definition->child_widgets.count > 1) {
      display_assert("spinner lists with more that 1 visible item need to have "
                     "code-generated lists associated with them... sorry.",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x508, true);
      system_exit(-1);
    }
    if (widget->type == UI_WIDGET_TYPE_SPINNER_LIST &&
        (definition->list_flags &
         UI_LIST_ITEMS_GENERATED_FROM_STRING_LIST_TAG_FLAG) != 0 &&
        definition->child_widgets.count == 0) {
      widget->list_selected_index++;
      if (widget->list_selected_index == widget->list_number_of_items) {
        widget->list_selected_index = 0;
      }
    } else {
      child = NULL;
      if (widget->focused_child != NULL) {
        item_index = widget->list_selected_index + 1;
        child = widget->focused_child->next;
        if (item_index == widget->list_number_of_items) {
          child = NULL;
        }
      }
      if (child == NULL) {
        child = widget->child;
        item_index = 0;
      }
      if (child != NULL) {
        widget_instance_give_focus_by_tag(widget, child->definition_tag_index,
                                          widget->local_player_index);
        widget->list_selected_index = (int16_t)item_index;
      } else {
        error(2,
              "failed to set focus to the next list item of a column widget");
        result = false;
      }
    }
  }
  if (result) {
    widget->list_last_tab_direction = 15;
  }
  return result;
}

/* widget_event_function_list_widget_goto_previous_item (0xe6cb0) — dpad-up
 * mirror of the function above: the selected index wraps to
 * number_of_items - 1 below 0, the spinner path focuses the previous
 * visible item, and without list items focus walks back through previous
 * links (to the tail child when there is none). Returns AL=0 only when a
 * column list has no child at the new index; otherwise sets the tab
 * direction (+0x3e) to -15 and returns AL=1. */
bool widget_event_function_list_widget_goto_previous_item(void *widget_ptr,
                                                          void *event_data,
                                                          char *widget_deleted)
{
  widget_instance_t *widget;
  ui_widget_definition_t *definition;
  widget_instance_t *child;
  int item_index;

  (void)event_data;
  (void)widget_deleted;
  widget = (widget_instance_t *)widget_ptr;
  if (widget == NULL) {
    display_assert("widget", "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x54f,
                   true);
    system_exit(-1);
  }
  definition = (ui_widget_definition_t *)tag_get(0x44654c61 /* 'DeLa' */,
                                                 widget->definition_tag_index);
  if (widget->type != UI_WIDGET_TYPE_SPINNER_LIST &&
      widget->type != UI_WIDGET_TYPE_COLUMN_LIST) {
    display_assert("calling a list widget function on a non-list widget",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x552, true);
    system_exit(-1);
  }
  if (widget->list_items != NULL && widget->list_number_of_items > 0) {
    item_index = widget->list_selected_index - 1;
    if (item_index < 0) {
      item_index = widget->list_number_of_items - 1;
    }
    if (widget->type == UI_WIDGET_TYPE_COLUMN_LIST) {
      child =
        (widget_instance_t *)widget_instance_get_nth_child(widget, item_index);
      if (child != NULL) {
        widget_instance_give_focus_by_tag(widget, child->definition_tag_index,
                                          widget->local_player_index);
        widget->list_selected_index = (int16_t)item_index;
      } else {
        error(2, "failed to set focus to the #%d list item of a column list "
                 "widget", item_index);
        return false;
      }
    } else if (widget->type == UI_WIDGET_TYPE_SPINNER_LIST) {
      if (definition->child_widgets.count > 1) {
        if (definition->child_widgets.count != 3) {
          display_assert("spinner lists must be either 1- or 3-wide... sorry",
                         "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x574,
                         true);
          system_exit(-1);
        }
        if (widget->focused_child != widget->child) {
          if (widget->focused_child == NULL) {
            display_assert("widget->focused_child",
                           "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x57d,
                           true);
            system_exit(-1);
          }
          if (widget->focused_child->previous != NULL) {
            widget_instance_give_focus_directly(
              widget, widget->focused_child->previous);
          }
        }
      }
      widget->list_selected_index = (int16_t)item_index;
    }
  } else {
    if (widget->type == UI_WIDGET_TYPE_SPINNER_LIST &&
        definition->child_widgets.count > 1) {
      display_assert("spinner lists with more that 1 visible item need to have "
                     "code-generated lists associated with them... sorry.",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x58c, true);
      system_exit(-1);
    }
    if (widget->type == UI_WIDGET_TYPE_SPINNER_LIST &&
        (definition->list_flags &
         UI_LIST_ITEMS_GENERATED_FROM_STRING_LIST_TAG_FLAG) != 0 &&
        definition->child_widgets.count == 0) {
      widget->list_selected_index--;
      if (widget->list_selected_index < 0) {
        widget->list_selected_index =
          (int16_t)(widget->list_number_of_items - 1);
      }
    } else {
      child = NULL;
      if (widget->focused_child != NULL) {
        item_index = widget->list_selected_index - 1;
        child = widget->focused_child->previous;
      }
      if (child == NULL) {
        /* no NULL check on widget->child here in the original */
        child = widget->child;
        item_index = 0;
        while (child->next != NULL) {
          child = child->next;
          item_index++;
        }
      }
      widget_instance_give_focus_by_tag(widget, child->definition_tag_index,
                                        widget->local_player_index);
      widget->list_selected_index = (int16_t)item_index;
    }
  }
  widget->list_last_tab_direction = -15;
  return true;
}

/* event_handler_dispatch (0xe6ed0) — runs one ui_widget_event_handler_reference
 * for widget: optional scenario script, optional event-handler function
 * (0xe9810), then (unless the function failed) the focus / reload / close /
 * open / replace / go-back actions selected by handler->flags, the handler's
 * sound, and deferred closes. On failure it tries the definition's
 * conditional widgets, then plays the audio feedback sound (inlined switch in
 * the binary) and reports whether the calling widget was deleted. Names and
 * shape follow PAL 2342 (T2). Differences from PAL observed in the 2276
 * disassembly: there is no reload_self (0x10) handling, and reload_widget
 * (0x20) only reports the NONE-tag error. audio_feedback is a short (MOVSX
 * word at 0xe7350). */
void event_handler_dispatch(void *widget_ptr, void *definition_ptr,
                            void *event_data, void *event_handler,
                            char *calling_widget_deleted)
{
  widget_instance_t *widget;
  ui_widget_definition_t *definition;
  ui_widget_event_handler_reference_t *handler;
  ui_widget_conditional_reference_t *conditional;
  widget_instance_t *other_widget;
  widget_instance_t *new_widget;
  widget_instance_t *parent;
  widget_instance_t *next;
  widget_instance_t *previous;
  widget_instance_t *topmost;
  widget_stack_data_t data;
  int widget_index;
  int conditional_index;
  int audio_feedback;
  bool widget_deleted;
  bool success;
  bool function_failed;
  bool close_widget_after;
  bool close_current;
  bool close_all;

  widget = (widget_instance_t *)widget_ptr;
  definition = (ui_widget_definition_t *)definition_ptr;
  handler = (ui_widget_event_handler_reference_t *)event_handler;
  widget_deleted = false;
  success = true;
  function_failed = false;
  audio_feedback = 0;
  close_widget_after = false;
  close_current = false;
  close_all = false;

  if ((handler->flags & UI_EVENT_HANDLER_RUN_SCENARIO_SCRIPT_FLAG) != 0 &&
      handler->script[0] != '\0') {
    if (!hs_evaluate_by_name(handler->script)) {
      error(2, "failed to run ui widget event script '%s'", handler->script);
    }
  }
  if ((handler->flags & UI_EVENT_HANDLER_RUN_FUNCTION_FLAG) != 0 &&
      !widget_deleted &&
      !ui_widget_event_handler_function_invoke(
        widget, (int)event_data, handler->function, &widget_deleted)) {
    error(2, "event handler function failed");
    function_failed = true;
  } else {
    if ((handler->flags & UI_EVENT_HANDLER_GIVE_FOCUS_TO_WIDGET_FLAG) != 0 &&
        !widget_deleted) {
      if (handler->widget_tag.tag_index != -1) {
        widget_instance_give_focus_by_tag(widget, handler->widget_tag.tag_index,
                                          widget->local_player_index);
        audio_feedback = 1;
      } else {
        error(2, "failed to give focus to a widget because "
                 "event_handler->ui_widget_tag == NONE");
        success = false;
      }
    }
    if ((handler->flags & UI_EVENT_HANDLER_RELOAD_WIDGET_FLAG) != 0 &&
        !widget_deleted && handler->widget_tag.tag_index == -1) {
      error(2, "failed to reload widget because event_handler->ui_widget_tag "
               "== NONE");
      success = false;
    }
    if ((handler->flags & UI_EVENT_HANDLER_CLOSE_CURRENT_WIDGET_FLAG) != 0 &&
        !widget_deleted) {
      close_widget_after = true;
    }
    if ((handler->flags & UI_EVENT_HANDLER_CLOSE_OTHER_WIDGET_FLAG) != 0 &&
        !widget_deleted && handler->widget_tag.tag_index != -1) {
      /* PAL widget_instance_find_by_tag_index, inlined in the binary */
      other_widget = NULL;
      for (widget_index = 0; widget_index < 4 && other_widget == NULL;
           widget_index++) {
        if ((int *)widget_globals_field_00[widget_index] != NULL) {
          other_widget =
            (widget_instance_t *)widget_instance_find_by_tag_index_recursive(
              (int *)widget_globals_field_00[widget_index],
              handler->widget_tag.tag_index);
        }
      }
      if (other_widget != NULL) {
        if (other_widget == widget) {
          close_current = true;
        } else {
          ui_widget_delete(other_widget);
        }
      } else {
        error(2, "failed to close widget because event_handler->ui_widget_tag "
                 "== NONE");
        success = false;
      }
    }
    if ((handler->flags & UI_EVENT_HANDLER_CLOSE_ALL_WIDGETS_FLAG) != 0 &&
        !widget_deleted) {
      close_all = true;
    }
    if ((handler->flags & UI_EVENT_HANDLER_OPEN_WIDGET_FLAG) != 0 &&
        handler->widget_tag.tag_index != -1) {
      if (ui_widget_launch_widget(widget, handler->widget_tag.tag_index) ==
          NULL) {
        error(2, "event handler failed to spawn widget");
        success = false;
      } else {
        if (audio_feedback == 0) {
          audio_feedback = 2;
        }
        widget_deleted = true;
      }
    }
    if ((handler->flags & UI_EVENT_HANDLER_REPLACE_WITH_OTHER_WIDGET_FLAG) !=
          0 &&
        !widget_deleted && handler->widget_tag.tag_index != -1) {
      new_widget = (widget_instance_t *)ui_widget_load_by_name_or_tag(
        NULL, handler->widget_tag.tag_index, (int)widget,
        widget->local_player_index, -1, -1, -1);
      if (new_widget != NULL) {
        parent = widget->parent;
        next = widget->next;
        previous = widget->previous;
        if (new_widget->previous != NULL) {
          new_widget->previous->next = NULL;
        }
        new_widget->previous = NULL;
        new_widget->parent = NULL;
        new_widget->horizontal_offset += widget->horizontal_offset;
        new_widget->vertical_offset += widget->vertical_offset;
        if (parent != NULL) {
          new_widget->parent = parent;
          if (parent->child == widget) {
            parent->child = new_widget;
          }
          if (parent->focused_child == widget) {
            parent->focused_child = new_widget;
          }
        }
        if (next != NULL) {
          if (next->previous != widget) {
            display_assert("next->previous == widget",
                           "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0xe89,
                           true);
            system_exit(-1);
          }
          next->previous = new_widget;
        }
        new_widget->next = next;
        if (previous != NULL) {
          if (previous->next != widget) {
            display_assert("previous->next == widget",
                           "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0xe90,
                           true);
            system_exit(-1);
          }
          previous->next = new_widget;
        }
        new_widget->previous = previous;
        for (widget_index = 0; widget_index < 4; widget_index++) {
          if ((widget_instance_t *)widget_globals_field_00[widget_index] ==
              new_widget) {
            widget_globals_field_00[widget_index] = 0;
            break;
          }
        }
        if (audio_feedback == 0) {
          audio_feedback = 2;
        }
        widget->previous = NULL;
        widget->next = NULL;
        widget->parent = NULL;
        close_current = true;
      } else {
        error(2, "failed to open widget because the specified widget tag was "
                 "not found");
        success = false;
      }
    }
    if ((handler->flags & UI_EVENT_HANDLER_GO_BACK_TO_PREVIOUS_WIDGET_FLAG) !=
        0) {
      widget_instance_go_back_to_previous(widget);
      if (audio_feedback == 0) {
        audio_feedback = 3;
      }
      widget_deleted = true;
    }
    if (handler->sound_effect.tag_index != -1) {
      sound_impulse_start(handler->sound_effect.tag_index, 1.0f);
    }
    if (close_all) {
      for (widget_index = 0; widget_index < 4; widget_index++) {
        if ((widget_instance_t *)widget_globals_field_00[widget_index] !=
            NULL) {
          ui_widget_delete(
            (widget_instance_t *)widget_globals_field_00[widget_index]);
        }
        /* pop_widget is inlined here in the binary (assert line 0x9fc) */
        while (widget_globals_field_10[widget_index] != 0) {
          pop_widget(&widget_globals_field_10[widget_index], &data);
        }
      }
      widget_deleted = true;
    } else if (close_widget_after) {
      topmost = widget;
      while (topmost->parent != NULL) {
        topmost = topmost->parent;
      }
      ui_widget_delete(topmost);
      widget_deleted = true;
    } else if (close_current) {
      ui_widget_delete(widget);
      widget_deleted = true;
    }
  }
  if (!success || function_failed) {
    if ((handler->flags &
         UI_EVENT_HANDLER_LOOK_FOR_CONDITIONAL_WIDGET_ON_FAILURE_FLAG) != 0) {
      for (conditional_index = 0;
           conditional_index < definition->conditional_widgets.count;
           conditional_index++) {
        conditional = (ui_widget_conditional_reference_t *)
                        definition->conditional_widgets.address +
                      conditional_index;
        if (function_failed == true &&
            (conditional->flags &
             UI_CONDITIONAL_WIDGET_LOAD_IF_FUNCTION_FAILS_FLAG) != 0) {
          if (!widget_deleted) {
            if (conditional->widget_tag.tag_index != -1) {
              if (ui_widget_launch_widget(
                    widget, conditional->widget_tag.tag_index) == NULL) {
                error(2, "condition handler failed to spawn widget");
              } else {
                widget_deleted = true;
              }
            }
          } else {
            error(2, "couldn't load conditional widget because the calling "
                     "widget was deleted");
          }
        }
      }
    }
  }
  ui_play_audio_feedback_sound(audio_feedback);
  *calling_widget_deleted = (char)widget_deleted;
}

/* widget_instance_render_recursive (0xe73c0) — renders one widget and, unless
 * its list type draws them itself, all of its children.  Parameter names follow
 * the PAL 2342 source (T2); layout evidence is the 2276 disassembly.
 *
 *  - definition = tag_get('DeLa', widget+0x00).
 *  - The cumulative alpha (widget+0x24 times +0x24 of every ancestor along the
 *    +0x30 parent chain) is computed inline here (2276 inlines
 *    widget_instance_get_cumulative_alpha_modifier).
 *  - Definition flag 0x2000 at +0x2c forces use_nifty_plasma_fx on.
 *  - offset is a packed {int16 x; int16 y} pair passed by value; the widget's
 *    own +0x0a (x) / +0x0c (y) are added to it before anything else.
 *  - Every game-data input (count +0x48, base +0x4c, stride 0x24, uint16
 *    function index at +0) is invoked, even for invisible widgets.
 *  - Nothing else happens unless the visible byte at widget+0x10 is set.
 *  - Background bitmap: tag +0x44, frame index widget+0x50.  While drawing
 *    with plasma fx ui_plasma_effect_color is set to {0, 0.05, 0.05, 0.05}
 *    and then cleared.  Definition flag 0x4 at +0x2c pulses the alpha with
 *    (cos(ms * 0.001 * 3) + 1) * 0.5 of the unsigned UI millisecond clock
 *    widget_globals_field_20.  The alpha * 255 is rounded with a bare FISTP.
 *    The 'bitm' sequence-block lookup (+0x54, element 0, size 0x40) has its
 *    result discarded, as in the binary.
 *  - Type at widget+0x0e: 1 text box, 2 spinner list (skips children when
 *    list flag 0x2 at +0x150 is set and the child-widget count at +0x3e0 is
 *    zero), 3 column list (skips children when list flag 0x1 is set).
 *  - Children: head +0x34, next +0x2c; focus is child == widget+0x38, and the
 *    child gets plasma fx when focused inside a spinner or column list.
 *    The binary stores focus / use_nifty_plasma_fx back into the low byte of
 *    their own argument slots before each recursive call. */
void widget_instance_render_recursive(int widget_address,
                                      viewport_bounds_t *clip_rect,
                                      int32_t offset, char focus,
                                      char use_nifty_plasma_fx)
{
  char parameters[0x8c];
  viewport_bounds_t bounds;
  viewport_bounds_t clipped;
  viewport_bounds_t *clip;
  int16_t *offset_xy;
  widget_instance_t *widget = (widget_instance_t *)widget_address;
  ui_widget_definition_t *definition;
  widget_instance_t *parent;
  widget_instance_t *child;
  int input_index;
  int bitmap;
  float alpha_modifier;
  float alpha;
  int alpha_byte;
  boolean render_children;

  definition = (ui_widget_definition_t *)tag_get(0x44654c61 /* 'DeLa' */,
                                                 widget->definition_tag_index);
  alpha_modifier = widget->alpha_modifier;
  for (parent = widget->parent; parent != NULL; parent = parent->parent) {
    alpha_modifier *= parent->alpha_modifier;
  }
  render_children = 1;
  if (!use_nifty_plasma_fx &&
      ((uint32_t)definition->flags & 0x2000) != 0) {
    use_nifty_plasma_fx = 1;
  }

  /* offset_xy[0] = x (low half), offset_xy[1] = y (high half) */
  offset_xy = (int16_t *)&offset;
  offset_xy[0] += widget->horizontal_offset;
  offset_xy[1] += widget->vertical_offset;
  for (input_index = 0; input_index < definition->field_48; input_index++) {
    ui_widget_game_data_function_invoke(
      widget,
      *(uint16_t *)((char *)definition->field_4c + input_index * 0x24));
  }

  if (widget->visible == 0) {
    return;
  }

  bitmap = (int)FUN_00077040(definition->field_44, 0,
                             (short)(uint16_t)widget->field_50);
  if (bitmap != 0) {
    alpha = alpha_modifier;
    bounds = definition->bounds;
    clip = clip_rect;
    tag_block_get_element(
      (char *)tag_get(0x6269746d, definition->field_44) + 0x54, 0, 0x40);
    if (use_nifty_plasma_fx) {
      ui_plasma_effect_color.alpha = 0.0f;
      ui_plasma_effect_color.red = 0.05f;
      ui_plasma_effect_color.green = 0.05f;
      ui_plasma_effect_color.blue = 0.05f;
    }
    bounds.x0 += offset_xy[0];
    bounds.x1 += offset_xy[0];
    bounds.y0 += offset_xy[1];
    bounds.y1 += offset_xy[1];
    if (clip_rect != NULL) {
      clipped = *clip_rect;
      clipped.x0 += offset_xy[0];
      clipped.y0 += offset_xy[1];
      clipped.x1 += offset_xy[0];
      clipped.y1 += offset_xy[1];
      clip = &clipped;
    }
    if ((definition->flags & 4) != 0) {
      alpha = alpha_modifier *
              ((x87_fcos(3.0f * ((float)widget_globals_field_20 * 0.001f)) + 1.0f) * 0.5f);
    }
    alpha *= 255.0f;
    alpha_byte = x87_round_to_int(alpha);
    /* 0xe758d/0xe7591 push LEA [EBP-0xc] twice: bounds is both src and dst */
    draw_bitmap_in_rect(bitmap, (int16_t *)&bounds, (int16_t *)&bounds,
                        (int16_t *)clip, (alpha_byte << 24) | 0xffffff,
                        (int)parameters, 0); /* dup-args-ok */
    if (use_nifty_plasma_fx) {
      ui_plasma_effect_color.alpha = 0.0f;
      ui_plasma_effect_color.red = 0.0f;
      ui_plasma_effect_color.green = 0.0f;
      ui_plasma_effect_color.blue = 0.0f;
    }
  }

  switch (widget->type) {
  case 1:
    widget_instance_render_text_box(definition, widget,
                                    (const int32_t *)clip_rect, offset,
                                    widget_instance_text_box_is_focused(widget));
    break;
  case UI_WIDGET_TYPE_SPINNER_LIST:
    widget_instance_render_spinner_list(widget, definition, clip_rect, offset,
                                        focus);
    if ((definition->list_flags &
         UI_LIST_ITEMS_GENERATED_FROM_STRING_LIST_TAG_FLAG) != 0 &&
        definition->child_widgets.count == 0) {
      render_children = 0;
    }
    break;
  case UI_WIDGET_TYPE_COLUMN_LIST:
    widget_instance_render_column_list((int)widget, (int)definition, clip_rect,
                                       offset, focus);
    render_children = (definition->list_flags & 1) == 0;
    break;
  }

  if (render_children) {
    for (child = widget->child; child != NULL; child = child->next) {
      focus = child == widget->focused_child;
      use_nifty_plasma_fx =
        focus && (widget->type == UI_WIDGET_TYPE_SPINNER_LIST ||
                  widget->type == UI_WIDGET_TYPE_COLUMN_LIST);
      widget_instance_render_recursive((int)child, clip_rect, offset, focus,
                                       use_nifty_plasma_fx);
    }
  }
}

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

  target = (int)((widget_instance_t *)widget)->field_48;
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

/* 0xe7760 — render_ui_widgets_postgame. Same per-slot visibility filter as
 * render_ui_widgets over the 4 root slots at 0x46cc20, but bails when the
 * virtual keyboard is active and passes a per-(local player count, player)
 * packed int16 pair from a 4x4 stack table as arg3 of
 * widget_instance_render_recursive (the binary reads each pair as one dword
 * at [ebp+idx*4-0x58], idx = player + count*4). The meaning of the two
 * halves (0/0xf0/0x140 values) is unproven. */
void render_ui_widgets_postgame(__int16 local_player_index, __int16 *bounds)
{
  int16_t player_offsets[4][4][2] = {
    { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { 0, 0 }, { 0, 0xf0 }, { 0, 0 }, { 0, 0 } },
    { { 0, 0 }, { 0, 0xf0 }, { 0x140, 0xf0 }, { 0, 0 } },
    { { 0, 0 }, { 0x140, 0 }, { 0, 0xf0 }, { 0x140, 0xf0 } }
  };
  viewport_bounds_t local_bounds;
  int widget;
  int i;

  if (virtual_keyboard_active()) {
    return;
  }

  local_player_index =
    local_player_index < 0
      ? 0
      : (local_player_index > 3 ? 3
                                : local_player_index);

  for (i = 0; i < 4; i++) {
    widget = widget_globals_field_00[i];
    if (widget == 0)
      continue;

    if (((widget_instance_t *)widget)->field_11 != 1) {
      if (((widget_instance_t *)widget)->field_15 == 1) {
        if (*(int16_t *)(widget + 0x8) != local_player_index &&
            *(int16_t *)(widget + 0x8) != -1 && local_player_index != -1 &&
            byte_46CC88 == 0)
          continue;
      } else {
        if (!(*(int16_t *)(widget + 0x8) == -1 && i == 0) &&
            *(int16_t *)(widget + 0x8) != local_player_index)
          continue;
      }
    }

    local_bounds.x0 = 0;
    local_bounds.y0 = 0;
    local_bounds.x1 = bounds[3] - bounds[1];
    local_bounds.y1 = bounds[2] - bounds[0];
    widget_instance_render_recursive(
      widget_globals_field_00[i], &local_bounds,
      *(int *)player_offsets[local_player_count() - 1][local_player_index], 1,
      0);
  }
}

/* render_ui_widgets — renders all active UI widget stacks and an optional
 * screen fade overlay. For each of the 4 widget root slots
 * (widget_globals_field_00), determines whether the widget should render
 * based on its local player index vs the current player, the "always render"
 * flag at +0x11, and the "in_game_mode" flag at +0x15, then renders the tree
 * via widget_instance_render_recursive (plus its tag name in a small debug
 * font when widget_globals_field_64 is set). Afterwards a fade value
 * (widget_globals_field_2c) in [0.0, 1.0] draws a fullscreen fade rectangle
 * (alpha = fade * 255 in the high byte of an ARGB color); fade >= 0.95 is
 * clamped to 1.0. The fade value is initialised to -1.0 (inactive). */
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

  assert_halt_at("c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x38f,
                 window_bounds != NULL);

  /* store clamped player index: -1 maps to 0, otherwise keep player_index */
  *(uint16_t *)0x5aa45c = (uint16_t)(((player_index == -1) - 1) & player_index);

  /* if network loading screen is active, bail */
  if (((char (*)(void))0x1c5960)() != 0) {
    return;
  }

  /* if virtual keyboard is active, render it and return */
  if (virtual_keyboard_active()) {
    virtual_keyboard_render();
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
    widget = widget_globals_field_00[i];
    if (widget == 0)
      continue;

    /* always-render flag at widget+0x11 */
    if (((widget_instance_t *)widget)->field_11 == 1) {
      goto do_render;
    }

    /* in-game mode flag at widget+0x15 */
    if (((widget_instance_t *)widget)->field_15 == 1) {
      uint16_t widget_player = *(uint16_t *)(widget + 0x8);
      if (widget_player == (uint16_t)clamped_player)
        goto do_render;
      if (widget_player == 0xffff)
        goto do_render;
      if ((uint16_t)clamped_player == 0xffff)
        goto do_render;
      if (byte_46CC88 != 0)
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

    widget_instance_render_recursive(widget, &local_bounds, 0, 1, 0);

    /* debug overlay: draw widget tag name */
    if (widget_globals_field_64 != 0) {
      local_bounds.x0 += 0x20;
      local_bounds.x1 += 0x20;
      local_bounds.y0 += 0x20;
      local_bounds.y1 += 0x20;
      color[0] = 1.0f;
      color[1] = 1.0f;
      color[2] = 1.0f;
      color[3] = 1.0f;
      font_tag = tag_loaded(0x666f6e74, "ui\\small_ui");
      draw_string_set_font(font_tag, -1, 0, 0, color);
      tag_name = tag_get_name(*(int *)widget_globals_field_00[i]);
      rasterizer_text_draw(&local_bounds, 0, 0, 0, tag_name);
    }
  }

  /* screen fade overlay */
  fade = widget_globals_field_2c;
  if (fade >= 0.0f && fade <= 1.0f) {
    local_bounds.y0 = 0;
    local_bounds.x1 = 0x280;
    local_bounds.x0 = 0;
    local_bounds.y1 = 0x1e0;
    if (fade >= *(float *)0x255ed4) { /* 0.95f */
      widget_globals_field_2c = 1.0f;
    }
    {
      int alpha =
        (int)(widget_globals_field_2c * *(float *)0x2602c8); /* * 255.0 */
      draw_quad((int16_t *)&local_bounds, alpha << 24);
    }
  }
}

/* widget_instance_initialize (0xe7b10) — Initialize one 0x58-byte widget
 * instance from its DeLa definition, load its children, dispatch creation
 * handlers, assign initial focus, and apply pause side effects.
 *
 * The register ABI is definition@EAX, widget@EDX, parent@ECX. All instance
 * offsets and definition offsets below are from the 2276 disassembly. PAL 2342
 * ui_widget.c supplies the T2 semantic field names.
 */
void widget_instance_initialize(void *definition_ptr, void *widget_ptr,
                                void *parent_ptr, int tag_index,
                                int local_player_index, int widget_stack)
{
  ui_widget_definition_t *definition;
  widget_instance_t *widget;
  widget_instance_t *parent;
  int handler_index;

  definition = (ui_widget_definition_t *)definition_ptr;
  widget = (widget_instance_t *)widget_ptr;
  parent = (widget_instance_t *)parent_ptr;

  csmemset(widget, 0, 0x58);
  if ((definition->list_flags &
       UI_LIST_ITEMS_GENERATED_FROM_STRING_LIST_TAG_FLAG) &&
      parent != NULL && tag_index == parent->definition_tag_index) {
    widget->type = 1;
  }
  widget->local_player_index = (short)local_player_index;
  widget->definition_tag_index = tag_index;
  widget->field_04 = (char *)definition + 4;
  widget->type = definition->type;
  widget->visible = 1;
  ((widget_instance_t *)widget)->field_11 = (char)(((unsigned int)definition->flags >> 9) & 1);
  ((widget_instance_t *)widget)->field_13 = (char)(((unsigned int)definition->flags >> 1) & 1);
  ((widget_instance_t *)widget)->field_18 = (int)widget_globals_field_20;
  ((widget_instance_t *)widget)->field_1c = *(int *)((char *)definition + 0x30) > 0
                                      ? *(int *)((char *)definition + 0x30)
                                      : 0;
  ((widget_instance_t *)widget)->field_20 = *(int *)((char *)definition + 0x34) > 0
                                      ? *(int *)((char *)definition + 0x34)
                                      : 0;
  widget->alpha_modifier = 1.0f;
  widget->parent = parent;

  switch (widget->type) {
  case 1:
    *(short *)((char *)widget + 0x40) = -1;
  }

  if (definition->field_44 != -1) {
    ((widget_instance_t *)widget)->field_56 =
      *(short *)((char *)tag_block_get_element(
                   (char *)tag_get(0x6269746d /* 'bitm' */,
                                   definition->field_44) +
                     0x54,
                   0, 0x40) +
                 0x22);
  }

  if (widget_globals_field_63 == 0 &&
      !ui_widget_load_children_recursive(widget, definition)) {
    error(2, "failed to load widget children");
  }

  for (handler_index = 0; handler_index < definition->event_handlers.count;
       handler_index++) {
    ui_widget_event_handler_reference_t *handler;

    handler = (ui_widget_event_handler_reference_t *)
                definition->event_handlers.address +
              handler_index;
    if (handler->event_type == 0x18) {
      int16_t event_data[4] = {0};
      event_data[1] = widget->local_player_index;
      event_handler_dispatch(widget, definition, event_data, handler,
                             (char *)&widget_stack + 3);
    }
  }

  if (widget->focused_child == NULL) {
    widget_instance_t *child = widget->child;
    while (child != NULL) {
      ui_widget_definition_t *child_definition = (ui_widget_definition_t *)
        tag_get(0x44654c61 /* 'DeLa' */, child->definition_tag_index);
      if (((widget_instance_t *)child)->field_12 == 0 &&
          (child_definition->event_handlers.count > 0 ||
           child->type == UI_WIDGET_TYPE_SPINNER_LIST ||
           child->type == UI_WIDGET_TYPE_COLUMN_LIST)) {
        widget_instance_give_focus_directly(widget, child);
      }
      child = child->next;
    }
  }

  if (((widget_instance_t *)widget)->field_13 == 1) {
    widget_globals_field_2a++;
    if (!game_time_get_paused()) {
      game_time_set_paused(true);
    }
    if (widget_globals_field_67 == 0 && byte_46CC88 == 0) {
      sound_set_music_enabled(1);
      widget_globals_field_67 = 1;
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
      (uint32_t)(widget_globals_field_20 -
                 dword_46CC90[(event[4] - 8) +
                              *(int16_t *)(event + 2) * 4]) >=
        0xfa) {
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
    timeout = ((widget_instance_t *)w)->field_1c;
    if (timeout > 0) {
      elapsed = (int)widget_globals_field_20 - ((widget_instance_t *)w)->field_18;
      fade_ticks = ((widget_instance_t *)w)->field_20;

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
            ((widget_instance_t *)child)->field_56 == 2) {
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
  if ((flags & UI_WIDGET_PASS_HANDLED_EVENTS_TO_ALL_CHILDREN_FLAG) != 0 &&
      (flags & UI_WIDGET_PASS_UNHANDLED_EVENTS_TO_CHILDREN_FLAG) == 0) {
    display_assert("if the _widget_pass_handled_events_to_all_children_bit "
                   "flag is checked, "
                   "_widget_pass_unhandled_events_to_children_bit must also "
                   "be checked for it to work",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0xd95, false);
  }

  if (!(((flags & UI_WIDGET_PASS_HANDLED_EVENTS_TO_ALL_CHILDREN_FLAG) == 0 &&
         consumed) ||
        ((flags & UI_WIDGET_PASS_UNHANDLED_EVENTS_TO_CHILDREN_FLAG) == 0 &&
         (flags & 0x100) == 0) ||
        widget_deleted)) {
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
      if (widget_globals_field_00[i] != 0) {
        break;
      }
    }
    if (i == 4) {
      main_goto_main_menu();
    }
  }

  if (*(int16_t *)event == 3 && event[5] == 1 && *(int16_t *)(event + 2) >= 0 &&
      *(int16_t *)(event + 2) < 4 && event[4] >= 8 && event[4] < 0xc) {
    dword_46CC90[(event[4] - 8) + *(int16_t *)(event + 2) * 4] =
      widget_globals_field_20;
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

  if (widget_globals_initialized == 0) {
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
    widget_memory_pool, 0x58, "c:\\halo\\SOURCE\\interface\\ui_widget.c",
    0x18b);
  if (widget == 0) {
    error(2, "failed to create new widget; out of memory!");
    return NULL;
  }

  if (a3 == 0) {
    root_widget = widget_globals_field_00[(int)stack_index];
    if (root_widget != 0) {
      previous_stack_player = *(int16_t *)(root_widget + 8);
      ui_widget_delete((void *)root_widget);
    } else {
      previous_stack_player = -1;
    }

    widget_globals_field_00[(int)stack_index] = widget;

    if (parent_tag_index != -1 &&
        (*(uint32_t *)((int)tag_get(0x44654c61, parent_tag_index) + 0x2c) &
         0x4000) == 0) {
      pending_load.tag_index = parent_tag_index;
      pending_load.a6 = a6;
      pending_load.a7 = (int16_t)a7;
      pending_load.widget_stack = previous_stack_player;
      push_widget(&widget_globals_field_10[(int)stack_index], &pending_load);
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
  assert_halt(widget_globals_initialized);

  widget_globals_field_65 = 0;

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

  if (widget_globals_field_66 == 0) {
    ui_start_main_menu_music();
  }

  reset_last_player1_profile_index();

done:
  if (!virtual_keyboard_initialize()) {
    error(2, "failed to initialize the virtual keyboard");
  }
  *(uint8_t *)0x31e050 = 0;
}

/* 0xe8830 — Return the client to its pregame screen after a network game.
 * The 2276 body closes widgets first and selects one of four screens based
 * on split-screen, quickstart, and local-host state.  The connected host
 * pauses the server countdown before opening map selection.
 */
void network_game_reset_to_pregame_ui(void)
{
  void *server;

  ui_widgets_close_all();
  if (network_game_is_splitscreen_local()) {
    if (network_game_is_quickstart_local()) {
      if (ui_widget_load_by_name_or_tag(
            "ui\\shell\\main_menu\\multiplayer_type_select\\split_"
            "screen\\pregame\\splitscreen_pregame_wrapper_normal",
            -1, 0, -1, -1, -1, -1) == 0) {
        error(2, "failed to load pregame screen after quickstart match");
      }
    } else {
      if (ui_widget_load_by_name_or_tag(
            "ui\\shell\\main_menu\\multiplayer_type_select\\split_"
            "screen\\splitscreen_map_select_postgame_wrapper",
            -1, 0, -1, -1, -1, -1) == 0) {
        error(2, "failed to load map select postgame screen");
      }
    }
  } else {
    server = global_network_game_server_get();
    if (server != 0) {
      server = global_network_game_server_get();
      network_game_server_pause_countdown(server, 1);
      if (ui_widget_load_by_name_or_tag(
            "ui\\shell\\main_menu\\multiplayer_type_"
            "select\\connected\\connected_map_select_postgame_wrapper",
            -1, 0, -1, -1, -1, -1) == 0) {
        error(2, "failed to load map select postgame screen");
      }
    } else {
      if (ui_widget_load_by_name_or_tag(
            "ui\\shell\\main_menu\\multiplayer_type_"
            "select\\connected\\pregame\\connected_pregame_screen",
            -1, 0, -1, -1, -1, -1) == 0) {
        error(2, "failed to load networked pregame status screen");
      }
    }
  }
}

void ui_widget_display_error(int16_t error_handle, int16_t local_player_index,
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

    deferred_slot = widget_globals_field_4c[(int)stack_index].error_handle;
    if (deferred_slot != -1) {
      error(2,
            "there is already a deferred-for-cinematic error queued for player"
            " #%d; ignoring this one",
            (int)stack_index);
      return;
    }

    widget_globals_field_4c[(int)stack_index].error_handle = error_handle;
    widget_globals_field_4c[(int)stack_index].field_02 = (uint8_t)is_modal;
    widget_globals_field_4c[(int)stack_index].field_03 = (uint8_t)pause_game;
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

    if (byte_46CC88 == 0) {
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

  if (byte_46CC88 != 0 && widget_globals_field_2c <= 1.0f &&
      widget_globals_field_2c >= 0.0f) {
    error(2, "aborting to the main menu root, for safety's sake");
    main_screen_shell_load();
    main_defer_map_map_change();
    widget_globals_field_2c = -1.0f;
  }

  root_widget = widget_globals_field_00[widget_stack_index];
  if (root_widget == 0) {
    root_tag_index = -1;
  } else {
    root_tag_index = *(int *)root_widget;
    if (((widget_instance_t *)root_widget)->field_15 == 1) {
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

  ((widget_instance_t *)widget)->field_15 = 1;
  if (((widget_instance_t *)widget)->field_13 == 0) {
    ((widget_instance_t *)widget)->field_13 = (uint8_t)pause_game;
    if (pause_game == 1) {
      if (widget_globals_field_2a < 0) {
        display_assert("widget pause counter is out of whack",
                       "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x8a9, true);
        system_exit(-1);
      }

      widget_globals_field_2a++;
      if (!game_time_get_paused()) {
        game_time_set_paused(1);
      }

      if (widget_globals_field_67 == 0 && byte_46CC88 == 0) {
        sound_set_music_enabled(1);
        widget_globals_field_67 = 1;
      }
    }
  }

  switch (error_handle) {
  case 0xd:
    *(uint8_t *)(widget + 0x16) = 1;
    /* fallthrough */
  case 0xc:
    ((widget_instance_t *)widget)->field_1c = 0;
    ((widget_instance_t *)widget)->field_20 = 0;
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
    ((widget_instance_t *)widget)->field_15 = 1;
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
      game_connection() != 3 && byte_46CC88 == 0 &&
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
          root_widget = widget_globals_field_00[stack_index];
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

          if (widget_globals_initialized != 0) {
            for (i = 0; i < 4; i++) {
              if (widget_globals_field_00[i] != 0) {
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

        root_widget = widget_globals_field_00[stack_index];
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
  if (widget_globals_initialized == 0) {
    display_assert("widget_globals.initialized",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x284, true);
    system_exit(-1);
  }

  /* Record frame timestamp for event throttling. */
  widget_globals_field_20 = system_milliseconds();

  /* If an async filesystem operation is pending, poll for completion. */
  if (widget_globals_initialization_thread != NULL) {
    if (thread_is_done(widget_globals_initialization_thread) != 0) {
      thread_close(widget_globals_initialization_thread);
      widget_globals_initialization_thread = NULL;
      ui_widgets_inhibit_processing(false);
      if (widget_globals_field_60 == 1) {
        if (bink_playback_has_video()) {
          bink_playback_stop();
        }
        ui_widget_load_error_screen(0x21, 1);
        return;
      }
      if (widget_globals_field_60 == 2) {
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
  if (widget_globals_field_48 != -1) {
    ui_widget_load_error_screen(widget_globals_field_48,
                                widget_globals_field_4a);
    widget_globals_field_48 = -1;
    return;
  }

  /* If any deferred error slots are populated, try to show them. */
  if ((widget_globals_field_30[0].error_handle == -1) &&
      (widget_globals_field_30[1].error_handle == -1) &&
      (widget_globals_field_30[2].error_handle == -1) &&
      (widget_globals_field_30[3].error_handle == -1)) {
    /* Normal widget event processing path. */
    blocked_by_pause = ui_check_for_pause_game() != 0;

    active_widget_stacks[0] =
      (widget_globals_field_00[0] != 0) &&
      (*(uint8_t *)(widget_globals_field_00[0] + 0x15) == 1);
    active_widget_stacks[1] =
      (widget_globals_field_00[1] != 0) &&
      (*(uint8_t *)(widget_globals_field_00[1] + 0x15) == 1);
    active_widget_stacks[2] =
      (widget_globals_field_00[2] != 0) &&
      (*(uint8_t *)(widget_globals_field_00[2] + 0x15) == 1);
    active_widget_stacks[3] =
      (widget_globals_field_00[3] != 0) &&
      (*(uint8_t *)(widget_globals_field_00[3] + 0x15) == 1);
    any_active_widget_stack = active_widget_stacks[0] |
                              active_widget_stacks[1] |
                              active_widget_stacks[2] | active_widget_stacks[3];

    widget_roots = widget_globals_field_00;
    for (stack_index = 0; stack_index < 4; stack_index++, widget_roots++) {
      widget = *widget_roots;
      if (active_widget_stacks[stack_index] == 1) {
        if ((widget == 0) || (((widget_instance_t *)widget)->field_15 != 1)) {
          continue;
        }
      } else if (byte_46CC88 == 0) {
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

      if ((widget_globals_field_65 == 0) &&
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
  deferred_error = widget_globals_field_30;
  while ((int)deferred_error < (int)&widget_globals_field_48) {
    if (deferred_error->error_handle != -1) {
      if ((byte_46CC88 == 0) && (!network_game_in_progress()) &&
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
 * to FUN_00082bd0 is not evidenced here, so they stay raw offsets. */
bool network_game_join_game_from_server_list(void *widget, void *event_data,
                                             bool *widget_deleted)
{
  widget_instance_t *list;
  void *entry;
  void *spawned;
  widget_instance_t *topmost_parent;
  widget_instance_t *parent;
  int parent_tag_index;
  int child_index;
  short selected;
  bool result;

  (void)event_data;

  list = (widget_instance_t *)widget;
  result = false;
  if (list->focused_child == NULL) {
    goto done;
  }
  selected = list->list_selected_index;
  if (selected < 0) {
    goto done;
  }
  if ((int)selected < (int)list->list_number_of_items &&
      list->list_items != NULL) {
    if (list->list_number_of_items > 0) {
      entry = ((void **)list->list_items)[selected];
      if (*(unsigned char *)((char *)entry + 0xe0) == 1) {
        if (*(short *)((char *)entry + 0xde) == 0) {
          transport_address address = {{0}};
          unsigned char join_params[0x22];

          FUN_00082bd0((char *)entry + 0x18,
                       (const uint32_t *)((char *)entry + 8),
                       (const uint32_t *)entry,
                       0x141e,
                       (uint32_t *)&address);
          if (address.address.ipv4_address != 0 &&
              address.port != 0) {
            *(uint16_t *)(join_params + 2) = 0;
            network_game_generate_join_game_token(join_params + 0x12);
            if (network_game_client_initiate_join_game(
                  global_network_game_client_get(), entry, join_params,
                  &address)) {
              topmost_parent = widget_instance_get_topmost_parent(widget);
              parent = list->parent;
              if (parent != NULL) {
                parent_tag_index = parent->definition_tag_index;
              } else {
                parent_tag_index = -1;
              }
              child_index = widget_instance_get_child_index_from_parent(widget);
              spawned = ui_widget_load_by_name_or_tag(
                "ui\\shell\\main_menu\\multiplayer_type_"
                "select\\connected\\pregame\\"
                "connected_pregame_screen",
                -1,
                0,
                -1,
                topmost_parent->definition_tag_index,
                parent_tag_index,
                child_index);
              if (spawned == NULL) {
                error(2, "event handler failed to spawn widget");
              } else {
                set_game_connection(1);
                result = true;
              }
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

/* ui_widget_display_deferred_errors — flushes the deferred-for-cinematic error
 * queue (4 records at 0x46cc6c, one per local-player slot, 4 bytes each:
 * int16 error_handle @+0, uint8 is_modal @+2, uint8 pause_game @+3). Must run
 * only outside a cinematic; asserts otherwise ("Noooooooooooooooooo!!!",
 * ui_widget.c line 0x93f, system_exit(-1) flavor). For each valid record
 * (0 <= handle < 0x28) it re-issues ui_widget_display_error(handle, slot,
 * is_modal, pause_game), then clears the slot to -1. Ref 0xe8db0. */
void ui_widget_display_deferred_errors(void)
{
  int16_t error_handle;
  int local_player_index;
  int16_t *record;

  if (cinematic_in_progress()) {
    display_assert("Noooooooooooooooooo!!!",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x93f, true);
    system_exit(-1);
  }

  local_player_index = 0;
  record = &widget_globals_field_4c[0].error_handle;
  do {
    error_handle = *record;
    if (error_handle >= 0 && error_handle < 0x28) {
      ui_widget_display_error(error_handle, local_player_index, (char)record[1],
                              *(char *)((int)record + 3));
    }
    *record = -1;
    local_player_index = local_player_index + 1;
    record = record + 2;
  } while ((int16_t)local_player_index < 4);
}

/* ui_widget_display_scenario_help — displays the in-game player-help dialog for
 * the scenario that is currently loaded. Copies the scenario tag name into a
 * 256-byte buffer, lowercases it, and matches it against ten level codes
 * ("a10".."d40") to select the matching player_help_screen widget tag. The
 * screen is loaded for the single-player local controller; string_index is then
 * written into the first child widget of type 1 (text box) at +0x40.
 * Asserts: "string_index>=0" (ui_widget.c 0x967) and "expected text box widget
 * in player help screen" (0x986), both system_exit(-1) flavor. Global
 * 0x326a08 is global_scenario_index (NONE when no scenario is loaded).
 * Ref 0xe8e20. */
void ui_widget_display_scenario_help(int16_t string_index)
{
  const char *screen_name;
  void *screen;
  int widget;
  char scenario_name[256];

  if (string_index < 0) {
    display_assert("string_index>=0",
                   "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x967, true);
    system_exit(-1);
  }

  if (*(int *)0x326a08 == NONE) {
    error(2, "can't display scenario help because no scenario is loaded");
  } else {
    csstrncpy(scenario_name, tag_get_name(*(int *)0x326a08), 0xff);
    scenario_name[255] = 0;
    csstr_tolower(scenario_name);

    if (crt_strstr(scenario_name, "a10") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_a10";
    } else if (crt_strstr(scenario_name, "a30") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_a30";
    } else if (crt_strstr(scenario_name, "a50") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_a50";
    } else if (crt_strstr(scenario_name, "b30") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_b30";
    } else if (crt_strstr(scenario_name, "b40") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_b40";
    } else if (crt_strstr(scenario_name, "c10") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_c10";
    } else if (crt_strstr(scenario_name, "c20") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_c20";
    } else if (crt_strstr(scenario_name, "c40") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_c40";
    } else if (crt_strstr(scenario_name, "d20") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_d20";
    } else if (crt_strstr(scenario_name, "d40") != NULL) {
      screen_name = "ui\\shell\\solo_game\\player_help\\player_help_screen_d40";
    } else {
      error(2, "can't display scenario help; unknown scenario is active '%s'",
            scenario_name);
      return;
    }

    screen = ui_widget_load_by_name_or_tag(
      screen_name, NONE, 0,
      (int)player_ui_get_single_player_local_player_controller(0), NONE, NONE,
      NONE);
    if (screen != NULL) {
      widget = *(int *)((int)screen + 0x34);
      while (1) {
        if (widget == 0) {
          display_assert("expected text box widget in player help screen",
                         "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x986,
                         true);
          system_exit(-1);
        }
        if (*(int16_t *)(widget + 0xe) == 1) {
          break;
        }
        widget = *(int *)(widget + 0x2c);
      }
      *(int16_t *)(widget + 0x40) = string_index;
    } else {
      error(2, "failed to load in-game help dialog");
    }
  }
}

/* Close all UI widgets and display the "damaged media" fatal error screen.
 *
 * Loads the "error_abort_to_dashboard_you_have_no_choice" widget by name,
 * asserts that it is a text box widget (type 1), sets its string_list_index
 * and the global error_string_index to 0x23, marks the widget as needing
 * a text update, then flushes input and enters the halt loop forever.
 * If the widget fails to load, logs an error and enters the halt loop
 * anyway. This function never returns. */
void display_error_damaged_media(void)
{
  void *widget;

  ui_widgets_close_all();
  widget = ui_widget_load_by_name_or_tag(
    "ui\\shell\\error\\error_abort_to_dashboard_you_have_no_choice", -1, 0, -1,
    -1, -1, -1);
  if (widget != NULL) {
    if (*(int16_t *)((char *)widget + 0xe) != 1) {
      display_assert("expected a text box widget",
                     "c:\\halo\\SOURCE\\interface\\ui_widget.c", 0x90f, 1);
      system_exit(-1);
    }
    *(int16_t *)((char *)widget + 0x40) = 0x23;
    ((widget_instance_t *)widget)->field_15 = 1;
    *(int16_t *)0x31e054 = 0x23;
    input_frame_end();
    main_halt_entry();
    return; /* main_halt_entry is a plain void here; original tail-jumps */
  }
  error(2, "failed to load '%s' widget",
        "ui\\shell\\error\\error_abort_to_dashboard_you_have_no_choice");
  input_frame_end();
  main_halt_entry();
}
