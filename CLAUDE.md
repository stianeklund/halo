# Halo: Combat Evolved — Decompilation/Re-implementation

This repo is an incremental, open-source re-implementation of the original Xbox launch title *Halo: Combat Evolved* (executable `cachebeta.xbe`, version `01.10.12.2276`, MD5 `c7869590a1c64ad034e49a5ee0c02465`). Re-implemented C functions are compiled and patched into the original XBE so they replace the originals at runtime — original and re-implemented code coexist while the project advances.

For background and the live progress report, see <https://blam.info/> and <https://blam.info/progress/>.

## How a re-implementation lands

The flow for adding or replacing a function:

1. Add the function/data declaration to `kb.json` (the knowledge base) with its original XBE address. `kb.json` is grouped by original `.obj` translation units; an entry's `source` field maps an `.obj` to a relative path under `src/halo/`. Symbols not yet ported still need entries here so the linker can resolve calls into the original binary.
2. Implement the function in the matching `src/halo/<area>/<file>.c`. Add new source files to `src/CMakeLists.txt`.
3. The build runs `tools/knowledge.py` to regenerate `decl.h`, `halo.xbe.def`, and `thunks.c` from `kb.json`. `tools/patch.py` then splices the freshly built PE into `halo-patched/cachebeta.xbe`, producing `halo-patched/default.xbe`.

You should never need to hand-edit the generated files under `build/generated/`.

## Code conventions (these matter — automation depends on them)

- **One function per known address, in the file `kb.json` says it belongs to.** `tools/maintain.py` will silently move functions to whatever path the KB declares — if a function isn't where the KB says, it gets relocated. Update `kb.json` and the source together.
- **Functions in each `.c` file are sorted by original XBE address, ascending.** `maintain.py --sort` enforces this. Don't reorder by hand for readability.
- **Style is enforced by `.clang-format`** (`clang-format -style=file:.clang-format -i <file>`): 2-space indent, 80-column limit, K&R-ish braces (`AfterFunction: true`, everything else attached), pointer-right (`int *p`), no tabs, no aligned assignments.
- `src/common.h` is force-included into every TU. It provides `assert_halt`, `CLAMP`, `MAXIMUM_*` constants, and pulls in `types.h`, `inlines.h`, and the generated `decl.h`. Don't re-include these per file.
- `src/types.h` is `#pragma pack(1)` and uses `cs(t, size)` / `co(t, field, offset)` static-assert macros to lock struct layouts to the original game. **Never reorder or repad a struct without updating these checks** — `tools/gen-struct-checks.py` emits the runtime verifier from this header.
- The target is **freestanding 32-bit x86, Pentium 3, MS ABI** (`-target i386-pc-win32 -march=pentium3 -nostdlib -ffreestanding -fno-builtin -fno-exceptions`). There is no libc. Use the project's `cs*` helpers (`csmemset`, `csstrlen`, `qmemcpy`, etc.) — most are routed through `src/inlines.h` or live in the original binary.
- Decompiled-style locals (Hex-Rays / IDA artifacts like `v4`, `pregame_info2`, `// [esp+Ch] [ebp-260h]`) are common in freshly imported functions. Cleaning them up is fine and welcome, but preserve behavior exactly — this is a re-implementation, not a refactor.
- Functions that need register-passed arguments use `@<reg>` annotations in `kb.json` declarations; `knowledge.py` strips them and emits register-thunk wrappers in `thunks.c`. Don't put `@<reg>` in C source.
- `-Wall -Werror` is on. Warnings break the build.

## Build

Requires Python 3 + `pip install -r requirements.txt` (libclang, pefile, pyxbe), CMake, and either Clang+lld or 32-bit MSVC. **You cannot build for x86-64** — the CMakeLists hard-errors if MSVC is configured 64-bit. Use `-AWin32` on Windows; the LLVM toolchain file already targets `i386-pc-win32`.

```bash
# Linux / WSL / macOS (Clang via project toolchain)
cmake -Bbuild -S. -DCMAKE_TOOLCHAIN_FILE=toolchains/llvm.cmake
cmake --build build

# Debug build (DWARF symbols + .gdbinit auto-generated for xemu's `-s` GDB stub)
cmake -Bbuild -S. -DCMAKE_TOOLCHAIN_FILE=toolchains/llvm.cmake -DCMAKE_BUILD_TYPE=Debug
```

A successful build produces `halo-patched/default.xbe`. **The patch step requires** `halo-patched/cachebeta.xbe` (the original retail XBE) to already be in place — the repo deliberately ships no game assets. Without it, `tools/patch.py` will fail at the end of the build; that is expected if you're only working on source.

CI (`.github/workflows/main.yml`) runs Debug + Release on Ubuntu (Docker/Clang), Windows (MSVC), and macOS (Homebrew Clang). If you change build flags, sanity-check all three paths.

## Tools cheat sheet (`tools/`)

