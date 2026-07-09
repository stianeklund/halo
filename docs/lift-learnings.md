# Lift Learnings

Hard-won lessons from runtime crashes after activating lifted code.
Each section documents the bug class, how to detect it pre-lift, and
how we diagnosed it when it reached the Xbox.

---

## 1. XCALL to a Ported Function

**Automation:** PARTIAL — `check_xcall_types.py` catches float↔int return/param mismatches (ERROR); missing register args not yet checked automatically.

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

**Automation:** PARTIAL — `buffer_alias_detector.py` detects local_XX reads inside a buffer's address range; not yet wired into `lift_pipeline.py` (Task 2 of workflow-improvement-plan).  Frame-map contract validator planned (Task 3).

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

**Automation:** PARTIAL — `check_callee_regs` skill audits unported callees for missing register args before a new lift; `audit_reg_abi.py` checks staged caller diffs.  NULL-const passing not yet detected automatically.

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

**Automation:** PARTIAL — `check_lift_hazards.py::check_param_loop_corruption` (WARN) detects function parameters mutated (++/--/+=) inside a loop body and used afterwards.  Regex-based; imperfect parsing may miss complex cases.

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

**Automation:** not mechanically detectable — methodology section, no bug pattern.

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

**Automation:** PARTIAL — `check_lift_hazards.py::check_float_int_bit_smuggling` (WARN) uses a two-pass per-function scan: detects a variable assigned via `(T*)(int)(expr)` that is later read via `(float)(int)var`.  Does not catch the case where the Ghidra pointer name is a cast chain without an explicit intermediate variable.

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
- **`*(type*)&local_XX` is ambiguous:** Ghidra uses this syntax for two distinct
  situations. (a) *Genuine type-punning* — same logical value reinterpreted as a
  different type (this section; fix with memcpy/union). (b) *Lifetime-based slot
  reuse* — two unrelated variables share the same EBP offset because their
  lifetimes don't overlap (optimizer decision); just declare two separate C locals
  and the compiler re-coalesces them — no union needed, and forcing one can change
  codegen. Tell them apart by checking whether the two "uses" of the slot are
  semantically the same value or independent data.

**Detection at runtime:** No crash — just visually wrong output. Compare
against the unpatched game at the same camera position.

---

## 7. Ghidra cdecl Arg Mis-Grouping (inner 0-arg getter swallows the outer call's args)

**Automation:** PARTIAL — stack-cleanup cross-check (Task 5 of workflow-improvement-plan): compare `ADD ESP,N` cleanup after each CALL against the callee's kb.json arg count; 0-arg getter followed by outer with missing args is the specific signal.  Not yet implemented as a standalone check.

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

**Automation:** PARTIAL — `check_lift_hazards.py::check_discarded_result` (WARN) detects `(void)var;` applied to a variable that was assigned from a function call in the preceding lines.  Does not yet detect the `(float)0` constant-load variant (accumulator seed not discarded, just read as zero).  Draft-decompiler annotation for accumulator slots planned (Task 8 of workflow-improvement-plan).

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

**Automation:** not mechanically detectable — methodology section, no bug pattern.

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

---

## 10. Caller-Site Register Order Contradicts Thunk Convention

**Automation:** PARTIAL — `audit_reg_abi.py` confirms registers are written before CALL but not which value flows to which register.  `dump_caller_regsetup.py` (new) mechanises evidence-gathering: given a callee address or name it disassembles every original CALL site from the pristine XBE and prints each caller's last register load per GPR plus the PUSH sequence; value→register comparison against C call sites remains manual review.  Value→register mapping check planned as Task 6 of workflow-improvement-plan.

**What happens:** A function declared `@<ecx>, @<eax>, @<ebx>` has its thunk
map first C arg → ECX, second → EAX, third → EBX. When ported callers call it
via the thunk, this is the expected order. But in the *original binary* a
specific caller may load the registers in a different order — e.g.,
ECX=first, **EBX=second, EAX=third** — before the CALL. The `@<reg>`
annotation describes what the *callee body reads*; it does not guarantee every
original caller prepared them in the same sequence. When that caller is ported
and calls the function in C with `f(a, b, c)`, the thunk sends b→EAX and c→EBX,
but the original binary expected b→EBX and c→EAX, so the second and third
arguments are silently swapped.

**Symptoms:** No crash. Silent wrong output that depends on arg semantics. If
the swapped arg is a small event-type integer used as a switch discriminant,
and the other arg is a datum handle (~0x00010000+), the switch hits its default
case on every call → a feature does nothing. No assertion, no log entry.

**Example:** `game_engine_player_event` (0xad0c0) called
`game_engine_hud_update_player` (0xacef0, `@<ecx>,@<eax>,@<ebx>`) with
ECX=param_1, **EBX=param_2** (kill event type), **EAX=param_3** (other player
handle). C thunk put param_2 → EAX and param_3 → EBX — swapped. Inside
`game_engine_get_score_hud_text`, `switch(param_2)` received a datum handle
(~0x00010000) instead of an event type (0–19) → always hit default → no kill,
suicide, or medal HUD text. The bug was live for months because the game still
ran without crashing.

**Fix:** In the ported caller, swap the argument positions in the C call to
match the original caller's register assignment:
`game_engine_hud_update_player(param_1, param_3, param_2)`.

**Prevention:** For any callee with `@<reg>` args, run
`rtk python3 tools/audit/dump_caller_regsetup.py <callee>` to dump every
original CALL site's last register load per GPR and PUSH sequence in one pass.
Compare each site's decoded summary against the C thunk convention.  If a
caller loads EBX before EAX (or any non-canonical order), its ported C call
must swap the corresponding args.  Critically, when a §10 swap is found and
fixed on one caller, audit **every** caller of that callee before closing —
`game_engine_hud_update_player` (0xacef0) had its swap fixed in `0xad0c0`
months before the sibling `FUN_000acff0` (0xacff0) was ported; that caller
carried the same swap undetected until 2026-06-10 because no all-callers audit
was done at the time of the first fix.  This is especially worth checking when
the same function has both direct callers and indirect callers (via an
intermediate dispatch function) — they often use different preparation
sequences.

**Detection:** No runtime signal. Only way to catch this before deployment: for
each ported caller, verify the call-site disassembly matches what the C thunk
convention sends. A switch inside the callee that accepts only small integers
but receives a datum handle is the smoking gun in a decompiler trace.

---

## 11. Builder Returns Count; Consumer Ignores It → Assert on Missing Entry

**Automation:** PARTIAL — `check_lift_hazards.py::check_discarded_result` (WARN) flags `(void)func(...)` for non-exempt named callees that likely return non-void values.  The specific builder-count class requires knowing the callee's return semantics; manual review remains necessary for first instances.

**What happens:** A sorted-array builder (data_iterator_next loop) both fills
an output buffer AND returns the count of active entries. The consumer function
calls the builder, discards the count, and then searches the buffer with a
hard-coded upper bound (e.g., `if (i > 15) assert`). When an entry is absent
from the buffer — because the iterator's active-bit check skipped it — the
search runs past the last valid entry, hits the bound, and asserts.

**Symptoms:** Assertion with a message like `"place<MULTIPLAYER_MAXIMUM_PLAYERS"`
at a search loop inside the consumer. The crashing function is not the builder
and not the caller: it is the consumer that searches the buffer the builder
populated.

**Complication — datum_get vs data_iterator_next divergence:** The entity whose
handle is being searched may still pass `datum_get` (salt/generation check
succeeds) even though `data_iterator_next` skipped it (active bit = 0 in the
datum header). Guards using `datum_get` or `datum_absolute_index_to_index` to
pre-filter the caller will not help: both return non-zero for the entry, but the
builder's iterator still omits it. The only correct fix is to bound the search
by the builder's return value.

**Example:** `FUN_000abd20` (sorted player array builder) returned an active
player count, but `FUN_000abf50` discarded it and searched all 16 slots. When
a quitting player's datum was absent from the iterator output but still passed
`datum_get` (their salt was unchanged), the search found no match and asserted.
Fix: capture the builder's return value and exit the search loop when `i >= count`,
returning a zeroed output for the "not found" case.

**Prevention:** When a function calls a data-structure builder and then searches
the result, check whether the builder returns a count. If it does and the caller
discards it with `(void)` or by casting to nothing, treat that as a latent bug
waiting for the first time an entry is legitimately absent from the output.

## 12. Permuter "No Improvement" Can Be Vacuous (Candidate Never Compiled)

