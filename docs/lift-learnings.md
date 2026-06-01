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

**Variant — the aliased locals are the callee's OUTPUT, sized from the frame:**
Sometimes the contiguous locals are not input you set up but **results the
callee writes into the buffer**, read back from offsets near the buffer's
tail. The fix is the same (make them array indices, not separate locals), but:
- **Size the buffer from the frame, not a guess.** The original `_chkstk`
  size minus the other named locals gives the true buffer size. Example
  (reconnect, FUN_001417c0): `_chkstk 0x1028` = `0x1010` buffer + `0x10`
  iterator + `0x8` bsp_data ⇒ `int local_102c[1028]`. The read-back flags
  were `local_102c[771]` (EBP-0x41c) and `[772]` (EBP-0x418) — *past* the
  Ghidra-guessed `[771]` array end. Undersizing risks the callee writing
  past your buffer → stack corruption.
- **Re-deriving the region exposed a SECOND bug.** The same disasm showed a
  `MOV [EBP-0x8],EAX` store (leaf index → bsp_data[0]) the original lift
  dropped entirely. When re-lifting a flagged/deactivated function, re-derive
  *every* instruction in the changed region — the dormant C is a draft, not a
  baseline, and the flagged call is rarely the only defect.

**Variant — a small vec3 buffer with a *silent visual* symptom (no crash):**
The aliased buffer can be as small as a 3-float vec3, and the failure can be
purely cosmetic instead of an assert. `FUN_0013ab20` (compute_dynamic_object_lighting)
built the per-object light intensity as three separate locals
`float local_40 = 0; float local_3c = 0; float local_38 = 0;` while *every other*
vec3 in the same function was correctly a `char[12]`. The light helper
(`0x7afb0`, reached via `FUN_00138fd0`) writes 12 contiguous bytes to `&local_40`,
and `FUN_00139e50` reads it back as `intensity_ptr[0..2]`. Under clang the three
floats need not be adjacent (or in order), so the RGB intensity was corrupted —
a green/yellow cast on **all** objects (bipeds, weapons, scenery) that worsened
near ground-emitting teleporter lights. Fix: `float local_40[3]`.
- **Small buffers hide in plain sight.** A 14-local buffer is obvious; a 3-float
  vec3 reads like three independent scalars. Any local whose *address* is taken
  and handed to a callee that writes/reads `p[1]`/`p[2]` must be one array.
- **A lift-added `= 0` initializer is a tell.** The original prologue zeroed only
  the success flag; the lifter added `= 0` to the three floats because it sensed
  they weren't fully written. Treat a lift-added zero-init on address-taken locals
  as a prompt to check contiguity, not as a fix.
- **No assert fired** — instruction diff / VC71 looked faithful (the opcodes
  *are* faithful; the frame layout is not). See §9 for localizing this kind of
  non-crashing regression.

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
- **CR2 = 0, EIP oscillating in the kernel idle loop (≈0x8001exxx)** → a *soft
  deadlock* (a thread blocked on a wait that never completes), NOT a page
  fault. Sample `info registers` via the xemu QMP monitor 2-3×: if EIP barely
  moves and only a counter (e.g. EDX) changes while every other register is
  frozen, no game thread is running. This is a synchronization/teardown bug,
  not a lifted-arg bug — a pure-compute lift would *page-fault* (CR2≠0) if its
  args/offsets were wrong, so a CR2=0 idle hang **clears** a compute function
  as the cause. Contrast the page-fault freeze (CR2 = a float bit-pattern used
  as a pointer, see §6) which *does* implicate a lift.

---

## 6. Float-as-Pointer Bit Smuggling

**What happens:** Ghidra decompiles a float value stored through an integer
register as a series of pointer casts: `local_18 = (short *)(int)(float_expr)`.
Later, the value is passed to a function as `(float)(int)psVar3`. In C,
`(int)` on a float does a **numeric truncation** (0.75 → 0), and `(float)`
converts back (0 → 0.0). But the original code used `PUSH ESI` to put the
raw IEEE 754 bits on the stack — no conversion at all. The bit pattern
0x3F400000 means float 0.75, but as an integer it's 1,061,158,912, and
`(float)1061158912` is a massive number.

