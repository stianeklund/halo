# Goal-Lift Progress: actor_looking.obj — Final Report

## Committed at ≥90% VC71 (25 functions)

| function | addr | vc71 | note |
|----------|------|------|------|
| FUN_00014620 | 0x14620 | 100.0% | init fight action state |
| FUN_00015cf0 | 0x15cf0 | 93.5% | look target finalizer |
| FUN_00016050 | 0x16050 | 93.1% | look target selector |
| FUN_00016210 | 0x16210 | 91.6% | target candidate evaluator |
| FUN_00016cf0 | 0x16cf0 | 90.6% | unit look-at dispatcher |
| FUN_000192b0 | 0x192b0 | 98.6% | encounter look-target probe |
| FUN_00019b20 | 0x19b20 | 91.8% | look-at-object trigger |
| FUN_00019c70 | 0x19c70 | 90.7% | look direction clamper |
| FUN_0001a670 | 0x1a670 | 96.5% | look prop reference updater |
| FUN_0001ac00 | 0x1ac00 | 92.9% | look prop init copy |
| FUN_000272d0 | 0x272d0 | 91.5% | look-target selector w/ patrol |
| FUN_000278e0 | 0x278e0 | 100.0% | prop importance calculator |
| FUN_0002a2b0 | 0x2a2b0 | 91.3% | look spec reset |
| FUN_000159d0 | 0x159d0 | 91.4% | guard action init |
| FUN_00014540 | 0x14540 | 95.6% | look target scripted init |
| FUN_00015520 | 0x15520 | 90.0% | flee state dispatcher (NEW LIFT) |
| FUN_00015880 | 0x15880 | 90.5% | flee state initializer |
| FUN_00015900 | 0x15900 | 93.1% | flee search state init |
| actor_look_secondary_stop | 0x16bd0 | 100.0% | obey action state init (NEW LIFT) |
| actor_look_compute_prop_interest | 0x16d40 | 90.8% | swarm component dispatcher (NEW LIFT) |
| FUN_00027a60 | 0x27a60 | 94.9% | set secondary look target (NEW LIFT) |
| FUN_0001a600 | 0x1a600 | 96.0% | look vector selector (active) |
| FUN_00015f60 | 0x15f60 | 92.7% | look vector selector (multi-path) |

## ABI fixes committed (ported=false, runtime-safety)
- FUN_00012ad0: actor_handle@<ebx>, action_type@<esi>
- FUN_00013dd0: actor_handle@<eax>, source_pos@<esi>
- FUN_00014e90: actor_handle@<eax>

## Structural/ABI ceilings — bodies written, not activated

| function | addr | vc71 | reason |
|----------|------|------|--------|
| FUN_00013dd0 | 0x13dd0 | 72.6% | @<eax>/@<esi> reg-arg ceiling |
| FUN_00013ef0 | 0x13ef0 | 63.6% | structural bug (47 extra insns) — flagged for re-lift |
| FUN_00014480 | 0x14480 | 88.2% | register-allocator ceiling (permuter exhausted) |
| FUN_00014ba0 | 0x14ba0 | 76.7% | conditional-copy layout mismatch |
| FUN_00014e90 | 0x14e90 | 87.6% | @<eax> reg-arg ceiling |
| FUN_00015040 | 0x15040 | 82.5% | branch-layout structural ceiling |
| FUN_00019940 | 0x19940 | 80.2% | branch-layout ceiling |
| FUN_0001a420 | 0x1a420 | 87.1% | reg-alloc ceiling (permuter ~89%) |
| FUN_0001a7e0 | 0x1a7e0 | 82.4% | structural ceiling (permuter ~86%) |
| FUN_00024ca0 | 0x24ca0 | 80.0% | branch-direction ceiling |
| FUN_00027a10 | 0x27a10 | 77.8% | @<eax> frameless-function ceiling |
| FUN_00029040 | 0x29040 | 69.7% | actor_look_update structural ceiling |
| FUN_000169a0 | 0x169a0 | 66.3% | @<esi> reg-arg ceiling |
| FUN_000197d0 | 0x197d0 | 89.8% | reg-alloc scheduling ceiling |
| FUN_00016590 | 0x16590 | 88.6% | scheduling ceiling |
