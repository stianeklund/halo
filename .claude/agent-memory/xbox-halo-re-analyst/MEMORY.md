# Agent Memory Index

- [maintain.py invocation](feedback_maintain_py.md) — always use relative paths; absolute paths cause maintain.py to empty the file
- [Object helper functions not yet in kb.json](reference_object_helpers.md) — addresses for 0x13d640, 0x13d7f0, 0xbbb80, 0xbbcb0, 0x425b0 and key struct offsets
- [Camera transition analysis](camera_transition_analysis.md) — vehicle exit state cascade is original behavior; smooth transitions come from observer blend timers not state_253
- [MSVC intrinsics catalog](reference_msvc_intrinsics.md) — _ftol2, _chkstk, __SEH_prolog/epilog, _allmul, _aullshr/div/rem, _allshr; no _CIxxx or signed 64-bit helpers