**Automation:** FULL — `tools/permuter/compile.sh` with the `strip_dup_typedefs.py` hook, wired in `tools/permuter/run.py`.  The fix (PYCPARSER_TYPEDEFS guard stripping) ships in the repo.  Verified: permuter now iterates and finds improving candidates.


A permuter run that prints `No improvements found` is **only** trustworthy if it
actually compiled and scored candidates. On `actor_looking.c` functions
(`FUN_00027a60`, `FUN_000153e0`, `actor_look_secondary`) every run was vacuous —
zero candidates ever compiled — so the recorded "structural ceiling /
exhausted" verdicts were artifacts. The committed VC71 %s came from
source-level fixes, not the permuter.

### Root cause: pycparser round-trip strips the `#ifndef TYPES_H` guard
`tools/permuter/run.py` builds `base.c` with type definitions inside an
`#ifndef TYPES_H` guard: pycparser (which has no `types.h`) needs them, but VC71
(`compile.sh` force-includes `types.h` via `/FI`, defining `TYPES_H`) must NOT
see them, or it hits redefinition errors.

The guard works for run.py's own pre-compile (it compiles the *guarded
original* `base.c`). But `permuter.py` does its own thing:
1. preprocesses with `cpp -P -nostdinc -DPERMUTER` — **no `-DTYPES_H`**, so the
   guarded type defs are *included*;
2. parses with pycparser and re-serializes the AST to the candidate source —
   **pycparser drops `#ifndef` directives**, so the guard is gone;
3. compiles the candidate with `compile.sh` → `/FI types.h` →
   `error C2371: 'vector3_t' : redefinition` (and `datum_handle_t`, plus any
   game structs the function pulled in).

Candidate never compiles → `permuter.py` reports `Unable to compile`, runs
**0 iterations**, prints `No improvements found`. **Scope is universal:**
`PYCPARSER_TYPEDEFS` always emits `vector3_t` and `datum_handle_t`, so even a
pure-zlib function with no game types (`circular_queue.c` FUN_00114630) fails
C2371 on those two — confirmed Initial LCS 98.7% but 0 iterations. Assume any
repo permuter "ceiling" claim is vacuous until a positive control proves
real iteration.

### How to detect (before trusting any permuter verdict)
- Run **without `-q`** and confirm `permuter.py` prints a real base score and
  **accruing iterations (hundreds)**. An instant exit, `Unable to compile`, or
  `Syntax error in base.c` means the run did nothing. `-q` hides permuter.py's
  own stream, so run.py's wrapper `Initial LCS: NN%` can look healthy while the
  real run scored nothing.
- To see the actual compiler error, temporarily change `compile.sh`'s
  `>/dev/null 2>&1` to capture output — **CL writes diagnostics to STDOUT, not
  stderr** — and look for `C2371` on the `permuterXXXX.c` candidate.

### Related reference-resolution bugs (all fixed) for renamed functions
When a function is renamed from `FUN_<addr>` to a real name in `kb.json`:
- **objdiff unit name still uses the address** → `find_delinked_reference`
  fails the `func_name`-in-name match and falls back to the whole-TU ref. Fix:
  rename one unit's `name` to contain the real name; delete duplicates.
- **`_resolve_ref_name` substring match** → `actor_look_secondary` matched
  `actor_look_secondary_stop` first, returning the wrong addr. Fixed to
  whole-identifier regex `\bNAME\s*\(`.
- **`typedef __int64 int64_t;` from cpp-expanded headers** → pycparser can't
  parse `__int64`. Fixed by stripping `__int64` typedefs from the
  pycparser-only block in `build_base_c`.

### Fix (implemented & verified 2026-06-02)
`tools/permuter/strip_dup_typedefs.py` + a hook in `compile.sh`: when compiling
a permuter candidate (filename `permuterXXXX.c`), strip the top-level typedefs
whose name is already defined in `types.h`, so types.h's definitions win and
there's no C2371. Typedefs `types.h` does NOT define (`vector4_t`, `uint64_t`)
are kept. `/FI` is preserved, so the candidate compiles in the **same**
environment as the real build (decls, macros, math) — preserving codegen
fidelity, which the score depends on. `run.py`'s own pre-compile of `base.c`
keeps its `#ifndef TYPES_H` guard and is untouched. Verified: the permuter now
iterates (hundreds of attempts, "0 errors") and finds improving candidates —
e.g. `actor_look_secondary` base score 5195 → new best 4995; `circular_queue.c`
FUN_00114630 iterates from a 679710 baseline. (Caveat: a function with no
per-function delinked ref scores against the whole-TU obj, giving a huge,
near-useless baseline — orthogonal reference-granularity limitation, not this
bug.)

Two operational gotchas when running from a `/tmp` worktree:
- **decl.h staleness reverts mid-run.** VC71 can't read `/tmp`, so `compile.sh`
  uses the main repo's `build/generated/decl.h`, which is regenerated stale for
  renamed functions. Chain the re-sync into the run command
  (`cp <worktree>/build/generated/decl.h <main>/... && permuter ...`) so it's
  current at base-compile time.
- **Orphaned `-j` workers.** Killing the parent (timeout/TaskStop) leaves
  `permuter.py` worker processes running; `pkill -f decomp-permuter/permuter.py`
  between runs.

---

## 13. Overlapping 16-bit Fields Packed into One 32-bit Store (CONCAT22)

**Automation:** FULL — `check_lift_hazards.py::check_concat_survival` (ERROR) prevents any CONCAT* token from reaching the final C source.  `draft_decompiler.py` rewrites them to shift/OR idioms before lifting.

**What happens:** Two adjacent 16-bit fields share one aligned dword. The
original code updates *both at once* with a single 32-bit store, which Ghidra
renders as `_DAT_XXXX = CONCAT22(high_field, low_field)` (Ghidra's
`CONCAT22(a,b) == (a<<16) | b`). A lift that doesn't recognize the dword as two
packed fields will mistranslate it — typically by editing only the low half, or
worse, by inventing an accumulation (`low += x`) where the original *sets* the
high half and *resets* the low half.

**Symptoms:** A counter/cursor that should be reset instead grows monotonically.
Here the growth marched a write cursor off the end of a fixed buffer, tripping a
bounds assert several frames later — the crash site is the *consumer* of the
field (an assert), not the function with the bad store.

**Example (`rasterizer_text_cache_character`, FUN_00183880):** the hardware
glyph cache packs `cursor_y` (0x4d04a8, low 16) and `max_char_height` (0x4d04aa,
high 16) into one dword. The original's three stores were:
```
block 1: _DAT_004d04a8 = (uint)cursor_y          -> keep cursor_y; max_char_height = 0
block 2: _DAT_004d04a8 = 0                        -> cursor_y = 0;  max_char_height = 0
block 3: _DAT_004d04a8 = CONCAT22(char_h, cur_y)  -> keep cursor_y; max_char_height = char_h
```
The prior lift dropped the `max_char_height = 0` resets (blocks 1,2) and read
block 3 as `cursor_y += char_height`. That advanced the row pen a full glyph
height *per character* until one was placed at `cursor_y = 128`, overflowing the
128-tall cache texture and tripping `bitmaps.c:421` ("y>=0 && y<bitmap->height")
during the boot-video → main-menu transition. The visible `eip=0x80` was the
assert handler (`display_assert` → `stack_walk`) faulting — a *secondary* effect
of the real bug.

**Prevention:**
- Build a field map for any global accessed at **both** 16-bit (`*(short*)X`,
  `*(short*)(X+2)`) and 32-bit (`*(uint*)X`) widths *before* lifting. Every
  `CONCAT22`/`(hi<<16)|lo` store to that address writes **two** fields; decompose
  it into the two `*(short*)` writes the original performs.
- A lift that turns a packed-field SET into a `+=` is the tell. If a field is
  only ever assigned constants/sums-of-other-fields in the original (never
  read-modify-written), it must not become an accumulator.
- `grep -n 'CONCAT22\|CONCAT44' <ghidra_dump>` and resolve every one to explicit
  sub-field stores. Never leave a `CONCAT` in lifted C.

**Detection at runtime:** A bounds/range assert in a *different* function (the
buffer consumer) a few frames after the bad write, often surfacing as a
secondary fault in the assert/stack-walk path (`eip` at a tiny address like
`0x80`). Extract the real assert string from the stack before chasing the
faulting `eip`.

---

## 14. Vertex-Buffer Stride Mismatch: Producer Floats/Vertex ≠ Consumer Stride

**What happens:** A geometry builder fills a flat float buffer, then hands it to
a fixed-function submit/draw routine that walks it at a **hard-coded stride**
(floats per vertex). If the builder writes a *different* number of floats per
vertex than the consumer reads, every vertex after the first is misaligned: the
consumer reads each vertex's fields from the middle of the previous vertex's
data. One stray vertex lands at a near-origin screen coordinate and the
primitive stretches across the framebuffer.

