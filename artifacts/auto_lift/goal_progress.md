| function | addr | source_file | screen_result | vc71 | action | reason |
|----------|------|-------------|---------------|------|--------|--------|
| actor_move_to_firing_position | 0x2d900 | actor_moving.c | 62.5% vc71 | 64.0% | skipped | structural cap: EBX vs EDI regalloc + rep-movsd pattern; sibling 0x2d850 also 65% |
| actor_mode_name | 0x1d5f0 | actions.c | clean | 100.0% | committed | 100% byte match |
| set_real_vector2d | 0x1d760 | actions.c | clean | 100.0% | committed | 100% byte match |
| actor_action_try_to_seek_cover | 0x1d350 | actions.c | clean | 92.9% | committed | meets threshold |
| FUN_0001d3c0 | 0x1d3c0 | actions.c | clean | 92.1% | committed | meets threshold |
| FUN_0001c030 | 0x1c030 | actions.c | clean | 100.0% | committed | 100% byte match |
| FUN_0001c190 | 0x1c190 | actions.c | clean | 100.0% | committed | 100% byte match |
| FUN_0001c0e0 | 0x1c0e0 | actions.c | clean | 86.2% | pending | below 88%, investigating |
| actor_action_can_stop_conversing | 0x1cfa0 | actions.c | clean | 82.5% | pending | below 88%, investigating |
| FUN_0001cb30 | 0x1cb30 | actions.c | @<eax> | - | skipped | register arg in EAX |
| FUN_0001cda0 | 0x1cda0 | actions.c | __fastcall+in_AL | - | skipped | register args |
| FUN_0001d530 | 0x1d530 | actions.c | @<eax> | - | skipped | register arg in EAX |
| FUN_0001c0e0 | 0x1c0e0 | actions.c | clean | 86.2% | skipped | assert line number mismatch (action_wait.c vs actions.c) |
| actor_action_can_stop_conversing | 0x1cfa0 | actions.c | clean | 82.5% | skipped | register allocation diff |
| errors_output_to_debug_file | 0x8f200 | errors.c | clean | 100.0% | committed | 100% byte match |
| errors_overflow_suppression_enable | 0x8f210 | errors.c | clean | 100.0% | committed | 100% byte match |
| error_get | 0x8f220 | errors.c | clean | 100.0% | committed | 100% byte match |
| errors_initialize | 0x8f370 | errors.c | clean | 100.0% | committed | 100% byte match |
| FUN_0008e680 | 0x8e680 | errors.c | clean | no delinked ref | committed | trivial, disasm-verified |
| FUN_0008e720 | 0x8e720 | errors.c | clean | no delinked ref | committed | trivial, disasm-verified |
| FUN_0008e750 | 0x8e750 | errors.c | clean | no delinked ref | committed | trivial, disasm-verified |
| actor_action_try_to_enter_vehicle | 0x1d420 | actions.c | clean | 96.4% | committed | via subagent |
| actor_get_pursuit_location | 0x1d4f0 | actions.c | clean | 76.9% | skipped | XOR EDX,EDX register scheduling diff |
| FUN_0001d180 | 0x1d180 | actions.c | complex | - | skipped | _ftol2 + param._2_2_ high-word access |
| actor_action_handle_done_fleeing | 0x1f6e0 | actions.c | clean | 90.3% | committed | via subagent |
| actor_action_handle_panic_transition | 0x1dd40 | actions.c | clean | 86.4% | skipped | structural ceiling (regalloc), kept source |
| actor_action_handle_combat_transition | 0x204f0 | actions.c | clean | 94.5% | committed | via subagent |
| actor_action_handle_combat_failure | 0x1f920 | actions.c | clean | 78.5% | skipped | structural ceiling (exit path duplication) |
| actor_action_handle_berserk_transition | 0x20470 | actions.c | clean | 76.2% | skipped | structural ceiling (zero-register pattern) |
| actor_action_handle_grenade_throwing | 0x205a0 | actions.c | clean | 75.9% | skipped | structural ceiling (switch lowering) |
| actor_action_handle_exit_pursuit | 0x1f9a0 | actions.c | clean | 92.2% | committed | via subagent |
| actor_action_set_default_state | 0x1d7c0 | actions.c | clean | 71.1% | skipped | structural ceiling (switch layout, EBX=1 pattern) |
| FUN_00056830 (sort comparator) | 0x56830 | encounters.c | clean | 95.4% | committed | |
| FUN_00058700 (wrapper) | 0x58700 | encounters.c | clean | 40.0% | skipped | tail-call optimization mismatch |
| FUN_00058710 (wrapper) | 0x58710 | encounters.c | clean | 40.0% | skipped | tail-call optimization mismatch |
| FUN_00058ae0 (wrapper) | 0x58ae0 | encounters.c | clean | 40.0% | skipped | tail-call optimization mismatch |
| encounter_link_activation | 0x5a5a0 | encounters.c | clean | 85.2% | skipped | register allocation diff |
| FUN_00058860 (encounter_set_team) | 0x58860 | encounters.c | clean | 100.0% | committed | perfect byte match |
| encounter_set_blind | 0x5ad60 | encounters.c | clean | 100.0% | committed | 100% byte match |
| encounter_set_deaf | 0x5ad90 | encounters.c | clean | 100.0% | committed | 100% byte match |
| encounter_force_activate | 0x5ba70 | encounters.c | clean | 75.9% | skipped | @<eax> tail-call mismatch |
| encounter_force_deactivate | 0x5baa0 | encounters.c | clean | 75.9% | skipped | @<eax> tail-call mismatch |
| FUN_000588d0 (ai_allow_dormant) | 0x588d0 | encounters.c | clean | 92.9% | committed | via subagent |
| encounter_iterator_new | 0x59990 | encounters.c | clean | 100.0% | committed | 100% byte match |
| FUN_000599c0 | 0x599c0 | encounters.c | clean | 100.0% | committed | 100% byte match |
| encounter_actor_iterator_prev | 0x59a90 | encounters.c | clean | 90.6% | committed | |
| FUN_00057c70 (ai_playfight) | 0x57c70 | encounters.c | clean | 91.1% | committed | |
| FUN_00057bc0 (ai_status) | 0x57bc0 | encounters.c | clean | 94.5% | committed | via subagent |
