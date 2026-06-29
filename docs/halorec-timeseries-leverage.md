# Leveraging `.halorec` recordings as a time series

A `.halorec` (halo-memory-viewer `HMRC` container) is not one snapshot — it is a
**time series** of full game-state memory: per frame a `t` and the raw bytes of
the four `data_t` pools (objects / actors / players / perception-props), all at
guest VAs. One recorded frame maps 1:1 to a Unicorn `--state-snapshot`
(`halorec_to_snapshot.py`). This doc covers how we exploit the *time* axis.

> **Copyright boundary.** Recordings and any snapshot/corpus built from them are
> raw copyrighted Halo runtime memory — gitignored, host-only on the self-hosted
> runner (same class as the XBE / delinked `.obj`). Everything described here is
> *tooling*; it reads recordings from `$HALO_HALOREC_DIR` and never writes game
> memory into the tree. See `tools/equivalence/host_snapshots/README.md`.

Cadence of the recordings we have (host-only):

| recording | frames | span | ≈ gap | pools |
|-----------|-------:|------|------:|-------|
| Recording 19 / 20 | 12 | ~20 s | ~1.85 s (~55 ticks) | all 4 |
| A10-Grunt-Shooting-Unpatched | 184 | ~52 s | ~0.28 s (~8.5 ticks) | all 4 |

## 1. Frame-sweep coverage union — `tools/equivalence/halorec_frame_sweep.py`

A single-frame snapshot drives only the one path that frame's state selects, so
perception scorers early-exit and report **weak** coverage. The sweep runs an
equivalence target once per frame the actor is alive in and **unions the visited
oracle-PC sets** across frames (and across subjects), turning weak single-frame
coverage into a high-confidence union.

Design rules (baked in):
- **Concolic Phase 2 force-disabled.** Real frames *replace* synthetic global
  injection; unioning invented coverage would hide what real states reach.
- **Alive-frames-only** (`--alive-handle`, default = the `actor_handle` arg).
  Absent-actor frames early-exit identically in both impls — trivial passes that
  inflate the pass count and mask a real divergence.
- **Per-frame pass/fail/error** distinguished (an errored frame is not a pass).
- The sweep is also a **multi-state divergence hunt**: a function the audit calls
  byte-faithful must pass in *every* real frame; any FAIL contradicts the audit.

To union across subjects/invocations, `--coverage-out` writes a `{func_size,
covered_pcs}` map and `--coverage-in` seeds the union from prior runs.
`unicorn_diff.run_diff` now emits `covered_pcs` + `func_size` in its output JSON
for this.

Proven result (advisor payoff test) on `actor_compute_prop_target_weight`
(0x2fd10), Recording 19, 3 door grunts via `--from-manifest`:

```
single-frame:   48.6% / 52.9%   (weak)
per-grunt union: 58.1% / 54.1% / 54.1%
GRAND UNION:    62.4%  (high)  — verdict PASS across 24 real states
```

i.e. the time axis alone lifts each grunt above its single frame, and the
cross-grunt union clears the 60% high bar — an independent runtime confirmation
of the a10 "faithful" audit at 24× the states, not one.

Nightly stays single-frame (5-min budget, advisor); the sweep is on-demand.

## 2. Trajectory / state-transition oracle — PARKED (blocked)

The idea: feed frame[i], run the lifted per-tick function, check the field it
writes matches frame[i+1]. **Unsound at our current cadence.** Frames land ~8.5
ticks apart, so frame[i+1] is hundreds of function executions downstream of
frame[i] — a byte-perfect lift would still "diverge" because the function's
inputs were already mutated by earlier functions in the same tick and its owned
field rewritten many times between frames.

**Why the cadence is coarse (corrected — it is NOT a sampling-rate knob).** The
viewer's recorder *already targets per-tick*: it dedups on the game tick
(`read_game_tick` = `*GAME_TIME_GLOBALS_PTR + 0x0C`) and skips a capture unless
the tick advanced (`app.rs` worker). It lands ~8.5 ticks apart because each
capture is **round-trip-bound**: `read_object_windows` issues *one backend read
per object* (~484 in the A10 recording) plus chunked AI-pool reads — ~500 reads
per frame, each a round-trip on the CPU-halting xemu-gdb stub. ~500 × ~0.5 ms ≈
the observed 0.28 s/frame. The bottleneck is **read count**, not a timer.

