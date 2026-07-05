---
name: project-actions-searching-same-position-9333band-bar
description: actors_searching_same_position 0x20140 93.3% AUTO_ACCEPT — another 90-95 actions.obj static clear; read-only, FCOMP-vs-threshold@0x253dd0 polarity, kind 0/1/2 search-record semantics
metadata:
  type: project
---

actor AI `actors_searching_same_position` @0x20140 (actions.c) 93.3% VC71 AUTO_ACCEPTED on the 90-95 STATIC lane — no runtime needed (matches [[project_actions_combat_targeting_9036band_bar]] 0x1dea0 93.6% and sibling 0x20670 96.8%).

Whole body decoded 1:1 vs cachebeta disasm:
- 4x datum_get(actor_data=[0x6325a4],handle) cdecl (PUSH handle then pool); the 1st/3rd duplicate on actor_handle and the 4th are FAITHFUL (not CSE'd either side) — do not re-flag as duplicate-arg.
- 2x datum_absolute_index_to_index([0x5ab23c] object pool, *(int*)(rec+0x270)) cdecl.
- 1x distance_squared3d(rec1+0xbc, rec2+0xbc) float-return ST0, then FCOMP [0x253dd0].
- FCOMP polarity: TEST AH,0x5;JNP -> distance<threshold => same=1; else return 0. Source `if (thr<=dist) return 0; result=1`. Correct.
- Read-only: NO struct/global writes anywhere. Side-effect audit trivially clean.

Reusable facts:
- +0x6c = search-type tag WORD; only 5 or 7 activates the search record at +0xa4 (else NULL -> false).
- record+0 = position kind word (0=world/distance, 1=index equality on record+2, 2=always-same); mismatched kinds -> false.
- +0x270 = absolute object index (dword) into pool *0x5ab23c; +0xbc = object position (float3).
- 0x253dd0 = distance-squared threshold float (~0.49 = 0.7^2), read as *(float*)addr EXACT (no IMM transcription risk).

~6.7% gap = reg-alloc (source var-reuse rec1/rec2 vs stack-slot+reg) + MSVC 3x duplicated epilogue tails = epilogue-dedup shape. No CF/dataflow delta.

Equivalence note: pipeline zero-fill 100/100 but WEAK 43% cov (only the +0x6c-gate NULL early-exit ran; kind 0/1/2 branches dark under zero-fill). NOT blocking in 90-95 band — full static disasm covers all branches 1:1; byte-match is the primary signal per policy.
