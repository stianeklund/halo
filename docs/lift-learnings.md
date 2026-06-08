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

---

## 10. Caller-Site Register Order Contradicts Thunk Convention

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

**Prevention:** For any callee with `@<reg>` args, run `get_function_callers`
in Ghidra. For each caller, read the 5–10 instructions before the CALL and
note which registers hold which values. If a caller loads EBX before EAX (or
any non-canonical order), its ported C call must swap the corresponding args.
This is especially worth checking when the same function has both direct callers
and indirect callers (via an intermediate dispatch function) — they often use
different preparation sequences.

**Detection:** No runtime signal. Only way to catch this before deployment: for
each ported caller, verify the call-site disassembly matches what the C thunk
convention sends. A switch inside the callee that accepts only small integers
but receives a datum handle is the smoking gun in a decompiler trace.

---

## 11. Builder Returns Count; Consumer Ignores It → Assert on Missing Entry

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
- Before trusting `build/halo.map` for *anything*, compare its mtime against the
  deployed build, and cross-check the deployed `DECOMP BUILD <hash>` line in
  `xbdm/debug.txt` against the binary you symbolized from. A hash/date mismatch
  means the map is lying.
- When a symbolized crash points somewhere surprising, re-symbolize from the
  export table before forming any hypothesis — wrong symbols are the most
  expensive kind of wrong, because every downstream step inherits the error.

---

## 16. Void-Return Out-Param Functions That Return EAX Implicitly

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
