---
name: network-game-player-is-local-zerofill-masquerade
description: 0x12a0d0 88.9% NEEDS_RUNTIME — acceptance note claimed live-state infection_swarm equiv but detail said ZERO-FILL fallback, only trivial path covered
metadata:
  type: project
---

network_game_player_is_local (0x12a0d0, network_game_globals.obj) 88.9% VC71 → NEEDS_RUNTIME.

Static body is byte-faithful (decoded 1:1 vs disasm): 5 CALLs all resolve to correct kb cdecl targets — network_player_is_valid(0x12ac80), network_game_client_get_machine(0x124c10, void* cdecl, NOT the _index variant), game_connection(0xfff80, short→CMP AX,0x3), display_assert(4 args reverse-push line 0x9b), system_exit(-1). Read-only: reads global *0x46e8c0, machine+0x40, player+0x1c; no writes. ABI regs=none, hazard scan clean, no FPU warn. Gap = 3-epilogue dedup + branch layout (reg-alloc shape), not a defect.

**Why NEEDS_RUNTIME not AUTO_ACCEPT:** the acceptance note asserted "0-divergence pass on the live-state infection_swarm snapshot (populated datum tables, real actor handles)" — but the equivalence DETAIL contradicted it: "Zero-fill fallback used (no infection_swarm.json in this worktree)... coverage weak 32.8%... only the non-client 'return true' path exercised, all seeds returned 0x1. The client machine-index comparison path (path1) and the assert path were NOT exercised."

**Why:** the runtime evidence was zero-fill single-path, NOT the real-snapshot pass the note claimed. The function's whole purpose (machine-index comparison, path1) had zero coverage from any oracle. Per band policy <90% requires golden/runtime verification; a weak-tier zero-fill run that only hits the trivial early-return is not it.

**How to apply:** when an acceptance note cites "live-state / infection_swarm / real snapshot" evidence, READ the equivalence detail — if it says zero-fill fallback or coverage weak / all-seeds-same-return, the note misrepresents the run. Discriminator (same as [[project_actions_0x1d530_sub90_statesnapshot_clear]] and [[project_evade_88band_equiv_globalseed_artifact]]): AUTO_ACCEPT on sub-90 needs a PASS that actually exercises the discriminating branch on real memory. Trivial-path-only zero-fill = weak tier = NEEDS_RUNTIME. Not REJECT — body faithful, no bug.
