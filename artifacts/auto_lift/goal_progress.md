# Goal Lift Progress Log
Goal: 20 functions at >=90% VC71
Started: 2026-05-20

| function | addr | source_file | screen_result | vc71 | action | reason |
|---|---|---|---|---|---|---|
| circular_queue_reset | 0x118d60 | memory/circular_queue.c | pass | 100.0% | committed | byte-match |
| rumble_player_set_scripted_values | 0xb9b80 | game/player_rumble.c | pass | 66.7% | reverted | HDATA dllimport double-deref cap |
| rumble_player_set_scale | 0xb9ba0 | game/player_rumble.c | pass | 66.7% | reverted | same dllimport cap |
| update_client_queue | 0xb8f40 | game/player_rumble.c | pass | 68.4% | reverted | REP MOVSD structural cap |
| FUN_0010c390 | 0x10c390 | math/random_math.c | pass | 74.3% | reverted | FPU scheduling cap |
| circular_queue_size | 0x118e70 | memory/circular_queue.c | pass | 85.7% | reverted | @eax thunk overhead cap |
| array_resize | 0x117b90 | memory/circular_queue.c | pass | 91.1% | committed | meets policy |
| FUN_00117da0 (array_add_element) | 0x117da0 | memory/circular_queue.c | pass | 94.6% | committed | meets policy |
| FUN_00117ff0 (array_delete_element) | 0x117ff0 | memory/circular_queue.c | pass | 95.2% | committed | meets policy |
| real_local_random | 0x9ce80 | effects/effects.c | pass | 80.0% | reverted | frame pointer cap (5-insn function) |
| FUN_00118620 (byte_swap_memory) | 0x118620 | memory/circular_queue.c | pass | 65.0% | reverted | SHRD/SHLD 8-byte loop structural cap |
| FUN_00119df0 | 0x119df0 | memory/data.c | pass | 90.7% | committed | meets policy (param_3<=0 fix) |
| FUN_00119ef0 | 0x119ef0 | memory/data.c | pass | 98.9% | committed | near byte-match (single-exit rewrite) |
| FUN_000d16a0 | 0xd16a0 | interface/hud.c | pass | 72.5% | reverted | stack canary + 21 extra insns structural cap |
