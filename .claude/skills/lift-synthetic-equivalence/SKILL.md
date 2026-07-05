---
name: lift-synthetic-equivalence
tier: agent
triggers: ["synthetic equivalence", "state snapshot", "per-branch", "equivalence", "capped lift", "snapshot equivalence"]
description: >-
  Hand-crafted state-snapshot equivalence for lifts whose VC71 score is
  structurally capped (@<reg> params, layout ceilings) or whose branches need
  specific game state to reach. Builds small synthetic JSON snapshots that
  drive unicorn_diff down every branch — no xemu capture needed. Proven on
  ~10 scenario.obj/wind.obj functions on 2026-07-04.
---

# Synthetic-Snapshot Equivalence

**Invoke this skill when:**
- VC71 match is capped by a structural ceiling (`@<reg>` params, block layout)
  and equivalence is the sanctioned evidence lane
- `unicorn_diff` reports weak coverage / "only early-exit path" and the dark
  branches depend on globals, tag data, or struct-typed params
- You need per-branch behavioral proof for a stateful non-leaf lift

**Do NOT use** when a live xemu capture exists that covers the paths — replay
that instead (`memsave_snapshot.py`, `.halorec` → `halorec_to_snapshot.py`).

## Snapshot JSON shape

```json
{
  "description": "what path this drives",
  "build_label": "synthetic",
  "regions":   { "0x700000": "<hexbytes>", "0x5064e4": "<8B hexbytes>" },
  "stub_returns": { "tag_get": 7340544, "FUN_0018e770": 7 },
  "arg_overrides": { "window_index": -1, "camera_position": "<hexbytes>" }
}
```

Write them with a small inline `python3 - <<'EOF'` using `struct.pack`, into
`artifacts/equivalence/<target>_snap_<path>.json` (gitignored, regenerable).
One snapshot per branch family; 20–30 seeds each:

```bash
BIPED_SIBLING_RESOLVE=1 rtk python3 tools/equivalence/unicorn_diff.py <target> \
  --seeds 30 --allow-stubs --mem-trace --float-tolerance 16 \
  --state-snapshot artifacts/equivalence/<target>_snap_<path>.json
```

## Hard-won rules (each one cost a debugging round)

1. **Regions must be ≥ 8 bytes.** Oracle DIR32 slot seeding reads 8 bytes at
   the symbol's address; a shorter region fails to seed the slot.
2. **Pointer params: `arg_overrides` takes buffer CONTENT as a hex string,
   not an address.** An int pin does NOT relocate a pointer param. To give a
   struct param a pointer FIELD, embed a fixed region address (e.g.
   `0x700100`) inside the content hex and map that region.
3. **`stub_returns` may point into `regions`** — that is how `tag_get` &co.
   return usable synthetic tag data.
4. **Real globals the code reads must be seeded at their real address**
   (e.g. `0x5064e4` → little-endian pointer to your synthetic scenario).
   Large indexed tables need the identity-reloc feature (regions covering
   `DAT_/FLOAT_<addr>` are patched to their REAL VA — commit 2976262a).
5. **`LIFTED-CRASH eip=0x1ffffc`** = unpatched rel32 to a defined intra-object
   sibling. Set `BIPED_SIBLING_RESOLVE=1`.
6. **Sibling asymmetry:** the oracle (per-function delinked obj) STUBS a
   callee that the candidate (full clang obj) has DEFINED and runs for real.
   Symptoms: `call-seq diverged`, or branches disagree because the real
   sibling read your synthetic state and returned something else than the
   stub. Fix by making the synthetic state valid for REAL semantics (e.g.
   sky palette count ≥ index so a real bounds-check passes, tag-block data
   pointer at +0x34 actually walkable), run with `--real-callees`, and if
   the seq comparator still flags the candidate's legitimate extra inner
   calls, drop `--no-stub-arg-trace` for that one path — mem-trace + return
   state still verify behavior. Note it in the commit message.
7. **Data-family callees** (`datum_get`, `data_next_index`) intercept as
   zero-return stubs; scalar `stub_returns` cannot drive iteration loops.
   Sequenced per-call stub returns are a harness TODO — park such paths.
8. **clang x87 rounding traps surface here as 1-ULP scratch diffs**: clang
   folds `&= 0x7fffffff` to FABS (extended precision) and keeps accumulators
   in ST across iterations; MSVC rounds through memory. Fix with volatile
   spill-slot access (reuse the exact param slot the original reuses). See
   FUN_0018ff00 (byte-exact after fix).

## Coverage interpretation

Per-path runs each show partial coverage — judge the UNION. Assert bodies
(display_assert + system_exit) are dead weight in the count; a set of paths
that jointly cover everything except assert bodies is complete. Record each
path's pass count + coverage in the commit message.

## Mechanical accept bar (structurally capped lifts)

VC71 ≥ 85% (or documented @<reg> ceiling) + all synthetic paths pass with
moderate/high confidence + reloc audit clean (lift-silent-bugs Check 0)
→ commit with the equivalence evidence lane cited.