**How to actually unblock it (cheapest first):**
1. **Lean AI-only recording mode** — skip `read_object_windows` (the ~484-read
   full-scene touch) when recording for AI. `read_ai_windows` already pulls the
   whole actor + perception-prop pools in ~32 KiB chunks (~5–8 round-trips
   total). Dropping the object windows cuts ~500 round-trips → ~8, ~60× faster;
   a capture then fits inside a 33 ms tick and the existing per-tick dedup yields
   one frame per tick. Small viewer change: gate `read_object_windows` behind a
   recording flag. The `.halorec` would carry only the AI pools — which is all
   `halorec_ai_diff` and the actor-pool sweep consume anyway.
2. **Pick the non-halting transport** — pair (1) with QMP `memsave` (zero CPU
   halt) or keep xemu-gdb (lowest latency; with ~8 reads/frame the halt is
   sub-ms).
3. **True zero-skip per-tick (architectural):** we own the XBE — patch a hook at
   the end of the per-tick AI/game update that memcpys the actor+prop pools into
   a reserved guest ring buffer each tick (in-guest, no network, no halt); drain
   it in bulk afterward. Decouples capture rate (per-tick) from transport rate
   (bulk). `patch.py`/`kb.json` can inject it.

**Still required even at per-tick: field ownership.** A single tick runs the
whole engine, so the oracle can only check the fields the target function is the
*sole* writer of that tick — derive the owned-offset set from its stores in
disassembly and confirm no other tick function writes them, else false
divergences. Do **not** build a `run_diff` single-impl refactor until (1)+(field
ownership) exist.

## 3. Patched-vs-unpatched divergence-onset — `tools/analysis/halorec_ai_diff.py localize`

The original a10 use and the strongest exploitation of the time axis. Record the
same scenario on two builds (unpatched-shooting vs patched-not-shooting), then
walk each scripted door grunt's fields forward and report the **first
(relative-time, field) where the two runs disagree**. That onset localizes a
regression *in time*; the first divergent field names the broken subsystem
(awareness, firing-position, path-commit, …). Pure data-diff — no Unicorn.

Two builds are not wall-clock synchronized and assign different datum salts to
the same scripted actor, so (advisor): **anchor at squad-spawn** and compare on
anchor-relative time, and **match grunts by datum slot**, not full handle.

Validation is `selftest` (NOT a Rec19-vs-Rec20 cross-diff, which are different
scenarios and diverge at frame 0, proving nothing):
- **self-diff ⇒ 0 onsets** (alignment + compare plumbing), and
- **one injected field perturbation ⇒ exactly that one onset** at the injected
  frame.

```
python3 tools/analysis/halorec_ai_diff.py selftest "$HALO_HALOREC_DIR/Recording 19.halorec"
python3 tools/analysis/halorec_ai_diff.py localize unpatched.halorec patched.halorec
```

Real use needs a paired patched/unpatched recording captured the same way; the
unpatched half (`A10-Grunt-Shooting-Unpatched`, 184 frames, all pools) exists.

## 4. Real-frame value corpus for concolic — `--emit-value-corpus` / `--value-corpus`

For the **residual** branches the union (step 1) still doesn't cover, concolic
Phase 2 normally injects invented constants (1, 2, 5, …) into zero-filled
globals. With a corpus of the *real* values those globals held across the
recording's frames, it injects feasible engine-produced values instead.

- Consumer (**proven** — `test_value_corpus.py`): `unicorn_diff.py
  --value-corpus PATH` → `concolic.generate_memory_injections(..,
  value_corpus=..)` injects the real value for a matched global instead of an
  invented constant.
- Producer (**population unverified**): `halorec_frame_sweep.py
  --emit-value-corpus PATH` writes `{global_addr: [observed values]}` from each
  sweep run's `global_reads`.

Honest status: **wired and consumer-tested, but never observed populating a
non-empty corpus.** On 0x2fd10 the union still left 37.6% uncovered, so residual
*does* exist — yet the corpus came out empty. The reason is a producer/consumer
address-set mismatch, NOT "no residual": concolic targets **zero-valued** globals
(`zero_reads` / the `val==0` fallback), but the producer harvests `global_reads`
from a **full-snapshot + `--allow-stubs`** sweep, where those globals are either
satisfied-from-snapshot (untracked) or read inside a stub. Producer and consumer
look at disjoint addresses. Closing this needs a snapshot-less probe pass to
discover the zero-globals concolic wants, then look up their real per-frame
values — not built. Step 4 is therefore scaffolding with a tested consumer, not a
working end-to-end loop; it was the explicitly-marginal mode of the four.
