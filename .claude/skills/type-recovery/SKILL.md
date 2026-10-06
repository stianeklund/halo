---
name: type-recovery
tier: agent
triggers: ["type recovery", "recover types", "struct reconstruction", "byte-preserving decompiler type recovery", "byte-accurate struct recovery", "struct recovery", "recover struct", "identify struct", "tag block", "pool stride", "object stride", "packed layout", "array of structs", "union layout", "offsetof", "static_assert", "sizeof check", "define struct", "struct definition", "new struct", "struct assert", "raw offset", "raw offsets", "pointer arithmetic", "offset replacement", "replace offsets", "struct field access", "field access rewrite", "typed field access", "typed field accesses", "retype pointer", "retype local", "named globals", "pointer indirection"]
description: Recover structs, pointer/local types, aggregate fields, and named globals in Halo decompiled or lifted C while preserving layout, ABI, access patterns, and byte-identical VC71 code generation.
---

# Byte-Preserving Type Recovery

Own the complete type-recovery task: establish layouts, define or extend C POD
structs, type pointers and locals, replace raw offsets with fields, reuse proven
aggregate types, and name absolute-address globals with correct indirection.
This file contains the contract previously split between `struct-recovery` and
`offset-to-struct`; neither legacy skill needs to be loaded separately.

The result is typed source with the same computational shape and emitted code.
Preserving an existing lift's bytes does not prove it already matches the XBE.
Compare against debug build 2276 for original-binary accuracy separately; never
report candidate-before/candidate-after neutrality as an original-binary match.
For new ports, `/lift` still owns implementation and its ABI/type gates. Apply
the evidence rules here during the lift; post-lift rewrites use the strict
neutrality gate below. Keep proven correctness fixes separate from recovery.

## Scope and baseline

- Scope one function or small related group in one TU. Inspect callers, target
  accesses, existing types, `rtk jq` ABI/global declarations, and the scoped diff
  before editing. A producer return-type or shared-header edit expands the scope
  to every affected caller or includer.
- Record the source, flags, ABI inputs, and a before-edit VC71 object. Build both
  versions through the same VC71 harness; `compile_tu_vc71` in
  `tools/recovery/structize.py` reuses `vc71_verify.compile_vc71`, including
  register-argument handling. Do not substitute a naive `cl.exe` invocation.
- Under `/source-recovery`, keep its manifest, category order, and captured
  baseline. One skill owns the rules, but globals, definitions, and access
  rewrites remain separate categories for review and validation.
- Before Ghidra MCP calls, run
  `rtk python3 tools/audit/check_ghidra_mcp.py`; stop if it fails. Query targeted
  callees, accesses, and disassembly rather than dumping whole subsystems.

## Recover evidence, not plausible layouts

Use these sources in descending order of authority:

1. Target-resident asserts/debug strings naming fields or sizes.
2. Allocation sizes and pool-constructor element strides. An initializer's
   `memset` length proves the cleared span; confirm it covers the whole object
   before treating it as total size.
3. Array indexing strides, element counts, and proven allocation boundaries.
4. Disassembly operand widths and consumers: MOVSX/MOVZX, byte/word stores,
   FLD/FSTP versus integer operations, and pointer-producing/consuming uses.
   A MOV alone does not establish pointer meaning or integer signedness.
5. Access clusters across relevant readers, writers, callers, and global xrefs.
6. Existing definitions and `rtk jq` queries of `kb.json`; extend partial types
   instead of creating competing definitions. Check prior work with
   `rtk python3 tools/memory/prior_fixes.py "<type or subsystem>"`.

Record layout evidence in `recovery/evidence/<struct>.json`, following
`recovery/evidence/README.md` and the packet-definition worked example. Per
field, retain offset, width, supported signedness/kind, array length, name,
confidence, and the function/address/instruction citation. Size and stride need
their own evidence; omit unproven values. Validate before using the table and
include it in the reviewed batch rather than leaving evidence only in chat.

```bash
rtk python3 tools/recovery/evidence_table.py validate recovery/evidence/<struct>.json
rtk python3 tools/recovery/evidence_table.py render recovery/evidence/<struct>.json
```

### Naming and unknowns

These naming-confidence rules apply directly within this workflow:

