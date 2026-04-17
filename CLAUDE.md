# Halo: Combat Evolved — Decompilation/Re-implementation

This repo is an incremental, open-source re-implementation of the original Xbox launch title *Halo: Combat Evolved* (executable `cachebeta.xbe`, version `01.10.12.2276`, MD5 `c7869590a1c64ad034e49a5ee0c02465`). Re-implemented C functions are compiled and patched into the original XBE so they replace the originals at runtime — original and re-implemented code coexist while the project advances.

For background and the live progress report, see <https://blam.info/> and <https://blam.info/progress/>.

## Mission

Recover code from the original Xbox binary quickly while staying semantically close to the original. Optimize for **faithfulness, auditability, and incremental progress**. Do not optimize for pretty modern C.

**Hard rules:**

- Treat the binary as the source of truth.
- Do not invent behavior, abstractions, or names just to make code cleaner.
- Preserve control-flow shape, side effects, memory access patterns, call patterns, and data layout.
- Prefer explicit uncertainty over false confidence. If something is unknown, keep it unknown: use names like `unknown`, `field_XX`, `pad_XX`, or `TODO`.
- Make small, reviewable changes. Do not perform broad speculative refactors.
- Do not silently replace unclear logic with guessed "equivalent" logic.
- Always add comments to functions, variables and similar when you have a high confidence your understanding of the decompiled data is correct.

**Primary goals, in order:**

1. Recover behavior faithfully.
2. Preserve ABI and data layout.
3. Keep uncertainty visible.
4. Reuse discovered knowledge from `kb.json` and existing types.
5. Improve readability only when it does not increase drift from the original.

## Analysis workflow (per function or data item)

Before writing any code for a target function or global:

1. Locate it in Ghidra by address. Use the Ghidra MCP tools to **both decompile and disassemble** it.
2. Record:
   - Address, callers, callees
   - Imports (Xbox kernel / XDK APIs used)
   - Globals touched (cross-reference against `kb.json`)
   - Strings referenced
   - Jump tables / switch structure
   - Likely subsystem or `.obj` grouping
3. Infer the narrowest defensible prototype — verify argument count from `PUSH` count and `ADD ESP,N` cleanup; verify return type from whether callers check `EAX`.
4. **Cross-check decompilation against disassembly** (mandatory — the decompiler hides critical details):
   - **Operand sizes**: `float ptr` (4 bytes) vs `double ptr` (8 bytes); `byte ptr` vs `word ptr` vs `dword ptr`. Reading a double constant as float gives wrong values.
   - **CALL targets**: Use the actual address in each `CALL` instruction, not the thunk name the decompiler shows.
   - **Interleaved pushes**: MSVC pre-pushes args for a later CALL before an earlier one. If one `ADD ESP,N` cleans more args than the preceding CALL expects, the earlier PUSHes belong to a different CALL.
   - **Register parameters**: If a register (ESI, EDI, EAX, etc.) is loaded before a CALL but never PUSHed, the callee reads it as an implicit arg. Needs `@<reg>` in `kb.json`.
5. Reuse existing project types and Xbox/XDK types before creating new ones.
6. Produce a structurally faithful first-pass implementation, even if ugly.
7. Keep pointer arithmetic, temporaries, branches, and odd logic if they help preserve fidelity.
8. Update `kb.json` conservatively; mark all inferred names and semantics as provisional unless strongly supported by the binary.
9. Summarize what is **confirmed**, **inferred**, and **still uncertain** for every non-trivial function.

## Work selection workflow

Default to `frontier-first` and `object-first` recovery instead of picking
random isolated functions.

Priority order:

1. Leaf functions on the frontier
2. Tiny utility or object-local helpers in the same `.obj`
3. Whole `.obj`-sized chunks from one subsystem
4. Harder stateful or control-heavy functions after the surrounding helpers exist

Use `tools/frontier.py` to drive this process. It reports:

- overall function coverage
- active translation units that are partially ported
- quick wins with only a few functions remaining
- recommended next targets ranked from current ported code references

When choosing between similar tasks, prefer the one that closes out or unlocks
an existing object cluster.

Use Ghidra output as a starting point, not as truth. Use `ghidra-xbe` metadata and Xbox headers to improve signatures and types. If available, use delinked object export to compare against extracted object structure. Use object diffing as a closeness signal, not as the only source of truth.

When the Ghidra delinker plugin is available, use it as an **optional**
validation lane:

1. Export a small delinked reference object for one function or a tight helper cluster.
2. Build the normal Halo candidate object with the existing toolchain.
3. Compare them with `tools/objdiff_lift.py` and `objdiff`.
4. Use the result to spot structural drift in control flow, calls, constants, or relocations.

