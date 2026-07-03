# .githooks/ — NOT the active hooks directory

These three files (`pre-commit`, `post-checkout`, `prepare-commit-msg`) are thin
wrappers that `exec` into `tools/hooks/`. They are kept as a portable, tracked
record of the intended hook wiring.

**They are dormant.** This repo's `core.hooksPath` is **`.git/hooks`** (untracked),
not `.githooks/`, so git never runs the files in this directory.

## Authority: `tools/hooks/install.sh`

The real hooks are installed into `.git/hooks` by the idempotent installer:

```bash
tools/hooks/install.sh          # install/repair (backs up the previous state)
tools/hooks/install.sh --check  # report drift only (run at SessionStart)
```

The installer keeps `core.hooksPath=.git/hooks` and:

- symlinks `pre-commit`, `post-commit`, and **`prepare-commit-msg`** (which git
  never installs on its own — its absence silently disabled auto-lift commit
  messages) into `tools/hooks/`;
- appends a guarded **rtk-trust** block into `.git/hooks/post-checkout` without
  clobbering codegraph's managed sync block (codegraph owns `post-checkout` /
  `post-merge` as regular files there).

If you ever want `.githooks/` to become the live directory instead, you would set
`git config core.hooksPath .githooks` **and** reproduce codegraph's managed
`post-checkout` / `post-merge` blocks here — otherwise codegraph sync stops
firing. The installer path above is preferred precisely to avoid that.
