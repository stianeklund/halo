# MP dead bipeds linger; limp/ragdoll flag never sets (patched)

**Status:** Root cause found (fix applied, awaiting live confirmation)  
**Class:** silent behavior regression vs cachebeta.xbe (debug 2276)  
**Regression:** Yes — unpatched cachebeta sets biped limp flag `0x00000020`; patched does not  
**Distinct from:** garbage-collector hysteresis, item come-to-rest timers, HUD/visibility culling  
**Shipped so far:** `f406dbcd6` (death-anim `transition_speed` + `unit+0x257` LOADW). Does **not** close the limp gap.  
**Root cause:** `FUN_001a2f40` sets the cannot-come-to-rest bit on every BSP
contact, so `object+0x4` bit `0x20` (at-rest) is never set and
`biped_start_limp_body_physics` always early-outs. See
[Root cause](#root-cause-fun_001a2f40-at_rest-blocker-bit) below.

---

## Symptom

In multiplayer on the patched XBE, dead bipeds stay on the map longer than on
unpatched cachebeta. A live comparison showed the concrete state difference:

- **cachebeta:** biped flags at `biped+0x424` include `0x00000020` (limp / ragdoll).
- **patched:** that bit is never set.

Limp is the body-physics path that lets a corpse come to rest so mild object GC
can collect it. Without the flag, corpses look like they “linger.”

---

## How limp is supposed to happen

Binary-backed chain (all VAs into cachebeta 2276):

1. **Kill** — `object_deplete_body` (`0x137530`, VC71 100%) ORs `object+0xb6` bit 4
   (dead). Combat deaths do **not** go through `FUN_001b1400`; that helper is
   only used from `unit_place` for already-dead placement.
2. **Each biped tick** — `FUN_001a6350` (`0x1a6350`, ~88%) sees dead + no parent
   (`object+0xcc == -1`) and calls `FUN_001a6280` (`0x1a6280`, ~86%).
3. **`FUN_001a6280`** writes desired anim state `*state_out = 0x19` (landing-dead),
   unless already limp or dying-airborne (`0x18`).
4. **`FUN_001b0d90`** (`0x1b0d90`) applies that desired state via `FUN_001a86b0`
   (100%) then `FUN_001ad260`.
5. **`FUN_001ad260`** (`0x1ad260`) maps state `0x19` to overlay index **1**
   (`"landing-dead"` in the overlay name table at `0x322450`), calls
   `unit_set_animation`, and always writes `unit+0x253 = 0x19` (even if the
   overlay lookup returns `-1`).
6. Later ticks: `FUN_001ab870` (`0x1ab870`, ~96.8%) on the replacement anim at
   `unit+0x80`. When it returns **2** (finished) and `unit+0x253 == 0x19`,
   `FUN_001b0d90` calls `biped_start_limp_body_physics` (`0x1a0970`, ~95%).
7. **`biped_start_limp_body_physics`** still early-outs unless **`object+0x4` bit
   `0x20` (at-rest)** is set; only then ORs `biped+0x424` bit `0x20`.

So limp requires all of: dead bit, state `0x19`, a real anim index at `+0x80`,
anim finished, and at-rest.

---

## What was investigated

Work was against cachebeta.xbe (MD5 `c7869590a1c64ad034e49a5ee0c02465`),
`src/halo/units/units.c`, `src/halo/units/bipeds.c`, `src/halo/objects/objects.c`,
`src/halo/items/items.c`, `src/halo/objects/damage.c`, and VC71 score-context
packs. Disassembly via `xbe_reference.py emit` + Capstone (Ghidra MCP was
available for preflight but not used for dumps).

### 1. Garbage collector (first suspect — ruled out as the limp cause)

| Function | Addr | VC71 | Notes |
|---|---|---|---|
| `objects_garbage_collect_tick` | — | 88.2% | Uses table `+0x2e` / `+0x30` as in XBE; 140/140 old unicorn equiv |
| `object_set_garbage_flag` | `0x13d920` | — | Called from `unit_died` at `0x1b308d` |
| `object_visible_to_any_player` | — | 88.4% | FCOM sense looks inverted vs XBE `test ah,5 / jnp` (≥). Would *protect* nearby corpses from mild GC. **Parked** — does not explain missing limp bit |

Mild GC: need ≥50 garbage objects this tick (`object_globals+0x04`), stop at ≤30.
Bipeds have **no** per-body timer (items do, 300–600 ticks, and MP items are
**not** garbage-flagged on come-to-rest when `game_engine_running()`). Collector
faithfulness is not why limp never starts.

### 2. `FUN_001ad260` — death overlay apply (user-requested first)

Jump table at `0x1ad714`, 44 states. State `0x19` → `0x1ad488`:
`mov eax, 1; jmp overlay_lookup` at `0x1ad455`. Overlay 0 is `"airborne-dead"`.

XBE has **24 CALLs**. Candidate SHAPE was 16 vs 24. Those 8 extras are same-TU
inlining (`FUN_001a88b0` ×2) and debug `MISSING:` warnings gated on
`*(uint8_t*)0x5054fb`, **not** a dropped `landing-dead` apply. The third
`tag_block_get_element(mode_block+0xb0, unit+0x252, 0x3c)` is discarded in the
XBE too (`mov al,[esi+0x253]` immediately after).

Early `return 0` on missing anim is only for `0x1e, 0x1f, 0x20, 0x21, 0x27, 0x29`.
State `0x19` always writes `unit+0x253` and returns 1.

VC71 **76.8%** (277 vs 366 insns, frame `0x18` vs `0x1c`). Classifier:
`loadw_field_width` (`movswl 0x12(%ecx)` = overlay slot 9 `"look"`, `was_none`
only) and `frame_mismatch`. Score-improve prefilter skipped spawning an
optimizer (`|n_cand−n_ref|/n_ref > 0.20`). Jump-table vs VC71 value-lookup
array is a documented codegen ceiling, not a wrong `0x19` body.

Real C vs XBE delta (not the limp flag): at `0x1ad66e` the original **always**
stores `transition_speed = 6` when the weapon-idle *class* changes, before the
`was_none` overlay-9 arm. Our C only set 6 inside `was_none`. For `0x19` the
computed speed is already 6, so this does not explain limp.

### 3. `FUN_001b0d90` — limp-start caller (user-requested second)

Limp case at `0x1b0fef`–`0x1b1061` matches our `goto start_limp` /
`destroy_unit` / `set_garbage_flag` shape, including `call 0x1a0970`.

LOADW: candidate did `cmpb %bl, 0x257(%esi)`; XBE does
`movsx ax, byte [edi+0x257]; cmp ax, bx`. That compare is in the **live
seat-change** block, skipped when `unit+0xb6` bit 4 is set (dead).

VC71 **79.2% → 79.7%** after the width fix; LOADW warning gone. Frame 80/80.
Not on the limp call.

### 4. Related scores (not rewritten)

| Function | Addr | VC71 | Role |
|---|---|---|---|
| `FUN_001a86b0` | `0x1a86b0` | 100% | May apply death? Idle→`0x19` allowed; `0x18`↔`0x19` allowed |
| `FUN_001a8790` | `0x1a8790` | 100% | Finished-anim “can apply” |
| `FUN_001ab870` | `0x1ab870` | 96.8% | Returns 2 when anim finished |
| `biped_start_limp_body_physics` | `0x1a0970` | 95.0% | Sets limp; requires at-rest |
| `FUN_001a6280` | `0x1a6280` | 86.4% | Writes desired `0x19` |
| `FUN_001a6350` | `0x1a6350` | 88.0% | Dead dispatch → `FUN_001a6280` |
| `unit_died` | `0x1b3060` | 73.2% | Drops, garbage flag; **does not** set `+0xb6` bit 4 |
| `FUN_001b1400` | `0x1b1400` | 74.4% | Death *placement* impulse only (4 dropped calls, FCOM). Not MP combat kill |
| `FUN_001a4440` / `FUN_001a5300` | `0x1a4440` / `0x1a5300` | unported | Likely at-rest physics; still original bytes |

`FUN_001a6350` only requests `0x19` when the biped is **free**. If
`object+0xcc` is still a parent after death, `FUN_001a6280` never runs.

Current weapon is **not** dropped on death (matches XBE). Extra weapons and
grenades are. MP items are not garbage-flagged on rest (matches our C).

---

## What shipped

Commit **`f406dbcd6`** (`src/halo/units/units.c` only):

1. `FUN_001ad260` — hoist `transition_speed = 6` to the weapon-idle-class-changed
   arm (XBE `0x1ad66e`), not only `was_none`.
2. `FUN_001b0d90` — `unit+0x257` compared as `movsx` int8→int16 (XBE `0x1b0ed7`).

Neither is the limp OR. Rebuild + live dump is still required to confirm limp.

---

## Root cause: `FUN_001a2f40` at-rest blocker bit

Hypothesis 2 confirmed, in a function that was not in the investigated set. The
earlier pass listed `FUN_001a4440` / `FUN_001a5300` as "likely at-rest physics;
still original bytes" and stopped there. Those two are indeed unported, but the
flag they act on is produced by `FUN_001a2f40` (`0x1a2f40`-`0x1a4436`, biped
ground/movement physics step, VC71 **77.0%**, 1613 reference instructions) --
the only **ported** function in the at-rest production path, and the only writer
of the bit that blocks at-rest.

### The chain to at-rest

`FUN_001a6350` calls, in order: `FUN_001a5300` (physics) -> `FUN_001a6280`
(death state) -> `FUN_001b0d90` (apply / limp start). `FUN_001a5300` builds a
biped-physics struct at `ebp-0xe4` and passes it in `ESI` to `FUN_001a2f40`
(`0x1a5dfb`: `lea esi,[ebp-0xe4]; call 0x1a2f40`). On return, `FUN_001a5300`
reads the struct's output flags at `+0xa0` (= `[ebp-0x44]`, since
`0xe4 - 0x44 = 0xa0`):

