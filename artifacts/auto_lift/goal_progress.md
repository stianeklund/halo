| function | addr | source_file | screen_result | vc71 | action | reason |
|---|---|---|---|---|---|---|
| FUN_0012c6d0 | 0x12c6d0 | network_server_manager.c | pass | 96.7% | committed | unique name picker; outer do-while loop |
| FUN_0012c750 | 0x12c750 | network_server_manager.c | pass | 96.1% | committed | unique team picker; char unique fix |
| actor_get_best_damaging_prop | 0x2fa70 | actor_perception.c | pass | 89.3% | committed | ~90% structural cap (ECX scheduling); kept at user request |
| symbol_table_dispose | 0x92090 | profile.c | pass | 100% | committed | byte-match |
| FUN_0002f230 | 0x2f230 | actor_perception.c | pass | 87.8% | committed | copy loop structural cap (rep movsl vs individual movs); kept per user policy |
| actor_perception_acknowledge | 0x2f2b0 | actor_perception.c | pass | 100% | committed | byte-match |
| FUN_0002f380 | 0x2f380 | actor_perception.c | pass | 91.6% | committed | fixed range check form (>=2 && <=3) and tail structure |
| FUN_0002f5b0 | 0x2f5b0 | actor_perception.c | pass | 94.5% | committed | float comparator; local float vars force early FPU stack load |
| FUN_00118620 | 0x118620 | circular_queue.c (byte_swapping.c) | pass | 69.2% | skipped | structural ceiling: 8-byte SHRD/SHLD byte-swap pattern unrecoverable from Ghidra; __int64 butterfly got 65.6% |
| FUN_001179e0 | 0x1179e0 | circular_queue.c (uncompress.c) | pass | 92.2% | committed | int z[14] for z_stream; inflate_ret declared in if block; != 1 branch form for JZ-to-success codegen |
| FUN_0002a3f0 | 0x2a3f0 | actor_looking.c | pass | 100% | committed | actor state check; path_active && !is_moving → 0 else 1 |
| FUN_0002a430 | 0x2a430 | actor_looking.c | pass | 90.9% | committed | activation state getter; returns +0x470 if state==3 else -1 |
| midpoint3d | 0x2a540 | actor_moving.c | pass | 100% | committed | pure FPU (a+b)*0.5f per component; FPU-WARN benign (register naming only) |
| actor_get_stopping_distances | 0x2a610 | actor_moving.c | pass | 96.8% | committed | bipd/vehicle branch; float dot product with FLD loads; 2.0f divisor |
| actor_aim_jump | 0x2ace0 | actor_moving.c | pass | 91.0% | committed | jump-aim active flag; velocity vector with magnitude clamp |
| FUN_00014ba0 | 0x14ba0 | actor_looking.c | pass | 73.3% | skipped | structural ceiling: MSVC tail-merge prevents separate RET per branch; 4 attempts max 76.7% |
| FUN_00016c80 | 0x16c80 | actor_looking.c | pass | 100% | committed | scenario guard-zone boundary callback; tag_block_get_element + flag mask |
| FUN_0001ab70 | 0x1ab70 | actor_looking.c | pass | 95.8% | committed | per-function delink; int copy loop (v+b0/b4/b8); NOP boundary excluded |
| FUN_0002a330 | 0x2a330 | actor_looking.c | pass | 100% | committed | set actor+0x402 and actor+0x46e flags; byte-match |
| FUN_00015250 | 0x15250 | actor_looking.c | pass | 91.9% | committed | >0 condition (jle) + char move_result + goto tail for prop branch |
| FUN_0002d900 | 0x2d900 | actor_looking.c | pass | 61.4% | skipped | structural ceiling: rep movsl 6-dword copy; C csmemcpy generates CALL |
| FUN_00014620 | 0x14620 | actor_looking.c | pass | 100% | committed | state init: assert+csmemset+char return; per-function delink (NOP boundary) |
| FUN_00016960 | 0x16960 | actor_looking.c | pass | 100% | committed | three-way float comparator; FLD param1/FCOMP param2 both passes; byte-match |
