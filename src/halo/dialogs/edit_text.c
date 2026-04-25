/* Moves the text cursor to the end of the edit text buffer and
 * clears any active selection. Asserts that the edit_text struct
 * is valid (non-null, has buffer, max_length > 0, strlen <= max). */
void edit_text_set_cursor_to_end(void *edit_text)
{
  int *et = (int *)edit_text;

  if (et == NULL || et[0] == 0 || *(int16_t *)((int)et + 4) <= 0 ||
      (unsigned int)csstrlen((const char *)et[0]) >
        (unsigned int)(int)*(int16_t *)((int)et + 4)) {
    display_assert("valid_edit_text(edit)",
                   "c:\\halo\\SOURCE\\dialogs\\edit_text.c", 0x9f, 1);
    system_exit(-1);
  }

  edit_text_clamp_cursor(edit_text);

  int16_t len = (int16_t)csstrlen((const char *)et[0]);
  *(int16_t *)((int)et + 6) = len;
  *(int16_t *)((int)et + 8) = -1;
}

/* Validates the edit_text struct and initializes cursor state by
 * placing the cursor at the end of the current text. */
void edit_text_initialize(void *edit_text)
{
  int *et = (int *)edit_text;

  if (et == NULL || et[0] == 0 || *(int16_t *)((int)et + 4) <= 0 ||
      (unsigned int)csstrlen((const char *)et[0]) >
        (unsigned int)(int)*(int16_t *)((int)et + 4)) {
    display_assert("valid_edit_text(edit)",
                   "c:\\halo\\SOURCE\\dialogs\\edit_text.c", 0x19, 1);
    system_exit(-1);
  }

  edit_text_set_cursor_to_end(edit_text);
}

/* Processes a single key event for the edit_text widget. Handles:
 * - Character insertion (with or without active selection)
 * - Left/Right arrow keys for cursor movement
 * - Shift+arrow for extending selection
 * - Backspace/Delete for character or selection deletion
 * When a selection is active, typing replaces it. Backspace/Delete
 * remove the selection range. Arrow keys collapse the selection to
 * the appropriate end. All cursor changes are snapped to unicode
 * character boundaries via unicode_snap_cursor.
 *
 * key_event layout:
 *   offset 0: uint8_t flags (bit 0 = shift held)
 *   offset 1: uint8_t character code
 *   offset 2: int16_t key code (0x1d=backspace, 0x54=delete, 0x4f=left,
 * 0x50=right)
 *
 * edit_text layout:
 *   offset 0: char* text buffer pointer
 *   offset 4: int16_t max_length
 *   offset 6: int16_t cursor_pos
 *   offset 8: int16_t selection (-1 = no selection)
 */
