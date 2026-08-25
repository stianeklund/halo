void ui_widget_game_data_function_invoke(
  void *widget, unsigned __int16 game_data_input_reference_function)
{
  assert_halt(widget);

  if (game_data_input_reference_function > 40u) {
    error(2, "invalid game_data_input_reference_function");
  } else {
    ui_widget_game_data_function_table[game_data_input_reference_function](
      widget);
  }
}

void ui_widget_game_data_build_version(int widget)
{
  wchar_t *v1, *v2; // eax

  if (!ui_widget_game_data_build_version_wide_str[0]) {
    ascii_to_wide(
#if DECOMP_CUSTOM
      build_ui_widget_text,
#else
      "01.10.12.2276",
#endif
      ui_widget_game_data_build_version_wide_str,
      sizeof(ui_widget_game_data_build_version_wide_str));
  }

  if (!*(uint32_t *)(widget + 60)) {
    v1 =
      ui_widget_realloc(0, sizeof(ui_widget_game_data_build_version_wide_str),
                        __FILE__, __LINE__);
    *(uint32_t *)(widget + 60) = (uint32_t)v1;
    if (v1) {
      csmemset(v1, 0, sizeof(ui_widget_game_data_build_version_wide_str));
    }
  }
  v2 = *(wchar_t **)(widget + 60);
  if (v2) {
    ustrncpy(v2, ui_widget_game_data_build_version_wide_str, 0x3Fu);
    *(wchar_t *)(*(uint32_t *)(widget + 60) + 126) = 0;
  }
}

/* FUN_000f46e0 (0xf46e0)
 * "mp player settings select" quarter-screen profile list widget update.
 *
 * Validates the widget hierarchy (container wrapper + 1-wide DeLa spinner +
 * 3 child widgets), then resolves the currently selected profile id against the
 * global profile record table at 0x5aa3c0 (3 records, stride 0x34).  On a hit
 * it fills the widget's name buffer (widget+0x4c, 0x18 bytes) and the sibling
 * widget's description buffer (sibling+0x3c, 0x200 bytes).  On a miss the
 * item-id array is re-sorted / compacted and the routine restarts from the top
 * (the back edges at 0x48e6 / 0x48fc target 0x46f0, i.e. the whole body is a
 * retry loop).
 *
 * Profile record layout (evidence: the accesses below, stride 0x34):
 *   +0x00 int    profile id
 *   +0x04 wchar_t name[12]
 *   +0x1c short  controller-setup index (< 0 => 0)
 *   +0x1e ushort flags; bit0 = "use default name", bits 8..15 = ustr string
 * index +0x2c byte   button description index +0x2d byte   joystick description
 * index MSVC keeps the record pointer as &record.name (base+4), so the offsets
 * used below are 4 less than the record-relative ones above. */
