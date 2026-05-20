# Goal-Lift Progress — 30 functions at >=90% VC71

| function | addr | source_file | screen_result | vc71 | action | reason |
|----------|------|-------------|---------------|------|--------|--------|
| FUN_0010c7d0 | 0x10c7d0 | random_math.c | pass | 90.4% | committed | 3D slerp; fucompp→fcomps fix |
| FUN_0010c390 | 0x10c390 | random_math.c | pass | 88.9% | reverted | scheduling cap; flds hoisting; permuter failed |
| FUN_0010c3c0 | 0x10c3c0 | random_math.c | pass | 70.5% | reverted | structural cap: FPU-stack acosf ABI |
| FUN_0010c440 | 0x10c440 | random_math.c | pass | 84.1% | reverted | structural cap: FPU-stack acosf ABI |
| FUN_0014eeb0 | 0x14eeb0 | collision_usage | __fastcall+in_EAX | - | skipped | register arg artifacts |
| FUN_0014ef30 | 0x14ef30 | collision_usage | __fastcall+in_EAX | - | skipped | register arg artifacts |
| FUN_0014ef80 | 0x14ef80 | collision_usage | in_EAX+unaff_EBX | - | skipped | register arg artifacts |
| FUN_0014f020 | 0x14f020 | collision_usage | unaff_EBX | - | skipped | register arg artifacts |
| FUN_000d1dd0 | 0xd1dd0 | hud.c | pass | 68.6% | skipped | /QIfist structural cap: reference uses fistpl, VC71 generates _ftol2 |
| rumble_player_set_scripted_values | 0xb9b80 | player_rumble.c | pass | 66.7% | skipped | XDK double-dereference structural cap: global ptr gets extra load in XDK build |
| FUN_000d1e90 | 0xd1e90 | hud.c | pass | 92.6% | committed | ARGB alpha+intensity -> color[4] array; array decl needed to prevent dead-store elim |
| hud_initialize_for_new_map | 0xd0360 | hud.c | pass | 98.5% | committed | Two separate interface_get_tag_index(6) calls; 0x46bd10 raw pointer access |
| FUN_000d1690 | 0xd1690 | hud.c | pass | 100% | committed | 2-instruction leaf: FLD [0x2533c8]; RET |
| real_local_random | 0x9ce80 | effects.c | pass | 80% | reverted | pop ecx vs add $4,%esp structural cap |
| FUN_0014d1b0 | 0x14d1b0 | collision_usage | ESI/EDI reg args in asm | - | skipped | disasm reveals ESI/EDI register args |
| update_client_add_player | 0xb8f00 | player_queues_new.c | pass | 100% | committed | data_new_datum assert pattern; clean __cdecl |
| update_client_queue | 0xb8f40 | player_queues_new.c | pass | 68.4% | reverted | structural cap: explicit int* loop vs REP MOVSD (ESI/EDI) |
| update_client_queue_push | 0xb8f70 | player_queues_new.c | pass | 100% | committed | counter reset + csmemset; clean leaf |
| player_new_queue | 0xb8ff0 | player_queues_new.c | pass | 100% | committed | data_new_datum assert pattern; returns queue_index |
| FUN_001bdd40 | 0x1bdd40 | predicted_resources.c | pass | 100% | committed | 2-insn leaf: MOV EAX,[0x4e9254]; RET |
| FUN_001bdd50 | 0x1bdd50 | predicted_resources.c | pass | 100% | committed | 2-insn leaf: MOV EAX,[0x4e9258]; RET |
| FUN_001bdd60 | 0x1bdd60 | predicted_resources.c | pass | 100% | committed | 2-insn leaf: MOV EAX,[0x4e925c]; RET |
| FUN_001bdd70 | 0x1bdd70 | predicted_resources.c | pass | 100% | committed | 2-insn leaf: MOV EAX,[0x4e9260]; RET |
| FUN_001bde90 | 0x1bde90 | predicted_resources.c | pass | 100% | committed | data_dispose + lruv_cache_dispose + zero flag |
| FUN_001bdec0 | 0x1bdec0 | predicted_resources.c | pass | 100% | committed | data_delete_all single call |
| FUN_000b97b0 | 0xb97b0 | player_queues_new.c | pass | 90.5% | committed | update action buffer slot; boundary artifact in wide ref corrected to b9880 |
