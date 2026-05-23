# Goal-Lift Progress Log

Goal: 30 functions at >=90% VC71

| # | function | addr | source_file | screen_result | vc71 | action | reason |
|---|----------|------|-------------|---------------|------|--------|--------|
| 13 | actor_action_handle_initial_action | 0x1dab0 | actions.c | pass | 97.8% | committed | result=0 preload avoids ESI save |
| 14 | actor_action_handle_pending_command_list | 0x1daf0 | actions.c | pass | 89.3% | reverted | permuter regressed; below 90% threshold |
| 15 | hud_messaging_slot_compare | 0xd50f0 | hud_messaging.c | pass | 100.0% | committed | qsort comparator, 3-level |
| 16 | FUN_000d52e0 | 0xd52e0 | hud_messaging.c | pass | 95.2% | committed | team HUD msg sender |
| 17 | FUN_000cf430 | 0xcf430 | input_xbox.c | pass | 97.1% | committed | saturate byte counter (goto/jle pattern) |
| X | hs_compile_sleep_until | 0xc8c50 | hs_runtime.c | skip | 76.6% | reverted | struct cap in assert-heavy TU |
| 19 | hud_messaging_initialize | 0xd4680 | hud_messaging.c | pass | 100.0% | committed | alloc HUD messaging buffer |
| 20 | FUN_000d46a0 | 0xd46a0 | hud_messaging.c | pass | 100.0% | committed | reset player globals + zero buffer (local var restructure) |
| 21 | hud_get_font_index | 0xd5160 | hud_messaging.c | pass | 100.0% | committed | jle fix + base-var restructure |
| 22 | hud_messaging_globals_update | 0xd51b0 | hud_messaging.c | pass | 100.0% | committed | single-line reset |
| X | hud_get_text_color | 0xd5180 | hud_messaging.c | fail | n/a | ported=false | name mismatch in delinked; ESI staging |
| 23 | FUN_000ce500 | 0xce500 | input_xbox.c | pass | 100.0% | committed | ReadFile wrapper; handle+buf vars force MSVC scheduling |
| 24 | FUN_000cf3e0 | 0xcf3e0 | input_xbox.c | pass | 97.0% | committed | joystick deadzone remap; no-locals → ESI inline-push only |
| 25 | FUN_000cf690 | 0xcf690 | input_xbox.c | pass | 100.0% (per-fn) | committed | xor eax,eax trivial; 21.1% false-neg from nop span in whole-obj |