void FUN_000f46e0(int *widget)
{
  short *list_tag;
  void *wrapper_tag;
  int child; /* [EBP-0x4] */
  int sibling; /* [EBP+0x8] — MSVC reuses the dead incoming param slot */
  int local_id; /* [EBP-0x8] */
  int item_id;
  int *item_ids;
  int *entry;
  int entry_index;
  wchar_t *rec_name;
  wchar_t *name_buf;
  wchar_t *desc_buf;
  wchar_t *src;
  unsigned short flags;
  int tag_index;
  int joystick_tag;
  int button_tag;
  int joystick_str;
  int button_str;
  int clamped;
  int count;
  int used;
  short list_index;

  while (1) {
    if (widget[0xc] == 0) {
      display_assert(
        "expected qtr-screen profile select list to be wrapped in a container "
        "widget",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x85e, 1);
      system_exit(-1);
    }
    list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *widget);
    if (*list_tag != 2) {
      display_assert(
        "expected a spinner list for 'mp player settings select' widget",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x864, 1);
      system_exit(-1);
    }
    if (*(int *)((char *)list_tag + 0x3e0) != 0) {
      display_assert(
        "expected 0 children (1-wide spinner) for 'mp player settings select' "
        "widget",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x865, 1);
      system_exit(-1);
    }
    wrapper_tag = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget[0xc]);
    if (*(int *)((char *)wrapper_tag + 0x3e0) != 3) {
      display_assert(
        "expected qtr-screen profile select wrapper screen to have 3 child "
        "widgets (pic, description, list... in that order)",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x867, 1);
      system_exit(-1);
    }
    child = *(int *)(widget[0xc] + 0x34);
    sibling = *(int *)(child + 0x2c);
    if (widget != *(int **)(sibling + 0x2c)) {
      display_assert(
        "expected qtr-screen profile select wrapper screen to have 3 child "
        "widgets (pic, description, list... in that order)",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x86d, 1);
      system_exit(-1);
    }
    if (*(short *)((char *)widget + 0x3c) < 0 ||
        (int)*(short *)((char *)widget + 0x3c) >=
          (int)*(unsigned short *)((char *)widget + 0x44)) {
      display_assert(
        "qtr-screen profile list has invalid list item index",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x86f, 1);
      system_exit(-1);
    }
    if (*(short *)((char *)widget + 0x3c) < 0 ||
        (int)*(short *)((char *)widget + 0x3c) >=
          (int)*(unsigned short *)((char *)widget + 0x44)) {
      display_assert(
        "invalid list item index",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x872, 1);
      system_exit(-1);
    }

    item_id = ((int *)widget[0x10])[*(short *)((char *)widget + 0x3c)];
    local_id = item_id;
    multiplayer_game_set_text_box_for_game_ruleset(&local_id, 1);

    if (item_id != -1) {
      entry_index = 0;
      entry = (int *)0x5aa3c0;
      do {
        if (*entry == item_id) {
          rec_name = (wchar_t *)(entry_index * 0x34 + 0x5aa3c4);
          if (rec_name != NULL) {
            name_buf =
              (wchar_t *)ui_widget_realloc(widget[0x13], 0x18,
                                           "c:\\halo\\SOURCE\\interface\\ui_"
                                           "widget_game_data_input_functions.c",
                                           0x886);
            widget[0x13] = (int)name_buf;
            if (name_buf == NULL) {
              return;
            }
            flags = *(unsigned short *)((char *)rec_name + 0x1a);
            if ((flags & 1) != 0) {
              tag_index =
                tag_loaded(0x75737472 /* 'ustr' */,
                           "ui\\shell\\strings\\default_player_profile_names");
              src = (wchar_t *)0x26cdf0; /* L"" */
              if (tag_index != -1) {
                src = (wchar_t *)FUN_0019d420(tag_index, flags >> 8);
              }
              ustrncpy((wchar_t *)widget[0x13], src, 0xb);
              *(short *)(widget[0x13] + 0x16) = 0;
            } else {
              ustrncpy((wchar_t *)widget[0x13], rec_name, 0xb);
              *(short *)(widget[0x13] + 0x16) = 0;
            }

            if (*(short *)((char *)rec_name + 0x18) < 0) {
              clamped = 0;
            } else {
              clamped = *(short *)((char *)rec_name + 0x18);
              if (clamped > (int)FUN_001c0ed0() - 1) {
                clamped = (int)FUN_001c0ed0() - 1;
              }
            }
            *(short *)(child + 0x50) = (short)clamped;

            desc_buf =
              (wchar_t *)ui_widget_realloc(*(int *)(sibling + 0x3c), 0x200,
                                           "c:\\halo\\SOURCE\\interface\\ui_"
                                           "widget_game_data_input_functions.c",
                                           0x89c);
            *(wchar_t **)(sibling + 0x3c) = desc_buf;
            if (desc_buf == NULL) {
              return;
            }

            if ((*(unsigned char *)((char *)rec_name + 0x1a) & 1) != 0) {
              joystick_tag =
                tag_loaded(0x75737472 /* 'ustr' */,
                           "ui\\shell\\main_menu\\player_profiles_"
                           "select\\joystick_set_defaults_descriptions");
              button_tag = tag_loaded(0x75737472 /* 'ustr' */,
                                      "ui\\shell\\main_menu\\player_profiles_"
                                      "select\\button_set_long_descriptions");
              if (joystick_tag == -1 || button_tag == -1) {
                **(short **)(sibling + 0x3c) = 0;
                *(short *)(*(int *)(sibling + 0x3c) + 0x1fe) = 0;
                return;
              }
              joystick_str = FUN_0019d420(
                joystick_tag,
                (unsigned short)*(unsigned char *)((char *)rec_name + 0x29));
              button_str = FUN_0019d420(
                button_tag,
                (unsigned short)*(unsigned char *)((char *)rec_name + 0x28));
              unicode_sprintf(*(wchar_t **)(sibling + 0x3c), 0xff, L"%s%hs%s",
                              (wchar_t *)joystick_str, "\r\n",
                              (wchar_t *)button_str);
              *(short *)(*(int *)(sibling + 0x3c) + 0x1fe) = 0;
            } else {
              joystick_tag =
                tag_loaded(0x75737472 /* 'ustr' */,
                           "ui\\shell\\main_menu\\player_profiles_"
                           "select\\joystick_set_short_descriptions");
              button_tag = tag_loaded(0x75737472 /* 'ustr' */,
                                      "ui\\shell\\main_menu\\player_profiles_"
                                      "select\\button_set_short_descriptions");
              if (joystick_tag != -1 && button_tag != -1) {
                joystick_str = FUN_0019d420(
                  joystick_tag,
                  (unsigned short)*(unsigned char *)((char *)rec_name + 0x29));
                button_str = FUN_0019d420(
                  button_tag,
                  (unsigned short)*(unsigned char *)((char *)rec_name + 0x28));
                unicode_sprintf(*(wchar_t **)(sibling + 0x3c), 0xff, L"%s%hs%s",
                                (wchar_t *)joystick_str, "\r\n",
                                (wchar_t *)button_str);
              }
            }
            *(short *)(*(int *)(sibling + 0x3c) + 0x1fe) = 0;
            return;
          }
          break;
        }
        entry = entry + 0xd; /* stride 0x34 bytes */
        entry_index = entry_index + 1;
      } while ((int)entry < 0x5aa45c);
    }

    if (*(unsigned short *)((char *)widget + 0x44) == 0) {
      name_buf = (wchar_t *)ui_widget_realloc(
        widget[0x13], 4,
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x8d2);
      widget[0x13] = (int)name_buf;
      if (name_buf != NULL) {
        *name_buf = 0;
      }
      *(short *)(child + 0x50) = 0;
      desc_buf = (wchar_t *)ui_widget_realloc(
        *(int *)(sibling + 0x3c), 4,
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x8d7);
      *(wchar_t **)(sibling + 0x3c) = desc_buf;
      if (desc_buf != NULL) {
        *desc_buf = 0;
      }
      return;
    }

    item_ids = (int *)widget[0x10];
    count = (int)*(unsigned short *)((char *)widget + 0x44);
    qsort(item_ids, count, 4, FUN_000f3960);
    for (used = 0; used < count; used++) {
      if (item_ids[used] == -1) {
        break;
      }
    }
    list_index = *(short *)((char *)widget + 0x3c);
    *(unsigned short *)((char *)widget + 0x44) = (unsigned short)used;
    if (list_index < 0) {
      *(short *)((char *)widget + 0x3c) = 0;
    } else {
      clamped = (int)(unsigned short)used - 1;
      if ((int)list_index <= clamped) {
        clamped = list_index;
      }
      *(short *)((char *)widget + 0x3c) = (short)clamped;
    }
  }
}