**Symptoms:** A diagonal smear/streak from the geometry to a screen corner (a
vertex pinned near (0,0) or (1,1)), *plus* the real geometry missing or garbled —
**both symptoms from the same bug**. A wrong color is common too, because the
packed-color slot of one vertex receives a coordinate float (e.g. `1.0f` =
`0x3f800000`, which reads as a saturated red-ish ARGB).

**Example (`rasterizer_text_draw_cached_char/chars`, FUN_00183c00 / FUN_00183cf0):**
the consumer `FUN_001741d0` reads **5 floats/vertex** (screen x, screen y, texel
u, texel v, packed color), 4 verts = 20 floats, winding TL,TR,BR,BL. The prior
lift wrote **7 floats/vertex** into a `float quad_verts[28]` buffer — each vertex
was `[x, y, tx, ty, color, 1.0, 1.0]`, where the trailing `1.0, 1.0` was actually
the *drop-shadow offset* baked per-vertex (it belongs in a separate two-pass loop
added to screen x/y, not per-vertex). At stride 5 the consumer reads vertex 0
from floats[0..4] = `[x, y, tx, ty, color]` — correct *by luck*, because it's the
buffer head — but vertex 1 from floats[5..9] = `[1.0, 1.0, x+w, y, tx+w]`, so its
screen position is **(1, 1)**: the stray `1.0, 1.0` pair becomes a vertex pinned
at the origin and the primitive stretches from the glyph to the corner = the
diagonal smear. Every later vertex then reads its screen/texel/color slots from
misaligned offsets → wrong (here reddish) color and scrambled/offscreen quads, so
the rest of the text renders "missing." The `[28]` buffer feeding a 4-vert
stride-5 consumer (which needs only 20) is the smoking gun — 8 stray floats.
Fix: rewrite both builders to stride-5, move the `1.0` shadow offset into a real
two-pass loop (pass 1 = shadow color at +1,+1; pass 2 = real color at +0,+0),
and add `cache_offset_x/y` to the **texel** coords (tx/ty), not the screen
position.

**Prevention:**
- Derive the consumer's stride from **its** disassembly (the per-vertex byte
  stride it advances by, or the vertex-size constant it multiplies the index by)
  *before* writing the producer. Make the producer's floats-per-vertex match
  exactly.
- Cross-check: `(declared buffer length) == (vertex count) × (consumer stride)`.
  A `float buf[28]` feeding a 4-vert stride-5 consumer (needs 20) is the smoking
  gun — the extra 8 floats are misattributed fields.
- Treat per-vertex constants (especially `1.0f`/`0.0f` pairs) with suspicion:
  they are frequently a shared offset, color, or w-coordinate that the original
  applies *outside* the per-vertex stores. Confirm against the original's vertex
  layout before baking anything constant into every vertex.

**Detection at runtime:** No crash — a visual smear + missing/garbled geometry.
Use §9 toggle bisection to localize to the builder, then read the consumer's
stride from disasm and diff the per-vertex field count.

---

## 15. Stale `build/halo.map` Sends Crash Symbolization to the Wrong Function

**Automation:** FULL — `tools/xbox/symbolize_exception.py` resolves pasted
exception text or address lists from the fresh `build/halo` PE export table and
`kb.json`; use it instead of `build/halo.map`.

**What happens:** `tools/build/build.py` does **not** regenerate `build/halo.map`.
A runtime crash address symbolized against that stale map resolves to whatever
function occupied the address in an *older* layout — which can be in a completely
unrelated subsystem. The whole diagnosis then chases the wrong code. A prior
session spent its entire budget blaming particle/effects code for what was a
text-rasterizer bug, purely because the map was weeks out of date relative to the
deployed build.

**Symptoms:** Symbol names that don't match the observed behavior; call stacks
that "don't make sense"; repeated dead-end hypotheses around a function that has
nothing to do with the failing feature.

**Prevention / correct procedure:**
- **Symbolize from the freshly-built PE export table of `build/halo`, not the
  `.map`.** `XBE_VA = base_addr + export_RVA`, where `base_addr =
  round_up(max XBE section end, 0x1000)` (= `0x642000` for this build). The
  export table is produced by the same build that was deployed, so it can't be
  stale relative to it.
- Prefer `rtk python3 tools/xbox/symbolize_exception.py --file /tmp/exception.txt`
  for pasted exception output, or pass individual EIP/backtrace addresses.
- Before trusting `build/halo.map` for *anything*, compare its mtime against the
  deployed build, and cross-check the deployed `DECOMP BUILD <hash>` line in
  `xbdm/debug.txt` against the binary you symbolized from. A hash/date mismatch
  means the map is lying.
- When a symbolized crash points somewhere surprising, re-symbolize from the
  export table before forming any hypothesis — wrong symbols are the most
  expensive kind of wrong, because every downstream step inherits the error.

---

## 16. Void-Return Out-Param Functions That Return EAX Implicitly

**Automation:** FULL — `check_lift_hazards.py::check_void_eax_returns` (ERROR) checks the `VOID_BUT_RETURNS_EAX` table against `decl.h`; `find_void_eax_returns.py` (Task 4 of workflow-improvement-plan) will batch-discover new candidates from disasm tails.

**What happens:** MSVC frequently ends "void-like" functions that write through
an out-parameter with `MOV EAX, [EBP+8]` (loading the first argument — the
out-param pointer — into EAX) immediately before `RET`. The function is logically
void, but MSVC returns the pointer in EAX as a convenience. Unported callers
in Ghidra are shown reading that EAX as a pointer (`puVar = (undefined4 *)fn(buf, idx)`).

When ported with `void` return type, our compiled function does not set EAX to
`param_1` on return. The unported caller receives garbage EAX and reads position
data (or any other output) from a dangling pointer. This causes:
- Wrong coordinates or positions read from the goal table
- Silent corruption (no crash — wrong numbers)
- Features that work but produce the wrong visual output

The symptom is **subtle**: the function call itself succeeds, no assert fires,
but the data read back is wrong because it came from a random address.

**Example:** `game_engine_get_goal_position` (0xa9380) writes x/y/z into `param_1`
and ends with `MOV EAX, [EBP+8]; RET`. The unported caller `FUN_000d6cc0` does
`puVar5 = (undefined4 *)FUN_000a9380(local_1c, idx)` and reads `*puVar5`,
`puVar5[1]`, `puVar5[2]`. Declared void, the CTF score-here nav pointed nowhere
and the flag-position nav showed to the carrier incorrectly.

**Detection:**
1. Check the last 4–6 instructions before `RET` in the original disassembly
   (`disassemble_function` in Ghidra MCP). If it ends with `MOV EAX, [EBP+8]`
   or `LEA EAX, [EBP-N]`, the function returns a pointer in EAX.
2. Search Ghidra's decompile of unported callers for the pattern
   `ptr = (type *)fn(buf, ...)` — if a void function's call site is assigned to
   a pointer, the function returns in EAX.
3. `check_lift_hazards.py` now checks the `VOID_BUT_RETURNS_EAX` table against
   `decl.h` and errors if any entry is still declared `void`.

**Fix:**
```c
// Before (wrong):
void game_engine_get_goal_position(int *param_1, short param_2) {
    // ... write to *param_1 ...
}

// After (correct):
int *game_engine_get_goal_position(int *param_1, short param_2) {
    // ... write to *param_1 ...
    return param_1;  // matches MOV EAX,[EBP+8] at original RET
}
```
Update the kb.json `decl` from `void fn(int *out, ...)` to `int *fn(int *out, ...)`
and add the address to `VOID_BUT_RETURNS_EAX` in `check_lift_hazards.py`.

**Unicorn catchability:** A unicorn test of the function in isolation won't catch
this — the function produces correct data in `param_1` regardless of return type.
The divergence only manifests in the **caller's** read of EAX. The correct test is
`unicorn_diff.py` on the *caller* function with `--allow-stubs --state-snapshot`,
or a targeted check comparing `*(int*)EAX_on_return` between oracle and candidate.

## 17. Address Offset Mis-Rendered as a Value Addition

**Automation:** PARTIAL — `check_lift_hazards.py::check_addr_value_add` (WARN) flags `*(T*)0xLITERAL + N` (N < 16) patterns excluding double-dereferences and counter-increment idioms.  Sibling-address cross-check (the strongest signal) is not yet automated; manual disasm verification required for each hit.