**Symptoms:** Persistent wrong colors (yellow/white tint on all objects),
wrong scale factors, or lighting that doesn't respond to distance. The
visual effect depends on which parameter gets the inflated value.

**Example:** FUN_0013ab20 computed a distance scale as a float interpolation,
stored it through `(short *)(int)`, then passed it as `(float)(int)psVar3`
to FUN_00139e50's `param_4` (distance_scale). The value ~0.5 became
~1 billion, making all lighting colors saturate to maximum — producing a
uniform yellow tint on weapons and world geometry.

**Prevention:**
- When Ghidra shows a float expression cast through `(int)` and then a
  pointer type, it's bit-smuggling. Use a proper `float` variable instead.
- Search for the pattern: `(float)(int)` on any variable that was assigned
  via `(type *)(int)(float_expr)`. Every instance is a potential bug.
- `grep -n '(float)(int)' src/file.c` after lifting to catch these.
- The underlying rule: **never use C casts to reinterpret float↔int bits**.
  Use `memcpy(&dst, &src, 4)` or a union if you genuinely need bit-casting.

**Detection at runtime:** No crash — just visually wrong output. Compare
against the unpatched game at the same camera position.

---

## 7. Ghidra cdecl Arg Mis-Grouping (inner 0-arg getter swallows the outer call's args)

**What happens:** For a call written `outer(get_x(), a, b, c)` where `get_x()`
is a **0-arg getter**, MSVC emits (cdecl, args pushed right-to-left):
```
push c ; push b ; push a ; CALL get_x ; push eax ; CALL outer ; add esp, N
```
Ghidra attributes the `push c/b/a` to the CALL that immediately follows them —
`get_x` — rendering it as a multi-arg call `get_x(a, b, c)` and leaving
`outer` with only the getter's result, `outer(eax)`. The lift faithfully
copies the mistake: the getter is called with junk extra args (cdecl-harmless,
it ignores them) and **outer is called with too few args**, reading a/b/c from
stale stack → garbage scalars or wild pointers.

This was the root cause of three runtime bugs fixed in a single session
(bored_camera, the "Loading level..." freeze, and the object name-table
corruption) and is the dominant defect class in the objects.obj mass lift.

**Symptoms:** Depends on the dropped args. Garbage scalars → nonsensical but
non-crashing behavior (random camera angles). Dropped pointer/buffer args →
page fault inside the callee (the level-load freeze was a dropped
origin/direction/results* on a BSP sphere test).

**Example (bored_camera_update, FUN_00084ae0):** Ghidra showed
`random_math_get_local_seed_address(min, max)` (declared 0-arg!) and
`random_real_range(seed)`. The pushed constants were `random_real_range`'s
range, not the getter's:
```c
random_real_range(random_math_get_local_seed_address(), -1.0996f, 0.3927f);
```

**Example (reconnect, FUN_001417c0 → collision_bsp_test_sphere):** Ghidra
showed `global_collision_bsp_get(0,0,dir,radius,results)` (declared 0-arg) and
`collision_bsp_test_sphere(bsp)`. The single `ADD ESP,0x18` after the second
CALL proved all 6 args belong to `collision_bsp_test_sphere`:
```c
collision_bsp_test_sphere(global_collision_bsp_get(), 0, 0,
                          obj+0x50, *(int*)(obj+0x5c), results);
```

**Prevention:**
- For any `outer(getter(...), ...)` where kb.json declares `getter` as 0-arg,
  STOP and read the disasm. The extra args almost always belong to `outer`.
- The **stack-cleanup tell:** a single `ADD ESP,N` after the *outer* CALL
  accounts for ALL of outer's args (N/4), even though the pushes are split
  across the inner getter CALL. If `N/4` exceeds the arg count Ghidra gave
  `outer`, args were stolen by the getter.
- Cross-check every XCALL / by-name arg count against the callee's kb.json
  decl; `tools/audit/` arg-count auditing surfaces these en masse.

**Detection at runtime:** Page fault (CR2≠0) inside the callee with a register
holding a stale stack value, OR nonsensical-but-non-crashing behavior when the
dropped args were scalars (see §5 for distinguishing the two).

