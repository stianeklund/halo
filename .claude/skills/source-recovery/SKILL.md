---
name: source-recovery
tier: user
description: The single readability/source-recovery orchestrator for already-lifted Halo Xbox code — /recover-source (and its alias /cleanup) scopes debt into a manifest, works the cleanup ladder in mandatory risk order, gates every category on byte-identical codegen, and reports every parked failure.
---

# /recover-source — Faithful Source Recovery

The sole end-to-end workflow for recovering readability and source structure in
already-lifted (`ported: true`, committed) code. `/cleanup <target>` is a
deprecated alias for this skill. **Default contract: the compiled bytes do not
change.** Anything beyond that is opt-in.

Usage: `/recover-source <src file | kb.json object> [--allow-risky]`

The manifest at `tools/recovery/source_recovery.py` is the machine state — never
work from an untracked baseline or silently rewrite a source file.

## Required sequence

```bash
R=tools/recovery/source_recovery.py; M=recovery/<file>.json
rtk python3 $R plan --source src/halo/<path>/<file>.c -o $M   # add --allow-risky to opt in
rtk python3 $R capture $M --object build/<...>/<tu>.obj       # COFF + assertion baseline
rtk python3 $R ladder  $M                                     # ordering view (read-only)
rtk python3 $R set-status $M <item-id> applied                # or: parked --reason <why>
rtk python3 $R check   $M --object <obj>                      # add --mode corrective if proven
rtk python3 $R report  $M
```

`set-status` also takes `--category <ladder-id>` to correct a detector's guess.

1. **Scope and baseline.** Confirm the exact `.c` path, inspect unrelated worktree
   changes without modifying them, `plan`, then `capture`. `capture` replaces the
   old `cleanup-report` block as machine state; `cleanup-report` remains the
   reference for *what* a baseline must contain (floors, delinked refs, oracles).
2. **Debt inventory.** Work small line-numbered items from the manifest. Do not
   treat every numeric literal as an address or infer a semantic name from a
   pattern alone.
3. **Binary evidence and contracts.** Get disassembly, call-site, layout, and
   runtime evidence before changing behavior. Evidence-backed `kb.json` names,
   prototypes, and globals are permitted; `@<reg>` assignments are immutable.
4. **Work the ladder in order** (below), one category at a time.
5. **Gate after each small change**, then `set-status`.
6. **Report.** `report` is the deliverable; `cleanup-report` remains the template
   for the human-facing before/after write-up.

## The ladder (mandatory order — manifest `category`)

| # | Category | Skill | Codegen risk | Gate |
|---|---|---|---|---|
| — | (pre) tooling check | `cleanup-report` | — | gaps → downgrade plan |
| 1 | `comments` | `re-comment-capture` | none | (a) byte-identical |
| 2 | `local-renames` | `local-var-cleanup` | none | (a) byte-identical |
| 3 | `symbol-names` | `naming-confidence` | none | (a) byte-identical |
| 4 | `const-enum` | `const-enum-recovery` | near-zero | (b) + no new `[IMM-WARN]` |
| 5 | `struct-define` | `struct-recovery` → `struct-assert` | none (defs only) | (a) + build passes (cs/co) |
| 6 | `offset-to-field` | `offset-to-struct` | low | (b) + hazard scan |
| 7 | `expr-simplify` | `expr-simplify` | medium | (c) — **opt-in** |
| 8 | `control-flow` | `control-flow-cleanup` | high | (c) — **opt-in** |

Header placement (`header-recovery`) rides along with 3/5: recovered types,
constants, and inline helpers go in the proven Bungie header, not a catch-all.

Categories may be **skipped when inapplicable, never reordered** — offset rewrites
need the asserts from 5, and renames before rewrites keep diffs reviewable.
`check`/`ladder` warn when a later category is applied while an earlier one still
has pending items; the warning is advisory (a category may not apply) but an
unexplained warning means you skipped a step. Without `--allow-risky` on the
manifest, `set-status ... applied` on a risky item is refused and `check` fails.

## Mechanical path for rungs 5 and 6 (`structize.py`)

`struct-define` and `offset-to-field` are **transcription, not judgement** once
the struct exists. Do them with the tool, not by hand — hand-editing hundreds of
offsets is where wrong-offset bugs come from, and the tool refuses where a human
would guess.

**Use `campaign` for multi-file runs (preferred) or `run` for a single file:**

