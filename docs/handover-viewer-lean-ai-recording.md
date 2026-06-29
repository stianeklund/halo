# Handover: add a "lean / AI-only" recording mode to halo-memory-viewer

**Target repo:** `halo-memory-viewer` (the egui/Rust memory recorder — a *separate*
repo from the lift repo where this doc lives). All paths below are relative to the
viewer repo root.

**You (the implementing LLM) are not expected to know the lift repo.** This doc is
self-contained. Implement the change in the viewer repo only.

---

## 1. Goal

Make the viewer able to record `.halorec` files at (close to) **one frame per game
tick** by adding a recording mode that captures **only the AI data pools** (actor
pool + perception-prop pool) and **skips the full per-object struct-window scan**.

Today a recording lands ~8.5 game ticks apart. The cause is not a timer — it is
**per-frame read count**. Cutting the per-object scan removes the dominant cost.

This is opt-in: the existing full-fidelity recording (used by the viewer's own
replay/microscope/diff UI) must remain the default and stay byte-for-byte
unchanged. The new mode trades object-window fidelity for capture speed.

## 2. How recording works today (with file:line evidence)

The recorder is driven by one boolean, `HaloApp::recording_stream`
(`crates/halo-viewer/src/app.rs:547`). On each refresh while recording, the app
sends a capture command:

```
// crates/halo-viewer/src/app.rs:752
want_regions: self.recording_stream,
```

In the worker that services the command
(`crates/halo-viewer/src/app.rs` ~250–308), `want_regions` gates the heavy work:

```
// ~app.rs:292
if want_regions {
    halo_decode::read_object_windows(&mut reader, &state);  // <-- the expensive part
    halo_decode::read_ai_windows(&mut reader, &state);      // <-- the part we WANT
}
let regions = want_regions.then(|| reader.captured_regions());
```

- `read_object_windows` (`crates/halo-decode/src/decode.rs:225`) loops **every
  object** and issues **one backend read per object** (`for o in &state.objects {
  reader.read(o.object_address, n) }`). In a busy scene that is ~480+ reads/frame.
- `read_ai_windows` (`crates/halo-decode/src/decode.rs:265`) reads the **entire
  actor pool** (`*0x6325A4`, element size `0x724`) and **entire perception-prop
  pool** (`*0x5AB23C`, element size `0x138`) in ~32 KiB chunks
  (`CAPTURE_CHUNK_BYTES = 0x8000`) — only ~15–20 reads total.

Each `reader.read` is a round-trip on the active backend
(`crates/halo-backend/src/{xbdm,qmp,gdb}.rs`). The default xemu-gdb backend
**halts the guest CPU** for each read (`gdb.rs` `halt`/`resume`). So a recording
frame is ~500 halting round-trips ≈ 0.28 s — about 8.5 ticks at 30 Hz.

The recorder *already* tries to capture per-tick: it reads the game tick
(`read_game_tick` = `*GAME_TIME_GLOBALS_PTR + 0x0C`,
`crates/halo-decode/src/decode.rs:323`) and **skips the whole capture unless the
tick advanced** (`app.rs` ~257–266, the `dedup_tick` path). So once a capture is
fast enough to finish inside one tick, this existing dedup will naturally yield
one frame per tick — no scheduling change needed.

## 3. The change

Split the single `want_regions` gate into two independent capture concerns:

1. Always (when recording) capture the **AI pools** — keep `read_ai_windows`.
2. Optionally skip the **object windows** — gate `read_object_windows` behind a
   new flag.

Concretely:

- Add a setting, e.g. `HaloApp::record_ai_only: bool` (default `false`), alongside
  `recording_stream` (`app.rs:547`, init near `app.rs:658`). Surface it as a
  checkbox next to the record control in the UI (the same place
  `recording_stream` is toggled; see `crates/halo-viewer/src/settings.rs` if
  settings are centralized there).
- Add a field to the capture command struct (the `WorkerCmd::Capture { … }`
  variant, ~`app.rs:48`), e.g. `skip_object_windows: bool`, and set it from
  `self.record_ai_only` at the construction site (`app.rs:752`).