- T1: an unambiguous 2276 identifier permits its exact semantic name, with a
  target citation.
- T2: behavior across relevant accesses/calls proves the role; use a descriptive
  semantic name and evidence comment.
- T3: layout/access is proven but meaning is not; use `field_<hex>` or a
  deliberately broad mechanical name.
- T4: guesses and cross-build/PDB suggestions remain hypotheses outside
  authoritative source. External material cannot prove 2276 names or layouts;
  retain its historical influence under `PROVENANCE.md`.
- An accessed unknown is `field_<hex>`. Only never-observed bytes are
  `pad_<hex>[n]`. Use lowercase hex without `0x`, at least two digits. Split
  padding only where an access is proven; do not infer meanings in the gaps.

## Define or extend C POD types

- Use existing project scalar types and exact widths. Preserve packing,
  alignment, offsets, and proven size/stride; spell out unobserved gaps.
- Add `co(type, member, offset)` for every recovered/edited member and replaced
  offset, and `cs(type, size)` when size is proven. These are the repo's
  `offsetof`/`sizeof` assertion helpers. For an unproven size, assert offsets and
  mark `/* size unproven */`; do not invent trailing padding or a stride.
- A pool element's datum salt is at +0x00; prove its total size from the pool
  constructor stride. Tag-block count/pointer pairs and array elements still
  require observed widths, offsets, and strides.
- Incompatible accesses at one offset may indicate a union, subobject,
  representation access, or a wrong binding. Verify the binary before choosing.
  Use a named union only with evidence for the arms; document a discriminator
  when one is proven. Do not hide conflicts behind semantic aliases.
- Reuse `real_point3d`, `real_vector3d`, colors, or other existing aggregate types
  only when layout and use prove the grouping. Three adjacent floats alone do
  not prove a vector or its semantic name. Assert aggregate and component
  offsets, and retain scalar accesses if an aggregate copy changes codegen.
- Prefer a binary-proven owning header. If none is established, use `src/types.h`
  for shared types or a TU-local definition for genuinely local types. Header
  moves follow `header-recovery`; preserve assertion line/file metadata and
  gate every includer. Prototypes remain owned by `kb.json`/generated `decl.h`.
- Keep C89 declarations before statements. Do not introduce methods, getters
  that add calls, constructors, templates, C++ bitfields, or new packing/other
  pragmas to force a match. Existing typed accessor macros are allowed only
  when expansion preserves the original call and access sequence.

The evidence table and placed definition must agree. A failed `co()`/`cs()`
requires investigation of evidence, alignment, or rendering; do not reshape the
type merely to make the build pass.

## Rewrite accesses and pointer/local types

- Replace `*(T *)((char *)base + offset)` with a direct asserted member when the
  base binding, address, width, signedness, and scalar kind agree. Offsets are
  layout evidence; a macro containing the old offset is not struct recovery.
- Keep the base expression, qualifiers, necessary casts, and access count.
  A float/integer or width mismatch is a finding to resolve before rewriting.
- An indexed element rewrite requires a proven, asserted element stride.
  Keep explicit local base pointers and pointer walks when their shape affects
  grouped stores; direct member spelling is subordinate to the byte gate.
- Retype a local/base only after accounting for every use. Remaining `base + N`
  expressions would scale by `sizeof(*base)` after retyping. Convert all such
  uses safely in the same small unit or retain an explicit byte-pointer view.
  `structize.py` deliberately leaves base declarations unchanged for this reason.
- Type a producer's return when evidence proves it and every caller is covered.
  Do not change immutable `@<reg>` assignments, arity, return width, or signatures
  as cleanup. A needed ABI/signature correction belongs to `/lift` separately.
  Generic `datum_get`/`tag_get` may remain `void *`; preserve the existing call
  through an established typed macro or a typed local at that original site.
- Do not combine repeated accessor calls or cache their results. A typed local
  substitutes for an existing temporary/cast at one site; it does not authorize
  merging call sites. Preserve calls and global/pointer re-reads in loops.
- Stack-buffer overlaps are not automatically structs. Compute offsets relative
  to the proven buffer base; preserve documented MSVC local overlap/aliasing.
  Leave unresolved or codegen-sensitive accesses raw with an evidence comment
  and report why they could not be converted.

Prefer the existing mechanical rewriter after establishing the binding:

```bash
rtk python3 tools/recovery/structize.py worklist --binding <id>
rtk python3 tools/recovery/verify_conflict.py --binding <id> --offset 0xNN
rtk python3 tools/recovery/structize.py run --binding <id> --source <file.c> --oracle vc71
```

Bindings live in `recovery/bindings.json`. The tool can split known fields and
converge access rewrites; unresolved conflicts remain RE work. Its refusals do
not prove that safe manual recovery is impossible. Aggregate fields, producer
types, local retyping, and globals may need small manual edits with the same
evidence and before/after byte gate.

## Replace absolute addresses with named globals

Prove whether an address holds an object, a pointer slot, or a pointer-to-pointer
slot. Preserve its original dereference depth, qualifiers, scalar kind, and
address expression. For example, these are different objects:

```c
#define state_globals (*(state_t *)0x123400)       /* object at address */
#define state_globals_ptr (*(state_t **)0x123400)  /* pointer stored there */
```

Use the appropriate name only with T1/T2 evidence; otherwise retain a mechanical
name. In `/source-recovery`'s `global-names` category the macro must preserve the
exact replaced cast/address token run. Changing the global's type to a recovered
struct is a separate gated type edit, not part of naming it.

Place the definition in an established owning/already-included header without
shifting assertion metadata. A `kb.json` data extern is not interchangeable:
generated `HDATA` declarations use `dllimport` and may add an import-slot load.
Do not move address-backed storage into a new C global, or change a constant-pool
load into a literal. Function addresses require proven `kb.json` declarations
and ABI handling; never invent a callable cast to remove an address.

## Preserve computational shape

Keep control flow, expression/operand order, temporary variables, call count and
ordering, casts, load/store widths, rounding points, and alias-dependent access
patterns. Do not modernize or optimize while recovering types. In particular:

- Do not reassociate, reorder, or fold floating-point expressions through
  intermediates. Preserve x87 spill/reload points and comparison direction,
  including NaN behavior.
- Do not hoist repeated loads, perform common-subexpression elimination, cache
  values across calls/stores, or combine calls even if the source looks redundant.
- Do not remove casts or temporaries merely because the recovered type makes
  them look unnecessary. Preserve signed/unsigned extensions and float-to-int
  conversion casts; do not spell compiler intrinsics as explicit calls.
- Typed accesses can change alias analysis and scheduling despite identical
  addresses. Investigate a failure; do not change flags, add `volatile`, barriers,
  dead stores, undefined behavior, or arbitrary pragmas to force neutrality.

## Strict verification and reporting

After each small rewrite, compile the same scope with the same VC71 harness and
compare the candidate objects' code bytes and relocations exactly. The COFF guard
supports an explicit before/after check:

```bash
rtk python3 tools/recovery/coff_candidate_guard.py capture <before-vc71.obj> -o <baseline.json>
rtk python3 tools/recovery/coff_candidate_guard.py check <baseline.json> <after-vc71.obj>
rtk python3 tools/audit/check_lift_hazards.py --changed-only
```

- Struct assertions and the normal project build must pass. Cover every caller
  or includer affected by a shared declaration. Preserve assert metadata.
- Run the owning source-recovery/lift validation and original-XBE regression
  checks as applicable. An unchanged or higher similarity score is insufficient
  proof of byte neutrality; a percentage may hide changed operands or calls.
- A Clang-only pass does not establish VC71 neutrality. If VC71 compilation or
  the before-edit object is unavailable, report verification as incomplete;
  do not accept the rewrite as byte-preserving. A delinked original is not needed
  for candidate-before/candidate-after comparison.
- On a byte delta, restore only the failing unit, retain the layout evidence,
  and report `kept-raw: codegen-sensitive` or the specific unresolved reason.
  Do not mutate a proven struct or weaken gates to make the rewrite pass.
- Preserve separate commits/categories for evidence/definitions, global naming,
  and access/type rewrites when the owning workflow requires them. This skill
  does not authorize automatic commits, integration, or deployment by itself.
- Report confirmed layouts/types, inferred names with confidence, uncertain
  ranges, conversions and kept-raw sites, affected scope, and exact gate results.
  Distinguish byte-neutrality from original-binary accuracy and skipped checks.
