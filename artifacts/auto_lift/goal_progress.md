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