---

## 8. Running Accumulator / Loop State Misread as a Constant

**What happens:** A function seeds a local accumulator from a value (often an
RNG result), mutates it each loop iteration, and branches on the running value.
Ghidra sometimes loses the data flow: it renders the initial load of the
accumulator slot as a constant (commonly `(float)0` from a `FILD` of a slot it
believes is zero) and drops the per-iteration write-back. The lift then ignores
the seed entirely and tests a never-changing expression.

**Symptoms:** The loop's selection/threshold logic silently collapses. The
function returns its "nothing matched" default every time → a feature quietly
does nothing, no crash. Here: no netgame equipment ever spawned (truly absent —
not invisible, not culled).

**Example (FUN_000aca70, item-collection weighted-random selector):** The
original is a roulette-wheel pick — `accum = random_range(0, total_weight)`,
then per entry `accum = (int)((float)accum - weight[i]); if (accum < 0) return
item[i];`. The lift (a) computed `random_range(...)` but **discarded** it
(`(void)rng;`), (b) read the accumulator load `FILD [EBP-4]` as the constant
`(float)0`, and (c) never wrote `accum` back — so it tested `weight[i] < 0`,
never true for positive weights, and always fell through to `return -1`
(no item). The disasm tell: the loop body has `FILD [EBP-N]; FSUB weight;
_ftol2; MOV [EBP-N],EAX` — a load **and store** of the same slot = a running
accumulator seeded before the loop, not a constant.

**Prevention:**
- When the decompiler shows `(float)0` (or `0`) added/subtracted inside a loop,
  check the disasm: if that slot is also *written* in the loop (`MOV [EBP-N],
  …`), it is a running accumulator seeded before the loop, not a constant.
- A value that is computed (RNG, distance, count) and then `(void)`-discarded by
  the lift is almost always meant to feed the very next computation. A
  `(void)x;` on a non-trivial result is a red flag, not a cleanup.
- Verify the loop's branch reads the mutated state, not a fixed input.

**Detection at runtime:** A feature does nothing with no error (items don't
spawn, a selection always returns the same/none). Use §9 toggle bisection.

---

## 9. Localizing Non-Crashing Runtime Regressions (toggle bisection)

VC71 match% and instruction diffing **do not converge** on FPU/stack-heavy
functions: a faithful lift there scores 98–100%, but a real behavioral bug can
sit at 98%+, and codegen-ceiling functions sit at 43–78% with the defect buried
in stack-offset and FPU-branch-encoding noise. `buffer_alias_detector.py` also
false-positives (it groups adjacent scalar locals as one buffer). The reason is
structural: **instruction-faithful ≠ behavior-faithful.** Stack-layout and
buffer-contiguity bugs (§2) are invisible to instruction comparison — the opcodes
match while the data at an offset differs because clang's frame ≠ MSVC's frame.

So for a wrong *runtime* result with no crash (tint, culling, a feature doing
nothing), the deployed build is the oracle. Bisect with the `ported` flag:
1. Revert the whole suspect chain to original (`ported=false`) + deploy. Symptom
   gone ⇒ the bug is in *our* lift, not the original (and rules out map/asset
   artifacts — confirm the symptom on a shipping map, not just a test map).
2. Re-enable functions in dependency-complete subsets and redeploy. Re-enabling a
   callee (`ported=true`) is always safe — its original entry keeps our redirect
   with args intact. Toggling a callee *off* under a still-ported caller risks the
   §1 thunk arg-drop, so split the chain at a plain-cdecl edge.
3. Combine independent symptoms in one build (e.g. green-tint vs no-spawn): each
   is observed separately, so attribution stays clean and you save deploy cycles.
4. Once localized, read the *call site* first (~10 insns, for a register/buffer
   swap), then the helper *output sizes* (floats written) vs what the caller
   reads back (§2).

Confirmed twice with this method (commit 478fd549): the §8 no-spawn selector and
the §2 green-tint vec3. **Do not declare a visual fix done from a code read — the
box is the only oracle** (a code read said "fixed" and the box disagreed
repeatedly during this hunt).
