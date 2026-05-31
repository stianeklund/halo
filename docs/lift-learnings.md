# Lift Learnings

Hard-won lessons from runtime crashes after activating lifted code.
Each section documents the bug class, how to detect it pre-lift, and
how we diagnosed it when it reached the Xbox.

---

## 1. XCALL to a Ported Function

**What happens:** `XCALL(0xADDR, type)(args)` does a raw indirect call to
the original XBE address. When the target is **unported**, original code is
intact and this works by accident. When the target gets **ported**, patch.py
overwrites that address with `PUSH <redirect>; RET`. The XCALL now hits the
redirect, jumps to our compiled C code, which expects proper cdecl args on
the stack. The XCALL's type signature is almost always wrong — missing args,
wrong types, or missing register args entirely.

**Symptoms:** Hang (FLD on NULL pointer), assertion failure, or silent
corruption. The crash is typically inside the *callee*, not at the call site.

**Example:** `CALL_FUN_0007a750()` was typed as `float(*)(void)` — zero
arguments. The original code did `PUSH EDI; CALL 0x7a750` (passing
`intensity_ptr`). After porting, our compiled `real_rgb_color_brightness`
read `[EBP+8]` = NULL → `FLD [NULL]` → hang.

**Prevention:**
- Before activating any batch of functions, scan for XCALLs whose targets
  are in the batch: `grep -oP 'XCALL\(0x\K[0-9a-f]+' src/file.c` and
  cross-reference against the ported set.
- Replace every XCALL with a named function call. There is no valid use
  case for XCALL when the target has a kb.json entry.

**Detection at runtime:** Thread halted with EBP==ESP (stack exhaustion from
thunk recursion) or EAX/EDX containing float-as-pointer values.

---

## 2. Stack Aliasing (Contiguous Struct as Separate Locals)

**What happens:** MSVC lays out local variables contiguously on the stack.
When a function passes `&local_80` to a callee that reads at fixed offsets
(e.g., `param_1[10]` for offset +0x28), it relies on all 14 locals being
adjacent in memory. Ghidra decompiles these as 14 independent `local_XX`
variables. Under clang, these locals are scattered across the stack frame.
The callee reads garbage at the expected offsets.

**Symptoms:** Assertion failure in the callee (e.g., `color.red >= 0.0f`
failing because offset +0x28 contains a stale stack value), or silent
corruption of lighting/geometry.

**Example:** FUN_0013b380 (render_lights) had 14 separate `int local_XX`
variables that were passed as `&local_80` to rasterizer FUN_001812c0. The
rasterizer read `param_1[10]` (offset 0x28) as `color.red`. Under clang,
that offset landed on an unrelated local. Fix: replace all 14 locals with
`int light_params[14]`.

**Prevention — the "address-of-local + callee offset" check:**
1. After lifting, grep for every `&local_XX` passed to a function call.
2. Decompile the callee. If it reads `param[N]` where N > 0, the caller
   must provide a contiguous buffer, not separate locals.
3. Check the callee's `memset`/`memcpy` size — it reveals the true
   required buffer size.
4. Replace the separate locals with a single array or char buffer of the
   correct size.

**Detection at runtime:** Assertion in a rasterizer or physics function
about out-of-range values. The call stack shows the lifted function as the
caller, not the crash site.

---

## 3. Register Argument Params Passed as NULL

**What happens:** A function with `@<reg>` annotations (e.g.,
`color_ptr@<ebx>`, `output_ptr@<esi>`) is called from lifted code with
`(float *)0` for the register params. This happens when the lift author
didn't know what values to pass — they wrote `(float *)0` as a placeholder
because the XCALL path didn't need those args. After the callee is ported,
the thunk expects all args via cdecl, including the register params.

**Symptoms:** NULL pointer dereference inside the callee or a function it
calls (e.g., `real_rgb_color_brightness` receiving NULL `color` pointer).

**Example:** FUN_00139e50 has `@<ebx>`, `@<esi>`, `@<edi>`. The call in
FUN_0013ab20 passed `(float *)0, (float *)0, (float *)0`. Original disasm
showed: `LEA EBX,[EBP-0x84]; MOV ESI,[EBP+0x10]; LEA EDI,[EBP-0x3c]`.
Fix: pass `local_88`, `param_3`, `&local_40`.

