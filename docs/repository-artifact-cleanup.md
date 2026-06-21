# Repository Artifact Cleanup

This repo should not publish original-XBE-derived binaries, delinked objects, RAM
dumps, or snapshots that embed raw game memory. Keep those files in a private
local artifact directory outside Git.

## High-Confidence Purge Targets

Remove these from Git history before publishing or sharing the remote:

```text
delinked/
artifacts/snapshots/
artifacts/snapshot_FUN_001a2290_actor_veto.json
```

These paths contain COFF objects exported from the original binary, raw memory
regions, or JSON snapshots that may embed raw memory bytes.

## Review Before Keeping

Audit these tracked binary fixtures before publishing. Keep them only if they are
synthetic or independently generated; otherwise regenerate them from clean-room
logic or replace them with hashes/metadata.

```text
tests/golden/periodic_tables/*.bin
ci/snapshot.json
```

## Local Preservation

Before removing or rewriting anything, copy local-only evidence outside the repo:

```bash
mkdir -p /mnt/g/dev/halo-local-artifacts
rsync -a delinked/ /mnt/g/dev/halo-local-artifacts/delinked/
rsync -a artifacts/snapshots/ /mnt/g/dev/halo-local-artifacts/snapshots/
rsync -a artifacts/snapshot_FUN_001a2290_actor_veto.json /mnt/g/dev/halo-local-artifacts/
```

## Non-History Removal

This keeps local files in the worktree but removes them from future commits. It
does not remove old copies from Git history.

```bash
rtk git rm -r --cached delinked artifacts/snapshots
rtk git rm --cached artifacts/snapshot_FUN_001a2290_actor_veto.json
```

## History Purge

Run this only during a coordinated maintenance window. It rewrites commit hashes.

```bash
git filter-repo \
  --path delinked/ \
  --path artifacts/snapshots/ \
  --path artifacts/snapshot_FUN_001a2290_actor_veto.json \
  --invert-paths

rtk git push --force-with-lease origin main
```

After the force-push, collaborators should reclone or hard-reset their local
branches to the rewritten remote. Delete remote branches or tags that still point
to pre-cleanup history.
