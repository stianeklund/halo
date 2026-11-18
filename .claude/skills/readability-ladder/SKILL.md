---
name: readability-ladder
tier: agent
description: "Doctrine for making lifted C readable without losing bytes: target style, evidence for types/names/constants, the risk-ordered ladder, byte-neutral techniques, gates, and regression triage."
triggers: ["readability", "readable", "source recovery", "cleanup", "type recovery", "recover types", "struct recovery", "recover struct", "define struct", "struct definition", "struct assert", "raw offset", "raw offsets", "offset replacement", "replace offsets", "field access rewrite", "typed field access", "retype pointer", "retype local", "named globals", "pointer indirection", "pool stride", "union layout", "rename locals", "local cleanup", "uvar", "ivar", "fvar", "decompiler locals", "mechanical names", "magic number", "named constant", "enum recovery", "recover enum", "flag bits", "sentinel value", "simplify expression", "redundant cast", "simplify cast", "collapse temporaries", "control flow cleanup", "simplify goto", "restructure loop", "flatten nesting", "header file", "create header", "new header", "where does this struct go", "split types.h", "comment capture", "evidence comment", "cleanup regression", "match dropped", "score dropped", "vc71 drop", "byte drop", "structize", "&v.i", "exploded vector", "boundary cast"]
---

# Readability ladder

How to turn lifted C into code that reads like the original authors wrote it,
without losing a single byte of accuracy against 2276. Campaign skills decide
*which* code to work on; this file decides *how*. `naming-confidence` governs
every name and layout claim.

**Contract:** raw-XBE aligned bytes per function never go down. A change that
is byte-neutral or byte-improving and more readable is in scope. A readable
change that loses bytes is not recovery — revert it or hand it to the byte lane
with the measured difference. Never fake neutrality (rule 10: no volatile
shaping, dead stores, barriers, pragmas, UB). `@<reg>` annotations, arity,
return width and `ported` flags are never touched here; an ABI fix is lift work.

## 1. Target style

Finished source follows `lift-implementation.md` → *Bungie Code Style*:
`lower_snake` names, `k_` constants, `_enum_member` and `_x_bit` enum members,
`MAXIMUM_`/`NUMBER_OF_` bounds, `*_index` handles, `NONE`, `TEST_FLAG`, cseries
types, typed datum `_get` macros over kb-named pools. Concretely:

- **No raw derefs.** `*(float *)(p + 0x24)` becomes a struct member. Repeated
  offsets on one base mean there is a struct to recover (§3).
- **No exploded aggregates.** `real_vector3d`/`real_point3d`/
  `real_euler_angles2d`/`real_matrix4x3`, not `float pos[3]`, `&v.i` arguments,
  or per-component copies. A 12-byte copy is a struct assignment.
- **No addresses in bodies.** A global is a named macro in an included header
  (§4); `XCALL(0x…)` becomes a kb.json name; a `.rdata` float pool constant
  becomes a literal only after its XBE value is verified (§6).
- **No decompiler names.** `iVarN`, `param_N`, `local_N`, `FUN_`, `DAT_` are
  debt. Semantic name with T1/T2 evidence, otherwise a mechanical role name (§5).
- **No casts or magic numbers** where a type, field, named constant, enum or
  macro expresses it. Angles use `_pi`, `DEGREES_TO_RADIANS(x)`,
  `RADIANS_TO_DEGREES(x)` (force-included `common.h`/`xdk_common.h`).
- **Typed accessors.** One `thing_get(h)` macro per datum/tag type wrapping
  `datum_get`/`tag_get`. Preserve call count and placement; never merge or
  cache repeated calls.
- **Minimal comments.** No function header comments (address and signature live
  in kb.json). Keep only what code cannot say: ABI quirks, match-sensitive
  constructs (`/* keep: NaN-inclusive guard */`), open uncertainty
  (`/* unverified: */`). General lessons go to `docs/lift-learnings.md` with a
  detector. Never cite external reference projects anywhere in the repo.
- **C89, concise.** Declarations first; fold a declaration into its
  initializer only when C89 allows and order is unchanged; no redundant
  parentheses or prototype-implied casts.
- **Owners.** Types go in the binary-proven header (§8) or `src/types.h`.
  Prototypes come only from kb.json → `decl.h`.

## 2. The ladder (risk order; one category per commit)

| # | Category | Risk | Gate |
|---|---|---|---|
| 1 | comments | none | byte-identical |
| 2 | local renames | none | byte-identical |
| 3 | symbol names (`FUN_x` → name, kb rename) | none | byte-identical, `--rename-map` for relocs |
| 4 | global names (address → macro) | none | byte-identical |
| 5 | constants / enums | near-zero | neutral + no new `[IMM-WARN]` |
| 6 | struct definitions / pad splits | none | byte-identical + `cs`/`co` build |
| 7 | offset → field, retyped locals/params | low | neutral-or-better + hazard scan |
| 8 | shared API retypes (kb.json decls) | low | global gate (all callers) |
| 9 | expression simplification | medium | neutral-or-better + behavior oracle |
| 10 | control flow | high | neutral-or-better + behavior oracle |

