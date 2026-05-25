# Goal-Lift Session Progress
Goal: 20 functions at >=90% VC71
Started: 2026-05-25

| function | addr | source_file | screen_result | vc71 | action | reason |
|----------|------|-------------|---------------|------|--------|--------|
| FUN_00076410 | 0x76410 | bitmap_utilities.c | FAIL | - | skipped | callee FUN_00075e70 has in_EAX |
| FUN_00075e70 | 0x75e70 | bitmap_utilities.c | FAIL | - | skipped | has in_EAX |
| FUN_00076300 | 0x76300 | bitmap_utilities.c | FAIL | - | skipped | calls FUN_00075e70 (in_EAX) |
| FUN_00075800 | 0x75800 | bitmap_utilities.c | FAIL | - | skipped | callee FUN_00075380 has @eax |
| FUN_00075a20 | 0x75a20 | bitmap_utilities.c | FAIL | - | skipped | callee FUN_00075380 has @eax |
| FUN_00076790 | 0x76790 | bitmap_utilities.c | FAIL | - | skipped | calls FUN_00076300 (reg-arg chain) |
| FUN_00078c30 | 0x78c30 | bitmap_utilities.c | FAIL | - | skipped | has unaff_ESI |
| FUN_000796e0 | 0x796e0 | bitmap_utilities.c | FAIL | - | skipped | has unaff_ESI/EDI/BX |
| FUN_000798e0 | 0x798e0 | bitmap_utilities.c | FAIL | - | skipped | calls FUN_000796e0 (reg args) |
| FUN_00079bb0 | 0x79bb0 | bitmap_utilities.c | FAIL | - | skipped | calls FUN_000796e0 (reg args) |
| FUN_0007b310 | 0x7b310 | bitmap_utilities.c | FAIL | - | skipped | calls FUN_00078c30 (unaff_ESI) |
| FUN_00077720 | 0x77720 | bitmap_utilities.c | FAIL | - | skipped | has in_EAX |
| FUN_00077cd0 | 0x77cd0 | bitmap_utilities.c | FAIL | - | skipped | calls FUN_00077720 (in_EAX) |
| FUN_00079250 | 0x79250 | bitmap_utilities.c | FAIL | - | skipped | has @eax |
| FUN_00079480 | 0x79480 | bitmap_utilities.c | FAIL | - | skipped | calls FUN_00079250 (@eax) |
| FUN_001165b0 | 0x1165b0 | circular_queue.c | FAIL | - | skipped | has in_EAX and unaff_ESI |
| bitmap_fade | 0x77e60 | bitmap_utilities.c | PASS | 95.8% | committed | clean callees, no reg args |
| FUN_00076bd0 | 0x76bd0 | bitmap_utilities.c | PASS | 87.0% | hold-permute | structural ceiling (reg alloc, loop inversion); permutation pending |
| FUN_00077120 | 0x77120 | bitmap_utilities.c | PASS | 80.1% | reverted | structural cap (MSVC codegen, return type, prologue) |
| FUN_001168e0 | 0x1168e0 | circular_queue.c | FAIL | - | skipped | callee FUN_00116390 has @eax/@ebx/@esi |
| FUN_0010c390 | 0x10c390 | random_math.c | PASS | 74.3% | reverted | structural cap (FIMUL vs FILD+FMUL, clang vs MSVC x87) |
| FUN_0010c3c0 | 0x10c3c0 | random_math.c | FAIL | - | skipped | callee FUN_001d94f0 has in_ST0 |
| FUN_0010c440 | 0x10c440 | random_math.c | FAIL | - | skipped | callee FUN_001d94f0 has in_ST0 |
| FUN_00116280 | 0x116280 | circular_queue.c | PASS | 55.0% | reverted | lift bug (55% < 65%); MSVC param-slot-reuse + register alloc divergence |
| FUN_000145c0 | 0x145c0 | actor_looking.c | PASS | 100.0% | committed | ai_conversation_finish wrapper |
| FUN_000145f0 | 0x145f0 | actor_looking.c | PASS | 100.0% | committed | duplicate of FUN_000145c0 for different calling context |
| FUN_000146f0 | 0x146f0 | actor_looking.c | PASS | 92.6% | committed | off-by-one fix: >= 4 not > 4 for comparison constant |
| FUN_00014b40 | 0x14b40 | actor_looking.c | PASS | 100.0% | committed | unit_stop_running_blindly wrapper |
| FUN_00014b70 | 0x14b70 | actor_looking.c | PASS | 96.0% | committed | set control-override handle + controlled flag |
| FUN_00014ba0 | 0x14ba0 | actor_looking.c | FAIL | 74.6% | reverted | structural cap: MSVC subbase ptr optimization (add eax,0x9c then [eax+0xc]) |
| FUN_00014ff0 | 0x14ff0 | actor_looking.c | PASS | 97.0% | committed | conditional replace of actor+0xb8 field |
| FUN_00015020 | 0x15020 | actor_looking.c | PASS | 100.0% | committed | off-by-one fix: >= 9 && <= 0xc |
| ai_debug_initialize | 0x48e90 | ai_debug.c | PASS | 92.0% | committed | already implemented; just wired kb.json |
| ai_debug_dispose | 0x48f50 | ai_debug.c | PASS | 100.0% | committed | already implemented; just wired kb.json |
| ai_debug_dispose_from_old_map | 0x48fa0 | ai_debug.c | PASS | 100.0% | committed | already implemented; just wired kb.json |
| ai_debug_clear_storage | 0x49000 | ai_debug.c | PASS | 100.0% | committed | already implemented; just wired kb.json |
| ai_debug_actor_deleted | 0x49080 | ai_debug.c | PASS | 100.0% | committed | already implemented; just wired kb.json |
| ai_debug_select_encounter | 0x49220 | ai_debug.c | PASS | 95.8% | committed | already implemented; just wired kb.json |
| ai_debug_select_actor | 0x4b1b0 | ai_debug.c | PASS | 100.0% | committed | already implemented; just wired kb.json |
| ai_debug_initialize_for_new_map | 0x4c0f0 | ai_debug.c | PASS | 100.0% | committed | already implemented; just wired kb.json |
| FUN_00014ba0 | 0x14ba0 | actor_looking.c | FAIL | 74.6% | reverted | structural cap: MSVC instruction count mismatch (24 vs 35) |
| FUN_00015150 | 0x15150 | actor_looking.c | PASS | 96.4% | committed | look-frame reset + unit_start_running_blindly guard |
| FUN_000151b0 | 0x151b0 | actor_looking.c | PASS | 100.0% | committed | look-timer advance + unit_start_running_blindly on countdown expiry |
| FUN_00027870 | 0x27870 | actor_looking.c | PASS | 100.0% | committed | scripted-look stop: debug log + clear look fields |
| FUN_00017090 | 0x17090 | actor_looking.c | PASS | 100.0% | committed | prop-interest wrapper: actor_look_compute_prop_interest(actor+0x9c) |