```bash
# Multi-file: discover + run across all files touching a struct, in one call.
# Returns a JSON report with next_actions — the LLM's work queue.
rtk python3 tools/recovery/structize.py campaign --binding actor_t [--dry-run]

# Check what conflicts remain without compiling (cheap to poll):
rtk python3 tools/recovery/structize.py worklist --binding actor_t

# Discover which files have raw offsets for a struct:
rtk python3 tools/recovery/structize.py discover --binding actor_t

# Single file — --binding looks up --base/--struct from recovery/bindings.json:
rtk python3 tools/recovery/structize.py run --binding actor_t \
    --source src/halo/ai/actor_looking.c [--manifest recovery/<file>.json]
```

Bindings are registered in `recovery/bindings.json` — each maps a struct name
to the base variable name(s) used in source and a glob constraining the search.

**The LLM automation loop:**
1. `campaign --binding X` → get JSON with `next_actions`
2. Resolve the top conflict (Ghidra MCP query → edit `types.h`)
3. `campaign --binding X` → conflict count decreases
4. Repeat until `conflicts_total == 0`

`run` does census → split → **re-census** → converge. That re-census is the
reason to prefer it: splitting is what makes `pad_` sites resolvable, so a
census taken before the split misses every site the split just unblocked — and
the run still reports success. Exit codes: `0` work done, `1` failed (file
restored), `2` converged but rewrote nothing.

The individual steps remain available for inspection or partial work:

```bash
S=tools/recovery/structize.py
rtk python3 $S layout actor_t                                     # authoritative offsets (from clang)
rtk python3 $S census --source <f.c> --base actor --struct actor_t -o recovery/census/<f>.json
rtk python3 $S split    --census recovery/census/<f>.json --apply  # rung 5: pad_ -> field_XX
rtk python3 $S census   ... -o recovery/census/<f>.json            # re-census (run does this for you)
rtk python3 $S converge --census recovery/census/<f>.json          # rung 6: rewrite what stays neutral
```

`converge` is the one command worth remembering: it rewrites every eligible
site, compiles, diffs at **function** granularity, re-applies while excluding
any function whose code moved, and proves the result byte-identical. Divergent
functions come back as `parked_functions` with a reason. It restores the file
untouched if it cannot converge.

What the tool will **not** do, by construction — each of these is a refusal in
the census, never a guess:

| Refusal | Why |
|---|---|
| cast kind ≠ field kind | `*(float*)` over an `int32_t` field is a pun; rewriting changes codegen |
| width or signedness mismatch | MOVSX vs MOVZX are different instructions |
| offset lands in a `pad_` run | rung 5 must split it first (that is what `split` is for) |
| offset ≥ `sizeof(struct)` | the **binding is wrong** — stop, do not rewrite |
| `volatile` access | the qualifier must survive |
| whole-struct cast (`*(vector3_t*)`) | a multi-field copy, not a field access |
| address taken, not dereferenced | would require `&` of a packed field |
| conflicting widths at one offset | a genuine RE question; it goes on the worklist |

**Known limit, measured:** the base declaration is deliberately never retyped.
A partial retype rescales every un-rewritten `base + 0xNN` by `sizeof(struct)`.
Retyping is only safe once coverage reaches ~100%, and is a separate step.

**A park is a finding, not a result** — but you no longer have to guess at it.
Several causes produce the identical "this function's codegen moved" signal, and
guessing between them has already been wrong once. Run:

```bash
rtk python3 tools/recovery/structize.py triage --census recovery/census/<f>.json
```

It recompiles each parked function's rewrites with `-fno-strict-aliasing` and,
for those that still diverge, bisects down to the individual offsets responsible.

| Verdict | Meaning | Weight |
|---|---|---|
| `tbaa` | Byte-identical under `-fno-strict-aliasing`. The raw `*(char *)` cast aliases everything and pins ordering; the field access carries a precise struct-path tag and frees the scheduler. | **Proof.** Same accesses, same addresses — a wrong binding cannot reach this verdict. |
| `address-form-or-alignment` | Survives the flag. Either LLVM keeping a raw `base + 0xNN` in a register for an indexed load vs. canonicalising the GEP to a `lea`, or a `pack(1)` alignment-1 member shifting `-O3` scheduling. | **Lead only.** A genuinely wrong binding lands here too. Read the named offsets against disassembly. |
| `only-in-combination` | Byte-identical when rewritten alone. | Not itself the problem; re-check after the other parks resolve. |

Measured on `actor_moving.c`: 3 parked functions → 2 pure TBAA, 1 reduced to two
adjacent byte loads (`0x426`/`0x427`) whose address form changed. None was a
wrong binding. On `actor_looking.c` the 2 parks were the `pack(1)` alignment
case, where `-fno-strict-aliasing` makes no difference — hence the table, not a
single story. Nothing wrong ever ships regardless, because the gate withholds
in every case.