| Site | Reads | Effect |
|---|---|---|
| `0x1a5f1e` | `+0xa0` bit 0 | -> `biped+0x424` bit 0 (airborne) |
| `0x1a5f31` | `+0xa0` bit 1 | -> `biped+0x424` bit 1 |
| `0x1a6216` | `+0xa0` bit `0x10` | set -> **skip** the at-rest OR |

The at-rest OR itself is `0x1a624c` (`or eax,0x20` into `[obj+4]`), gated on:
`[ebp-0xe0] & 0x10 == 0`, `obj+0x424 & 1 == 0` (not airborne),
`[ebp-0x44] & 0x10 == 0`, and `|velocity|^2 <= [0x253f44]`.

### The divergence

XBE `0x1a3e78`-`0x1a3eae`, inside loop A over the collision-result array
(`EDI = entry + 0x14`, stride `0x2c`; so `[edi+0xc]` = `entry+0x20` =
`object_handle`, `[edi+0x14]` = `entry+0x28` = `flags`):

```
1a3e78  test byte [esi+0xa0], 0x10   ; already set?
1a3e7f  jne  0x1a3eb5                ;   -> next iteration
1a3e81  test byte [edi+0x14], 8      ; e->flags & 8
1a3e85  jne  0x1a3eae                ;   -> SET
1a3e87  mov  eax, [edi+0xc]          ; e->object_handle
1a3e8a  cmp  eax, -1
1a3e8d  je   0x1a3eb5                ;   BSP contact -> do NOT set
1a3e8f  ...  datum_get ; test dl,0x40
1a3eac  jne  0x1a3eb5                ;   type-6 (scenery) -> do NOT set
1a3eae  or   byte [esi+0xa0], 0x10   ; SET
```

