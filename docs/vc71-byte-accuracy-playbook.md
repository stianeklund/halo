# VC71 Byte Accuracy Tuning Playbook

This playbook documents the systematic methodology for improving Visual C++ 7.1 byte match accuracy of lifted Halo CE functions from **50%–80%** to **90%–100%**.

---

## 1. Diagnostic Triage Ladder

Always start by running `vc71_verify.py` on the target source file with `--no-cache` and targeted diagnostic flags:

```bash
# Get baseline scores for all functions in the file
rtk python3 tools/verify/vc71_verify.py src/halo/path/to/file.c --no-cache

# Inspect line-by-line assembly diffs for a specific function
rtk python3 tools/verify/vc71_verify.py src/halo/path/to/file.c --function <func_name> --show-diffs

# Run targeted warning probes
rtk python3 tools/verify/vc71_verify.py src/halo/path/to/file.c --loadw-only  # Integer vs short/char load width
rtk python3 tools/verify/vc71_verify.py src/halo/path/to/file.c --imm-only    # Immediate constant / magic literals
rtk python3 tools/verify/vc71_verify.py src/halo/path/to/file.c --fcom-only   # FPU comparison sense (< vs <=)
```

---

## 2. Tuning Patterns & Solutions

### Pattern A: Variable Type & Register Allocation Tuning

**Symptom:** `--show-diffs` shows register mismatch (`movl %esi, %eax` vs `movb %bl, %al`, or `xorl %esi, %esi` vs `xorb %bl, %bl`).

**Root Cause:** MSVC 7.1 allocates registers based on exact C variable sizes:
- 1-byte variables (`bool`, `char`, `uint8_t`, `int8_t`): allocated to byte registers (`AL`, `BL`, `CL`, `DL`).
- 2-byte variables (`short`, `uint16_t`, `int16_t`): allocated to word registers (`AX`, `BX`, `CX`, `DX`).
- 4-byte variables (`int`, `uint32_t`, pointers): allocated to dword registers (`EAX`, `EBX`, `ECX`, `EDX`, `ESI`, `EDI`).

**Remedies:**
1. **Local Boolean / Status Flags**: Change `int success = 0;` to `bool success = false;` or `char success = 0;`. MSVC will switch from `ESI`/`EDI` to `BL`.
2. **Loop Counters & Bounds**: Change `int i; int num_words;` to `short i; short num_words;` when indexing word or short arrays. MSVC will emit word-sized comparison and increment instructions (`testw %cx, %cx`).
3. **Function Parameters**: Change `int param_1` to `char param_1` or `short param_1` if the disassembly loads a byte (`movb 0x8(%ebp), %al`) or word (`movw 0x8(%ebp), %ax`).

---

### Pattern B: `kb.json` ABI Declaration Alignment

**Symptom:** Low match score despite matching C logic, or `[REGPARM]` stripped loads failing to mode match.

**Remedies:**
1. **No Spaces in `@<reg>` Annotations**: Ensure register annotations in `kb.json` have NO spaces between the parameter name and `@<reg>`:
   - **CORRECT:** `"char FUN_0012fd80(int server@<esi>, int machine);"`
   - **INCORRECT:** `"char FUN_0012fd80(int server @<esi>, int machine);"` (space breaks `vc71_verify` regparm parser).
2. **Return Type Matching**: Ensure return types in `kb.json` match the original register return:
   - `bool` / `char`: returned in `AL` (`movb $1, %al`).
   - `int` / `uint32_t`: returned in `EAX` (`movl $1, %eax`).
   - `float`: returned in `ST(0)`.
3. **Regenerate Header**: Always run `regen_decl_header()` after modifying `kb.json`:
   ```bash
   rtk python3 -c "import sys; sys.path.insert(0, 'tools/verify'); import vc71_regression; vc71_regression.regen_decl_header()"
   ```

---

### Pattern C: De-artifacting Ghidra Decompiler Output

**Symptom:** `[IMM-WARN]`, bloated instruction count, or deep nested `if`-trees.

**Remedies:**
1. **Nested `if`-Trees to `switch` Statements**: Ghidra decompiles MSVC jump tables / switch statements into nested `if (code < ...)` branches. Restoring a single C `switch (var)` block eliminates dozens of redundant compare and branch instructions.
2. **Float & Constant Literals**: Ghidra often renders constants as global address dereferences (e.g. `*(float *)0x2533c8` or `*(float *)0x2533c0`). Replace address dereferences with literal floats (`1.0f`, `0.0f`).
3. **Cross-Product Subtraction Order**: Verify subtraction order in `cross(A, B)` against disassembly:
   $$\text{cross}(A,B)[0] = A[1] \cdot B[2] - A[2] \cdot B[1]$$
   Reversing subtraction order negates vectors and breaks assembly alignment.

---

### Pattern D: Unported Callee Invocations & Linker Rules

**Symptom:** `lld-link: error: undefined symbol: _FUN_000XXXXX` during project build.

**Remedies:**
- **Unported Callees**: Must be invoked via function pointer casts `((void (*)(void))0xXXXXX)();` in source code. Do NOT declare bare C prototypes for unported addresses without registering them in `kb.json`, as `clang` / `lld-link` will emit unresolved external symbol errors.
- **Ported Callees**: Must be called by their official `kb.json` function name.

---

## 3. Step-by-Step Tuning Workflow

```
1. Run vc71_verify.py on target file -> Identify functions < 90% match.
2. Run vc71_verify.py --show-diffs --function <func> -> Inspect assembly diffs.
3. Check kb.json decls -> Remove spaces in @<reg>, align return/parameter types.
4. Tune C types -> Match int/short/bool to MSVC register loads (AL/BL/CX/ESI).
5. De-artifact logic -> Replace if-trees with switch, address loads with literals.
6. Re-run vc71_verify.py --no-cache -> Verify match score >= 90% (ideal 100%).
7. Run check_lift_hazards.py --changed-only -> Verify zero hazard regressions.
8. Run build.py -q --target halo -> Verify clean project build & link.
```