- In the worker (`app.rs:292`), change the gate to:

  ```rust
  if want_regions {
      if !skip_object_windows {
          halo_decode::read_object_windows(&mut reader, &state);
      }
      halo_decode::read_ai_windows(&mut reader, &state);
  }
  ```

That is the whole functional change. `captured_regions()` then returns only the
AI-pool regions for lean frames.

### Optional further trim (only if still > 1 tick/frame after the above)

`read_ai_windows` reads the **full `maximum_count`** slot span of each pool
(to capture sparse slots past `current_count`). For a per-tick AI recording you
usually only need the **live** slots. If lean capture still spans >1 tick, add a
variant that caps the actor/prop span to `current_count` (see `capture_pool_span`,
`decode.rs:291`). Do this only if measurements require it — it changes which slots
a lean recording contains.

## 4. Compatibility guarantees (do not break these)

- **File format is region-count agnostic.** `Recording::save` / `write_gz`
  (`crates/halo-viewer/src/replay.rs:145`) serialize `(t, region_count, [(addr,
  len, bytes)…])` per frame. Fewer regions per frame = smaller frames, **same
  format**. The loader (`replay.rs` ~197) and all external consumers read
  `region_count` generically. **Do not change the format or the magic.**
- **Downstream contract.** External tooling (in the lift repo) consumes a
  recording purely through the `data_t` pool headers it finds in the regions:
  the actor pool (`*0x6325A4`, es `0x724`) and perception-prop pool (`*0x5AB23C`,
  es `0x138`). Both are produced by `read_ai_windows`, which lean mode **keeps**.
  So lean recordings remain fully usable downstream. The object pool
  (`*0x5A8D50`) is optional for those consumers.
- **Default unchanged.** With `record_ai_only = false`, behavior must be
  identical to today (object windows still captured). Verify the existing tests
  still pass: `crates/halo-viewer/tests/record_capture.rs`,
  `tests/e2e_snapshot.rs`, `tests/snapshot_export.rs`.

## 5. Known consequence (call it out in the UI)

A lean recording has **no object-window regions**, so the viewer's own
replay/microscope/deep-field/object-diff views will show empty/partial object
data when scrubbing a lean clip (the AI tables remain fully populated). This is
expected. Make `record_ai_only` an explicit opt-in and, ideally, label lean
recordings in the recordings list so a user scrubbing one isn't confused by empty
object views. Do **not** make it the default.

## 6. Verification

1. Build and run; enable the new "AI-only" record toggle; record a clip during
   active gameplay with AI present.
2. Confirm the cadence dropped: the per-frame `t` deltas should fall from ~0.28 s
   toward ~0.033 s (one 30 Hz tick), or at worst 1–2 ticks. (Inspect the saved
   `.halorec`: it stores `t` per frame.)
3. Confirm the AI pools are still fully captured: the actor pool (`*0x6325A4`)
   and perception-prop pool (`*0x5AB23C`) headers and element spans must be
   present and intact in each frame; only the object windows should be absent.
4. Confirm a non-lean recording is byte-identical to before (default path
   untouched) and the listed tests pass.

## 7. Out of scope (do not touch)

- The `.halorec` binary format / magic / version (`replay.rs`).
- `read_ai_windows`, `read_game_tick`, or the tick-dedup logic — they are correct;
  the only change is *not calling* `read_object_windows` in lean mode.
- The backends (`xbdm.rs`/`qmp.rs`/`gdb.rs`). (If you want maximum benefit,
  recommend the user pick the non-halting QMP `memsave` backend for lean
  recording, but that is a runtime choice, not a code change.)
- Anything in the lift repo. The consumer of these recordings (a per-tick
  "trajectory" differential that checks a lifted function's owned fields against
  the next frame) lives there and is being built separately; it only needs the
  finer cadence this change produces.

## 8. One-paragraph summary for the PR

> Recording currently issues one backend read per object (~480/frame) plus the AI
> pools, so each frame costs ~500 halting round-trips (~0.28 s ≈ 8.5 game ticks).
> Add an opt-in "AI-only" recording mode that skips `read_object_windows` while
> keeping `read_ai_windows`, cutting per-frame reads ~20× so the existing
> per-tick dedup can capture ~1 frame per game tick. Format unchanged (region
> lists are variable-length); the full-fidelity default is untouched. Lean clips
> lack object-window data and are labeled as such.