An MCP-friendly wrapper script may be required instead of the extension's own
headless scripts. Prefer small GUI/MCP scripts in `ghidra_scripts/` that call
the installed exporter and relocation synthesizer directly.

This is advisory only. The patched XBE runtime and binary analysis remain the
source of truth.

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
- `kb_meta.py` — low-risk reverse engineering metadata manager. Validates and updates `kb_meta.json` without affecting code generation or linking.
- `frontier.py` — reports coverage and ranks object-first recovery targets from the current source tree and KB metadata.
- `export_delinked_object.py` — runs Ghidra headless with `DelinkProgram.java` to export a delinked reference object for a symbol or address range.
- `objdiff_lift.py` — runs `objdiff` against a delinked reference object and a compiled candidate object. Use this as an optional structural validation step outside the normal build.
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

## Rules for function bodies

- Preserve branch structure and return behavior exactly.
- Preserve the order of side effects.
- Preserve explicit temporaries if they map to observable binary behavior.
- Do not compress awkward logic into cleaner-looking logic unless you are certain it is truly equivalent.
- For hard functions, prefer a literal translation over a polished rewrite.
- Keep decompiler artifacts (`v4`, `pregame_info2`, `// [esp+Ch] [ebp-260h]`) if cleaning them up risks behavior drift. Clean-up is welcome only when it is clearly safe.

## Inline assembly safety

When calling original binary functions via inline asm (`__asm__ __volatile__("call ...")`), **never** use the `"=a"(result)` output-only constraint with a separate `"r"(input)` for another operand. The compiler may assign EAX to both, and writing EAX in the asm body clobbers the input before it is read.

**Dangerous pattern — do not use:**
```c
int ok;
__asm__ __volatile__("movl %1, %%eax\n\tcall *%2"
                     : "=a"(ok)               // output-only EAX
                     : "r"(handle), "r"(fn)    // compiler may pick EAX for either
                     : "ecx", "edx", "memory", "cc");
```

**Safe pattern — always use this instead:**
```c
int _eax = handle;  // pre-load the value into an EAX-tied variable
__asm__ __volatile__("call *%[fn]"
                     : "+a"(_eax)              // EAX is both input AND output
                     : [fn] "r"((void *)0xADDRESS)
                     : "ecx", "edx", "memory", "cc");
int result = (bool)_eax;
```

The `"+a"` constraint ties the variable to EAX for both input and output, so the compiler cannot assign EAX to any other operand. The function pointer gets a different register (typically EDI or ESI).

## Rules for types and structs

- Reuse existing types from `src/types.h` and Xbox/XDK headers whenever possible.
- Preserve field offsets with padding when layout is uncertain — never reorder or repad without updating the `cs`/`co` static-assert checks in `types.h`.
- Do not rename struct fields semantically without binary evidence.
- Do not create duplicate structs that represent the same memory layout.
- If a field's meaning is unclear, preserve the offset and access pattern rather than guessing a name.

## Rules for names and declarations

- Use existing canonical names from `kb.json` when present.
- If a name is inferred, keep it conservative (`sub_XXXX`, `field_XX`, `unk_XX`).
- Do not rename symbols based only on intuition; note plausible alternatives instead.
- Resolve globals and external calls through `kb.json` and existing declarations first. Do not replace unknown external functions with guessed semantics.
- Keep unresolved dependencies explicit rather than hiding them.

## What to avoid

- Large speculative rewrites or modern engine-style cleanup
- Guess-heavy type reconstruction presented as fact
- Semantic field/variable names without binary evidence
- Hiding uncertainty (unknown is better than wrong)
- Mixing behavior changes with formatting or cleanup in the same commit
- Inventing behavior for unanalyzed callees

## What this project is *not*

- Not a clean-room reimplementation: it depends on calling into the unported portions of the original XBE, so byte-for-byte fidelity to the original layout matters.
- Not a port: the target is still 32-bit Xbox / `i386-pc-win32`. There is no portability layer and no plan to make there be one in this tree.
- Not a place to add new features: PRs that change game behavior, even "improvements," are out of scope unless they're behind `DECOMP_CUSTOM` / `DEBUG_BUILD` and clearly justified.

## kb.json declaration safety

Every declaration in `kb.json` affects the linker — wrong entries silently corrupt memory at runtime. Treat `kb.json` changes with the same rigor as code changes.

`kb_meta.json` is separate on purpose. It is for low-risk workflow metadata such
as symbol status, confidence, provenance, and short notes. It must not be
treated as build truth, and it must not silently drive code generation, linker
behavior, or runtime declarations.