**Prevention:**
- When lifting a call to an `@<reg>` function, always check the original
  disassembly for the LEA/MOV instructions that set up the register args
  immediately before the CALL.
- Never pass NULL for `@<reg>` params. If you don't know the value, leave
  the function unported until you do.
- `grep -n '(float \*)0\|(void \*)0\|(int \*)0' src/file.c` after lifting
  to find placeholder NULLs.

---

## 4. Parameter Corruption by Loop

**What happens:** Ghidra decompiles a REP MOVSD loop as
`*param = *src; param++; src++` — modifying the function parameter
directly. But the original code used a copy register (EDI) for the loop
while preserving the parameter in EAX or on the stack at [EBP+N]. After the
loop, the parameter is used again (e.g., passed to another function). In
our C code, the parameter has been advanced past the end of the buffer.

**Symptoms:** A downstream function receives a garbage pointer (typically a
valid-looking address that points into unrelated memory, or a float value
reinterpreted as a pointer).

**Example:** FUN_0013ab20's memcopy loop did `param_3 = param_3 + 1` for 29
iterations, advancing it 0x74 bytes. Later, `param_3` was passed as
`output_ptr@<esi>` to FUN_00139e50. ESI ended up as 0x3ea4a4a5 (a float
value from the memory region 0x74 bytes past the buffer).

**Prevention:**
- When a function parameter is used in a loop AND used again after the
  loop, always use a separate copy variable for the loop iteration.
- Check: does the original disassembly use a different register for the
  loop than for later uses of the same value? If `MOV EDI, [EBP+0x10]`
  precedes `REP MOVSD` but later code reads `[EBP+0x10]` directly, the
  parameter must not be modified.
- Ghidra's `puVar10 = param_3` followed by a loop using `puVar10` is
  the correct pattern. If the loop uses `param_3` directly, it's a bug.

---

## 5. Debugging Runtime Crashes on Xbox (Methodology)

When a lifted function causes a hang or crash on map load, this is the
diagnostic procedure that works:

### Step 1: Get the crash context
```bash
python3 tools/xbox/xbdm_debug_txt.py -20          # check for assertions
python3 tools/xbox/xbdm_rdcp.py "halt"             # halt the CPU
for t in 96 80 32 28; do
  echo "=== Thread $t ==="
  python3 tools/xbox/xbdm_rdcp.py "getcontext thread=$t control int"
done
```
Thread 28 is typically the main game thread. Look for EBP==ESP (stack
corruption/exhaustion) or EIP in an unusual range.

### Step 2: Identify the crashing function
Read the XBE memory at the crash EIP to see the instruction. Then check if
the address is in original code (0x10000-0x1Dxxxx) or compiled code
(0x400000+). For compiled code, find what original function was redirected
there:

```bash
# Read PUSH+RET at a known ported function to find XBE target
python3 tools/xbox/xbdm_rdcp.py "getmem addr=0xADDRESS length=6"
# Bytes 68 XX XX XX XX C3 → PUSH 0xXXXXXXXX; RET
```

If the crash EIP is the target of a PUSH+RET, you've found which ported
function is crashing.

### Step 3: Walk the call stack
Read stack memory at EBP to get saved_EBP and return_addr:
```bash
python3 tools/xbox/xbdm_rdcp.py "getmem addr=<EBP> length=128"
```
Parse as little-endian 4-byte values. [EBP+0] = saved EBP (next frame),
[EBP+4] = return address. Follow the chain to find the full call stack.

### Step 4: Map XBE addresses to functions
For each return address, read the PUSH+RET redirect at known ported
functions to build a mapping. The XBE address space has our compiled code
at high addresses and original code at low addresses. Use the redirect
targets to correlate.

### Step 5: Read the original disassembly
Once you know which function and call site is wrong, disassemble the
original function in Ghidra to see what registers/stack values the code
actually expects. Compare against the C source line by line.

### Key signals
- **EAX = function's own address** → thunk infinite recursion (unimplemented
  `@<reg>` function)
- **EAX = 0, instruction is FLD [EAX]** → NULL pointer from missing argument
- **ESI/EDI = float value (0x3Exxxxxx)** → parameter corruption, the
  register contains data from a wrong memory region
- **EBP == ESP** → stack exhaustion (infinite recursion) or frame not set up