The lift folded the two independent tests into one condition:

```c
if ((e->flags & 8) == 0 && e->object_handle != -1) {
  if (object_try_and_get_and_verify_type(e->object_handle, 0x40) != NULL)
    goto loopA_nomark;
}
*(unsigned char *)((char *)physics + 0xa0) |= 0x10;
```

With `flags & 8 == 0` **and** `object_handle == -1` -- a plain BSP-surface
contact, the normal case for a body lying on world geometry -- the outer `if` is
false, control falls straight through to the `|= 0x10`, and the original's
`je 0x1a3eb5` skip is lost.

Consequence: `physics+0xa0` bit `0x10` is set on essentially every ground
contact, `0x1a6216` therefore always branches to `0x1a6251`
(`and eax,0xffffffdf`), `object+0x4` bit `0x20` is never set for any biped on
BSP, and `biped_start_limp_body_physics` early-outs at `0x1a09ab`
(`test byte [esi+4], 0x20`). `biped+0x424` bit `0x20` is never ORed -- the
reported symptom. It also keeps `FUN_001a5300`'s dead-and-at-rest early-out
(`0x1a531e`) from ever firing, so dead bipeds keep running full physics.

The `0x10` bit means "resting on something that cannot be come to rest on":
`flags & 8` surfaces, or a contact object whose type is not 6 (scenery). A BSP
surface is a valid resting place, which is why the original skips it.

### Fix

`src/halo/units/bipeds.c` -- split the condition so a BSP contact skips the set:

```c
if ((e->flags & 8) == 0) {
  if (e->object_handle == -1) {
    goto loopA_nomark;
  }
  if (object_try_and_get_and_verify_type(e->object_handle, 0x40) != NULL) {
    goto loopA_nomark;
  }
}
*(unsigned char *)((char *)physics + 0xa0) |= 0x10;
loopA_nomark:;
```

Verified: build clean; `check_lift_hazards.py --changed-only` reports no
ERROR-level findings (the WARNs in `bipeds.c` are all pre-existing and
elsewhere); `vc71_verify.py src/halo/units/bipeds.c` holds `FUN_001a2f40` at
77.0% and regresses no other function in the TU.

