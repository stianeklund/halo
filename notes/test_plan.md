Worktree Session Summary: mellow-exploring-pizza

Main Implementation: FUN_00022390 — Actor Combat Aiming State Update

The big deliverable. A 1963-byte, 613-instruction function that updates AI aiming every tick. Previously only the first ~80 bytes were implemented (fire-ok flag check). We completed the full implementation (~300 lines of C):

- Fire-ok flag check/copy/clear cycle
- Vehicle velocity moving-flag determination
- In-combat flag computation via combat property scaling
- Burst parameters lookup from actv tag
- Fire timer from burst data or cached value, with random range
- Rate-of-fire modifier from weapon damage analysis (FUN_000fac20)
- Debug printf paths gated on global flags
- Encounter suppression check
- Target position + suppression adjustment (FUN_00021430 — discovered hidden @<esi> register arg)
- Perpendicular vector computation with normalize3d
- Random yaw/pitch angles from burst reference
- Error scaling with combat properties
- Burst angle clamping with FPTAN, cone_radius, extended_radius
- Trig rotation (FCOS/FSIN) for primary/secondary error vectors
- Per-tick secondary scaling
- Actor field writes (0x64c–0x684)
- Combat vocalization sound dispatch

VC71: 77.7% — investigated thoroughly with two Opus agents, confirmed genuinely structural (register rotation, FPU stack scheduling, branch layout, @reg calling convention artifacts). No source bugs found.

Supporting Work

- x87_fptan helper added to src/x87_math.h — matches original FPTAN instruction
- FUN_00021430 kb.json fix — declaration updated from void(void) to void(float *tar
- kb_reg_baseline.json — added 0x21430 entry

Refactor: TICKS_PER_SECOND

Defined #define TICKS_PER_SECOND (*(float *)0x253394) /* 30.0f */ in common.h. Replaced all 27 raw *(float *)0x253394 dereferences across 10 source files. Same binary faithfulness (reads from original XBE constant pool), much better readability.

Investigation: "Structural Ceiling" Audit

Checked all sub-85% functions in actor_combat.c against our history of false ceiling claims (10/15 turned out to be real bugs historically). For this file, all four low scorers were genuinely structural:

┌──────────────┬───────┬───────────────────────────────────────────────────────────
│   Function   │ Score │                               Cause                                │
├──────────────┼───────┼────────────────────────────────────────────────────────────────────┤
│ FUN_00020f80 │ 65.9% │ DEC-chain vs CMP-chain switch + @eax                               │
├──────────────┼───────┼────────────────────────────────────────────────────────────────────┤
│ FUN_00021590 │ 68.8% │ @esi reg-arg + FCOMP vs FUCOMPP (found 30.0f fidelity gap — fixed) │
├──────────────┼───────┼────────────────────────────────────────────────────────────────────┤
│ FUN_00021270 │ 82.2% │ @eax/@ecx preamble + assert duplication                            │
├──────────────┼───────┼────────────────────────────────────────────────────────────────────┤
│ FUN_00021640 │ 78.1% │ @reg preamble + SETZ boolean materialization                       │
└──────────────┴───────┴───────────────────────────────────────────────────────────
