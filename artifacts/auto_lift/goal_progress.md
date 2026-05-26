# Goal-Lift Progress Log

| function | addr | source_file | screen_result | vc71 | action | reason |
|----------|------|-------------|---------------|------|--------|--------|
| FUN_00098200 | 0x98200 | contrails.c | pass | 81.9% | reverted | structural ceiling (fucompp/fcomp, FPU scheduler) — below 90% |
| FUN_0010c390 | 0x10c390 | random_math.c | pass | 74.3% | reverted | structural ceiling (fimull vs fildl+fmulp, FPU scheduling reorder) |
| FUN_0009f430 | 0x9f430 | effects.c | pass | 95.7% | committed | existing impl verified; FPU-WARN is commutative mult ordering |
| FUN_00039c80 | 0x39c80 | actors.c | pass | 87.1% | reverted | structural ceiling (7× FPU comparison idiom + scheduling) |
| FUN_00116280 | 0x116280 | circular_queue.c | pass | 89.9% | reverted | structural ceiling (stack prologue + jae/jbe comparison direction) |
| FUN_00116280 | 0x116280 | circular_queue.c | pass | 95.9% | committed | fresh re-lift with unsigned arithmetic; direct permuter skip (already >=90%) |
| memory_pool_allocation_size | 0x11e330 | memory_pool.c | pass | 100% | committed | trivial offset function |
| memory_pool_initialize | 0x11e340 | memory_pool.c | pass | 100% | committed | pool header init |
| memory_pool_get_free_size | 0x11e390 | memory_pool.c | pass | 100% | committed | simple field getter |
| memory_pool_get_used_size | 0x11e3a0 | memory_pool.c | pass | 100% | committed | conditional pointer arithmetic |
| memory_pool_get_contiguous_free_size | 0x11e3c0 | memory_pool.c | pass | 75% | reverted | structural ceiling (MSVC xor/sub merge idiom — two-return path vs single-return) |
| memory_pool_new | 0x11e650 | memory_pool.c | pass | 100% | committed | debug_malloc + initialize wrapper |
| memory_pool_delete | 0x11e690 | memory_pool.c | pass | 97.1% | committed | validate + csmemset + debug_free |
| FUN_000db140 | 0xdb140 | event_manager.c | pass | 100% | committed | empty stub |
| FUN_000db1b0 | 0xdb1b0 | event_manager.c | pass | 100% | committed | empty stub |
| FUN_000dc730 | 0xdc730 | event_manager.c | pass | 100% | committed | animation_update_internal wrapper |
| game_engine_get_variant | 0xa9350 | game_engine.c | pass | 100% | committed | returns &DAT_00456af8 |
| FUN_000dc7a0 | 0xdc7a0 | first_person_weapons.c | pass | 85.7% | reverted | structural ceiling (OR EDI,-1 sentinel register idiom) |
| game_engine_statistics_append | 0xa83e0 | game_engine.c | pass | 80% | reverted | structural ceiling (MSVC indirect tail-call JMP EAX) |
| game_engine_handle_client_message | 0xa8400 | game_engine.c | pass | 80% | reverted | structural ceiling (MSVC indirect tail-call JMP EAX) |
| game_engine_handle_server_message | 0xa8420 | game_engine.c | pass | 80% | reverted | structural ceiling (MSVC indirect tail-call JMP EAX) |
| motion_sensor_initialize | 0xdb0f0 | event_manager.c | pass | 100% | committed | game_state_malloc + assert |
| FUN_000db150 | 0xdb150 | event_manager.c | pass | 100% | committed | motion sensor slot table init |
| first_person_weapon_get_local_index | 0xdd110 | first_person_weapons.c | pass | 96.5% | committed | slot search loop |
| FUN_000de360 | 0xde360 | first_person_weapons.c | pass | 86.2% | reverted | structural ceiling (thunk overhead for @<edi>/@<eax>/@<ebx> callees) |
| FUN_00118620 | 0x118620 | circular_queue.c | pass | 68.0% | reverted | structural ceiling (8-byte swap uses SHRD/SHLD from __int64 source) |
| memory_pool_compact | 0x11e840 | memory_pool.c | pass | 86.4% | reverted | structural ceiling (@<edi> call overhead for FUN_0011e430 = 2 extra insns; permuter regressed to 75%) |
| memory_pool_block_resize | 0x11e8a0 | memory_pool.c | pass | 91.3% | committed | permuter improved 88.6→91.3% (new_var pointer for pool->free_size) |
| FUN_001176a0 | 0x1176a0 | circular_queue.c | pass | 79.2% | reverted | structural ceiling (register-arg callees: FUN_00116390 @<eax>/<ebx>/<esi>, FUN_001171a0 @<ecx>/<edx>/<eax>) |
| FUN_001176f0 | 0x1176f0 | circular_queue.c | pass | 64.9% | reverted | structural ceiling (4x FUN_00116390 @<reg>, 2x FUN_001170b0 @<eax>) |
| FUN_00114fa0 | 0x114fa0 | circular_queue.c | pass | 70.7% | reverted | structural ceiling (complex inflate_codes loop: switch/goto/do-while → MSVC/clang divergence) |
| FUN_0003e570 | 0x3e570 | actors.c | pass | 87.2% | reverted | structural ceiling (register alloc + LEA/AND vs ADD/SHL csmemset size; permuter: no improvement) |
| FUN_0010c3c0 | 0x10c3c0 | random_math.c | skipped | n/a | skipped | structural ceiling (FPU calling convention: callee FUN_001d94f0 reads float10 in_ST0 — unreplicable from C) |
| FUN_0010c440 | 0x10c440 | random_math.c | skipped | n/a | skipped | structural ceiling (same FPU calling convention dependency as 0x10c3c0) |
| hud_messaging_initialize_for_new_map | 0xd5ff0 | hud_messaging.c | pass | 100% | committed | single csmemset call |
| hud_messaging_dispose_from_old_map | 0xd6010 | hud_messaging.c | pass | 100% | committed | empty stub |
| hud_messaging_dispose | 0xd6020 | hud_messaging.c | pass | 100% | committed | empty stub |
| FUN_00172590 | 0x172590 | rasterizer.obj | skipped | n/a | skipped | __FILE__=rasterizer_xbox_shadows.c but source file not yet in tree; skip |
| ai_communication_initialize | 0x42a30 | ai_communication.c | pass | 88% | skipped | structural ceiling (int16_t counter: xorw/incw vs xorl/incl — MSVC 16-bit reg use; ported=false) |
| ai_communication_initialize_for_new_map | 0x42b90 | ai_communication.c | pass | 94.6% | committed | removed g-cache var + do-while + MOVSX loop counter |
| ai_communication_dispose_from_old_map | 0x42ca0 | ai_communication.c | pass | 100% | committed | single-call wrapper |