**What happens:** Two adjacent fields of a global struct live at `BASE` and
`BASE+N` (e.g. two `short` rect edges at `0x506588` and `0x50658a`). The
original reads the second field directly: `*(short*)(BASE+N)`. A lift that is
thinking of the struct as "base plus an index" writes the `+N` on the **wrong
side of the dereference**: `*(short*)BASE + N` (value add) instead of
`*(short*)(BASE+N)` (address add). These are not equal —
`((int)*(short*)0x506588) + 2 != *(short*)0x50658a` — so the function reads the
wrong field *and* adds a stray constant.

**Symptoms:** No crash (both addresses are valid globals). Silent wrong scalar:
a width computed from the height fields, a Y from an X field, etc. The effect is
a subtly-wrong geometric quantity — here a nav-point clamping ellipse with the
wrong aspect ratio, mis-positioning off-cone markers.

**Example (`nav_point_draw_single`, FUN_000d6660):** the visibility-cone
half-extents are two rect deltas. Disasm at 0xd67e9-0xd6822:
```
fVar1.int: movswl [0x50658a]; movswl [0x506586]; sub   ->  *0x50658a - *0x506586   (width = field[3]-field[1])
fVar3.int: movswl [0x506588]; movswl [0x506584]; sub   ->  *0x506588 - *0x506584   (height = field[2]-field[0])
```
The lift wrote fVar1 as `*(short*)0x506588 + 2 - *(short*)0x506584 + 2`, turning
the address offsets `0x506588+2`/`0x506584+2` into value `+2`s on the height
fields — computing `height + 4` instead of `width`.

**Prevention — the detection grep (this is the reusable part):**
- `grep -nE '\*\(\w+ \*\)0x[0-9a-f]+ \+ [0-9]'` (and the `*(T*)(ptr) + N` form on
  a named pointer). For each hit, check whether a **sibling expression reads the
  adjacent address `BASE+N` directly**. If it does, the `+N` is an address
  offset that belongs *inside* the cast: rewrite `*(T*)BASE + N` →
  `*(T*)(BASE+N)`.
- Cross-confirm against the disasm: a single `movswl [BASE+N]` (or
  `mov [BASE+N]`) with **no** trailing `add reg, N` proves the constant is part
  of the address, not a value added after the load.

**Detection at runtime:** No signal — a wrong scalar with no crash. Use §9
toggle bisection / a state-snapshot mem-trace differential to localize, then
read the disasm to confirm which address is actually loaded.

---

## 18. Deactivation Stub Re-Pushes Register-Arg Slots (Stack Offset Shift)

**Automation:** FULL — `patch.py::generate_deactivation_redirect` now builds `stack_only_indices` and only re-pushes those; 6 exact-byte self-test cases in `deact_cases` including the FUN_00014e90 regression shape.  The `--test-thunks` gate runs at build time.

**What happens:** When a `ported=false` function has `@<reg>` annotations,
`patch.py` generates a deactivation stub at the compiled impl entry that
marshals the C caller's stack args into registers and CALLs the original. The
bug: the stub re-pushed **all** C-caller args (including the register-arg
slots) onto the stack before the CALL. The original function's frame only
expects its stack-positional params; the extra register-arg slots shift every
`[EBP+N]` reference by `4 * (number of register args)`.

For `f(int handle @<eax>, char *state)` (1 reg + 1 stack):
- C caller pushes: `state`, `handle` (R→L). Stack: `[ret] [handle] [state]`.
- Old stub re-pushed both → original sees `[ebp+8] = handle` (wrong — it
  expects `state` there, since `handle` travels in EAX).
- Fixed stub re-pushes only `state` → original sees `[ebp+8] = state` (correct).

**Scope:** 22 of 50 `ported=false` `@<reg>` functions had mixed reg+stack
params. Every one had a broken stub. Pure-register functions (28 of 50) were
also affected in theory (re-pushing a slot the original ignores) but
harmlessly — the original never reads `[ebp+8]` for a pure-register callee.

**Symptoms:** ACCESS_VIOLATION inside the *original* (unported) function, with
a datum handle (e.g. `0xe364001f`) in a register that should hold a resolved
pointer. The fault address is in original code (0x10000–0x1Dxxxx), not in
our compiled code — misleading, because the *stub* is the actual culprit.

**Example (FUN_00014e90, flee firing-position evaluator):** Declared
`char FUN_00014e90(int actor_handle @<eax>, char *state_data)`. Original reads
`state_data` from `[EBP+8]` at 0x14ed7 (`MOV ESI,[EBP+8]`), then accesses
`[ESI+0x1c]` at 0x14eda. The broken stub placed `actor_handle` (a datum handle
`0xe3640003`) at `[EBP+8]` → `ESI = 0xe3640003` → `MOV EAX,[0xe364001f]` →
ACCESS_VIOLATION at 0x14eda.

**Prevention:**
- The fix is infrastructure-level (in `generate_deactivation_redirect`), not
  per-lift. No action needed when lifting — the build-time self-tests catch
  regressions.
- If adding a *new* `@<reg>` annotation to a `ported=false` function, run
  `patch.py --test-thunks` to verify the generated stub is correct.
- The diagnostic tell: a crash inside **original** code at an address that
  *should* work, with a register holding a datum handle where a pointer is
  expected. Check the deactivation stub at the compiled impl entry
  (`getmem addr=<impl_export> length=32`) — if it re-pushes more args than the
  original expects on stack, this is the bug.

**Related:** §1 (XCALL ABI mismatch — same symptom class, different cause),
[[feedback_deactivation_stub_callee_saved]] (the companion bug where the stub
also clobbered callee-saved registers).

---

## 19. "Structural Ceiling" Scores Are Often Improvable

**What happens:** A function scores 73–80% VC71 and gets labeled "structural
ceiling" — the gap attributed to `@<reg>` prologues, FPU idiom differences
(FUCOMPP vs FCOMPS), or compiler-specific scheduling. The function stays
dormant, never investigated further. But the gap often contains 5–15pp of
recoverable match hidden behind codegen noise, reachable through three
techniques that don't require inline assembly or ABI changes.

**Why "structural" verdicts are unreliable at <85%:**
§12 showed prior permuter verdicts were vacuous (candidates never compiled).
Even with a working permuter, a 120-second random search samples a tiny
fraction of the codegen space. And instruction-level diffs at 73% are too
noisy to visually separate structural from improvable — dozens of
mismatched instructions make it impossible to tell which came from the
`@<reg>` prologue and which from a suboptimal C idiom.

**Proven session example:** FUN_001a2160 went from 73.4% → 82.8% through
source rewrites, and FUN_001a1a10 went from 80.0% → 91.5% from a single
variable addition. Both were previously declared structural ceilings.

**Technique 1 — `cos()/sin()` intrinsification (replace x87 helpers):**
VC71 with `/O2` (which includes `/Oi`) compiles `(float)cos((double)x)` and
`(float)sin((double)x)` to inline FCOS/FSIN instructions. When the same
variable feeds both, MSVC uses `FLD ST0` to share the value on the FPU stack
— a pattern the `x87_fcos`/`x87_fsin` inline asm helpers cannot produce
(each helper does its own `FLD [mem]`). The fix: use standard math calls
under `#if defined(_MSC_VER) && !defined(__clang__)` and keep the x87
helpers in the `#else` path (clang `cosf` goes to libm, not FCOS).
Recovered ~5 instructions on FUN_001a2160.

**Technique 2 — pointer-base aliasing (consecutive stores):**
MSVC generates compact `FSTP [EDI]; FSTP [EDI+4]; FSTP [EDI+8]` when it
sees three stores through the same base pointer. Writing
`*(float*)(obj+0x30) = ...; *(float*)(obj+0x34) = ...; *(float*)(obj+0x38) = ...`
with separate base+offset expressions prevents this optimization. The fix:
declare `float *up_ptr = (float*)(obj + 0x30)` and write `up_ptr[0] = ...;
up_ptr[1] = ...; up_ptr[2] = ...`. Same for read-side: `float *fwd = (float*)
(obj + 0x24)` then `fwd[0]`, `fwd[1]`, `fwd[2]`. This also applies to
physics-struct fields accessed as `physics[0x2e]` vs through a `float *nv`
pointer. Recovered ~10 instructions on FUN_001a2160.

**Technique 3 — early register-load hint (permuter-proven):**
When a `@<reg>` parameter (e.g., `direction@<eax>`) is used late in the
function, MSVC may spill it after the prologue and reload it later. Saving
it to a local early (`float dir0 = direction[0]`) forces the register load
to happen while the pointer is still live in the register, matching the
original's register flow. Recovered +11.5pp on FUN_001a1a10 (80% → 91.5%).
The permuter finds these automatically but they're easy to spot manually:
any `@<reg>` parameter whose first USE is far from the function entry.

