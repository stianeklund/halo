# Agent Memory Index

- [maintain.py invocation](feedback_maintain_py.md) — always use relative paths; absolute paths cause maintain.py to empty the file
- [Object helper functions not yet in kb.json](reference_object_helpers.md) — addresses for 0x13d640, 0x13d7f0, 0xbbb80, 0xbbcb0, 0x425b0 and key struct offsets
- [Camera transition analysis](camera_transition_analysis.md) — vehicle exit state cascade is original behavior; smooth transitions come from observer blend timers not state_253
- [MSVC intrinsics catalog](reference_msvc_intrinsics.md) — _ftol2, _chkstk, __SEH_prolog/epilog, _allmul, _aullshr/div/rem, _allshr; no _CIxxx or signed 64-bit helpers
- [Equivalence hardcoded-import artifact](feedback_equivalence_hardcoded_import.md) — non-leaf calling XAPILIB import via literal-address cast diverges 100% in Unicorn (huge INSN_count, ESP blowup); trust VC71 not equivalence
- [Thunk signature propagation](feedback_thunk_signature_propagation.md) — correcting a callee decl arg count breaks JMP-forwarding tail-call thunks; fix thunk decl + forward the arg, grep all callers first
- [NT-import wrapper VC71 ceiling](feedback_nt_import_wrapper_ceiling.md) — kernel-import wrappers cap ~45-65%; instruction diff (offsets/args/stores), not score, decides correctness; param-slot reuse lift technique
- [short counter reg-width ceiling](feedback_short_counter_reg_width.md) — int16_t loop counters → 32-bit xorl/incl under VC71 vs original 16-bit; benign few-insn ceiling, don't churn source or permute
