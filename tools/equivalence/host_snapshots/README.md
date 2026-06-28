# host_snapshots — host-only equivalence state (NOT in git)

State snapshots built from Halo Memory Viewer `.halorec` recordings, used by
`unicorn_diff.py --state-snapshot` (via `regression_test.py`) to drive equivalence
tests with **real** object / actor / perception-prop game state.

## Copyright boundary (why almost everything here is gitignored)

A `.halorec` recording and the `*.json` snapshot built from it both contain **raw
Halo runtime memory** — literal bytes of actor/prop records populated from the
copyrighted game. Hex-encoding into JSON does not change what they are. This repo
is **public**, so that material must never be committed — the same rule already
applied to the XBE and the delinked `.obj` references, which live host-only and
are symlinked in by the self-hosted runner.

Committed here (our own work, no game memory):
- `manifest.json` — describes how to regenerate the snapshots from recordings.
- `README.md` — this file.

Gitignored (copyrighted game memory, host-only):
- `*.json` snapshots produced by `build_host_snapshots.py` (everything except `manifest.json`).
- the `.halorec` recordings themselves (kept in a separate directory entirely; `*.halorec` is gitignored repo-wide).

## Regenerating

```bash
# point at the directory holding the .halorec recordings (default: the dev box path)
export HALO_HALOREC_DIR=/path/to/recordings
python3 tools/equivalence/build_host_snapshots.py
```

If no recordings directory is found, the generator prints a notice and exits 0;
the snapshot-dependent regression targets then **SKIP** (they never fail CI for a
missing copyrighted asset).