**Data globals (`HDATA`):**
- `HDATA char *foo;` → compiler generates an indirect load (reads pointer VALUE at the address, then dereferences). Use when Ghidra shows `DAT_XXXX` (the value at the address is a pointer).
- `HDATA char foo[N];` → compiler uses the address directly. Use when Ghidra shows `&DAT_XXXX` or accesses offsets relative to the address (e.g., `DAT_XXXX + 0x10`).
- Getting this wrong adds or removes a level of indirection, causing silent memory corruption.
- **When in doubt, use hardcoded addresses** (`*(int *)0xXXXXXX`) instead of adding a `kb.json` data entry. Hardcoded addresses bypass the import table entirely and are always safe. Reserve `kb.json` data entries for globals used across multiple functions.

**Function declarations:**
- Verify argument count against the disassembly: count `PUSH` instructions before `CALL`, confirm with `ADD ESP,N` after (N/4 = arg count for cdecl).
- Verify return type: `void` vs non-void matters — callers may check EAX.
- Variadic functions (`...`) can be thunked but need extra care.
- Register-argument functions (`@<reg>`) on the forward-thunk path (calls into unported originals) only support the register arg as the **first** parameter. The reverse-thunk path in `tools/patch.py` (used when the function is ported in C) handles single-register and two-register cases via caller-saved scratch slots; EBX/EDI source registers are not yet supported there.
- Reverse-thunk ABI rule: treat lifted C as free to clobber all caller-saved registers (`EAX`, `ECX`, `EDX`). Never park an original return address or other critical state in a caller-saved register across the call into ported C; keep it on the stack or preserve it explicitly.

**Testing discipline:**
- Build and test the ISO in xemu after **every** `kb.json` change, not in batches. A single bad declaration can crash the game with no obvious connection to the change.
- If a batch of changes causes a crash, bisect by reverting `kb.json` entries one at a time — the code implementations are usually correct; the declarations are what break things.

## When working in this repo

- Prefer `tools/xemu_qmp.py` over xemu MCP tools for routine xemu control
  (status, ISO load/eject, reset, stop/continue, HMP passthrough). Use xemu
  MCP only as a fallback when the script cannot perform the needed action.
- When the XBE misbehaves but the cause isn't obvious, run `tools/asserts.gdb`
  against the live xemu GDB stub before guessing — it breaks on every
  `severity != 0` call to the beta's `display_assert` and prints the
  original's assertion message/file/line. Much faster than chasing a
  downstream `stack_walk` page fault. See `docs/assertion_tripwire.md`.
- If you add or rename a symbol, edit `kb.json` first and re-run `tools/maintain.py` so file placement and ordering settle automatically.
- Use `kb.json` only for declarations, addresses, object membership, and source mapping that the build requires.
- Use `kb_meta.json` for tentative reverse engineering metadata such as `ported` status, confidence, provenance, and short summaries.
- Before picking a new target, run `tools/frontier.py` and prefer active TUs, quick wins, or frontier-heavy objects over unrelated isolated functions.
- After landing a function already represented in `kb.json`, update `kb_meta.json` to reflect at least its `status` when practical.
- If using the Ghidra delinker plugin, prefer exporting small coherent slices rather than large speculative regions. Use `objdiff` results to guide edits, but do not treat them as stronger evidence than the binary or runtime behavior.
- If a function comes from IDA/Ghidra output, lift it as faithfully as you can (matching control flow and arithmetic), then verify against the address in `kb.json`. Don't "fix" suspicious-looking original logic — log it instead.
- Never commit anything from `halo-patched/` (game assets) or `build/`. `.gitignore` already covers these.
- The disclaimer in `README.md` is load-bearing: this is for study and research. Don't add anything that ships copyrighted assets or assumes the user has them.

## Output format for decompilation tasks

When implementing or analyzing a non-trivial function, structure your response as:

- **Target:** function name / address
- **Confirmed:** facts grounded in the binary or existing metadata
- **Inferred:** best current interpretations (label these clearly)
- **Uncertain:** open questions and risky assumptions
- **Proposed code:** the implementation or patch
- **kb.json updates:** exact fields to add or change
- **Validation:** how closeness was checked (Ghidra diff, object diff, etc.)
- **Next steps:** smallest useful follow-up actions

## When in doubt

- Choose the more conservative interpretation.
- Preserve structure over style.
- Keep unknowns explicit — an honest `TODO` beats a plausible lie.
- Leave a clear audit trail for a human reverse engineer.
- Exact old-compiler byte matching is **not** required unless explicitly requested.