/* color picker menu dispose (event handler table index 62, 0x0eebe0) — frees
 * the child widget cached at +0x40 back to the widget pool, if present. */
bool ui_widget_color_picker_menu_dispose(void *widget, void *event_data,
                                         bool *widget_deleted)
{
  void *child;

  child = *(void **)((char *)widget + 0x40);
  if (child != NULL) {
    widget_free(child);
    *(void **)((char *)widget + 0x40) = NULL;
  }
  return true;
}

/* disable if no xdemos (event handler table index 86, 0x0f0070) — marks the
 * widget disabled (+0x12) and clears its enabled/visible byte (+0x10) when no
 * Xbox demo content is installed. */
bool ui_widget_disable_if_no_xdemos(void *widget, void *event_data,
                                    bool *widget_deleted)
{
  if (!xbox_demos_available()) {
    *(uint8_t *)((char *)widget + 0x12) = 1;
    *(uint8_t *)((char *)widget + 0x10) = 0;
  }
  return true;
}

/* pop history stack once (event handler table index 98, 0x0f0620) — pops one
 * entry from the widget history stack of the widget's local player (+0x8). */
bool ui_widget_pop_history_stack_once(void *widget, void *event_data,
                                      bool *widget_deleted)
{
  ui_widgets_pop_stack(*(uint16_t *)((char *)widget + 0x8));
  return true;
}