Skip inapplicable categories; do not mix two in one commit. Commit prefixes:
`Types:`, `Names:`, `Readability:` (constants, comments, cleanup), each a
logical bulk passing every gate. `tools/recovery/check_category_purity.py
<category> --staged` verifies a staged diff's shape when you want proof.

## 3. Types and structs

**Evidence, in descending authority:** target asserts/strings naming fields or
sizes; allocation sizes and `data_new` strides (`memset` length proves the
cleared span only); index strides; operand widths (MOVSX/MOVZX, FLD/FSTP vs
integer, byte/word stores); access clusters across readers, writers, callers;
existing `src/types.h` definitions and `rtk jq` kb.json queries. Ghidra MCP
(after `check_ghidra_mcp.py`) for targeted accesses only. External references
suggest *where to look*; they never supply an offset, width or size.

- Accessed-but-unproven member: `field_<hex>`. Never-observed bytes:
  `pad_<hex>[n]`. Split padding only at proven accesses.
- Add `co(type, member, offset)` per edited member and `cs(type, size)` when
  size is proven; otherwise `/* size unproven */`. A failing assert is a finding
  about the evidence, never a reason to reshape the type.
- Reuse `real_point3d`/`real_vector3d`/colors only when layout *and* use prove
  the grouping; three adjacent floats alone do not.
- Incompatible widths at one offset: union, subobject, or wrong binding —
  verify against disassembly (`tools/recovery/verify_conflict.py`) before
  choosing. Record evidence in `recovery/evidence/<struct>.json`
  (`evidence_table.py validate`).
- **Retyping a base** rescales every remaining `base + N`; convert all uses in
  the same unit or keep a byte-pointer view. Retyping a variable is usually
  byte-neutral where a cast at the use is not.
- **Shared APIs:** when a callee takes `float *` for a vector, retype its
  kb.json decl (and `kb_reg_baseline.json`) to the typed form, regenerate
  `decl.h`, fix callers with the compiler. Callers outside the campaign scope
  may cast at the call — acceptable boundary debt; list it in the report.

**Mechanical path:** `tools/recovery/structize.py` (bindings in
`recovery/bindings.json`) does pad splits and offset→field rewrites and refuses
where a human would guess (kind/width/sign mismatch, pad hit, offset ≥ size,
volatile, address-taken). `campaign --binding X` → resolve top conflict →
repeat; `run` re-censuses after split. `triage --census …` classifies parked
functions: `tbaa` (proof the binding is right; aliasing freed scheduling),
`address-form-or-alignment` (lead only — read the offsets against
disassembly), `only-in-combination`. `structize.py` never retypes the base.

## 4. Globals

Prove object vs pointer slot vs pointer-to-pointer; keep dereference depth and
cast type. The neutral form is a macro with the exact replaced token run:
`#define update_client_globals_initialized (*(uint8_t *)0x45b1d0)`. A kb.json
`objects.data` extern is **not** equivalent (emitted as `dllimport`, adds an
import-slot load). Retyping the global to a recovered struct is a separate
type edit.

## 5. Names

- **Functions/symbols** (`FUN_` → name): T1/T2 per `naming-confidence`. A kb
  rename is byte-neutral; update every caller and re-key the
  `vc71_scores.json` floor in the same commit. "Renaming changes the
  relocation" is not a park reason — use `--rename-map`.
- **Locals**, mechanical vocabulary allowed without evidence: `i`, `j`,
  `index`, `count`, `element`, `entry`, `src`, `dst`, `result`, `sum`. Datum
  pointer named after its element type once the pool is named; handles
  `<thing>_index`. Rename-only diffs; never merge two decompiler locals (that
  may be an MSVC spill/reload) — that is §7 work.
- Same role, same name across the TU.

## 6. Constants and literals

Two proofs: the value (always provable) and the meaning (often not).

- Shapes: `_<enum>_<member>`, `_<thing>_<name>_bit`, `k_<name>`,
  `MAXIMUM_<THINGS>`, `NUMBER_OF_<THINGS>`; proven value, unproven meaning →
  `_<enum>_unknown<value>`. Search `rtk rg '<value>' src/` before minting one.
- C89 enums are `int`: a mode in an `int8_t` field keeps the `int8_t` field;
  only the constants are enum members.
- Each replacement must fold to the identical bits. VC71 folds in double
  precision: check `DEGREES_TO_RADIANS(x)` etc. against the original
  immediate. Keep a literal if the rewrite changes arithmetic order
  (`x * 0.0174533f` is not `DEGREES_TO_RADIANS(x)`), or if the original source
  shape evidently used one.
- Literal stores compile to `mov [x],imm`; const-pool compares to
  `fcomp [addr]`. A rewrite must not change which form a site gets. Replace
  `*(float *)0x…` pool loads with literals only after reading the value from
  the pristine XBE (`tools/equivalence/xbe_image.py`).
- Do not name a literal without evidence; an unnamed magic number is honest.
  Matching small integers against existing `#define`s is not evidence.

## 7. Expressions and control flow

