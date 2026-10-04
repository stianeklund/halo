The goal of this project is to study and create a free, open-source re-implementation of the original Xbox launch title [Halo: Combat Evolved](https://en.wikipedia.org/wiki/Halo:_Combat_Evolved).

**Disclaimer:** This project is intended only for educational and research purposes, and is not intended to promote piracy or violation of any copyright laws. This repository does not include original game executables, nor does it include required game assets. You will need to provide these files from your own copy of the game. Buy a copy. Heck, buy two! See [`PROVENANCE.md`](PROVENANCE.md) for the evidence and sourcing policy.

**Motivation:** Fans of Halo CE will be empowered to customize and enjoy this incredible classic in new ways, to understand the mechanics behind the game, and to eventually achieve interoperability of their copy of the game with more of their personal devices. Moreover, this project serves as an exciting vehicle to drive advancements in program analysis research and tooling.

**Methodology:** Pieces of the game are slowly being re-implemented in C source code. These re-implemented pieces are then compiled and patched into the original executable, such that the re-implemented pieces are used instead of their original implementation counterparts. This approach enables incremental development, testing, and debugging.

Fork Notes
----------
This repository is a fork of the original [halo-re/halo](https://github.com/halo-re/halo) reimplementation effort,
with the same long-term goals and binary-faithful methodology. Differences from the original:

* **LLM-assisted lifting.** Most new decompilation work is produced with agent tooling (`/lift`, `/auto-lift`) and then validated against binary evidence. Every lift is untrusted until it passes build, ABI, and structural/behavioral verification. See [`AGENTS.md`](AGENTS.md) and [`docs/agent-instructions/`](docs/agent-instructions/README.md).
* **Clang/LLD is the supported toolchain.** The original README lists Visual Studio as an equal option. Here CI only builds with Clang, and CMake refuses non-MSVC compilers unless `toolchains/llvm.cmake` is used. The MSVC path remains in `CMakeLists.txt` but is not exercised by CI.
* **The full build runs verification gates.** Besides compiling and patching, `cmake --build build` runs hazard, raw-cast and thunk-conflict audits, `maintain.py --check`, and `kb_meta.py validate` before it writes the patched XBE.
* **Extra verification tooling:** equivalence testing, snapshot checks, a raw-XBE byte regression gate, and a runtime oracle (see [CI workflows](#ci-workflows)).
* **`kb_meta.json`** holds RE workflow metadata alongside `kb.json`.
* **Provenance policy and licence.** See [`PROVENANCE.md`](PROVENANCE.md), [`LICENSE`](LICENSE) and [`NOTICE.md`](NOTICE.md).

Game Code Progress
------------------
<!-- GAME_CODE_PROGRESS_START -->
[![Decompilation Progress](https://img.shields.io/badge/decompilation-95.40%25-brightgreen.svg)](https://stianeklund.github.io/halo/)
[![Ported Functions](https://img.shields.io/badge/functions-6,700%2F7,023-blue.svg)](https://stianeklund.github.io/halo/)

Progress breakdown from the [Decompilation Progress Dashboard](https://stianeklund.github.io/halo/):

* **Ported Functions:** `6,700 / 7,023` (`95.40%`)
  `[██████████████████████████████████████░░] 95.40%`
* **Ported Code Bytes:** `1,493,740 / 1,729,914` (`86.35%`)
  `[███████████████████████████████████░░░░░] 86.35%`
* **Average VC71 Mnemonic Match:** `95.50%` (`6,714` scored functions, size-weighted: `92.10%`; structural signal, not raw-byte accuracy)
* **Equivalence Tests:** `5,738` functions tested (`2,072` high confidence)
* **Translation Units:** `194` source units (`39` platform/SDK buckets tracked separately)

> Explore the interactive call graph and unit breakdown: **[Decompilation Progress Dashboard](https://stianeklund.github.io/halo/)** (or locally at [`artifacts/progress/index.html`](artifacts/progress/index.html))
<!-- GAME_CODE_PROGRESS_END -->

Community
---------
The homepage for this project is: https://stianeklund.github.io/halo/

Current State
-------------
* Able to patch and run existing game
* Main loop of the game is re-implemented
* Most functions are implemented; the remainder is tracked on the dashboard above
* Long way to go...

Getting the game running
------------------------
You need three things: your own game files, a toolchain, and somewhere to run the result.

1. **Game files (not included).** Create a `halo-patched/` directory at the repo root containing:
   * your retail disc game files, and
   * the original debug executable, build `01.10.12.2276` (MD5 `c7869590a1c64ad034e49a5ee0c02465`), named `cachebeta.xbe`.

   The build targets exactly this binary. A retail `default.xbe` will not work as a substitute, and `kb.json` addresses are absolute virtual addresses for it.
2. **Toolchain.** CMake, Python 3, and Clang with `lld` (the build uses `lld-link` to create Xbox import libraries). Install the Python dependencies with `python3 -m pip install -r requirements.txt` (`libclang`, `pefile`, `pyxbe`). The build checks these against the running interpreter.
3. **Build.** See [Build](#build). The output is `halo-patched/default.xbe`, the original game with the re-implementation patched in.
4. **Run.** Either copy `halo-patched/` to a real Xbox or run it in [xemu](https://xemu.app/). See [Deploy and run](#deploy-and-run).

Build
-----
There are two build levels:

| Command | Needs game files | What it does |
|---|---|---|
| `cmake --build build --target halo` | No | Compiles the re-implementation only. This is what GitHub-hosted CI runs. |
| `cmake --build build` | Yes (`halo-patched/cachebeta.xbe`) | Compiles, runs the audit gates, then patches the original XBE into `halo-patched/default.xbe`. |

Always configure with `-DCMAKE_TOOLCHAIN_FILE=toolchains/llvm.cmake` unless you are using MSVC. Add `-DCMAKE_BUILD_TYPE=Debug` for symbols (Release is the default).

### Linux (Ubuntu) / WSL
```bash
sudo apt install cmake clang lld python3-pip
python3 -m pip install --user -r requirements.txt
cmake -Bbuild -S. -DCMAKE_TOOLCHAIN_FILE=toolchains/llvm.cmake
cmake --build build
```

### macOS (Intel and Apple Silicon)
```bash
brew install llvm lld cmake
python3 -m pip install --user -r requirements.txt
export PATH="/opt/homebrew/opt/llvm/bin:/usr/local/opt/llvm/bin:$PATH"
cmake -Bbuild -S. -DCMAKE_TOOLCHAIN_FILE=$PWD/toolchains/llvm.cmake
cmake --build build
```

### Docker
```bash
docker build -t halo .
docker run -it --rm -u $(id -u):$(id -g) -v $PWD:/work -w /work halo /bin/bash -c "cmake -Bbuild -S. -DCMAKE_TOOLCHAIN_FILE=toolchains/llvm.cmake && cmake --build build"
```

### Windows
Use WSL with the Linux steps above. Native MSVC is still wired into CMake but is not covered by CI:
```bash
python3 -m pip install --user -r requirements.txt
cmake -AWin32 -Bbuild -S.
cmake --build build
```

### Optional CMake switches
`-DENABLE_XDK_HYBRID=ON` (compile selected TUs with XDK MSVC 7.1 for FPU codegen matching), `-DHALO_TEST_HARNESS=ON` (in-engine test harness), `-DHALO_RNG_TRACE=ON` (see [`docs/rng-trace.md`](docs/rng-trace.md)), `-DHALO_RETAIL64=ON` (experimental 64 MB retail-compatible profile). None are needed for a normal build.

Deploy and run
--------------
`tools/xbox/build_deploy_run.sh` configures if needed, builds, and uploads the patched XBE over XBDM. It deploys the XBE only, so the disc files must already be on the target.

```bash
./tools/xbox/build_deploy_run.sh -q                                    # xemu or Xbox at 127.0.0.1 (or $XBOX_HOST)
./tools/xbox/build_deploy_run.sh --xbox <xbox-ip> -q                   # real Xbox hardware
./tools/xbox/build_deploy_run.sh --xemu-bridged --xbox <guest-ip> -q   # xemu guest on a bridged adapter
```

More detail: [`docs/xbdm.md`](docs/xbdm.md), [`docs/xemu-bridged-deploy.md`](docs/xemu-bridged-deploy.md), [`docs/boot-init-and-checkpoints.md`](docs/boot-init-and-checkpoints.md). The upstream route of packaging `halo-patched/` as an ISO with `extract-xiso` is not part of this fork's workflow and is untested here.

### GDB debugging
Build with `-DCMAKE_BUILD_TYPE=Debug` using the LLVM toolchain. The build generates a `.gdbinit`, loaded automatically when you run `gdb` from the repo root. Launch xemu with `-s` to expose a GDB server.

CI workflows
------------
The badges below show the latest recorded result for each workflow on `main`. Select a badge to open its run history.

[![Main build and gates](https://github.com/stianeklund/halo/actions/workflows/main.yml/badge.svg?branch=main)](https://github.com/stianeklund/halo/actions/workflows/main.yml)
[![Audit gates](https://github.com/stianeklund/halo/actions/workflows/audit.yml/badge.svg?branch=main)](https://github.com/stianeklund/halo/actions/workflows/audit.yml)
[![Equivalence tests](https://github.com/stianeklund/halo/actions/workflows/equivalence.yml/badge.svg?branch=main)](https://github.com/stianeklund/halo/actions/workflows/equivalence.yml)
[![Snapshot verification](https://github.com/stianeklund/halo/actions/workflows/snapshot-tests.yml/badge.svg?branch=main)](https://github.com/stianeklund/halo/actions/workflows/snapshot-tests.yml)
[![Raw-XBE byte regression](https://github.com/stianeklund/halo/actions/workflows/vc71-regression.yml/badge.svg?branch=main)](https://github.com/stianeklund/halo/actions/workflows/vc71-regression.yml)
[![Runtime oracle tests](https://github.com/stianeklund/halo/actions/workflows/runtime-oracle.yml/badge.svg?branch=main)](https://github.com/stianeklund/halo/actions/workflows/runtime-oracle.yml)
[![Progress report](https://github.com/stianeklund/halo/actions/workflows/progress-report.yml/badge.svg?branch=main)](https://github.com/stianeklund/halo/actions/workflows/progress-report.yml)

Workflows that need the original game binary run on a **self-hosted runner** that holds your local game files. GitHub-hosted runners never have them, so those workflows cannot pass on a fork without your own runner (see [`docs/self-hosted-runner-setup.md`](docs/self-hosted-runner-setup.md)).

| Workflow | Runner | Triggers | Checks |
|---|---|---|---|
| Main (`main.yml`) | GitHub-hosted | push, PR | Clang Debug/Release builds on Ubuntu (Docker) and macOS (`--target halo`), agent-doc validation, ported-function regression gate, port-deactivation gate |
| Provenance Artifact Guard | GitHub-hosted | push, PR | Rejects proprietary or game-derived artifacts |
| Audit Gates | self-hosted | push to `main` (fast subset), PR, nightly | Binary-evidence audits against committed baselines; only new findings fail |
| Equivalence Tests | self-hosted | push/PR touching `src/`, `kb.json`, equivalence tools; nightly | Differential equivalence of ported functions against the original ([`docs/equivalence-testing.md`](docs/equivalence-testing.md)) |
| Snapshot Verification | self-hosted | push/PR touching `src/`, `kb.json`, equivalence tools | Ported functions against captured game state ([`docs/snapshot-verification.md`](docs/snapshot-verification.md)) |
| Raw-XBE Byte Regression | self-hosted | PR touching build inputs; manual | Rebuilds and compares raw bytes against the base revision ([`docs/byte-regression-ci.md`](docs/byte-regression-ci.md)) |
| Runtime Oracle Tests | self-hosted | nightly (night-window gated), manual | Builds, deploys to the dev Xbox and checks behavior. Never runs on push |
| Progress Report | self-hosted | push to `main`, after the nightly equivalence run | Regenerates the dashboard and the progress block above |
| Compiler Profile Calibration | self-hosted | weekly, manual | Measures VC71 mnemonic similarity per optimization profile |

Reversing
---------
Interested in reversing the game? PRs are welcome!

The process of adding re-implemented functions is mostly automated:
* Add new function/data declarations to `kb.json` as they are discovered and confirmed. The definitions in `kb.json` are used to automatically generate header files and link the new implementation with the original XBE.
* Implement new functions in the appropriate source file under `src/halo/`. Add new source files to `src/CMakeLists.txt`.
* The build system will compile and patch the XBE with redirects from the original implementations to the re-implementations.
* Your new code will call functions in the original binary that have not yet been re-implemented. These are linked automatically, provided the definitions of data and called functions are in `kb.json`.

New function ports in this fork go through `/lift` rather than being hand-committed; see [`AGENTS.md`](AGENTS.md) and [`docs/README.md`](docs/README.md) for the full doc index.

See the [Progress Report](https://stianeklund.github.io/halo/) to explore the call graph, familiarize yourself with the code base, and examine the project frontier.

Low-risk reverse engineering workflow metadata can be stored in `kb_meta.json`.
Unlike `kb.json`, it does not affect code generation or linking. Use
`tools/analysis/kb_meta.py` to validate and update metadata, and `tools/analysis/frontier.py` to
rank `.obj` clusters referenced by current ported code.

License
-------
Contributions by Stian Eklund are dedicated to the public domain under [CC0 1.0](LICENSE). Work by other authors from the original fork, and the vendored code under `third_party/`, keep their own terms; see [`NOTICE.md`](NOTICE.md). Halo and its assets belong to Microsoft and are not covered.
