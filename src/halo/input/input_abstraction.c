#include "x87_math.h"

void input_abstraction_dispose(void)
{
  csmemset((void *)0x46b820, 0, 0xdc);
}

/* Snapshot the current system time into the input abstraction globals.
   Called each tick to timestamp when input was last processed. */
void input_abstraction_mark_time(void)
{
  *(unsigned int *)0x46b8f0 = system_milliseconds();
}

/* Copy a controller's preference block from the per-controller preferences
   array at 0x46b820 (4 entries of 0x18 bytes). */
void input_abstraction_get_local_player_preferences(short local_player_index,
                                                    void *preferences_out)
{
  if (local_player_index < 0 || local_player_index >= MAXIMUM_GAMEPADS) {
    display_assert(
      "(local_player_index>=0) && (local_player_index<MAXIMUM_GAMEPADS)",
      "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x1ef, 1);
    system_exit(-1);
  }
  if (preferences_out == NULL) {
    display_assert("preferences",
                   "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x1f0, 1);
    system_exit(-1);
  }
  csmemcpy(preferences_out, (char *)0x46b820 + local_player_index * 0x18, 0x18);
}

/* Store a controller's preference block into the per-controller preferences
   array at 0x46b820 (4 entries of 0x18 bytes; entry layout matches the block
   built by input_abstraction_initialize).  Bytes +0x10/+0x11 of the incoming
   block are the start/back button slots and must still hold their identity
   values (0x0c/0x0d) -- the engine refuses remapped start/back. */
void input_abstraction_update_local_player_preferences(short controller_index,
                                                       void *preferences)
{
  if (controller_index < 0 || controller_index >= MAXIMUM_GAMEPADS) {
    display_assert(
      "(controller_index>=0) && (controller_index<MAXIMUM_GAMEPADS)",
      "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x1fb, 1);
    system_exit(-1);
  }
  if (preferences == 0) {
    display_assert("preferences",
                   "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x1fc, 1);
    system_exit(-1);
  }
  if (((char *)preferences)[0x10] != '\x0c' ||
      ((char *)preferences)[0x11] != '\x0d') {
    display_assert(
      "invalid controller preferences; can't remap start & back buttons",
      "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x1ff, 1);
    system_exit(-1);
  }
  csmemcpy((char *)0x46b820 + controller_index * 0x18, preferences, 0x18);
}

/* Return the per-local-player input state slot.  The array lives at 0x46b880
   with a 0x1c-byte stride; element contents are not yet recovered, so the
   return type stays an opaque pointer. */
void *input_abstraction_get_input_state(short local_player_index)
{
  if (local_player_index < 0 || local_player_index >= MAXIMUM_GAMEPADS) {
    display_assert(
      "(local_player_index>=0) && (local_player_index<MAXIMUM_GAMEPADS)",
      "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x209, 1);
    system_exit(-1);
  }
  return (char *)0x46b880 + local_player_index * 0x1c;
}

/* React to a controller hot-plug/removal event reported by
   input_get_device_states (0xcf830), which passes a bit field of the devices
   that changed.  Does nothing until input_abstraction_initialize has set the
   ready flag at 0x46b8f8.

   Any change stops bink playback, but only once the input system has been
   settled for 2 seconds: 0x46b8f0 is the last mark_time/initialize timestamp,
   and 0x46b8fc is the timestamp of the first change in the 0xfff000 class
   (0 until one is seen).  Bits 0xfff000 additionally arm 0x46b8fc.  The
   message text (including its "to due" typo) is verbatim from 0x281094. */
void input_abstraction_update_device_changes(unsigned int device_change_flags)
{
  if (*(char *)0x46b8f8 == '\0') {
    return;
  }
  if (device_change_flags != 0 &&
      ((unsigned int)(system_milliseconds() - *(unsigned int *)0x46b8f0) >=
         2000 ||
       (*(unsigned int *)0x46b8fc != 0 &&
        (unsigned int)(system_milliseconds() - *(unsigned int *)0x46b8fc) >=
          2000))) {
    error(2, "stopping bink playback to due to change in input devices");
    bink_playback_stop();
  }
  if ((device_change_flags & 0xfff000) != 0 && *(unsigned int *)0x46b8fc == 0) {
    *(unsigned int *)0x46b8fc = system_milliseconds();
  }
}

void input_abstraction_initialize(void)
{
  int i;
  char *entry;

  csmemset((void *)0x46b820, 0, 0xdc);
  entry = (char *)0x46b828;
  for (i = 0; i < 4; i++) {
    *(float *)(entry - 8) = 120.0f;
    *(float *)(entry - 4) = 60.0f;
    entry[0] = 0;
    entry[1] = 4;
    entry[2] = 2;
    entry[3] = 3;
    entry[4] = 1;
    entry[5] = 5;
    entry[6] = 6;
    entry[7] = 7;
    entry[8] = 0xc;
    entry[9] = 0xd;
    entry[10] = 0xe;
    entry[11] = 0xf;
    *(int16_t *)(entry + 12) = 0;
    entry[14] = 0;
    entry[15] = 0;
    *(char *)(0x46b8f4 + i) = (char)input_has_gamepad(i);
    entry += 0x18;
  }
  *(int *)0x46b8f0 = system_milliseconds();
  *(char *)0x46b8f8 = 1;
}

void input_abstraction_update(void)
{
  float fVar1;
  float fVar2;
  float fVar4;
  float fVar5;
  char cVar7;
  uint16_t uVar9;
  uint16_t uVar10;
  int16_t sVar11;
  void *pvVar12;
  uint8_t *pbVar13;
  uint8_t *puVar14;
  int iVar17;
  int iVar18;
  float rx;
  float ry;
  float lx;
  float ly;
  int local_20;
  int local_18;
  double lang_d;
  double rang_d;
  float lang, rang;
  float sin_abs, cos_val;
  double dominant;
  float left_diff, right_diff;
  char *state;
  int error_controller;
  int error_code;
  int display_error_flag;
  int i, connected_count;

  tag_block_get_element((char *)game_globals_get() + 0x110, 0, 0x80);

  for (local_18 = 0; local_18 < 4; local_18++) {
    pvVar12 = input_get_gamepad_state(local_18);
    if (pvVar12 != NULL) {
      iVar17 = local_18 * 0x18;
      state = (char *)0x46b880 + local_18 * 0x1c;

      flt_457098[local_18] = *(float *)((char *)0x46b820 + iVar17);
      flt_4570A8[local_18] = *(float *)((char *)0x46b824 + iVar17);

      /* Left stick normalization */
      fVar2 = (float)*(int16_t *)((char *)pvVar12 + 0x22);
      fVar1 = (float)*(int16_t *)((char *)pvVar12 + 0x20);

      lang = (float)x87_atan2((double)fVar2, (double)fVar1);
      lang_d = (double)lang;
      sin_abs = x87_fabs(x87_fsin(lang));
      cos_val = x87_fabs(x87_fcos(lang));
      if (sin_abs <= cos_val)
        dominant = (double)cos_val;
      else
        dominant = (double)sin_abs;
      dominant = *(double *)0x2573d8 / dominant;

      fVar1 = (float)((double)(fVar1 * *(float *)0x280f80) * dominant);
      if (fVar1 < -1.0f) {
        lx = -1.0f;
      } else if (fVar1 > 1.0f) {
        lx = 1.0f;
      } else {
        lx = fVar1;
      }

      fVar2 = (float)((double)(fVar2 * *(float *)0x280f80) * dominant);
      if (fVar2 < -1.0f) {
        ly = -1.0f;
      } else if (fVar2 > 1.0f) {
        ly = 1.0f;
      } else {
        ly = fVar2;
      }

      /* Right stick normalization */
      fVar2 = (float)*(int16_t *)((char *)pvVar12 + 0x26);
      fVar1 = (float)*(int16_t *)((char *)pvVar12 + 0x24);

      rang = (float)x87_atan2((double)fVar2, (double)fVar1);
      rang_d = (double)rang;
      sin_abs = x87_fabs(x87_fsin(rang));
      cos_val = x87_fabs(x87_fcos(rang));
      if (sin_abs <= cos_val)
        dominant = (double)cos_val;
      else
        dominant = (double)sin_abs;
      dominant = *(double *)0x2573d8 / dominant;

      fVar1 = (float)((double)(fVar1 * *(float *)0x280f80) * dominant);
      if (fVar1 < -1.0f) {
        rx = -1.0f;
      } else if (fVar1 > 1.0f) {
        rx = 1.0f;
      } else {
        rx = fVar1;
      }

      fVar2 = (float)((double)(fVar2 * *(float *)0x280f80) * dominant);
      if (fVar2 < -1.0f) {
        ry = -1.0f;
      } else if (fVar2 > 1.0f) {
        ry = 1.0f;
      } else {
        ry = fVar2;
      }

      /* Remap buttons */
      pbVar13 = (uint8_t *)((char *)0x46b829 + iVar17);
      puVar14 = (uint8_t *)state + 1;
      for (iVar18 = 2; iVar18 != 0; iVar18--) {
        puVar14[-1] = *(uint8_t *)(pbVar13[-1] + 0x10 + (int)pvVar12);
        puVar14[0] = *(uint8_t *)(pbVar13[0] + 0x10 + (int)pvVar12);
        puVar14[1] = *(uint8_t *)(pbVar13[1] + 0x10 + (int)pvVar12);
        puVar14[2] = *(uint8_t *)(pbVar13[2] + 0x10 + (int)pvVar12);
        puVar14[3] = *(uint8_t *)(pbVar13[3] + 0x10 + (int)pvVar12);
        puVar14[4] = *(uint8_t *)(pbVar13[4] + 0x10 + (int)pvVar12);
        pbVar13 += 6;
        puVar14 += 6;
      }

      sVar11 = *(int16_t *)((char *)0x46b834 + iVar17);
      if (sVar11 == 2 || sVar11 == 3) {
        uVar9 = (short)((lx < 0.0f ? 1 : 0) | (ly < 0.0f ? 2 : 0));
        uVar10 = (short)((rx < 0.0f ? 1 : 0) | (ry < 0.0f ? 2 : 0));

        left_diff = lang - *(float *)((char *)0x280f84 + (int16_t)uVar9 * 4);
        right_diff = rang - *(float *)((char *)0x280f84 + (int16_t)uVar10 * 4);

        fVar4 = x87_sqrt(ly * ly + lx * lx);
        fVar5 = x87_sqrt(ry * ry + rx * rx);

        if ((double)x87_fabs(left_diff) < *(double *)0x281148) {
          if (fabs(lang_d) < *(float *)0x254a58 ||
              fabs(lang_d) > *(float *)0x26af48) {
            local_20 = (lx < 0.0f) ? -1 : 1;
            lx = (float)local_20 * fVar4;
            local_20 = (ly < 0.0f) ? -1 : 1;
            ly = (float)local_20 * fVar4 * (*(double *)0x2573d8 - (double)left_diff * *(double *)0x281140);
          } else {
            local_20 = (ly < 0.0f) ? -1 : 1;
            ly = (float)local_20 * fVar4;
            local_20 = (lx < 0.0f) ? -1 : 1;
            lx = (float)local_20 * fVar4 * (*(double *)0x2573d8 - (double)left_diff * *(double *)0x281140);
          }
        } else {
          if (fabs((double)ly) < fabs((double)lx)) {
            local_20 = (lx < 0.0f) ? -1 : 1;
            ly = 0.0f;
            lx = (float)local_20 * fVar4;
          } else {
            local_20 = (ly < 0.0f) ? -1 : 1;
            lx = 0.0f;
            ly = (float)local_20 * fVar4;
          }
        }

        if ((double)x87_fabs(right_diff) < *(double *)0x281138) {
          if (fabs(rang_d) < *(float *)0x254a58 ||
              fabs(rang_d) > *(float *)0x26af48) {
            local_20 = (rx < 0.0f) ? -1 : 1;
            rx = (float)local_20 * fVar5;
            local_20 = (ry < 0.0f) ? -1 : 1;
            ry = (float)local_20 * fVar5 * (*(double *)0x2573d8 - (double)right_diff * *(double *)0x281140);
          } else {
            local_20 = (ry < 0.0f) ? -1 : 1;
            ry = (float)local_20 * fVar5;
            local_20 = (rx < 0.0f) ? -1 : 1;
            rx = (float)local_20 * fVar5 * (*(double *)0x2573d8 - (double)right_diff * *(double *)0x281140);
          }
        } else {
          if (fabs((double)ry) < fabs((double)rx)) {
            local_20 = (rx < 0.0f) ? -1 : 1;
            ry = 0.0f;
            rx = (float)local_20 * fVar5;
          } else {
            local_20 = (ry < 0.0f) ? -1 : 1;
            rx = 0.0f;
            ry = (float)local_20 * fVar5;
          }
        }
      }

      cVar7 = *(char *)((char *)0x46b836 + iVar17);
      if (cVar7 == '\0' && *(char *)((char *)0x46b837 + iVar17) != '\0') {
        cVar7 = input_abstraction_print_config_control(local_18);
      }

      switch (*(int16_t *)((char *)0x46b834 + iVar17)) {
      case 0:
        if (*(char *)((char *)pvVar12 + 0x1a) != '\0') {
          *(float *)(state + 0x10) = 1.0f;
        } else if (*(char *)((char *)pvVar12 + 0x1b) != '\0') {
          *(float *)(state + 0x10) = -1.0f;
        } else {
          *(float *)(state + 0x10) = -lx;
        }
        if (*(char *)((char *)pvVar12 + 0x18) != '\0') {
          *(float *)(state + 0xc) = 1.0f;
        } else if (*(char *)((char *)pvVar12 + 0x19) != '\0') {
          *(float *)(state + 0xc) = -1.0f;
        } else {
          *(float *)(state + 0xc) = ly;
        }
        *(float *)(state + 0x14) = -rx;
        if (cVar7 == '\0') {
          *(float *)(state + 0x18) = *(float *)0x2533c8 * ry;
        } else {
          *(float *)(state + 0x18) = *(float *)0x255e94 * ry;
        }
        *(char *)((char *)0x46b8f4 + local_18) = 1;
        break;

      case 1:
        if (*(char *)((char *)pvVar12 + 0x1a) != '\0') {
          *(float *)(state + 0x14) = 1.0f;
        } else if (*(char *)((char *)pvVar12 + 0x1b) != '\0') {
          *(float *)(state + 0x14) = -1.0f;
        } else {
          *(float *)(state + 0x14) = -lx;
        }
        if (*(char *)((char *)pvVar12 + 0x18) != '\0') {
          if (cVar7 != '\0') {
            fVar1 = *(float *)0x255e94;
          } else {
            fVar1 = *(float *)0x2533c8;
          }
        } else {
          sVar11 = (cVar7 == '\0');
          if (*(char *)((char *)pvVar12 + 0x19) != '\0') {
            if (sVar11) {
              fVar1 = *(float *)0x255e94;
            } else {
              fVar1 = *(float *)0x2533c8;
            }
          } else {
            if (cVar7 != '\0') {
              fVar1 = *(float *)0x255e94;
            } else {
              fVar1 = *(float *)0x2533c8;
            }
            fVar1 *= ly;
          }
        }
        *(float *)(state + 0x18) = fVar1;
        *(float *)(state + 0xc) = ry;
        *(float *)(state + 0x10) = -rx;
        *(char *)((char *)0x46b8f4 + local_18) = 1;
        break;

      case 2:
        if (*(char *)((char *)pvVar12 + 0x1a) != '\0') {
          *(float *)(state + 0x14) = 1.0f;
        } else if (*(char *)((char *)pvVar12 + 0x1b) != '\0') {
          *(float *)(state + 0x14) = -1.0f;
        } else {
          *(float *)(state + 0x14) = -lx;
        }
        if (*(char *)((char *)pvVar12 + 0x18) != '\0') {
          *(float *)(state + 0xc) = 1.0f;
        } else if (*(char *)((char *)pvVar12 + 0x19) != '\0') {
          *(float *)(state + 0xc) = -1.0f;
        } else {
          *(float *)(state + 0xc) = ly;
        }
        *(float *)(state + 0x10) = -rx;
        if (cVar7 == '\0') {
          *(float *)(state + 0x18) = *(float *)0x2533c8 * ry;
        } else {
          *(float *)(state + 0x18) = *(float *)0x255e94 * ry;
        }
        *(char *)((char *)0x46b8f4 + local_18) = 1;
        break;

      case 3:
        if (*(char *)((char *)pvVar12 + 0x1a) != '\0') {
          *(float *)(state + 0x10) = 1.0f;
        } else if (*(char *)((char *)pvVar12 + 0x1b) != '\0') {
          *(float *)(state + 0x10) = -1.0f;
        } else {
          *(float *)(state + 0x10) = -lx;
        }
        if (*(char *)((char *)pvVar12 + 0x18) != '\0') {
          if (cVar7 != '\0') {
            fVar1 = *(float *)0x255e94;
          } else {
            fVar1 = *(float *)0x2533c8;
          }
        } else {
          sVar11 = (cVar7 == '\0');
          if (*(char *)((char *)pvVar12 + 0x19) != '\0') {
            if (sVar11) {
              fVar1 = *(float *)0x255e94;
            } else {
              fVar1 = *(float *)0x2533c8;
            }
          } else {
            if (cVar7 != '\0') {
              fVar1 = *(float *)0x255e94;
            } else {
              fVar1 = *(float *)0x2533c8;
            }
            fVar1 *= ly;
          }
        }
        *(float *)(state + 0x18) = fVar1;
        *(float *)(state + 0xc) = ry;
        *(float *)(state + 0x14) = -rx;
        *(char *)((char *)0x46b8f4 + local_18) = 1;
        break;

      default:
        error(2, "unknown joystick preset");
        *(char *)((char *)0x46b8f4 + local_18) = 1;
        break;
      }
    } else {
      if (*(char *)((char *)0x46b8f4 + local_18) != '\0') {
        error_controller = local_18;
        if (main_menu_is_active()) {
          connected_count = 0;
          i = 0;
          do {
            if (*(char *)((char *)0x46b8f4 + i) != '\0')
              connected_count++;
            i++;
          } while (i < 4);

          display_error_flag = 0;
          error_code = 0xd;
          if (connected_count < 2) {
            if (player_ui_get_single_player_local_player_controller(0) != local_18 &&
                player_ui_get_single_player_local_player_controller(1) != local_18 &&
                player_ui_get_single_player_local_player_controller(2) != local_18 &&
                player_ui_get_single_player_local_player_controller(3) != local_18 &&
                !player_ui_local_player_wants_to_play_multiplayer(local_18)) {
              error_controller = -1;
            }
          } else {
            if (player_ui_get_single_player_local_player_controller(0) != local_18 &&
                player_ui_get_single_player_local_player_controller(1) != local_18 &&
                player_ui_get_single_player_local_player_controller(2) != local_18 &&
                player_ui_get_single_player_local_player_controller(3) != local_18 &&
                !player_ui_local_player_wants_to_play_multiplayer(local_18)) {
              goto skip_disconnect_ui;
            }
          }
        } else {
          if (network_game_client_get() == NULL) {
            display_error_flag = 1;
            error_code = 0xc;
          } else {
            display_error_flag = 0;
            error_code = 0xd;
          }
          if (local_player_exists((int16_t)local_18) != 1)
            goto skip_disconnect_ui;
        }

        if (FUN_000f5640())
          items_initialize();
        display_error_deferred(error_code, error_controller, display_error_flag, display_error_flag); /* dup-args-ok: display_error_flag pushed twice in pristine at 0x000cf3a8 */
      }
    skip_disconnect_ui:
      *(char *)((char *)0x46b8f4 + local_18) = 0;
    }
  }
}
