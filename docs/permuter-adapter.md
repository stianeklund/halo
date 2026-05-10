# decomp-permuter Adapter for VC71/x86/MSVC

Wires `simonlindholm/decomp-permuter` to our Visual C++ 7.1 toolchain so it can
random-walk C source permutations toward a byte-matching target for functions in
the 88–99% match band.

## Dependencies

- `third_party/decomp-permuter/` — pinned at commit `efc5c5e7d9850f7267323b7dca6b41bc30a42d1f`
- `objcopy` (GNU binutils) — converts VC71/Ghidra COFF output to ELF i386 for the permuter's scorer
- `cpp` — used to preprocess source before pycparser ingestion
- Python deps: `pip install toml` (Levenshtein is optional; difflib is used by default)

## Adapter files

| File | Purpose |
|------|---------|
| `tools/permuter/compile.sh` | Called by permuter as `compile.sh input.c -o output.o`. Invokes CL.Exe, converts COFF→ELF. |
| `tools/permuter/score.py`   | Standalone scorer using `compare_obj.py` LCS ratio. Prints integer score (0=perfect). |
| `tools/permuter/run.py`     | Driver: sets up work dir, preprocesses source, runs permuter, reports results. |

## Usage

```bash
python3 tools/permuter/run.py \
  --function FUN_0014b220 \
  --source src/halo/physics/collision_features.c \
  --time 60 \
  --threads 4
```

The driver:
1. Preprocesses the source with `cpp -P` to get a fully expanded single function.
2. Writes `base.c` with explicit typedefs for pycparser compatibility.
3. Converts the delinked COFF reference to ELF (`target.o`).
4. Symlinks `compile.sh` into the work dir.
5. Runs `permuter.py --best-only`.
6. Reports initial and best LCS match percentages.

## Architecture-specific wiring

### COFF→ELF conversion

Both VC71 (our compiler) and Ghidra's delinker emit COFF i386 objects. The permuter's
`get_arch()` reads ELF headers. `objcopy -I pe-i386 -O elf32-i386` converts both
transparently. Function symbols and instruction bytes are preserved correctly.

### Windows-accessible TMPDIR

`CL.Exe` is a native Windows process running under WSL. It cannot create or read files
under `/tmp` (which is a Linux tmpfs). All temp `.c` files produced by the permuter's
`tempfile.NamedTemporaryFile` must land on a drive-mapped path (e.g. `/mnt/g/...`).
`run.py` sets `TMPDIR=/path/to/build/vc71/permuter_tmp` before launching the permuter.
`compile.sh` uses this same directory for intermediate COFF files.

### pycparser typedefs

The permuter preprocesses `base.c` with `cpp -P -nostdinc -DPERMUTER`, which strips
all system headers. `pycparser` then parses the output without knowing `uint32_t`,
`bool`, etc. Without explicit typedefs, casts like `(uint32_t)(expr)` are silently
rewritten as `uint32_t * (expr)` — a valid C expression but semantically wrong — and
VC71 rejects the result. `run.py` prepends a typedef block to `base.c`:
```c
typedef unsigned int uint32_t;
typedef unsigned char bool;
// ... etc
```
These survive `-nostdinc` because they're directly in `base.c`, not in included headers.
They do NOT conflict with `types.h` because VC71 sees them via the force-include
(`/FI xdk_common.h`) which already has identical definitions — but only after the
permuter's cpp step.

### compile.sh symlink resolution

`compile.sh` is typically symlinked from a work dir into `tools/permuter/`. It uses
`readlink -f` to resolve its own path and compute `REPO_ROOT`, so path detection works
correctly regardless of the symlink location.

## Scoring

The permuter's built-in `Scorer` uses `objdump -d --no-show-raw-insn` on both
`target.o` and `candidate.o` and computes a penalty-based diff (insertion=100,
deletion=100, regalloc=5, etc.). Score 0 = perfect match.

`tools/permuter/score.py` is an alternative scorer using our `compare_obj.py` LCS ratio.
Use it for ad-hoc comparison: `python3 tools/permuter/score.py candidate.o target.o`.

## Limitations

1. **COFF single-function isolation is hard.** The delinked reference contains the full
   translation unit. `target.o` has all functions; `base.c` has only the target function.
   The permuter's score includes a constant penalty for the deleted functions, but relative
   improvement is still valid. If per-function isolation is needed, export a per-function
   reference via `ghidra-live` MCP or `batch_delink.py --function`.

2. **pycparser parse failures (~2/60 iterations).** The randomizer sometimes produces C
   that pycparser's AST can't round-trip (e.g. compound literals, MSVC-specific syntax).
   These are non-fatal — the permuter skips them and continues.

3. **VC71 is slow for exotic transforms.** Compile time is ~0.15s/call on warm runs.
   With 4 threads (`-j4`), you get ~26 candidates/second, which is reasonable.

4. **C89 only.** Our codebase is C89; pycparser handles C99/C11 features it sees after
   preprocessing. If `base.c` uses C99 constructs after round-tripping, VC71 (C89) rejects
   them. Add `PERM_IGNORE(...)` wrappers around C99 expressions if needed.

5. **No inline asm support.** MSVC-style `__asm {}` blocks will cause pycparser failures.
   Functions with `__asm` should not be permuted.
