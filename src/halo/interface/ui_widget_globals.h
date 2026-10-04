/* ui_widget.c file-scope globals.
 *
 * Repo-local header: the original header path is not binary-proven.  Kept
 * out of types.h because struct definitions added there move the VC71 code
 * of unrelated functions.  Evidence table:
 * recovery/evidence/widget_globals.json. */

#ifndef HALO_INTERFACE_UI_WIDGET_GLOBALS_H
#define HALO_INTERFACE_UI_WIDGET_GLOBALS_H

#include "../../types.h"

/* Per-local-player error queued by display_error_deferred; error_handle -1
 * marks an empty slot. */
typedef struct ui_widget_deferred_error {
  int16_t error_handle;       ///< offset=0x00
  int16_t local_player_index; ///< offset=0x02
  uint8_t a3;                 ///< offset=0x04
  uint8_t a4;                 ///< offset=0x05
} ui_widget_deferred_error_t;
cs(ui_widget_deferred_error_t, 0x06);
co(ui_widget_deferred_error_t, error_handle, 0x00);
co(ui_widget_deferred_error_t, local_player_index, 0x02);
co(ui_widget_deferred_error_t, a3, 0x04);
co(ui_widget_deferred_error_t, a4, 0x05);

/* Per-local-player error held back while a cinematic plays. */
typedef struct ui_widget_cinematic_deferred_error {
  int16_t error_handle; ///< offset=0x00
  uint8_t field_02;     ///< offset=0x02
  uint8_t field_03;     ///< offset=0x03
} ui_widget_cinematic_deferred_error_t;
cs(ui_widget_cinematic_deferred_error_t, 0x04);
co(ui_widget_cinematic_deferred_error_t, error_handle, 0x00);
co(ui_widget_cinematic_deferred_error_t, field_02, 0x02);
co(ui_widget_cinematic_deferred_error_t, field_03, 0x03);

/* ui_widget_definition_t.flags bit 10, named by the assert string
 * "if the _widget_pass_handled_events_to_all_children_bit flag is checked,
 * _widget_pass_unhandled_events_to_children_bit must also be checked"
 * (bit 0 is UI_WIDGET_PASS_UNHANDLED_EVENTS_TO_CHILDREN_FLAG). */
#define UI_WIDGET_PASS_HANDLED_EVENTS_TO_ALL_CHILDREN_FLAG 0x400

/* widget_globals: 0x68-byte block at 0x46cc20, zeroed by
 * ui_widgets_initialize.  Members are spelled widget_globals_<member> and
 * addressed from widget_globals_base rather than through a struct type: a
 * struct definition here moves the VC71 code of
 * widget_instance_process_one_event_recursive (-2 bytes).  Names: assert
 * strings "widget_globals.initialized" and
 * "widget_globals.initialization_thread==NULL" (T1); the rest are offsets. */
#define widget_globals_base ((char *)0x46cc20)
#define WIDGET_GLOBALS_SIZE 0x68

#define widget_globals_field_00 ((int *)(widget_globals_base + 0x00))
#define widget_globals_field_10 ((int *)(widget_globals_base + 0x10))
#define widget_globals_field_20 (*(uint32_t *)(widget_globals_base + 0x20))
#define widget_globals_field_24 (*(int *)(widget_globals_base + 0x24))
#define widget_globals_field_28 (*(int16_t *)(widget_globals_base + 0x28))
#define widget_globals_field_2a (*(int16_t *)(widget_globals_base + 0x2a))
#define widget_globals_field_2c (*(float *)(widget_globals_base + 0x2c))
#define widget_globals_field_30 \
  ((ui_widget_deferred_error_t *)(widget_globals_base + 0x30))
#define widget_globals_field_48 (*(int16_t *)(widget_globals_base + 0x48))
#define widget_globals_field_4a (*(uint8_t *)(widget_globals_base + 0x4a))
#define widget_globals_field_4c \
  ((ui_widget_cinematic_deferred_error_t *)(widget_globals_base + 0x4c))
#define widget_globals_initialization_thread \
  (*(void **)(widget_globals_base + 0x5c))
#define widget_globals_field_60 (*(int16_t *)(widget_globals_base + 0x60))
#define widget_globals_initialized (*(bool *)(widget_globals_base + 0x62))
#define widget_globals_field_63 (*(uint8_t *)(widget_globals_base + 0x63))
#define widget_globals_field_64 (*(uint8_t *)(widget_globals_base + 0x64))
#define widget_globals_field_65 (*(uint8_t *)(widget_globals_base + 0x65))
#define widget_globals_field_66 (*(uint8_t *)(widget_globals_base + 0x66))
#define widget_globals_field_67 (*(uint8_t *)(widget_globals_base + 0x67))

/* Points at the stack memory pool at 0x31a018, whose name field is the
 * string "widget_memory_pool". */
#define widget_memory_pool (*(void **)0x31e04c)

/* Byte just past widget_globals; not cleared with it. */
#define byte_46CC88 (*(uint8_t *)0x46cc88)

/* Per local player (stride 4), one timestamp for each of event buttons
 * 8..0xb, taken from widget_globals_field_20. */
#define dword_46CC90 ((uint32_t *)0x46cc90)

/* Set by set_ui_plasma_effect_color and around plasma-fx widget draws;
 * read by draw_bitmap_in_rect. */
#define ui_plasma_effect_color (*(real_argb_color *)0x5aa460)

#endif /* HALO_INTERFACE_UI_WIDGET_GLOBALS_H */