**Verifying the tool itself:** `rtk python3 -m tools.recovery.test_structize_e2e`
compiles real C with the project's flags and asserts the gate *fails* when fed a
wrong offset, a nonexistent field, and a build error. Run it after touching
`structize.py`; `test_structize.py` alone cannot catch a corruption bug because
it never invokes a compiler.

The conflict list from `split` is the **ranked RE worklist** — offsets ordered
by how many call sites they unblock. Resolve them with `struct-recovery`
(disassembly widths), then re-run `split`; each answer converts its sites from
refused to mechanical.

## Gates

- **(a) Neutral** — comments, local renames, symbol names, struct definitions,
  header moves. Gate on **byte-identical `.text`** via the COFF neutrality guard:
  `check --mode neutral`. This is primary and works with **no delinked reference**.
  The assertion-metadata guard runs in both modes and blocks any `__LINE__`/
  `__FILE__` drift.
- **(b) Near-neutral** — const/enum, offsets → fields. (a) plus
  `rtk python3 tools/verify/vc71_regression.py check --source <file> --threshold 0`
  (zero tolerance — **not** the default 2pp), plus the category check: no new
  `[IMM-WARN]` for const/enum, `check_lift_hazards.py --changed-only` for offsets.
- **(c) Risky** — expr-simplify, control-flow. (a)+(b) plus a **behavioral oracle**
  (`/verify equivalence` or a golden-harness case) and manifest `--allow-risky`.
  A codegen delta here is only acceptable with `--mode corrective` and the oracle
  green; it is recorded as an observation, never waved through.

## Measurement traps (check before believing a score)

- A **stale VC71 cache `.obj`** pins the score: a number that will not move when
  the source clearly changed is a measurement bug, not a ceiling — re-run with
  `--no-cache`.
- A **bloated or truncated delinked reference** fakes the score in both
  directions. `objdump -t delinked/<obj>.obj` and confirm the symbol actually
  bounds the target function before trusting a %.
- **Header moves shift `__LINE__`** for every `assert_halt` below the new
  `#include` — never automatically neutral. See `header-recovery` §5; the
  assertion-metadata guard is what catches it.

## Session rules

- **Scope**: one TU (or one kb.json object's TUs) per session. Resist adjacent drift.
- **Preconditions**: clean working tree; target ported AND committed. Never run
  recovery on an in-flight lift — finish `/lift` verification first.
- **One commit per category** (Separation rule): a reviewer must be able to say
  "commit 3 is renames-only". Never combine ladder categories in one commit.
  Before each category commit, `rtk python3
  tools/recovery/check_category_purity.py <category> --staged` must pass — it
  compares the staged diff's token streams against that category's allowed edit
  shape and fails closed. Exit 0 (pure) or 2 (`expr-simplify`/`control-flow`,
  not mechanically checkable — the codegen and behavioural gates own those) is a
  pass; exit 1 means split the commit.
- **Never touched here**: `@<reg>` annotations, `ported` flags, kb.json signatures,
  build config. A needed signature fix is *lift* work — stop and surface it.
- **Regression protocol**: gate failure → revert that unit and `set-status ...
  parked --reason <why>`, then continue with independent items. A drop discovered
  later (or runtime misbehavior) → `cleanup-regression-triage` before anything else.
- **Floors ratchet up**: after the session, `vc71_regression.py update --source
  <file>` if anything improved, so gains are locked in.
- Never weaken or bypass a strict gate to make progress; park instead.

## Delegation

One category per subagent, **sequential** — each category's commit is the next
one's base, so parallel category agents on one TU collide. Give each agent: the
manifest path, the target skill name, the category id, and an explicit
same-worktree instruction (a subagent worktree mismatch has silently discarded
work before).

### Specialized agent

For batch/multi-TU campaigns (`recover-goal`), category agents run as
`agentType: 'halo-source-recovery'` — the dedicated readability/source-recovery
subagent, counterpart to `vc71-match-optimizer` on the score side. It knows
this ladder, these gates, and the naming-confidence tiers; it never chases
VC71 score and never touches `@<reg>` annotations. `.claude/workflows/recover-goal.js`
already wires this. A single-session `/recover-source` invocation may run
inline instead — delegate to the agent explicitly when scoping more than one
TU or when running unattended.

## Fidelity rules

- Distinguish **neutral changes** (source/readability only, exact output unchanged)
  from **corrective fidelity improvements** (a proven mismatch in the current
  lift). Corrective changes need binary evidence and their own verification; do
  not call them cleanup.
- `naming-confidence` is cross-cutting. Semantic names only with evidence;
  otherwise mechanical names or `unknown`/`field_XX`.
- Preserve Shape: ABI, stack shape, evaluation order, side effects, control flow.
- A skipped VC71 reference is reported as **skipped**, never as passed.