**Technique 4 — `if`-else-if cascade → `switch` (DEC-chain → CMP-chain):**
clang lowers a *dense* small-integer cascade — `if (x==0) … else if (x==1)
… else if (x==2) …` — to a **DEC-chain** (`sub $0,reg; je; dec reg; je; dec
reg; je`), treating it as a computed switch. MSVC 7.1 instead emits an
independent **CMP-chain** (`test reg,reg; jne; cmp $1; jne; cmp $2; jne`).
Rewriting the cascade as `switch (x) { case 0: … case 1: … case 2: …
default: … }` makes clang emit the CMP-chain, matching MSVC. Behavior is
identical — the variable takes exactly one arm. **Guard-clause early-returns
do NOT help:** clang compiles `if (x==k) { …; return; }` chains the same as
else-if and still DEC-chains; only `switch` flips the lowering. Recovered
**+9.8pp on FUN_00054220 (88.1% → 97.9%, ai_profile.obj)** — the whole gap
was this one cascade. Only applies to *dense, ascending* sets of ≥3 constants:
a 2-way branch (`==2 / ==1`) already compiles to independent compares (no win).

**Detector (grep):**
```bash
grep -rnE '\} else if \([A-Za-z_][A-Za-z0-9_]* == [0-9]' src/
```
Each hit is a candidate. Confirm it is a dense ascending set of ≥3 integer
constants on the *same* variable, then check the function's `vc71_verify
--show-diffs` for the DEC-chain signature (`sub $0`/`dec`/`je` where MSVC has
`cmp $N`/`jne`). If present, convert that cascade to `switch` and re-verify.
*(Automation: documented grep only — this is a match-optimization hint, not a
correctness hazard, so it is deliberately not wired into the correctness-
focused `check_lift_hazards.py`; it lives in the §19 rewrite checklist below
alongside the other manual-spot techniques.)*

**Prevention — the rewrite checklist (before declaring a ceiling):**
1. Does the function use `x87_fcos`/`x87_fsin`? → Try `cos()`/`sin()` under
   `_MSC_VER`.
2. Are there 3+ stores to consecutive offsets from the same struct/object?
   → Use a single `float *ptr` base.
3. Does a `@<reg>` parameter get used far from entry? → Add an early
   `local = reg_param[0]` load.
4. Is there a dense `if`-else-if cascade on a small-integer variable (≥3
   consecutive constants)? → Convert to `switch` (DEC-chain → CMP-chain).
   Detector: `grep -rnE '\} else if \([A-Za-z_][A-Za-z0-9_]* == [0-9]' src/`.
5. Run the permuter for 120s on the IMPROVED source (not the original) —
   the search space is smaller after manual fixes.
6. If VC71 match is still <85% after all five steps, THEN it's a genuine
   structural ceiling. Document which specific instructions are unmatched
   (FPU stack depth refs like `FLD ST(1)`, FPU comparison idioms, `@<reg>`
   prologue) so future sessions don't re-investigate.

**Variant — vector3d_scale_add operand confusion (§2 + §4 interaction):**
In FUN_001a2f40, the Ghidra decompiler lost track of which vector was
which across `vector3d_scale_add` calls — outputting `vecA` where the
disasm showed `vecB`, and `gp` where the disasm showed the in-place
buffer. The tell: two `vector3d_scale_add` calls with DIFFERENT dot-product
operands where the mathematical operation (Gram-Schmidt projection) requires
them to be IDENTICAL (project the same vector). Combined with a material-
condition inversion and a double-normalize control-flow error, this produced
5 interrelated bugs in a single branch. These bugs were invisible to VC71
comparison (they produce similar instruction counts) but would cause wrong
ground-movement physics at runtime.

---

## 20. `__chkstk` Missing Runtime — Static Buffer Workaround Kills EBP-Relative Addressing

**Automation:** RESOLVED — `_chkstk` naked stub added to `src/halo/cseries/xbox_crt.c`
(2026-06-11, commit `d01852a4`). No future function will hit this linker error. Any
`static` buffer with a `/* ... avoid _chkstk ... */` comment is a legacy workaround
that can now be converted back to a stack declaration.

**What happens:** Clang (targeting `i386-pc-win32`) emits `mov eax, N; call __chkstk`
before `sub esp, eax` for any function whose stack frame exceeds one page (4096 bytes).
`__chkstk` is a Windows CRT runtime symbol — our lld-link build had no definition for
it, so the link failed with `undefined symbol: __chkstk`.

The workaround that accumulated in the codebase: declare large local buffers as
`static`. This suppresses `__chkstk` (static storage doesn't consume stack space) and
the link error disappears. But it introduces a **permanent VC71 ceiling**: the original
binary accesses all its local buffers via EBP-relative addressing
(`lea eax, [ebp - 0x8890]`), while the static workaround uses absolute symbol addresses
(`mov eax, OFFSET _buf`). That generates entirely different instruction sequences for
every buffer access — on `FUN_00025c10` (117KB frame, 1535-instruction function) this
caused ~30% instruction divergence and a stuck score of 77.3%.

**The fix:** Add a no-op `__chkstk` stub to `xbox_crt.c`:
```c
/* Windows x86 name-mangling: C _chkstk → __chkstk in object file */
__attribute__((naked)) void _chkstk(void)
{
  __asm__("ret\n\t");
}
```
Xbox stacks are fully committed at thread creation by the kernel — page-probing is
a no-op. The stub satisfies the linker, EAX is preserved by `ret`, and the caller's
`sub esp, eax` still gets the correct frame size.

**Score impact (FUN_00025c10):** 77.3% static → **87.1% stack** after removing the
`#ifdef` guards and converting all four large locals back to plain stack arrays.
The entire 10pp gain came from the addressing change alone; no logic was touched.

**Affected functions pre-fix:**
- `FUN_00025c10` (0x25c10): 77.3% with static → 87.1% with stack
- `actor_has_accessible_firing_position` (0x25a00): stuck at 78.4% with static
  (lower impact because its buffer is accessed less densely)

**How to detect residual workarounds:**
```bash
grep -rn 'static.*avoid.*_chkstk\|static.*_chkstk\|chkstk linker' src/
```
Each hit is a buffer that can now be converted to a stack declaration. Convert,
rebuild, and re-verify — the VC71 score should improve.

**Name-mangling note:** On `i386-pc-win32`, the C compiler prepends `_` to all
cdecl symbols: C `_chkstk` → object-file `__chkstk`. Naming the C function
`__chkstk` (2 underscores) would produce `___chkstk` (3 underscores) in the object
and the linker would still fail. This is the same convention as `_ftol2` in
`xdk_rt.c` (C `_ftol2` → `__ftol2` in object), which is what the original binary
emits for float→int truncations.

---

## 21. NaN Float Literal Masking a Latent Crash

**Automation:** PARTIAL — `check_lift_hazards.py::check_float_int_bit_smuggling` (WARN) catches `(T*)(int)(float_expr)` patterns. The specific `*(float *)"bytes"` literal pattern that silently produces NaN is not yet detected automatically; a check for NaN/Inf float literals in source is planned.