void edit_text_process_key(void *edit_text, void *keystroke)
{
  int *et = (int *)edit_text;
  unsigned char *key = (unsigned char *)keystroke;
  int16_t sel_start, sel_end;
  int text;
  int len;
  int cursor_pos;

  if (et == NULL || et[0] == 0 || *(int16_t *)((int)et + 4) <= 0 ||
      (unsigned int)csstrlen((const char *)et[0]) >
        (unsigned int)(int)*(int16_t *)((int)et + 4)) {
    display_assert("valid_edit_text(edit)",
                   "c:\\halo\\SOURCE\\dialogs\\edit_text.c", 0x23, 1);
    system_exit(-1);
  }

  edit_text_clamp_cursor(edit_text);

  int16_t key_code = *(int16_t *)(key + 2);

  /* --- Backspace / Delete --- */
  if (key_code == 0x1d || key_code == 0x54) {
    /* If there is an active selection, delete the selected range */
    if (edit_text_get_selection_range(edit_text, &sel_end, &sel_start)) {
      text = et[0];
      len = csstrlen((const char *)(text + (int)sel_start));
      csmemmove((void *)(text + (int)sel_end),
                (const void *)(text + (int)sel_start), (unsigned int)(len + 1));
      *(int16_t *)((int)et + 6) = sel_end;
      *(int16_t *)((int)et + 8) = -1;
      unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
      return;
    }

    if (key_code == 0x1d) {
      /* Backspace: delete character before cursor */
      int16_t old_cursor = *(int16_t *)((int)et + 6);
      if (old_cursor > 0) {
        unicode_cursor_backward((const char *)et[0], (int16_t *)((int)et + 6));
        text = et[0];
        len = csstrlen((const char *)(text + (int)old_cursor));
        csmemmove((void *)(text + (int)*(int16_t *)((int)et + 6)),
                  (const void *)(text + (int)old_cursor),
                  (unsigned int)(len + 1));
      }
    } else {
      /* Delete: delete character at cursor */
      int16_t cur = *(int16_t *)((int)et + 6);
      if ((unsigned int)(int)cur >= (unsigned int)csstrlen((const char *)et[0]))
        goto snap_and_return;

      int16_t temp_cursor = cur;
      unicode_cursor_forward((const char *)et[0], &temp_cursor);
      text = et[0];
      len = csstrlen((const char *)(text + (int)temp_cursor));
      csmemmove((void *)(text + (int)*(int16_t *)((int)et + 6)),
                (const void *)(text + (int)temp_cursor),
                (unsigned int)(len + 1));
    }
    goto snap_and_return;
  }

  /* --- Left / Right arrow --- */
  if (key_code == 0x4f || key_code == 0x50) {
    if ((key[0] & 1) == 0) {
      /* No shift: if selection active, collapse to appropriate end */
      if (edit_text_get_selection_range(edit_text, &sel_start, &sel_end)) {
        *(int16_t *)((int)et + 8) = -1;
        if (key_code == 0x4f) {
          /* Left: move cursor to selection start */
          *(int16_t *)((int)et + 6) = sel_start;
          unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
          return;
        }
        /* Right: move cursor to selection end */
        *(int16_t *)((int)et + 6) = sel_end;
        unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
        return;
      }
      if ((key[0] & 1) == 0)
        goto move_cursor;
    }

    /* Shift held (or shift re-check fell through): begin/extend selection */
    if (*(int16_t *)((int)et + 8) == -1) {
      *(int16_t *)((int)et + 8) = *(int16_t *)((int)et + 6);
    }

  move_cursor:
    if (key_code == 0x4f && *(int16_t *)((int)et + 6) > 0) {
      unicode_cursor_backward((const char *)et[0], (int16_t *)((int)et + 6));
    } else if (key_code == 0x50) {
      if ((unsigned int)(int)*(int16_t *)((int)et + 6) <
          (unsigned int)csstrlen((const char *)et[0])) {
        unicode_cursor_forward((const char *)et[0], (int16_t *)((int)et + 6));
      }
    }

    /* If selection collapsed (cursor == selection anchor), clear it */
    if (*(int16_t *)((int)et + 8) == *(int16_t *)((int)et + 6)) {
      *(int16_t *)((int)et + 8) = -1;
      unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
      return;
    }
    goto snap_and_return;
  }

  /* --- Character insertion --- */
  if (key[1] == 0 || key[1] == 0xff)
    goto snap_and_return;

  if (edit_text_get_selection_range(edit_text, &sel_start, &sel_end)) {
    /* Replace selection with typed character */
    text = et[0];
    len = csstrlen((const char *)(text + (int)sel_end));
    csmemmove((void *)(text + (int)sel_start + 1),
              (const void *)(text + (int)sel_end), (unsigned int)(len + 1));
    *(int16_t *)((int)et + 6) = sel_start;
    *(int16_t *)((int)et + 8) = -1;
    *(unsigned char *)((int)sel_start + et[0]) = key[1];
    *(int16_t *)((int)et + 6) = *(int16_t *)((int)et + 6) + 1;
    unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
    return;
  }

  /* No selection: insert at cursor if room */
  if ((unsigned int)csstrlen((const char *)et[0]) >=
      (unsigned int)(int)*(int16_t *)((int)et + 4))
    goto snap_and_return;

  cursor_pos = (int)*(int16_t *)((int)et + 6) + et[0];
  len = csstrlen((const char *)cursor_pos);
  csmemmove((void *)(cursor_pos + 1), (const void *)cursor_pos,
            (unsigned int)(len + 1));
  *(unsigned char *)((int)*(int16_t *)((int)et + 6) + et[0]) = key[1];
  *(int16_t *)((int)et + 6) = *(int16_t *)((int)et + 6) + 1;
  unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
  return;

snap_and_return:
  unicode_snap_cursor((const char *)et[0], (int16_t *)((int)et + 6));
  return;
}