**Never:** reassociate or reorder float arithmetic (x87 rounds per step);
flip float comparisons (`!(x >= e)` is NaN-inclusive); change cast width or
signedness (MOVSX vs MOVZX); hoist/CSE global or pointer re-reads in loops;
turn `(int)float` into anything else (`_ftol2`); delete a temp that forces a
spill/reload; move anything across aliased writes.

**Allowed, gated:** delete provably same-width redundant casts; split a
mega-expression into C89 temps without changing order or rounding; remove
decompiler temps when the gate shows neutral-or-better (often a byte *gain*).

**Control flow** is evidence (Preserve Shape). Only decompiler-artifact gotos
and duplicated exits are candidates, written down first, behind a behavioral
oracle (`unicorn_diff.py <fn> --seeds 100 --allow-stubs --mem-trace`, golden
harness, or `ab_check.py`). Traps: Ghidra do/goto loops have an inverted
condition; early exits are often the NaN path; VC71 tail-merges exits; dense
`switch` → jump table, sparse → compare chain.

## 8. Headers and accessors

- A new header must be named by a `__FILE__` string in the XBE (assert inside a
  header inline). No proven header → the type stays in `src/types.h`.
  Known (2026-07-24 sweep, `c:\halo\source\…`): `ai/actor_type_definitions.h`,
  `ai/encounters.h`, `ai/path.h`, `hs/hs_library_internal_compile.h`,
  `hs/hs_library_internal_runtime.h`, `objects/objects.h`,
  `objects/widgets/widget_types.h`, `sound/sound_classes.h`,
  `sound/sound_definitions.h`, `sound/sound_dsound.h`, plus short-name hits
  `bitmaps_inlines.h`, `real_math.h`, `reference_lists.h`. Re-sweep the XBE
  for `*.h\0` strings before creating one.
- Headers hold types, `cs`/`co`, macros, enums, inlines the binary proves —
  never function prototypes (kb.json owns those). Guard mirrors the path
  (`HALO_AI_ENCOUNTERS_H`); self-contained; `types.h` never includes them.
- **Adding a line to a `.c` is not free.** `__LINE__` in `assert_halt` is a
  pushed immediate; one added `#include` or `#define` above an assert shifts
  every assert below. Put definitions in a header the TU already includes,
  below its last assert-bearing inline, or keep the `.c` line count constant.
  Pin implicit sites first if needed: `assert_halt(c)` →
  `assert_halt_at(__FILE__, <line>, c)` (separate commit;
  `tools/audit/recover_assert_sites.py --apply`).
- **Unused static inlines in widely included headers** (e.g. `real_math.h`)
  shift other TUs' codegen. New inlines go in a narrowly included header.
- A `static __inline T *f(...) { void *p = ...; return p; }` typed view is
  fine, but **not as a call argument** where a constant push precedes it (VC71
  hoists the inline). Use a macro or direct member access there.
- A cast added at a call site can change VC71 codegen; retype the variable
  instead.

## 9. Gates

| Gate | Command | When |
|---|---|---|
| Per-TU neutral | `rtk python3 tools/recovery/neutral_gate.py capture/check --source <tu.c> --dir <d>` | every edit (byte-identical categories) |
| Per-function raw bytes | stage the TU, `rtk python3 tools/verify/byte_regression.py local` | edits that may change codegen |
| Build | `timeout 1500 cmake --build build -j8 -- -k` (clang `-Wall -Werror`) | before each commit |
| Canaries | `game_engine.c`, `ai_communication.c`, `rasterizer_xbox_vertex_shaders_runtime.c` | after any header/`types.h` change |
| Global | `git add` then `rtk python3 tools/verify/byte_regression.py local` | before each commit; any function losing bytes fails |
| Hazards | `rtk python3 tools/audit/check_lift_hazards.py --staged-only` | before each commit |
| Behavior | `unicorn_diff.py` before/after, same `build :` line | §7 changes |

Judge by **aligned** bytes per function, not the global positional
`byte_accuracy`. A Clang-only or unchanged-percentage check is not proof of
neutrality. Source files contain `#line` directives: map compiler line
numbers back through the printed snippet before editing. `types.h` member or
enum additions can perturb VC71 in unrelated TUs — run the canaries.

## 10. Regression triage

1. Localize: `git log --oneline <base>..HEAD -- <tu>`; bisect the per-category
   commits; per-function bytes identify the function.
2. Classify by category: comments/renames → diff was not pure; constants →
   bit pattern or literal↔pool form (`vc71_verify.py --imm-only`);
   offset→field → width/sign or lost base-pointer shape (`--loadw-only`);
   expressions → reassociation or deleted spill temp (`--fpu-only`); control
   flow → oracle decides.
3. Revert just that unit; keep the rest. Park the attempt in
   `artifacts/parked/patches/` (never discard). Safer alternative: keep the
   raw form with a `/* keep: codegen-sensitive */` comment.
4. Rule out scorer artifacts first (stale cache `.obj`, bloated/truncated
   delinked reference). Runtime symptom → `prior_fixes.py`, then `debug` with
   the commit already identified.