**What happens:** A lifted float constant is written as a byte-pattern cast: `*(float *)"\x00\x00\xff\x7f"`. The bytes 00 00 FF 7F (little-endian) = 0x7FFF0000 as a 32-bit float. Float 0x7FFF0000 has exponent=0xFF and non-zero mantissa → **NaN**, not FLT_MAX (0x7F7FFFFF). In IEEE 754, any comparison involving NaN returns false: `distance < NaN` = false. The effect: the BSP ray test used this as its max-distance argument and silently produced **no hits at all** (every candidate surface's distance check failed). The function returned false for every test call, so downstream code that depended on the test result was never reached.

**Symptoms:** No immediate crash — but a caller-expected code path is never executed. In FUN_0014df70, the BSP test always returning false caused bsp_hit=false on every call, so the collision result was never filled and FUN_00198580 (which requires a valid cluster ref in collision_result+0xC) was never invoked. When the NaN was fixed to FLT_MAX, the BSP test began returning hits, exposing a **latent crash**: FUN_00198580 called tag_block_get_element with the uninitialised -1 cluster ref → `(-1) & 0x7FFFFFFF = 0x7FFFFFFF` → index out of range in a block of 1039 → assert.

**Example (FUN_0014df70, collision raycast):** The max_t argument to `collision_bsp_test_vector` was `*(float *)"\x00\x00\xff\x7f"` (NaN). The underlying latent bug: when the BSP test finds no cluster object refs (count=0), collision_result+0xC stays at -1 (its initialised value). The caller FUN_00198580 uses collision_result+0xC as a cluster-object index, masking with `& 0x7FFFFFFF` → 0x7FFFFFFF → tag_block_get_element assert. Fix: (1) use `3.4028235e+38f` (FLT_MAX) for the max_t constant; (2) guard against the case where count=0 leaves no valid cluster ref — in that case treat the BSP test as a miss (result=0) so FUN_00198580 is never called.

**Prevention:**
- Never use `*(float *)"bytes"` to embed float constants; write the decimal literal or a named constant so the value is human-readable.
- After any fix that changes a float constant to its correct value, check whether the old wrong value was silently suppressing a code path — run the corrected build in the scenario that previously avoided the path (e.g., multiplayer map load).
- When a latent crash is triggered by a previously-suppressed code path: search for WHERE the bad value originates, not just what asserts. The assert (`tag_block_get_element` with 0x7FFFFFFF) was a symptom; the root cause was -1 in collision_result+0xC, itself caused by count=0 after a previously-always-false BSP test.

**Detection at runtime:** After correcting a float constant, a new crash appears in a code path that was previously never reached. The new crash may be several call levels removed from the changed code — bisect by temporarily reverting the constant change to confirm the new code path is the cause.

---

## 22. In-Place Vector Normalizer Misused as a Pure Magnitude (No-Clamp Restore Dropped)

**Automation:** RESOLVED — `check_lift_hazards.py::check_inplace_mutator_misuse` (WARN), shipped in this commit. Plus the standing golden-master case for `FUN_001a2f40` ground-tangent (see `test_harness.c`). Suppress a verified-legitimate hit with `/* hazard-ok: normalize-in-place */`.

**What happens:** Three Halo vector helpers **normalize their argument IN PLACE and return its pre-normalization length** — they are NOT pure magnitude functions despite the name:
- `magnitude3d` (`FUN_00012f10`, a 2D normalize of `v[0]/v[1]`)
- `normalize3d` (`FUN_00013010`)
- `normalize2d`

The original code's clamp idiom keeps a raw copy precisely because the call destroys it:
```c
raw0 = damp * c0 - vel[0];  raw1 = damp * c1 - vel[1];
v[0] = raw0;  v[1] = raw1;
len = magnitude3d(v);                 /* v[0]/v[1] now overwritten with unit vector */
if (len > clamp) { v[0] = v[0] * clamp; v[1] = v[1] * clamp; }  /* scale the unit vec */
else             { v[0] = raw0;          v[1] = raw1;        }  /* RESTORE the raw    */
```
A "cleanup" that mistakes the helper for pure and deletes the `else`-restore makes the no-clamp branch emit the **normalized unit vector (magnitude ~1.0)** instead of the small raw delta. Injected as velocity every frame, that is a runaway speed.

**Example (FUN_001a2f40, biped ground-tangent, `flags & 1`):** disasm `bipeds_FUN_001a2f40.obj` at `LAB_001a3219` (file offset `0x2d9`) reloads `raw0` from `edi` and `raw1` from `[ebp-0xac]` in the no-clamp branch rather than reading `tang[0]/tang[1]` at `[ebp-0x44]/[ebp-0x40]` — the proof the helper clobbered those slots. With the restore present the lift matches; deleting it reproduced the "jump/walk up an incline → massive speed → die" bug. (`magnitude3d` mutation is independently locked by `test_harness.c` which asserts `v[0]/v[1]` come back normalized.)

**Prevention:**
- Treat `magnitude3d`/`normalize3d`/`normalize2d` as in-place mutators. If a later branch needs the pre-call vector, save a raw copy first and restore it — never reuse the post-call vector as if it were unchanged.
- The detector fires only on the dangerous shape: the captured length is compared with a relational clamp (`>`,`<`,`>=`,`<=`), a branch scales `v[i]` in place (`v[i] = <…v[i]…> * …`), and **no sibling branch restores `v[i]` from a non-`v` source**. The common legitimate idioms (`normalize3d(v); v[i] = -v[i];` sign-flip with the return ignored, or `len = normalize3d(v); if (len != eps) v[i] = len*v[i];` intentional rescale on a validity check) do not match and are not flagged.

## 23. NaN-Blind Degenerate-Vector Guard (`<` Lets NaN Through)

**Automation:** RESOLVED — `check_lift_hazards.py::check_nan_blind_guard` (WARN), shipped in this commit. Suppress a verified-legitimate hit with `/* hazard-ok: nan-checked */`.

**What happens:** A guard meant to reject a degenerate (zero/tiny) vector tests its squared magnitude against a small epsilon with a relational `<` (or `<=`):
```c
m2 = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
if (m2 < 1e-8f) return 0;          /* reject (0,0,0)/tiny — but NOT NaN */
```
IEEE-754 comparisons involving NaN are **unordered**: `NaN < 1e-8f`, `NaN <= 1e-8f`, `NaN > x`, and `NaN == x` are all **FALSE**. So if any component of `v` is NaN, `m2` is NaN, the guard does **not** fire, and the NaN vector flows straight into the path the guard was supposed to protect — typically a `normalize`/`assert_valid_real_normal*` that then HALTs (debug build) or produces garbage (release).

**Example (actor_looking.c:529, PoA a10, 2026-06-22 — commit 2ccb9a1f):** `look_spec_28660_safe` rejected the `(0,0,0)` copy-case look-spec with `if (m2 < 1e-8f) return 0;`. A raycast-blocked idle-look made `FUN_000283b0` return without writing its out-vector at `actor+0x570`, leaving an uninitialized X = `0xffffffff` (the stack sentinel) with valid Y/Z. The type-4 look-spec carried `{NaN, -0.821730, 0.066777}`; `m2` was NaN; `NaN < 1e-8f` was false; the guard fell through to `FUN_00028660`, which copied the NaN verbatim into the direction vector → `assert_valid_real_normal3d(-1.#QNAN0, …)` HALT. Live proof: before the fix the guard's emitted compare routed NaN to *proceed*; after, it emits `fucompi`/`jb` routing NaN to the reject branch. The original release engine reaches the same NaN here but does not HALT only because its assert is compiled out.

**Prevention:**
- Reject with the **negated** form so NaN is also caught: `if (!(m2 >= 1e-8f)) return 0;`. `NaN >= eps` is false, so `!(…)` is true → the guard fires on NaN as well as `(0,0,0)`/tiny. Verify the codegen uses an unordered-aware compare (`fucomi`/`fucompi`) routing NaN to the reject branch.
- The detector fires only on the dangerous shape: an `if (<lhs> < EPS)` / `<= EPS` where `<lhs>` is magnitude-like (a `m2`/`mag`/`len`/`dist`/`dot`-style name, a `magnitude*/dot_product*/length*/distance*` call, or an inline sum-of-products) **and** `EPS` is a small positive literal (`0 < eps <= 0.01`) **and** the guard rejects (`return`/`continue`/`break`/`goto`) within two lines. Real thresholds (`0.1`, `0.5`), integer/loop bounds, and the fixed `!(x >= EPS)` form do not match.
- The deeper fix is to not let the NaN/uninitialized value exist in the first place: zero-initialize vector slots at creation and ensure a builder that returns "no result" does not leave a stale out-vector. The guard is the safety net; prefer fixing the source when the producing path is yours.

## 24. Narrow Field Read as a Wider Type (`int` vs `int16_t`/`int8_t`)

**Automation:** FULL — `compare_obj.py::compare_load_widths` surfaces `[LOADW-WARN]` in `vc71_verify.py`; sweep the whole tree with `python3 tools/verify/vc71_regression.py loadw`. Self-test: `python3 tools/verify/compare_obj.py --self-test`. This is a byte-diff-lane check (needs a delinked reference), not a source hazard scan — see below.

**What happens:** The decompiler widens a narrow field. Ghidra frequently types a 16- or 8-bit field as `undefined4`/`int`, so the lift reads `*(int *)(p + off)` (a 32-bit load) where the original does a signed/unsigned 16- or 8-bit load (`movsx`/`movzx`, or a 16/8-bit `mov`). The extra bytes come from the **adjacent field**: when the next field is non-zero, the widened read returns a garbage value.

**Symptoms:** Not a crash by itself — a wrong scalar. If the widened value is used as a table/tag_block/array index, the high bits push it out of bounds → an assert (`tag_groups.c:3089` "#N is not a valid … index in [0,M)") or an out-of-bounds read. It does **not** change the VC71 match % meaningfully (one instruction among dozens) and it does **not** reproduce under zero-fill equivalence (the adjacent field must be non-zero), so neither the % nor seeded equivalence catches it — only the box, or this detector.

**Example (c10 Silent Cartographer scenario_load, 2026-07-01 — commit ccd6421a):** `FUN_0014df70`'s fog-zone test read `pg_surf = *(int *)((char *)pg + 0x24)`. The original does `movsx eax, word ptr [eax+0x24]` (signed 16-bit). Field `+0x24` is a surface index (0) and `+0x26` is an adjacent field (1); the dword read combined them into `0x00010000` = 65536, overflowing the count-1 fog block at `scen+0x190` → `tag_groups.c:3089` HALT. Fix: `short pg_surf` + `*(short *)`. The rebuilt code emits `movswl 0x24(%eax),%eax`, byte-identical to the original.

**Prevention / detection:**
- The detector compares the SET of narrow (16/8-bit) struct-pointer field reads on each side (PRESENCE, not count). It recognises THREE codegen forms of a narrow read — extend-load (`movswl/movzwl/movsbl/movzbl <mem>,%r32`), split load (`movw/movb <mem>,%r16`), and **fused ALU** (`testb/testw/andb/cmpw/orb/...` = a `b`/`w`-suffixed `test|cmp|and|or|xor|add|sub|adc|sbb` reading a `disp(%reg)` operand in either position) — so a correct lift MSVC compiled to any form is **not** false-flagged. The fused-ALU form was added 2026-07-02 for the **d3080 (hud_draw) false positive**: cl.exe folds `movb 0xc(%ebx),%cl; test %cl,%al` into a single `testb %cl, 0xc(%eax)`, which otherwise looked like a missing narrow load. All three map to the same `(n8|n16, disp:off)` key. Frame-relative (`%ebp`/`%esp`), absolute-global, and SIB/indexed accesses are excluded as build-specific noise; keys are (width-class, displacement) with the base register dropped, so register allocation does not matter. Presence (not count) is deliberate: two builds routinely read the same field a different number of times (CSE/recomputation) — a count-based diff false-flags that (it did, on `FUN_0014ec30`).
- A `[LOADW-WARN]` is a **review item**, not proof: verify the flagged field against disassembly. "reference reads a narrow field our lift does not" = we widened it (the dangerous direction); "our lift reads a narrow field the reference does not" = we over-narrowed (truncating a 32-bit value).
- **Known false-positive / negative sources — do NOT treat a hit as a bug without disasm:**
  - Functions whose delinked reference is bad/truncated (low VC71 match %) produce spurious hits — ignore them.
  - Base+displacement splitting: if one build folds the field offset into the base via `lea` (`lea 0x8(%esi),%eax; movswl (%eax)`) and the other uses `movswl 0x8(%esi)`, the keys differ (`disp:0x0` vs `disp:0x8`) → false flag on both sides. This is the **dominant FP class**, more visible since the fused-ALU form widened coverage (e.g. hud_messaging `d47c0` ref `addw 0x1c(%eax)` vs ours `addw 0x11c(%edx)`, Δ0x100 — same field via a different struct pointer). **Discriminator:** a real widening bug is ASYMMETRIC IN WIDTH (ref narrow, candidate wide/absent from the narrow set); a base-fold FP is SYMMETRIC width (both n16/n8) at a round-offset delta. Triage on width asymmetry, not offset difference alone.
  - Displacement-collision false negative: two different structs read narrow at the same offset both map to one key and cancel — a real widening at one can be masked by a legit narrow read at the other.
  - Indexed addressing (`disp(%reg,%idx,scale)`) is not matched, so **narrow ARRAY-element reads are not covered** — the detector does not protect int16/int8 array fields.
- Not added to `check_lift_hazards.py`: a bare `*(int*)(p+off)` in source is not a bug without the reference disassembly (most such reads are legitimately 32-bit), so a source-only heuristic would be false-positive-heavy. The mechanical detector therefore lives in the byte-diff lane where both sides are MSVC codegen and the signal is cleanest. See also `feedback_check_disasm` ("Ghidra lies about int vs int16_t").

## 25. Wrong Numeric Literal (Float/Magic Immediate Transcription Error)

**Automation:** FULL — `compare_obj.py::compare_immediates` surfaces `[IMM-WARN]` in `vc71_verify.py` (detail lines expanded by default; `--imm-only` for a focused run). Self-test: `python3 tools/verify/compare_obj.py --self-test`. This is a byte-diff-lane check (needs a delinked reference), not a source hazard scan — a bare `0.857f` in source is not detectable as wrong without the reference immediate to compare against.

**What happens:** A numeric literal in the lifted source is transcribed with the wrong value — a mistyped float constant, a wrong FourCC tag magic, or an altered integer magic. The comment beside it is often correct; only the decimal digits are wrong. Because MSVC materializes a compile-time float/int constant as an inline `push $imm32` (or `mov $imm32`), both the correct and the wrong value are the **same opcode**, so the LCS instruction-diff aligns them against each other and buries the operand delta in the sub-99% mismatch — the VC71 match % barely moves and never surfaces the constant as actionable.

**Symptoms:** Not a crash at the literal — a subtly wrong computation. For the motivating bug the wrong `cos` broke a length-preserving rotation, producing a non-unit vector that tripped `assert_valid_real_normal3d` far downstream, intermittently, only on the code path that used it. Byte-match is weak evidence for this class; the box or this detector are the only reliable catches.

**Example (actor_aim_grenade, PoA covenant combat, 2026-07-04 — commit c2866b56):** `rotate_vector3d_by_sincos(nrm, worldup, sin_a, 0.857651889f)` — the source literal `0.857651889f` compiles to `0x3f5b8f13` (cos 30.95°), but the original immediate is `0x3f5db3d7` (`0.866025388` = cos 30°). Paired with `sin_a = ±0.5` (sin 30°), the wrong cos gave `sin²+cos² = 0.9856`, shrinking every rotated facing ~0.7% → `assert_valid_real_normal3d` HALT at `actor_combat.c:1865` on grenade throws whose aim was >30° off the actor facing. The detector reports it directly: `near-miss float literal — reference 0x3f5db3d7 (~0.8660254f) vs our lift 0x3f5b8f13 (~0.8576519f) (rel err 0.967%)`. See `[[project_actor_aim_grenade_cos_literal_fixed]]`.

**Prevention / detection:**
- The detector compares the SET of large constant immediates on each side (PRESENCE, not count — like §24, because CSE materializes the same constant a different number of times). Only immediates whose **signed-32 magnitude ≥ 0x01000000** are censused: this keeps single-precision float bit-patterns (positive `0x3f5db3d7` and negative `0xc0490fdb` = −π), FourCC tag magics (`'weap'`=0x77656170), and large integer magics, while excluding XBE addresses (all below the ceiling — and relocated to 0-placeholders on the candidate side anyway), small positive integers (loop bounds, masks, struct sizes), and small sign-extended negatives (`-1` = 0xffffffff, the MSVC SEH scope-index initializer `movl $-1, -0x4(%ebp)`; stack offsets).
- A reference-only float whose nearest candidate-only float is within 5% relative error is reported as a **paired near-miss** (`reference X vs our lift Y`) — the transcription-error signature (right magnitude, wrong bits). Unpaired large constants are reported as absent/introduced.
- **Very low false-positive rate:** both objects are the SAME MSVC 7.1 (`CL.Exe`) codegen — the delinked reference is the original binary, the candidate is our source recompiled by the same compiler — so identical source constants produce identical inline immediates. A censused divergence is therefore a real SOURCE-literal difference, not compiler noise. An `[IMM-WARN]` is still a **review item**: verify the flagged literal against the disassembly immediate before changing it (a legitimately different constant, e.g. a deliberately different tuning value, is possible but rare in a faithful lift).
- Not added to `check_lift_hazards.py`: without the reference immediate, a source literal is just a number — a source-only heuristic cannot know it is wrong. The detector lives in the byte-diff lane for the same reason as §24. See also `feedback_check_disasm`, `[[reference_loadw_detector]]` (sibling immediate/width transcription traps).

## 26. VC71 Reference Truncated at Mid-Body `ret` (Multi-Return / Switch Understated Match)

**Automation:** FULL — `compare_obj.py::_first_function_insns_from_text` boundary logic, guarded by self-test cases B1–B3 (`python3 tools/verify/compare_obj.py --self-test`). Not a source hazard; it is a scoring-accuracy fix in the byte-diff lane.

**What happens:** A lifted function scores far below its true match (e.g. 40–66%) and looks like a hopeless `@<reg>`/structural ceiling, when the *lift is actually 77–93% faithful*. The delinked reference `.obj` is complete (objdump shows the full instruction count), but `vc71_verify` compares only the first ~half of it. `compare_obj.first_function_insns` bounds the per-function chunk by stopping at the first `ret` **followed by a jump-target label** — a heuristic meant to drop stale-export neighbour slots. But a **multi-return** function emits a mid-body `ret` at every early `return;`, immediately followed by the alternate-path block's label; and a **`switch`** emits `case: return;` as a `ret` before the next `caseD_` label (reached only through the indirect jump table). Both are INTERNAL control flow, so the cap truncated the reference mid-function — comparing full-candidate against half-reference and understating the score by 27–50pp.

**How to detect:** candidate/ref instruction counts in the `PASS/FAIL` line are wildly asymmetric (`191/87`, `344/100`) AND `objdump -d delinked/functions/<hex8>.obj | grep -c ':'` shows far MORE instructions than vc71's ref count. That gap = truncation, not bloat. (A genuinely bloated lift has a complete reference and a candidate larger than it.)

**Fix (2026-07-06, `network_connection.obj` cluster):** a `ret`+label is a function boundary only when the label is **neither** (a) an internal branch target already referenced by an earlier operand in the body, **nor** (b) a `caseD_`/`default` sub-label of a `switchD_<id>` whose dispatch we already fell into. A genuine neighbour's entry label is never referenced by our jumps and its switch id is new after our terminating `ret`, so the stale-0x2000-export protection is preserved (self-test B3). This only ever extends the reference to the *complete same function* — never folds in a neighbour — so scores become more accurate: usually up (matching tail included), occasionally ~1pp down (the previously-cut tail was unmatched and is now honestly counted). Observed: `FUN_001288e0` 40.5→90.3%, `FUN_001292f0` 65.6→92.8%, `FUN_001286e0` 59.0→89.8%, plus collateral `network_connection_new` 45.3→80.7%, `network_connection_write` 58.8→84.0%. Every 100% function stayed 100%.

**Lesson:** before declaring a sub-70% score a structural/`@<reg>` ceiling, sanity-check the reference length against `objdump` on the chunk. A 2× candidate/ref asymmetry is a truncation tell, not a lift defect. See `[[reference_perfn_delink_range_and_stub_returns]]`, `[[reference_vc71_scoring_integrity]]`.

## 27. VC71 Shape Levers: volatile Store/Reload, `&param` Poison, Else-Block Sinking, Assert Masks

**Automation:** not mechanically detectable because these are optional codegen-shape choices, not correctness bugs — the source is behaviorally right either way, and the detector is the VC71 LCS score itself (a low score with frame/layout-shaped diffs is the trigger; see the `lift-score-improve` skill Step 3d for the checklist).

**What happens:** A faithful lift scores catastrophically low (FUN_001ac680: 46.1%) not because any instruction is wrong but because the *shape* diverges everywhere: values held in ST that the original stores/reloads each use, a bloated frame, whole blocks in the wrong order, and wrong x87 compare masks in asserts. Four source levers recovered it to 97.3%:

1. **`volatile float` local** reproduces the original's store-once/reload-each-use x87 pattern (`FSTP [slot]` + `FLD [slot]` per use). A plain local gets FPU-enregistered (value in ST across branches → totally different scheduling). Volatile is also the numerically faithful choice: each reload rounds to float where ST keeps double precision. VC71's allocator even placed the volatile into the dead `plan` param home slot ([EBP+0x18]) exactly like the original. Caveat: two reads in one expression = two loads; factor `x = d*cv; x + x` to get the single-load `FADD ST,ST0` doubling.
2. **Never take `&param`** (or alias it via `uint8_t *p`) to smuggle a value into a param slot: VC71 must assume every store through the derived pointer clobbers the base, so it pre-computes and SPILLS every field address (`lea; mov [ebp-N]` barrage), blows the frame 4 → 0x18 bytes, and spills char flags out of BL (~25pp observed loss). Keep field access as `*(int *)(param + 0xN)`.
3. **Code after the epilogue** (original main path placed after validate+RET, backward jmp) = `if (c) {small} else {huge}` — VC71 sinks the huge else past the join. `if (!c) goto L;` moves blocks the OPPOSITE way. A single-assignment non-volatile local used in the condition and both arms stays ST-resident (non-popping FCOMS) for free.
4. **Assert form `if (!(cond))`** with the condition spelled per the assert string: `!(x >= 0)` → TEST AH,1;JE-skip, `!(x < 0)` → TEST 5;JNP-skip — NaN faithfully falls into the assert. Plain `x < 0` spellings give wrong masks AND wrong unordered behavior. FCOM operand order mirrors source operand order (`0.0f > x` loads the const first). Always verify the clang NaN path too (§ Step 3c NaN caution).

**Not source-controllable** (document, don't chase, ~2pp): VC71-elided `x = x;` slot self-moves (two original variables coalesced into one slot; clang blocks the workaround with `-Werror,-Wself-assign`), operand preload hoisting into both branch successors, FDIV vs FDIVR selection, FLD/FXCH scheduling.

**Lesson:** a sub-50% score with systematic frame/layout divergence is not a structural ceiling — re-derive the shape from the reference disasm before giving up. See `[[reference_vc71_shape_levers_volatile_layout]]`.

## 28. Interpolation Range-Gate Written with `>` Instead of `!=` (Inverted Range Skips the Lerp)

**Automation:** RESOLVED — `check_lift_hazards.py::check_range_gate_relational` (WARN), shipped in this commit. Suppress a verified-legitimate ordered compare with `/* hazard-ok: range-gate */` on the `if` line.

**What happens:** A widget/meter interpolation is gated by a degeneracy test on two lo/hi range pairs. The original binary tests each pair with `FCOM`/`JE` and *skips* the lerp only when a range collapses:
```c
/* original: skip-if-degenerate, else interpolate */
if (in_hi == in_lo || out_hi == out_lo) { /* no interpolation */ }
else { scalar = out_lo + (in - in_lo) / (in_hi - in_lo) * (out_hi - out_lo); }
```
The faithful positive form (what enables the lerp) is the negation of that OR: `in_hi != in_lo && out_hi != out_lo`. A lift that "cleans this up" to a relational operator —
```c
if (in_hi > in_lo && out_hi > out_lo) { /* interpolate */ }   /* WRONG */
```
looks equivalent for a normal ascending range but is **false for an INVERTED range** (`hi < lo`). Any widget whose output range descends (`out_lo = 1.0`, `out_hi = 0.0`) then never interpolates: the output scalar is pinned to `out_lo` and the driven value is stuck at one end.

**Example (hud.c `FUN_000d27a0`, sniper elevation needle, 2026-07-08 — commit d2e0630f):** the elevation widget maps aim pitch to a texture-V scroll with an inverted output range (`out_lo = 1.0`, `out_hi = 0.0`). A prior lift wrote `out_hi > out_lo` → `0.0 > 1.0` is false → interpolation skipped → `out_scalar` stuck at `1.0` → `angle_ticks` map1 V-offset (`color_block+0xc`) computed to `0.768` instead of `~0.268`, scrolling the tick texture off the visible scope rail. Symptom: the blue elevation-needle triangles (▶◀) never rendered on either scope rail. Confirmed by capturing the real element + oracle `render_desc` via gdb at the rasterizer call (`0x15f8e0`) against a `core_save` checkpoint pulled off the box with `[[xbdm-getfile]]`, from the **faithful cachebeta build** (`[[feedback_snapshot_from_cachebeta_oracle]]`); the fix (`>` → `!=` on both pairs) restored the needle, verified in-game. The reference capture must come from cachebeta, never the reimplementation, so our own defect is not baked into the oracle.

**Prevention:**
- When a lift's `if` gate is the *negation* of an original OR-of-equalities (`a==b || c==d`), spell it `a != b && c != d`, not `a > b && c > d`. `>` silently drops the inverted-range case.
- Verify against the original's compare: `FCOM`/`FUCOM` + `JE`/`JNE` (equality) is `==`/`!=`, not `JA`/`JB`/`JAE`/`JBE` (ordered). If the disasm branches on the zero flag alone (`JE`/`JNE`), the source operator is `==`/`!=`.
- The detector fires only on the dangerous shape: an `if` ANDing two field-pair comparisons `*(float*)(B+o1) <rel> *(float*)(B+o2)` that share one base pointer `B` and use a relational operator (`<`,`>`,`<=`,`>=`). The faithful `!=`/`==` forms do not match, so the fixed source is green.