- `knowledge.py` — KB loader and codegen. Reads `kb.json`, emits `decl.h`, `halo.xbe.def`, `thunks.c`. Run by CMake; can be invoked standalone for inspection.
- `maintain.py` — automated tree maintenance: relocates misplaced functions per KB, sorts functions by address, re-formats with clang-format. Also does `--update-from <file>` to insert/replace a single function from a snippet. **Run this after touching `kb.json` or adding functions.**
- `patch.py` — splices the freshly built PE sections into the original XBE and rewrites the entry point.
- `gen-struct-checks.py` — emits the runtime `static_assert`s for `types.h` struct sizes/offsets.
- `gen-build-info.py` — emits `build_info.c` (build date, git rev) on every build.
- `check_requirements.py` — sanity-checks that the Python deps are importable (`tools/__init__.py` shims them in).

## Xbox platform headers (`third_party/xbox/`)

Vendored Xbox API headers live in `third_party/xbox/` and are on the compiler
include path for all TUs. **Do not include them from `common.h`** — pull them
in per `.c` file only where needed.

Use `src/xbox.h` as the include wrapper. It handles `#pragma pack(push/pop)`
(the global `pack(1)` from `types.h` must not bleed into Xbox structs, which
use natural MSVC alignment) and gates subsystem headers behind opt-in macros:

```c
#define XBOX_D3D8     // Direct3D 8 — D3DDevice_*, D3DFORMAT, D3DTexture, etc.
#define XBOX_XAPI     // XINPUT_GAMEPAD, XInputOpen/GetState/SetState, etc.
#define XBOX_DSOUND   // DirectSound — CDirectSoundBuffer, DSBUFFERDESC, etc.
#define XBOX_XACTENG  // XACT audio engine
#define XBOX_XGRAPHIC // XGIsSwizzledFormat, XGSetTextureHeader, etc.
#define XBOX_XONLINE  // Xbox Live / networking stubs
#include "xbox.h"
```

`xboxkrnl.h` is always included by `xbox.h` and provides the full Xbox kernel
export list (371+ functions: `ExAllocatePool`, `MmAllocateContiguousMemory`,
`KeWaitForSingleObject`, `RtlEnterCriticalSection`, `NtCreateFile`, etc.) plus
NT base types (`LIST_ENTRY`, `RTL_CRITICAL_SECTION`, `LARGE_INTEGER`,
`NTSTATUS`, `STRING`/`ANSI_STRING`).

See `third_party/xbox/SOURCES.md` for the origin and license of every file.
Several headers carry GPL v2 (inherited from Cxbx); they are used only as type
and constant references and are not linked into any produced binary.

## What this project is *not*

- Not a clean-room reimplementation: it depends on calling into the unported portions of the original XBE, so byte-for-byte fidelity to the original layout matters.
- Not a port: the target is still 32-bit Xbox / `i386-pc-win32`. There is no portability layer and no plan to make there be one in this tree.
- Not a place to add new features: PRs that change game behavior, even "improvements," are out of scope unless they're behind `DECOMP_CUSTOM` / `DEBUG_BUILD` and clearly justified.

## kb.json declaration safety

Every declaration in `kb.json` affects the linker — wrong entries silently corrupt memory at runtime. Treat `kb.json` changes with the same rigor as code changes.

**Data globals (`HDATA`):**
- `HDATA char *foo;` → compiler generates an indirect load (reads pointer VALUE at the address, then dereferences). Use when Ghidra shows `DAT_XXXX` (the value at the address is a pointer).
- `HDATA char foo[N];` → compiler uses the address directly. Use when Ghidra shows `&DAT_XXXX` or accesses offsets relative to the address (e.g., `DAT_XXXX + 0x10`).
- Getting this wrong adds or removes a level of indirection, causing silent memory corruption.
- **When in doubt, use hardcoded addresses** (`*(int *)0xXXXXXX`) instead of adding a `kb.json` data entry. Hardcoded addresses bypass the import table entirely and are always safe. Reserve `kb.json` data entries for globals used across multiple functions.

**Function declarations:**
- Verify argument count against the disassembly: count `PUSH` instructions before `CALL`, confirm with `ADD ESP,N` after (N/4 = arg count for cdecl).
- Verify return type: `void` vs non-void matters — callers may check EAX.
- Variadic functions (`...`) can be thunked but need extra care.
- Register-argument functions (`@<reg>`) only support the register arg as the **first** parameter.

**Testing discipline:**
- Build and test the ISO in xemu after **every** `kb.json` change, not in batches. A single bad declaration can crash the game with no obvious connection to the change.
- If a batch of changes causes a crash, bisect by reverting `kb.json` entries one at a time — the code implementations are usually correct; the declarations are what break things.

## When working in this repo

- If you add or rename a symbol, edit `kb.json` first and re-run `tools/maintain.py` so file placement and ordering settle automatically.
- If a function comes from IDA/Ghidra output, lift it as faithfully as you can (matching control flow and arithmetic), then verify against the address in `kb.json`. Don't "fix" suspicious-looking original logic — log it instead.
- Never commit anything from `halo-patched/` (game assets) or `build/`. `.gitignore` already covers these.
- The disclaimer in `README.md` is load-bearing: this is for study and research. Don't add anything that ships copyrighted assets or assumes the user has them.
