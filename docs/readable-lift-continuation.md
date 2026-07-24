# Handover — Readable-Lift Initiative

_Branch: `lift-session-20260724` (13 commits on top of `main`). Session date: 2026-07-24._

## Objective
Make lifted C read like faithful Bungie source (named structs/fields, typed
pointers, no raw address casts, recovered constants) **without** sacrificing
VC71 byte-match — and add tooling so new lifts stay that way.

## Current State
- **Complete for this scope.** All planned work landed and is committed; each
  change is VC71-gated and byte-verified. Nothing is half-applied.
- `src/halo/game/player_control.c`: fully cleaned — 0 inline-asm blocks, 0 raw
  fn-ptr casts, and the entire `player_control_t` element struct recovered
  across all six functions that touch it.
- `src/types.h`: new `player_control_t` (0x40 element).
- Phase 3 tooling shipped: `tools/audit/check_readability.py`,
  `tools/readability_baseline.json`, `tools/hooks/pre-commit-readability.sh`.
- Working tree clean except `tools/equivalence/leaf_cache.json` (coverage-pct
  churn from VC71/equiv runs — intentionally uncommitted, not ours to commit).

## Confirmed (tool-backed)
- `0x1ac350` is `unit_find_weapon_to_ready(int unit_handle) -> int16_t`, NOT
  `sound_dispose`. Live-XBE disasm: `object_get_and_verify_type(h,3)` → scans 4
  weapon slots at `unit+0x2a8` (stride 4) → returns first slot where
  `weapon_must_be_readied` (0xfb090) is true, else NONE. arg-count 1/1 OK.
- `player_control` element: stride 0x40, at `player_control_globals+0x10`.
  Field names recovered from the binary's own `display_assert` strings
  (`player->desired_angles.yaw`@0x0c / `.pitch`@0x10 / `primary_trigger`@0x1c);
  `desired_weapon_index`@0x20 / `desired_grenade_index`@0x22 /
  `desired_zoom_level`@0x24 (target of `weapon_rotate_zoom_level`);
  `unit_index`@0x0; `action_flags`@0x08 / `persistent_action_flags`@0x0a (uint16,
  from `set_action_flags`). Unknowns kept as `field_0xNN`.
- `new_unit` look-pitch constant: original binary stores `0x3fbf0243`/`0xbfbf0243`
  (= ±1.4922565f = ±85.5° in radians) at `0xb702e`; confirmed in delinked ref
  (offset `cae`). Our `1.49f` was wrong (`0x3FBEB852`); fixed to `1.4922565f`.
- VC71 (per-function, delinked ref `delinked/player_control.obj`, range
  b6380–b8e00): `get_facing` 79.2→**81.4%**; `new_unit` **94.3%** (IMM-WARN now
  cleared); `set_facing` 85.5%; `clear_aim_assist`/`set_unit_seat` 100%. All
  conversions codegen-neutral or better; no regressions.
- Deployed + self-verified live (rev `1f58138a`); clean boot to title screen.

## Inferred (not binary-proven)
- `field_0x38`/`field_0x3c` (±85.5°) are look-pitch min/max limits — inferred
  from the symmetric ±85.5° init values; kept as `field_0xNN` (no usage evidence
  in this file).

## Important Changes
- `src/types.h`: added `player_control_t` (16 fields, 0x40).
- `src/halo/game/player_control.c`: asm→`@<reg>` named calls; ~16 raw fn-ptr
  casts→named calls; full `player_control_t` typing of all six element
  accessors; `new_unit` constant fix.
- `kb.json`: `0x1ac350` decl → `int16_t unit_find_weapon_to_ready(int unit_handle)`;
  four `void(void)` decls corrected to real cdecl sigs (0x86270/0x862c0/0xe0b50/
  0xfc710) plus 2 `@<reg>` callees (0xb70b0/0xb7f90) landed earlier in the session.
- `tools/audit/check_readability.py` (+ baseline + hook): readability ratchet.
  raw fn-ptr casts = HARD gate (blocks only NEWLY-added casts); `fun_call` /
  `raw_offset_deref` = SOFT (auto-ratchet-down, warn-never-block). Listed in
  CLAUDE.md/AGENTS.md Analysis Tools.
- `tools/raw_cast_baseline.txt` 401→382; `readability_baseline.json`
  raw_offset_deref 14903→14850.

## Validation
- `tools/build/build.py -q --target halo`: exit 0 (run after every change).
- `tools/verify/vc71_verify.py src/halo/game/player_control.c`: PASS, numbers
  above (also `--no-cache --imm-only` used to confirm the constant fix).
- `tools/audit/check_lift_hazards.py --changed-only`: clean each commit.
- `tools/audit/check_arg_counts.py --callee 0x1ac350`: OK (declared==observed).
- `build_deploy_run.sh -q`: exit 0, self-verify OK; XBDM screenshot = title screen.
- NOT run: gameplay-path runtime test of `get_facing`/`new_unit` (would need a
  level load / input fixture). Behavior rests on the VC71 byte-match evidence.

## Uncertain / Risks
- Residual `[IMM-WARN]` VC71 prints against `player_control_initialize_for_new_map`
  is a **truncated-reference per-function-chunk-misalignment artifact**, not a
  real defect — the ±85.5° stack stores it flags live in the **unported** 0xb7f90
  look-input clamp. Pre-existing; re-export of the delinked ref did not help
  (relocation synthesizer errors past b8e00; functions past kb.json's last entry
  b8d30 throw off chunk alignment).
- `desired_zoom_level`@0x24 name leans on `weapon_rotate_zoom_level`; also reset
  by `player_clear_aim_assist`, so it may be a broader "weapon interaction" field.
- `field_0x38`/`0x3c` names left generic on purpose (see Inferred).

## Next Steps
1. Recover the `action` / `player_action` input struct (0x20 bytes) — the other
   struct pervading `player_control.c`; would clear most residual advisory
   findings there. Same method: type the buffer, convert `*(T*)(action+0xNN)`.
2. Apply the readable-lift pattern to a new high-traffic TU (units.c / objects.c);
   run `check_readability.py --changed-only` after edits as the feedback loop.
3. Optional: port `0xb7f90` (look-input pitch clamp) — that clears the last
   `player_control.c` IMM-WARN artifact and the `FUN_000b7f90` name.
4. Optional deferred: `check_assert_targets.py --emit-asserts` (task #3), and a
   named constant for the ±85.5° look-pitch limit if 0xb7f90 confirms the meaning.

## Resume Prompt
> Continue the readable-lift initiative on branch `lift-session-20260724`.
> `player_control.c` is fully recovered (struct `player_control_t` in types.h,
> zero raw casts/asm, `new_unit` constant fixed) and Phase 3 tooling
> (`check_readability.py`) is shipped. Next: recover the 0x20-byte `action`
> input struct used throughout `player_control.c`, OR roll the pattern to a new
> TU. Every change must stay VC71-gated (`vc71_verify.py`) and byte-neutral;
> derive field names from binary evidence (asserts, call sites), keep unknowns as
> `field_0xNN`, and run `check_readability.py --changed-only` after edits. See
> `docs/readable-lift-continuation.md` and memory `project_readable_lift_initiative`.
