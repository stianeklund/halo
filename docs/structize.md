# structize — mechanised struct recovery (ladder rungs 5 and 6)

`tools/recovery/structize.py` turns two source-recovery steps into transcription
instead of judgement:

- **rung 5, `struct-define`** — subdivide `pad_` runs into `field_XX` at the
  offsets a source file actually dereferences.
- **rung 6, `offset-to-field`** — rewrite `*(int *)(actor + 0x1c)` into
  `((actor_t *)actor)->field_01c`.

Hand-editing hundreds of offsets is how wrong-offset bugs get in. The tool
refuses exactly where a human would guess, and proves what it does emit.

## Use it

```bash
rtk python3 tools/recovery/structize.py run \
    --source src/halo/ai/actor_moving.c --base actor --struct actor_t
```

`run` does census → split → **re-census** → converge. The re-census is the
reason to prefer it over the individual steps: splitting is what makes `pad_`
sites resolvable, so a census taken before the split misses everything the split
just unblocked — and the run still reports success. Measured on `actions.c`:
403 eligible before the split, 458 after.

Exit `0` work done, `1` failed (file restored untouched), `2` converged but
rewrote nothing.

## What the gate proves

`converge` rewrites every eligible site, compiles, diffs at **function**
granularity, re-applies while excluding any function whose code moved, and
proves the rest byte-identical. One divergent function costs that function, not
the other 118.

**The oracle is Visual C++ 7.1, not clang.** The binary was built by MSVC 7.1,
so "did my edit change the generated code?" is only meaningful when MSVC 7.1
answers it. This is not a formality: on `actor_moving.c` the clang gate parked
three functions as divergent and VC71 says they do not diverge at all — 196
sites withheld for nothing. `--oracle clang` exists as a fallback and stamps
`oracle_warning` into the report payload so a clang-gated result can never be
read as evidence about the real compiler.

## What it refuses

Every one of these is a refusal in the census, never a guess:

| Refusal | Why |
|---|---|
| cast kind ≠ field kind | `*(float*)` over an `int32_t` field is a pun |
| width or signedness mismatch | MOVSX and MOVZX are different instructions |
| offset lands in a `pad_` run | rung 5 must split it first |
| offset ≥ `sizeof(struct)` | the binding is **wrong** — stop |
| `volatile` access | the qualifier must survive |
| whole-struct cast | a multi-field copy, not a field access |
| address taken, not dereferenced | would need `&` of a packed field |
| conflicting widths at one offset | a real RE question — goes on the worklist |

The conflict list from `split` is a **ranked RE worklist**, ordered by how many
call sites each unresolved offset unblocks. Answer one from disassembly, re-run,
and its sites convert automatically. Offsets appearing in several files convert
in all of them from one answer.

## Explaining a park

```bash
rtk python3 tools/recovery/structize.py triage --census recovery/census/<f>.json
```

Recompiles each parked function and, where the divergence survives, bisects down
to the individual offsets responsible.

| Verdict | Weight |
|---|---|
| `tbaa` | **Proof.** Byte-identical under `-fno-strict-aliasing` ⇒ same accesses at same addresses ⇒ a wrong binding cannot reach it. (clang oracle only — VC71 has no such switch.) |
| `address-form-or-alignment` | **Lead only.** A wrong binding lands here too. Check the named offsets against disassembly. |
| `only-in-combination` | Clean alone; re-check after other parks resolve. |

## Known limit

The base declaration is deliberately never retyped. A partial `char *actor` →
`actor_t *actor` rescales every un-rewritten `actor + 0xNN` by
`sizeof(actor_t)`. Retyping is safe only near 100% coverage, as a separate pass.

## Verifying the tool

```bash
rtk python3 -m tools.recovery.test_structize        # pure functions
rtk python3 -m tools.recovery.test_structize_e2e    # compiles real C, 27 tests
```

`test_structize.py` alone cannot catch a corruption bug — it never invokes a
compiler. The e2e suite deliberately feeds the tool a wrong offset, a
nonexistent field, and a build error, and asserts the gate *fails*. Writing it
found two false-greens in the tool, both in the "reports success" direction.

## Results so far

| TU | Sites converted | Parked | VC71 match |
|---|---|---|---|
| `actor_looking.c` | 663 | 2 of 119 fns | not measured |
| `actors.c` | 553 | 16 of 124 fns | not measured |
| `actions.c` | 458 | 0 | not measured |
| `actor_moving.c` | 364 of 364 | 0 | 81.24% → 81.24%, 0/51 fns moved |

`actor_t` went from 35 to 470+ named fields with `sizeof` unchanged at 0x724.
Repo-wide `raw_offset_deref` fell 14843 → 14486, its first recorded decrease.

The three TUs above `actor_moving.c` were gated with clang and are **not**
VC71-verified; their parks are likely recoverable by re-running under the VC71
oracle.