Still to do: live confirmation that `biped+0x424` bit `0x20` now sets on a dead
MP biped, per the dump table below.

### Also checked, matches the XBE

These were diffed instruction-by-instruction against cachebeta during this pass
and need no work:

- `biped_start_limp_body_physics` (`0x1a0970`) -- gate order and both ORs match.
- `FUN_001b0d90` limp case (`0x1b0fef`-`0x1b1061`) -- `tag+0x17c` bit 1,
  `unit+0x4` bit `0x20`, `unit+0x64`, `biped+0x424` bit 0 / `tag+0x2f4` bit 10.
- `FUN_001a6280` (`0x1a6280`) -- limp-noodle sub-step, dying-airborne
  (`+0x459 >= 3` signed), normal dying `0x19`.
- `FUN_001a6350` (`0x1a6350`) -- parent dispatch, normalize gate, `+0x42a`
  anim-mode switch, velocity clamp, `+0x459`/`+0x45a` counters, dead ->
  `FUN_001a6280`.
- `FUN_001ab870` (`0x1ab870`) -- thin wrapper over unported
  `animation_update_internal` (`0x121c30`); returns `(short)EAX`.
- `FUN_001ad260` state `0x19` -> overlay index 1, and the `-1` overlay path
  still applies (only `0x1e/0x1f/0x20/0x21/0x27/0x29` early-return 0).

`biped_start_limp_body_physics` has exactly one caller in the whole binary
(`0x1b104b`, in `FUN_001b0d90`), which bounds the search.

Minor, not the bug: `0x1a3880` tests the result count with a signed `jl 0x10`
where the lift uses an unsigned compare. Identical for any real count.

---

## Remaining hypotheses (ordered)

The lifts for state `0x19` and the limp *case* match the XBE. The flag is still
missing, so something upstream never reaches `biped_start_limp_body_physics`,
or that function early-outs.

Superseded by the root cause above; kept for the record.

1. **`unit+0x253 == 0x19` but `unit+0x80 == -1`** — overlay lookup failed
   (`unit_mode+0x40` count ≤ 1, or mode bytes `+0x250/+0x251` point at the wrong
   graph). Death state is stamped, replacement anim never runs, status 2 never
   fires. **Most useful if live dump shows this.**
2. **Anim index valid, `object+0x4` bit `0x20` clear** — death anim plays;
   at-rest never set. Next: who writes/clears that bit (`object_reset` clears
   it; `FUN_001a4440` / `FUN_001a5300` still original).
3. **`unit+0x253` never becomes `0x19`** — tick path: parent still attached, or
   `FUN_001a86b0` blocked apply, or dead bit never set (check
   `object_deplete_body` actually ran).
4. **All of the above match cachebeta, limp still clear** — then
   `biped_start_limp_body_physics` or `FUN_001ab870` never returning 2. Last,
   not first.

Parked, do **not** start here:

- Garbage collector rewrite.
- Inverting `object_visible_to_any_player` FCOM (GC linger *after* limp, not
  the missing flag).
- Chasing `FUN_001ad260` from 76.8% as if case `0x19` were missing.
- Treating `FUN_001b1400` as the MP kill path.

---

## Next live dump (do this before more lifting)

On one dead patched biped, and the same moment on cachebeta if possible:

| Field | Meaning | cachebeta expectation |
|---|---|---|
| `unit+0xb6` bit 4 | dead mark | set |
| `unit+0x253` | anim state | `0x19` |
| `unit+0x80` (int16) | replacement / overlay anim index | not `-1` |
| `object+0x4` bit `0x20` | at-rest | set before limp |
| `biped+0x424` bit `0x20` | limp itself | set on cachebeta, clear on patched |

Act on the **first** patched mismatch, rebuild, dump again.

---

## Key source / binary anchors

- `FUN_001ad260` — `src/halo/units/units.c` (~7712), XBE `0x1ad260`–`0x1ad713`
- `FUN_001b0d90` limp — `units.c` (~10988), XBE `0x1b0fef`–`0x1b1061`
- `FUN_001a6280` / `FUN_001a6350` — `units.c` (~838 / ~893)
- `biped_start_limp_body_physics` — `src/halo/units/bipeds.c` (~545)
- `object_deplete_body` dead bit — `src/halo/objects/damage.c` (~802)
- Overlay name table — `0x322450` (index 1 = landing-dead)
- Jump table — `0x1ad714`

Artifacts from the RE pass: `/tmp/gc-disasm/ad260_ref.obj`,
`/tmp/gc-disasm/b0d90_ref.obj`; score packs under
`artifacts/score_context/FUN_001ad260.json` and `FUN_001b0d90.json`.
