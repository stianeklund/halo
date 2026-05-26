| function | addr | source_file | screen_result | vc71 | action | reason |
|---|---|---|---|---|---|---|
| actor_get_best_damaging_prop | 0x2fa70 | actor_perception.c | pass | 89.3% | committed | ~90% structural cap (ECX scheduling); kept at user request |
| symbol_table_dispose | 0x92090 | profile.c | pass | 100% | committed | byte-match |
| FUN_0002f230 | 0x2f230 | actor_perception.c | pass | 87.8% | committed | copy loop structural cap (rep movsl vs individual movs); kept per user policy |
| actor_perception_acknowledge | 0x2f2b0 | actor_perception.c | pass | 100% | committed | byte-match |
| FUN_0002f380 | 0x2f380 | actor_perception.c | pass | 91.6% | committed | fixed range check form (>=2 && <=3) and tail structure |
| FUN_0002f5b0 | 0x2f5b0 | actor_perception.c | pass | 94.5% | committed | float comparator; local float vars force early FPU stack load |
